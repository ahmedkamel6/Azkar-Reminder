#include "Settings.h"
#include "Logger.h"
#include "../third_party/json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <random>
#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#endif

using json = nlohmann::json;

namespace dhikr {

// Helper: UTF-8 <-> UTF-16
static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
#ifdef _WIN32
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return "";
    std::string result(size - 1, 0);
    int written = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
    if (written <= 0) return "";
    return result;
#else
    std::string result;
    for (wchar_t wc : wstr) {
        if (wc < 0x80) {
            result.push_back((char)wc);
        } else if (wc < 0x800) {
            result.push_back((char)(0xC0 | (wc >> 6)));
            result.push_back((char)(0x80 | (wc & 0x3F)));
        } else {
            result.push_back((char)(0xE0 | (wc >> 12)));
            result.push_back((char)(0x80 | ((wc >> 6) & 0x3F)));
            result.push_back((char)(0x80 | (wc & 0x3F)));
        }
    }
    return result;
#endif
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
#ifdef _WIN32
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 1) return L"";
    std::wstring result(size - 1, 0);
    int written = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
    if (written <= 0) return L"";
    return result;
#else
    std::wstring result;
    for (size_t i = 0; i < str.size();) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x80) {
            result.push_back((wchar_t)c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < str.size()) {
            wchar_t wc = ((c & 0x1F) << 6) | (str[i + 1] & 0x3F);
            result.push_back(wc);
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < str.size()) {
            wchar_t wc = ((c & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
            result.push_back(wc);
            i += 3;
        } else {
            i += 1; // skip malformed
        }
    }
    return result;
#endif
}

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
        LOG_INFO("Settings file not found — using defaults");
        return true; // First launch, defaults are fine
    }

    try {
        json j;
        f >> j;
        auto& s = m_settings;

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
        if (j.contains("weeklySchedule") && j["weeklySchedule"].is_array()) {
            auto& arr = j["weeklySchedule"];
            for (size_t i = 0; i < std::min((size_t)7, arr.size()); ++i) {
                auto& d = arr[i];
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
        if (j.contains("quietPeriods") && j["quietPeriods"].is_array()) {
            s.quietPeriods.clear();
            for (auto& qp : j["quietPeriods"]) {
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
        if (j.contains("hideFromCapture"))           s.hideFromCapture           = j["hideFromCapture"].get<bool>();


        // Internal
        if (j.contains("firstLaunch"))   s.firstLaunch = j["firstLaunch"].get<bool>();
        if (j.contains("paused"))        s.paused = j["paused"].get<bool>();
        if (j.contains("pauseUntilUnix")) s.pauseUntilUnix = j["pauseUntilUnix"].get<int>();

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
        j["hideFromCapture"]           = s.hideFromCapture;


        // Internal
        j["firstLaunch"]    = s.firstLaunch;
        j["paused"]         = s.paused;
        j["pauseUntilUnix"] = s.pauseUntilUnix;

        // Atomic write: write to temp file then rename
        auto tmpPath = m_filePath;
        tmpPath += ".tmp";
        {
            std::ofstream f(tmpPath);
            if (!f.is_open()) {
                LOG_ERROR("Cannot open settings temp file for writing");
                return false;
            }
            std::string jsonStr = j.dump(2);
            f << jsonStr;
            f.flush();
            if (f.fail()) {
                LOG_ERROR("Failed to write settings to temp file");
                std::error_code ec;
                std::filesystem::remove(tmpPath, ec);
                return false;
            }
        }
        // Atomic replace
        std::error_code ec;
        std::filesystem::rename(tmpPath, m_filePath, ec);
        if (ec) {
            std::filesystem::copy_file(tmpPath, m_filePath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmpPath, ec);
        }

        LOG_INFO("Settings saved");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to save settings: ") + e.what());
        return false;
    }
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
    }
    NotifyChanged();
}

} // namespace dhikr
