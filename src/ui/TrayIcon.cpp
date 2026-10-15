#include "TrayIcon.h"
#include "core/LogoRenderer.h"
#include "../core/Logger.h"
#include "../core/Scheduler.h"
#include "../core/Settings.h"
#include "../core/ApplicationMetadata.h"
#include "../../src/resource.h"
#include <ctime>
#include <sstream>
#include <iomanip>

namespace dhikr {

TrayIcon& TrayIcon::Instance() {
    static TrayIcon instance;
    return instance;
}

bool TrayIcon::Create(HWND hwndParent, HINSTANCE hInstance) {
    m_hwnd  = hwndParent;
    m_hInst = hInstance;

    int cx = GetSystemMetrics(SM_CXSMICON);
    if (cx <= 0) cx = 16;
    HICON hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, cx, cx, LR_SHARED);
    if (!hIcon) {
        hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    }
    if (!hIcon) {
        hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd             = hwndParent;
    m_nid.uID              = 1;
    m_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAY_CALLBACK;
    m_nid.hIcon            = hIcon;
    std::wstring initialTip = std::wstring(ApplicationMetadata::AppName) + L" — Active";
    wcsncpy_s(m_nid.szTip, initialTip.c_str(), _TRUNCATE);

    // Remove any stale icon from previous run before adding
    Shell_NotifyIconW(NIM_DELETE, &m_nid);

    BOOL ok = FALSE;
    for (int attempt = 0; attempt < 8; ++attempt) {
        m_nid.cbSize = sizeof(NOTIFYICONDATAW);
        ok = Shell_NotifyIconW(NIM_ADD, &m_nid);
        if (ok) break;

        // If icon already exists in Shell cache, NIM_MODIFY will succeed
        if (Shell_NotifyIconW(NIM_MODIFY, &m_nid)) {
            ok = TRUE;
            break;
        }

        // Try V3/V4 size compatibility fallback
        m_nid.cbSize = NOTIFYICONDATAW_V3_SIZE;
        ok = Shell_NotifyIconW(NIM_ADD, &m_nid);
        if (ok) break;

        m_nid.cbSize = NOTIFYICONDATAW_V2_SIZE;
        ok = Shell_NotifyIconW(NIM_ADD, &m_nid);
        if (ok) break;

        Sleep(100);
    }

    if (!ok) {
        DWORD err = GetLastError();
        LOG_WARN("Could not initialize tray icon yet (Explorer shell tray may not be ready, error=" + std::to_string(err) + ")");
        return false;
    }

    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
    m_created = true;

    LOG_INFO("Tray icon created successfully");
    return true;
}

void TrayIcon::UpdateIcon(ColorTheme themeOverride) {
    if (!m_created) return;
    int cx = GetSystemMetrics(SM_CXSMICON);
    if (cx <= 0) cx = 16;
    if (m_dynamicIcon) {
        DestroyIcon(m_dynamicIcon);
        m_dynamicIcon = nullptr;
    }
    int resId = IDI_APP_ICON_SMALL;
    int fallbackRes = IDI_APP_ICON;

    ColorTheme currentTheme = (themeOverride != static_cast<ColorTheme>(-1)) 
        ? themeOverride 
        : Settings::Instance().GetCopy().colorTheme;

    if (m_paused) {
        resId = IDI_APP_DORMANT_SMALL;
        fallbackRes = IDI_APP_DORMANT_ICON;
    } else {
        switch (currentTheme) {
            case ColorTheme::Yellow: resId = IDI_APP_YELLOW_SMALL; fallbackRes = IDI_APP_YELLOW_ICON; break;
            case ColorTheme::Blue:   resId = IDI_APP_BLUE_SMALL;   fallbackRes = IDI_APP_BLUE_ICON;   break;
            case ColorTheme::Red:    resId = IDI_APP_RED_SMALL;    fallbackRes = IDI_APP_RED_ICON;    break;
            case ColorTheme::Silver: resId = IDI_APP_SILVER_SMALL; fallbackRes = IDI_APP_SILVER_ICON; break;
            case ColorTheme::Gold:   resId = IDI_APP_GOLD_SMALL;   fallbackRes = IDI_APP_GOLD_ICON;   break;
            case ColorTheme::Green:
            default:                 resId = IDI_APP_ICON_SMALL;   fallbackRes = IDI_APP_ICON;        break;
        }
    }
    HICON hIcon = (HICON)LoadImageW(m_hInst, MAKEINTRESOURCEW(resId), IMAGE_ICON, cx, cx, LR_DEFAULTCOLOR);
    if (!hIcon) {
        hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(resId));
    }
    if (!hIcon) {
        hIcon = LoadIconW(m_hInst, MAKEINTRESOURCEW(fallbackRes));
    }
    if (hIcon) {
        m_nid.hIcon = hIcon;
        m_nid.uFlags = NIF_ICON;
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }
}

void TrayIcon::Remove() {
    if (m_created) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        if (m_dynamicIcon) {
            DestroyIcon(m_dynamicIcon);
            m_dynamicIcon = nullptr;
        }
        m_created = false;
    }
}

void TrayIcon::UpdateTooltip(const std::wstring& text) {
    if (!m_created) return;
    m_nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(m_nid.szTip, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::ShowBalloon(const std::wstring& title,
                            const std::wstring& msg,
                            DWORD dwInfoFlags) {
    if (!m_created) return;
    m_nid.uFlags = NIF_INFO;
    m_nid.dwInfoFlags = dwInfoFlags;
    m_nid.uTimeout    = 3000;
    wcsncpy_s(m_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(m_nid.szInfo,      msg.c_str(),   _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::SetPaused(bool paused) {
    m_paused = paused;
    if (!m_created) return;
    std::wstring tip = std::wstring(ApplicationMetadata::AppName) + (paused ? L" — Paused" : L" — Active");
    UpdateTooltip(tip);
    UpdateIcon();
}

void TrayIcon::HandleMessage(HWND hwnd, LPARAM lParam) {
    UINT msg = LOWORD(lParam);
    switch (msg) {
        case WM_RBUTTONUP:
        case NIN_KEYSELECT:
            ShowContextMenu(hwnd);
            break;
        case WM_LBUTTONDBLCLK:
        case NIN_SELECT:
            PostMessageW(hwnd, WM_COMMAND, ID_TRAY_OPEN, 0);
            break;
    }
}

void TrayIcon::ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // ── Build context menu ─────────────────────────────────────────────────
    std::wstring openLabel = std::wstring(L"Open ") + ApplicationMetadata::AppName;
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN, openLabel.c_str());

    // Bold first item (default action)
    MENUITEMINFOW mii{};
    mii.cbSize    = sizeof(mii);
    mii.fMask     = MIIM_STATE;
    mii.fState    = MFS_DEFAULT;
    SetMenuItemInfoW(hMenu, ID_TRAY_OPEN, FALSE, &mii);

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Pause submenu
    HMENU hPause = CreatePopupMenu();
    AppendMenuW(hPause, MF_STRING, ID_TRAY_PAUSE_15,    L"15 Minutes");
    AppendMenuW(hPause, MF_STRING, ID_TRAY_PAUSE_30,    L"30 Minutes");
    AppendMenuW(hPause, MF_STRING, ID_TRAY_PAUSE_60,    L"1 Hour");
    AppendMenuW(hPause, MF_STRING, ID_TRAY_PAUSE_120,   L"2 Hours");
    AppendMenuW(hPause, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hPause, MF_STRING, ID_TRAY_PAUSE_TODAY, L"Until Tomorrow");

    bool paused = Scheduler::Instance().IsPaused();
    if (!paused) {
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPause, L"⏸  Pause Reminders");
    } else {
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESUME, L"▶  Resume Reminders");
    }

    // Next reminder time
    time_t next = Scheduler::Instance().GetNextReminderTime();
    std::wstring nextLabel = L"Next Reminder";
    if (next > 0) {
        struct tm lt;
        localtime_s(&lt, &next);
        wchar_t buf[32];
        wcsftime(buf, 32, L"%I:%M %p", &lt);
        nextLabel = std::wstring(L"Next: ") + buf;
    }
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, ID_TRAY_NEXT, nextLabel.c_str());

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS,   L"⚙  Settings");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_STATISTICS, L"📊  Statistics");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_ABOUT,      L"ℹ  About");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT,       L"Exit");

    // Show menu at cursor position
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenuEx(hMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
        pt.x, pt.y, hwnd, nullptr);

    DestroyMenu(hPause);
    DestroyMenu(hMenu);
}

} // namespace dhikr
