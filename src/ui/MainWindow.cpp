#include "MainWindow.h"
#include "DesignSystem.h"
#include "TrayIcon.h"
#include "PopupWindow.h"
#include "core/LogoRenderer.h"
#include "core/ResourceLoader.h"
#include "core/WindowChrome.h"
#include "pages/HomePage.h"
#include "pages/RemindersPage.h"
#include "pages/LibraryPage.h"
#include "pages/SettingsPage.h"
#include "pages/AboutPage.h"
#include "../core/Application.h"
#include "../core/Settings.h"
#include "../core/Scheduler.h"
#include "../core/ContentEngine.h"
#include "../core/Logger.h"
#include "../core/Localization.h"
#include "../core/ApplicationMetadata.h"
#include "../update/Updater.h"
#include "../core/FullScreenDetector.h"
#include "../resource.h"
#include <algorithm>
#include <windowsx.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <filesystem>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace dhikr {

#define TIMER_FULLSCREEN_RESUME 1002
#define TIMER_TRAY_RETRY        1003

static std::string WStringToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return {};
    std::string s(sz, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), sz, nullptr, nullptr);
    return s;
}

static std::wstring GetNavLabel(int i) {
    const wchar_t* keys[] = {
        L"nav.home", L"nav.reminders", L"nav.library",
        L"nav.settings", L"nav.about"
    };
    return Localization::Instance().Get(keys[i]);
}

MainWindow& MainWindow::Instance() {
    static MainWindow instance;
    return instance;
}

MainWindow::~MainWindow() {
    ReleaseD2D();
}

bool MainWindow::Create(HINSTANCE hInstance) {
    m_hInst = hInstance;

    // Use GetDpiForSystem for initial metrics. We will refine it to GetDpiForWindow after HWND creation.
    m_dpiScale = GetDpiForSystem() / 96.f;

    m_width  = (int)(820.f * m_dpiScale);
    m_height = (int)(560.f * m_dpiScale);
    RegisterClass(hInstance);
    if (!CreateWindow_(hInstance)) return false;

    if (!InitD2D()) return false;

    InitTextFormats();

    InitPages(hInstance);

    // Create popup window
    PopupWindow::Instance().Create(hInstance);

    // Request initial data update
    PostMessageW(m_hwnd, WM_APP + 100, 0, 0);

    UpdateAppIcon();

    LOG_INFO("Main window created");
    return true;
}

void MainWindow::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);

    int bigResId = IDI_APP_ICON;
    int smallResId = IDI_APP_ICON_SMALL;
    if (ds::Accent::IsAppPaused()) {
        bigResId = IDI_APP_DORMANT_ICON;
        smallResId = IDI_APP_DORMANT_SMALL;
    } else {
        switch (Settings::Instance().GetCopy().colorTheme) {
            case ColorTheme::Yellow: bigResId = IDI_APP_YELLOW_ICON; smallResId = IDI_APP_YELLOW_SMALL; break;
            case ColorTheme::Blue:   bigResId = IDI_APP_BLUE_ICON;   smallResId = IDI_APP_BLUE_SMALL;   break;
            case ColorTheme::Red:    bigResId = IDI_APP_RED_ICON;    smallResId = IDI_APP_RED_SMALL;    break;
            case ColorTheme::Silver: bigResId = IDI_APP_SILVER_ICON; smallResId = IDI_APP_SILVER_SMALL; break;
            case ColorTheme::Gold:   bigResId = IDI_APP_GOLD_ICON;   smallResId = IDI_APP_GOLD_SMALL;   break;
            case ColorTheme::Green:
            default:                 bigResId = IDI_APP_ICON;        smallResId = IDI_APP_ICON_SMALL;   break;
        }
    }

    wc.hIcon   = LoadIconW(hInstance, MAKEINTRESOURCEW(bigResId));
    if (!wc.hIcon)   wc.hIcon   = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(smallResId));
    if (!wc.hIconSm) wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

bool MainWindow::CreateWindow_(HINSTANCE hInstance) {
    // Standard framed window style with native Windows caption, system menu, and minimize box
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    RECT rc = { 0, 0, m_width, m_height };
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_APPWINDOW);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - winW)  / 2;
    int y = (screenH - winH) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClassName,
        ApplicationMetadata::AppName,
        style,
        x, y, winW, winH,
        nullptr, nullptr, hInstance, this);

    if (m_hwnd) {
        UINT dpi = GetDpiForWindow(m_hwnd);
        if (dpi != 0) {
            m_dpiScale = dpi / 96.f;
            m_width  = (int)(820.f * m_dpiScale);
            m_height = (int)(560.f * m_dpiScale);

            RECT rcDpi = { 0, 0, m_width, m_height };
            AdjustWindowRectEx(&rcDpi, style, FALSE, WS_EX_APPWINDOW);
            winW = rcDpi.right - rcDpi.left;
            winH = rcDpi.bottom - rcDpi.top;

            int sw = GetSystemMetricsForDpi(SM_CXSCREEN, dpi);
            int sh = GetSystemMetricsForDpi(SM_CYSCREEN, dpi);
            x = (sw - winW)  / 2;
            y = (sh - winH) / 2;
            SetWindowPos(m_hwnd, nullptr, x, y, winW, winH, SWP_NOZORDER | SWP_NOACTIVATE);
        }

        // 1. Set window title and enable DWM Immersive Dark Mode for native Windows caption
        SetWindowTextW(m_hwnd, ApplicationMetadata::AppName);
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(m_hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
        DwmSetWindowAttribute(m_hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &darkMode, sizeof(darkMode));

        // 2. Request native rounded corners on Windows 11 (DWMWCP_ROUND = 2)
        DWORD cornerPref = 2; // DWMWCP_ROUND
        DwmSetWindowAttribute(m_hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPref, sizeof(cornerPref));

        // 3. Native title bar caption color matching dark theme (#101310)
        COLORREF captionColor = RGB(16, 19, 16);
        DwmSetWindowAttribute(m_hwnd, 35 /* DWMWA_CAPTION_COLOR */, &captionColor, sizeof(captionColor));

        // 4. Native title text color: crisp light off-white
        COLORREF textColor = RGB(241, 244, 239);
        DwmSetWindowAttribute(m_hwnd, 36 /* DWMWA_TEXT_COLOR */, &textColor, sizeof(textColor));

        // 5. Subtle DWM border matching obsidian dark theme
        COLORREF borderColor = RGB(36, 41, 36);
        DwmSetWindowAttribute(m_hwnd, 34 /* DWMWA_BORDER_COLOR */, &borderColor, sizeof(borderColor));

        m_navRail = std::make_shared<NavigationRail>();
        m_navRail->SetDpiScale(m_dpiScale);
        m_navRail->SetBounds(SidebarRect());
        m_navRail->OnNavigate = [this](PageId p) {
            LOG_INFO("[NAV] Navigation to page " + std::to_string(static_cast<int>(p)));
            UIStateManager::Instance().SetPage(p);
        };
        InputController::Instance().AddControl(m_navRail);
        
        UpdateAppIcon();
    }

    LOG_INFO("[WINDOW] Main HWND = " + std::to_string((uintptr_t)m_hwnd));
    LOG_INFO("[WINDOW] Thread ID = " + std::to_string(GetCurrentThreadId()));

    return m_hwnd != nullptr;
}

bool MainWindow::InitD2D() {
    m_d2dFactory    = Application::Instance().D2DFactory();
    m_dwriteFactory = Application::Instance().DWriteFactory();

    D2D1_SIZE_U size = D2D1::SizeU(m_width, m_height);
    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        rtp,
        D2D1::HwndRenderTargetProperties(m_hwnd, size,
            D2D1_PRESENT_OPTIONS_NONE),
        &m_renderTarget);

    if (FAILED(hr)) {
        LOG_ERROR("Failed to create D2D render target");
        return false;
    }

    hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f), &m_brush);
    if (FAILED(hr) || !m_brush) {
        // Brush is required for all painting; treat this as a fatal init failure
        // so the caller can decide whether to retry or fall back to GDI.
        LOG_ERROR("Failed to create D2D solid brush");
        m_brush = nullptr;
        // We still return true so the window is shown; OnPaint has a GDI fallback.
    }

    // Initialize WIC and load logos from embedded resources with high-quality downsampling
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory));
    if (SUCCEEDED(hr) && m_wicFactory && m_renderTarget) {
        UINT smallSize = (UINT)(48 * m_dpiScale);
        if (smallSize < 48) smallSize = 48;
        UINT largeSize = (UINT)(256 * m_dpiScale);
        if (largeSize < 256) largeSize = 256;

        struct LogoRes { int smallId; int largeId; };
        LogoRes themes[6] = {
            { IDR_LOGO_SMALL,        IDR_LOGO },        // Green
            { IDR_LOGO_YELLOW_SMALL, IDR_LOGO_YELLOW }, // Yellow
            { IDR_LOGO_BLUE_SMALL,   IDR_LOGO_BLUE },   // Blue
            { IDR_LOGO_RED_SMALL,    IDR_LOGO_RED },    // Red
            { IDR_LOGO_SILVER_SMALL, IDR_LOGO_SILVER }, // Silver
            { IDR_LOGO_GOLD_SMALL,   IDR_LOGO_GOLD },   // Gold
        };

        for (int i = 0; i < 6; ++i) {
            m_logoSmallBitmaps[i] = ResourceLoader::LoadBitmapFromResource(
                m_hInst, themes[i].smallId, m_wicFactory, m_renderTarget, RT_RCDATA, smallSize, smallSize);
            m_logoBitmaps[i] = ResourceLoader::LoadBitmapFromResource(
                m_hInst, themes[i].largeId, m_wicFactory, m_renderTarget, RT_RCDATA, largeSize, largeSize);
        }

        // Dormant silver/chrome/grey logos
        m_logoDormantSmallBitmap = ResourceLoader::LoadBitmapFromResource(
            m_hInst, IDR_LOGO_DORMANT_SMALL, m_wicFactory, m_renderTarget, RT_RCDATA, smallSize, smallSize);
        m_logoDormantBitmap = ResourceLoader::LoadBitmapFromResource(
            m_hInst, IDR_LOGO_DORMANT, m_wicFactory, m_renderTarget, RT_RCDATA, largeSize, largeSize);

        LOG_INFO("MainWindow: Embedded theme logos loaded successfully with WIC resampler");
    }

    return true;
}

bool MainWindow::InitTextFormats() {
    auto tryCreateFormat = [&](IDWriteTextFormat** ppFmt,
                                DWRITE_FONT_WEIGHT weight, float size) {
        HRESULT hr = m_dwriteFactory->CreateTextFormat(
            L"Cairo", nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            size * m_dpiScale, L"en-US", ppFmt);
        if (FAILED(hr) || !*ppFmt) {
            m_dwriteFactory->CreateTextFormat(
                L"Segoe UI", nullptr, weight,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                size * m_dpiScale, L"en-US", ppFmt);
        }
    };

    tryCreateFormat(&m_tfNavLabel, DWRITE_FONT_WEIGHT_REGULAR,  ds::FontSize::NavLabel);
    tryCreateFormat(&m_tfAppName,  DWRITE_FONT_WEIGHT_SEMI_BOLD, ds::FontSize::AppTitle);
    tryCreateFormat(&m_tfVersion,  DWRITE_FONT_WEIGHT_REGULAR,   ds::FontSize::Version);

    // Icon font — dynamically resolve Segoe Fluent Icons (Win 11) or Segoe MDL2 Assets (Win 10/11)
    const wchar_t* iconFontFamily = ds::Icon::GetFontFamily(m_dwriteFactory);
    m_dwriteFactory->CreateTextFormat(
        iconFontFamily, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        ds::Dims::NavIconSize * m_dpiScale, L"en-US", &m_tfIcon);

    if (m_tfIcon) {
        m_tfIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfVersion) m_tfVersion->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    
    // We update NavLabel reading direction dynamically in DrawSidebarItem or Draw.
    if (m_tfNavLabel) m_tfNavLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    return true;
}

void MainWindow::InitPages(HINSTANCE hInstance) {
    LOG_INFO("[MAIN] InitPages called!");
    m_pages[PAGE_HOME]      = std::make_unique<HomePage>();
    m_pages[PAGE_REMINDERS] = std::make_unique<RemindersPage>();
    m_pages[PAGE_LIBRARY]   = std::make_unique<LibraryPage>();
    m_pages[PAGE_SETTINGS]  = std::make_unique<SettingsPage>();
    m_pages[PAGE_ABOUT]     = std::make_unique<AboutPage>();
}

void MainWindow::ReleaseD2D() {
    if (m_logoDormantSmallBitmap) { m_logoDormantSmallBitmap->Release(); m_logoDormantSmallBitmap = nullptr; }
    if (m_logoDormantBitmap)      { m_logoDormantBitmap->Release();      m_logoDormantBitmap      = nullptr; }
    for (int i = 0; i < 6; ++i) {
        if (m_logoSmallBitmaps[i]) { m_logoSmallBitmaps[i]->Release(); m_logoSmallBitmaps[i] = nullptr; }
        if (m_logoBitmaps[i])      { m_logoBitmaps[i]->Release();      m_logoBitmaps[i]      = nullptr; }
    }
    if (m_wicFactory)      { m_wicFactory->Release();      m_wicFactory      = nullptr; }
    if (m_tfVersion)     { m_tfVersion->Release();     m_tfVersion     = nullptr; }
    if (m_tfAppName)     { m_tfAppName->Release();     m_tfAppName     = nullptr; }
    if (m_tfNavLabel)    { m_tfNavLabel->Release();    m_tfNavLabel    = nullptr; }
    if (m_tfIcon)        { m_tfIcon->Release();        m_tfIcon        = nullptr; }
    if (m_brush)         { m_brush->Release();         m_brush         = nullptr; }
    if (m_renderTarget)  { m_renderTarget->Release();  m_renderTarget  = nullptr; }
}

void MainWindow::Show() {
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    UpdateWindow(m_hwnd);
}

void MainWindow::Hide() {
    ShowWindow(m_hwnd, SW_HIDE);
    // Trim working set pages when hidden to tray to minimize RAM usage to ~2.8-3.5 MB
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

void MainWindow::NavigateTo(int pageId) {
    if (pageId < 0 || pageId >= kNavItems) return;
    if (m_activePage == pageId) return; // Guard: do not re-activate or reset scroll of current active page
    m_activePage = pageId;
    if (m_pages[m_activePage]) {
        m_pages[m_activePage]->OnActivated();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

D2D1_RECT_F MainWindow::SidebarRect() const {
    float sw = ds::Dims::SidebarW * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    if (isRtl) return D2D1::RectF((float)m_width - sw, 0.f, (float)m_width, (float)m_height);
    return D2D1::RectF(0.f, 0.f, sw, (float)m_height);
}

D2D1_RECT_F MainWindow::ContentRect() const {
    float sw = ds::Dims::SidebarW * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    if (isRtl) return D2D1::RectF(0.f, 0.f, (float)m_width - sw, (float)m_height);
    return D2D1::RectF(sw, 0.f, (float)m_width, (float)m_height);
}

// ── State ───────────────────────────────────────────────────────────────────
void MainWindow::OnPaint() {
    // Always call BeginPaint/EndPaint to validate (clear) the dirty region
    // reported by Windows. Failure to do so causes WM_PAINT to be sent
    // continuously, resulting in a perpetually blank window.
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(m_hwnd, &ps);

    if (!m_renderTarget || !m_brush) {
        // ── GDI fallback ────────────────────────────────────────────────────
        // D2D is not ready (e.g. brush creation failed, or device lost).
        // Paint a solid dark background so the window is never blank white.
        if (hdc) {
            HBRUSH fb = CreateSolidBrush(RGB(0x10, 0x13, 0x10)); // ds::Dark::Surface
            if (fb) { FillRect(hdc, &ps.rcPaint, fb); DeleteObject(fb); }
            else    { FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(DKGRAY_BRUSH)); }
        }
        EndPaint(m_hwnd, &ps);
        return;
    }

    // ── D2D path ────────────────────────────────────────────────────
    // BeginPaint/EndPaint validate the dirty region; D2D draws independently
    // into the HwndRenderTarget which is bound to the same HWND.
    EndPaint(m_hwnd, &ps); // release the GDI DC — D2D doesn't use it

    m_renderTarget->BeginDraw();

    // Force-clear to solid background
    auto bg = ds::Background();
    m_renderTarget->Clear(bg);

    DrawBackground(m_renderTarget);
    DrawSidebar(m_renderTarget);
    DrawCurrentPage(m_renderTarget);

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseD2D();
        InitD2D();
        InitTextFormats();
        InvalidateRect(m_hwnd, nullptr, FALSE); // request a fresh repaint
    }
}

void MainWindow::DrawBackground(ID2D1RenderTarget* rt) {
    if (!m_brush || !rt) return;

    // Subtle atmospheric Aurora canvas gradient
    D2D1_GRADIENT_STOP stops[4];
    stops[0].position = 0.00f; stops[0].color = ds::Aurora::CanvasStop0(); // Top-Right Deep Moss Glow
    stops[1].position = 0.35f; stops[1].color = ds::Aurora::CanvasStop1(); // Midnight Olive Pine
    stops[2].position = 0.70f; stops[2].color = ds::Aurora::CanvasStop2(); // Abyssal Forest
    stops[3].position = 1.00f; stops[3].color = ds::Aurora::CanvasStop3(); // Velvet Midnight Base

    ID2D1GradientStopCollection* pStopCol = nullptr;
    HRESULT hr = rt->CreateGradientStopCollection(stops, 4, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pStopCol);
    if (SUCCEEDED(hr) && pStopCol) {
        bool isRtl = Localization::Instance().IsRTL();
        // Top-right to bottom-left gradient flow (or top-left to bottom-right in RTL)
        D2D1_POINT_2F startPt = isRtl ? D2D1::Point2F(0.0f, 0.0f) : D2D1::Point2F((float)m_width, 0.0f);
        D2D1_POINT_2F endPt   = isRtl ? D2D1::Point2F((float)m_width, (float)m_height) : D2D1::Point2F(0.0f, (float)m_height);

        ID2D1LinearGradientBrush* pGradBrush = nullptr;
        hr = rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(startPt, endPt),
            pStopCol,
            &pGradBrush);
        if (SUCCEEDED(hr) && pGradBrush) {
            rt->FillRectangle(D2D1::RectF(0.0f, 0.0f, (float)m_width, (float)m_height), pGradBrush);
            pGradBrush->Release();
        } else {
            m_brush->SetColor(ds::Background());
            rt->FillRectangle(D2D1::RectF(0.0f, 0.0f, (float)m_width, (float)m_height), m_brush);
        }
        pStopCol->Release();
    } else {
        m_brush->SetColor(ds::Background());
        rt->FillRectangle(D2D1::RectF(0.0f, 0.0f, (float)m_width, (float)m_height), m_brush);
    }

    if (Settings::Instance().GetCopy().arabesqueOverlay) {
        ds::Arabesque::DrawLattice(rt, D2D1::RectF(0.0f, 0.0f, (float)m_width, (float)m_height), 0.025f);
    }
}

void MainWindow::DrawSidebar(ID2D1RenderTarget* rt) {
    if (!m_brush) return;
    auto sr = SidebarRect();

    m_brush->SetColor(ds::Sidebar());
    rt->FillRectangle(sr, m_brush);

    // Divider border
    m_brush->SetColor(ds::Border());
    if (Localization::Instance().IsRTL()) {
        rt->DrawLine(
            D2D1::Point2F(sr.left, sr.top),
            D2D1::Point2F(sr.left, (float)m_height),
            m_brush, 1.f * m_dpiScale);
    } else {
        rt->DrawLine(
            D2D1::Point2F(sr.right, sr.top),
            D2D1::Point2F(sr.right, (float)m_height),
            m_brush, 1.f * m_dpiScale);
    }

    // Nav items
    for (int i = 0; i < kNavItems; ++i) {
        std::wstring label = GetNavLabel(i);
        DrawSidebarItem(rt, i, label.c_str(),
            m_navRail->GetItemRect(i).top, i == m_activePage, i == m_navRail->GetHoveredItem());
    }

    DrawSidebarBottom(rt);
}

void MainWindow::DrawSidebarItem(ID2D1RenderTarget* rt, int index,
                                  const wchar_t* label, float y,
                                  bool active, bool hover) {
    if (!m_brush) return;
    auto r = m_navRail->GetItemRect(index);
    float sw = ds::Dims::SidebarW * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    // Active / hover pill
    float rx = 6.f * m_dpiScale;
    float pillPad = 6.f * m_dpiScale;
    D2D1_RECT_F pillRect = D2D1::RectF(r.left + pillPad, r.top + 2.f*m_dpiScale, r.right - pillPad, r.bottom - 2.f*m_dpiScale);

    if (active) {
        m_brush->SetColor(ds::ActiveNavBg());
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillRect, rx, rx), m_brush);

        // Accent indicator bar
        m_brush->SetColor(ds::Accent::Primary());
        float indW = 3.f * m_dpiScale;
        float indTop    = pillRect.top    + 6.f * m_dpiScale;
        float indBottom = pillRect.bottom - 6.f * m_dpiScale;
        D2D1_RECT_F bar = isRtl
            ? D2D1::RectF(pillRect.right - indW, indTop, pillRect.right, indBottom)
            : D2D1::RectF(pillRect.left,         indTop, pillRect.left + indW, indBottom);
        rt->FillRoundedRectangle(D2D1::RoundedRect(bar, 1.5f, 1.5f), m_brush);
    } else if (hover) {
        m_brush->SetColor(ds::CardHover());
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillRect, rx, rx), m_brush);
    }

    // Icon
    float iconSize = ds::Dims::NavIconSize * m_dpiScale;
    float cx = isRtl ? (r.right - 22.f * m_dpiScale - iconSize / 2.f) : (r.left + 22.f * m_dpiScale + iconSize / 2.f);
    float cy = r.top + (r.bottom - r.top) / 2.f;
    auto iconColor = active ? ds::Accent::Primary() : (hover ? ds::TextPrimary() : ds::TextSecondary());
    DrawNavIcon(rt, index, cx, cy, iconSize, iconColor);

    // Label
    if (m_tfNavLabel) {
        m_tfNavLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfNavLabel->SetTextAlignment(isRtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
        m_brush->SetColor(active ? ds::TextPrimary() : (hover ? ds::TextPrimary() : ds::TextSecondary()));
        
        float textX = isRtl ? (r.left + 12.f * m_dpiScale) : (cx + iconSize / 2.f + 12.f * m_dpiScale);
        float textXEnd = isRtl ? (cx - iconSize / 2.f - 12.f * m_dpiScale) : (r.right - 12.f * m_dpiScale);
        
        D2D1_RECT_F textRect = D2D1::RectF(textX, r.top, textXEnd, r.bottom);
        rt->DrawTextW(label, (UINT32)wcslen(label), m_tfNavLabel, textRect, m_brush);
    }
}

void MainWindow::DrawNavIcon(ID2D1RenderTarget* rt, int pageId,
                               float cx, float cy, float size,
                               D2D1_COLOR_F color) {
    if (!m_tfIcon || !m_brush) return;
    m_brush->SetColor(color);
    
    const wchar_t* glyph = ds::Icon::Home;
    switch (pageId) {
        case PAGE_HOME:       glyph = ds::Icon::Home; break;
        case PAGE_REMINDERS:  glyph = ds::Icon::Reminders; break;
        case PAGE_LIBRARY:    glyph = ds::Icon::Library; break;
        case PAGE_SETTINGS:   glyph = ds::Icon::Settings; break;
        case PAGE_ABOUT:      glyph = ds::Icon::About; break;
    }

    // Measure to center exactly
    D2D1_RECT_F rect = D2D1::RectF(cx - size, cy - size, cx + size, cy + size);
    rt->DrawTextW(glyph, 1, m_tfIcon, rect, m_brush);
}

ID2D1Bitmap* MainWindow::GetThemeLogo(ColorTheme theme, bool isSmall) const {
    int idx = std::clamp((int)theme, 0, 5);
    if (isSmall) {
        return m_logoSmallBitmaps[idx] ? m_logoSmallBitmaps[idx] : m_logoSmallBitmaps[0];
    }
    return m_logoBitmaps[idx] ? m_logoBitmaps[idx] : m_logoBitmaps[0];
}

ID2D1Bitmap* MainWindow::GetEffectiveLogo(bool isSmall) const {
    if (ds::Accent::IsAppPaused()) {
        if (isSmall) {
            return m_logoDormantSmallBitmap ? m_logoDormantSmallBitmap : m_logoDormantBitmap;
        }
        return m_logoDormantBitmap ? m_logoDormantBitmap : m_logoDormantSmallBitmap;
    }
    return GetThemeLogo(Settings::Instance().GetCopy().colorTheme, isSmall);
}

void MainWindow::DrawEffectiveLogo(ID2D1RenderTarget* rt, const D2D1_RECT_F& dest, bool isSmall, float alpha) const {
    if (!rt) return;

    if (ds::Accent::IsAppPaused()) {
        ID2D1Bitmap* dormant = isSmall ? (m_logoDormantSmallBitmap ? m_logoDormantSmallBitmap : m_logoDormantBitmap)
                                       : (m_logoDormantBitmap ? m_logoDormantBitmap : m_logoDormantSmallBitmap);
        if (dormant) {
            D2D1_SIZE_F sz = dormant->GetSize();
            D2D1_RECT_F fit = LogoRenderer::FitRect(dest, sz.width, sz.height);
            rt->DrawBitmap(dormant, fit, alpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        } else {
            LogoRenderer::Draw(rt, dest, alpha);
        }
        return;
    }

    if (ds::Accent::IsTransitioning()) {
        ID2D1Bitmap* bmpFrom = GetThemeLogo(ds::Accent::FromTheme(), isSmall);
        ID2D1Bitmap* bmpTo   = GetThemeLogo(ds::Accent::ToTheme(), isSmall);
        float t = ds::Accent::GetTransitionProgress();

        if (bmpFrom && bmpTo) {
            D2D1_SIZE_F sz = bmpFrom->GetSize();
            D2D1_RECT_F fit = LogoRenderer::FitRect(dest, sz.width, sz.height);
            // Seamless cross-fade
            if (1.0f - t > 0.001f) {
                rt->DrawBitmap(bmpFrom, fit, alpha * (1.0f - t), D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            }
            if (t > 0.001f) {
                rt->DrawBitmap(bmpTo, fit, alpha * t, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            }
            return;
        }
    }

    ID2D1Bitmap* activeLogo = GetEffectiveLogo(isSmall);
    if (activeLogo) {
        D2D1_SIZE_F sz = activeLogo->GetSize();
        D2D1_RECT_F fit = LogoRenderer::FitRect(dest, sz.width, sz.height);
        rt->DrawBitmap(activeLogo, fit, alpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        LogoRenderer::Draw(rt, dest, alpha);
    }
}

void MainWindow::DrawSidebarBottom(ID2D1RenderTarget* rt) {
    if (!m_brush) return;
    auto sr = SidebarRect();
    float pad = ds::Space::L * m_dpiScale;
    float bottomY = (float)m_height - 40.f * m_dpiScale;

    // Smooth animated 3D coin medallion
    float cx = sr.left + (sr.right - sr.left) / 2.f;
    float cy = bottomY - 14.f * m_dpiScale;
    float size = 18.f * m_dpiScale;
    D2D1_RECT_F rect = D2D1::RectF(cx - size, cy - size, cx + size, cy + size);
    DrawEffectiveLogo(rt, rect, true, 1.0f);

    // Version string
    if (m_tfVersion) {
        m_brush->SetColor(ds::TextMuted());
        std::wstring vstr = Localization::Instance().Get(L"about.version") + L" " + ApplicationMetadata::AppVersion;
        D2D1_RECT_F vr = D2D1::RectF(sr.left, bottomY + 8.f*m_dpiScale, sr.right, bottomY + 30.f*m_dpiScale);
        m_tfVersion->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_tfVersion->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawTextW(vstr.c_str(), (UINT32)vstr.size(), m_tfVersion, vr, m_brush);
    }
}

void MainWindow::DrawCurrentPage(ID2D1RenderTarget* rt) {
    auto cr = ContentRect();
    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        m_pages[m_activePage]->Draw(rt, cr);
    }
}

// ── Message handlers ──────────────────────────────────────────────────────────
LRESULT MainWindow::OnSize(int w, int h) {
    m_width  = w;
    m_height = h;
    if (m_renderTarget) {
        m_renderTarget->Resize(D2D1::SizeU(w, h));
    }
    
    if (m_navRail) {
        m_navRail->SetDpiScale(m_dpiScale);
        m_navRail->SetBounds(SidebarRect());
    }
    
    auto cr = ContentRect();
    for (auto& p : m_pages) {
        if (p) p->OnSize(cr);
    }
    return 0;
}

LRESULT MainWindow::OnCommand(WPARAM wp) {
    switch (LOWORD(wp)) {
        case ID_TRAY_OPEN:
            Show();
            break;
        case ID_TRAY_PAUSE_15:   Scheduler::Instance().Pause(15);  TrayIcon::Instance().SetPaused(true);  UpdateAppIcon(); Invalidate(); break;
        case ID_TRAY_PAUSE_30:   Scheduler::Instance().Pause(30);  TrayIcon::Instance().SetPaused(true);  UpdateAppIcon(); Invalidate(); break;
        case ID_TRAY_PAUSE_60:   Scheduler::Instance().Pause(60);  TrayIcon::Instance().SetPaused(true);  UpdateAppIcon(); Invalidate(); break;
        case ID_TRAY_PAUSE_120:  Scheduler::Instance().Pause(120); TrayIcon::Instance().SetPaused(true); UpdateAppIcon(); Invalidate(); break;
        case ID_TRAY_PAUSE_TODAY: Scheduler::Instance().PauseUntilTomorrow(); TrayIcon::Instance().SetPaused(true); UpdateAppIcon(); Invalidate(); break;
        case ID_TRAY_RESUME:     Scheduler::Instance().Resume();    TrayIcon::Instance().SetPaused(false); UpdateAppIcon(); Invalidate(); break;
        case ID_TRAY_SETTINGS:   Show(); NavigateTo(PAGE_SETTINGS); break;
        case ID_TRAY_STATISTICS: Show(); NavigateTo(PAGE_HOME); break;
        case ID_TRAY_ABOUT:      Show(); NavigateTo(PAGE_ABOUT); break;
        case ID_TRAY_EXIT:
            Application::Instance().Shutdown();
            PostQuitMessage(0);
            break;
        case ID_TRAY_NEXT: {
            // Show next reminder immediately
            auto* item = ContentEngine::Instance().GetNext();
            if (item) PopupWindow::Instance().Show(item);
            break;
        }
    }
    return 0;
}

LRESULT MainWindow::OnReminder(bool forcePreview) {
    if (!forcePreview && (Scheduler::Instance().IsPaused() || !Settings::Instance().GetCopy().remindersEnabled)) return 0;
    if (forcePreview) {
        auto* item = ContentEngine::Instance().GetNext();
        if (item) {
            PopupWindow::Instance().Show(item, true /* force */);
        }
        return 0;
    }

    if (PopupWindow::Instance().IsVisible()) {
        return 0;
    }
    auto* item = ContentEngine::Instance().GetNext();
    if (item) {
        // Fullscreen distraction protection check
        if (Settings::Instance().GetCopy().suppressDuringFullscreen && FullScreenDetector::Instance().IsFullScreenActive()) {
            HWND fg = GetForegroundWindow();
            wchar_t fgTitle[128] = { 0 };
            wchar_t fgClass[64]  = { 0 };
            if (fg) {
                GetWindowTextW(fg, fgTitle, 128);
                GetClassNameW(fg, fgClass, 64);
            }
            std::string fgInfo = "FG: '" + WStringToUtf8(fgTitle) + "' [" + WStringToUtf8(fgClass) + "]";

            int mode = Settings::Instance().GetCopy().fullscreenSuppressionMode;
            if (mode == 2) {
                // Windows notification only
                LOG_INFO("[FULLSCREEN] Fullscreen active (" + fgInfo + ") — delivering via quiet notification");
                TrayIcon::Instance().ShowBalloon(ApplicationMetadata::AppName, item->text, NIIF_INFO);
                return 0;
            } else if (mode == 1) {
                // Suppress completely
                LOG_INFO("[FULLSCREEN] Fullscreen active (" + fgInfo + ") — reminder suppressed");
                return 0;
            } else {
                // Queue until fullscreen ends (Default)
                LOG_INFO("[FULLSCREEN] Fullscreen active (" + fgInfo + ") — queuing reminder until fullscreen ends");
                m_pendingFullscreenReminder = std::make_unique<ContentItem>(*item);
                SetTimer(m_hwnd, TIMER_FULLSCREEN_RESUME, 2000, nullptr);
                return 0;
            }
        }

        PopupWindow::Instance().Show(item);
        // Update tray tooltip
        time_t next = Scheduler::Instance().GetNextReminderTime();
        if (next > 0) {
            struct tm lt;
            localtime_s(&lt, &next);
            wchar_t buf[64];
            wcsftime(buf, 64, L"Next reminder at %I:%M %p", &lt);
            TrayIcon::Instance().UpdateTooltip(buf);
        }
    } else {
        // No content loaded
        TrayIcon::Instance().ShowBalloon(ApplicationMetadata::AppName,
            L"Please import a content file to receive reminders.", NIIF_INFO);
    }
    return 0;
}

void MainWindow::UpdateAppIcon(ColorTheme themeOverride) {
    if (!m_hwnd) return;
    int cxBig = GetSystemMetrics(SM_CXICON);
    int cyBig = GetSystemMetrics(SM_CYICON);
    int cxSmall = GetSystemMetrics(SM_CXSMICON);
    int cySmall = GetSystemMetrics(SM_CYSMICON);
    int bigResId = IDI_APP_ICON;
    int smallResId = IDI_APP_ICON_SMALL;

    ColorTheme activeTheme = (themeOverride != static_cast<ColorTheme>(-1)) ? themeOverride
        : (ds::Accent::IsTransitioning() ? m_targetTheme : Settings::Instance().GetCopy().colorTheme);

    if (ds::Accent::IsAppPaused()) {
        bigResId = IDI_APP_DORMANT_ICON;
        smallResId = IDI_APP_DORMANT_SMALL;
    } else {
        switch (activeTheme) {
            case ColorTheme::Yellow: bigResId = IDI_APP_YELLOW_ICON; smallResId = IDI_APP_YELLOW_SMALL; break;
            case ColorTheme::Blue:   bigResId = IDI_APP_BLUE_ICON;   smallResId = IDI_APP_BLUE_SMALL;   break;
            case ColorTheme::Red:    bigResId = IDI_APP_RED_ICON;    smallResId = IDI_APP_RED_SMALL;    break;
            case ColorTheme::Silver: bigResId = IDI_APP_SILVER_ICON; smallResId = IDI_APP_SILVER_SMALL; break;
            case ColorTheme::Gold:   bigResId = IDI_APP_GOLD_ICON;   smallResId = IDI_APP_GOLD_SMALL;   break;
            case ColorTheme::Green:
            default:                 bigResId = IDI_APP_ICON;        smallResId = IDI_APP_ICON_SMALL;   break;
        }
    }

    HICON hBig = (HICON)LoadImageW(m_hInst, MAKEINTRESOURCEW(bigResId), IMAGE_ICON, cxBig, cyBig, LR_SHARED);
    if (!hBig) hBig = LoadIconW(m_hInst, MAKEINTRESOURCEW(bigResId));

    HICON hSmall = (HICON)LoadImageW(m_hInst, MAKEINTRESOURCEW(smallResId), IMAGE_ICON, cxSmall, cySmall, LR_SHARED);
    if (!hSmall) hSmall = LoadIconW(m_hInst, MAKEINTRESOURCEW(smallResId));

    m_hCurrentIconBig = hBig;
    m_hCurrentIconSmall = hSmall;

    if (hBig) {
        SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
        SetClassLongPtrW(m_hwnd, GCLP_HICON, (LONG_PTR)hBig);
    }
    if (hSmall) {
        SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
        SetClassLongPtrW(m_hwnd, GCLP_HICONSM, (LONG_PTR)hSmall);
    }

    // Force DWM frame and title bar to immediately re-render caption icon
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    RedrawWindow(m_hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
}

inline float EaseInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

void MainWindow::TransitionToTheme(ColorTheme newTheme) {
    ColorTheme oldTheme = Settings::Instance().GetCopy().colorTheme;
    if (oldTheme == newTheme && !ds::Accent::IsTransitioning()) return;

    m_prevTheme = ds::Accent::IsTransitioning() ? ds::Accent::ToTheme() : oldTheme;
    m_targetTheme = newTheme;
    m_themeAnimStartTime = GetTickCount();
    m_themeAnimDuration = 350.0f;

    ds::Accent::StartTransition(m_prevTheme, m_targetTheme);

    // Immediately update window caption icon, taskbar icon, and tray icon!
    UpdateAppIcon(m_targetTheme);
    TrayIcon::Instance().UpdateIcon(m_targetTheme);

    SetTimer(m_hwnd, TIMER_THEME_TRANSITION, 16, nullptr);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

LRESULT MainWindow::OnSettingsChanged() {
    if (m_navRail) {
        m_navRail->SetDpiScale(m_dpiScale);
        m_navRail->SetBounds(SidebarRect());
    }

    // Update icons for Tray and Taskbar Window
    TrayIcon::Instance().SetPaused(ds::Accent::IsAppPaused());
    TrayIcon::Instance().UpdateIcon();
    UpdateAppIcon();

    static std::wstring s_currentLanguage = Localization::Instance().GetLanguage();
    std::wstring activeLang = Localization::Instance().GetLanguage();
    if (activeLang != s_currentLanguage) {
        s_currentLanguage = activeLang;
        float savedScroll = 0.f;
        if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
            savedScroll = m_pages[m_activePage]->GetScrollY();
        }
        InitPages(m_hInst);
        if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
            m_pages[m_activePage]->SetScrollY(savedScroll);
        }
    }

    auto cr = ContentRect();
    for (auto& p : m_pages) {
        if (p) {
            p->OnSize(cr);
            p->OnSettingsChanged();
        }
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == TIMER_THEME_TRANSITION) {
        DWORD elapsed = GetTickCount() - m_themeAnimStartTime;
        float t = std::min(1.0f, (float)elapsed / m_themeAnimDuration);
        float eased = EaseInOutCubic(t);
        ds::Accent::SetTransitionProgress(eased);
        InvalidateRect(m_hwnd, nullptr, FALSE);

        if (t >= 1.0f) {
            KillTimer(m_hwnd, TIMER_THEME_TRANSITION);
            ds::Accent::EndTransition(m_targetTheme);

            Settings::Instance().Modify([this](AppSettings& s) {
            s.colorTheme = m_targetTheme;
            switch (m_targetTheme) {
                case ColorTheme::Yellow: s.accentColor = { 0xFA, 0xCC, 0x15 }; break;
                case ColorTheme::Blue:   s.accentColor = { 0x38, 0xBD, 0xF8 }; break;
                case ColorTheme::Red:    s.accentColor = { 0xF8, 0x71, 0x71 }; break;
                case ColorTheme::Silver: s.accentColor = { 0xE2, 0xE8, 0xF0 }; break;
                case ColorTheme::Gold:   s.accentColor = { 0xF5, 0xBA, 0x22 }; break;
                case ColorTheme::Green:
                default:                 s.accentColor = { 0xB7, 0xF3, 0x4A }; break;
            }
            });
            Settings::Instance().Save();

            TrayIcon::Instance().UpdateIcon();
            UpdateAppIcon();

            for (auto& p : m_pages) {
                if (p) p->OnSettingsChanged();
            }
        }
        return 0;
    }
    if (timerId == TIMER_FULLSCREEN_RESUME) {
        if (Scheduler::Instance().GetState() != ScheduleState::Active) {
            m_pendingFullscreenReminder.reset();
            KillTimer(m_hwnd, TIMER_FULLSCREEN_RESUME);
            return 0;
        }
        if (m_pendingFullscreenReminder) {
            if (!FullScreenDetector::Instance().IsFullScreenActive()) {
                KillTimer(m_hwnd, TIMER_FULLSCREEN_RESUME);
                ContentItem itemToShow = *m_pendingFullscreenReminder;
                m_pendingFullscreenReminder.reset();
                LOG_INFO("[FULLSCREEN] Fullscreen mode ended — displaying queued reminder");
                PopupWindow::Instance().Show(&itemToShow);
            }
        } else {
            KillTimer(m_hwnd, TIMER_FULLSCREEN_RESUME);
        }
        return 0;
    }
    if (timerId == TIMER_TRAY_RETRY) {
        if (TrayIcon::Instance().IsCreated()) {
            KillTimer(m_hwnd, TIMER_TRAY_RETRY);
        } else if (TrayIcon::Instance().Create(m_hwnd, m_hInst)) {
            KillTimer(m_hwnd, TIMER_TRAY_RETRY);
            LOG_INFO("Tray icon successfully initialized via retry timer");
        }
        return 0;
    }
    if (timerId == 1006 /* TIMER_SMOOTH_SCROLL */) {
        if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
            if (m_pages[m_activePage]->OnScrollTick()) {
                InvalidateRect(m_hwnd, nullptr, FALSE);
                UpdateWindow(m_hwnd);
                return 0;
            }
        }
        KillTimer(m_hwnd, 1006);
        return 0;
    }
    return 0;
}

LRESULT MainWindow::OnMouseMove(int x, int y) {
    if (!m_mouseTracking) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_hwnd;
        TrackMouseEvent(&tme);
        m_mouseTracking = true;
    }

    bool needsRepaint = false;
    if (InputController::Instance().OnMouseMove((float)x, (float)y)) needsRepaint = true;
    
    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        if (m_pages[m_activePage]->OnMouseMove(x, y)) needsRepaint = true;
    }

    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnMouseLeave() {
    m_mouseTracking = false;
    bool needsRepaint = false;
    if (InputController::Instance().OnMouseLeave()) needsRepaint = true;

    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        m_pages[m_activePage]->OnMouseLeave();
    }
    
    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnMouseWheel(int delta) {
    bool needsRepaint = false;
    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        if (m_pages[m_activePage]->OnMouseWheel(delta)) {
            needsRepaint = true;
            SetTimer(m_hwnd, 1006 /* TIMER_SMOOTH_SCROLL */, 8, nullptr);
        }
    }
    if (needsRepaint) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
        UpdateWindow(m_hwnd);
    }
    return 0;
}

LRESULT MainWindow::OnLButtonDown(int x, int y) {
    LOG_INFO("[INPUT] WM_LBUTTONDOWN hwnd=" + std::to_string((uintptr_t)m_hwnd) + " x=" + std::to_string(x) + " y=" + std::to_string(y));

    bool needsRepaint = false;
    if (InputController::Instance().OnLButtonDown((float)x, (float)y)) needsRepaint = true;

    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        if (m_pages[m_activePage]->OnLButtonDown(x, y)) needsRepaint = true;
    }

    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnLButtonUp(int x, int y) {
    bool needsRepaint = false;
    if (InputController::Instance().OnLButtonUp((float)x, (float)y)) needsRepaint = true;

    if (m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
        if (m_pages[m_activePage]->OnLButtonUp(x, y)) needsRepaint = true;
    }

    if (needsRepaint) InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

LRESULT MainWindow::OnSetCursor(HWND hwnd, UINT hitTest, UINT message) {
    if (hitTest == HTCLIENT) {
        bool hand = InputController::Instance().HasHoveredControl();
        if (!hand && m_activePage >= 0 && m_activePage < kNavItems && m_pages[m_activePage]) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(m_hwnd, &pt);
            hand = m_pages[m_activePage]->HitTestInteractive(pt.x, pt.y);
        }
        SetCursor(LoadCursorW(nullptr, hand ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    return DefWindowProcW(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(hitTest, message));
}

LRESULT MainWindow::OnNcCalcSize(WPARAM wp, LPARAM lp) {
    return DefWindowProcW(m_hwnd, WM_NCCALCSIZE, wp, lp);
}

LRESULT MainWindow::OnNcHitTest(int x, int y) {
    return DefWindowProcW(m_hwnd, WM_NCHITTEST, 0, MAKELPARAM(x, y));
}

LRESULT MainWindow::OnPowerBroadcast(WPARAM wp) {
    if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) {
        Scheduler::Instance().OnSystemWake();
    }
    return TRUE;
}

LRESULT MainWindow::OnTrayCallback(LPARAM lp) {
    TrayIcon::Instance().HandleMessage(m_hwnd, lp);
    return 0;
}

LRESULT MainWindow::OnGetMinMaxInfo(MINMAXINFO* mmi) {
    mmi->ptMinTrackSize.x = (LONG)(ds::Dims::MainWindowMinW * m_dpiScale);
    mmi->ptMinTrackSize.y = (LONG)(ds::Dims::MainWindowMinH * m_dpiScale);
    mmi->ptMaxTrackSize.x = (LONG)(ds::Dims::MainWindowW * 1.35f * m_dpiScale);
    mmi->ptMaxTrackSize.y = (LONG)(ds::Dims::MainWindowH * 1.35f * m_dpiScale);
    return 0;
}

LRESULT MainWindow::OnClose() {
    LOG_INFO("MainWindow::OnClose called");
    if (!TrayIcon::Instance().IsCreated()) {
        if (!TrayIcon::Instance().Create(m_hwnd, m_hInst)) {
            LOG_WARN("Tray icon not available on close — minimizing window to taskbar");
            ShowWindow(m_hwnd, SW_MINIMIZE);
            return 0;
        }
    }
    // Hide to tray instead of closing
    Hide();
    return 0;
}

LRESULT MainWindow::OnDestroy() {
    LOG_INFO("MainWindow::OnDestroy called");
    KillTimer(m_hwnd, TIMER_FULLSCREEN_RESUME);
    KillTimer(m_hwnd, TIMER_TRAY_RETRY);
    m_pendingFullscreenReminder = nullptr;
    PostQuitMessage(0);
    return 0;
}

LRESULT MainWindow::OnUpdateAvailable() {
    return 0;
}

// ── Window procedure ──────────────────────────────────────────────────────────
LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    static UINT s_uTaskbarRestartMsg = RegisterWindowMessageW(L"TaskbarCreated");
    if (msg == s_uTaskbarRestartMsg && s_uTaskbarRestartMsg != 0) {
        TrayIcon::Instance().Create(self->m_hwnd, self->m_hInst);
        return 0;
    }

    switch (msg) {
        case WM_GETICON: {
            if (wp == ICON_SMALL || wp == 2 /* ICON_SMALL2 */) {
                return (LRESULT)self->m_hCurrentIconSmall;
            }
            return (LRESULT)self->m_hCurrentIconBig;
        }
        case WM_PAINT:
            self->OnPaint();
            return 0;
        case WM_SIZE:             return self->OnSize(LOWORD(lp), HIWORD(lp));
        case WM_COMMAND:          return self->OnCommand(wp);
        case WM_REMINDER_TRIGGER: return self->OnReminder(wp == 1);
        case WM_SETTINGS_CHANGED: return self->OnSettingsChanged();
        case WM_THEME_CHANGED: {
            self->TransitionToTheme((ColorTheme)wp);
            return 0;
        }
        case WM_TIMER:            return self->OnTimer((UINT_PTR)wp);
        case WM_MOUSEMOVE:        return self->OnMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        case WM_MOUSEWHEEL:       return self->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
        case WM_MOUSELEAVE:       return self->OnMouseLeave();
        case WM_LBUTTONDOWN:      return self->OnLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        case WM_LBUTTONUP:        return self->OnLButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        case WM_CHAR: {
            if (self->m_activePage >= 0 && self->m_activePage < kNavItems && self->m_pages[self->m_activePage]) {
                if (self->m_pages[self->m_activePage]->OnChar((wchar_t)wp)) {
                    self->Invalidate();
                    return 0;
                }
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_KEYDOWN: {
            if (self->m_activePage >= 0 && self->m_activePage < kNavItems && self->m_pages[self->m_activePage]) {
                if (self->m_pages[self->m_activePage]->OnKeyDown(wp)) {
                    self->Invalidate();
                    return 0;
                }
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_SETCURSOR:        return self->OnSetCursor(hwnd, LOWORD(lp), HIWORD(lp));
        case WM_POWERBROADCAST:   return self->OnPowerBroadcast(wp);
        case WM_TRAY_CALLBACK:    return self->OnTrayCallback(lp);
        case WM_GETMINMAXINFO:    return self->OnGetMinMaxInfo((MINMAXINFO*)lp);
        case WM_NCCALCSIZE:       return DefWindowProcW(hwnd, msg, wp, lp);
        case WM_NCPAINT:          return DefWindowProcW(hwnd, msg, wp, lp);
        case WM_NCACTIVATE:       return DefWindowProcW(hwnd, msg, wp, lp);
        case WM_NCHITTEST:        return DefWindowProcW(hwnd, msg, wp, lp);
        case WM_NCLBUTTONDBLCLK:  return 0; // Disable maximize on title bar double click
        case WM_SYSCOMMAND: {
            if ((wp & 0xFFF0) == SC_MAXIMIZE) return 0; // Ignore maximize commands
            if ((wp & 0xFFF0) == SC_MINIMIZE) {
                LRESULT res = DefWindowProcW(hwnd, msg, wp, lp);
                SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
                return res;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_CLOSE:            return self->OnClose();
        case WM_DESTROY:          return self->OnDestroy();
        case WM_ERASEBKGND:       return 1; // prevent flicker
        case WM_UPDATE_AVAILABLE: return self->OnUpdateAvailable();
        case WM_APP + 100: {
            return 0;
        }
        case WM_APP + 101: {
            AppUIState* newState = reinterpret_cast<AppUIState*>(wp);
            if (newState) {
                UIStateManager::Instance().CommitState(*newState);
                delete newState;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_DPICHANGED: {
            UINT newDpi = HIWORD(wp);
            RECT* newRect = reinterpret_cast<RECT*>(lp);
            return self->OnDpiChanged(newDpi, newRect);
        }
        default:                  return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

LRESULT MainWindow::OnDpiChanged(UINT newDpi, const RECT* newRect) {
    m_dpiScale = newDpi / 96.0f;

    // Resize window
    SetWindowPos(m_hwnd, nullptr, newRect->left, newRect->top,
                 newRect->right - newRect->left, newRect->bottom - newRect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // Recreate text formats with new DPI scale
    if (m_tfVersion) { m_tfVersion->Release(); m_tfVersion = nullptr; }
    if (m_tfAppName) { m_tfAppName->Release(); m_tfAppName = nullptr; }
    if (m_tfNavLabel) { m_tfNavLabel->Release(); m_tfNavLabel = nullptr; }
    if (m_tfIcon) { m_tfIcon->Release(); m_tfIcon = nullptr; }

    InitTextFormats();

    // Trigger components to update sizes
    if (m_navRail) {
        m_navRail->SetDpiScale(m_dpiScale);
        m_navRail->SetBounds(SidebarRect());
    }

    auto cr = ContentRect();
    for (auto& p : m_pages) {
        if (p) p->OnSize(cr);
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
    return 0;
}

} // namespace dhikr
