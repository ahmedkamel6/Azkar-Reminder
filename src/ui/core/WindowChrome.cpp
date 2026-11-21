#include "WindowChrome.h"
#include "LogoRenderer.h"
#include "../MainWindow.h"
#include "../DesignSystem.h"
#include "../../core/ApplicationMetadata.h"
#include "../../core/Application.h"

namespace dhikr {

void WindowChrome::UpdateMetrics(float dpiScale, int windowWidth, int windowHeight) {
    m_dpiScale = dpiScale;
    m_width = windowWidth;
    m_height = windowHeight;
}

D2D1_RECT_F WindowChrome::GetCaptionBtnRect(int index) const {
    float btnW = ds::Dims::CaptionBtnW * m_dpiScale;
    float th = ds::Dims::TitleBarH * m_dpiScale;
    float startX = m_width - btnW * 2;
    return D2D1::RectF(startX + (index - 1) * btnW, 0, startX + index * btnW, th);
}

LRESULT WindowChrome::HitTest(HWND hwnd, int x, int y) {
    POINT pt = {x, y};
    ScreenToClient(hwnd, &pt);

    // If outside the window client, it's not our problem (though WM_NCHITTEST shouldn't send that usually)
    if (pt.y < 0 || pt.y > m_height || pt.x < 0 || pt.x > m_width) return HTNOWHERE;

    float th = ds::Dims::TitleBarH * m_dpiScale;
    float border = 4.0f * m_dpiScale;

    if (!IsZoomed(hwnd)) {
        // Resizing borders (disabled when maximized)
        if (pt.y < border) {
            if (pt.x < border) return HTTOPLEFT;
            if (pt.x > m_width - border) return HTTOPRIGHT;
            return HTTOP;
        }
        if (pt.y > m_height - border) {
            if (pt.x < border) return HTBOTTOMLEFT;
            if (pt.x > m_width - border) return HTBOTTOMRIGHT;
            return HTBOTTOM;
        }
        if (pt.x < border) return HTLEFT;
        if (pt.x > m_width - border) return HTRIGHT;
    }

    // Title bar area
    if (pt.y < th) {
        // Caption buttons (Min, Close)
        float btnW = ds::Dims::CaptionBtnW * m_dpiScale;
        float startX = m_width - btnW * 2;
        if (pt.x >= startX) return HTCLIENT; // Let client handle the buttons (WM_MOUSEMOVE/LBUTTONDOWN)
        
        return HTCAPTION; // Draggable area
    }

    return HTCLIENT; // Normal application area
}

void WindowChrome::Draw(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteTextFormat* fontApp, IDWriteTextFormat* fontIcon, ID2D1Bitmap* logo) {
    float th = ds::Dims::TitleBarH * m_dpiScale;
    
    // Background
    brush->SetColor(ds::Sidebar());
    rt->FillRectangle(D2D1::RectF(0, 0, (float)m_width, th), brush);
    brush->SetColor(ds::Border());
    rt->DrawLine(D2D1::Point2F(0, th), D2D1::Point2F((float)m_width, th), brush, 0.5f);

    // Application Logo and Title
    float logoSize = 22.f * m_dpiScale;
    float startX = 14.f * m_dpiScale;
    float textX = startX;

    D2D1_RECT_F logoRect = D2D1::RectF(startX, (th - logoSize) / 2.f, startX + logoSize, (th + logoSize) / 2.f);
    ID2D1Bitmap* chromeLogo = MainWindow::Instance().GetEffectiveLogo(true);
    if (chromeLogo) {
        D2D1_SIZE_F sz = chromeLogo->GetSize();
        D2D1_RECT_F fitRect = LogoRenderer::FitRect(logoRect, sz.width, sz.height);
        rt->DrawBitmap(chromeLogo, fitRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        LogoRenderer::Draw(rt, logoRect, 1.0f);
    }
    textX += logoSize + 8.f * m_dpiScale;

    float textWidth = 115.f * m_dpiScale;
    if (fontApp) {
        IDWriteFactory* dwf = Application::Instance().DWriteFactory();
        if (dwf) {
            IDWriteTextLayout* layout = nullptr;
            if (SUCCEEDED(dwf->CreateTextLayout(ApplicationMetadata::AppName, (UINT32)wcslen(ApplicationMetadata::AppName), fontApp, 400.f * m_dpiScale, th, &layout)) && layout) {
                DWRITE_TEXT_METRICS tm;
                layout->GetMetrics(&tm);
                textWidth = tm.width;
                layout->Release();
            }
        }

        brush->SetColor(ds::TextPrimary());
        D2D1_RECT_F textRect = D2D1::RectF(textX, 0, textX + textWidth, th);
        DWRITE_PARAGRAPH_ALIGNMENT pa = fontApp->GetParagraphAlignment();
        fontApp->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        rt->DrawTextW(ApplicationMetadata::AppName, (UINT32)wcslen(ApplicationMetadata::AppName), fontApp, textRect, brush);
        fontApp->SetParagraphAlignment(pa);
    }

    // Pill badge: v1.2
    float badgeX = textX + textWidth + 12.f * m_dpiScale;
    float badgeW = 62.f * m_dpiScale;
    float badgeH = 19.f * m_dpiScale;
    float badgeY = (th - badgeH) / 2.f;
    D2D1_RECT_F badgeRect = D2D1::RectF(badgeX, badgeY, badgeX + badgeW, badgeY + badgeH);
    float badgeRadius = 4.f * m_dpiScale;

    brush->SetColor(ds::Accent::Primary(0.12f));
    rt->FillRoundedRectangle(D2D1::RoundedRect(badgeRect, badgeRadius, badgeRadius), brush);

    brush->SetColor(ds::Accent::Primary(0.60f));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(badgeRect, badgeRadius, badgeRadius), brush, 1.0f);

    IDWriteFactory* dwf = Application::Instance().DWriteFactory();
    if (dwf) {
        IDWriteTextFormat* tfBadge = nullptr;
        if (SUCCEEDED(dwf->CreateTextFormat(L"Cairo", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.5f * m_dpiScale, L"en-US", &tfBadge)) && tfBadge) {
            tfBadge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            tfBadge->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            brush->SetColor(ds::Accent::Primary());
            const wchar_t* badgeText = L"v1.2";
            rt->DrawTextW(badgeText, (UINT32)wcslen(badgeText), tfBadge, badgeRect, brush);
            tfBadge->Release();
        }
    }

    // Caption Buttons
    if (!fontIcon) return;
    
    DWRITE_TEXT_ALIGNMENT ta = fontIcon->GetTextAlignment();
    DWRITE_PARAGRAPH_ALIGNMENT pa = fontIcon->GetParagraphAlignment();
    fontIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fontIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    for (int i = 1; i <= 2; ++i) { // 1=min, 2=close
        D2D1_RECT_F btnRect = GetCaptionBtnRect(i);
        
        // Slightly inset visual hover pill so it harmonizes cleanly with rounded window corners
        D2D1_RECT_F visualRect = btnRect;
        visualRect.top += 3.f * m_dpiScale;
        visualRect.bottom -= 3.f * m_dpiScale;
        visualRect.left += 2.f * m_dpiScale;
        visualRect.right -= (i == 2 ? 6.f * m_dpiScale : 2.f * m_dpiScale);

        float radius = 4.f * m_dpiScale;

        if (m_pressedBtn == i) {
            brush->SetColor(i == 2 ? ds::Rgba(0xC4, 0x2B, 0x1C) : ds::Rgba(0x33, 0x33, 0x33));
            rt->FillRoundedRectangle(D2D1::RoundedRect(visualRect, radius, radius), brush);
        } else if (m_hoveredBtn == i) {
            brush->SetColor(i == 2 ? ds::Rgba(0xE8, 0x11, 0x23) : ds::CardHover());
            rt->FillRoundedRectangle(D2D1::RoundedRect(visualRect, radius, radius), brush);
        }
        
        brush->SetColor((m_hoveredBtn == i && i == 2) ? ds::Rgba(0xFF, 0xFF, 0xFF) : ds::TextPrimary());
        const wchar_t* glyph = (i == 1) ? ds::Icon::Minimize : ds::Icon::Close;
        rt->DrawTextW(glyph, 1, fontIcon, btnRect, brush);
    }

    fontIcon->SetTextAlignment(ta);
    fontIcon->SetParagraphAlignment(pa);
}

bool WindowChrome::OnMouseMove(float x, float y) {
    int hover = 0;
    for (int i = 1; i <= 2; ++i) {
        D2D1_RECT_F r = GetCaptionBtnRect(i);
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            hover = i;
            break;
        }
    }
    if (m_hoveredBtn != hover) {
        m_hoveredBtn = hover;
        return true;
    }
    return false;
}

bool WindowChrome::OnMouseLeave() {
    if (m_hoveredBtn != 0 || m_pressedBtn != 0) {
        m_hoveredBtn = 0;
        m_pressedBtn = 0;
        return true;
    }
    return false;
}

bool WindowChrome::OnLButtonDown(float x, float y) {
    if (m_hoveredBtn != 0) {
        m_pressedBtn = m_hoveredBtn;
        return true;
    }
    return false;
}

bool WindowChrome::OnLButtonUp(HWND hwnd, float x, float y) {
    int pressed = m_pressedBtn;
    m_pressedBtn = 0;
    if (pressed != 0 && pressed == m_hoveredBtn) {
        if (pressed == 1) ShowWindow(hwnd, SW_MINIMIZE);
        else if (pressed == 2) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return true;
    }
    return pressed != 0;
}

}
