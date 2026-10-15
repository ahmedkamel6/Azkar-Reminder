#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>

namespace dhikr {

// Forward declarations
class Settings;
class ContentEngine;
class Database;
class Logger;

// ── Application singleton ─────────────────────────────────────────────────────
class Application {
public:
    static Application& Instance();

    // Initialize all subsystems; returns false on critical failure
    bool Initialize(HINSTANCE hInstance);

    // Run the message loop (blocks until quit)
    int Run();

    // Graceful shutdown
    void Shutdown();

    // Direct2D factory (shared across all windows)
    ID2D1Factory*       D2DFactory()   const { return m_d2dFactory; }
    IDWriteFactory*     DWriteFactory() const { return m_dwriteFactory; }

    // App instance handle
    HINSTANCE HInstance() const { return m_hInstance; }

    // Application data path (e.g. %APPDATA%\DhikrReminder\)
    const std::wstring& AppDataPath() const { return m_appDataPath; }

    // Main window handle
    HWND MainWindow() const { return m_mainHwnd; }
    void SetMainWindow(HWND hwnd) { m_mainHwnd = hwnd; }

    // Content file path
    std::wstring ContentFilePath() const { return m_appDataPath + L"\\content.json"; }

    // Apply startup registry entry
    bool SetStartWithWindows(bool enable);
    bool GetStartsWithWindows() const;

    // ── Application identity ──────────────────────────────────────────────
    // GitHub repository — used exclusively by the update checker.
    // Format: "owner/repo"  (no https:// prefix, no trailing slash)
    static constexpr wchar_t kGitHubRepo[] = L"your-github-username/DhikrReminder";

    // Inno Setup AppId — must remain stable across all updates so Inno
    // treats a new installation as an upgrade, not a fresh install.
    static constexpr wchar_t kAppId[]      = L"{{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}";


    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

private:
    Application() = default;
    ~Application() { Shutdown(); }

    bool InitAppDataPath();
    bool InitDirectX();
    bool InitSubsystems();
    void HandleWake();

    HINSTANCE        m_hInstance     = nullptr;
    HWND             m_mainHwnd      = nullptr;
    std::wstring     m_appDataPath;

    ID2D1Factory*    m_d2dFactory    = nullptr;
    IDWriteFactory*  m_dwriteFactory = nullptr;

    bool             m_initialized   = false;
    bool             m_settingsReady = false;
    bool             m_shuttingDown  = false;
    HANDLE           m_hCairoFont    = nullptr;
};

} // namespace dhikr
