#include "PopupWindow.h"
#include "MainWindow.h"
#include "DesignSystem.h"
#include "core/LogoRenderer.h"
#include "core/ResourceLoader.h"
#include "../core/Application.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include "../audio/AudioEngine.h"
#include "../core/Database.h"
#include "../core/Scheduler.h"
#include "../core/Localization.h"
#include "../../src/resource.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <vector>
#include <dwmapi.h>
#include <shlwapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")

namespace dhikr {

namespace {
    std::wstring Utf8ToWideLocal(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), size);
        return result;
    }
}

PopupWindow& PopupWindow::Instance() {
    static PopupWindow instance;
    return instance;
}

PopupWindow::~PopupWindow() {
    ReleaseD2D();
    CleanupCachedDC();
    if (m_grainBitmap) { m_grainBitmap->Release(); m_grainBitmap = nullptr; }
    for (auto& sec : m_secondaryPopups) {
        if (sec.hwnd && IsWindow(sec.hwnd)) {
            DestroyWindow(sec.hwnd);
        }
    }
    m_secondaryPopups.clear();
    if (m_hwnd) { DestroyWindow(m_hwnd); m_hwnd = nullptr; }
}

void PopupWindow::HideAll() {
    if (m_hwnd && IsWindow(m_hwnd)) ShowWindow(m_hwnd, SW_HIDE);
    for (auto& sec : m_secondaryPopups) {
        if (sec.hwnd && IsWindow(sec.hwnd)) {
            ShowWindow(sec.hwnd, SW_HIDE);
        }
    }
}

void PopupWindow::CleanupCachedDC() {
    if (m_hCachedMemDC) {
        if (m_hCachedOldBitmap) {
            SelectObject(m_hCachedMemDC, m_hCachedOldBitmap);
            m_hCachedOldBitmap = nullptr;
        }
        if (m_hCachedBitmap) {
            DeleteObject(m_hCachedBitmap);
            m_hCachedBitmap = nullptr;
        }
        DeleteDC(m_hCachedMemDC);
        m_hCachedMemDC = nullptr;
    }
    m_cachedBmpWidth = 0;
    m_cachedBmpHeight = 0;
}

// ── Window class registration ─────────────────────────────────────────────────
void PopupWindow::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
}

bool PopupWindow::Create(HINSTANCE hInstance) {
    m_hInst = hInstance;
    RegisterClass(hInstance);

    // Use GetDpiForSystem temporarily
    m_dpiScale = GetDpiForSystem() / 96.f;

    // Create layered window (hidden initially)
    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName,
        L"DhikrPopup",
        WS_POPUP,
        0, 0, 1, 1,
        nullptr, nullptr, hInstance, this);

    if (!m_hwnd) {
        LOG_ERROR("Failed to create popup window");
        return false;
    }

    UINT dpi = GetDpiForWindow(m_hwnd);
    if (dpi != 0) {
        m_dpiScale = dpi / 96.f;
    }

    // Note: DWM backdrop (Mica/Acrylic) is NOT compatible with WS_EX_LAYERED windows
    // that use UpdateLayeredWindow — they conflict and produce white artifact backgrounds.
    // The popup uses its own alpha-blended rendering via UpdateLayeredWindow instead.


    if (!InitD2D()) {
        // D2D initialisation failed (e.g. driver issue); popup will be skipped
        // gracefully — reminders will still be logged but the overlay won't show.
        LOG_WARN("Popup D2D init failed — popup rendering disabled");
        return true; // window exists; safe to continue without it
    }
    GenerateGrainBitmap(); // safe: already guards on m_dcRenderTarget

    LOG_INFO("Popup window created");
    return true;
}

// ── Grain texture (pre-generated noise bitmap) ────────────────────────────────
void PopupWindow::GenerateGrainBitmap() {
    constexpr int SIZE = ds::Grain::Size;
    if (!m_dcRenderTarget) return;

    std::vector<uint32_t> pixels(SIZE * SIZE);
    std::mt19937 rng(0xDEADBEEF); // deterministic seed
    std::uniform_int_distribution<int> noiseDist(0, 20);

    for (int i = 0; i < SIZE * SIZE; ++i) {
        uint8_t v = (uint8_t)noiseDist(rng);
        uint8_t a = (uint8_t)(v * 3); // alpha: subtle
        pixels[i] = (a << 24) | (v << 16) | (v << 8) | v;
    }

    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f
    );

    m_dcRenderTarget->CreateBitmap(
        D2D1::SizeU(SIZE, SIZE),
        pixels.data(),
        SIZE * 4,
        &props,
        &m_grainBitmap
    );
}

// ── Direct2D init ─────────────────────────────────────────────────────────────
bool PopupWindow::InitD2D() {
    m_d2dFactory    = Application::Instance().D2DFactory();
    m_dwriteFactory = Application::Instance().DWriteFactory();

    // DC render target (for UpdateLayeredWindow)
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.f, 96.f);

    m_d2dFactory->CreateDCRenderTarget(&props, &m_dcRenderTarget);

    const wchar_t* fontName = ds::Font::Arabic();

    // Adaptive text formats (Cairo Arabic)
    m_dwriteFactory->CreateTextFormat(
        fontName, nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        18.f * m_dpiScale,
        L"ar-SA", &m_textFmtMainLarge);
    if (m_textFmtMainLarge) {
        m_textFmtMainLarge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtMainLarge->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtMainLarge->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_textFmtMainLarge->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    m_dwriteFactory->CreateTextFormat(
        fontName, nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        15.f * m_dpiScale,
        L"ar-SA", &m_textFmtMainMedium);
    if (m_textFmtMainMedium) {
        m_textFmtMainMedium->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtMainMedium->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtMainMedium->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_textFmtMainMedium->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    m_dwriteFactory->CreateTextFormat(
        fontName, nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.5f * m_dpiScale,
        L"ar-SA", &m_textFmtMainCompact);
    if (m_textFmtMainCompact) {
        m_textFmtMainCompact->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtMainCompact->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtMainCompact->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_textFmtMainCompact->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    m_dwriteFactory->CreateTextFormat(
        fontName, nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        ds::FontSize::PopupMain * m_dpiScale,
        L"ar-SA", &m_textFmtMain);

    if (m_textFmtMain) {
        m_textFmtMain->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtMain->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtMain->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_textFmtMain->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    m_dwriteFactory->CreateTextFormat(
        fontName, nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        ds::FontSize::PopupSub * m_dpiScale,
        L"ar-SA", &m_textFmtSub);

    if (m_textFmtSub) {
        m_textFmtSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFmtSub->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    }

    m_dwriteFactory->CreateTextFormat(
        fontName, nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        8.5f * m_dpiScale,
        L"en-US", &m_textFmtSmall);

    if (m_textFmtSmall) {
        m_textFmtSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_textFmtSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    // Initialize WIC and load cropped circular logos for all themes + dormant
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory));
    if (SUCCEEDED(hr) && m_wicFactory && m_dcRenderTarget) {
        UINT logoPx = (UINT)(36 * m_dpiScale);
        if (logoPx < 36) logoPx = 36;

        int themeSmallIds[6] = {
            IDR_LOGO_SMALL,
            IDR_LOGO_YELLOW_SMALL,
            IDR_LOGO_BLUE_SMALL,
            IDR_LOGO_RED_SMALL,
            IDR_LOGO_SILVER_SMALL,
            IDR_LOGO_GOLD_SMALL
        };

        for (int i = 0; i < 6; ++i) {
            m_logoBitmaps[i] = ResourceLoader::LoadBitmapFromResource(
                m_hInst, themeSmallIds[i], m_wicFactory, m_dcRenderTarget, RT_RCDATA, logoPx, logoPx);
        }

        m_logoDormantBitmap = ResourceLoader::LoadBitmapFromResource(
            m_hInst, IDR_LOGO_DORMANT_SMALL, m_wicFactory, m_dcRenderTarget, RT_RCDATA, logoPx, logoPx);

        LOG_INFO("PopupWindow: Embedded cropped circular theme logos loaded successfully");
    }

    return m_dcRenderTarget != nullptr;
}

void PopupWindow::ReleaseD2D() {
    for (int i = 0; i < 6; ++i) {
        if (m_logoBitmaps[i]) { m_logoBitmaps[i]->Release(); m_logoBitmaps[i] = nullptr; }
    }
    if (m_logoDormantBitmap)   { m_logoDormantBitmap->Release();   m_logoDormantBitmap   = nullptr; }
    if (m_wicFactory)          { m_wicFactory->Release();          m_wicFactory          = nullptr; }
    if (m_textFmtSmall)        { m_textFmtSmall->Release();        m_textFmtSmall        = nullptr; }
    if (m_textFmtSub)          { m_textFmtSub->Release();          m_textFmtSub          = nullptr; }
    if (m_textFmtMainCompact)  { m_textFmtMainCompact->Release();  m_textFmtMainCompact  = nullptr; }
    if (m_textFmtMainMedium)   { m_textFmtMainMedium->Release();   m_textFmtMainMedium   = nullptr; }
    if (m_textFmtMainLarge)    { m_textFmtMainLarge->Release();    m_textFmtMainLarge    = nullptr; }
    if (m_textFmtMain)         { m_textFmtMain->Release();         m_textFmtMain         = nullptr; }
    if (m_dcRenderTarget)      { m_dcRenderTarget->Release();      m_dcRenderTarget      = nullptr; }
}

// ── Show / Hide ───────────────────────────────────────────────────────────────
void PopupWindow::Show(const ContentItem* item, bool force) {
    if (!m_hwnd || !item) return;
    if (!force && m_state != PopupAnimState::Hidden && m_state != PopupAnimState::Closing) {
        // Already visible — do not abruptly replace text while user is reading!
        return;
    }

    KillTimer(m_hwnd, TIMER_POPUP_ANIM_EXIT);
    KillTimer(m_hwnd, TIMER_POPUP_AUTODISMISS);
    KillTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER);

    m_itemCopy = *item;
    m_item = &m_itemCopy;
    m_copyFeedback = false;
    m_currentCounter = 1;
    m_targetCounter = (m_item && m_item->repetitions > 1) ? m_item->repetitions : 33;

    const auto s = Settings::Instance().GetCopy();
    std::wstring measureText = m_item->text;
    if (!s.showTashkeel) {
        measureText = ds::StripArabicTashkeel(measureText);
    }

    // 1. Adaptive typography selection
    IDWriteTextFormat* mainFmt = m_textFmtMain;
    if (s.popupFontSize == PopupFontSize::Large) {
        mainFmt = m_textFmtMainLarge ? m_textFmtMainLarge : m_textFmtMain;
    } else if (s.popupFontSize == PopupFontSize::Small) {
        mainFmt = m_textFmtMainCompact ? m_textFmtMainCompact : m_textFmtMain;
    } else {
        if (measureText.size() <= 40 && m_textFmtMainLarge) {
            mainFmt = m_textFmtMainLarge;
        } else if (measureText.size() <= 90 && m_textFmtMainMedium) {
            mainFmt = m_textFmtMainMedium;
        } else if (measureText.size() > 90 && m_textFmtMainCompact) {
            mainFmt = m_textFmtMainCompact;
        }
    }

    // 2. Content-adaptive maximum wrap width based on text length & word count
    size_t textLen = measureText.size();
    float maxTextW = 320.f * m_dpiScale;
    if (textLen <= 25) {
        maxTextW = 280.f * m_dpiScale;
    } else if (textLen <= 55) {
        maxTextW = 380.f * m_dpiScale;
    } else if (textLen <= 100) {
        maxTextW = 480.f * m_dpiScale;
    } else if (textLen <= 180) {
        maxTextW = 580.f * m_dpiScale;
    } else {
        maxTextW = 680.f * m_dpiScale;
    }

    float leftPad   = 68.f * m_dpiScale; // Left accent + logo + breathing room
    float rightPad  = 42.f * m_dpiScale; // Right action buttons room
    float padY      = 16.f * m_dpiScale; // Vertical padding

    float desiredTextW = 0.f;
    float desiredTextH = 0.f;

    if (mainFmt && m_dwriteFactory) {
        IDWriteTextLayout* layout = nullptr;
        if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(measureText.c_str(), (UINT32)measureText.size(), mainFmt, maxTextW, 2000.f, &layout))) {
            DWRITE_TEXT_METRICS metrics;
            layout->GetMetrics(&metrics);
            layout->Release();
            desiredTextW = metrics.width;
            desiredTextH = metrics.height;
        }

        if (s.showSource && m_textFmtSub && !m_item->description.empty()) {
            if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(m_item->description.c_str(), (UINT32)m_item->description.size(), m_textFmtSub, maxTextW, 2000.f, &layout))) {
                DWRITE_TEXT_METRICS metrics;
                layout->GetMetrics(&metrics);
                layout->Release();
                desiredTextW = std::max(desiredTextW, metrics.width);
                desiredTextH += metrics.height + 4.f * m_dpiScale;
            }
        }
    }

    // Dynamic width & height tightly bounded with padding
    float finalW = desiredTextW + leftPad + rightPad + 16.f * m_dpiScale;
    float finalH = desiredTextH + padY * 2.f;

    float minW = 280.f * m_dpiScale;
    float maxW = 820.f * m_dpiScale;
    float minH = 72.f * m_dpiScale;
    float maxH = 320.f * m_dpiScale;

    m_width  = std::clamp((int)std::ceil(finalW), (int)minW, (int)maxW);
    m_height = std::clamp((int)std::ceil(finalH), (int)minH, (int)maxH);

    auto monitors = EnumerateMonitors();

    if (s.popupMonitor == PopupMonitor::All && monitors.size() > 1) {
        // Multi-monitor simultaneous mode: show popups on all screens
        RECT pos0 = CalculatePositionForWorkArea(m_width, m_height, monitors[0].rcWork);
        m_screenX = pos0.left;
        m_screenY = pos0.top;

        size_t needed = monitors.size() - 1;
        while (m_secondaryPopups.size() < needed) {
            HWND hSec = CreateWindowExW(
                WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                kClassName, L"DhikrPopupSec", WS_POPUP,
                0, 0, m_width, m_height,
                nullptr, nullptr, m_hInst, nullptr);
            if (hSec) {
                SetWindowLongPtrW(hSec, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
                m_secondaryPopups.push_back({ hSec, 0, 0 });
                LOG_INFO("Created secondary popup HWND=" + std::to_string(reinterpret_cast<uintptr_t>(hSec)));
            } else {
                LOG_ERROR("Failed to create secondary popup! Error=" + std::to_string(GetLastError()));
                break;
            }
        }

        for (size_t i = 1; i < monitors.size() && (i - 1) < m_secondaryPopups.size(); ++i) {
            RECT posI = CalculatePositionForWorkArea(m_width, m_height, monitors[i].rcWork);
            m_secondaryPopups[i - 1].x = posI.left;
            m_secondaryPopups[i - 1].y = posI.top;
            SetWindowPos(m_secondaryPopups[i - 1].hwnd, HWND_TOPMOST,
                posI.left, posI.top,
                m_width, m_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }

        for (size_t i = needed; i < m_secondaryPopups.size(); ++i) {
            if (m_secondaryPopups[i].hwnd && IsWindow(m_secondaryPopups[i].hwnd)) {
                ShowWindow(m_secondaryPopups[i].hwnd, SW_HIDE);
            }
        }
    } else {
        // Single monitor mode (Primary, Active, or Specific)
        RECT pos = CalculatePosition(m_width, m_height);
        m_screenX = pos.left;
        m_screenY = pos.top;

        for (auto& sec : m_secondaryPopups) {
            if (sec.hwnd && IsWindow(sec.hwnd)) ShowWindow(sec.hwnd, SW_HIDE);
        }
    }

    // Position and show the primary window (still transparent)
    SetWindowPos(m_hwnd, HWND_TOPMOST,
        m_screenX, m_screenY,
        m_width, m_height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // Play notification sound
    AudioEngine::Instance().PlayReminder();

    // Start enter animation
    StartEnterAnim();

    LOG_INFO("Popup shown (Monitors detected: " + std::to_string(monitors.size()) + ")");
}

void PopupWindow::Hide() {
    if (!m_hwnd) return;
    KillTimer(m_hwnd, TIMER_POPUP_AUTODISMISS);
    KillTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER);
    StartExitAnim();
}

std::vector<PopupWindow::MonitorInfo> PopupWindow::EnumerateMonitors() {
    std::vector<MonitorInfo> list;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT, LPARAM lp) -> BOOL {
        auto* vec = reinterpret_cast<std::vector<MonitorInfo>*>(lp);
        MONITORINFOEXW mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hMon, &mi)) {
            MonitorInfo info;
            info.index = 0;
            info.hMon = hMon;
            info.rcWork = mi.rcWork;
            info.rcMonitor = mi.rcMonitor;
            info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
            info.name = mi.szDevice;
            vec->push_back(info);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&list));

    // Guarantee Primary monitor is always at index 0, followed by other monitors left-to-right
    std::stable_sort(list.begin(), list.end(), [](const MonitorInfo& a, const MonitorInfo& b) {
        if (a.isPrimary != b.isPrimary) return a.isPrimary > b.isPrimary;
        return a.rcMonitor.left < b.rcMonitor.left;
    });

    for (size_t i = 0; i < list.size(); ++i) {
        list[i].index = (int)i;
    }
    return list;
}

// ── Position calculation ──────────────────────────────────────────────────────
HMONITOR PopupWindow::GetTargetMonitor() {
    auto s = Settings::Instance().GetCopy();
    auto monitors = EnumerateMonitors();
    if (monitors.empty()) {
        return MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    }

    switch (s.popupMonitor) {
        case PopupMonitor::Active: {
            POINT pt;
            GetCursorPos(&pt);
            return MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
        }
        case PopupMonitor::Specific: {
            if (s.specificMonitor >= 0 && s.specificMonitor < (int)monitors.size()) {
                return monitors[s.specificMonitor].hMon;
            }
            break;
        }
        case PopupMonitor::All:
        case PopupMonitor::Primary:
        default: {
            for (const auto& m : monitors) {
                if (m.isPrimary) return m.hMon;
            }
            break;
        }
    }
    return monitors[0].hMon;
}

RECT PopupWindow::CalculatePositionForWorkArea(int w, int h, const RECT& work) {
    int margin = (int)(ds::Dims::PopupMargin * m_dpiScale);
    auto s = Settings::Instance().GetCopy();

    int x, y;
    int pw = work.right  - work.left;
    int ph = work.bottom - work.top;

    switch (s.popupPosition) {
        case PopupPosition::TopLeft:       x = work.left  + margin;       y = work.top  + margin; break;
        case PopupPosition::TopCenter:     x = work.left  + (pw - w) / 2; y = work.top  + margin; break;
        case PopupPosition::TopRight:      x = work.right - w - margin;   y = work.top  + margin; break;
        case PopupPosition::CenterLeft:    x = work.left  + margin;       y = work.top  + (ph - h) / 2; break;
        case PopupPosition::Center:        x = work.left  + (pw - w) / 2; y = work.top  + (ph - h) / 2; break;
        case PopupPosition::CenterRight:   x = work.right - w - margin;   y = work.top  + (ph - h) / 2; break;
        case PopupPosition::BottomLeft:    x = work.left  + margin;       y = work.bottom - h - margin; break;
        case PopupPosition::BottomCenter:  x = work.left  + (pw - w) / 2; y = work.bottom - h - margin; break;
        case PopupPosition::BottomRight:   x = work.right - w - margin;   y = work.bottom - h - margin; break;
        default:                           x = work.left  + (pw - w) / 2; y = work.top  + margin; break;
    }

    // Clamp to monitor work area safely
    int maxX = std::max(static_cast<int>(work.left), static_cast<int>(work.right  - w));
    int maxY = std::max(static_cast<int>(work.top),  static_cast<int>(work.bottom - h));
    x = std::clamp(x, static_cast<int>(work.left), maxX);
    y = std::clamp(y, static_cast<int>(work.top),  maxY);

    return { x, y, x + w, y + h };
}

RECT PopupWindow::CalculatePosition(int w, int h) {
    HMONITOR hMon = GetTargetMonitor();
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }
    return CalculatePositionForWorkArea(w, h, mi.rcWork);
}

// ── Animation helpers ─────────────────────────────────────────────────────────
void PopupWindow::StartEnterAnim() {
    KillTimer(m_hwnd, TIMER_POPUP_ANIM_EXIT);
    KillTimer(m_hwnd, TIMER_POPUP_AUTODISMISS);
    m_state = PopupAnimState::Entering;
    m_animT = 0.f;
    m_animStart = GetTickCount();

    auto s = Settings::Instance().GetCopy();
    float speedMult = 1.0f;
    if (s.animationSpeed == AnimationSpeed::Cinematic) speedMult = 1.35f;
    else if (s.animationSpeed == AnimationSpeed::Snappy) speedMult = 0.65f;

    auto animStyle = s.popupAnimation;
    switch (animStyle) {
        case PopupAnimation::SpringBloom:     m_animDuration = 380.f * speedMult; break;
        case PopupAnimation::LiquidFloat:     m_animDuration = 400.f * speedMult; break;
        case PopupAnimation::HorizontalDrift: m_animDuration = 350.f * speedMult; break;
        case PopupAnimation::OrigamiUnfurl:   m_animDuration = 360.f * speedMult; break;
        case PopupAnimation::EtherealSweep:   m_animDuration = 420.f * speedMult; break;
        case PopupAnimation::ZenBreathing:    m_animDuration = 450.f * speedMult; break;
        case PopupAnimation::RippleGenesis:   m_animDuration = 390.f * speedMult; break;
        case PopupAnimation::Classic:
        default:                              m_animDuration = ds::Anim::PopupEnterMs * speedMult; break;
    }

    Render(0.02f); // Render first frame immediately
    SetTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER, ds::Anim::FrameIntervalMs, nullptr);
}

void PopupWindow::StartExitAnim() {
    KillTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER);
    m_state = PopupAnimState::Closing;
    m_animT = 0.f;
    m_animStart = GetTickCount();

    auto s = Settings::Instance().GetCopy();
    float speedMult = 1.0f;
    if (s.animationSpeed == AnimationSpeed::Cinematic) speedMult = 1.35f;
    else if (s.animationSpeed == AnimationSpeed::Snappy) speedMult = 0.65f;

    m_animDuration = ds::Anim::PopupExitMs * speedMult;
    SetTimer(m_hwnd, TIMER_POPUP_ANIM_EXIT, ds::Anim::FrameIntervalMs, nullptr);
}

void PopupWindow::SetupAutoDismiss() {
    int durationMs = Settings::Instance().GetCopy().popupDurationSec * 1000;
    SetTimer(m_hwnd, TIMER_POPUP_AUTODISMISS, durationMs, nullptr);
}

void PopupWindow::CancelAutoDismiss() {
    KillTimer(m_hwnd, TIMER_POPUP_AUTODISMISS);
}

// ── Rendering ─────────────────────────────────────────────────────────────────
void PopupWindow::Render(float animProgress) {
    if (!m_dcRenderTarget || !m_hwnd) return;

    bool entering = (m_state == PopupAnimState::Entering);
    bool exiting  = (m_state == PopupAnimState::Closing);

    const auto s = Settings::Instance().GetCopy();
    bool animEnabled = s.animationEnabled;

    float alpha = 1.0f;
    float slideOffsetX = 0.0f;
    float slideOffsetY = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float sweepT = 0.0f;

    if (!animEnabled) {
        alpha = exiting ? 0.0f : 1.0f;
        UpdateLayered(alpha, 0.f, 0.f, 1.0f, 1.0f, 0.f);
        return;
    }

    // Direction factor for vertical motion based on screen position
    bool isTop = (s.popupPosition == PopupPosition::TopLeft ||
                  s.popupPosition == PopupPosition::TopCenter ||
                  s.popupPosition == PopupPosition::TopRight);
    float dirY = isTop ? -1.0f : 1.0f;
    float baseDistY = ds::Anim::PopupSlideDistPx * m_dpiScale;

    if (entering) {
        float t = std::clamp(animProgress, 0.0f, 1.0f);
        switch (s.popupAnimation) {
            case PopupAnimation::SpringBloom: {
                // Style 1: Spring Bloom (3D Scale Bloom + Gentle Rise)
                alpha = ds::EaseOutQuad(t);
                float bloom = ds::SpringBloomEase(t);
                scaleX = 0.82f + 0.18f * bloom;
                scaleY = 0.82f + 0.18f * bloom;
                slideOffsetY = dirY * 18.f * m_dpiScale * (1.0f - ds::EaseOutCubic(t));
                slideOffsetX = 0.0f;
                break;
            }
            case PopupAnimation::HorizontalDrift: {
                // Style 2: Horizontal Drift (Lateral Sheet Docking)
                alpha = ds::EaseOutCubic(t);
                float drift = ds::EaseOutQuint(t);
                bool isLeftAlign = (s.popupPosition == PopupPosition::TopLeft ||
                                    s.popupPosition == PopupPosition::CenterLeft ||
                                    s.popupPosition == PopupPosition::BottomLeft);
                float dirX = isLeftAlign ? -1.0f : 1.0f;
                slideOffsetX = dirX * 65.f * m_dpiScale * (1.0f - drift);
                slideOffsetY = 0.0f;
                scaleX = 0.96f + 0.04f * drift;
                scaleY = 0.96f + 0.04f * drift;
                break;
            }
            case PopupAnimation::LiquidFloat: {
                // Style 3: Liquid Float (Fluid Buoyancy Harmonic Wave)
                alpha = ds::EaseOutCubic(t);
                float wave = ds::LiquidWaveEase(t);
                slideOffsetY = dirY * 30.f * m_dpiScale * (1.0f - wave);
                slideOffsetX = 0.0f;
                scaleX = 1.0f;
                scaleY = 0.92f + 0.08f * wave;
                break;
            }
            case PopupAnimation::OrigamiUnfurl: {
                // Style 4: Origami Unfurl (Scroll / Parchment Unfolding)
                alpha = ds::EaseOutQuad(t);
                float unfurlY = ds::EaseOutQuart(t);
                float unfurlX = ds::EaseOutCubic(t);
                scaleX = 0.94f + 0.06f * unfurlX;
                scaleY = 0.35f + 0.65f * unfurlY;
                slideOffsetX = 0.0f;
                slideOffsetY = 0.0f;
                break;
            }
            case PopupAnimation::EtherealSweep: {
                // Style 5: Ethereal Aura Sweep (Luminous Flow + Sheen Sweep)
                alpha = ds::EaseOutQuad(t);
                float drift = ds::EaseOutCubic(t);
                slideOffsetY = dirY * 16.f * m_dpiScale * (1.0f - drift);
                slideOffsetX = 0.0f;
                scaleX = 0.96f + 0.04f * drift;
                scaleY = 0.96f + 0.04f * drift;
                sweepT = t;
                break;
            }
            case PopupAnimation::ZenBreathing: {
                // Style 6: Zen Breathing Ease (Harmonic Respiratory Settle)
                alpha = ds::EaseOutQuad(t);
                float breathe = ds::ZenBreathingEase(t);
                scaleX = 0.88f + 0.12f * breathe;
                scaleY = 0.88f + 0.12f * breathe;
                slideOffsetY = dirY * 12.f * m_dpiScale * (1.0f - ds::EaseOutCubic(t));
                slideOffsetX = 0.0f;
                break;
            }
            case PopupAnimation::RippleGenesis: {
                // Style 7: Ripple Genesis (Oasis Kinetic Wave)
                alpha = ds::EaseOutCubic(t);
                float ripple = ds::RippleGenesisEase(t);
                scaleX = 0.72f + 0.28f * ripple;
                scaleY = 0.72f + 0.28f * ripple;
                slideOffsetY = dirY * 10.f * m_dpiScale * (1.0f - ds::EaseOutCubic(t));
                slideOffsetX = 0.0f;
                break;
            }
            case PopupAnimation::Classic:
            default: {
                // Style 0: Classic Spring Slide
                alpha = ds::EaseOutCubic(t);
                slideOffsetY = dirY * baseDistY * (1.0f - ds::SoftSpringEase(t));
                slideOffsetX = 0.0f;
                scaleX = 1.0f;
                scaleY = 1.0f;
                break;
            }
        }
    } else if (exiting) {
        float t = std::clamp(animProgress, 0.0f, 1.0f);
        alpha = ds::EaseIn(1.0f - t);
        switch (s.popupAnimation) {
            case PopupAnimation::SpringBloom: {
                float sc = 1.0f - 0.12f * ds::EaseIn(t);
                scaleX = sc;
                scaleY = sc;
                slideOffsetY = dirY * 12.f * m_dpiScale * ds::EaseIn(t);
                break;
            }
            case PopupAnimation::HorizontalDrift: {
                bool isLeftAlign = (s.popupPosition == PopupPosition::TopLeft ||
                                    s.popupPosition == PopupPosition::CenterLeft ||
                                    s.popupPosition == PopupPosition::BottomLeft);
                float dirX = isLeftAlign ? -1.0f : 1.0f;
                slideOffsetX = dirX * 45.f * m_dpiScale * ds::EaseIn(t);
                break;
            }
            case PopupAnimation::OrigamiUnfurl: {
                scaleY = 1.0f - 0.60f * ds::EaseIn(t);
                scaleX = 1.0f - 0.06f * ds::EaseIn(t);
                break;
            }
            case PopupAnimation::EtherealSweep: {
                slideOffsetY = dirY * 14.f * m_dpiScale * ds::EaseIn(t);
                scaleX = 1.0f - 0.05f * ds::EaseIn(t);
                scaleY = 1.0f - 0.05f * ds::EaseIn(t);
                break;
            }
            case PopupAnimation::ZenBreathing: {
                scaleX = 1.0f - 0.08f * ds::EaseIn(t);
                scaleY = 1.0f - 0.08f * ds::EaseIn(t);
                slideOffsetY = dirY * 8.f * m_dpiScale * ds::EaseIn(t);
                break;
            }
            case PopupAnimation::RippleGenesis: {
                scaleX = 1.0f - 0.18f * ds::EaseIn(t);
                scaleY = 1.0f - 0.18f * ds::EaseIn(t);
                slideOffsetY = dirY * 10.f * m_dpiScale * ds::EaseIn(t);
                break;
            }
            case PopupAnimation::LiquidFloat:
            case PopupAnimation::Classic:
            default: {
                slideOffsetY = dirY * baseDistY * ds::EaseIn(t);
                break;
            }
        }
    }

    // Add hover brightness boost
    if (m_hovering) {
        alpha = std::min(1.0f, alpha + 0.05f);
    }

    UpdateLayered(alpha, slideOffsetX, slideOffsetY, scaleX, scaleY, sweepT);
}

void PopupWindow::UpdateLayered(float alpha, float slideOffsetX, float slideOffsetY, float scaleX, float scaleY, float sweepT) {
    if (!m_hwnd || !m_dcRenderTarget || m_width <= 0 || m_height <= 0) return;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return;

    // Check if we need to recreate the cached DC and DIB section
    if (!m_hCachedMemDC || m_cachedBmpWidth != m_width || m_cachedBmpHeight != m_height) {
        CleanupCachedDC();

        m_hCachedMemDC = CreateCompatibleDC(hdcScreen);
        if (!m_hCachedMemDC) {
            ReleaseDC(nullptr, hdcScreen);
            return;
        }

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = m_width;
        bmi.bmiHeader.biHeight      = -m_height;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pvBits = nullptr;
        m_hCachedBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
        if (!m_hCachedBitmap) {
            DeleteDC(m_hCachedMemDC);
            m_hCachedMemDC = nullptr;
            ReleaseDC(nullptr, hdcScreen);
            return;
        }

        m_hCachedOldBitmap = (HBITMAP)SelectObject(m_hCachedMemDC, m_hCachedBitmap);
        m_cachedBmpWidth = m_width;
        m_cachedBmpHeight = m_height;
    }

    // Bind DC to D2D render target
    RECT rcBounds = { 0, 0, m_width, m_height };
    HRESULT hrBind = m_dcRenderTarget->BindDC(m_hCachedMemDC, &rcBounds);
    if (FAILED(hrBind)) {
        LOG_WARN("Popup: BindDC failed, skipping frame");
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    m_dcRenderTarget->BeginDraw();
    m_dcRenderTarget->Clear(D2D1::ColorF(0, 0));

    if (scaleX != 1.0f || scaleY != 1.0f) {
        D2D1_POINT_2F center = D2D1::Point2F((float)m_width * 0.5f, (float)m_height * 0.5f);
        m_dcRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(scaleX, scaleY, center));
    } else {
        m_dcRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    // Draw all layers
    DrawBackground(m_dcRenderTarget, alpha, sweepT);
    DrawGrainTexture(m_dcRenderTarget, alpha);
    DrawAccentBar(m_dcRenderTarget, alpha);
    DrawCrescentMark(m_dcRenderTarget, alpha);
    DrawContent(m_dcRenderTarget, alpha);
    DrawCounterPill(m_dcRenderTarget, alpha);
    DrawCloseButton(m_dcRenderTarget, alpha);
    DrawCopyButton(m_dcRenderTarget, alpha);
    DrawPauseButton(m_dcRenderTarget, alpha);

    // Reset transform
    m_dcRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    HRESULT hrEnd = m_dcRenderTarget->EndDraw();
    if (FAILED(hrEnd)) {
        LOG_WARN("Popup EndDraw failed: 0x" + std::to_string(hrEnd));
    }

    // UpdateLayeredWindow
    POINT ptSrc  = { 0, 0 };
    SIZE  szWnd  = { m_width, m_height };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    POINT ptDest = { m_screenX + (int)slideOffsetX, m_screenY + (int)slideOffsetY };

    if (!UpdateLayeredWindow(m_hwnd, hdcScreen, &ptDest, &szWnd,
        m_hCachedMemDC, &ptSrc, 0, &blend, ULW_ALPHA)) {
        LOG_WARN("UpdateLayeredWindow primary failed: " + std::to_string(GetLastError()));
    }

    // Synchronize layered update across all connected monitors
    if (Settings::Instance().GetCopy().popupMonitor == PopupMonitor::All) {
        for (size_t i = 0; i < m_secondaryPopups.size(); ++i) {
            const auto& sec = m_secondaryPopups[i];
            if (sec.hwnd && IsWindow(sec.hwnd)) {
                POINT ptSec = { sec.x + (int)slideOffsetX, sec.y + (int)slideOffsetY };
                if (!UpdateLayeredWindow(sec.hwnd, hdcScreen, &ptSec, &szWnd,
                    m_hCachedMemDC, &ptSrc, 0, &blend, ULW_ALPHA)) {
                    static DWORD lastErr = 0;
                    DWORD err = GetLastError();
                    if (err != lastErr) {
                        lastErr = err;
                        LOG_ERROR("UpdateLayeredWindow secondary " + std::to_string(i) + " failed: " + std::to_string(err));
                    }
                }
            }
        }
    }

    ReleaseDC(nullptr, hdcScreen);
}

void PopupWindow::DrawBackground(ID2D1RenderTarget* rt, float alpha, float sweepT) {
    if (!rt) return;

    const auto s = Settings::Instance().GetCopy();
    float r = ds::Dims::PopupRadius * m_dpiScale;
    if (s.popupCorner == PopupCornerStyle::Squircle) {
        r = 8.f * m_dpiScale;
    } else if (s.popupCorner == PopupCornerStyle::Capsule) {
        r = 24.f * m_dpiScale;
    }

    D2D1_ROUNDED_RECT bgRect{
        D2D1::RectF(0.f, 0.f, (float)m_width, (float)m_height),
        r, r };

    // 1. Surface Material Gradient
    D2D1_GRADIENT_STOP stops[4];
    D2D1_GRADIENT_STOP borderStops[3];

    if (s.popupSurface == PopupSurfaceStyle::DeepObsidian) {
        // High contrast pure obsidian velvet
        stops[0].position = 0.00f; stops[0].color = ds::Rgba(0x0E, 0x13, 0x10, 0.99f);
        stops[1].position = 0.35f; stops[1].color = ds::Rgba(0x09, 0x0D, 0x0A, 0.99f);
        stops[2].position = 0.70f; stops[2].color = ds::Rgba(0x06, 0x08, 0x07, 0.99f);
        stops[3].position = 1.00f; stops[3].color = ds::Rgba(0x04, 0x06, 0x05, 0.99f);

        borderStops[0].position = 0.0f;  borderStops[0].color = ds::Accent::Primary(0.85f);
        borderStops[1].position = 0.45f; borderStops[1].color = ds::Rgba(0x3B, 0x48, 0x3E, 0.80f);
        borderStops[2].position = 1.0f;  borderStops[2].color = ds::Rgba(0x20, 0x27, 0x22, 0.60f);
    } else if (s.popupSurface == PopupSurfaceStyle::AuroraGlow) {
        // Vibrant illuminated emerald aura
        stops[0].position = 0.00f; stops[0].color = ds::Aurora::PopupStop0(1.35f);
        stops[1].position = 0.35f; stops[1].color = ds::Aurora::PopupStop1(1.25f);
        stops[2].position = 0.70f; stops[2].color = ds::Aurora::PopupStop2(1.15f);
        stops[3].position = 1.00f; stops[3].color = ds::Aurora::PopupStop3(1.10f);

        borderStops[0].position = 0.0f;  borderStops[0].color = ds::Accent::Primary(0.95f);
        borderStops[1].position = 0.45f; borderStops[1].color = ds::Aurora::PopupBorderStop1(1.25f);
        borderStops[2].position = 1.0f;  borderStops[2].color = ds::Aurora::PopupBorderStop2(1.0f);
    } else {
        // Frosted Acrylic Glass (Default)
        stops[0].position = 0.00f; stops[0].color = ds::Aurora::PopupStop0(1.0f);
        stops[1].position = 0.35f; stops[1].color = ds::Aurora::PopupStop1(1.0f);
        stops[2].position = 0.70f; stops[2].color = ds::Aurora::PopupStop2(1.0f);
        stops[3].position = 1.00f; stops[3].color = ds::Aurora::PopupStop3(1.0f);

        borderStops[0].position = 0.0f;  borderStops[0].color = ds::Aurora::PopupBorderStop0(1.0f);
        borderStops[1].position = 0.45f; borderStops[1].color = ds::Aurora::PopupBorderStop1(1.0f);
        borderStops[2].position = 1.0f;  borderStops[2].color = ds::Aurora::PopupBorderStop2(1.0f);
    }

    ID2D1GradientStopCollection* pStopCollection = nullptr;
    HRESULT hr = rt->CreateGradientStopCollection(
        stops, 4,
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        &pStopCollection);

    if (SUCCEEDED(hr) && pStopCollection) {
        D2D1_POINT_2F startPt = D2D1::Point2F(0.f, 0.f);
        D2D1_POINT_2F endPt   = D2D1::Point2F((float)m_width * 0.95f, (float)m_height);

        ID2D1LinearGradientBrush* pGradientBrush = nullptr;
        hr = rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(startPt, endPt),
            pStopCollection,
            &pGradientBrush);

        if (SUCCEEDED(hr) && pGradientBrush) {
            pGradientBrush->SetOpacity(alpha);
            rt->FillRoundedRectangle(bgRect, pGradientBrush);
            pGradientBrush->Release();
        }
        pStopCollection->Release();
    } else {
        // Fallback solid fill
        ID2D1SolidColorBrush* brush = nullptr;
        auto bgColor = ds::PopupBg();
        bgColor.a = alpha;
        rt->CreateSolidColorBrush(bgColor, &brush);
        if (brush) {
            rt->FillRoundedRectangle(bgRect, brush);
            brush->Release();
        }
    }

    // 2. Arabesque Islamic Lattice Overlay
    if (s.arabesqueOverlay) {
        ds::Arabesque::DrawLattice(rt, bgRect.rect, 0.025f * alpha);
    }

    // 3. Ethereal Specular Light Sweep Effect
    if (sweepT > 0.001f && sweepT < 0.999f) {
        float sweepX = -80.f + sweepT * ((float)m_width + 160.f);
        float bandW  = 75.f * m_dpiScale;
        D2D1_GRADIENT_STOP sweepStops[3];
        sweepStops[0].position = 0.0f; sweepStops[0].color = D2D1::ColorF(1.f, 1.f, 1.f, 0.0f);
        sweepStops[1].position = 0.5f; sweepStops[1].color = ds::Accent::Primary(0.22f * alpha);
        sweepStops[2].position = 1.0f; sweepStops[2].color = D2D1::ColorF(1.f, 1.f, 1.f, 0.0f);
        ID2D1GradientStopCollection* pSweepCol = nullptr;
        if (SUCCEEDED(rt->CreateGradientStopCollection(sweepStops, 3, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pSweepCol)) && pSweepCol) {
            ID2D1LinearGradientBrush* pSweepBrush = nullptr;
            D2D1_POINT_2F sp0 = D2D1::Point2F(sweepX - bandW, 0.f);
            D2D1_POINT_2F sp1 = D2D1::Point2F(sweepX + bandW, (float)m_height);
            if (SUCCEEDED(rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(sp0, sp1), pSweepCol, &pSweepBrush)) && pSweepBrush) {
                rt->FillRoundedRectangle(bgRect, pSweepBrush);
                pSweepBrush->Release();
            }
            pSweepCol->Release();
        }
    }

    // 4. Specular Glass Rim Border
    ID2D1GradientStopCollection* pBorderStopCol = nullptr;
    hr = rt->CreateGradientStopCollection(
        borderStops, 3,
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        &pBorderStopCol);

    if (SUCCEEDED(hr) && pBorderStopCol) {
        D2D1_POINT_2F startPt = D2D1::Point2F(0.f, 0.f);
        D2D1_POINT_2F endPt   = D2D1::Point2F((float)m_width, (float)m_height);

        ID2D1LinearGradientBrush* pBorderBrush = nullptr;
        hr = rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(startPt, endPt),
            pBorderStopCol,
            &pBorderBrush);

        if (SUCCEEDED(hr) && pBorderBrush) {
            pBorderBrush->SetOpacity(alpha * 0.90f);
            rt->DrawRoundedRectangle(bgRect, pBorderBrush, 1.2f);
            pBorderBrush->Release();
        }
        pBorderStopCol->Release();
    } else {
        // Fallback border
        ID2D1SolidColorBrush* brush = nullptr;
        auto borderColor = ds::PopupBorder();
        borderColor.a = alpha * 0.7f;
        rt->CreateSolidColorBrush(borderColor, &brush);
        if (brush) {
            rt->DrawRoundedRectangle(bgRect, brush, 1.0f);
            brush->Release();
        }
    }
}

void PopupWindow::DrawGrainTexture(ID2D1RenderTarget* rt, float alpha) {
    if (!m_grainBitmap || alpha < 0.05f) return;
    (void)rt;
}

void PopupWindow::DrawAccentBar(ID2D1RenderTarget* rt, float alpha) {
    ID2D1SolidColorBrush* brush = nullptr;
    auto accentColor = ds::Accent::Primary(alpha);
    rt->CreateSolidColorBrush(accentColor, &brush);
    if (!brush) return;

    float barW = 3.5f * m_dpiScale;
    float barH = 32.f * m_dpiScale;
    float padX = 14.f * m_dpiScale;
    float cy   = (float)m_height * 0.5f;

    D2D1_ROUNDED_RECT barRect{
        D2D1::RectF(padX, cy - barH * 0.5f, padX + barW, cy + barH * 0.5f),
        1.75f * m_dpiScale, 1.75f * m_dpiScale };
    rt->FillRoundedRectangle(barRect, brush);

    // Subtle lime glow effect behind bar
    auto glowColor = ds::Accent::Glow(alpha * 0.15f);
    brush->SetColor(glowColor);
    D2D1_ROUNDED_RECT glowRect{
        D2D1::RectF(padX - 2.5f * m_dpiScale, cy - (barH * 0.5f + 2.5f * m_dpiScale),
                    padX + barW + 2.5f * m_dpiScale, cy + (barH * 0.5f + 2.5f * m_dpiScale)),
        4.f * m_dpiScale, 4.f * m_dpiScale };
    rt->FillRoundedRectangle(glowRect, brush);

    brush->Release();
}

ID2D1Bitmap* PopupWindow::GetEffectiveLogo() const {
    if (ds::Accent::IsAppPaused()) {
        return m_logoDormantBitmap;
    }
    int idx = std::clamp((int)Settings::Instance().GetCopy().colorTheme, 0, 5);
    return m_logoBitmaps[idx] ? m_logoBitmaps[idx] : m_logoBitmaps[0];
}

void PopupWindow::DrawCrescentMark(ID2D1RenderTarget* rt, float alpha) {
    float cx     = 40.f * m_dpiScale;
    float cy     = (float)m_height * 0.5f;
    float radius = 14.f * m_dpiScale; // Harmonious circular emblem (28dp diameter)

    D2D1_RECT_F dest = D2D1::RectF(cx - radius, cy - radius, cx + radius, cy + radius);
    auto* bmp = GetEffectiveLogo();
    if (bmp) {
        D2D1_SIZE_F sz = bmp->GetSize();
        D2D1_RECT_F fitRect = LogoRenderer::FitRect(dest, sz.width, sz.height);
        rt->DrawBitmap(bmp, fitRect, alpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        LogoRenderer::Draw(rt, dest, alpha);
    }
}

void PopupWindow::DrawContent(ID2D1RenderTarget* rt, float alpha) {
    if (!m_item) return;

    ID2D1SolidColorBrush* textBrush = nullptr;
    auto textColor = ds::TextPrimary();
    textColor.a = alpha;
    rt->CreateSolidColorBrush(textColor, &textBrush);
    if (!textBrush) return;

    const auto s = Settings::Instance().GetCopy();
    std::wstring displayText = m_item->text;
    if (!s.showTashkeel) {
        displayText = ds::StripArabicTashkeel(displayText);
    }

    float leftPad  = 68.f * m_dpiScale;
    float rightPad = 42.f * m_dpiScale;
    float padY     = 14.f * m_dpiScale;
    float w        = (float)m_width - leftPad - rightPad;

    // Select adaptive typography format
    IDWriteTextFormat* mainFmt = m_textFmtMain;
    if (s.popupFontSize == PopupFontSize::Large) {
        mainFmt = m_textFmtMainLarge ? m_textFmtMainLarge : m_textFmtMain;
    } else if (s.popupFontSize == PopupFontSize::Small) {
        mainFmt = m_textFmtMainCompact ? m_textFmtMainCompact : m_textFmtMain;
    } else {
        if (displayText.size() <= 40 && m_textFmtMainLarge) {
            mainFmt = m_textFmtMainLarge;
        } else if (displayText.size() <= 90 && m_textFmtMainMedium) {
            mainFmt = m_textFmtMainMedium;
        } else if (displayText.size() > 90 && m_textFmtMainCompact) {
            mainFmt = m_textFmtMainCompact;
        }
    }

    float mainH = 0.f;
    float subH  = 0.f;
    IDWriteTextLayout* mainLayout = nullptr;
    IDWriteTextLayout* subLayout  = nullptr;

    // Main text (Arabic, RTL, centered)
    if (mainFmt && !displayText.empty()) {
        if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(displayText.c_str(), (UINT32)displayText.size(), mainFmt, w, 2000.f, &mainLayout))) {
            DWRITE_TEXT_METRICS metrics;
            mainLayout->GetMetrics(&metrics);
            mainH = metrics.height;
        }
    }

    // Description / transliteration (smaller, secondary color)
    if (s.showSource && m_textFmtSub && !m_item->description.empty()) {
        if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(m_item->description.c_str(), (UINT32)m_item->description.size(), m_textFmtSub, w, 2000.f, &subLayout))) {
            DWRITE_TEXT_METRICS metrics;
            subLayout->GetMetrics(&metrics);
            subH = metrics.height + 4.f * m_dpiScale;
        }
    }

    float totalTextH = mainH + subH;
    float availH = (float)m_height;
    float yCursor = std::max(padY, (availH - totalTextH) / 2.f);
    m_counterRect = { 0, 0, 0, 0 };

    if (mainLayout) {
        rt->DrawTextLayout(D2D1::Point2F(leftPad, yCursor), mainLayout, textBrush);
        yCursor += mainH + 4.f * m_dpiScale;
        mainLayout->Release();
    }

    if (subLayout) {
        auto subColor = ds::TextSecondary();
        subColor.a = alpha * 0.85f;
        textBrush->SetColor(subColor);
        rt->DrawTextLayout(D2D1::Point2F(leftPad, yCursor), subLayout, textBrush);
        subLayout->Release();
    }

    textBrush->Release();
}

void PopupWindow::DrawCounterPill(ID2D1RenderTarget* rt, float alpha) {
    (void)rt;
    (void)alpha;
    m_counterRect = { 0, 0, 0, 0 };
}

void PopupWindow::DrawCloseButton(ID2D1RenderTarget* rt, float alpha) {
    float btnSize = 18.f * m_dpiScale;
    float margin  = 8.f * m_dpiScale;
    float cx = (float)m_width  - margin - btnSize / 2.f;
    float cy = margin + btnSize / 2.f;

    m_closeRect = {
        (int)(cx - btnSize/2), (int)(cy - btnSize/2),
        (int)(cx + btnSize/2), (int)(cy + btnSize/2) };

    ID2D1SolidColorBrush* brush = nullptr;
    auto color = ds::TextMuted();
    color.a = alpha * (m_hovering ? 0.8f : 0.5f);
    rt->CreateSolidColorBrush(color, &brush);
    if (!brush) return;

    float arm = 4.5f * m_dpiScale;
    rt->DrawLine(D2D1::Point2F(cx - arm, cy - arm), D2D1::Point2F(cx + arm, cy + arm), brush, 1.5f);
    rt->DrawLine(D2D1::Point2F(cx + arm, cy - arm), D2D1::Point2F(cx - arm, cy + arm), brush, 1.5f);

    brush->Release();
}

void PopupWindow::DrawCopyButton(ID2D1RenderTarget* rt, float alpha) {
    float btnSize = 16.f * m_dpiScale;
    float margin  = 8.f * m_dpiScale;
    float cx = (float)m_width - margin - btnSize / 2.f;
    float cy = (float)m_height * 0.5f;

    m_copyRect = {
        (int)(cx - btnSize/2), (int)(cy - btnSize/2),
        (int)(cx + btnSize/2), (int)(cy + btnSize/2) };

    ID2D1SolidColorBrush* brush = nullptr;
    auto color = m_copyFeedback ? ds::Accent::Primary(alpha) : ds::TextMuted();
    color.a = alpha * (m_hovering ? 0.8f : 0.45f);
    rt->CreateSolidColorBrush(color, &brush);
    if (!brush) return;

    float s = m_dpiScale;
    if (m_copyFeedback) {
        // Small checkmark
        rt->DrawLine(D2D1::Point2F(cx - 4.f*s, cy), D2D1::Point2F(cx - 1.f*s, cy + 3.f*s), brush, 1.5f);
        rt->DrawLine(D2D1::Point2F(cx - 1.f*s, cy + 3.f*s), D2D1::Point2F(cx + 4.f*s, cy - 3.f*s), brush, 1.5f);
    } else {
        // Copy icon: two overlapping rectangles
        D2D1_RECT_F backSheet  = D2D1::RectF(cx - 2.f*s, cy - 5.f*s, cx + 5.f*s, cy + 2.f*s);
        D2D1_RECT_F frontSheet = D2D1::RectF(cx - 5.f*s, cy - 2.f*s, cx + 2.f*s, cy + 5.f*s);
        rt->DrawRectangle(backSheet, brush, 1.2f);
        rt->DrawRectangle(frontSheet, brush, 1.2f);
    }

    brush->Release();
}

void PopupWindow::DrawPauseButton(ID2D1RenderTarget* rt, float alpha) {
    float btnSize = 16.f * m_dpiScale;
    float margin  = 8.f * m_dpiScale;
    float cx = (float)m_width  - margin - btnSize / 2.f;
    float cy = (float)m_height - margin - btnSize / 2.f;

    m_pauseRect = {
        (int)(cx - btnSize/2), (int)(cy - btnSize/2),
        (int)(cx + btnSize/2), (int)(cy + btnSize/2) };

    ID2D1SolidColorBrush* brush = nullptr;
    auto color = ds::TextMuted();
    color.a = alpha * (m_hovering ? 0.7f : 0.4f);
    rt->CreateSolidColorBrush(color, &brush);
    if (!brush) return;

    // Pause icon (two vertical bars)
    float barW   = 2.5f * m_dpiScale;
    float barH   = 8.f * m_dpiScale;
    float spacing= 2.5f * m_dpiScale;
    D2D1_RECT_F left  = D2D1::RectF(cx - spacing - barW, cy - barH/2, cx - spacing,       cy + barH/2);
    D2D1_RECT_F right = D2D1::RectF(cx + spacing,         cy - barH/2, cx + spacing + barW, cy + barH/2);
    rt->FillRectangle(left,  brush);
    rt->FillRectangle(right, brush);

    brush->Release();
}

// ── Hit testing ───────────────────────────────────────────────────────────────
bool PopupWindow::HitTestClose(int x, int y) const {
    return x >= m_closeRect.left && x <= m_closeRect.right &&
           y >= m_closeRect.top  && y <= m_closeRect.bottom;
}

bool PopupWindow::HitTestCopy(int x, int y) const {
    return x >= m_copyRect.left && x <= m_copyRect.right &&
           y >= m_copyRect.top  && y <= m_copyRect.bottom;
}

bool PopupWindow::HitTestPause(int x, int y) const {
    return x >= m_pauseRect.left && x <= m_pauseRect.right &&
           y >= m_pauseRect.top  && y <= m_pauseRect.bottom;
}

bool PopupWindow::HitTestCounter(int x, int y) const {
    return x >= m_counterRect.left && x <= m_counterRect.right &&
           y >= m_counterRect.top  && y <= m_counterRect.bottom;
}

// ── Timer handler ─────────────────────────────────────────────────────────────
LRESULT PopupWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == TIMER_POPUP_ANIM_ENTER) {
        DWORD elapsed = GetTickCount() - m_animStart;
        float t = std::min(1.f, (float)elapsed / m_animDuration);
        Render(t);
        if (t >= 1.f) {
            KillTimer(m_hwnd, TIMER_POPUP_ANIM_ENTER);
            m_state = PopupAnimState::Visible;
            SetupAutoDismiss();

            // Record delivery
            if (m_item) {
                Database::Instance().InsertReminderEvent(m_item->id, m_item->category, "delivered");
            }
        }
    } else if (timerId == TIMER_POPUP_ANIM_EXIT) {
        DWORD elapsed = GetTickCount() - m_animStart;
        float t = std::min(1.f, (float)elapsed / m_animDuration);
        Render(t);
        if (t >= 1.f) {
            KillTimer(m_hwnd, TIMER_POPUP_ANIM_EXIT);
            HideAll();
            m_state = PopupAnimState::Hidden;
            m_item  = nullptr;
            // When popup finishes exiting and main window is hidden, trim working set
            HWND hMain = MainWindow::Instance().GetHwnd();
            if (!hMain || !IsWindowVisible(hMain)) {
                SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
            }
        }
    } else if (timerId == TIMER_POPUP_AUTODISMISS) {
        KillTimer(m_hwnd, TIMER_POPUP_AUTODISMISS);
        StartExitAnim();
    }
    return 0;
}

// ── Mouse handlers ────────────────────────────────────────────────────────────
LRESULT PopupWindow::OnMouseMove(HWND hwnd, int x, int y) {
    if (!m_mouseTracking) {
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        m_mouseTracking = true;
    }

    bool wasHovering = m_hovering;
    m_hovering = true;
    if (!wasHovering) {
        // Pause the auto-dismiss timer while hovering
        CancelAutoDismiss();
        Render(1.f);
    }
    return 0;
}

LRESULT PopupWindow::OnMouseLeave(HWND /*hwnd*/) {
    m_hovering      = false;
    m_mouseTracking = false;
    // Resume auto-dismiss
    SetupAutoDismiss();
    Render(1.f);
    return 0;
}

LRESULT PopupWindow::OnLButtonDown(HWND hwnd, int x, int y) {
    if (HitTestCounter(x, y)) {
        m_currentCounter++;
        AudioEngine::Instance().PlayReminder();
        if (m_currentCounter > m_targetCounter) {
            m_currentCounter = m_targetCounter;
            CancelAutoDismiss();
            StartExitAnim();
        } else {
            Render(1.f);
        }
        return 0;
    } else if (HitTestClose(x, y)) {
        // Dismiss
        CancelAutoDismiss();
        if (m_item) {
            Database::Instance().InsertReminderEvent(m_item->id, m_item->category, "dismissed");
        }
        StartExitAnim();
        return 0;
    } else if (HitTestCopy(x, y)) {
        if (m_item && !m_item->text.empty()) {
            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                size_t len = (m_item->text.size() + 1) * sizeof(wchar_t);
                HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, len);
                if (hGlob) {
                    wchar_t* pBuf = static_cast<wchar_t*>(GlobalLock(hGlob));
                    if (pBuf) {
                        memcpy(pBuf, m_item->text.c_str(), len);
                        GlobalUnlock(hGlob);
                        SetClipboardData(CF_UNICODETEXT, hGlob);
                    }
                }
                CloseClipboard();
            }
            m_copyFeedback = true;
            Render(1.f);
        }
        return 0;
    } else if (HitTestPause(x, y)) {
        // Quick pause 30 min via tray menu approach
        CancelAutoDismiss();
        StartExitAnim();
        Scheduler::Instance().Pause(30);
        return 0;
    } else {
        // Click anywhere else = dismiss
        CancelAutoDismiss();
        StartExitAnim();
    }
    return 0;
}

// ── Window procedure ──────────────────────────────────────────────────────────
LRESULT CALLBACK PopupWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PopupWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<PopupWindow*>(cs->lpCreateParams);
        if (self) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            if (!self->m_hwnd) {
                self->m_hwnd = hwnd;
            }
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    } else {
        self = reinterpret_cast<PopupWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_TIMER:        return self->OnTimer((UINT_PTR)wp);
        case WM_MOUSEMOVE:    return self->OnMouseMove(hwnd, LOWORD(lp), HIWORD(lp));
        case WM_MOUSELEAVE:   return self->OnMouseLeave(hwnd);
        case WM_LBUTTONDOWN:  return self->OnLButtonDown(hwnd, LOWORD(lp), HIWORD(lp));
        case WM_SETCURSOR: {
            if (LOWORD(lp) == HTCLIENT) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                if (self->HitTestClose(pt.x, pt.y) || self->HitTestCopy(pt.x, pt.y) || self->HitTestPause(pt.x, pt.y) || self->HitTestCounter(pt.x, pt.y)) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_NCHITTEST:
            return HTCLIENT;

        case WM_DESTROY:
            return 0;

        case WM_DPICHANGED: {
            UINT newDpi = HIWORD(wp);
            RECT* newRect = reinterpret_cast<RECT*>(lp);
            return self->OnDpiChanged(newDpi, newRect);
        }

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

LRESULT PopupWindow::OnDpiChanged(UINT newDpi, const RECT* newRect) {
    m_dpiScale = newDpi / 96.0f;

    SetWindowPos(m_hwnd, nullptr, newRect->left, newRect->top,
                 newRect->right - newRect->left, newRect->bottom - newRect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // Recreate text formats with new DPI
    if (m_textFmtMainLarge) { m_textFmtMainLarge->Release(); m_textFmtMainLarge = nullptr; }
    if (m_textFmtMainMedium) { m_textFmtMainMedium->Release(); m_textFmtMainMedium = nullptr; }
    if (m_textFmtMainCompact) { m_textFmtMainCompact->Release(); m_textFmtMainCompact = nullptr; }
    if (m_textFmtMain) { m_textFmtMain->Release(); m_textFmtMain = nullptr; }
    if (m_textFmtSub) { m_textFmtSub->Release(); m_textFmtSub = nullptr; }
    if (m_textFmtSmall) { m_textFmtSmall->Release(); m_textFmtSmall = nullptr; }

    // This will skip factory creation but run format creation
    InitD2D();

    return 0;
}

} // namespace dhikr
