#include "Settings.h"
#include "Utf8.h"
#include "AtomicFile.h"
#include "Logger.h"
#include "../third_party/json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <random>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#endif

using json = nlohmann::json;

namespace dhikr {

Settings& Settings::Instance() {
    static Settings instance;
    return instance;
}

// ── Helpers: Color <-> hex string ────────────────────────────────────────────
static std::string ColorToHex(const Color& c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

static Color HexToColor(const std::string& hex) {
    if (hex.size() < 7) return { 0xB7, 0xF3, 0x4A };
    auto fromHex = [](char h, char l) -> uint8_t {
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return 0;
        };
        return (uint8_t)(hexVal(h) * 16 + hexVal(l));
    };
    return { fromHex(hex[1],hex[2]), fromHex(hex[3],hex[4]), fromHex(hex[5],hex[6]) };
}

// ── UUID generator for anonymous installation ID ──────────────────────────────
static std::wstring GenerateUUID() {
#ifdef _WIN32
    GUID guid;
    CoCreateGuid(&guid);
    wchar_t buf[64];
    StringFromGUID2(guid, buf, 64);
    return buf;
#else
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);
    wchar_t buf[64];
    swprintf(buf, 64, L"%08llx-%04llx-%04llx-%04llx-%012llx",
        (unsigned long long)(part1 >> 32),
        (unsigned long long)((part1 >> 16) & 0xFFFF),
        (unsigned long long)((part1 & 0x0FFF) | 0x4000),
        (unsigned long long)(((part2 >> 48) & 0x3FFF) | 0x8000),
        (unsigned long long)(part2 & 0xFFFFFFFFFFFFULL));
    return buf;
#endif
}

bool Settings::Load(const std::filesystem::path& appDataPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath = appDataPath / "settings.json";

    // Generate installation ID if needed
    if (m_settings.installationId.empty()) {
        m_settings.installationId = GenerateUUID();
    }

    std::ifstream f(m_filePath);
    if (!f.is_open()) {
        std::error_code error;
        if (std::filesystem::exists(m_filePath, error) || error) {
            LOG_ERROR("Settings file exists but cannot be read");
            return false;
        }
        LOG_INFO("Settings file not found — using defaults");
        return true; // First launch, defaults are fine
    }

    try {
        json j;
        f >> j;
        if (!j.is_object()) throw std::runtime_error("Settings must be a JSON object");
        auto s = m_settings;

        // General
        if (j.contains("language"))        s.language = Utf8ToWide(j["language"].get<std::string>());
        if (j.contains("startWithWindows")) s.startWithWindows = j["startWithWindows"].get<bool>();
        if (j.contains("theme"))           s.theme = Utf8ToWide(j["theme"].get<std::string>());
        if (j.contains("colorTheme"))      s.colorTheme = (ColorTheme)std::clamp(j["colorTheme"].get<int>(), 0, 5);
        if (j.contains("accentColor"))     s.accentColor = HexToColor(j["accentColor"].get<std::string>());
        if (j.contains("installationId")) {
            s.installationId = Utf8ToWide(j["installationId"].get<std::string>());
        }

        // Reminders enabled
        if (j.contains("remindersEnabled")) s.remindersEnabled = j["remindersEnabled"].get<bool>();

        // Weekly schedule (7 days)
        if (j.contains("weeklySchedule")) {
            if (!j["weeklySchedule"].is_array()) throw std::runtime_error("weeklySchedule must be an array");
            auto& arr = j["weeklySchedule"];
            for (size_t i = 0; i < std::min((size_t)7, arr.size()); ++i) {
                auto& d = arr[i];
                if (!d.is_object()) throw std::runtime_error("Day schedule must be an object");
                auto& ds = s.weeklySchedule[i];
                if (d.contains("enabled"))          ds.enabled = d["enabled"].get<bool>();
                if (d.contains("startHour"))        ds.startHour = d["startHour"].get<int>();
                if (d.contains("startMin"))         ds.startMin = d["startMin"].get<int>();
                if (d.contains("endHour"))          ds.endHour = d["endHour"].get<int>();
                if (d.contains("endMin"))           ds.endMin = d["endMin"].get<int>();
                if (d.contains("frequencyMinutes")) ds.frequencyMinutes = d["frequencyMinutes"].get<int>();
            }
        }

        // Quiet periods
        if (j.contains("quietPeriods")) {
            if (!j["quietPeriods"].is_array()) throw std::runtime_error("quietPeriods must be an array");
            s.quietPeriods.clear();
            for (auto& qp : j["quietPeriods"]) {
                if (!qp.is_object()) throw std::runtime_error("Quiet period must be an object");
                QuietPeriod q;
                if (qp.contains("startHour")) q.startHour = qp["startHour"].get<int>();
                if (qp.contains("startMin"))  q.startMin  = qp["startMin"].get<int>();
                if (qp.contains("endHour"))   q.endHour   = qp["endHour"].get<int>();
                if (qp.contains("endMin"))    q.endMin    = qp["endMin"].get<int>();
                s.quietPeriods.push_back(q);
            }
        }

        // Popup Appearance & Display
        if (j.contains("popupPosition"))    s.popupPosition = (PopupPosition)j["popupPosition"].get<int>();
        if (j.contains("popupMonitor"))     s.popupMonitor = (PopupMonitor)j["popupMonitor"].get<int>();
        if (j.contains("specificMonitor"))  s.specificMonitor = j["specificMonitor"].get<int>();
        if (j.contains("popupDurationSec")) s.popupDurationSec = j["popupDurationSec"].get<int>();
        if (j.contains("animationEnabled")) s.animationEnabled = j["animationEnabled"].get<bool>();
        if (j.contains("popupAnimation"))   s.popupAnimation = (PopupAnimation)std::clamp(j["popupAnimation"].get<int>(), 0, 7);
        if (j.contains("animationSpeed"))   s.animationSpeed = (AnimationSpeed)std::clamp(j["animationSpeed"].get<int>(), 0, 2);
        if (j.contains("popupSurface"))     s.popupSurface = (PopupSurfaceStyle)std::clamp(j["popupSurface"].get<int>(), 0, 2);
        if (j.contains("popupCorner"))      s.popupCorner = (PopupCornerStyle)std::clamp(j["popupCorner"].get<int>(), 0, 2);
        if (j.contains("arabesqueOverlay")) s.arabesqueOverlay = j["arabesqueOverlay"].get<bool>();

        // Typography & Content
        if (j.contains("popupFontSize"))    s.popupFontSize = (PopupFontSize)std::clamp(j["popupFontSize"].get<int>(), 0, 2);
        if (j.contains("showTashkeel"))     s.showTashkeel = j["showTashkeel"].get<bool>();
        if (j.contains("showCounterPill"))  s.showCounterPill = j["showCounterPill"].get<bool>();
        if (j.contains("showSource"))       s.showSource = j["showSource"].get<bool>();

        // Sound
        if (j.contains("soundEnabled"))    s.soundEnabled = j["soundEnabled"].get<bool>();
        if (j.contains("soundPreset"))     s.soundPreset = (SoundPreset)std::clamp(j["soundPreset"].get<int>(), 0, 4);
        if (j.contains("soundVolume"))     s.soundVolume = std::clamp(j["soundVolume"].get<int>(), 0, 100);
        if (j.contains("customSoundPath")) s.customSoundPath = Utf8ToWide(j["customSoundPath"].get<std::string>());

        // Full-screen & Distraction Awareness
        if (j.contains("suppressDuringFullscreen"))  s.suppressDuringFullscreen  = j["suppressDuringFullscreen"].get<bool>();
        if (j.contains("fullscreenSuppressionMode")) s.fullscreenSuppressionMode = std::clamp(j["fullscreenSuppressionMode"].get<int>(), 0, 2);


        // Internal
        if (j.contains("firstLaunch"))   s.firstLaunch = j["firstLaunch"].get<bool>();
        if (j.contains("paused"))        s.paused = j["paused"].get<bool>();
        if (j.contains("pauseUntilUnix")) s.pauseUntilUnix = j["pauseUntilUnix"].get<int64_t>();

        Validate(s);
        m_settings = std::move(s);
        LOG_INFO("Settings loaded successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to parse settings: ") + e.what());
        return false;
    }
}

bool Settings::Save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_filePath.empty()) return false;

    try {
        auto& s = m_settings;
        json j;

        // General
        j["language"]        = WideToUtf8(s.language);
        j["startWithWindows"] = s.startWithWindows;
        j["theme"]           = "dark";
        j["colorTheme"]      = (int)s.colorTheme;
        j["accentColor"]     = ColorToHex(s.accentColor);
        j["installationId"]  = WideToUtf8(s.installationId);

        // Reminders
        j["remindersEnabled"] = s.remindersEnabled;

        // Weekly schedule
        json schedArr = json::array();
        for (int i = 0; i < 7; ++i) {
            auto& ds = s.weeklySchedule[i];
            json d;
            d["enabled"]          = ds.enabled;
            d["startHour"]        = ds.startHour;
            d["startMin"]         = ds.startMin;
            d["endHour"]          = ds.endHour;
            d["endMin"]           = ds.endMin;
            d["frequencyMinutes"] = ds.frequencyMinutes;
            schedArr.push_back(d);
        }
        j["weeklySchedule"] = schedArr;

        // Quiet periods
        json qpArr = json::array();
        for (auto& qp : s.quietPeriods) {
            json q;
            q["startHour"] = qp.startHour;
            q["startMin"]  = qp.startMin;
            q["endHour"]   = qp.endHour;
            q["endMin"]    = qp.endMin;
            qpArr.push_back(q);
        }
        j["quietPeriods"] = qpArr;

        // Popup Appearance & Display
        j["popupPosition"]    = (int)s.popupPosition;
        j["popupMonitor"]     = (int)s.popupMonitor;
        j["specificMonitor"]  = s.specificMonitor;
        j["popupDurationSec"] = s.popupDurationSec;
        j["animationEnabled"] = s.animationEnabled;
        j["popupAnimation"]   = (int)s.popupAnimation;
        j["animationSpeed"]   = (int)s.animationSpeed;
        j["popupSurface"]     = (int)s.popupSurface;
        j["popupCorner"]      = (int)s.popupCorner;
        j["arabesqueOverlay"] = s.arabesqueOverlay;

        // Typography & Content
        j["popupFontSize"]    = (int)s.popupFontSize;
        j["showTashkeel"]     = s.showTashkeel;
        j["showCounterPill"]  = s.showCounterPill;
        j["showSource"]       = s.showSource;

        // Sound
        j["soundEnabled"]    = s.soundEnabled;
        j["soundPreset"]     = (int)s.soundPreset;
        j["soundVolume"]     = s.soundVolume;
        j["customSoundPath"] = WideToUtf8(s.customSoundPath);

        // Full-screen & Distraction Awareness
        j["suppressDuringFullscreen"]  = s.suppressDuringFullscreen;
        j["fullscreenSuppressionMode"] = s.fullscreenSuppressionMode;


        // Internal
        j["firstLaunch"]    = s.firstLaunch;
        j["paused"]         = s.paused;
        j["pauseUntilUnix"] = s.pauseUntilUnix;

        if (!WriteFileAtomically(m_filePath, j.dump(2))) {
            LOG_ERROR("Failed to replace settings file; previous file retained");
            return false;
        }

        LOG_INFO("Settings saved");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to save settings: ") + e.what());
        return false;
    }
}

bool Settings::BackupInvalidFile() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto name = "settings-invalid-" + std::to_string(time(nullptr)) + "-" + std::to_string(attempt) + ".json";
        std::error_code error;
        if (std::filesystem::copy_file(m_filePath, m_filePath.parent_path() / name, error)) return true;
        if (error != std::errc::file_exists) return false;
    }
    return false;
}

void Settings::NotifyChanged() {
    std::vector<ChangeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_changeCallbacks;
    }
    for (auto& cb : callbacks) {
        if (cb) cb();
    }
}

AppSettings Settings::GetCopy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings;
}

void Settings::Update(const AppSettings& newSettings) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_settings = newSettings;
        Validate(m_settings);
    }
    NotifyChanged();
}

void Settings::UpdatePreferences(const AppSettings& newSettings) {
    Modify([&](AppSettings& current) {
        auto replacement = newSettings;
        replacement.paused = current.paused;
        replacement.pauseUntilUnix = current.pauseUntilUnix;
        replacement.firstLaunch = current.firstLaunch;
        replacement.installationId = current.installationId;
        replacement.lastUpdateCheck = current.lastUpdateCheck;
        replacement.pendingUpdateVersion = current.pendingUpdateVersion;
        replacement.pendingUpdateUrl = current.pendingUpdateUrl;
        current = std::move(replacement);
    });
}

void Settings::Modify(const std::function<void(AppSettings&)>& change) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        change(m_settings);
        Validate(m_settings);
    }
    NotifyChanged();
}

void Settings::Validate(AppSettings& settings) {
    for (auto& day : settings.weeklySchedule) {
        day.startHour = std::clamp(day.startHour, 0, 23);
        day.endHour = std::clamp(day.endHour, 0, 23);
        day.startMin = std::clamp(day.startMin, 0, 59);
        day.endMin = std::clamp(day.endMin, 0, 59);
        day.frequencyMinutes = std::clamp(day.frequencyMinutes, 1, 1440);
    }
    for (auto& quiet : settings.quietPeriods) {
        quiet.startHour = std::clamp(quiet.startHour, 0, 23);
        quiet.endHour = std::clamp(quiet.endHour, 0, 23);
        quiet.startMin = std::clamp(quiet.startMin, 0, 59);
        quiet.endMin = std::clamp(quiet.endMin, 0, 59);
    }
    settings.popupDurationSec = std::clamp(settings.popupDurationSec, 2, 60);
    settings.specificMonitor = std::max(0, settings.specificMonitor);
    settings.popupPosition = static_cast<PopupPosition>(std::clamp(static_cast<int>(settings.popupPosition), 0, 8));
    settings.popupMonitor = static_cast<PopupMonitor>(std::clamp(static_cast<int>(settings.popupMonitor), 0, 3));
    settings.popupFontSize = static_cast<PopupFontSize>(std::clamp(static_cast<int>(settings.popupFontSize), 0, 2));
    settings.popupAnimation = static_cast<PopupAnimation>(std::clamp(static_cast<int>(settings.popupAnimation), 0, 7));
    settings.animationSpeed = static_cast<AnimationSpeed>(std::clamp(static_cast<int>(settings.animationSpeed), 0, 2));
    settings.popupSurface = static_cast<PopupSurfaceStyle>(std::clamp(static_cast<int>(settings.popupSurface), 0, 2));
    settings.popupCorner = static_cast<PopupCornerStyle>(std::clamp(static_cast<int>(settings.popupCorner), 0, 2));
    settings.colorTheme = static_cast<ColorTheme>(std::clamp(static_cast<int>(settings.colorTheme), 0, 5));
    settings.soundPreset = static_cast<SoundPreset>(std::clamp(static_cast<int>(settings.soundPreset), 0, 4));
    settings.fullscreenSuppressionMode = std::clamp(settings.fullscreenSuppressionMode, 0, 2);
    settings.soundVolume = std::clamp(settings.soundVolume, 0, 100);
    settings.pauseUntilUnix = std::max<int64_t>(0, settings.pauseUntilUnix);
    if (settings.language != L"ar" && settings.language != L"en" && settings.language != L"system") settings.language = L"en";
}

} // namespace dhikr
