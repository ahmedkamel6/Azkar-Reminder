#include "WindowsAutoStart.h"
#include "../../core/Logger.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace dhikr {

std::unique_ptr<IAutoStart> IAutoStart::Create() {
    return std::make_unique<WindowsAutoStart>();
}

bool WindowsAutoStart::IsAutoStartEnabled() const {
    HKEY hKey = nullptr;
    LSTATUS status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_QUERY_VALUE,
        &hKey
    );
    if (status != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD dataSize = 0;
    status = RegQueryValueExW(hKey, L"AzkarReminder", nullptr, &type, nullptr, &dataSize);
    RegCloseKey(hKey);

    return (status == ERROR_SUCCESS);
}

bool WindowsAutoStart::SetAutoStartEnabled(bool enable, const std::filesystem::path& appPath) {
    HKEY hKey = nullptr;
    const wchar_t* runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    LSTATUS status = enable
        ? RegCreateKeyExW(HKEY_CURRENT_USER, runKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                          KEY_SET_VALUE, nullptr, &hKey, nullptr)
        : RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_SET_VALUE, &hKey);
    if (!enable && status == ERROR_FILE_NOT_FOUND) return true;
    if (status != ERROR_SUCCESS) {
        LOG_ERROR("Failed to open HKCU Run registry key, status=" + std::to_string(status));
        return false;
    }

    const wchar_t* valName = L"AzkarReminder";

    if (!enable) {
        RegDeleteValueW(hKey, valName);
        RegDeleteValueW(hKey, L"DhikrReminder");
        RegCloseKey(hKey);
        LOG_INFO("Auto-start with Windows disabled (registry value removed)");
        return true;
    }

    std::wstring exeStr;
    if (!appPath.empty()) {
        exeStr = appPath.wstring();
    } else {
        wchar_t exeBuf[MAX_PATH] = { 0 };
        DWORD len = GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) {
            RegCloseKey(hKey);
            LOG_ERROR("Failed to get executable module file path");
            return false;
        }
        exeStr = exeBuf;
    }

    // Command: "E:\path\to\AzkarReminder.exe" --background
    std::wstring cmd = L"\"" + exeStr + L"\" --background";

    status = RegSetValueExW(
        hKey,
        valName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(cmd.c_str()),
        static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t))
    );

    // Clean up any old key name
    RegDeleteValueW(hKey, L"DhikrReminder");
    RegCloseKey(hKey);

    if (status == ERROR_SUCCESS) {
        LOG_INFO("Auto-start with Windows enabled successfully via HKCU Run");
        return true;
    } else {
        LOG_ERROR("Failed to set HKCU Run value, status=" + std::to_string(status));
        return false;
    }
}

} // namespace dhikr
#endif
