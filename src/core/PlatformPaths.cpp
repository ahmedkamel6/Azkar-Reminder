#include "PlatformPaths.h"
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace dhikr {

namespace {

#ifndef _WIN32
std::filesystem::path GetHomeDir() {
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::filesystem::path(home);
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return std::filesystem::path(pw->pw_dir);
    }
    return std::filesystem::current_path();
}
#endif

} // namespace

std::filesystem::path PlatformPaths::GetConfigDir() {
#ifdef _WIN32
    wchar_t* appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData))) {
        std::filesystem::path p = std::filesystem::path(appData) / L"Azkar Reminder";
        CoTaskMemFree(appData);
        return p;
    }
    return std::filesystem::current_path() / L"Azkar Reminder";
#elif defined(__APPLE__)
    return GetHomeDir() / "Library" / "Application Support" / "Azkar Reminder";
#else
    // Linux XDG_CONFIG_HOME
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && *xdgConfig && std::filesystem::path(xdgConfig).is_absolute()) {
        return std::filesystem::path(xdgConfig) / "azkar-reminder";
    }
    return GetHomeDir() / ".config" / "azkar-reminder";
#endif
}

std::filesystem::path PlatformPaths::GetDataDir() {
#ifdef _WIN32
    return GetConfigDir(); // On Windows, config & data live together in LocalAppData
#elif defined(__APPLE__)
    return GetHomeDir() / "Library" / "Application Support" / "Azkar Reminder";
#else
    // Linux XDG_DATA_HOME
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData && *xdgData && std::filesystem::path(xdgData).is_absolute()) {
        return std::filesystem::path(xdgData) / "azkar-reminder";
    }
    return GetHomeDir() / ".local" / "share" / "azkar-reminder";
#endif
}

std::filesystem::path PlatformPaths::GetLogsDir() {
#ifdef _WIN32
    return GetConfigDir() / L"logs";
#elif defined(__APPLE__)
    return GetHomeDir() / "Library" / "Logs" / "Azkar Reminder";
#else
    // Linux XDG_STATE_HOME or fallback to data/logs
    const char* xdgState = std::getenv("XDG_STATE_HOME");
    if (xdgState && *xdgState && std::filesystem::path(xdgState).is_absolute()) {
        return std::filesystem::path(xdgState) / "azkar-reminder" / "logs";
    }
    return GetDataDir() / "logs";
#endif
}

bool PlatformPaths::EnsureDirectoriesExist() {
    try {
        std::filesystem::create_directories(GetConfigDir());
        std::filesystem::create_directories(GetDataDir());
        std::filesystem::create_directories(GetLogsDir());
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace dhikr
