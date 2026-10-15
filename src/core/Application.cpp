#include "Application.h"
#include "Logger.h"
#include "Settings.h"
#include "Database.h"
#include "ContentEngine.h"
#include "Scheduler.h"
#include "ApplicationMetadata.h"
#include "Localization.h"
#include "../ui/MainWindow.h"
#include "../ui/TrayIcon.h"
#include "../ui/OnboardingFlow.h"
#include "../audio/AudioEngine.h"
#include "../update/Updater.h"
#include "../../src/resource.h"
#include <shlobj.h>
#include <sstream>
#include <filesystem>
#include <comdef.h>
#include <taskschd.h>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")

namespace dhikr {

Application& Application::Instance() {
    static Application instance;
    return instance;
}


bool Application::Initialize(HINSTANCE hInstance) {
    if (m_initialized) return true;
    m_hInstance = hInstance;

    // 1. App data path first (needed by logger)
    if (!InitAppDataPath()) return false;

    // 2. Logger
    Logger::Instance().Initialize(m_appDataPath + L"\\logs");
    char verBuf[64];
    WideCharToMultiByte(CP_UTF8, 0, ApplicationMetadata::AppVersion, -1, verBuf, sizeof(verBuf), nullptr, nullptr);
    LOG_INFO(std::string("=== AzkarReminder v") + verBuf + " starting ===");

    // 2.5 Load Cairo Arabic Font into process memory
    HRSRC hFontRes = FindResourceW(hInstance, MAKEINTRESOURCEW(IDR_FONT_CAIRO), RT_RCDATA);
    if (hFontRes) {
        HGLOBAL hMem = LoadResource(hInstance, hFontRes);
        if (hMem) {
            void* pData = LockResource(hMem);
            DWORD size = SizeofResource(hInstance, hFontRes);
            DWORD numFonts = 0;
            m_hCairoFont = AddFontMemResourceEx(pData, size, nullptr, &numFonts);
            if (m_hCairoFont) {
                LOG_INFO("Cairo Arabic font loaded into process (" + std::to_string(numFonts) + " font registered)");
            }
        }
    }

    // 3. Settings & Localization
    if (!Settings::Instance().Load(m_appDataPath)) {
        if (!Settings::Instance().BackupInvalidFile()) {
            LOG_ERROR("Could not preserve malformed settings");
            return false;
        }
        LOG_WARN("Malformed settings backed up; defaults are active");
        if (!Settings::Instance().Save()) LOG_WARN("Default settings could not be persisted");
    }
    m_settingsReady = true;
    std::wstring lang = Settings::Instance().GetCopy().language;
    if (lang == L"system") {
        lang = (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_ARABIC) ? L"ar" : L"en";
    }
    Localization::Instance().SetLanguage(lang);

    // 4. Database
    if (!Database::Instance().Open(m_appDataPath + L"\\history.db")) {
        LOG_WARN("Database unavailable — statistics and history disabled");
    }

    // 5. DirectX factories
    if (!InitDirectX()) {
        LOG_ERROR("DirectX initialization failed");
        return false;
    }

    // 6. Content engine — load user's saved content first, then fall back to assets/content.md
    {
        std::wstring userContent = m_appDataPath + L"\\content.json";
        ContentEngine::Instance().SetSavePath(userContent);
        ContentLoadResult loadResult = ContentLoadResult::FileNotFound;
        if (std::filesystem::exists(userContent)) {
            loadResult = ContentEngine::Instance().Load(userContent);
            if (loadResult == ContentLoadResult::Success) {
                LOG_INFO("Loaded user content.json successfully (" + std::to_string(ContentEngine::Instance().Count()) + " items)");
            } else {
                loadResult = ContentLoadResult::EmptyContent;
            }
        }

        if (loadResult != ContentLoadResult::Success) {
            loadResult = ContentEngine::Instance().LoadFromMarkdownResource(m_hInstance, IDR_CONTENT_MD);
            if (loadResult == ContentLoadResult::Success && !ContentEngine::Instance().IsEmpty()) {
                LOG_INFO("Loaded default content from embedded resource (" + std::to_string(ContentEngine::Instance().Count()) + " items)");
                if (!std::filesystem::exists(userContent)) ContentEngine::Instance().Save();
            } else {
                LOG_ERROR("Failed to load embedded content resource. Ensure IDR_CONTENT_MD is properly compiled into the executable.");
            }
        }

        if (ContentEngine::Instance().IsEmpty()) {
            LOG_INFO("Library is empty; automatic reminders have no content");
        }
    }

    // 7. Audio engine
    AudioEngine::Instance().Initialize(m_hInstance);

    // 8. Sync startup registry with settings
    SetStartWithWindows(Settings::Instance().GetCopy().startWithWindows);

    m_initialized = true;
    return true;
}

bool Application::InitAppDataPath() {
    wchar_t* appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData))) {
        return false;
    }
    m_appDataPath = std::wstring(appData) + L"\\Azkar Reminder";
    std::wstring oldPath = std::wstring(appData) + L"\\Dhikr Reminder";
    CoTaskMemFree(appData);

    // Create directory if needed
    if (!CreateDirectoryW(m_appDataPath.c_str(), nullptr)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
    }

    // Auto-migrate settings/database/content from the legacy path ONLY if needed
    // DO NOT migrate from executable path as it breaks installations
    
    std::wstring newDb = m_appDataPath + L"\\history.db";
    std::wstring newSettings = m_appDataPath + L"\\settings.json";
    std::wstring newContent = m_appDataPath + L"\\content.json";

    auto migrateFile = [](const std::wstring& oldFile, const std::wstring& newFile) {
        if (GetFileAttributesW(newFile.c_str()) == INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesW(oldFile.c_str()) != INVALID_FILE_ATTRIBUTES) {
            CopyFileW(oldFile.c_str(), newFile.c_str(), TRUE);
            return true;
        }
        return false;
    };

    // Migrate database (older version used dhikr.db, new version expects history.db)
    if (!migrateFile(oldPath + L"\\history.db", newDb)) {
        migrateFile(oldPath + L"\\dhikr.db", newDb);
    }
    
    migrateFile(oldPath + L"\\settings.json", newSettings);
    migrateFile(oldPath + L"\\content.json", newContent);

    return true;
}

bool Application::InitDirectX() {
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_MULTI_THREADED,
        &m_d2dFactory);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create D2D factory");
        return false;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_dwriteFactory));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create DWrite factory");
        return false;
    }

    return true;
}

int Application::Run() {
    auto settings = Settings::Instance().GetCopy();

    // Show onboarding on first launch
    if (settings.firstLaunch) {
        OnboardingFlow onboarding;
        onboarding.Show(m_hInstance);
        Settings::Instance().Modify([](AppSettings& current) { current.firstLaunch = false; });
        Settings::Instance().Save();
    }

    // Create main window
    if (!MainWindow::Instance().Create(m_hInstance)) {
        LOG_ERROR("Failed to create main window");
        return 1;
    }
    m_mainHwnd = MainWindow::Instance().GetHwnd();

    // Show the window unless launched with --background.
    // The --background flag is used by the startup registry entry so the
    // app silently starts in the tray without opening a visible window.
    bool startBackground = (wcsstr(GetCommandLineW(), L"--background") != nullptr);
    if (!startBackground) {
        MainWindow::Instance().Show();
    } else {
        // App started silently in background tray — trim initial driver heap to drop working set to ~3 MB
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    }

    // Create tray icon (and start retry timer if Shell is not ready yet)
    if (!TrayIcon::Instance().Create(m_mainHwnd, m_hInstance)) {
        SetTimer(m_mainHwnd, 1003 /* TIMER_TRAY_RETRY */, 3000, nullptr);
    }

    // Start scheduler — it posts WM_REMINDER_TRIGGER to main window
    Scheduler::Instance().Start(m_mainHwnd);

    // Check for updates (async, once per 24h)
    // NOTE: Updater::CheckIfDue was intentionally removed to avoid ML heuristics.
    // The application no longer phones home silently in the background.

    // ── Message loop ───────────────────────────────────────────────────────
    LOG_INFO("Entering message loop");
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    LOG_INFO("Exited message loop, msg=" + std::to_string(msg.message) + " wParam=" + std::to_string(msg.wParam));

    return (int)msg.wParam;
}

void Application::Shutdown() {
    if (m_shuttingDown) return;
    m_shuttingDown = true;

    LOG_INFO("Application shutting down");

    Scheduler::Instance().Stop();
    TrayIcon::Instance().Remove();
    AudioEngine::Instance().Shutdown();

    Updater::Instance().Shutdown();
    Database::Instance().Checkpoint();
    Database::Instance().Close();

    if (m_settingsReady) Settings::Instance().Save();

    if (m_dwriteFactory) { m_dwriteFactory->Release(); m_dwriteFactory = nullptr; }
    if (m_d2dFactory)    { m_d2dFactory->Release();    m_d2dFactory    = nullptr; }

    if (m_hCairoFont) {
        RemoveFontMemResourceEx(m_hCairoFont);
        m_hCairoFont = nullptr;
    }

    LOG_INFO("Application shutdown complete");
}

bool Application::SetStartWithWindows(bool enable) {
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

    wchar_t exePath[MAX_PATH] = { 0 };
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        RegCloseKey(hKey);
        LOG_ERROR("Failed to get executable module file path");
        return false;
    }

    // Command: "E:\path\to\AzkarReminder.exe" --background
    std::wstring cmd = L"\"" + std::wstring(exePath) + L"\" --background";

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

bool Application::GetStartsWithWindows() const {
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

} // namespace dhikr
