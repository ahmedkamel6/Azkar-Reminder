#include "SettingsPage.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/Settings.h"
#include "../../core/Scheduler.h"
#include "../../core/Logger.h"
#include "../../core/Localization.h"
#include "../../audio/AudioEngine.h"
#include "../../update/Updater.h"
#include "../../core/ApplicationMetadata.h"
#include "../PopupWindow.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <commdlg.h>
#include <shellapi.h>

namespace dhikr {

SettingsPage::SettingsPage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;
    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    bool isRtl = Localization::Instance().IsRTL();
    const wchar_t* fontName = ds::Font::Arabic();
    const wchar_t* locale   = isRtl ? L"ar-SA" : L"en-US";

    auto mk = [&](float sz, DWRITE_FONT_WEIGHT wt, IDWriteTextFormat** out) {
        dwf->CreateTextFormat(fontName, nullptr, wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sz * m_dpiScale, locale, out);
    };
    mk(ds::FontSize::PageTitle, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfTitle);
    mk(15.f,                    DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfSection);
    mk(ds::FontSize::Body,      DWRITE_FONT_WEIGHT_REGULAR,   &m_tfLabel);
    mk(ds::FontSize::Body,      DWRITE_FONT_WEIGHT_REGULAR,   &m_tfValue);
    mk(ds::FontSize::BtnLabel,  DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfBtn);

    const wchar_t* iconFontFamily = ds::Icon::GetFontFamily(dwf);
    dwf->CreateTextFormat(
        iconFontFamily, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.f * m_dpiScale, L"en-US", &m_tfIcon);
    if (m_tfIcon) {
        m_tfIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    dwf->CreateTextFormat(
        iconFontFamily, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        16.f * m_dpiScale, L"en-US", &m_tfSectionIcon);
    if (m_tfSectionIcon) {
        m_tfSectionIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfSectionIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    if (m_tfBtn) {
        m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfTitle) {
        m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }
    if (m_tfSection) {
        m_tfSection->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSection->SetTextAlignment(isRtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
        m_tfSection->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_tfLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfValue) {
        m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_tfValue->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    m_cachedMonitors = PopupWindow::EnumerateMonitors();
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
}

SettingsPage::~SettingsPage() {
    if (m_tfTitle)       { m_tfTitle->Release();       m_tfTitle       = nullptr; }
    if (m_tfSection)     { m_tfSection->Release();     m_tfSection     = nullptr; }
    if (m_tfLabel)       { m_tfLabel->Release();       m_tfLabel       = nullptr; }
    if (m_tfValue)       { m_tfValue->Release();       m_tfValue       = nullptr; }
    if (m_tfBtn)         { m_tfBtn->Release();         m_tfBtn         = nullptr; }
    if (m_tfIcon)        { m_tfIcon->Release();        m_tfIcon        = nullptr; }
    if (m_tfSectionIcon) { m_tfSectionIcon->Release(); m_tfSectionIcon = nullptr; }
    if (m_brush)         { m_brush->Release();         m_brush         = nullptr; }
}

void SettingsPage::DrawSectionHeader(ID2D1RenderTarget* rt, float x, float& y, float w, const wchar_t* locKey, const wchar_t* iconGlyph) {
    if (!m_brush || !m_tfSection) return;

    // Subtle divider line before section (except the first one)
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y), m_brush, 1.f);
    y += 14.f * m_dpiScale;

    std::wstring title = Localization::Instance().Get(locKey);
    bool isRtl = Localization::Instance().IsRTL();

    float headerH  = 26.f * m_dpiScale;
    float iconSize = 20.f * m_dpiScale;
    float gap      = 8.f * m_dpiScale;

    m_brush->SetColor(ds::Accent::Primary());

    if (isRtl) {
        D2D1_RECT_F iconR = D2D1::RectF(x + w - iconSize, y, x + w, y + headerH);
        if (m_tfSectionIcon && iconGlyph) {
            rt->DrawTextW(iconGlyph, (UINT32)wcslen(iconGlyph), m_tfSectionIcon, iconR, m_brush);
        }
        D2D1_RECT_F textR = D2D1::RectF(x, y, x + w - iconSize - gap, y + headerH);
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfSection, textR, m_brush);
    } else {
        D2D1_RECT_F iconR = D2D1::RectF(x, y, x + iconSize, y + headerH);
        if (m_tfSectionIcon && iconGlyph) {
            rt->DrawTextW(iconGlyph, (UINT32)wcslen(iconGlyph), m_tfSectionIcon, iconR, m_brush);
        }
        D2D1_RECT_F textR = D2D1::RectF(x + iconSize + gap, y, x + w, y + headerH);
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfSection, textR, m_brush);
    }

    y += 32.f * m_dpiScale;
}

void SettingsPage::DrawToggle(ID2D1RenderTarget* rt, float x, float y, bool on, bool hover) {
    float tw = ds::Dims::ToggleW * m_dpiScale;
    float th = ds::Dims::ToggleH * m_dpiScale;
    float kd = ds::Dims::ToggleKnobD * m_dpiScale;
    float rx = th / 2.f;

    D2D1_RECT_F trackRect = D2D1::RectF(x, y, x + tw, y + th);

    if (on) {
        auto trackColor = hover ? ds::Accent::Primary(0.90f) : ds::Accent::Primary();
        m_brush->SetColor(trackColor);
        rt->FillRoundedRectangle(D2D1::RoundedRect(trackRect, rx, rx), m_brush);

        // High contrast knob: dark obsidian on light/bright tracks (Silver, Gold, Yellow, Green),
        // or pure white on darker tracks
        float lum = 0.299f * trackColor.r + 0.587f * trackColor.g + 0.114f * trackColor.b;
        D2D1_COLOR_F knobColor = (lum >= 0.58f) ? ds::Rgba(0x0C, 0x10, 0x0E, 1.0f) : ds::Rgba(0xFF, 0xFF, 0xFF, 1.0f);

        float kx = x + tw - kd - (th - kd) / 2.f;
        float ky = y + (th - kd) / 2.f;
        m_brush->SetColor(knobColor);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx + kd / 2.f, ky + kd / 2.f), kd / 2.f, kd / 2.f), m_brush);
    } else {
        // High contrast disabled track: dark obsidian pill with distinct border
        m_brush->SetColor(hover ? ds::Rgba(0x20, 0x27, 0x22, 1.0f) : ds::Rgba(0x16, 0x1C, 0x18, 1.0f));
        rt->FillRoundedRectangle(D2D1::RoundedRect(trackRect, rx, rx), m_brush);
        m_brush->SetColor(hover ? ds::Accent::Primary(0.5f) : ds::Rgba(0x52, 0x60, 0x56, 1.0f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(trackRect, rx, rx), m_brush, 1.2f);

        float kx = x + (th - kd) / 2.f;
        float ky = y + (th - kd) / 2.f;
        m_brush->SetColor(hover ? ds::Rgba(0xCB, 0xD5, 0xE1, 1.0f) : ds::Rgba(0x94, 0xA3, 0xB8, 1.0f));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx + kd / 2.f, ky + kd / 2.f), kd / 2.f, kd / 2.f), m_brush);
    }
}

void SettingsPage::DrawLabel(ID2D1RenderTarget* rt, float x, float y, const wchar_t* text, float controlReserveW) {
    if (!m_tfLabel || !m_brush) return;
    float labelH = 26.f * m_dpiScale;
    if (y + labelH < m_contentY - 10.f || y > m_contentY + m_viewportH + 10.f) return;

    m_brush->SetColor(ds::TextPrimary());

    bool isRtl = Localization::Instance().IsRTL();
    float ctrl = controlReserveW * m_dpiScale;
    float gap  = 16.f * m_dpiScale;

    float lx, lRight;
    if (isRtl) {
        lx     = m_contentX + ctrl + gap;
        lRight = m_contentX + m_contentW;
    } else {
        lx     = m_contentX;
        lRight = m_contentX + m_contentW - ctrl - gap;
    }

    rt->DrawTextW(text, (UINT32)wcslen(text), m_tfLabel,
        D2D1::RectF(lx, y, lRight, y + labelH), m_brush);
}

void SettingsPage::DrawDropdown(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* val, bool hover) {
    if (r.bottom < m_contentY - 10.f || r.top > m_contentY + m_viewportH + 10.f) return;
    float rx = ds::Dims::InputRadius * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    m_brush->SetColor(hover ? ds::CardHover() : ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(hover ? ds::Accent::Primary(0.6f) : ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 1.0f);
    if (m_tfValue) {
        m_brush->SetColor(ds::TextPrimary());
        D2D1_RECT_F tr = isRtl
            ? D2D1::RectF(r.left + 28*m_dpiScale, r.top, r.right - 14*m_dpiScale, r.bottom)
            : D2D1::RectF(r.left + 14*m_dpiScale, r.top, r.right - 28*m_dpiScale, r.bottom);
        rt->DrawTextW(val, (UINT32)wcslen(val), m_tfValue, tr, m_brush);
    }
    // Chevron
    m_brush->SetColor(hover ? ds::Accent::Primary() : ds::TextSecondary());
    float cx = isRtl ? (r.left + 14 * m_dpiScale) : (r.right - 14 * m_dpiScale);
    float cy = (r.top + r.bottom) / 2.f;
    rt->DrawLine(D2D1::Point2F(cx-4*m_dpiScale, cy-2*m_dpiScale), D2D1::Point2F(cx, cy+2*m_dpiScale), m_brush, 1.8f);
    rt->DrawLine(D2D1::Point2F(cx, cy+2*m_dpiScale), D2D1::Point2F(cx+4*m_dpiScale, cy-2*m_dpiScale), m_brush, 1.8f);
}

void SettingsPage::DrawButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r,
                               const wchar_t* label, bool accent, bool hover, bool pressed) {
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    D2D1_RECT_F drawR = r;
    
    if (pressed) {
        float shrink = 1.0f * m_dpiScale;
        drawR.left += shrink; drawR.top += shrink;
        drawR.right -= shrink; drawR.bottom -= shrink;
    }
    
    if (accent) {
        auto accCol = hover ? ds::Accent::Primary(0.9f) : ds::Accent::Primary();
        m_brush->SetColor(accCol);
        rt->FillRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush);
        float lum = 0.299f * accCol.r + 0.587f * accCol.g + 0.114f * accCol.b;
        m_brush->SetColor(lum >= 0.58f ? ds::Rgba(0x08,0x0A,0x09) : ds::Rgba(0xFF,0xFF,0xFF));
    } else {
        m_brush->SetColor(hover ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush);
        m_brush->SetColor(hover ? ds::BorderHighlight() : ds::Rgba(0x3E, 0x48, 0x40));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush, 1.f);
        m_brush->SetColor(ds::Rgba(0xFF, 0xFF, 0xFF)); // High contrast pure white text
    }
    if (m_tfBtn) rt->DrawTextW(label, (UINT32)wcslen(label), m_tfBtn, drawR, m_brush);
}

void SettingsPage::DrawIconButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r,
                                  const wchar_t* iconGlyph, bool hover, bool pressed) {
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    D2D1_RECT_F drawR = r;

    if (pressed) {
        float shrink = 1.0f * m_dpiScale;
        drawR.left += shrink; drawR.top += shrink;
        drawR.right -= shrink; drawR.bottom -= shrink;
    }

    m_brush->SetColor(hover ? ds::CardHover() : ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush);
    m_brush->SetColor(hover ? ds::BorderHighlight() : ds::Rgba(0x3E, 0x48, 0x40));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush, 1.f);

    m_brush->SetColor(hover ? ds::Accent::Primary() : ds::Rgba(0xFF, 0xFF, 0xFF));
    if (m_tfIcon && iconGlyph) {
        rt->DrawTextW(iconGlyph, (UINT32)wcslen(iconGlyph), m_tfIcon, drawR, m_brush);
    }
}

void SettingsPage::DrawButtonWithIcon(ID2D1RenderTarget* rt, const D2D1_RECT_F& r,
                                      const wchar_t* iconGlyph, const wchar_t* label,
                                      bool accent, bool hover, bool pressed) {
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    D2D1_RECT_F drawR = r;

    if (pressed) {
        float shrink = 1.0f * m_dpiScale;
        drawR.left += shrink; drawR.top += shrink;
        drawR.right -= shrink; drawR.bottom -= shrink;
    }

    if (accent) {
        auto accCol = hover ? ds::Accent::Primary(0.9f) : ds::Accent::Primary();
        m_brush->SetColor(accCol);
        rt->FillRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush);
        float lum = 0.299f * accCol.r + 0.587f * accCol.g + 0.114f * accCol.b;
        m_brush->SetColor(lum >= 0.58f ? ds::Rgba(0x08,0x0A,0x09) : ds::Rgba(0xFF,0xFF,0xFF));
    } else {
        m_brush->SetColor(hover ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush);
        m_brush->SetColor(hover ? ds::BorderHighlight() : ds::Rgba(0x3E, 0x48, 0x40));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(drawR, rx, rx), m_brush, 1.f);
        m_brush->SetColor(ds::Rgba(0xFF, 0xFF, 0xFF)); // High contrast pure white text
    }

    bool isRtl = Localization::Instance().IsRTL();
    float iconW = 18.f * m_dpiScale;
    float gap = 6.f * m_dpiScale;

    if (isRtl) {
        D2D1_RECT_F iconR = D2D1::RectF(drawR.right - iconW - 12.f * m_dpiScale, drawR.top, drawR.right - 12.f * m_dpiScale, drawR.bottom);
        D2D1_RECT_F textR = D2D1::RectF(drawR.left + 8.f * m_dpiScale, drawR.top, iconR.left - gap, drawR.bottom);
        if (m_tfIcon && iconGlyph) rt->DrawTextW(iconGlyph, (UINT32)wcslen(iconGlyph), m_tfIcon, iconR, m_brush);
        if (m_tfBtn && label) {
            m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawTextW(label, (UINT32)wcslen(label), m_tfBtn, textR, m_brush);
        }
    } else {
        D2D1_RECT_F iconR = D2D1::RectF(drawR.left + 12.f * m_dpiScale, drawR.top, drawR.left + 12.f * m_dpiScale + iconW, drawR.bottom);
        D2D1_RECT_F textR = D2D1::RectF(iconR.right + gap, drawR.top, drawR.right - 8.f * m_dpiScale, drawR.bottom);
        if (m_tfIcon && iconGlyph) rt->DrawTextW(iconGlyph, (UINT32)wcslen(iconGlyph), m_tfIcon, iconR, m_brush);
        if (m_tfBtn && label) {
            m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawTextW(label, (UINT32)wcslen(label), m_tfBtn, textR, m_brush);
        }
    }
}

void SettingsPage::DrawSlider(ID2D1RenderTarget* rt, float x, float y, float w, float value) {
    float trackH = 4.f * m_dpiScale;
    float ky = y + (ds::Dims::ToggleH * m_dpiScale - trackH) / 2.f;
    float rx = trackH / 2.f;

    // Track
    m_brush->SetColor(ds::Border());
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, ky, x+w, ky+trackH), rx, rx), m_brush);
    // Fill
    float fillW = w * value;
    m_brush->SetColor(ds::Accent::Primary());
    if (fillW > 0) rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, ky, x+fillW, ky+trackH), rx, rx), m_brush);
    // Knob
    float knobR = 7.f * m_dpiScale;
    float kx = x + fillW;
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx, ky + trackH/2.f), knobR, knobR), m_brush);
    m_brush->SetColor(ds::Background());
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx, ky + trackH/2.f), knobR*0.5f, knobR*0.5f), m_brush);
}

void SettingsPage::DrawPositionGrid(ID2D1RenderTarget* rt, float x, float y) {
    auto& pos = m_pendingSettings.popupPosition;
    float dotSize  = 18.f * m_dpiScale;
    float cellSize = dotSize + 8.f * m_dpiScale;

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            int idx = row * 3 + col;
            float cx = x + col * cellSize + dotSize / 2.f;
            float cy = y + row * cellSize + dotSize / 2.f;

            bool selected = ((int)pos == idx);
            if (selected) {
                auto accCol = ds::Accent::Primary();
                m_brush->SetColor(accCol);
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotSize/2.f, dotSize/2.f), m_brush);
                float lum = 0.299f * accCol.r + 0.587f * accCol.g + 0.114f * accCol.b;
                m_brush->SetColor(lum >= 0.58f ? ds::Rgba(0x08, 0x0A, 0x09) : ds::Rgba(0xFF, 0xFF, 0xFF));
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 3.f * m_dpiScale, 3.f * m_dpiScale), m_brush);
            } else {
                m_brush->SetColor(ds::Rgba(0x18, 0x1E, 0x19));
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotSize/2.f, dotSize/2.f), m_brush);
                m_brush->SetColor(ds::Rgba(0x4A, 0x58, 0x4D));
                rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotSize/2.f, dotSize/2.f), m_brush, 1.0f);
            }
        }
    }
}

void SettingsPage::DrawColorThemeSelector(ID2D1RenderTarget* rt, float x, float y, float w) {
    if (!m_brush) return;

    D2D1_COLOR_F palette[6] = {
        D2D1::ColorF(0xB7/255.f, 0xF3/255.f, 0x4A/255.f), // Green
        D2D1::ColorF(0xFA/255.f, 0xDC/255.f, 0x26/255.f), // Radiant Solar Yellow (#FADC26)
        D2D1::ColorF(0x38/255.f, 0xBD/255.f, 0xF8/255.f), // Blue
        D2D1::ColorF(0xF8/255.f, 0x71/255.f, 0x71/255.f), // Red
        D2D1::ColorF(0xE2/255.f, 0xE8/255.f, 0xF0/255.f), // Silver
        D2D1::ColorF(0xD4/255.f, 0xAF/255.f, 0x37/255.f)  // Regal Antique Metallic Gold (#D4AF37)
    };

    float dotRadius = 11.f * m_dpiScale;
    float step = 34.f * m_dpiScale;
    float cy = y + (ds::Dims::InputH * m_dpiScale) / 2.f;
    float startX = x + 18.f * m_dpiScale;

    for (int i = 0; i < 6; ++i) {
        float cx = startX + i * step;
        bool selected = ((int)m_pendingSettings.colorTheme == i);
        bool hovered  = (m_hoveredElement == (100 + i));

        // Outer selection / hover ring
        if (selected) {
            m_brush->SetColor(palette[i]);
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotRadius + 4.5f * m_dpiScale, dotRadius + 4.5f * m_dpiScale), m_brush, 2.0f);
        } else if (hovered) {
            D2D1_COLOR_F hCol = palette[i];
            hCol.a = 0.5f;
            m_brush->SetColor(hCol);
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotRadius + 3.5f * m_dpiScale, dotRadius + 3.5f * m_dpiScale), m_brush, 1.5f);
        }

        // Inner solid color circle
        m_brush->SetColor(palette[i]);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotRadius, dotRadius), m_brush);

        // Thin dark border for contrast
        m_brush->SetColor(D2D1::ColorF(0, 0, 0, 0.35f));
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotRadius, dotRadius), m_brush, 0.75f);

        // Center indicator dot for selected item
        if (selected) {
            float lum = 0.299f * palette[i].r + 0.587f * palette[i].g + 0.114f * palette[i].b;
            m_brush->SetColor(lum >= 0.60f ? D2D1::ColorF(0.08f, 0.10f, 0.08f, 0.95f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 3.5f * m_dpiScale, 3.5f * m_dpiScale), m_brush);
        }
    }
}

void SettingsPage::CommitSettings(bool notifyLanguage) {
    float savedScroll = m_scrollY;
    float savedTarget = m_targetScrollY;
    Settings::Instance().UpdatePreferences(m_pendingSettings);
    Settings::Instance().Save();
    if (notifyLanguage) {
        Localization::Instance().SetLanguage(m_pendingSettings.language);
    }
    m_hasUnsavedChanges = false;
    MainWindow::Instance().OnSettingsChanged();
    m_scrollY = savedScroll;
    m_targetScrollY = savedTarget;
    Scheduler::Instance().Recalculate();
    MainWindow::Instance().Invalidate();
}

void SettingsPage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);

    bool isRtl = Localization::Instance().IsRTL();
    if (m_tfTitle)   m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfSection) {
        m_tfSection->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSection->SetTextAlignment(isRtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    }
    if (m_tfLabel) {
        m_tfLabel->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_tfLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfValue) {
        m_tfValue->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_tfValue->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfBtn)     m_tfBtn->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    float pad = ds::Space::XL * m_dpiScale;
    float x = bounds.left + pad;
    float y = bounds.top  + pad;
    float w = bounds.right - bounds.left - pad * 2.f;
    m_contentX = x; m_contentW = w;

    // ── 1. Page Title Header (Fixed) ──────────────────────────────────────────
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        std::wstring title = Localization::Instance().Get(L"settings.title");
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle, D2D1::RectF(x, y, x + w, y + 30.f * m_dpiScale), m_brush);
    }
    float headerBottom = y + 36.f * m_dpiScale;

    // Save bar height reservation at bottom
    float saveBarH = m_hasUnsavedChanges ? (56.f * m_dpiScale) : 0.f;

    // ── 2. Scrollable Canvas Setup ───────────────────────────────────────────
    float scrollAreaTop = headerBottom + 6.f * m_dpiScale;
    float scrollAreaBottom = bounds.bottom - pad - saveBarH;
    m_viewportH = std::max(10.f, scrollAreaBottom - scrollAreaTop);
    m_contentY = scrollAreaTop;

    D2D1_RECT_F clipRect = D2D1::RectF(bounds.left, scrollAreaTop, bounds.right, scrollAreaBottom);
    rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float curY = scrollAreaTop + 8.f * m_dpiScale - m_scrollY;
    float rowH = 46.f * m_dpiScale;
    float fh   = ds::Dims::InputH * m_dpiScale;
    float btnH = ds::Dims::BtnH * m_dpiScale;
    float fieldW = 200.f;
    float fwPx   = fieldW * m_dpiScale;

    auto ctrlX = [&](float ctrlW) -> float {
        return isRtl ? x : (x + w - ctrlW * m_dpiScale);
    };

    // ══════════════════════════════════════════════════════════════════════════
    // Section 1: General (الإعدادات العامة)
    // ══════════════════════════════════════════════════════════════════════════
    DrawSectionHeader(rt, x, curY, w, L"settings.general", ds::Icon::Settings);

    // Language
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.language").c_str(), fieldW);
    const wchar_t* langStr = m_pendingSettings.language == L"ar" ? L"العربية" : (m_pendingSettings.language == L"en" ? L"English" : L"System Default");
    float fX = ctrlX(fieldW);
    DrawDropdown(rt, D2D1::RectF(fX, curY, fX + fwPx, curY + fh), langStr, m_hoveredElement == 1);
    curY += rowH;

    // Color Theme (طابع الألوان)
    float themeW = 236.f;
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.color_theme").c_str(), themeW);
    DrawColorThemeSelector(rt, ctrlX(themeW), curY, themeW * m_dpiScale);
    curY += rowH;

    // Start with Windows
    float togW = ds::Dims::ToggleW;
    DrawLabel(rt, x, curY + 4*m_dpiScale, Localization::Instance().Get(L"settings.start_with_windows").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), curY, m_pendingSettings.startWithWindows, m_hoveredElement == 2);
    curY += rowH + 10.f * m_dpiScale;

    // ══════════════════════════════════════════════════════════════════════════
    // Section 2: Popup & Display (النافذة المنبثقة والشاشات)
    // ══════════════════════════════════════════════════════════════════════════
    DrawSectionHeader(rt, x, curY, w, L"settings.popup", ds::Icon::Monitor);

    // Multi-Monitor Target Selector
    if (m_cachedMonitors.empty()) {
        m_cachedMonitors = PopupWindow::EnumerateMonitors();
    }
    const auto& monitors = m_cachedMonitors;
    std::wstring monDisplay;
    if (m_pendingSettings.popupMonitor == PopupMonitor::Primary) {
        monDisplay = Localization::Instance().Get(L"settings.monitor_primary");
    } else if (m_pendingSettings.popupMonitor == PopupMonitor::All) {
        monDisplay = Localization::Instance().Get(L"settings.monitor_all");
    } else if (m_pendingSettings.popupMonitor == PopupMonitor::Active) {
        monDisplay = Localization::Instance().Get(L"settings.monitor_active");
    } else if (m_pendingSettings.popupMonitor == PopupMonitor::Specific) {
        monDisplay = Localization::Instance().Get(L"settings.monitor_num") + L" " + std::to_wstring(m_pendingSettings.specificMonitor + 1);
        if (m_pendingSettings.specificMonitor >= 0 && m_pendingSettings.specificMonitor < (int)monitors.size()) {
            if (monitors[m_pendingSettings.specificMonitor].isPrimary) {
                monDisplay += L" (" + Localization::Instance().Get(L"settings.monitor_primary") + L")";
            }
        }
    } else {
        monDisplay = Localization::Instance().Get(L"settings.monitor_primary");
    }

    float monFieldW = 240.f;
    float mfwPx = monFieldW * m_dpiScale;
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.multi_monitor_title").c_str(), monFieldW);
    float mfX = ctrlX(monFieldW);
    DrawDropdown(rt, D2D1::RectF(mfX, curY, mfX + mfwPx, curY + fh), monDisplay.c_str(), m_hoveredElement == 3);
    curY += rowH;

    // Position Picker Grid
    DrawLabel(rt, x, curY + 8*m_dpiScale, Localization::Instance().Get(L"settings.position").c_str(), 100.f);
    DrawPositionGrid(rt, ctrlX(100.f), curY);
    curY += rowH + 28*m_dpiScale;

    // Duration
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.duration").c_str(), fieldW);
    std::wstring durLabel = std::to_wstring(m_pendingSettings.popupDurationSec) + (isRtl ? L" ثوانٍ" : L"s");
    float ddX = ctrlX(fieldW);
    DrawDropdown(rt, D2D1::RectF(ddX, curY, ddX + fwPx, curY + fh), durLabel.c_str(), m_hoveredElement == 5);
    curY += rowH;

    // Surface Material / Glass Style
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.popup_surface").c_str(), fieldW);
    const wchar_t* surfaceKeys[] = { L"surface.frosted", L"surface.obsidian", L"surface.aurora" };
    int surfIdx = std::clamp((int)m_pendingSettings.popupSurface, 0, 2);
    std::wstring surfStr = Localization::Instance().Get(surfaceKeys[surfIdx]);
    float sfX = ctrlX(fieldW);
    DrawDropdown(rt, D2D1::RectF(sfX, curY, sfX + fwPx, curY + fh), surfStr.c_str(), m_hoveredElement == 30);
    curY += rowH;

    // Corner Radius
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.corner_style").c_str(), fieldW);
    const wchar_t* cornerKeys[] = { L"corner.rounded", L"corner.squircle", L"corner.capsule" };
    int cornIdx = std::clamp((int)m_pendingSettings.popupCorner, 0, 2);
    std::wstring cornStr = Localization::Instance().Get(cornerKeys[cornIdx]);
    float cnX = ctrlX(fieldW);
    DrawDropdown(rt, D2D1::RectF(cnX, curY, cnX + fwPx, curY + fh), cornStr.c_str(), m_hoveredElement == 31);
    curY += rowH;

    // Arabesque Islamic Geometry Overlay
    DrawLabel(rt, x, curY + 4*m_dpiScale, Localization::Instance().Get(L"settings.arabesque").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), curY, m_pendingSettings.arabesqueOverlay, m_hoveredElement == 32);
    curY += rowH;

    // Animations
    DrawLabel(rt, x, curY + 4*m_dpiScale, Localization::Instance().Get(L"settings.animations").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), curY, m_pendingSettings.animationEnabled, m_hoveredElement == 6);
    curY += rowH;

    // Animation Style & Preview (when animations are enabled)
    if (m_pendingSettings.animationEnabled) {
        float animFieldW = 210.f;
        float afwPx = animFieldW * m_dpiScale;
        float prevBtnW = 120.f * m_dpiScale;
        float gapX = 8.f * m_dpiScale;

        DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.animation_style").c_str(), animFieldW + 130.f);

        std::wstring animLabel;
        switch (m_pendingSettings.popupAnimation) {
            case PopupAnimation::SpringBloom:     animLabel = Localization::Instance().Get(L"anim.spring_bloom"); break;
            case PopupAnimation::HorizontalDrift: animLabel = Localization::Instance().Get(L"anim.horizontal_drift"); break;
            case PopupAnimation::LiquidFloat:     animLabel = Localization::Instance().Get(L"anim.liquid_float"); break;
            case PopupAnimation::OrigamiUnfurl:   animLabel = Localization::Instance().Get(L"anim.origami_unfurl"); break;
            case PopupAnimation::EtherealSweep:   animLabel = Localization::Instance().Get(L"anim.ethereal_sweep"); break;
            case PopupAnimation::ZenBreathing:    animLabel = Localization::Instance().Get(L"anim.zen_breathing"); break;
            case PopupAnimation::RippleGenesis:   animLabel = Localization::Instance().Get(L"anim.ripple_genesis"); break;
            case PopupAnimation::Classic:
            default:                              animLabel = Localization::Instance().Get(L"anim.classic"); break;
        }

        if (isRtl) {
            float pX = x;
            DrawButtonWithIcon(rt, D2D1::RectF(pX, curY, pX + prevBtnW, curY + btnH),
                ds::Icon::Play, Localization::Instance().Get(L"settings.preview_anim").c_str(),
                false, m_hoveredElement == 8, m_pressedElement == 8);

            float dX = pX + prevBtnW + gapX;
            DrawDropdown(rt, D2D1::RectF(dX, curY, dX + afwPx, curY + fh), animLabel.c_str(), m_hoveredElement == 7);
        } else {
            float totalW = afwPx + gapX + prevBtnW;
            float startCtrlX = x + w - totalW;
            DrawDropdown(rt, D2D1::RectF(startCtrlX, curY, startCtrlX + afwPx, curY + fh), animLabel.c_str(), m_hoveredElement == 7);

            float pX = startCtrlX + afwPx + gapX;
            DrawButtonWithIcon(rt, D2D1::RectF(pX, curY, pX + prevBtnW, curY + btnH),
                ds::Icon::Play, Localization::Instance().Get(L"settings.preview_anim").c_str(),
                false, m_hoveredElement == 8, m_pressedElement == 8);
        }
        curY += rowH;

        // Motion Pace / Speed
        DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.animation_speed").c_str(), fieldW);
        const wchar_t* speedKeys[] = { L"speed.normal", L"speed.cinematic", L"speed.snappy" };
        int speedIdx = std::clamp((int)m_pendingSettings.animationSpeed, 0, 2);
        std::wstring speedStr = Localization::Instance().Get(speedKeys[speedIdx]);
        float spX = ctrlX(fieldW);
        DrawDropdown(rt, D2D1::RectF(spX, curY, spX + fwPx, curY + fh), speedStr.c_str(), m_hoveredElement == 33);
        curY += rowH;
    }
    curY += 10.f * m_dpiScale;

    // ══════════════════════════════════════════════════════════════════════════
    // Section 3: Typography & Content (الخط ومحتوى الذكر)
    // ══════════════════════════════════════════════════════════════════════════
    DrawSectionHeader(rt, x, curY, w, L"settings.typography_section", ds::Icon::Schedule);

    // Font Size
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.font_size").c_str(), fieldW);
    const wchar_t* fontKeys[] = { L"font.medium", L"font.small", L"font.large" };
    int fontIdx = std::clamp((int)m_pendingSettings.popupFontSize, 0, 2);
    std::wstring fontStr = Localization::Instance().Get(fontKeys[fontIdx]);
    float fnX = ctrlX(fieldW);
    DrawDropdown(rt, D2D1::RectF(fnX, curY, fnX + fwPx, curY + fh), fontStr.c_str(), m_hoveredElement == 40);
    curY += rowH;

    // Show Tashkeel
    DrawLabel(rt, x, curY + 4*m_dpiScale, Localization::Instance().Get(L"settings.show_tashkeel").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), curY, m_pendingSettings.showTashkeel, m_hoveredElement == 41);
    curY += rowH;

    // Show Hadith Source & Virtue
    DrawLabel(rt, x, curY + 4*m_dpiScale, Localization::Instance().Get(L"settings.show_source").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), curY, m_pendingSettings.showSource, m_hoveredElement == 43);
    curY += rowH + 10.f * m_dpiScale;

    // ══════════════════════════════════════════════════════════════════════════
    // Section 4: Sound (الصوت)
    // ══════════════════════════════════════════════════════════════════════════
    DrawSectionHeader(rt, x, curY, w, L"settings.sound", ds::Icon::Volume);

    // Enable Sound
    DrawLabel(rt, x, curY + 4*m_dpiScale, Localization::Instance().Get(L"settings.enable_sound").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), curY, m_pendingSettings.soundEnabled, m_hoveredElement == 11);
    curY += rowH;

    // Sound Melody Preset
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.sound_preset").c_str(), fieldW);
    const wchar_t* soundPresetKeys[] = { L"sound.chime", L"sound.water", L"sound.ambient", L"sound.wood", L"sound.custom" };
    int presetIdx = std::clamp((int)m_pendingSettings.soundPreset, 0, 4);
    std::wstring presetStr = Localization::Instance().Get(soundPresetKeys[presetIdx]);
    float prX = ctrlX(fieldW);
    DrawDropdown(rt, D2D1::RectF(prX, curY, prX + fwPx, curY + fh), presetStr.c_str(), m_hoveredElement == 50);
    curY += rowH;

    // Volume Slider
    float sliderReserve = fieldW + 64.f;
    DrawLabel(rt, x, curY + 4*m_dpiScale, Localization::Instance().Get(L"settings.volume").c_str(), sliderReserve);
    if (isRtl) {
        float pctW = 40.f * m_dpiScale;
        if (m_tfValue) {
            m_brush->SetColor(ds::TextSecondary());
            m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            wchar_t buf[8]; swprintf_s(buf, L"%d%%", m_pendingSettings.soundVolume);
            rt->DrawTextW(buf, (UINT32)wcslen(buf), m_tfValue,
                D2D1::RectF(x, curY, x + pctW, curY + fh), m_brush);
        }
        float sliderX = x + pctW + 8.f * m_dpiScale;
        DrawSlider(rt, sliderX, curY, fwPx, m_pendingSettings.soundVolume / 100.f);
    } else {
        float sliderX = x + w - fwPx;
        DrawSlider(rt, sliderX, curY, fwPx, m_pendingSettings.soundVolume / 100.f);
        if (m_tfValue) {
            m_brush->SetColor(ds::TextSecondary());
            m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            wchar_t buf[8]; swprintf_s(buf, L"%d%%", m_pendingSettings.soundVolume);
            rt->DrawTextW(buf, (UINT32)wcslen(buf), m_tfValue,
                D2D1::RectF(sliderX - 48.f*m_dpiScale, curY, sliderX - 8.f*m_dpiScale, curY + fh), m_brush);
        }
    }
    curY += rowH;

    // Custom Audio File (only when Custom preset is selected)
    if (m_pendingSettings.soundPreset == SoundPreset::Custom) {
        std::wstring soundName = m_pendingSettings.customSoundPath.empty() ?
            Localization::Instance().Get(L"settings.sound_default") :
            std::filesystem::path(m_pendingSettings.customSoundPath).filename().wstring();

        float soundControlReserve = 340.f;
        DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.notification_sound").c_str(), soundControlReserve);
        float browseW = 150.f * m_dpiScale;
        float resetW = 36.f * m_dpiScale;

        if (isRtl) {
            float cX = x;
            std::wstring browseStr = Localization::Instance().Get(L"settings.choose_sound");
            DrawButton(rt, D2D1::RectF(cX, curY, cX + browseW, curY + btnH), browseStr.c_str(), false, m_hoveredElement == 13, m_pressedElement == 13);
            cX += browseW + 8.f * m_dpiScale;

            if (!m_pendingSettings.customSoundPath.empty()) {
                DrawIconButton(rt, D2D1::RectF(cX, curY, cX + resetW, curY + btnH), ds::Icon::Refresh, m_hoveredElement == 14, m_pressedElement == 14);
                cX += resetW + 8.f * m_dpiScale;
            }

            if (m_tfValue) {
                m_brush->SetColor(ds::TextSecondary());
                m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                rt->DrawTextW(soundName.c_str(), (UINT32)soundName.size(), m_tfValue,
                    D2D1::RectF(cX, curY + 6*m_dpiScale, x + w, curY + btnH), m_brush);
            }
        } else {
            float cX = x + w;
            if (!m_pendingSettings.customSoundPath.empty()) {
                cX -= resetW;
                DrawIconButton(rt, D2D1::RectF(cX, curY, cX + resetW, curY + btnH), ds::Icon::Refresh, m_hoveredElement == 14, m_pressedElement == 14);
                cX -= 8.f * m_dpiScale;
            }

            cX -= browseW;
            std::wstring browseStr = Localization::Instance().Get(L"settings.choose_sound");
            DrawButton(rt, D2D1::RectF(cX, curY, cX + browseW, curY + btnH), browseStr.c_str(), false, m_hoveredElement == 13, m_pressedElement == 13);
            cX -= 12.f * m_dpiScale;

            if (m_tfValue) {
                m_brush->SetColor(ds::TextSecondary());
                m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                rt->DrawTextW(soundName.c_str(), (UINT32)soundName.size(), m_tfValue,
                    D2D1::RectF(x, curY + 6*m_dpiScale, cX, curY + btnH), m_brush);
            }
        }
        curY += rowH + 6*m_dpiScale;
    }

    // Test Sound Button
    float testBtnW = 140.f;
    float bX = ctrlX(testBtnW);
    DrawButtonWithIcon(rt, D2D1::RectF(bX, curY, bX + testBtnW*m_dpiScale, curY + btnH),
        ds::Icon::Play, Localization::Instance().Get(L"settings.test_sound").c_str(),
        false, m_hoveredElement == 15, m_pressedElement == 15);
    curY += rowH + 10.f * m_dpiScale;

    // ══════════════════════════════════════════════════════════════════════════
    // Section 4: System (النظام)
    // ══════════════════════════════════════════════════════════════════════════
    DrawSectionHeader(rt, x, curY, w, L"settings.system", ds::Icon::System);

    // Full-screen distraction suppression toggle
    DrawLabel(rt, x, curY + 4*m_dpiScale, Localization::Instance().Get(L"settings.fullscreen_suppression").c_str(), togW);
    DrawToggle(rt, ctrlX(togW), curY, m_pendingSettings.suppressDuringFullscreen, m_hoveredElement == 21);
    curY += rowH;

    // Full-screen behavior dropdown (when enabled)
    if (m_pendingSettings.suppressDuringFullscreen) {
        float fsw = 240.f;
        float fswPx = fsw * m_dpiScale;
        DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"settings.fullscreen_behavior").c_str(), fsw);
        const wchar_t* modeKeys[] = { L"settings.fullscreen_queue", L"settings.fullscreen_suppress", L"settings.fullscreen_win_notify" };
        int modeIdx = std::clamp(m_pendingSettings.fullscreenSuppressionMode, 0, 2);
        std::wstring modeStr = Localization::Instance().Get(modeKeys[modeIdx]);
        float ffsX = ctrlX(fsw);
        DrawDropdown(rt, D2D1::RectF(ffsX, curY, ffsX + fswPx, curY + fh), modeStr.c_str(), m_hoveredElement == 22);
        curY += rowH;
    }

    // Version label + value
    DrawLabel(rt, x, curY + 6*m_dpiScale, Localization::Instance().Get(L"about.version").c_str(), 100.f);
    if (m_tfValue) {
        m_brush->SetColor(ds::TextSecondary());
        float vX = ctrlX(100.f);
        rt->DrawTextW(ApplicationMetadata::AppVersion, (UINT32)wcslen(ApplicationMetadata::AppVersion),
            m_tfValue, D2D1::RectF(vX, curY + 6*m_dpiScale, vX + 100.f*m_dpiScale, curY + rowH), m_brush);
    }
    curY += rowH;

    // Check for Updates button
    float sysBtnW = 180.f;
    float sbwPx = sysBtnW * m_dpiScale;
    float ubX = ctrlX(sysBtnW);
    std::wstring updateStr = Localization::Instance().Get(L"settings.check_updates");
    DrawButton(rt, D2D1::RectF(ubX, curY, ubX + sbwPx, curY + btnH), updateStr.c_str(), false, m_hoveredElement == 23, m_pressedElement == 23);
    curY += rowH;

    // Reset settings button (red danger)
    float resetX = ctrlX(sysBtnW);
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    bool hoverReset   = (m_hoveredElement == 24);
    bool pressedReset = (m_pressedElement == 24);
    m_brush->SetColor(pressedReset ? ds::Rgba(0xC0,0x40,0x40,0.25f) : (hoverReset ? ds::Rgba(0xC0,0x40,0x40,0.15f) : ds::Surface()));
    D2D1_RECT_F rr = D2D1::RectF(resetX, curY, resetX + sbwPx, curY + btnH);
    rt->FillRoundedRectangle(D2D1::RoundedRect(rr, rx, rx), m_brush);
    m_brush->SetColor(ds::Rgba(0xC0,0x40,0x40,0.8f));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(rr, rx, rx), m_brush, 1.f);
    if (m_tfBtn) {
        m_brush->SetColor(ds::Rgba(0xE8,0x40,0x40));
        std::wstring resetStr = Localization::Instance().Get(L"settings.reset_all");
        rt->DrawTextW(resetStr.c_str(), (UINT32)resetStr.size(), m_tfBtn, rr, m_brush);
    }
    curY += rowH + 24.f * m_dpiScale;

    // Calculate maximum scroll
    float totalContentH = (curY + m_scrollY) - (scrollAreaTop + 8.f * m_dpiScale);
    m_maxScroll = std::max(0.f, totalContentH - m_viewportH + 20.f * m_dpiScale);

    rt->PopAxisAlignedClip();

    // ── 3. Scrollbar Indicator ───────────────────────────────────────────────
    if (m_maxScroll > 0.f) {
        float sbW = 4.f * m_dpiScale;
        float sbX = isRtl ? (bounds.left + 4.f * m_dpiScale) : (bounds.right - 8.f * m_dpiScale);
        float trackH = m_viewportH;
        float thumbH = std::max(32.f * m_dpiScale, trackH * (m_viewportH / (m_viewportH + m_maxScroll)));
        float thumbY = scrollAreaTop + (trackH - thumbH) * (m_scrollY / m_maxScroll);

        m_brush->SetColor(ds::Rgba(255, 255, 255, 0.08f));
        rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(sbX, scrollAreaTop, sbX + sbW, scrollAreaTop + trackH), 2.f, 2.f), m_brush);

        m_brush->SetColor(ds::Accent::Primary(0.5f));
        rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(sbX, thumbY, sbX + sbW, thumbY + thumbH), 2.f, 2.f), m_brush);
    }

    // ── 4. Unsaved Changes Save Bar (Floating Bottom Bar) ─────────────────────
    if (m_hasUnsavedChanges) {
        float barY = bounds.bottom - pad - saveBarH;
        D2D1_RECT_F barR = D2D1::RectF(x, barY, x + w, barY + 48.f * m_dpiScale);
        float barRx = ds::Dims::CardRadius * m_dpiScale;

        // Floating bar background & border
        m_brush->SetColor(ds::Dark::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(barR, barRx, barRx), m_brush);
        m_brush->SetColor(ds::BorderHighlight());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(barR, barRx, barRx), m_brush, 1.2f);

        // Notice text
        if (m_tfLabel) {
            m_brush->SetColor(ds::TextPrimary());
            std::wstring notice = isRtl ? L"لديك تعديلات غير محفوظة" : L"You have unsaved changes";
            D2D1_RECT_F textR = isRtl
                ? D2D1::RectF(barR.left + 170.f*m_dpiScale, barR.top, barR.right - 16.f*m_dpiScale, barR.bottom)
                : D2D1::RectF(barR.left + 16.f*m_dpiScale, barR.top, barR.right - 170.f*m_dpiScale, barR.bottom);
            m_tfLabel->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            rt->DrawTextW(notice.c_str(), (UINT32)notice.size(), m_tfLabel, textR, m_brush);
        }

        // Save button
        float sbW = 150.f * m_dpiScale;
        float sbH = ds::Dims::BtnH * m_dpiScale;
        float sbX = isRtl ? (barR.left + 8.f*m_dpiScale) : (barR.right - sbW - 8.f*m_dpiScale);
        float sbY = barR.top + (barR.bottom - barR.top - sbH) / 2.f;
        std::wstring saveStr = Localization::Instance().Get(L"reminders.save_star");
        DrawButton(rt, D2D1::RectF(sbX, sbY, sbX + sbW, sbY + sbH), saveStr.c_str(), true, m_hoveredSave, m_pressedElement == 99);
    }
}

bool SettingsPage::OnMouseWheel(int delta) {
    if (m_maxScroll <= 0.f) return false;
    float scrollStep = (delta / 120.f) * (72.f * m_dpiScale);
    m_targetScrollY = std::clamp(m_targetScrollY - scrollStep, 0.f, m_maxScroll);
    float diff = m_targetScrollY - m_scrollY;
    if (std::abs(diff) < 0.4f) {
        m_scrollY = m_targetScrollY;
    } else {
        m_scrollY += diff * 0.25f;
    }
    return true;
}

bool SettingsPage::OnScrollTick() {
    m_targetScrollY = std::clamp(m_targetScrollY, 0.f, m_maxScroll);
    float diff = m_targetScrollY - m_scrollY;
    if (std::abs(diff) < 0.4f) {
        if (m_scrollY != m_targetScrollY) {
            m_scrollY = m_targetScrollY;
            return true;
        }
        return false;
    }
    m_scrollY += diff * 0.24f;
    return true;
}

bool SettingsPage::OnLButtonDown(int x, int y) {
    float pad = ds::Space::XL * m_dpiScale;
    float saveBarH = m_hasUnsavedChanges ? (56.f * m_dpiScale) : 0.f;
    bool isRtl = Localization::Instance().IsRTL();
    RECT rc; GetClientRect(Application::Instance().MainWindow(), &rc);

    // 1. Hit test Save Button in floating bar
    if (m_hasUnsavedChanges) {
        float barY = (float)rc.bottom - pad - saveBarH;
        D2D1_RECT_F barR = D2D1::RectF(m_contentX, barY, m_contentX + m_contentW, barY + 48.f * m_dpiScale);
        float sbW = 150.f * m_dpiScale;
        float sbH = ds::Dims::BtnH * m_dpiScale;
        float sbX = isRtl ? (barR.left + 8.f*m_dpiScale) : (barR.right - sbW - 8.f*m_dpiScale);
        float sbY = barR.top + (barR.bottom - barR.top - sbH) / 2.f;

        if (x >= sbX && x <= sbX + sbW && y >= sbY && y <= sbY + sbH) {
            m_pressedElement = 99;
            bool languageChanged = (Settings::Instance().GetCopy().language != m_pendingSettings.language);

            Settings::Instance().UpdatePreferences(m_pendingSettings);
            Settings::Instance().Save();
            m_hasUnsavedChanges = false;

            if (languageChanged) {
                Localization::Instance().SetLanguage(m_pendingSettings.language);
                MainWindow::Instance().OnSettingsChanged();
            }

            MainWindow::Instance().Invalidate();
            Scheduler::Instance().Recalculate();
            LOG_INFO("Settings saved successfully (Single Page View)");
            return true;
        }
    }

    // 2. Check if inside scrollable content area
    if (y < m_contentY || y > m_contentY + m_viewportH) {
        return false;
    }

    // Virtual Y including scroll offset
    float vy = y + m_scrollY;
    float startY = m_contentY + 8.f * m_dpiScale;
    float rowH = 46.f * m_dpiScale;
    float fh   = ds::Dims::InputH * m_dpiScale;
    float btnH = ds::Dims::BtnH * m_dpiScale;
    float fieldW = 200.f;
    float fwPx   = fieldW * m_dpiScale;
    float togW   = ds::Dims::ToggleW;

    auto ctrlLeft = [&](float ctrlW) -> float {
        return isRtl ? m_contentX : (m_contentX + m_contentW - ctrlW * m_dpiScale);
    };

    float curY = startY;

    // ── Section 1: General ──
    curY += 14.f * m_dpiScale + 32.f * m_dpiScale; // Header height

    // Language
    float langX = ctrlLeft(fieldW);
    if (x >= langX && x <= langX + fwPx && vy >= curY && vy <= curY + fh) {
        float ctrlVisualY = curY - m_scrollY;
        POINT pt{ (LONG)langX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);

        std::vector<DropdownItem> items = {
            { 1, L"System Default", m_pendingSettings.language == L"system" },
            { 2, L"English", m_pendingSettings.language == L"en" },
            { 3, L"العربية", m_pendingSettings.language == L"ar" }
        };

        DropdownPopup::Instance().Show(
            Application::Instance().MainWindow(),
            pt.x, pt.y, fieldW, items,
            [this](int chosenId) {
                if (chosenId == 1) m_pendingSettings.language = L"system";
                else if (chosenId == 2) m_pendingSettings.language = L"en";
                else if (chosenId == 3) m_pendingSettings.language = L"ar";
                CommitSettings(true);
            });
        return true;
    }
    curY += rowH;

    // Color Theme hit testing
    float themeW = 236.f;
    float thStep = 34.f * m_dpiScale;
    float thStartX = ctrlLeft(themeW) + 18.f * m_dpiScale;
    float thCy = curY + fh / 2.f;
    for (int i = 0; i < 6; ++i) {
        float cx = thStartX + i * thStep;
        float r = 16.f * m_dpiScale;
        if (x >= cx - r && x <= cx + r && vy >= thCy - r && vy <= thCy + r) {
            ColorTheme newTheme = (ColorTheme)i;
            if (m_pendingSettings.colorTheme != newTheme) {
                m_pendingSettings.colorTheme = newTheme;
                MainWindow::Instance().TransitionToTheme(newTheme);
            }
            return true;
        }
    }
    curY += rowH;

    // Start with Windows
    float swX = ctrlLeft(togW);
    if (x >= swX && x <= swX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) {
        m_pendingSettings.startWithWindows = !m_pendingSettings.startWithWindows;
        Application::Instance().SetStartWithWindows(m_pendingSettings.startWithWindows);
        CommitSettings(false);
        return true;
    }
    curY += rowH + 10.f * m_dpiScale;

    // ── Section 2: Popup & Display ──
    curY += 14.f * m_dpiScale + 32.f * m_dpiScale; // Header height

    // Multi-Monitor Target Dropdown
    float monFieldW = 240.f;
    float mfwPx = monFieldW * m_dpiScale;
    float mfX = ctrlLeft(monFieldW);
    if (x >= mfX && x <= mfX + mfwPx && vy >= curY && vy <= curY + fh) {
        float ctrlVisualY = curY - m_scrollY;
        POINT pt{ (LONG)mfX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);

        auto monitors = PopupWindow::EnumerateMonitors();
        std::vector<DropdownItem> items;
        // Primary Monitor (Default) is always first and the standard option!
        items.push_back({ 1, Localization::Instance().Get(L"settings.monitor_primary"), m_pendingSettings.popupMonitor == PopupMonitor::Primary });
        items.push_back({ 0, Localization::Instance().Get(L"settings.monitor_all"), m_pendingSettings.popupMonitor == PopupMonitor::All });
        items.push_back({ 2, Localization::Instance().Get(L"settings.monitor_active"), m_pendingSettings.popupMonitor == PopupMonitor::Active });

        for (size_t i = 0; i < monitors.size(); ++i) {
            std::wstring mLabel = Localization::Instance().Get(L"settings.monitor_num") + L" " + std::to_wstring(i + 1);
            if (monitors[i].isPrimary) mLabel += L" (" + Localization::Instance().Get(L"settings.monitor_primary") + L")";
            bool sel = (m_pendingSettings.popupMonitor == PopupMonitor::Specific && m_pendingSettings.specificMonitor == (int)i);
            items.push_back({ 10 + (int)i, mLabel, sel });
        }

        DropdownPopup::Instance().Show(
            Application::Instance().MainWindow(),
            pt.x, pt.y, monFieldW, items,
            [this](int chosenId) {
                if (chosenId == 1) m_pendingSettings.popupMonitor = PopupMonitor::Primary;
                else if (chosenId == 0) m_pendingSettings.popupMonitor = PopupMonitor::All;
                else if (chosenId == 2) m_pendingSettings.popupMonitor = PopupMonitor::Active;
                else if (chosenId >= 10) {
                    m_pendingSettings.popupMonitor = PopupMonitor::Specific;
                    m_pendingSettings.specificMonitor = chosenId - 10;
                }
                CommitSettings(false);
            });
        return true;
    }
    curY += rowH;

    // Position Picker Grid
    float gridX = ctrlLeft(100.f);
    float dotSize = 18.f * m_dpiScale;
    float cellSize = dotSize + 8.f * m_dpiScale;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            float dcx = gridX + c * cellSize + dotSize / 2.f;
            float dcy = curY + r * cellSize + dotSize / 2.f;
            if (x >= dcx - 12*m_dpiScale && x <= dcx + 12*m_dpiScale && vy >= dcy - 12*m_dpiScale && vy <= dcy + 12*m_dpiScale) {
                m_pendingSettings.popupPosition = (PopupPosition)(r * 3 + c);
                CommitSettings(false);
                return true;
            }
        }
    }
    curY += rowH + 28*m_dpiScale;

    // Duration Dropdown
    float durX = ctrlLeft(fieldW);
    if (x >= durX && x <= durX + fwPx && vy >= curY && vy <= curY + fh) {
        float ctrlVisualY = curY - m_scrollY;
        POINT pt{ (LONG)durX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);

        std::vector<DropdownItem> items;
        int durations[] = { 2, 3, 5, 7, 10, 15 };
        for (int d : durations) {
            std::wstring label = std::to_wstring(d) + (isRtl ? L" ثوانٍ" : L"s");
            items.push_back({ d, label, m_pendingSettings.popupDurationSec == d });
        }

        DropdownPopup::Instance().Show(
            Application::Instance().MainWindow(),
            pt.x, pt.y, fieldW, items,
            [this](int chosenDuration) {
                m_pendingSettings.popupDurationSec = chosenDuration;
                CommitSettings(false);
            });
        return true;
    }
    curY += rowH;

    // Surface Material Dropdown
    float surfX = ctrlLeft(fieldW);
    if (x >= surfX && x <= surfX + fwPx && vy >= curY && vy <= curY + fh) {
        float ctrlVisualY = curY - m_scrollY;
        POINT pt{ (LONG)surfX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);

        std::vector<DropdownItem> items = {
            { 0, Localization::Instance().Get(L"surface.frosted"),  m_pendingSettings.popupSurface == PopupSurfaceStyle::FrostedAcrylic },
            { 1, Localization::Instance().Get(L"surface.obsidian"), m_pendingSettings.popupSurface == PopupSurfaceStyle::DeepObsidian },
            { 2, Localization::Instance().Get(L"surface.aurora"),   m_pendingSettings.popupSurface == PopupSurfaceStyle::AuroraGlow }
        };

        DropdownPopup::Instance().Show(
            Application::Instance().MainWindow(),
            pt.x, pt.y, fieldW, items,
            [this](int chosen) {
                m_pendingSettings.popupSurface = (PopupSurfaceStyle)chosen;
                CommitSettings(false);
            });
        return true;
    }
    curY += rowH;

    // Corner Radius Dropdown
    float cornX = ctrlLeft(fieldW);
    if (x >= cornX && x <= cornX + fwPx && vy >= curY && vy <= curY + fh) {
        float ctrlVisualY = curY - m_scrollY;
        POINT pt{ (LONG)cornX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);

        std::vector<DropdownItem> items = {
            { 0, Localization::Instance().Get(L"corner.rounded"),  m_pendingSettings.popupCorner == PopupCornerStyle::Rounded },
            { 1, Localization::Instance().Get(L"corner.squircle"), m_pendingSettings.popupCorner == PopupCornerStyle::Squircle },
            { 2, Localization::Instance().Get(L"corner.capsule"),  m_pendingSettings.popupCorner == PopupCornerStyle::Capsule }
        };

        DropdownPopup::Instance().Show(
            Application::Instance().MainWindow(),
            pt.x, pt.y, fieldW, items,
            [this](int chosen) {
                m_pendingSettings.popupCorner = (PopupCornerStyle)chosen;
                CommitSettings(false);
            });
        return true;
    }
    curY += rowH;

    // Arabesque Islamic Geometry Overlay Toggle
    float araX = ctrlLeft(togW);
    if (x >= araX && x <= araX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) {
        m_pendingSettings.arabesqueOverlay = !m_pendingSettings.arabesqueOverlay;
        CommitSettings(false);
        return true;
    }
    curY += rowH;

    // Animation Toggle
    float animX = ctrlLeft(togW);
    if (x >= animX && x <= animX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) {
        m_pendingSettings.animationEnabled = !m_pendingSettings.animationEnabled;
        CommitSettings(false);
        return true;
    }
    curY += rowH;

    // Animation Style & Preview (when animations are enabled)
    if (m_pendingSettings.animationEnabled) {
        float animFieldW = 210.f;
        float afwPx = animFieldW * m_dpiScale;
        float prevBtnW = 120.f * m_dpiScale;
        float gapX = 8.f * m_dpiScale;

        float dropX, prevX;
        if (isRtl) {
            prevX = m_contentX;
            dropX = prevX + prevBtnW + gapX;
        } else {
            float totalW = afwPx + gapX + prevBtnW;
            dropX = m_contentX + m_contentW - totalW;
            prevX = dropX + afwPx + gapX;
        }

        // Dropdown hit test
        if (x >= dropX && x <= dropX + afwPx && vy >= curY && vy <= curY + fh) {
            float ctrlVisualY = curY - m_scrollY;
            POINT pt{ (LONG)dropX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
            ClientToScreen(Application::Instance().MainWindow(), &pt);

            std::vector<DropdownItem> items = {
                { 0, Localization::Instance().Get(L"anim.classic"),          m_pendingSettings.popupAnimation == PopupAnimation::Classic },
                { 1, Localization::Instance().Get(L"anim.spring_bloom"),     m_pendingSettings.popupAnimation == PopupAnimation::SpringBloom },
                { 2, Localization::Instance().Get(L"anim.horizontal_drift"), m_pendingSettings.popupAnimation == PopupAnimation::HorizontalDrift },
                { 3, Localization::Instance().Get(L"anim.liquid_float"),     m_pendingSettings.popupAnimation == PopupAnimation::LiquidFloat },
                { 4, Localization::Instance().Get(L"anim.origami_unfurl"),   m_pendingSettings.popupAnimation == PopupAnimation::OrigamiUnfurl },
                { 5, Localization::Instance().Get(L"anim.ethereal_sweep"),   m_pendingSettings.popupAnimation == PopupAnimation::EtherealSweep },
                { 6, Localization::Instance().Get(L"anim.zen_breathing"),    m_pendingSettings.popupAnimation == PopupAnimation::ZenBreathing },
                { 7, Localization::Instance().Get(L"anim.ripple_genesis"),   m_pendingSettings.popupAnimation == PopupAnimation::RippleGenesis }
            };

            DropdownPopup::Instance().Show(
                Application::Instance().MainWindow(),
                pt.x, pt.y, animFieldW, items,
                [this](int chosenStyle) {
                    m_pendingSettings.popupAnimation = (PopupAnimation)chosenStyle;
                    CommitSettings(false);
                    // Live preview immediately upon choosing!
                    Scheduler::Instance().TriggerNow();
                });
            return true;
        }

        // Preview button hit test
        if (x >= prevX && x <= prevX + prevBtnW && vy >= curY && vy <= curY + btnH) {
            m_pressedElement = 8;
            Scheduler::Instance().TriggerNow();
            return true;
        }

        curY += rowH;

        // Motion Pace / Speed Dropdown
        float spX = ctrlLeft(fieldW);
        if (x >= spX && x <= spX + fwPx && vy >= curY && vy <= curY + fh) {
            float ctrlVisualY = curY - m_scrollY;
            POINT pt{ (LONG)spX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
            ClientToScreen(Application::Instance().MainWindow(), &pt);

            std::vector<DropdownItem> items = {
                { 0, Localization::Instance().Get(L"speed.normal"),    m_pendingSettings.animationSpeed == AnimationSpeed::Normal },
                { 1, Localization::Instance().Get(L"speed.cinematic"), m_pendingSettings.animationSpeed == AnimationSpeed::Cinematic },
                { 2, Localization::Instance().Get(L"speed.snappy"),    m_pendingSettings.animationSpeed == AnimationSpeed::Snappy }
            };

            DropdownPopup::Instance().Show(
                Application::Instance().MainWindow(),
                pt.x, pt.y, fieldW, items,
                [this](int chosenSpeed) {
                    m_pendingSettings.animationSpeed = (AnimationSpeed)chosenSpeed;
                    CommitSettings(false);
                });
            return true;
        }
        curY += rowH;
    }
    curY += 10.f * m_dpiScale;

    // ── Section 3: Typography & Content ──
    curY += 14.f * m_dpiScale + 32.f * m_dpiScale; // Header height

    // Font Size Dropdown
    float fnX = ctrlLeft(fieldW);
    if (x >= fnX && x <= fnX + fwPx && vy >= curY && vy <= curY + fh) {
        float ctrlVisualY = curY - m_scrollY;
        POINT pt{ (LONG)fnX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);

        std::vector<DropdownItem> items = {
            { 0, Localization::Instance().Get(L"font.medium"), m_pendingSettings.popupFontSize == PopupFontSize::Medium },
            { 1, Localization::Instance().Get(L"font.small"),  m_pendingSettings.popupFontSize == PopupFontSize::Small },
            { 2, Localization::Instance().Get(L"font.large"),  m_pendingSettings.popupFontSize == PopupFontSize::Large }
        };

        DropdownPopup::Instance().Show(
            Application::Instance().MainWindow(),
            pt.x, pt.y, fieldW, items,
            [this](int chosenFont) {
                m_pendingSettings.popupFontSize = (PopupFontSize)chosenFont;
                CommitSettings(false);
            });
        return true;
    }
    curY += rowH;

    // Show Tashkeel Toggle
    float tskX = ctrlLeft(togW);
    if (x >= tskX && x <= tskX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) {
        m_pendingSettings.showTashkeel = !m_pendingSettings.showTashkeel;
        CommitSettings(false);
        return true;
    }
    curY += rowH;

    // Show Hadith Source & Virtue Toggle
    float srcX = ctrlLeft(togW);
    if (x >= srcX && x <= srcX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) {
        m_pendingSettings.showSource = !m_pendingSettings.showSource;
        CommitSettings(false);
        return true;
    }
    curY += rowH + 10.f * m_dpiScale;

    // ── Section 4: Sound ──
    curY += 14.f * m_dpiScale + 32.f * m_dpiScale; // Header height

    // Enable Sound Toggle
    float sndX = ctrlLeft(togW);
    if (x >= sndX && x <= sndX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) {
        m_pendingSettings.soundEnabled = !m_pendingSettings.soundEnabled;
        CommitSettings(false);
        return true;
    }
    curY += rowH;

    // Sound Melody Preset Dropdown
    float prX = ctrlLeft(fieldW);
    if (x >= prX && x <= prX + fwPx && vy >= curY && vy <= curY + fh) {
        float ctrlVisualY = curY - m_scrollY;
        POINT pt{ (LONG)prX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
        ClientToScreen(Application::Instance().MainWindow(), &pt);

        std::vector<DropdownItem> items = {
            { 0, Localization::Instance().Get(L"sound.chime"),   m_pendingSettings.soundPreset == SoundPreset::Chime },
            { 1, Localization::Instance().Get(L"sound.water"),   m_pendingSettings.soundPreset == SoundPreset::WaterDrop },
            { 2, Localization::Instance().Get(L"sound.ambient"), m_pendingSettings.soundPreset == SoundPreset::AmbientEcho },
            { 3, Localization::Instance().Get(L"sound.wood"),    m_pendingSettings.soundPreset == SoundPreset::WoodTap },
            { 4, Localization::Instance().Get(L"sound.custom"),  m_pendingSettings.soundPreset == SoundPreset::Custom }
        };

        DropdownPopup::Instance().Show(
            Application::Instance().MainWindow(),
            pt.x, pt.y, fieldW, items,
            [this](int chosenPreset) {
                m_pendingSettings.soundPreset = (SoundPreset)chosenPreset;
                CommitSettings(false);
                AudioEngine::Instance().ReloadSound();
                AudioEngine::Instance().PlayReminder();
            });
        return true;
    }
    curY += rowH;

    // Volume Slider
    float sliderX = isRtl ? (m_contentX + 40.f * m_dpiScale + 8.f * m_dpiScale) : ctrlLeft(fieldW);
    if (x >= sliderX && x <= sliderX + fwPx && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) {
        m_isDraggingVolume = true;
        float t = (x - sliderX) / fwPx;
        m_pendingSettings.soundVolume = std::clamp((int)(t * 100), 0, 100);
        MainWindow::Instance().Invalidate();
        return true;
    }
    curY += rowH;

    // Sound File Browse / Reset (only when Custom preset is selected)
    if (m_pendingSettings.soundPreset == SoundPreset::Custom) {
        float browseW = 150.f * m_dpiScale;
        float resetW  = 36.f * m_dpiScale;
        if (isRtl) {
            float cX = m_contentX;
            if (x >= cX && x <= cX + browseW && vy >= curY && vy <= curY + btnH) {
                m_pressedElement = 13;
                wchar_t szFile[MAX_PATH] = { 0 };
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = Application::Instance().MainWindow();
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Audio Files (*.mp3;*.wav;*.wma;*.m4a)\0*.mp3;*.wav;*.wma;*.m4a\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameW(&ofn)) {
                    m_pendingSettings.customSoundPath = szFile;
                    CommitSettings(false);
                    AudioEngine::Instance().ReloadSound();
                    AudioEngine::Instance().PlayReminder();
                }
                return true;
            }
            cX += browseW + 8.f * m_dpiScale;
            if (!m_pendingSettings.customSoundPath.empty()) {
                if (x >= cX && x <= cX + resetW && vy >= curY && vy <= curY + btnH) {
                    m_pressedElement = 14;
                    m_pendingSettings.customSoundPath = L"";
                    CommitSettings(false);
                    AudioEngine::Instance().ReloadSound();
                    AudioEngine::Instance().PlayReminder();
                    return true;
                }
            }
        } else {
            float cX = m_contentX + m_contentW;
            if (!m_pendingSettings.customSoundPath.empty()) {
                cX -= resetW;
                if (x >= cX && x <= cX + resetW && vy >= curY && vy <= curY + btnH) {
                    m_pressedElement = 14;
                    m_pendingSettings.customSoundPath = L"";
                    CommitSettings(false);
                    AudioEngine::Instance().ReloadSound();
                    AudioEngine::Instance().PlayReminder();
                    return true;
                }
                cX -= 8.f * m_dpiScale;
            }
            cX -= browseW;
            if (x >= cX && x <= cX + browseW && vy >= curY && vy <= curY + btnH) {
                m_pressedElement = 13;
                wchar_t szFile[MAX_PATH] = { 0 };
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = Application::Instance().MainWindow();
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Audio Files (*.mp3;*.wav;*.wma;*.m4a)\0*.mp3;*.wav;*.wma;*.m4a\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameW(&ofn)) {
                    m_pendingSettings.customSoundPath = szFile;
                    CommitSettings(false);
                    AudioEngine::Instance().ReloadSound();
                    AudioEngine::Instance().PlayReminder();
                }
                return true;
            }
        }
        curY += rowH + 6*m_dpiScale;
    }

    // Test Sound Button
    float testBtnW = 140.f;
    float tbX = ctrlLeft(testBtnW);
    if (x >= tbX && x <= tbX + testBtnW * m_dpiScale && vy >= curY && vy <= curY + btnH) {
        m_pressedElement = 15;
        AudioEngine::Instance().PlayReminder((float)m_pendingSettings.soundVolume / 100.0f);
        MainWindow::Instance().Invalidate();
        return true;
    }
    curY += rowH + 10.f * m_dpiScale;

    // ── Section 4: System ──
    curY += 14.f * m_dpiScale + 32.f * m_dpiScale; // Header height

    // Fullscreen Suppression Toggle
    float fsX = ctrlLeft(togW);
    if (x >= fsX && x <= fsX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) {
        m_pendingSettings.suppressDuringFullscreen = !m_pendingSettings.suppressDuringFullscreen;
        CommitSettings(false);
        return true;
    }
    curY += rowH;

    // Fullscreen Behavior Dropdown
    if (m_pendingSettings.suppressDuringFullscreen) {
        float fsw = 240.f;
        float fswPx = fsw * m_dpiScale;
        float ffsX = ctrlLeft(fsw);
        if (x >= ffsX && x <= ffsX + fswPx && vy >= curY && vy <= curY + fh) {
            float ctrlVisualY = curY - m_scrollY;
            POINT pt{ (LONG)ffsX, (LONG)(ctrlVisualY + fh + 2.f * m_dpiScale) };
            ClientToScreen(Application::Instance().MainWindow(), &pt);

            std::vector<DropdownItem> items = {
                { 0, Localization::Instance().Get(L"settings.fullscreen_queue"), m_pendingSettings.fullscreenSuppressionMode == 0 },
                { 1, Localization::Instance().Get(L"settings.fullscreen_suppress"), m_pendingSettings.fullscreenSuppressionMode == 1 },
                { 2, Localization::Instance().Get(L"settings.fullscreen_win_notify"), m_pendingSettings.fullscreenSuppressionMode == 2 }
            };

            DropdownPopup::Instance().Show(
                Application::Instance().MainWindow(),
                pt.x, pt.y, fsw, items,
                [this](int mode) {
                    m_pendingSettings.fullscreenSuppressionMode = mode;
                    CommitSettings(false);
                });
            return true;
        }
        curY += rowH;
    }

    curY += rowH; // Skip version row

    // Check for Updates
    float sysBtnW = 180.f;
    float sbwPx = sysBtnW * m_dpiScale;
    float ubX = ctrlLeft(sysBtnW);
    if (x >= ubX && x <= ubX + sbwPx && vy >= curY && vy <= curY + btnH) {
        m_pressedElement = 23;
        Updater::Instance().CheckNow(Application::Instance().MainWindow());
        MainWindow::Instance().Invalidate();
        return true;
    }
    curY += rowH;

    // Reset All Settings Button
    float rbX = ctrlLeft(sysBtnW);
    if (x >= rbX && x <= rbX + sbwPx && vy >= curY && vy <= curY + btnH) {
        m_pressedElement = 24;
        std::wstring title = Localization::Instance().Get(L"settings.reset_confirm_title");
        std::wstring msg = Localization::Instance().Get(L"settings.reset_confirm_msg");
        int res = MessageBoxW(Application::Instance().MainWindow(), msg.c_str(), title.c_str(), MB_YESNO | MB_ICONWARNING);
        if (res == IDYES) {
            AppSettings def;
            def.language = Settings::Instance().GetCopy().language; // preserve language
            Settings::Instance().UpdatePreferences(def);
            Settings::Instance().Save();
            m_pendingSettings = def;
            m_hasUnsavedChanges = false;
            MainWindow::Instance().OnSettingsChanged();
            Scheduler::Instance().Recalculate();
            MainWindow::Instance().Invalidate();
        }
        return true;
    }

    return false;
}

bool SettingsPage::OnLButtonUp(int x, int y) {
    m_pressedElement = -1;
    if (m_isDraggingVolume) {
        m_isDraggingVolume = false;
        CommitSettings(false);
        AudioEngine::Instance().PlayReminder((float)m_pendingSettings.soundVolume / 100.0f);
    }
    return true;
}

bool SettingsPage::OnMouseMove(int x, int y) {
    int oldHover = m_hoveredElement;
    bool oldSave = m_hoveredSave;

    float pad = ds::Space::XL * m_dpiScale;
    float saveBarH = m_hasUnsavedChanges ? (56.f * m_dpiScale) : 0.f;
    bool isRtl = Localization::Instance().IsRTL();
    RECT rc; GetClientRect(Application::Instance().MainWindow(), &rc);

    // Save button hover
    m_hoveredSave = false;
    if (m_hasUnsavedChanges) {
        float barY = (float)rc.bottom - pad - saveBarH;
        D2D1_RECT_F barR = D2D1::RectF(m_contentX, barY, m_contentX + m_contentW, barY + 48.f * m_dpiScale);
        float sbW = 150.f * m_dpiScale;
        float sbH = ds::Dims::BtnH * m_dpiScale;
        float sbX = isRtl ? (barR.left + 8.f*m_dpiScale) : (barR.right - sbW - 8.f*m_dpiScale);
        float sbY = barR.top + (barR.bottom - barR.top - sbH) / 2.f;

        if (x >= sbX && x <= sbX + sbW && y >= sbY && y <= sbY + sbH) {
            m_hoveredSave = true;
        }
    }

    m_hoveredElement = -1;

    // Check if inside scrollable content area
    if (y >= m_contentY && y <= m_contentY + m_viewportH) {
        float vy = y + m_scrollY;
        float startY = m_contentY + 8.f * m_dpiScale;
        float rowH = 46.f * m_dpiScale;
        float fh   = ds::Dims::InputH * m_dpiScale;
        float btnH = ds::Dims::BtnH * m_dpiScale;
        float fieldW = 200.f;
        float fwPx   = fieldW * m_dpiScale;
        float togW   = ds::Dims::ToggleW;

        auto ctrlLeft = [&](float ctrlW) -> float {
            return isRtl ? m_contentX : (m_contentX + m_contentW - ctrlW * m_dpiScale);
        };

        float curY = startY;

        // Section 1: General
        curY += 14.f * m_dpiScale + 32.f * m_dpiScale;
        float langX = ctrlLeft(fieldW);
        if (x >= langX && x <= langX + fwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 1;
        curY += rowH;

        // Color Theme swatches hover
        float themeW = 236.f;
        float thStep = 34.f * m_dpiScale;
        float thStartX = ctrlLeft(themeW) + 18.f * m_dpiScale;
        float thCy = curY + fh / 2.f;
        for (int i = 0; i < 6; ++i) {
            float cx = thStartX + i * thStep;
            float r = 15.f * m_dpiScale;
            if (x >= cx - r && x <= cx + r && vy >= thCy - r && vy <= thCy + r) {
                m_hoveredElement = 100 + i;
            }
        }
        curY += rowH;

        float swX = ctrlLeft(togW);
        if (x >= swX && x <= swX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 2;
        curY += rowH + 10.f * m_dpiScale;

        // Section 2: Popup
        curY += 14.f * m_dpiScale + 32.f * m_dpiScale;
        float monFieldW = 240.f;
        float mfwPx = monFieldW * m_dpiScale;
        float mfX = ctrlLeft(monFieldW);
        if (x >= mfX && x <= mfX + mfwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 3;
        curY += rowH;

        curY += rowH + 28*m_dpiScale; // Skip grid

        float durX = ctrlLeft(fieldW);
        if (x >= durX && x <= durX + fwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 5;
        curY += rowH;

        float surfX = ctrlLeft(fieldW);
        if (x >= surfX && x <= surfX + fwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 30;
        curY += rowH;

        float cornX = ctrlLeft(fieldW);
        if (x >= cornX && x <= cornX + fwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 31;
        curY += rowH;

        float araX = ctrlLeft(togW);
        if (x >= araX && x <= araX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 32;
        curY += rowH;

        float animX = ctrlLeft(togW);
        if (x >= animX && x <= animX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 6;
        curY += rowH;

        if (m_pendingSettings.animationEnabled) {
            float animFieldW = 210.f;
            float afwPx = animFieldW * m_dpiScale;
            float prevBtnW = 120.f * m_dpiScale;
            float gapX = 8.f * m_dpiScale;

            float dropX, prevX;
            if (isRtl) {
                prevX = m_contentX;
                dropX = prevX + prevBtnW + gapX;
            } else {
                float totalW = afwPx + gapX + prevBtnW;
                dropX = m_contentX + m_contentW - totalW;
                prevX = dropX + afwPx + gapX;
            }

            if (x >= dropX && x <= dropX + afwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 7;
            if (x >= prevX && x <= prevX + prevBtnW && vy >= curY && vy <= curY + btnH) m_hoveredElement = 8;
            curY += rowH;

            float spX = ctrlLeft(fieldW);
            if (x >= spX && x <= spX + fwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 33;
            curY += rowH;
        }
        curY += 10.f * m_dpiScale;

        // Section 3: Typography & Content
        curY += 14.f * m_dpiScale + 32.f * m_dpiScale;
        float fnX = ctrlLeft(fieldW);
        if (x >= fnX && x <= fnX + fwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 40;
        curY += rowH;

        float tskX = ctrlLeft(togW);
        if (x >= tskX && x <= tskX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 41;
        curY += rowH;

        float srcX = ctrlLeft(togW);
        if (x >= srcX && x <= srcX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 43;
        curY += rowH + 10.f * m_dpiScale;

        // Section 4: Sound
        curY += 14.f * m_dpiScale + 32.f * m_dpiScale;
        float sndX = ctrlLeft(togW);
        if (x >= sndX && x <= sndX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 11;
        curY += rowH;

        float prX = ctrlLeft(fieldW);
        if (x >= prX && x <= prX + fwPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 50;
        curY += rowH;

        float sliderX = isRtl ? (m_contentX + 40.f * m_dpiScale + 8.f * m_dpiScale) : ctrlLeft(fieldW);
        if (m_isDraggingVolume) {
            float t = (x - sliderX) / fwPx;
            m_pendingSettings.soundVolume = std::clamp((int)(t * 100), 0, 100);
            MainWindow::Instance().Invalidate();
        }
        if (x >= sliderX && x <= sliderX + fwPx && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 12;
        curY += rowH;

        if (m_pendingSettings.soundPreset == SoundPreset::Custom) {
            float browseW = 150.f * m_dpiScale;
            float resetW  = 36.f * m_dpiScale;
            if (isRtl) {
                float cX = m_contentX;
                if (x >= cX && x <= cX + browseW && vy >= curY && vy <= curY + btnH) m_hoveredElement = 13;
                cX += browseW + 8.f * m_dpiScale;
                if (!m_pendingSettings.customSoundPath.empty()) {
                    if (x >= cX && x <= cX + resetW && vy >= curY && vy <= curY + btnH) m_hoveredElement = 14;
                }
            } else {
                float cX = m_contentX + m_contentW;
                if (!m_pendingSettings.customSoundPath.empty()) {
                    cX -= resetW;
                    if (x >= cX && x <= cX + resetW && vy >= curY && vy <= curY + btnH) m_hoveredElement = 14;
                    cX -= 8.f * m_dpiScale;
                }
                cX -= browseW;
                if (x >= cX && x <= cX + browseW && vy >= curY && vy <= curY + btnH) m_hoveredElement = 13;
            }
            curY += rowH + 6*m_dpiScale;
        }

        float testBtnW = 140.f;
        float tbX = ctrlLeft(testBtnW);
        if (x >= tbX && x <= tbX + testBtnW * m_dpiScale && vy >= curY && vy <= curY + btnH) m_hoveredElement = 15;
        curY += rowH + 10.f * m_dpiScale;

        // Section 4: System
        curY += 14.f * m_dpiScale + 32.f * m_dpiScale;
        float fsX = ctrlLeft(togW);
        if (x >= fsX && x <= fsX + togW * m_dpiScale && vy >= curY && vy <= curY + ds::Dims::ToggleH * m_dpiScale) m_hoveredElement = 21;
        curY += rowH;

        if (m_pendingSettings.suppressDuringFullscreen) {
            float fsw = 240.f;
            float fswPx = fsw * m_dpiScale;
            float ffsX = ctrlLeft(fsw);
            if (x >= ffsX && x <= ffsX + fswPx && vy >= curY && vy <= curY + fh) m_hoveredElement = 22;
            curY += rowH;
        }

        curY += rowH; // Skip version

        float sysBtnW = 180.f;
        float sbwPx = sysBtnW * m_dpiScale;
        float ubX = ctrlLeft(sysBtnW);
        if (x >= ubX && x <= ubX + sbwPx && vy >= curY && vy <= curY + btnH) m_hoveredElement = 23;
        curY += rowH;

        float rbX = ctrlLeft(sysBtnW);
        if (x >= rbX && x <= rbX + sbwPx && vy >= curY && vy <= curY + btnH) m_hoveredElement = 24;
    }

    return (m_hoveredElement != oldHover || m_hoveredSave != oldSave);
}

void SettingsPage::OnMouseLeave() {
    m_hoveredElement = -1;
    m_pressedElement = -1;
    m_isDraggingVolume = false;
    m_hoveredSave = false;
}

void SettingsPage::OnSettingsChanged() {
    m_cachedMonitors = PopupWindow::EnumerateMonitors();
    if (!m_hasUnsavedChanges) {
        m_pendingSettings = Settings::Instance().GetCopy();
    }
}

void SettingsPage::OnActivated() {
    m_cachedMonitors = PopupWindow::EnumerateMonitors();
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
    // Note: m_scrollY is preserved across page activations so the user never loses their position
}

bool SettingsPage::HitTestInteractive(int x, int y) {
    return m_hoveredElement >= 0 || m_hoveredSave;
}

} // namespace dhikr
