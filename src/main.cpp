#include "core/Application.h"
#include "ui/MainWindow.h"
#include "ui/ScreenshotExporter.h"
#include "resource.h"
#include <windows.h>
#include <shobjidl.h>
#include <string>

// ── Entry point ───────────────────────────────────────────────────────────────
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE /*hPrevInstance*/,
                      LPWSTR    lpCmdLine,
                      int       /*nCmdShow*/) {
    // ── Command line flags: Screenshots export (isolated headless tool) ────
    if (lpCmdLine && wcsstr(lpCmdLine, L"--export-screenshots") != nullptr) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        auto& app = dhikr::Application::Instance();
        if (app.Initialize(hInstance)) {
            dhikr::MainWindow::Instance().Create(hInstance);

            std::wstring outDir = L"screenshots";
            const wchar_t* p = wcsstr(lpCmdLine, L"--export-screenshots");
            if (p) {
                p += wcslen(L"--export-screenshots");
                while (*p == L' ' || *p == L'\t') p++;
                if (*p) {
                    outDir = p;
                    while (!outDir.empty() && (outDir.back() == L' ' || outDir.back() == L'\t' || outDir.back() == L'\r' || outDir.back() == L'\n')) {
                        outDir.pop_back();
                    }
                    if (outDir.size() >= 2 && outDir.front() == L'"' && outDir.back() == L'"') {
                        outDir = outDir.substr(1, outDir.size() - 2);
                    }
                }
            }
            dhikr::ScreenshotExporter::ExportAll(outDir);
            app.Shutdown();
        }
        CoUninitialize();
        return 0;
    }

    // ── Single instance enforcement ────────────────────────────────────────
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"AzkarReminderSingleInstanceV1_2");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = nullptr;
        for (int attempt = 0; attempt < 15; ++attempt) {
            existing = FindWindowW(L"DhikrMain", nullptr);
            if (existing) break;
            Sleep(100);
        }

        // Handle --quit flag (from uninstaller)
        if (lpCmdLine && wcsstr(lpCmdLine, L"--quit") != nullptr) {
            if (existing) {
                // ID_TRAY_EXIT is 2008
                PostMessageW(existing, WM_COMMAND, 2008, 0);
            }
            CloseHandle(hMutex);
            return 0;
        }

        // Normal launch: Bring existing window to foreground
        if (existing) {
            // PostMessage to tell existing window thread to show itself
            PostMessageW(existing, WM_COMMAND, 2001 /* ID_TRAY_OPEN */, 0);
            ShowWindow(existing, SW_SHOW);
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(hMutex);
        return 0;
    }

    // If --quit was passed but no instance was running, just exit
    if (lpCmdLine && wcsstr(lpCmdLine, L"--quit") != nullptr) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    if (lpCmdLine && wcsstr(lpCmdLine, L"--setup-autostart") != nullptr) {
        dhikr::Application::Instance().SetStartWithWindows(true);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    if (lpCmdLine && wcsstr(lpCmdLine, L"--remove-autostart") != nullptr) {
        dhikr::Application::Instance().SetStartWithWindows(false);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // ── COM initialization (needed for tray, shell, dialogs) ──────────────
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // ── Set explicit AppUserModelID for Taskbar icon & shortcut binding ────
    SetCurrentProcessExplicitAppUserModelID(L"AhmedMohamedKamel.AzkarReminder");

    // ── DPI awareness ─────────────────────────────────────────────────────
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // ── Check for --background flag ────────────────────────────────────────
    bool startBackground = (lpCmdLine && wcsstr(lpCmdLine, L"--background") != nullptr);

    // ── Initialize application ─────────────────────────────────────────────
    auto& app = dhikr::Application::Instance();
    if (!app.Initialize(hInstance)) {
        MessageBoxW(nullptr,
            L"Azkar Reminder failed to start.\n\nPlease check the log file in %LOCALAPPDATA%\\Azkar Reminder\\logs.",
            L"Azkar Reminder — Startup Error",
            MB_ICONERROR | MB_OK);
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // ── Run ────────────────────────────────────────────────────────────────
    int result = app.Run();

    // ── Cleanup ────────────────────────────────────────────────────────────
    app.Shutdown();
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);

    return result;
}
