#include "DropdownPopup.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/Localization.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>

namespace dhikr {

DropdownPopup& DropdownPopup::Instance() {
    static DropdownPopup instance;
    return instance;
}

DropdownPopup::DropdownPopup() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DhikrDropdownPopup";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.style = CS_DROPSHADOW;
    RegisterClassExW(&wc);
}

DropdownPopup::~DropdownPopup() {
    Close();
}

void DropdownPopup::Show(HWND parent, int screenX, int screenY, float minWidth,
                         const std::vector<DropdownItem>& items, OnItemSelected callback) {
    Close();
    m_parent = parent;
    m_items = items;
    m_callback = std::move(callback);
    m_hoveredIndex = -1;
    m_scrollY = 0.f;
    m_maxScroll = 0.f;
    m_isRtl = Localization::Instance().IsRTL();

    if (parent && IsWindow(parent)) {
        m_dpiScale = (float)GetDpiForWindow(parent) / 96.f;
    } else {
        HDC hdc = GetDC(nullptr);
        m_dpiScale = hdc ? (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f : 1.0f;
        if (hdc) ReleaseDC(nullptr, hdc);
    }

    m_itemH = 34.f * m_dpiScale;
    float pad = 6.f * m_dpiScale;
    m_width = std::max(minWidth * m_dpiScale, 160.f * m_dpiScale);

    float totalContentH = pad * 2.f + (float)items.size() * m_itemH;
    float maxAllowedH = 224.f * m_dpiScale; // Cap at max ~6.5 items visible

    if (totalContentH > maxAllowedH) {
        m_height = maxAllowedH;
        m_maxScroll = totalContentH - maxAllowedH;
    } else {
        m_height = totalContentH;
        m_maxScroll = 0.f;
    }

    int x = screenX;
    int y = screenY;

    // Constrain to parent window so dropdown never pokes outside the app window
    if (parent && IsWindow(parent)) {
        RECT clientRc;
        if (GetClientRect(parent, &clientRc)) {
            POINT ptTL = { clientRc.left, clientRc.top };
            POINT ptBR = { clientRc.right, clientRc.bottom };
            ClientToScreen(parent, &ptTL);
            ClientToScreen(parent, &ptBR);

            if (x + (int)m_width > ptBR.x - 4) {
                x = ptBR.x - (int)m_width - 4;
            }
            if (x < ptTL.x + 4) {
                x = ptTL.x + 4;
            }

            // Check if dropdown extends beyond parent window bottom
            if (y + (int)m_height > ptBR.y - 8) {
                int spaceBelow = ptBR.y - 8 - y;
                int ctrlTop = screenY - (int)(36.f * m_dpiScale);
                int spaceAbove = ctrlTop - (ptTL.y + 8);

                if (spaceAbove >= (int)m_height) {
                    // Fits fully above the control
                    y = ctrlTop - (int)m_height - 2;
                } else if (spaceAbove > spaceBelow) {
                    // More room above: pop upwards and clamp height
                    int h = std::max((int)(80.f * m_dpiScale), spaceAbove);
                    y = ctrlTop - h - 2;
                    m_height = (float)h;
                    m_maxScroll = std::max(0.f, totalContentH - m_height);
                } else {
                    // More room below: clamp height downwards
                    int h = std::max((int)(80.f * m_dpiScale), spaceBelow);
                    m_height = (float)h;
                    m_maxScroll = std::max(0.f, totalContentH - m_height);
                }
            }
        }
    }

    // Multi-monitor aware boundary check
    POINT pt = { x, y };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    if (!hMon && parent) {
        hMon = MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST);
    }

    MONITORINFO mi = { sizeof(mi) };
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        RECT work = mi.rcWork;
        if (x + (int)m_width > work.right - 8) {
            x = work.right - (int)m_width - 8;
        }
        if (x < work.left + 8) {
            x = work.left + 8;
        }
        if (y + (int)m_height > work.bottom - 8) {
            y = screenY - (int)m_height - 6;
        }
        if (y < work.top + 8) {
            y = work.top + 8;
        }
    }

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"DhikrDropdownPopup", L"",
        WS_POPUP,
        x, y, (int)m_width, (int)m_height,
        parent, nullptr, GetModuleHandle(nullptr), this);

    if (!m_hwnd) return;

    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    // Modern rounded corners
    DWORD cornerPref = 2; // DWMWCP_ROUNDED
    DwmSetWindowAttribute(m_hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPref, sizeof(cornerPref));

    // DirectX Render Target
    auto* d2d = Application::Instance().D2DFactory();
    if (d2d) {
        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
            m_hwnd, D2D1::SizeU((UINT32)m_width, (UINT32)m_height));
        d2d->CreateHwndRenderTarget(&rtProps, &hwndProps, &m_rt);
        if (m_rt) {
            m_rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &m_brush);
        }
    }

    m_dwf = Application::Instance().DWriteFactory();
    if (m_dwf) {
        HRESULT hr = m_dwf->CreateTextFormat(
            L"Cairo", nullptr, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 12.5f * m_dpiScale,
            m_isRtl ? L"ar-SA" : L"en-US", &m_tfItem);
        if (FAILED(hr) || !m_tfItem) {
            m_dwf->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, 12.5f * m_dpiScale,
                m_isRtl ? L"ar-SA" : L"en-US", &m_tfItem);
        }
        if (m_tfItem) {
            m_tfItem->SetTextAlignment(m_isRtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
            m_tfItem->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    ShowWindow(m_hwnd, SW_SHOW);
    SetCapture(m_hwnd);
    Render();
}

void DropdownPopup::Close() {
    if (m_hwnd) {
        if (GetCapture() == m_hwnd) ReleaseCapture();
        if (m_brush)  { m_brush->Release();  m_brush = nullptr; }
        if (m_tfItem) { m_tfItem->Release(); m_tfItem = nullptr; }
        if (m_rt)     { m_rt->Release();     m_rt = nullptr; }
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

LRESULT CALLBACK DropdownPopup::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = (DropdownPopup*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (self) {
        return self->HandleMessage(hwnd, msg, wp, lp);
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT DropdownPopup::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
            float pad = 6.f * m_dpiScale;
            int oldHover = m_hoveredIndex;
            if (x >= 0 && x <= (int)m_width && y >= (int)pad && y <= (int)(m_height - pad)) {
                float contentY = (float)y - pad + m_scrollY;
                m_hoveredIndex = (int)(contentY / m_itemH);
                if (m_hoveredIndex >= (int)m_items.size()) m_hoveredIndex = -1;
            } else {
                m_hoveredIndex = -1;
            }
            if (m_hoveredIndex != oldHover) {
                Render();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            float pad = 6.f * m_dpiScale;

            if (x < 0 || x > (int)m_width || y < 0 || y > (int)m_height) {
                Close();
                return 0;
            }

            float contentY = (float)y - pad + m_scrollY;
            int idx = (int)(contentY / m_itemH);
            if (idx >= 0 && idx < (int)m_items.size()) {
                int chosenId = m_items[idx].id;
                auto cb = m_callback;
                Close();
                if (cb) cb(chosenId);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (m_maxScroll > 0.f) {
                int delta = GET_WHEEL_DELTA_WPARAM(wp);
                float scrollStep = m_itemH * 1.5f;
                m_scrollY -= (delta > 0 ? 1.f : -1.f) * scrollStep;
                m_scrollY = std::clamp(m_scrollY, 0.f, m_maxScroll);

                // Update hover
                POINT pt; GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                float pad = 6.f * m_dpiScale;
                float contentY = (float)pt.y - pad + m_scrollY;
                if (pt.x >= 0 && pt.x <= (int)m_width && pt.y >= (int)pad && pt.y <= (int)(m_height - pad)) {
                    m_hoveredIndex = (int)(contentY / m_itemH);
                    if (m_hoveredIndex >= (int)m_items.size()) m_hoveredIndex = -1;
                } else {
                    m_hoveredIndex = -1;
                }
                Render();
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wp == VK_ESCAPE) {
                Close();
                return 0;
            }
            break;
        }

        case WM_KILLFOCUS: {
            Close();
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void DropdownPopup::Render() {
    if (!m_rt || !m_brush) return;

    m_rt->BeginDraw();

    // Solid dark popup container
    D2D1_RECT_F fullRect = D2D1::RectF(0.f, 0.f, m_width, m_height);
    float cornerR = 8.f * m_dpiScale;

    // Popup backdrop fill
    m_brush->SetColor(ds::PopupBg());
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(fullRect, cornerR, cornerR), m_brush);

    // Popup border
    m_brush->SetColor(ds::PopupBorder());
    m_rt->DrawRoundedRectangle(D2D1::RoundedRect(fullRect, cornerR, cornerR), m_brush, 1.0f);

    float pad = 6.f * m_dpiScale;

    // Push clip rect for smooth scrolling
    D2D1_RECT_F clipRect = D2D1::RectF(0.f, pad, m_width, m_height - pad);
    m_rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float curY = pad - m_scrollY;
    float itemPadX = 12.f * m_dpiScale;
    float scrollbarW = (m_maxScroll > 0.f) ? (6.f * m_dpiScale) : 0.f;

    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        D2D1_RECT_F itemRect = D2D1::RectF(pad, curY, m_width - pad - scrollbarW, curY + m_itemH);
        float itemR = 6.f * m_dpiScale;

        // Draw only if visible in viewport
        if (itemRect.bottom >= pad && itemRect.top <= m_height - pad) {
            bool isHovered = ((int)i == m_hoveredIndex);
            if (item.isSelected) {
                // Selected item pill background
                m_brush->SetColor(ds::Accent::Primary(0.22f));
                m_rt->FillRoundedRectangle(D2D1::RoundedRect(itemRect, itemR, itemR), m_brush);

                // Left or right emerald indicator notch
                float notchW = 3.f * m_dpiScale;
                D2D1_RECT_F notchRect = m_isRtl
                    ? D2D1::RectF(itemRect.right - notchW, itemRect.top + 6.f * m_dpiScale, itemRect.right, itemRect.bottom - 6.f * m_dpiScale)
                    : D2D1::RectF(itemRect.left, itemRect.top + 6.f * m_dpiScale, itemRect.left + notchW, itemRect.bottom - 6.f * m_dpiScale);
                m_brush->SetColor(ds::Accent::Primary(1.0f));
                m_rt->FillRoundedRectangle(D2D1::RoundedRect(notchRect, 2.f, 2.f), m_brush);
            } else if (isHovered) {
                // Hovered item background
                m_brush->SetColor(D2D1::ColorF(1.f, 1.f, 1.f, 0.08f));
                m_rt->FillRoundedRectangle(D2D1::RoundedRect(itemRect, itemR, itemR), m_brush);
            }

            // Text
            D2D1_RECT_F textRect = D2D1::RectF(
                itemRect.left + (m_isRtl ? itemPadX : itemPadX + 6.f * m_dpiScale),
                itemRect.top,
                itemRect.right - (m_isRtl ? itemPadX + 6.f * m_dpiScale : itemPadX),
                itemRect.bottom);

            if (item.isSelected) {
                m_brush->SetColor(ds::Accent::Primary(1.0f));
            } else if (isHovered) {
                m_brush->SetColor(ds::TextPrimary());
            } else {
                m_brush->SetColor(ds::TextSecondary());
            }

            if (m_tfItem) {
                m_rt->DrawText(item.text.c_str(), (UINT32)item.text.size(),
                    m_tfItem, textRect, m_brush);
            }
        }

        curY += m_itemH;
    }

    m_rt->PopAxisAlignedClip();

    // Draw subtle scrollbar if needed
    if (m_maxScroll > 0.f) {
        float scrollTrackH = m_height - pad * 2.f;
        float thumbH = std::max(20.f * m_dpiScale, scrollTrackH * (scrollTrackH / (scrollTrackH + m_maxScroll)));
        float thumbY = pad + (m_scrollY / m_maxScroll) * (scrollTrackH - thumbH);
        float thumbX = m_isRtl ? (pad + 1.f * m_dpiScale) : (m_width - pad - 4.f * m_dpiScale);

        m_brush->SetColor(D2D1::ColorF(1.f, 1.f, 1.f, 0.22f));
        D2D1_ROUNDED_RECT thumbRect = D2D1::RoundedRect(
            D2D1::RectF(thumbX, thumbY, thumbX + 3.f * m_dpiScale, thumbY + thumbH),
            1.5f * m_dpiScale, 1.5f * m_dpiScale);
        m_rt->FillRoundedRectangle(thumbRect, m_brush);
    }

    m_rt->EndDraw();
}

} // namespace dhikr
