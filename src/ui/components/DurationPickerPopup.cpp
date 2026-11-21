#include "DurationPickerPopup.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/Localization.h"
#include <windowsx.h>
#include <algorithm>

namespace dhikr {

DurationPickerPopup& DurationPickerPopup::Instance() {
    static DurationPickerPopup instance;
    return instance;
}

DurationPickerPopup::DurationPickerPopup() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DhikrDurationPickerPopup";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.style = CS_DROPSHADOW;
    RegisterClassExW(&wc);
}

DurationPickerPopup::~DurationPickerPopup() {
    Close();
}

void DurationPickerPopup::Show(HWND parent, int x, int y, int initialMinutes, OnDurationSelected callback) {
    Close();
    m_parent = parent;
    m_minutes = std::clamp(initialMinutes > 0 ? initialMinutes : 30, 1, 1440);
    m_callback = std::move(callback);
    m_hoveredElement = -1;
    m_pressedElement = -1;
    m_firstKeyInput = true;

    if (parent && IsWindow(parent)) {
        m_dpiScale = (float)GetDpiForWindow(parent) / 96.f;
    } else {
        HDC hdc = GetDC(nullptr);
        m_dpiScale = hdc ? (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f : 1.0f;
        if (hdc) ReleaseDC(nullptr, hdc);
    }

    float w = 310.f * m_dpiScale;
    float h = 250.f * m_dpiScale;

    // Multi-monitor aware screen clamping
    POINT pt = { x, y };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (!hMon && parent) {
        hMon = MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST);
    }
    MONITORINFO mi = { sizeof(mi) };
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        RECT work = mi.rcWork;
        if (x + (int)w > work.right - 10) x = work.right - (int)w - 10;
        if (x < work.left + 10) x = work.left + 10;
        if (y + (int)h > work.bottom - 10) y = work.bottom - (int)h - 10;
        if (y < work.top + 10) y = work.top + 10;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"DhikrDurationPickerPopup", L"",
        WS_POPUP,
        x, y, (int)w, (int)h,
        parent, nullptr, GetModuleHandle(nullptr), this);

    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    // Direct2D Render Target
    auto* d2d = Application::Instance().D2DFactory();
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
        m_hwnd, D2D1::SizeU((UINT32)w, (UINT32)h));

    d2d->CreateHwndRenderTarget(&rtProps, &hwndProps, &m_rt);
    if (m_rt) {
        m_rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &m_brush);
    }

    m_dwf = Application::Instance().DWriteFactory();
    if (m_dwf) {
        const wchar_t* fontName = ds::Font::Arabic();

        m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 14.f * m_dpiScale, L"ar-SA", &m_tfTitle);
        if (m_tfTitle) {
            m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_tfTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 36.f * m_dpiScale, L"en-US", &m_tfBig);
        if (m_tfBig) {
            m_tfBig->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_tfBig->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.5f * m_dpiScale, L"ar-SA", &m_tfUnit);
        if (m_tfUnit) {
            m_tfUnit->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_tfUnit->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 11.f * m_dpiScale, L"en-US", &m_tfPill);
        if (m_tfPill) {
            m_tfPill->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_tfPill->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, ds::FontSize::BtnLabel * m_dpiScale, L"ar-SA", &m_tfBtn);
        if (m_tfBtn) {
            m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    ShowWindow(m_hwnd, SW_SHOW);
    SetFocus(m_hwnd);
    SetCapture(m_hwnd);
}

void DurationPickerPopup::Close() {
    if (m_hwnd) {
        if (GetCapture() == m_hwnd) ReleaseCapture();
        if (m_brush)   { m_brush->Release();   m_brush = nullptr; }
        if (m_tfTitle) { m_tfTitle->Release(); m_tfTitle = nullptr; }
        if (m_tfBig)   { m_tfBig->Release();   m_tfBig = nullptr; }
        if (m_tfUnit)  { m_tfUnit->Release();  m_tfUnit = nullptr; }
        if (m_tfPill)  { m_tfPill->Release();  m_tfPill = nullptr; }
        if (m_tfBtn)   { m_tfBtn->Release();   m_tfBtn = nullptr; }
        if (m_rt)      { m_rt->Release();      m_rt = nullptr; }
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

LRESULT CALLBACK DurationPickerPopup::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) return 0;
    DurationPickerPopup* self = (DurationPickerPopup*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (self) return self->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT DurationPickerPopup::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static const int kPresets[] = { 1, 5, 10, 15, 20, 30, 45, 60 };

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);

        int oldHover = m_hoveredElement;
        m_hoveredElement = -1;

        float cx = 310.f * m_dpiScale / 2.f;

        // Up/Down arrows (1, 2)
        auto hitBox = [&](float boxX, float boxY, float w, float h) {
            return (x >= boxX && x <= boxX + w && y >= boxY && y <= boxY + h);
        };

        if (hitBox(cx - 24.f * m_dpiScale, 40.f * m_dpiScale, 48.f * m_dpiScale, 18.f * m_dpiScale)) m_hoveredElement = 1; // Up arrow
        if (hitBox(cx - 24.f * m_dpiScale, 96.f * m_dpiScale, 48.f * m_dpiScale, 18.f * m_dpiScale)) m_hoveredElement = 2; // Down arrow

        // Stepper pills (-5, -1, +1, +5)
        float stepY = 62.f * m_dpiScale;
        float stepH = 28.f * m_dpiScale;
        if (hitBox(cx - 96.f * m_dpiScale, stepY, 34.f * m_dpiScale, stepH)) m_hoveredElement = 3; // -5
        if (hitBox(cx - 56.f * m_dpiScale, stepY, 30.f * m_dpiScale, stepH)) m_hoveredElement = 4; // -1
        if (hitBox(cx + 26.f * m_dpiScale, stepY, 30.f * m_dpiScale, stepH)) m_hoveredElement = 5; // +1
        if (hitBox(cx + 62.f * m_dpiScale, stepY, 34.f * m_dpiScale, stepH)) m_hoveredElement = 6; // +5

        // Quick preset pills (20..27)
        float pillY = 142.f * m_dpiScale;
        float pillH = 26.f * m_dpiScale;
        float startX = 14.f * m_dpiScale;
        float pillW = 32.f * m_dpiScale;
        float gap = 4.f * m_dpiScale;
        for (int i = 0; i < 8; ++i) {
            float px = startX + i * (pillW + gap);
            if (hitBox(px, pillY, pillW, pillH)) {
                m_hoveredElement = 20 + i;
                break;
            }
        }

        // Action buttons: Cancel (10), Apply (11)
        float btnY = 192.f * m_dpiScale;
        float btnH = 36.f * m_dpiScale;
        float btnW = 100.f * m_dpiScale;
        if (hitBox(cx - btnW - 8.f * m_dpiScale, btnY, btnW, btnH)) m_hoveredElement = 10; // Cancel
        if (hitBox(cx + 8.f * m_dpiScale, btnY, btnW, btnH))        m_hoveredElement = 11; // Apply

        if (oldHover != m_hoveredElement) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        if (x < 0 || x > rc.right || y < 0 || y > rc.bottom) {
            Close();
            return 0;
        }

        m_pressedElement = m_hoveredElement;
        m_firstKeyInput = false;

        if (m_pressedElement == 1) { // Up
            m_minutes = std::clamp(m_minutes + 1, 1, 1440);
        } else if (m_pressedElement == 2) { // Down
            m_minutes = std::clamp(m_minutes - 1, 1, 1440);
        } else if (m_pressedElement == 3) { // -5
            m_minutes = std::clamp(m_minutes - 5, 1, 1440);
        } else if (m_pressedElement == 4) { // -1
            m_minutes = std::clamp(m_minutes - 1, 1, 1440);
        } else if (m_pressedElement == 5) { // +1
            m_minutes = std::clamp(m_minutes + 1, 1, 1440);
        } else if (m_pressedElement == 6) { // +5
            m_minutes = std::clamp(m_minutes + 5, 1, 1440);
        } else if (m_pressedElement >= 20 && m_pressedElement <= 27) { // Preset pill
            int idx = m_pressedElement - 20;
            if (idx >= 0 && idx < 8) {
                m_minutes = kPresets[idx];
            }
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_pressedElement == 10 && m_hoveredElement == 10) { // Cancel
            Close();
        } else if (m_pressedElement == 11 && m_hoveredElement == 11) { // Apply
            if (m_callback) m_callback(m_minutes);
            Close();
        }
        m_pressedElement = -1;
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 5 : 1;
        if (delta > 0) m_minutes = std::clamp(m_minutes + step, 1, 1440);
        else          m_minutes = std::clamp(m_minutes - step, 1, 1440);
        m_firstKeyInput = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CHAR: {
        wchar_t ch = static_cast<wchar_t>(wp);
        if (ch >= L'0' && ch <= L'9') {
            int digit = ch - L'0';
            if (m_firstKeyInput) {
                m_minutes = (digit == 0 ? 1 : digit);
                m_firstKeyInput = false;
            } else {
                long long newVal = (long long)m_minutes * 10 + digit;
                if (newVal <= 1440) m_minutes = (int)newVal;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            Close();
        } else if (wp == VK_RETURN) {
            if (m_callback) m_callback(m_minutes);
            Close();
        } else if (wp == VK_BACK) {
            m_minutes = std::max(1, m_minutes / 10);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == VK_UP) {
            int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 5 : 1;
            m_minutes = std::clamp(m_minutes + step, 1, 1440);
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == VK_DOWN) {
            int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 5 : 1;
            m_minutes = std::clamp(m_minutes - step, 1, 1440);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_DPICHANGED: {
        UINT newDpi = HIWORD(wp);
        RECT* newRect = reinterpret_cast<RECT*>(lp);
        m_dpiScale = newDpi / 96.0f;
        int newW = newRect->right - newRect->left;
        int newH = newRect->bottom - newRect->top;
        SetWindowPos(m_hwnd, nullptr, newRect->left, newRect->top,
                     newW, newH,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (m_rt && newW > 0 && newH > 0) {
            m_rt->Resize(D2D1::SizeU((UINT32)newW, (UINT32)newH));
        }
        if (m_tfTitle) { m_tfTitle->Release(); m_tfTitle = nullptr; }
        if (m_tfBig)   { m_tfBig->Release();   m_tfBig   = nullptr; }
        if (m_tfUnit)  { m_tfUnit->Release();  m_tfUnit  = nullptr; }
        if (m_tfPill)  { m_tfPill->Release();  m_tfPill  = nullptr; }
        if (m_tfBtn)   { m_tfBtn->Release();   m_tfBtn   = nullptr; }
        if (m_dwf) {
            const wchar_t* fontName = ds::Font::Arabic();
            m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 14.f * m_dpiScale, L"ar-SA", &m_tfTitle);
            if (m_tfTitle) { m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); m_tfTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
            m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 36.f * m_dpiScale, L"en-US", &m_tfBig);
            if (m_tfBig) { m_tfBig->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); m_tfBig->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
            m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 12.5f * m_dpiScale, L"ar-SA", &m_tfUnit);
            if (m_tfUnit) { m_tfUnit->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); m_tfUnit->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
            m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 11.f * m_dpiScale, L"en-US", &m_tfPill);
            if (m_tfPill) { m_tfPill->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); m_tfPill->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
            m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, ds::FontSize::BtnLabel * m_dpiScale, L"ar-SA", &m_tfBtn);
            if (m_tfBtn) { m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_KILLFOCUS:
        Close();
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

void DurationPickerPopup::DrawArrowBtn(ID2D1RenderTarget* rt, float cx, float cy, bool up, bool hover) {
    m_brush->SetColor(hover ? ds::Accent::Primary() : ds::TextMuted());
    float s = 6.f * m_dpiScale;
    if (up) {
        rt->DrawLine(D2D1::Point2F(cx - s, cy + s * 0.5f), D2D1::Point2F(cx, cy - s * 0.5f), m_brush, 2.f * m_dpiScale);
        rt->DrawLine(D2D1::Point2F(cx, cy - s * 0.5f), D2D1::Point2F(cx + s, cy + s * 0.5f), m_brush, 2.f * m_dpiScale);
    } else {
        rt->DrawLine(D2D1::Point2F(cx - s, cy - s * 0.5f), D2D1::Point2F(cx, cy + s * 0.5f), m_brush, 2.f * m_dpiScale);
        rt->DrawLine(D2D1::Point2F(cx, cy + s * 0.5f), D2D1::Point2F(cx + s, cy - s * 0.5f), m_brush, 2.f * m_dpiScale);
    }
}

void DurationPickerPopup::DrawPillBtn(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, const wchar_t* text, bool hover, bool active) {
    float r = 6.f * m_dpiScale;
    if (active) {
        m_brush->SetColor(ds::Accent::Primary(0.25f));
        rt->FillRoundedRectangle(D2D1::RoundedRect(rect, r, r), m_brush);
        m_brush->SetColor(ds::Accent::Primary());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(rect, r, r), m_brush, 1.2f);
    } else {
        m_brush->SetColor(hover ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(rect, r, r), m_brush);
        m_brush->SetColor(hover ? ds::Accent::Primary(0.6f) : ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(rect, r, r), m_brush, 0.8f);
    }

    if (m_tfPill) {
        m_brush->SetColor(active ? ds::Accent::Primary() : (hover ? ds::TextPrimary() : ds::TextSecondary()));
        rt->DrawTextW(text, (UINT32)wcslen(text), m_tfPill, rect, m_brush);
    }
}

void DurationPickerPopup::Render() {
    if (!m_rt) return;

    m_rt->BeginDraw();

    // 1. Background & Border
    m_rt->Clear(ds::Dark::Surface());
    auto size = m_rt->GetSize();
    D2D1_ROUNDED_RECT boxRect = D2D1::RoundedRect(
        D2D1::RectF(0.5f, 0.5f, size.width - 0.5f, size.height - 0.5f),
        14.f * m_dpiScale, 14.f * m_dpiScale);

    m_brush->SetColor(ds::Dark::Surface());
    m_rt->FillRoundedRectangle(boxRect, m_brush);
    m_brush->SetColor(ds::BorderHighlight());
    m_rt->DrawRoundedRectangle(boxRect, m_brush, 1.2f);

    float cx = size.width / 2.f;

    // 2. Header Title
    std::wstring title = Localization::Instance().Get(L"durationpicker.title");
    if (title.empty() || title == L"durationpicker.title") {
        title = Localization::Instance().IsRTL() ? L"تحديد وقت مخصص" : L"Custom Interval";
    }
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        m_rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle,
            D2D1::RectF(0, 10.f * m_dpiScale, size.width, 32.f * m_dpiScale), m_brush);
    }

    // Divider
    m_brush->SetColor(ds::BorderSubtle());
    m_rt->DrawLine(D2D1::Point2F(16.f * m_dpiScale, 36.f * m_dpiScale),
                   D2D1::Point2F(size.width - 16.f * m_dpiScale, 36.f * m_dpiScale), m_brush, 1.f);

    // 3. Up/Down Arrows
    DrawArrowBtn(m_rt, cx, 48.f * m_dpiScale, true, m_hoveredElement == 1);
    DrawArrowBtn(m_rt, cx, 104.f * m_dpiScale, false, m_hoveredElement == 2);

    // 4. Stepper Buttons (-5, -1, +1, +5)
    float stepY = 62.f * m_dpiScale;
    float stepH = 28.f * m_dpiScale;
    DrawPillBtn(m_rt, D2D1::RectF(cx - 96.f * m_dpiScale, stepY, cx - 62.f * m_dpiScale, stepY + stepH), L"-5", m_hoveredElement == 3);
    DrawPillBtn(m_rt, D2D1::RectF(cx - 56.f * m_dpiScale, stepY, cx - 26.f * m_dpiScale, stepY + stepH), L"-1", m_hoveredElement == 4);
    DrawPillBtn(m_rt, D2D1::RectF(cx + 26.f * m_dpiScale, stepY, cx + 56.f * m_dpiScale, stepY + stepH), L"+1", m_hoveredElement == 5);
    DrawPillBtn(m_rt, D2D1::RectF(cx + 62.f * m_dpiScale, stepY, cx + 96.f * m_dpiScale, stepY + stepH), L"+5", m_hoveredElement == 6);

    // 5. Main Center Display
    if (m_tfBig) {
        wchar_t buf[16];
        swprintf_s(buf, L"%d", m_minutes);
        m_brush->SetColor(ds::Accent::Primary());
        m_rt->DrawTextW(buf, (UINT32)wcslen(buf), m_tfBig,
            D2D1::RectF(cx - 50.f * m_dpiScale, 54.f * m_dpiScale, cx + 50.f * m_dpiScale, 98.f * m_dpiScale), m_brush);
    }

    // 6. Unit & Breakdown Label
    bool isRtl = Localization::Instance().IsRTL();
    std::wstring unitStr;
    if (m_minutes < 60) {
        unitStr = std::to_wstring(m_minutes) + L" " + Localization::Instance().Get(L"home.minutes");
    } else {
        int h = m_minutes / 60;
        int m = m_minutes % 60;
        if (m == 0) {
            unitStr = std::to_wstring(h) + L" " + Localization::Instance().Get(L"schedule.hours");
        } else {
            if (isRtl) {
                unitStr = std::to_wstring(m_minutes) + L" دقيقة (" + std::to_wstring(h) + L" ساعة و " + std::to_wstring(m) + L" دقيقة)";
            } else {
                unitStr = std::to_wstring(m_minutes) + L" min (" + std::to_wstring(h) + L"h " + std::to_wstring(m) + L"m)";
            }
        }
    }

    if (m_tfUnit) {
        m_brush->SetColor(ds::TextSecondary());
        m_rt->DrawTextW(unitStr.c_str(), (UINT32)unitStr.size(), m_tfUnit,
            D2D1::RectF(10.f * m_dpiScale, 114.f * m_dpiScale, size.width - 10.f * m_dpiScale, 134.f * m_dpiScale), m_brush);
    }

    // 7. Quick Presets Row (1m, 5m, 10m, 15m, 20m, 30m, 45m, 60m)
    static const int kPresets[] = { 1, 5, 10, 15, 20, 30, 45, 60 };
    static const wchar_t* kPresetLabels[] = { L"1m", L"5m", L"10m", L"15m", L"20m", L"30m", L"45m", L"1h" };
    float pillY = 142.f * m_dpiScale;
    float pillH = 26.f * m_dpiScale;
    float startX = 14.f * m_dpiScale;
    float pillW = 32.f * m_dpiScale;
    float gap = 4.f * m_dpiScale;
    for (int i = 0; i < 8; ++i) {
        float px = startX + i * (pillW + gap);
        D2D1_RECT_F pr = D2D1::RectF(px, pillY, px + pillW, pillY + pillH);
        bool isAct = (m_minutes == kPresets[i]);
        DrawPillBtn(m_rt, pr, kPresetLabels[i], m_hoveredElement == (20 + i), isAct);
    }

    // 8. Footer Action Buttons: Cancel & Apply
    float btnY = 192.f * m_dpiScale;
    float btnH = 36.f * m_dpiScale;
    float btnW = 105.f * m_dpiScale;
    float rx = ds::Dims::BtnRadius * m_dpiScale;

    // Cancel Button
    D2D1_RECT_F cancelR = D2D1::RectF(cx - btnW - 8.f * m_dpiScale, btnY, cx - 8.f * m_dpiScale, btnY + btnH);
    m_brush->SetColor(m_hoveredElement == 10 ? ds::CardHover() : ds::Surface());
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(cancelR, rx, rx), m_brush);
    m_brush->SetColor(ds::Border());
    m_rt->DrawRoundedRectangle(D2D1::RoundedRect(cancelR, rx, rx), m_brush, 1.f);
    if (m_tfBtn) {
        m_brush->SetColor(m_hoveredElement == 10 ? ds::TextPrimary() : ds::TextSecondary());
        std::wstring cancelStr = Localization::Instance().Get(L"common.cancel");
        m_rt->DrawTextW(cancelStr.c_str(), (UINT32)cancelStr.size(), m_tfBtn, cancelR, m_brush);
    }

    // Apply Button
    D2D1_RECT_F applyR = D2D1::RectF(cx + 8.f * m_dpiScale, btnY, cx + btnW + 8.f * m_dpiScale, btnY + btnH);
    m_brush->SetColor(m_hoveredElement == 11 ? ds::Accent::Primary(0.9f) : ds::Accent::Primary());
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(applyR, rx, rx), m_brush);
    if (m_tfBtn) {
        m_brush->SetColor(ds::Rgba(0x08, 0x0A, 0x09)); // Dark contrasting text
        std::wstring applyStr = Localization::Instance().Get(L"timepicker.apply");
        m_rt->DrawTextW(applyStr.c_str(), (UINT32)applyStr.size(), m_tfBtn, applyR, m_brush);
    }

    m_rt->EndDraw();
}

} // namespace dhikr
