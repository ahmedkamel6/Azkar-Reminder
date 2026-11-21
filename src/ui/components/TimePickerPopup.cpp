#include "TimePickerPopup.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include <windowsx.h>

namespace dhikr {

TimePickerPopup& TimePickerPopup::Instance() {
    static TimePickerPopup instance;
    return instance;
}

TimePickerPopup::TimePickerPopup() {
    // Register class
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DhikrTimePickerPopup";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.style = CS_DROPSHADOW;
    RegisterClassExW(&wc);
}

TimePickerPopup::~TimePickerPopup() {
    Close();
}

void TimePickerPopup::Show(HWND parent, int x, int y, int initialHour, int initialMinute, OnTimeSelected callback) {
    Close();
    m_parent = parent;
    m_hour = initialHour;
    m_minute = initialMinute;
    m_callback = std::move(callback);
    m_hoveredElement = -1;
    m_pressedElement = -1;

    if (parent && IsWindow(parent)) {
        m_dpiScale = (float)GetDpiForWindow(parent) / 96.f;
    } else {
        HDC hdc = GetDC(nullptr);
        m_dpiScale = hdc ? (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f : 1.0f;
        if (hdc) ReleaseDC(nullptr, hdc);
    }

    float w = 240.f * m_dpiScale;
    float h = 180.f * m_dpiScale;

    // Keep on screen (multi-monitor aware)
    POINT pt = { x, y };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (!hMon && parent) {
        hMon = MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST);
    }
    MONITORINFO mi = { sizeof(mi) };
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        RECT work = mi.rcWork;
        if (x + (int)w > work.right - 8) x = work.right - (int)w - 8;
        if (x < work.left + 8) x = work.left + 8;
        if (y + (int)h > work.bottom - 8) y = work.bottom - (int)h - 8;
        if (y < work.top + 8) y = work.top + 8;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"DhikrTimePickerPopup", L"",
        WS_POPUP,
        x, y, (int)w, (int)h,
        parent, nullptr, GetModuleHandle(nullptr), this);

    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    // Init Direct2D
    auto* d2d = Application::Instance().D2DFactory();
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
        m_hwnd, D2D1::SizeU((UINT32)w, (UINT32)h));
    
    d2d->CreateHwndRenderTarget(&rtProps, &hwndProps, &m_rt);
    if (m_rt) {
        m_rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);
    }

    m_dwf = Application::Instance().DWriteFactory();
    if (m_dwf) {
        const wchar_t* fontName = ds::Font::Arabic();
        m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 32.f * m_dpiScale, L"en-US", &m_tfBig);
        if (m_tfBig) {
            m_tfBig->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_tfBig->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, ds::FontSize::BtnLabel * m_dpiScale, L"en-US", &m_tfBtn);
        if (m_tfBtn) {
            m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    ShowWindow(m_hwnd, SW_SHOWNA);
    SetCapture(m_hwnd);
}

void TimePickerPopup::Close() {
    if (m_hwnd) {
        if (GetCapture() == m_hwnd) ReleaseCapture();
        if (m_brush) { m_brush->Release(); m_brush = nullptr; }
        if (m_tfBig) { m_tfBig->Release(); m_tfBig = nullptr; }
        if (m_tfBtn) { m_tfBtn->Release(); m_tfBtn = nullptr; }
        if (m_rt) { m_rt->Release(); m_rt = nullptr; }
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

LRESULT CALLBACK TimePickerPopup::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) return 0;
    TimePickerPopup* self = (TimePickerPopup*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (self) return self->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT TimePickerPopup::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
        
        // Element hit testing
        int oldHover = m_hoveredElement;
        m_hoveredElement = -1;

        float cx = 240.f * m_dpiScale / 2.f;
        float hx = cx - 40.f * m_dpiScale;
        float mx = cx + 20.f * m_dpiScale;
        float btnY = 140.f * m_dpiScale;
        
        // Up/Down arrows (h=1,2, m=3,4)
        auto hitBox = [&](float boxX, float boxY) {
            return (x >= boxX - 20*m_dpiScale && x <= boxX + 20*m_dpiScale &&
                    y >= boxY - 20*m_dpiScale && y <= boxY + 20*m_dpiScale);
        };
        
        if (hitBox(hx, 30*m_dpiScale)) m_hoveredElement = 1; // Hour up
        if (hitBox(hx, 110*m_dpiScale)) m_hoveredElement = 2; // Hour down
        if (hitBox(mx, 30*m_dpiScale)) m_hoveredElement = 3; // Min up
        if (hitBox(mx, 110*m_dpiScale)) m_hoveredElement = 4; // Min down
        
        // AM/PM
        if (x >= cx + 60*m_dpiScale && x <= cx + 110*m_dpiScale && y >= 50*m_dpiScale && y <= 90*m_dpiScale) m_hoveredElement = 5;

        // Cancel / Apply
        if (y >= btnY && y <= btnY + 30*m_dpiScale) {
            if (x >= cx - 60*m_dpiScale && x <= cx - 10*m_dpiScale) m_hoveredElement = 10;
            if (x >= cx + 10*m_dpiScale && x <= cx + 100*m_dpiScale) m_hoveredElement = 11;
        }

        if (oldHover != m_hoveredElement) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        // Check if outside window to close on click
        RECT rc; GetClientRect(hwnd, &rc);
        if (x < 0 || x > rc.right || y < 0 || y > rc.bottom) {
            // outside client area
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
        if (m_pressedElement > 0) {
            if (m_pressedElement == 1) { m_hour = (m_hour + 1) % 24; }
            if (m_pressedElement == 2) { m_hour = (m_hour + 23) % 24; }
            if (m_pressedElement == 3) { m_minute = (m_minute + 1) % 60; }
            if (m_pressedElement == 4) { m_minute = (m_minute + 59) % 60; }
            if (m_pressedElement == 5) { m_hour = (m_hour + 12) % 24; }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_pressedElement == 10 && m_hoveredElement == 10) { // Cancel
            Close();
        } else if (m_pressedElement == 11 && m_hoveredElement == 11) { // Apply
            if (m_callback) m_callback(m_hour, m_minute);
            Close();
        }
        m_pressedElement = -1;
        return 0;
    }
    
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        if (m_hoveredElement == 1 || m_hoveredElement == 2 || m_hoveredElement == 0 /* hour area */) {
            if (delta > 0) m_hour = (m_hour + 1) % 24;
            else m_hour = (m_hour + 23) % 24;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (m_hoveredElement == 3 || m_hoveredElement == 4 || m_hoveredElement == -1 /* min area */) {
            if (delta > 0) m_minute = (m_minute + 1) % 60;
            else m_minute = (m_minute + 59) % 60;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) Close();
        else if (wp == VK_RETURN) {
            if (m_callback) m_callback(m_hour, m_minute);
            Close();
        } else if (wp == VK_UP) {
            m_minute = (m_minute + 1) % 60;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == VK_DOWN) {
            m_minute = (m_minute + 59) % 60;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == VK_LEFT) {
            m_hour = (m_hour + 23) % 24;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == VK_RIGHT) {
            m_hour = (m_hour + 1) % 24;
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

        if (m_tfBig) { m_tfBig->Release(); m_tfBig = nullptr; }
        if (m_tfBtn) { m_tfBtn->Release(); m_tfBtn = nullptr; }

        if (m_dwf) {
            const wchar_t* fontName = ds::Font::Arabic();
            m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 32.f * m_dpiScale, L"en-US", &m_tfBig);
            if (m_tfBig) {
                m_tfBig->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                m_tfBig->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }

            m_dwf->CreateTextFormat(fontName, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, ds::FontSize::BtnLabel * m_dpiScale, L"en-US", &m_tfBtn);
            if (m_tfBtn) {
                m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
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

void TimePickerPopup::DrawArrowBtn(ID2D1RenderTarget* rt, float cx, float cy, bool up, bool hover) {
    m_brush->SetColor(hover ? ds::Accent::Primary() : ds::TextMuted());
    float s = 6.f * m_dpiScale;
    if (up) {
        rt->DrawLine(D2D1::Point2F(cx-s, cy+s), D2D1::Point2F(cx, cy-s), m_brush, 2.f*m_dpiScale);
        rt->DrawLine(D2D1::Point2F(cx, cy-s), D2D1::Point2F(cx+s, cy+s), m_brush, 2.f*m_dpiScale);
    } else {
        rt->DrawLine(D2D1::Point2F(cx-s, cy-s), D2D1::Point2F(cx, cy+s), m_brush, 2.f*m_dpiScale);
        rt->DrawLine(D2D1::Point2F(cx, cy+s), D2D1::Point2F(cx+s, cy-s), m_brush, 2.f*m_dpiScale);
    }
}

void TimePickerPopup::Render() {
    if (!m_rt) return;

    m_rt->BeginDraw();
    
    // Background and border
    m_rt->Clear(ds::Card());
    auto size = m_rt->GetSize();
    m_brush->SetColor(ds::Border());
    m_rt->DrawRectangle(D2D1::RectF(0, 0, size.width, size.height), m_brush, 1.f*m_dpiScale);

    float cx = size.width / 2.f;
    float hx = cx - 40.f * m_dpiScale;
    float mx = cx + 20.f * m_dpiScale;
    float btnY = 140.f * m_dpiScale;

    // Arrows
    DrawArrowBtn(m_rt, hx, 30*m_dpiScale, true, m_hoveredElement == 1);
    DrawArrowBtn(m_rt, hx, 110*m_dpiScale, false, m_hoveredElement == 2);
    DrawArrowBtn(m_rt, mx, 30*m_dpiScale, true, m_hoveredElement == 3);
    DrawArrowBtn(m_rt, mx, 110*m_dpiScale, false, m_hoveredElement == 4);

    // Time text
    if (m_tfBig) {
        wchar_t buf[16];
        int displayHour = m_hour % 12;
        if (displayHour == 0) displayHour = 12;

        m_brush->SetColor(ds::TextPrimary());

        // Hour
        swprintf_s(buf, L"%02d", displayHour);
        m_rt->DrawTextW(buf, 2, m_tfBig, D2D1::RectF(hx-30*m_dpiScale, 50*m_dpiScale, hx+30*m_dpiScale, 90*m_dpiScale), m_brush);

        // Colon
        m_brush->SetColor(ds::TextSecondary());
        m_rt->DrawTextW(L":", 1, m_tfBig, D2D1::RectF(cx-20*m_dpiScale, 45*m_dpiScale, cx, 90*m_dpiScale), m_brush);
        m_brush->SetColor(ds::TextPrimary());

        // Min
        swprintf_s(buf, L"%02d", m_minute);
        m_rt->DrawTextW(buf, 2, m_tfBig, D2D1::RectF(mx-30*m_dpiScale, 50*m_dpiScale, mx+30*m_dpiScale, 90*m_dpiScale), m_brush);

        // AM / PM
        m_brush->SetColor(m_hoveredElement == 5 ? ds::Accent::Primary() : ds::TextSecondary());
        swprintf_s(buf, L"%s", m_hour < 12 ? L"AM" : L"PM");
        if (m_tfBtn) {
            m_rt->DrawTextW(buf, 2, m_tfBtn, D2D1::RectF(cx+60*m_dpiScale, 50*m_dpiScale, cx+110*m_dpiScale, 90*m_dpiScale), m_brush);
        }
    }

    // Buttons
    if (m_tfBtn) {
        // Cancel
        m_brush->SetColor(m_hoveredElement == 10 ? ds::TextPrimary() : ds::TextSecondary());
        m_rt->DrawTextW(L"Cancel", 6, m_tfBtn, D2D1::RectF(cx-60*m_dpiScale, btnY, cx-10*m_dpiScale, btnY+30*m_dpiScale), m_brush);

        // Apply
        m_brush->SetColor(m_hoveredElement == 11 ? ds::Accent::Dark() : ds::Accent::Primary());
        D2D1_RECT_F applyR = D2D1::RectF(cx+10*m_dpiScale, btnY, cx+100*m_dpiScale, btnY+30*m_dpiScale);
        m_rt->FillRoundedRectangle(D2D1::RoundedRect(applyR, 4*m_dpiScale, 4*m_dpiScale), m_brush);
        m_brush->SetColor(ds::Card()); // dark text
        m_rt->DrawTextW(L"Apply", 5, m_tfBtn, applyR, m_brush);
    }

    m_rt->EndDraw();
}

} // namespace dhikr
