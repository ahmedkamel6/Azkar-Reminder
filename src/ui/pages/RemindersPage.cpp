#include "RemindersPage.h"
#include "../DesignSystem.h"
#include "../TrayIcon.h"
#include "../../core/Application.h"
#include "../../core/Settings.h"
#include "../../core/Scheduler.h"
#include "../../core/Logger.h"
#include "../../core/Localization.h"
#include "../components/TimePickerPopup.h"
#include "../components/DurationPickerPopup.h"
#include "../components/DropdownPopup.h"
#include <algorithm>
#include <sstream>

namespace dhikr {

RemindersPage::RemindersPage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;
    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    const wchar_t* fontName = ds::Font::Arabic();
    auto mk = [&](float sz, DWRITE_FONT_WEIGHT wt, IDWriteTextFormat** out) {
        dwf->CreateTextFormat(fontName, nullptr, wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sz * m_dpiScale, L"en-US", out);
    };
    mk(ds::FontSize::PageTitle,  DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfTitle);
    mk(ds::FontSize::Secondary,  DWRITE_FONT_WEIGHT_REGULAR,   &m_tfSub);
    mk(ds::FontSize::Body,       DWRITE_FONT_WEIGHT_REGULAR,   &m_tfLabel);
    mk(ds::FontSize::InputText,  DWRITE_FONT_WEIGHT_REGULAR,   &m_tfValue);
    mk(ds::FontSize::BtnLabel,   DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfBtn);
    mk(ds::FontSize::Body,       DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfDay);
    
    bool isRtl = Localization::Instance().IsRTL();
    DWRITE_READING_DIRECTION rd = isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    if (m_tfTitle) m_tfTitle->SetReadingDirection(rd);
    if (m_tfSub)   m_tfSub->SetReadingDirection(rd);
    if (m_tfLabel) m_tfLabel->SetReadingDirection(rd);
    if (m_tfDay)   m_tfDay->SetReadingDirection(rd);

    // Centered button text format
    if (m_tfBtn) {
        m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    // Centered chip value text format
    if (m_tfValue) {
        m_tfValue->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfValue->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
}

RemindersPage::~RemindersPage() {
    if (m_tfTitle) { m_tfTitle->Release(); m_tfTitle = nullptr; }
    if (m_tfSub)   { m_tfSub->Release();   m_tfSub   = nullptr; }
    if (m_tfLabel) { m_tfLabel->Release(); m_tfLabel = nullptr; }
    if (m_tfValue) { m_tfValue->Release(); m_tfValue = nullptr; }
    if (m_tfBtn)   { m_tfBtn->Release();   m_tfBtn   = nullptr; }
    if (m_tfDay)   { m_tfDay->Release();   m_tfDay   = nullptr; }
    if (m_brush)   { m_brush->Release();   m_brush   = nullptr; }
}

void RemindersPage::DrawToggle(ID2D1RenderTarget* rt, float x, float y, bool on, bool hover) {
    float tw = 40.f * m_dpiScale;
    float th = 22.f * m_dpiScale;
    float kd = 16.f * m_dpiScale;
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

void RemindersPage::DrawRowToggle(ID2D1RenderTarget* rt, float cx, float cy, bool on) {
    float tw = 28.f * m_dpiScale;
    float th = 16.f * m_dpiScale;
    float kd = 12.f * m_dpiScale;
    float rx = th / 2.f;

    D2D1_RECT_F trackRect = D2D1::RectF(cx, cy, cx + tw, cy + th);

    if (on) {
        auto trackColor = ds::Accent::Primary();
        m_brush->SetColor(trackColor);
        rt->FillRoundedRectangle(D2D1::RoundedRect(trackRect, rx, rx), m_brush);

        float lum = 0.299f * trackColor.r + 0.587f * trackColor.g + 0.114f * trackColor.b;
        D2D1_COLOR_F knobColor = (lum >= 0.58f) ? ds::Rgba(0x0C, 0x10, 0x0E, 1.0f) : ds::Rgba(0xFF, 0xFF, 0xFF, 1.0f);

        float kx = cx + tw - kd - (th - kd) / 2.f;
        float ky = cy + (th - kd) / 2.f;
        m_brush->SetColor(knobColor);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx + kd / 2.f, ky + kd / 2.f), kd / 2.f, kd / 2.f), m_brush);
    } else {
        m_brush->SetColor(ds::Rgba(0x16, 0x1C, 0x18, 1.0f));
        rt->FillRoundedRectangle(D2D1::RoundedRect(trackRect, rx, rx), m_brush);
        m_brush->SetColor(ds::Rgba(0x52, 0x60, 0x56, 1.0f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(trackRect, rx, rx), m_brush, 1.0f);

        float kx = cx + (th - kd) / 2.f;
        float ky = cy + (th - kd) / 2.f;
        m_brush->SetColor(ds::Rgba(0x94, 0xA3, 0xB8, 1.0f));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx + kd / 2.f, ky + kd / 2.f), kd / 2.f, kd / 2.f), m_brush);
    }
}

void RemindersPage::DrawSmallTimeField(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, int h, int m, bool enabled) {
    m_brush->SetColor(enabled ? ds::Surface() : ds::Rgba(0x11, 0x14, 0x11, 1.0f));
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, 4*m_dpiScale, 4*m_dpiScale), m_brush);
    m_brush->SetColor(enabled ? ds::Rgba(0x3E, 0x4B, 0x42, 1.0f) : ds::Rgba(0x22, 0x2A, 0x24, 0.7f));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, 4*m_dpiScale, 4*m_dpiScale), m_brush, 0.8f);

    if (m_tfValue) {
        wchar_t buf[16];
        bool isRtl = Localization::Instance().IsRTL();
        const wchar_t* period = (h < 12) ? (isRtl ? L"ص" : L"AM") : (isRtl ? L"م" : L"PM");
        swprintf_s(buf, L"%02d:%02d %s", h % 12 == 0 ? 12 : h % 12, m, period);
        m_brush->SetColor(enabled ? ds::Rgba(0xFF, 0xFF, 0xFF, 1.0f) : ds::TextMuted());
        rt->DrawTextW(buf, (UINT32)wcslen(buf), m_tfValue, r, m_brush);
    }
}

void RemindersPage::DrawSaveButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    auto btnColor = m_hoverSave ? ds::Accent::Dark() : ds::Accent::Primary();
    m_brush->SetColor(btnColor);
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    if (m_tfBtn) {
        float lum = 0.299f * btnColor.r + 0.587f * btnColor.g + 0.114f * btnColor.b;
        m_brush->SetColor(lum >= 0.58f ? ds::Rgba(0x08, 0x0A, 0x09) : ds::Rgba(0xFF, 0xFF, 0xFF));
        std::wstring str = Localization::Instance().Get(m_hasUnsavedChanges ? L"reminders.save_star" : L"reminders.save");
        rt->DrawTextW(str.c_str(), (UINT32)str.size(), m_tfBtn, r, m_brush);
    }
}

void RemindersPage::DrawWeekRow(ID2D1RenderTarget* rt, int dayIndex, float x, float y, float w) {
    auto& s = m_pendingSettings;
    auto& ds_ref = s.weeklySchedule[dayIndex];
    bool active = (dayIndex == m_hoveredRow);
    bool isRtl = Localization::Instance().IsRTL();

    float rowH = 32.f * m_dpiScale;

    // Row hover highlight
    if (active) {
        m_brush->SetColor(ds::CardHover());
        rt->FillRectangle(D2D1::RectF(x, y, x+w, y+rowH), m_brush);
    }

    // Row separator
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawLine(D2D1::Point2F(x + 8.f*m_dpiScale, y+rowH), D2D1::Point2F(x+w - 8.f*m_dpiScale, y+rowH), m_brush, 0.5f);

    float cy = y + (rowH - 16.f*m_dpiScale) / 2.f;
    float chipH = 24.f * m_dpiScale;
    float chipY = y + (rowH - chipH) / 2.f;
    float timeW = 92.f * m_dpiScale;
    float freqW = 82.f * m_dpiScale;
    auto dayColor = ds_ref.enabled ? ds::Rgba(0xFF, 0xFF, 0xFF, 1.0f) : ds::TextMuted();

    const wchar_t* dkeys[] = {
        L"schedule.sun", L"schedule.mon", L"schedule.tue",
        L"schedule.wed", L"schedule.thu", L"schedule.fri", L"schedule.sat"
    };

    if (isRtl) {
        float cx = x + w - 16.f*m_dpiScale - 28.f*m_dpiScale;
        // Day toggle
        DrawRowToggle(rt, cx, cy, ds_ref.enabled);
        cx -= 75.f * m_dpiScale;

        // Day name
        m_brush->SetColor(dayColor);
        if (m_tfDay) {
            m_tfDay->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            D2D1_RECT_F dr = D2D1::RectF(cx, y, cx + 65.f*m_dpiScale, y + rowH);
            std::wstring dstr = Localization::Instance().Get(dkeys[dayIndex]);
            rt->DrawTextW(dstr.c_str(), (UINT32)dstr.size(), m_tfDay, dr, m_brush);
        }

        // Left-side chips
        float leftX = x + 16.f * m_dpiScale;

        // Frequency chip
        D2D1_RECT_F freqR = D2D1::RectF(leftX, chipY, leftX + freqW, chipY + chipH);
        bool fHover = (m_hoveredElement == 30 + dayIndex);
        m_brush->SetColor(fHover ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(freqR, 4*m_dpiScale, 4*m_dpiScale), m_brush);
        m_brush->SetColor(fHover ? ds::Accent::Primary(0.6f) : ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(freqR, 4*m_dpiScale, 4*m_dpiScale), m_brush, 0.5f);
        if (m_tfValue) {
            std::wstring fStr;
            int fmin = ds_ref.frequencyMinutes;
            if (fmin < 60) fStr = std::to_wstring(fmin) + L" د";
            else if (fmin == 60) fStr = L"1 س";
            else if (fmin == 120) fStr = L"2 س";
            else fStr = std::to_wstring(fmin / 60) + L" س " + (fmin % 60 != 0 ? (std::to_wstring(fmin % 60) + L" د") : L"");
            m_brush->SetColor(dayColor);
            rt->DrawTextW(fStr.c_str(), (UINT32)fStr.size(), m_tfValue, freqR, m_brush);
        }
        leftX += freqW + 8.f*m_dpiScale;

        // End time chip
        D2D1_RECT_F endR = D2D1::RectF(leftX, chipY, leftX + timeW, chipY + chipH);
        DrawSmallTimeField(rt, endR, ds_ref.endHour, ds_ref.endMin, ds_ref.enabled);
        leftX += timeW + 8.f*m_dpiScale;

        // Start time chip
        D2D1_RECT_F startR = D2D1::RectF(leftX, chipY, leftX + timeW, chipY + chipH);
        DrawSmallTimeField(rt, startR, ds_ref.startHour, ds_ref.startMin, ds_ref.enabled);
    } else {
        float cx = x + 16.f * m_dpiScale;
        // Day toggle
        DrawRowToggle(rt, cx, cy, ds_ref.enabled);
        cx += 36.f * m_dpiScale;

        // Day name
        m_brush->SetColor(dayColor);
        if (m_tfDay) {
            m_tfDay->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            D2D1_RECT_F dr = D2D1::RectF(cx, y, cx + 65.f*m_dpiScale, y + rowH);
            std::wstring dstr = Localization::Instance().Get(dkeys[dayIndex]);
            rt->DrawTextW(dstr.c_str(), (UINT32)dstr.size(), m_tfDay, dr, m_brush);
        }

        // Right-side chips
        float rightX = x + w - 16.f*m_dpiScale - freqW;

        // Frequency chip
        D2D1_RECT_F freqR = D2D1::RectF(rightX, chipY, rightX + freqW, chipY + chipH);
        bool fHover = (m_hoveredElement == 30 + dayIndex);
        m_brush->SetColor(fHover ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(freqR, 4*m_dpiScale, 4*m_dpiScale), m_brush);
        m_brush->SetColor(fHover ? ds::Accent::Primary(0.6f) : ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(freqR, 4*m_dpiScale, 4*m_dpiScale), m_brush, 0.5f);
        if (m_tfValue) {
            std::wstring fStr;
            int fmin = ds_ref.frequencyMinutes;
            if (fmin < 60) fStr = std::to_wstring(fmin) + L" min";
            else if (fmin == 60) fStr = L"1h";
            else if (fmin == 120) fStr = L"2h";
            else fStr = std::to_wstring(fmin / 60) + L"h " + (fmin % 60 != 0 ? (std::to_wstring(fmin % 60) + L"m") : L"");
            m_brush->SetColor(dayColor);
            rt->DrawTextW(fStr.c_str(), (UINT32)fStr.size(), m_tfValue, freqR, m_brush);
        }
        rightX -= (timeW + 8.f*m_dpiScale);

        // End time chip
        D2D1_RECT_F endR = D2D1::RectF(rightX, chipY, rightX + timeW, chipY + chipH);
        DrawSmallTimeField(rt, endR, ds_ref.endHour, ds_ref.endMin, ds_ref.enabled);
        rightX -= (timeW + 8.f*m_dpiScale);

        // Start time chip
        D2D1_RECT_F startR = D2D1::RectF(rightX, chipY, rightX + timeW, chipY + chipH);
        DrawSmallTimeField(rt, startR, ds_ref.startHour, ds_ref.startMin, ds_ref.enabled);
    }
}

void RemindersPage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);

    m_viewH = bounds.bottom - bounds.top;
    float padX = 24.f * m_dpiScale;
    float padY = 16.f * m_dpiScale;
    float x = bounds.left + padX;
    float w = bounds.right - bounds.left - padX * 2.f;
    bool isRtl = Localization::Instance().IsRTL();
    m_contentX = x; m_contentW = w;
    m_contentY = bounds.top + padY;

    auto& s = m_pendingSettings;

    // Clip rendering strictly to page bounds
    rt->PushAxisAlignedClip(bounds, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float y = bounds.top + padY - m_scrollY;

    // ── 1. Page Header: Title + Subtitle + Global Toggle Switch ─────────────
    float headH = 40.f * m_dpiScale;
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F tr = isRtl ? D2D1::RectF(x, y, x + w - 120.f*m_dpiScale, y + 24*m_dpiScale)
                              : D2D1::RectF(x, y, x + w - 120.f*m_dpiScale, y + 24*m_dpiScale);
        std::wstring title = Localization::Instance().Get(L"nav.reminders");
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle, tr, m_brush);
    }
    if (m_tfSub) {
        m_brush->SetColor(ds::TextSecondary());
        m_tfSub->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F sr = isRtl ? D2D1::RectF(x, y + 24*m_dpiScale, x + w - 120.f*m_dpiScale, y + 40*m_dpiScale)
                              : D2D1::RectF(x, y + 24*m_dpiScale, x + w - 120.f*m_dpiScale, y + 40*m_dpiScale);
        std::wstring sub = isRtl ? L"اضبط أوقات التذكيرات وجدول أيام الأسبوع" : L"Configure notification timing and active days";
        rt->DrawTextW(sub.c_str(), (UINT32)sub.size(), m_tfSub, sr, m_brush);
    }

    // Global Switch on opposite side
    float togW = 40.f * m_dpiScale;
    float togX = isRtl ? x : (x + w - togW);
    DrawToggle(rt, togX, y + 8.f*m_dpiScale, s.remindersEnabled, m_hoverToggle);

    y += headH + 12.f * m_dpiScale;

    // ── 2. Top Card: Master Reminder Controls (Start, End, Frequency, Quiet, Pause) ──
    float qCardH = 104.f * m_dpiScale;
    D2D1_RECT_F qCardR = D2D1::RectF(x, y, x + w, y + qCardH);
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(qCardR, 8.f*m_dpiScale, 8.f*m_dpiScale), m_brush);
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(qCardR, 8.f*m_dpiScale, 8.f*m_dpiScale), m_brush, 0.75f);

    float colGap = 10.f * m_dpiScale;
    float chipH  = 24.f * m_dpiScale;

    // ── Row 1: Start Time, End Time, Frequency (3 columns) ──
    float w1 = (w - 24.f * m_dpiScale - colGap * 2.f) / 3.f;
    float row1LabelY = y + 6.f * m_dpiScale;
    float row1ChipY  = y + 22.f * m_dpiScale;

    for (int col = 0; col < 3; ++col) {
        float cx = isRtl ? (x + w - 12.f * m_dpiScale - (col + 1) * w1 - col * colGap)
                         : (x + 12.f * m_dpiScale + col * (w1 + colGap));
        
        std::wstring lblText;
        std::wstring valText;
        bool isHovered = false;

        if (col == 0) { // Start Time
            lblText = Localization::Instance().Get(L"reminders.start_time");
            int h = s.weeklySchedule[0].startHour;
            int m = s.weeklySchedule[0].startMin;
            const wchar_t* period = (h < 12) ? (isRtl ? L"ص" : L"AM") : (isRtl ? L"م" : L"PM");
            wchar_t tbuf[32];
            swprintf_s(tbuf, L"%02d:%02d %s  ▾", h % 12 == 0 ? 12 : h % 12, m, period);
            valText = tbuf;
            isHovered = (m_hoveredElement == 101);
        } else if (col == 1) { // End Time
            lblText = Localization::Instance().Get(L"reminders.end_time");
            int h = s.weeklySchedule[0].endHour;
            int m = s.weeklySchedule[0].endMin;
            const wchar_t* period = (h < 12) ? (isRtl ? L"ص" : L"AM") : (isRtl ? L"م" : L"PM");
            wchar_t tbuf[32];
            swprintf_s(tbuf, L"%02d:%02d %s  ▾", h % 12 == 0 ? 12 : h % 12, m, period);
            valText = tbuf;
            isHovered = (m_hoveredElement == 102);
        } else { // Frequency
            lblText = Localization::Instance().Get(L"reminders.frequency");
            int f = s.weeklySchedule[0].frequencyMinutes;
            if (f < 60) valText = std::to_wstring(f) + L" " + Localization::Instance().Get(L"home.minutes") + L"  ▾";
            else if (f == 60) valText = (isRtl ? L"1 ساعة  ▾" : L"1 hour  ▾");
            else if (f == 120) valText = (isRtl ? L"2 ساعة  ▾" : L"2 hours  ▾");
            else valText = std::to_wstring(f / 60) + (isRtl ? L" س  ▾" : L"h  ▾");
            isHovered = (m_hoveredElement == 103);
        }

        // Draw label
        if (m_tfSub) {
            m_brush->SetColor(ds::TextSecondary());
            m_tfSub->SetTextAlignment(isRtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
            D2D1_RECT_F lr = D2D1::RectF(cx, row1LabelY, cx + w1, row1LabelY + 14.f * m_dpiScale);
            rt->DrawTextW(lblText.c_str(), (UINT32)lblText.size(), m_tfSub, lr, m_brush);
        }

        // Draw interactive chip
        D2D1_RECT_F chipR = D2D1::RectF(cx, row1ChipY, cx + w1, row1ChipY + chipH);
        m_brush->SetColor(isHovered ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(chipR, 5.f*m_dpiScale, 5.f*m_dpiScale), m_brush);
        m_brush->SetColor(isHovered ? ds::Accent::Primary(0.6f) : ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(chipR, 5.f*m_dpiScale, 5.f*m_dpiScale), m_brush, 0.5f);

        if (m_tfValue) {
            m_brush->SetColor(ds::TextPrimary());
            rt->DrawTextW(valText.c_str(), (UINT32)valText.size(), m_tfValue, chipR, m_brush);
        }
    }

    // ── Row 2: Quiet Hours, Pause For (2 columns) ──
    float w2 = (w - 24.f * m_dpiScale - colGap) / 2.f;
    float row2LabelY = y + 54.f * m_dpiScale;
    float row2ChipY  = y + 70.f * m_dpiScale;

    for (int col = 0; col < 2; ++col) {
        float cx = isRtl ? (x + w - 12.f * m_dpiScale - (col + 1) * w2 - col * colGap)
                         : (x + 12.f * m_dpiScale + col * (w2 + colGap));
        
        std::wstring lblText;
        std::wstring valText;
        bool isHovered = false;

        if (col == 0) { // Quiet Hours
            lblText = Localization::Instance().Get(L"reminders.quiet_hours");
            if (s.quietPeriods.empty()) {
                valText = isRtl ? L"بدون  ▾" : L"None  ▾";
            } else {
                wchar_t qbuf[32];
                auto& qp = s.quietPeriods[0];
                const wchar_t* p1 = qp.startHour < 12 ? (isRtl ? L"ص" : L"AM") : (isRtl ? L"م" : L"PM");
                const wchar_t* p2 = qp.endHour < 12 ? (isRtl ? L"ص" : L"AM") : (isRtl ? L"م" : L"PM");
                swprintf_s(qbuf, L"%d %s – %d %s  ▾", 
                    qp.startHour % 12 == 0 ? 12 : qp.startHour % 12, p1,
                    qp.endHour % 12 == 0 ? 12 : qp.endHour % 12, p2);
                valText = qbuf;
            }
            isHovered = (m_hoveredElement == 104);
        } else { // Pause
            lblText = Localization::Instance().Get(L"reminders.pause_for");
            valText = Scheduler::Instance().IsPaused() ? (Localization::Instance().Get(L"home.paused") + L"  ▾") : L"...  ▾";
            isHovered = (m_hoveredElement == 105);
        }

        // Draw label
        if (m_tfSub) {
            m_brush->SetColor(ds::TextSecondary());
            m_tfSub->SetTextAlignment(isRtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
            D2D1_RECT_F lr = D2D1::RectF(cx, row2LabelY, cx + w2, row2LabelY + 14.f * m_dpiScale);
            rt->DrawTextW(lblText.c_str(), (UINT32)lblText.size(), m_tfSub, lr, m_brush);
        }

        // Draw interactive chip
        D2D1_RECT_F chipR = D2D1::RectF(cx, row2ChipY, cx + w2, row2ChipY + chipH);
        m_brush->SetColor(isHovered ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(chipR, 5.f*m_dpiScale, 5.f*m_dpiScale), m_brush);
        m_brush->SetColor(isHovered ? ds::Accent::Primary(0.6f) : ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(chipR, 5.f*m_dpiScale, 5.f*m_dpiScale), m_brush, 0.5f);

        if (m_tfValue) {
            m_brush->SetColor(ds::TextPrimary());
            rt->DrawTextW(valText.c_str(), (UINT32)valText.size(), m_tfValue, chipR, m_brush);
        }
    }

    y += qCardH + 10.f * m_dpiScale;

    // ── 3. Weekly Schedule Card Container ───────────────────────────────────
    float sHeaderH = 28.f * m_dpiScale;
    float rowH = 32.f * m_dpiScale;
    float sCardH = sHeaderH + 7 * rowH + 6.f * m_dpiScale;

    D2D1_RECT_F sCardR = D2D1::RectF(x, y, x + w, y + sCardH);
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(sCardR, 10.f*m_dpiScale, 10.f*m_dpiScale), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(sCardR, 10.f*m_dpiScale, 10.f*m_dpiScale), m_brush, 0.75f);

    // Table Header Background
    D2D1_RECT_F thR = D2D1::RectF(x, y, x + w, y + sHeaderH);
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(thR, 10.f*m_dpiScale, 10.f*m_dpiScale), m_brush);
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawLine(D2D1::Point2F(x, y + sHeaderH), D2D1::Point2F(x + w, y + sHeaderH), m_brush, 0.75f);

    // Column Headers Text
    if (m_tfSub) {
        m_brush->SetColor(ds::TextSecondary());
        std::wstring hDay   = isRtl ? L"اليوم" : L"Day";
        std::wstring hStart = Localization::Instance().Get(L"reminders.start_time");
        std::wstring hEnd   = Localization::Instance().Get(L"reminders.end_time");
        std::wstring hFreq  = Localization::Instance().Get(L"reminders.frequency");

        float timeW = 92.f * m_dpiScale;
        float freqW = 82.f * m_dpiScale;

        if (isRtl) {
            // Day header on right
            m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            D2D1_RECT_F r1 = D2D1::RectF(x + w - 16.f*m_dpiScale - 100.f*m_dpiScale, y, x + w - 16.f*m_dpiScale, y + sHeaderH);
            rt->DrawTextW(hDay.c_str(), (UINT32)hDay.size(), m_tfSub, r1, m_brush);

            // Left side headers
            m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            float lx = x + 16.f * m_dpiScale;
            D2D1_RECT_F rF = D2D1::RectF(lx, y, lx + freqW, y + sHeaderH);
            rt->DrawTextW(hFreq.c_str(), (UINT32)hFreq.size(), m_tfSub, rF, m_brush);
            lx += freqW + 8.f*m_dpiScale;

            D2D1_RECT_F rE = D2D1::RectF(lx, y, lx + timeW, y + sHeaderH);
            rt->DrawTextW(hEnd.c_str(), (UINT32)hEnd.size(), m_tfSub, rE, m_brush);
            lx += timeW + 8.f*m_dpiScale;

            D2D1_RECT_F rS = D2D1::RectF(lx, y, lx + timeW, y + sHeaderH);
            rt->DrawTextW(hStart.c_str(), (UINT32)hStart.size(), m_tfSub, rS, m_brush);
        } else {
            // Day header on left
            m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            D2D1_RECT_F r1 = D2D1::RectF(x + 16.f*m_dpiScale, y, x + 16.f*m_dpiScale + 100.f*m_dpiScale, y + sHeaderH);
            rt->DrawTextW(hDay.c_str(), (UINT32)hDay.size(), m_tfSub, r1, m_brush);

            // Right side headers
            m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            float rx2 = x + w - 16.f*m_dpiScale - freqW;
            D2D1_RECT_F rF = D2D1::RectF(rx2, y, rx2 + freqW, y + sHeaderH);
            rt->DrawTextW(hFreq.c_str(), (UINT32)hFreq.size(), m_tfSub, rF, m_brush);
            rx2 -= (timeW + 8.f*m_dpiScale);

            D2D1_RECT_F rE = D2D1::RectF(rx2, y, rx2 + timeW, y + sHeaderH);
            rt->DrawTextW(hEnd.c_str(), (UINT32)hEnd.size(), m_tfSub, rE, m_brush);
            rx2 -= (timeW + 8.f*m_dpiScale);

            D2D1_RECT_F rS = D2D1::RectF(rx2, y, rx2 + timeW, y + sHeaderH);
            rt->DrawTextW(hStart.c_str(), (UINT32)hStart.size(), m_tfSub, rS, m_brush);
        }
    }

    // 7 Day rows
    float rowStartY = y + sHeaderH + 3.f * m_dpiScale;
    for (int i = 0; i < 7; ++i) {
        DrawWeekRow(rt, i, x, rowStartY + i * rowH, w);
    }

    y += sCardH + 16.f * m_dpiScale;

    // ── 4. Action Buttons (Save Changes & Preview Notification) ─────────────
    auto saveR    = GetSaveRect();
    auto previewR = GetPreviewRect();

    // 1. Draw Save Button
    DrawSaveButton(rt, saveR);

    // 2. Draw Preview Button
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    m_brush->SetColor(m_hoverPreview ? ds::Rgba(0x22, 0x2C, 0x24) : ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(previewR, rx, rx), m_brush);
    m_brush->SetColor(m_hoverPreview ? ds::Accent::Primary() : ds::Rgba(0x4A, 0x58, 0x4E, 1.0f));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(previewR, rx, rx), m_brush, 1.2f);
    if (m_tfBtn) {
        m_brush->SetColor(ds::Rgba(0xFF, 0xFF, 0xFF, 1.0f)); // Pure crisp brilliant white
        std::wstring btnLabel = Localization::Instance().Get(L"reminders.preview");
        rt->DrawTextW(btnLabel.c_str(), (UINT32)btnLabel.size(), m_tfBtn, previewR, m_brush);
    }

    y += 38.f * m_dpiScale + 16.f * m_dpiScale;

    // Total content height
    m_totalContentH = (y + m_scrollY) - (bounds.top + padY);

    rt->PopAxisAlignedClip();
}

D2D1_RECT_F RemindersPage::GetToggleRect() const {
    float tw = 40.f * m_dpiScale;
    float th = 22.f * m_dpiScale;
    float y  = m_contentY - m_scrollY + 8.f * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    float tX = isRtl ? m_contentX : (m_contentX + m_contentW - tw);
    return D2D1::RectF(tX, y, tX + tw, y + th);
}

D2D1_RECT_F RemindersPage::GetSaveRect() const {
    float btnW = 160.f * m_dpiScale;
    float btnH = 38.f * m_dpiScale;
    float gap  = 12.f * m_dpiScale;
    
    // Position below weekly schedule card
    float headH = 40.f * m_dpiScale;
    float qCardH = 104.f * m_dpiScale;
    float sHeaderH = 28.f * m_dpiScale;
    float rowH = 32.f * m_dpiScale;
    float sCardH = sHeaderH + 7 * rowH + 6.f * m_dpiScale;

    float y = m_contentY - m_scrollY + headH + 12.f*m_dpiScale + qCardH + 10.f*m_dpiScale + sCardH + 16.f*m_dpiScale;

    bool isRtl = Localization::Instance().IsRTL();
    float sX = isRtl ? m_contentX : (m_contentX + m_contentW - btnW);
    return D2D1::RectF(sX, y, sX + btnW, y + btnH);
}

D2D1_RECT_F RemindersPage::GetPreviewRect() const {
    float previewW = 180.f * m_dpiScale;
    float saveW    = 160.f * m_dpiScale;
    float btnH     = 38.f * m_dpiScale;
    float gap      = 12.f * m_dpiScale;
    
    float headH = 40.f * m_dpiScale;
    float qCardH = 104.f * m_dpiScale;
    float sHeaderH = 28.f * m_dpiScale;
    float rowH = 32.f * m_dpiScale;
    float sCardH = sHeaderH + 7 * rowH + 6.f * m_dpiScale;

    float y = m_contentY - m_scrollY + headH + 12.f*m_dpiScale + qCardH + 10.f*m_dpiScale + sCardH + 16.f*m_dpiScale;

    bool isRtl = Localization::Instance().IsRTL();
    float pX = isRtl ? (m_contentX + saveW + gap) : (m_contentX + m_contentW - saveW - gap - previewW);
    return D2D1::RectF(pX, y, pX + previewW, y + btnH);
}

bool RemindersPage::OnMouseWheel(int delta) {
    float maxScroll = std::max(0.f, m_totalContentH - m_viewH + 20.f * m_dpiScale);
    if (maxScroll <= 0) {
        if (m_scrollY != 0) {
            m_scrollY = 0;
            m_targetScrollY = 0;
            return true;
        }
        return false;
    }
    float scrollStep = (delta / 120.f) * (64.f * m_dpiScale);
    m_targetScrollY = std::clamp(m_targetScrollY - scrollStep, 0.f, maxScroll);
    float diff = m_targetScrollY - m_scrollY;
    if (std::abs(diff) < 0.4f) {
        m_scrollY = m_targetScrollY;
    } else {
        m_scrollY += diff * 0.25f;
    }
    return true;
}

bool RemindersPage::OnScrollTick() {
    float maxScroll = std::max(0.f, m_totalContentH - m_viewH + 20.f * m_dpiScale);
    m_targetScrollY = std::clamp(m_targetScrollY, 0.f, maxScroll);
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

bool RemindersPage::OnLButtonDown(int x, int y) {
    bool isRtl = Localization::Instance().IsRTL();

    // 1. Global Enable Toggle
    auto togR = GetToggleRect();
    if (x >= togR.left && x <= togR.right && y >= togR.top && y <= togR.bottom) {
        m_pendingSettings.remindersEnabled = !m_pendingSettings.remindersEnabled;
        m_hasUnsavedChanges = true;
        MainWindow::Instance().Invalidate();
        return true;
    }

    // 2. Quick Controls Card Hit-Testing
    float headH = 40.f * m_dpiScale;
    float qCardY = m_contentY - m_scrollY + headH + 12.f * m_dpiScale;
    float colGap = 10.f * m_dpiScale;
    float chipH  = 24.f * m_dpiScale;

    // Row 1: Start Time (101), End Time (102), Frequency (103)
    float w1 = (m_contentW - 24.f*m_dpiScale - colGap * 2.f) / 3.f;
    float row1ChipY = qCardY + 22.f * m_dpiScale;
    for (int col = 0; col < 3; ++col) {
        float cx = isRtl ? (m_contentX + m_contentW - 12.f*m_dpiScale - (col + 1)*w1 - col*colGap)
                         : (m_contentX + 12.f*m_dpiScale + col * (w1 + colGap));
        D2D1_RECT_F chipR = D2D1::RectF(cx, row1ChipY, cx + w1, row1ChipY + chipH);

        if (x >= chipR.left && x <= chipR.right && y >= chipR.top && y <= chipR.bottom) {
            POINT pt{ (LONG)cx, (LONG)(chipR.bottom + 2.f*m_dpiScale) };
            ClientToScreen(Application::Instance().MainWindow(), &pt);

            if (col == 0) { // Master Start Time
                TimePickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y,
                    m_pendingSettings.weeklySchedule[0].startHour,
                    m_pendingSettings.weeklySchedule[0].startMin,
                    [this](int h, int m) {
                        for (int i = 0; i < 7; ++i) {
                            m_pendingSettings.weeklySchedule[i].startHour = h;
                            m_pendingSettings.weeklySchedule[i].startMin = m;
                        }
                        m_hasUnsavedChanges = true;
                        MainWindow::Instance().Invalidate();
                    });
                return true;
            } else if (col == 1) { // Master End Time
                TimePickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y,
                    m_pendingSettings.weeklySchedule[0].endHour,
                    m_pendingSettings.weeklySchedule[0].endMin,
                    [this](int h, int m) {
                        for (int i = 0; i < 7; ++i) {
                            m_pendingSettings.weeklySchedule[i].endHour = h;
                            m_pendingSettings.weeklySchedule[i].endMin = m;
                        }
                        m_hasUnsavedChanges = true;
                        MainWindow::Instance().Invalidate();
                    });
                return true;
            } else { // Master Frequency
                int curFreq = m_pendingSettings.weeklySchedule[0].frequencyMinutes;
                std::vector<DropdownItem> items;
                int freqs[] = { 5, 15, 30, 45, 60, 120 };
                for (int f : freqs) {
                    std::wstring label;
                    if (f < 60) label = std::to_wstring(f) + L" " + Localization::Instance().Get(L"home.minutes");
                    else if (f == 60) label = isRtl ? L"1 ساعة" : L"1 hour";
                    else if (f == 120) label = isRtl ? L"2 ساعة" : L"2 hours";
                    else label = std::to_wstring(f / 60) + (isRtl ? L" ساعات" : L" hours");
                    items.push_back({ f, label, (curFreq == f) });
                }
                std::wstring customOpt = isRtl ? L"تخصيص وقت محدد..." : L"Custom Duration...";
                items.push_back({ -1, customOpt, false });

                DropdownPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y, w1 / m_dpiScale, items,
                    [this, pt, curFreq](int chosenFreq) {
                        if (chosenFreq == -1) {
                            DurationPickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y, curFreq,
                                [this](int customMinutes) {
                                    for (int i = 0; i < 7; ++i) {
                                        m_pendingSettings.weeklySchedule[i].frequencyMinutes = customMinutes;
                                    }
                                    m_hasUnsavedChanges = true;
                                    MainWindow::Instance().Invalidate();
                                });
                        } else {
                            for (int i = 0; i < 7; ++i) {
                                m_pendingSettings.weeklySchedule[i].frequencyMinutes = chosenFreq;
                            }
                            m_hasUnsavedChanges = true;
                            MainWindow::Instance().Invalidate();
                        }
                    });
                return true;
            }
        }
    }

    // Row 2: Quiet Hours (104), Pause For (105)
    float w2 = (m_contentW - 24.f*m_dpiScale - colGap) / 2.f;
    float row2ChipY = qCardY + 70.f * m_dpiScale;
    for (int col = 0; col < 2; ++col) {
        float cx = isRtl ? (m_contentX + m_contentW - 12.f*m_dpiScale - (col + 1)*w2 - col*colGap)
                         : (m_contentX + 12.f*m_dpiScale + col * (w2 + colGap));
        D2D1_RECT_F chipR = D2D1::RectF(cx, row2ChipY, cx + w2, row2ChipY + chipH);

        if (x >= chipR.left && x <= chipR.right && y >= chipR.top && y <= chipR.bottom) {
            POINT pt{ (LONG)cx, (LONG)(chipR.bottom + 2.f*m_dpiScale) };
            ClientToScreen(Application::Instance().MainWindow(), &pt);

            if (col == 0) { // Quiet Hours
                std::vector<DropdownItem> items = {
                    { 0, isRtl ? L"بدون أوقات هدوء" : L"Disable Quiet Hours", m_pendingSettings.quietPeriods.empty() },
                    { 1, isRtl ? L"10:00 م - 06:00 ص (النوم)" : L"10:00 PM - 06:00 AM (Night)", !m_pendingSettings.quietPeriods.empty() && m_pendingSettings.quietPeriods[0].startHour == 22 },
                    { 2, isRtl ? L"11:00 م - 07:00 ص" : L"11:00 PM - 07:00 AM", !m_pendingSettings.quietPeriods.empty() && m_pendingSettings.quietPeriods[0].startHour == 23 },
                    { 3, isRtl ? L"12:00 م - 02:00 م (الظهيرة)" : L"12:00 PM - 02:00 PM (Noon)", !m_pendingSettings.quietPeriods.empty() && m_pendingSettings.quietPeriods[0].startHour == 12 }
                };
                DropdownPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y, w2 / m_dpiScale, items,
                    [this](int cmd) {
                        m_pendingSettings.quietPeriods.clear();
                        if (cmd == 1) m_pendingSettings.quietPeriods.push_back({22, 0, 6, 0});
                        else if (cmd == 2) m_pendingSettings.quietPeriods.push_back({23, 0, 7, 0});
                        else if (cmd == 3) m_pendingSettings.quietPeriods.push_back({12, 0, 14, 0});
                        m_hasUnsavedChanges = true;
                        MainWindow::Instance().Invalidate();
                    });
                return true;
            } else { // Pause For
                std::vector<DropdownItem> items = {
                    { 15, L"15 " + Localization::Instance().Get(L"home.minutes"), false },
                    { 30, L"30 " + Localization::Instance().Get(L"home.minutes"), false },
                    { 60, L"1 " + Localization::Instance().Get(L"schedule.hours"), false },
                    { 120, L"2 " + Localization::Instance().Get(L"schedule.hours"), false },
                    { 1440, Localization::Instance().Get(L"reminders.pause_today"), false },
                    { 999, isRtl ? L"استئناف الآن" : L"Resume Now", false }
                };
                DropdownPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y, w2 / m_dpiScale, items,
                    [this](int cmd) {
                        if (cmd == 999) Scheduler::Instance().Resume();
                        else if (cmd == 1440) Scheduler::Instance().PauseUntilTomorrow();
                        else Scheduler::Instance().Pause(cmd);
                        TrayIcon::Instance().SetPaused(ds::Accent::IsAppPaused());
                        MainWindow::Instance().UpdateAppIcon();
                        m_pendingSettings = Settings::Instance().GetCopy();
                        MainWindow::Instance().Invalidate();
                    });
                return true;
            }
        }
    }

    // 3. Weekly Schedule Table Hit-Testing
    if (m_hoveredElement >= 0) {
        m_pressedElement = m_hoveredElement;

        // Day Toggle (0-6)
        if (m_hoveredElement >= 0 && m_hoveredElement < 7) {
            m_pendingSettings.weeklySchedule[m_hoveredElement].enabled = !m_pendingSettings.weeklySchedule[m_hoveredElement].enabled;
            m_hasUnsavedChanges = true;
            MainWindow::Instance().Invalidate();
            return true;
        }
        // Day Start Time (10-16)
        else if (m_hoveredElement >= 10 && m_hoveredElement < 17) {
            int day = m_hoveredElement - 10;
            POINT pt{ x, y + 20 };
            ClientToScreen(Application::Instance().MainWindow(), &pt);
            TimePickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y,
                m_pendingSettings.weeklySchedule[day].startHour,
                m_pendingSettings.weeklySchedule[day].startMin,
                [this, day](int h, int m) {
                    m_pendingSettings.weeklySchedule[day].startHour = h;
                    m_pendingSettings.weeklySchedule[day].startMin = m;
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                });
            return true;
        }
        // Day End Time (20-26)
        else if (m_hoveredElement >= 20 && m_hoveredElement < 27) {
            int day = m_hoveredElement - 20;
            POINT pt{ x, y + 20 };
            ClientToScreen(Application::Instance().MainWindow(), &pt);
            TimePickerPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y,
                m_pendingSettings.weeklySchedule[day].endHour,
                m_pendingSettings.weeklySchedule[day].endMin,
                [this, day](int h, int m) {
                    m_pendingSettings.weeklySchedule[day].endHour = h;
                    m_pendingSettings.weeklySchedule[day].endMin = m;
                    m_hasUnsavedChanges = true;
                    MainWindow::Instance().Invalidate();
                });
            return true;
        }
        // Day Frequency (30-36)
        else if (m_hoveredElement >= 30 && m_hoveredElement < 37) {
            int day = m_hoveredElement - 30;
            int curFreq = m_pendingSettings.weeklySchedule[day].frequencyMinutes;

            std::vector<DropdownItem> items;
            int freqs[] = { 5, 15, 30, 45, 60, 120 };
            for (int f : freqs) {
                std::wstring label;
                if (f < 60) label = std::to_wstring(f) + L" " + Localization::Instance().Get(L"home.minutes");
                else if (f == 60) label = isRtl ? L"1 ساعة" : L"1 hour";
                else if (f == 120) label = isRtl ? L"2 ساعة" : L"2 hours";
                else label = std::to_wstring(f / 60) + (isRtl ? L" ساعات" : L" hours");
                items.push_back({ f, label, (curFreq == f) });
            }
            std::wstring customOpt = isRtl ? L"تخصيص وقت محدد..." : L"Custom Duration...";
            items.push_back({ -1, customOpt, false });

            POINT pt{ x, y + 20 };
            ClientToScreen(Application::Instance().MainWindow(), &pt);

            DropdownPopup::Instance().Show(Application::Instance().MainWindow(), pt.x, pt.y, 140.f, items,
                [this, pt, curFreq, day](int chosenFreq) {
                    if (chosenFreq == -1) {
                        DurationPickerPopup::Instance().Show(
                            Application::Instance().MainWindow(),
                            pt.x, pt.y, curFreq,
                            [this, day](int customMinutes) {
                                m_pendingSettings.weeklySchedule[day].frequencyMinutes = customMinutes;
                                m_hasUnsavedChanges = true;
                                MainWindow::Instance().Invalidate();
                            });
                    } else {
                        m_pendingSettings.weeklySchedule[day].frequencyMinutes = chosenFreq;
                        m_hasUnsavedChanges = true;
                        MainWindow::Instance().Invalidate();
                    }
                });
            return true;
        }
    }
    
    // 4. Action Buttons Hit-Testing
    auto previewR = GetPreviewRect();
    if (x >= previewR.left && x <= previewR.right && y >= previewR.top && y <= previewR.bottom) {
        Scheduler::Instance().TriggerNow();
        return true;
    }

    auto saveR = GetSaveRect();
    if (x >= saveR.left && x <= saveR.right && y >= saveR.top && y <= saveR.bottom) {
        if (m_hasUnsavedChanges) {
            float savedScroll = m_scrollY;
            Settings::Instance().UpdatePreferences(m_pendingSettings);
            Settings::Instance().Save();
            m_hasUnsavedChanges = false;
            MainWindow::Instance().OnSettingsChanged();
            m_scrollY = savedScroll;
            Scheduler::Instance().Recalculate();
            MainWindow::Instance().Invalidate();
            LOG_INFO("Unified Reminder & Schedule settings saved successfully");
        }
        return true;
    }
    return false;
}

bool RemindersPage::OnLButtonUp(int /*x*/, int /*y*/) {
    m_pressedElement = -1;
    return false;
}

bool RemindersPage::OnMouseMove(int x, int y) {
    auto tr = GetToggleRect(); 
    m_hoverToggle  = (x >= tr.left && x <= tr.right && y >= tr.top && y <= tr.bottom);
    auto sr = GetSaveRect();   
    m_hoverSave    = (x >= sr.left && x <= sr.right && y >= sr.top && y <= sr.bottom);
    auto pr = GetPreviewRect();
    m_hoverPreview = (x >= pr.left && x <= pr.right && y >= pr.top && y <= pr.bottom);

    int oldElem = m_hoveredElement;
    int oldRow  = m_hoveredRow;

    m_hoveredElement = -1;
    m_hoveredRow = -1;

    // Hit-test Quick Controls Card
    float headH = 40.f * m_dpiScale;
    float qCardY = m_contentY - m_scrollY + headH + 12.f * m_dpiScale;
    float qCardH = 104.f * m_dpiScale;
    float colGap = 10.f * m_dpiScale;
    float chipH  = 24.f * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    // Row 1: Start (101), End (102), Freq (103)
    float w1 = (m_contentW - 24.f*m_dpiScale - colGap * 2.f) / 3.f;
    float row1ChipY = qCardY + 22.f * m_dpiScale;
    for (int col = 0; col < 3; ++col) {
        float cx = isRtl ? (m_contentX + m_contentW - 12.f*m_dpiScale - (col + 1)*w1 - col*colGap)
                         : (m_contentX + 12.f*m_dpiScale + col * (w1 + colGap));
        D2D1_RECT_F chipR = D2D1::RectF(cx, row1ChipY, cx + w1, row1ChipY + chipH);
        if (x >= chipR.left && x <= chipR.right && y >= chipR.top && y <= chipR.bottom) {
            m_hoveredElement = 101 + col;
        }
    }

    // Row 2: Quiet (104), Pause (105)
    float w2 = (m_contentW - 24.f*m_dpiScale - colGap) / 2.f;
    float row2ChipY = qCardY + 70.f * m_dpiScale;
    for (int col = 0; col < 2; ++col) {
        float cx = isRtl ? (m_contentX + m_contentW - 12.f*m_dpiScale - (col + 1)*w2 - col*colGap)
                         : (m_contentX + 12.f*m_dpiScale + col * (w2 + colGap));
        D2D1_RECT_F chipR = D2D1::RectF(cx, row2ChipY, cx + w2, row2ChipY + chipH);
        if (x >= chipR.left && x <= chipR.right && y >= chipR.top && y <= chipR.bottom) {
            m_hoveredElement = 104 + col;
        }
    }

    // Hit-test Weekly Schedule Card
    float sHeaderH = 28.f * m_dpiScale;
    float rowH = 32.f * m_dpiScale;
    float sCardY = qCardY + qCardH + 10.f * m_dpiScale;
    float rowStartY = sCardY + sHeaderH + 3.f * m_dpiScale;
    float schedEndY = rowStartY + 7 * rowH;

    if (x >= m_contentX && x <= m_contentX + m_contentW && y >= rowStartY && y < schedEndY) {
        int row = (int)((y - rowStartY) / rowH);
        if (row >= 0 && row < 7) {
            m_hoveredRow = row;
            float timeW = 92.f * m_dpiScale;
            float freqW = 82.f * m_dpiScale;
            float chipH = 24.f * m_dpiScale;
            float rowY = rowStartY + row * rowH;
            float chipY = rowY + (rowH - chipH) / 2.f;

            if (isRtl) {
                float togCx = m_contentX + m_contentW - 16.f*m_dpiScale - 28.f*m_dpiScale;
                if (x >= togCx && x <= togCx + 28.f*m_dpiScale) {
                    m_hoveredElement = row; // toggle
                }
                float lx = m_contentX + 16.f * m_dpiScale;
                if (x >= lx && x <= lx + freqW && y >= chipY && y <= chipY + chipH) {
                    m_hoveredElement = 30 + row; // freq
                }
                lx += freqW + 8.f*m_dpiScale;
                if (x >= lx && x <= lx + timeW && y >= chipY && y <= chipY + chipH) {
                    m_hoveredElement = 20 + row; // end time
                }
                lx += timeW + 8.f*m_dpiScale;
                if (x >= lx && x <= lx + timeW && y >= chipY && y <= chipY + chipH) {
                    m_hoveredElement = 10 + row; // start time
                }
            } else {
                float togCx = m_contentX + 16.f * m_dpiScale;
                if (x >= togCx && x <= togCx + 28.f*m_dpiScale) {
                    m_hoveredElement = row; // toggle
                }
                float rx2 = m_contentX + m_contentW - 16.f*m_dpiScale - freqW;
                if (x >= rx2 && x <= rx2 + freqW && y >= chipY && y <= chipY + chipH) {
                    m_hoveredElement = 30 + row; // freq
                }
                rx2 -= (timeW + 8.f*m_dpiScale);
                if (x >= rx2 && x <= rx2 + timeW && y >= chipY && y <= chipY + chipH) {
                    m_hoveredElement = 20 + row; // end time
                }
                rx2 -= (timeW + 8.f*m_dpiScale);
                if (x >= rx2 && x <= rx2 + timeW && y >= chipY && y <= chipY + chipH) {
                    m_hoveredElement = 10 + row; // start time
                }
            }
        }
    }

    return (m_hoveredElement != oldElem || m_hoveredRow != oldRow || m_hoverToggle || m_hoverSave || m_hoverPreview);
}

void RemindersPage::OnMouseLeave() { 
    m_hoverToggle    = false; 
    m_hoverSave      = false; 
    m_hoverPreview   = false;
    m_hoveredRow     = -1;
    m_hoveredElement = -1;
    m_pressedElement = -1;
}

void RemindersPage::OnSettingsChanged() {
    if (!m_hasUnsavedChanges) {
        m_pendingSettings = Settings::Instance().GetCopy();
    }
}

void RemindersPage::OnActivated() {
    m_pendingSettings = Settings::Instance().GetCopy();
    m_hasUnsavedChanges = false;
    // Note: m_scrollY is preserved across page activations so the user never loses their position
}

bool RemindersPage::HitTestInteractive(int x, int y) {
    auto tr = GetToggleRect();
    if (x >= tr.left && x <= tr.right && y >= tr.top && y <= tr.bottom) return true;

    if (m_hoveredElement >= 0) return true;

    auto sr = GetSaveRect();
    if (x >= sr.left && x <= sr.right && y >= sr.top && y <= sr.bottom) return true;

    auto pr = GetPreviewRect();
    if (x >= pr.left && x <= pr.right && y >= pr.top && y <= pr.bottom) return true;

    return false;
}

} // namespace dhikr
