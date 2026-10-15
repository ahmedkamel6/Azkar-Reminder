#define MINIAUDIO_IMPLEMENTATION
#include "../../../third_party/miniaudio.h"

#include "MiniAudioEngine.h"
#include "../../core/Logger.h"
#include "../../core/Settings.h"
#include "../../core/PlatformPaths.h"
#include "../../core/Utf8.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace dhikr {

std::unique_ptr<IAudioEngine> IAudioEngine::Create() {
    return std::make_unique<MiniAudioEngine>();
}

MiniAudioEngine& MiniAudioEngine::Instance() {
    static MiniAudioEngine instance;
    return instance;
}

MiniAudioEngine::MiniAudioEngine() = default;

MiniAudioEngine::~MiniAudioEngine() {
    Shutdown();
}

bool MiniAudioEngine::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return InitializeLocked();
}

bool MiniAudioEngine::InitializeLocked() {
    if (m_initialized) return true;
    if (std::chrono::steady_clock::now() < m_nextRetry) return false;
    m_nextRetry = std::chrono::steady_clock::now() + std::chrono::seconds(30);

    m_engine = new ma_engine();
    ma_engine_config config = ma_engine_config_init();

    ma_result result = ma_engine_init(&config, m_engine);
    if (result != MA_SUCCESS) {
        LOG_WARN("Failed to initialize miniaudio engine: " + std::to_string(result) + " (running in silent fallback mode)");
        delete m_engine;
        m_engine = nullptr;
        m_initialized = false;
        return false;
    }

    auto* device = ma_engine_get_device(m_engine);
    if (!device || !device->pContext || device->pContext->backend == ma_backend_null) {
        ma_engine_uninit(m_engine);
        delete m_engine; m_engine = nullptr;
        LOG_WARN("No audio output device is available; reminders remain visible");
        return false;
    }
    m_initialized = true;
    LOG_INFO("MiniAudio engine initialized successfully");
    return true;
}

void MiniAudioEngine::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sound) {
        ma_sound_uninit(m_sound);
        delete m_sound;
        m_sound = nullptr;
    }
    if (m_engine) {
        ma_engine_uninit(m_engine);
        delete m_engine;
        m_engine = nullptr;
    }
    m_initialized = false;
    m_nextRetry = {};
}

std::filesystem::path MiniAudioEngine::ResolveSoundFilePath() const {
    const auto s = Settings::Instance().GetCopy();

    // 1. Custom sound path if configured and exists
    if (!s.customSoundPath.empty()) {
        auto p = std::filesystem::u8path(WideToUtf8(s.customSoundPath));
        if (std::filesystem::exists(p)) return p;
    }

    // 2. Data directory audio asset
    auto dataDirAudio = PlatformPaths::GetDataDir() / "audio" / "diode_matrix.mp3";
    if (std::filesystem::exists(dataDirAudio)) return dataDirAudio;

    // 3. Executable-relative bundle paths (macOS .app Resources & Linux AppImage /usr/share)
#ifdef __APPLE__
    char exeBuf[1024] = {};
    uint32_t bufSize = sizeof(exeBuf);
    if (_NSGetExecutablePath(exeBuf, &bufSize) == 0) {
        std::error_code ec;
        auto exeDir = std::filesystem::weakly_canonical(std::filesystem::path(exeBuf), ec).parent_path();
        auto bundleAudio = exeDir / ".." / "Resources" / "audio" / "diode_matrix.mp3";
        if (std::filesystem::exists(bundleAudio)) return bundleAudio;
        auto sibAudio = exeDir / "audio" / "diode_matrix.mp3";
        if (std::filesystem::exists(sibAudio)) return sibAudio;
    }
#elif defined(__linux__)
    std::error_code ec;
    auto exePath = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        auto exeDir = exePath.parent_path();
        auto appImageAudio = exeDir / ".." / "share" / "azkar-reminder" / "audio" / "diode_matrix.mp3";
        if (std::filesystem::exists(appImageAudio)) return appImageAudio;
        auto sibAudio = exeDir / "audio" / "diode_matrix.mp3";
        if (std::filesystem::exists(sibAudio)) return sibAudio;
    }
    if (std::filesystem::exists("/usr/share/azkar-reminder/audio/diode_matrix.mp3")) {
        return "/usr/share/azkar-reminder/audio/diode_matrix.mp3";
    }
#endif

#ifdef _WIN32
    wchar_t executable[32768] = {};
    auto length = GetModuleFileNameW(nullptr, executable, 32768);
    if (length > 0 && length < 32768) {
        auto audio = std::filesystem::path(executable).parent_path() / "audio" / "diode_matrix.mp3";
        if (std::filesystem::exists(audio)) return audio;
    }
#endif
    // 4. Local relative paths
    if (std::filesystem::exists("audio/diode_matrix.mp3")) return "audio/diode_matrix.mp3";
    if (std::filesystem::exists("assets/audio/diode_matrix.mp3")) return "assets/audio/diode_matrix.mp3";
    if (std::filesystem::exists("../assets/audio/diode_matrix.mp3")) return "../assets/audio/diode_matrix.mp3";

    return {};
}

void MiniAudioEngine::ReloadSound() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !m_engine) return;
    LoadSoundLocked();
}

bool MiniAudioEngine::LoadSoundLocked() {

    if (m_sound) {
        ma_sound_uninit(m_sound);
        delete m_sound;
        m_sound = nullptr;
    }

    auto path = ResolveSoundFilePath();
    if (path.empty()) {
        LOG_WARN("No reminder audio file found to reload");
        return false;
    }

    m_sound = new ma_sound();
    const auto utf8Path = path.u8string();
    ma_result res = ma_sound_init_from_file(m_engine, reinterpret_cast<const char*>(utf8Path.c_str()), 0, nullptr, nullptr, m_sound);
    if (res != MA_SUCCESS) {
        LOG_WARN("Failed loading sound file: " + path.string() + ", error=" + std::to_string(res));
        delete m_sound;
        m_sound = nullptr;
        return false;
    }

    m_loadedPath = path;
    LOG_INFO("Loaded reminder audio: " + path.string());
    return true;
}

void MiniAudioEngine::PlayReminder(float volumeOverride) {
    const auto settings = Settings::Instance().GetCopy();
    if (!settings.soundEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized && !InitializeLocked()) return;

    float vol = volumeOverride >= 0.0f ? volumeOverride : (settings.soundVolume / 100.0f);
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;

    auto currentPath = ResolveSoundFilePath();
    if (currentPath.empty()) {
        LOG_WARN("Cannot play reminder chime: no audio asset found");
        return;
    }

    if ((!m_sound || currentPath != m_loadedPath) && !LoadSoundLocked()) return;
    ma_sound_stop(m_sound);
    ma_sound_seek_to_pcm_frame(m_sound, 0);
    ma_sound_set_volume(m_sound, vol);
    ma_result res = ma_sound_start(m_sound);
    if (res != MA_SUCCESS) {
        LOG_WARN("Playback error: " + std::to_string(res));
    }
}

void MiniAudioEngine::Stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !m_engine) return;
    if (m_sound) {
        ma_sound_stop(m_sound);
        ma_sound_seek_to_pcm_frame(m_sound, 0);
    }
}

bool MiniAudioEngine::IsAvailable() const {
    return m_initialized.load();
}

} // namespace dhikr
