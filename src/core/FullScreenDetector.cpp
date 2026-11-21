#include "FullScreenDetector.h"
#include <algorithm>
#include <cstring>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace dhikr {

namespace {

bool IsWindowFullScreenOnMonitor(HWND hwnd, HMONITOR& outMon) {
    outMon = nullptr;
    if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return false;
    }

    // Never suppress for our own application windows
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) {
        return false;
    }

    // Ignore cloaked UWP/virtual-desktop background windows that still report WS_VISIBLE
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
        return false;
    }

    // Exclude Windows shell / desktop windows
    if (hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) {
        return false;
    }

    wchar_t cls[64] = { 0 };
    if (GetClassNameW(hwnd, cls, 64) > 0) {
        if (wcscmp(cls, L"Progman") == 0 ||
            wcscmp(cls, L"WorkerW") == 0 ||
            wcscmp(cls, L"Shell_TrayWnd") == 0 ||
            wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0) {
            return false;
        }
    }

    // Ignore transparent / tool overlays (e.g., FPS counters, screen overlays)
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & (WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT)) {
        return false;
    }

    // A maximized desktop window is NOT a full-screen application.
    // Users actively browse the web, write code, or read in maximized apps.
    if (IsZoomed(hwnd)) {
        return false;
    }

    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    if (style & WS_MAXIMIZE) {
        return false;
    }

    // Resizable overlapping windows with thick resizing borders are regular desktop apps
    if ((style & WS_THICKFRAME) && !(style & WS_POPUP)) {
        return false;
    }

    // Check if the window bounds completely cover its monitor
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (!hMon) return false;

    MONITORINFO mi{ sizeof(mi) };
    if (!GetMonitorInfoW(hMon, &mi)) return false;

    RECT rcWin;
    if (!GetWindowRect(hwnd, &rcWin)) return false;

    bool coversMonitor = (rcWin.left   <= mi.rcMonitor.left &&
                          rcWin.top    <= mi.rcMonitor.top &&
                          rcWin.right  >= mi.rcMonitor.right &&
                          rcWin.bottom >= mi.rcMonitor.bottom);
    if (!coversMonitor) {
        return false;
    }

    // Distinguish true fullscreen / borderless applications from normal maximized windows
    if ((style & WS_CAPTION) == WS_CAPTION) {
        POINT ptTopLeft = { 0, 0 };
        ClientToScreen(hwnd, &ptTopLeft);
        if (ptTopLeft.y > mi.rcMonitor.top + 10) {
            return false;
        }
    }

    outMon = hMon;
    return true;
}

BOOL CALLBACK EnumFullScreenWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* mons = reinterpret_cast<std::vector<HMONITOR>*>(lParam);
    HMONITOR hMon = nullptr;
    if (IsWindowFullScreenOnMonitor(hwnd, hMon) && hMon) {
        if (std::find(mons->begin(), mons->end(), hMon) == mons->end()) {
            mons->push_back(hMon);
        }
    }
    return TRUE;
}

} // namespace

FullScreenDetector& FullScreenDetector::Instance() {
    static FullScreenDetector instance;
    return instance;
}

std::vector<HMONITOR> FullScreenDetector::GetFullScreenMonitors() const {
    std::vector<HMONITOR> fsMonitors;

    // 1. Enumerate all visible top-level windows across all monitors so background
    //    fullscreen videos/games on another monitor are detected even when the user
    //    clicks or works on a different monitor.
    EnumWindows(EnumFullScreenWindowsProc, reinterpret_cast<LPARAM>(&fsMonitors));

    // 2. Also inspect Direct3D exclusive fullscreen / presentation mode state.
    // Note: Do NOT check QUNS_BUSY here as Windows 10/11 frequently returns QUNS_BUSY (2)
    // during ordinary desktop sessions.
    QUERY_USER_NOTIFICATION_STATE qState;
    if (SUCCEEDED(SHQueryUserNotificationState(&qState))) {
        if (qState == QUNS_RUNNING_D3D_FULL_SCREEN ||
            qState == QUNS_PRESENTATION_MODE) {
            HWND fg = GetForegroundWindow();
            HMONITOR fgMon = fg ? MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST)
                                : MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
            if (fgMon && std::find(fsMonitors.begin(), fsMonitors.end(), fgMon) == fsMonitors.end()) {
                fsMonitors.push_back(fgMon);
            }
        }
    }

    return fsMonitors;
}

bool FullScreenDetector::IsMonitorFullScreen(HMONITOR hMon) const {
    if (!hMon) return false;
    auto fsMonitors = GetFullScreenMonitors();
    return std::find(fsMonitors.begin(), fsMonitors.end(), hMon) != fsMonitors.end();
}

bool FullScreenDetector::IsFullScreenActive() const {
    return !GetFullScreenMonitors().empty();
}

} // namespace dhikr
