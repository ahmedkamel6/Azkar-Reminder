#include "HomePage.h"
#include "../DesignSystem.h"
#include "../TrayIcon.h"
#include "../../core/Application.h"
#include "../../core/Database.h"
#include "../../core/Scheduler.h"
#include "../../core/Settings.h"
#include "../../core/Localization.h"
#include "../../core/ContentEngine.h"
#include "../core/LogoRenderer.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace dhikr {

ID2D1Factory* HomePage::m_d2dFactory_ = nullptr;

HomePage::HomePage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;

    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    const wchar_t* fontName = ds::Font::Arabic();
    auto create = [&](float size, DWRITE_FONT_WEIGHT wt, IDWriteTextFormat** out) {
        HRESULT hr = dwf->CreateTextFormat(fontName, nullptr, wt,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            size * m_dpiScale, L"en-US", out);
        if (FAILED(hr) || !*out) {
            dwf->CreateTextFormat(L"Segoe UI", nullptr, wt,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                size * m_dpiScale, L"en-US", out);
        }
    };

    create(19.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfGreet);
    create(11.5f, DWRITE_FONT_WEIGHT_REGULAR,   &m_tfSub);
    create(24.f, DWRITE_FONT_WEIGHT_BOLD,      &m_tfBigNum);
    create(17.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfMedNum);
    create(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfLabel);
    create(9.5f, DWRITE_FONT_WEIGHT_REGULAR,   &m_tfSmall);
    create(18.f, DWRITE_FONT_WEIGHT_BOLD,      &m_tfTime);
    create(9.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfBadge);
    create(10.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_tfBtn);

    if (m_tfBtn) {
        m_tfBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfBadge) {
        m_tfBadge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfBadge->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    m_masterToggle = std::make_shared<ToggleControl>(Settings::Instance().GetCopy().remindersEnabled && !ds::Accent::IsAppPaused());
    m_masterToggle->OnClick = []() {
        Settings::Instance().Modify([](AppSettings& settings) {
            settings.remindersEnabled = !settings.remindersEnabled;
        });
        const auto s = Settings::Instance().GetCopy();
        if (s.remindersEnabled) Scheduler::Instance().Resume();
        Settings::Instance().Save();
        Scheduler::Instance().Recalculate();
        TrayIcon::Instance().SetPaused(!s.remindersEnabled);
        MainWindow::Instance().UpdateAppIcon();
        MainWindow::Instance().Invalidate();
    };
}

HomePage::~HomePage() {
    if (m_tfGreet)  { m_tfGreet->Release();  m_tfGreet  = nullptr; }
    if (m_tfSub)    { m_tfSub->Release();    m_tfSub    = nullptr; }
    if (m_tfBigNum) { m_tfBigNum->Release(); m_tfBigNum = nullptr; }
    if (m_tfMedNum) { m_tfMedNum->Release(); m_tfMedNum = nullptr; }
    if (m_tfLabel)  { m_tfLabel->Release();  m_tfLabel  = nullptr; }
    if (m_tfSmall)  { m_tfSmall->Release();  m_tfSmall  = nullptr; }
    if (m_tfTime)   { m_tfTime->Release();   m_tfTime   = nullptr; }
    if (m_tfBadge)  { m_tfBadge->Release();  m_tfBadge  = nullptr; }
    if (m_tfBtn)    { m_tfBtn->Release();    m_tfBtn    = nullptr; }
    if (m_brush)    { m_brush->Release();    m_brush    = nullptr; }
}

void HomePage::OnActivated() {
    m_todayDelivered = Database::Instance().GetTodayDeliveredCount();
    m_streak = Database::Instance().GetCurrentStreakDays();
    m_total = Database::Instance().GetWeekCount();
    m_dailyStats = Database::Instance().GetDailyStats(7);
}

void HomePage::OnSettingsChanged() {
    if (m_masterToggle) {
        m_masterToggle->SetIsOn(Settings::Instance().GetCopy().remindersEnabled && !ds::Accent::IsAppPaused());
    }
    OnActivated();
}

void HomePage::DrawCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover) {
    float rx = 12.f * m_dpiScale;
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(r, rx, rx);

    // Smooth card fill
    m_brush->SetColor(isHover ? ds::CardHover() : ds::Card());
    rt->FillRoundedRectangle(rr, m_brush);

    // Subtle border, highlighted on hover
    m_brush->SetColor(isHover ? ds::Accent::Primary(0.55f) : ds::BorderSubtle());
    rt->DrawRoundedRectangle(rr, m_brush, isHover ? 1.25f : 0.75f);
}

void HomePage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_d2dFactory_) {
        rt->GetFactory(&m_d2dFactory_);
    }
    if (!m_brush) {
        rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &m_brush);
    }

    if (m_todayDelivered == -1) {
        OnActivated();
    }

    bool isRtl = Localization::Instance().IsRTL();
    DWRITE_READING_DIRECTION rd = isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    if (m_tfGreet)  m_tfGreet->SetReadingDirection(rd);
    if (m_tfSub)    m_tfSub->SetReadingDirection(rd);
    if (m_tfBigNum) m_tfBigNum->SetReadingDirection(rd);
    if (m_tfMedNum) m_tfMedNum->SetReadingDirection(rd);
    if (m_tfLabel)  m_tfLabel->SetReadingDirection(rd);
    if (m_tfSmall)  m_tfSmall->SetReadingDirection(rd);
    if (m_tfTime)   m_tfTime->SetReadingDirection(rd);

    float pad = 20.f * m_dpiScale;
    float x = bounds.left + pad;
    float y = bounds.top  + pad;
    float w = bounds.right - bounds.left - pad * 2.f;

    // ── 1. Top Header Row: Greeting & Status Badge ──────────────────────────────
    float badgeW = 160.f * m_dpiScale;
    float headerH = 44.f * m_dpiScale;
    if (isRtl) {
        DrawStatusBadge(rt, x, y + 4.f * m_dpiScale, badgeW);
        DrawGreeting(rt, D2D1::RectF(x + badgeW + 12.f * m_dpiScale, y, x + w, y + headerH));
    } else {
        DrawGreeting(rt, D2D1::RectF(x, y, x + w - badgeW - 12.f * m_dpiScale, y + headerH));
        DrawStatusBadge(rt, x + w - badgeW, y + 4.f * m_dpiScale, badgeW);
    }
    y += headerH + 14.f * m_dpiScale;

    // ── 2. Row 1: Next Reminder Card & Today's Goal Card ─────────────────────────
    float gap = 14.f * m_dpiScale;
    float colW = (w - gap) / 2.f;
    float row1H = 118.f * m_dpiScale;

    D2D1_RECT_F rNext  = isRtl ? D2D1::RectF(x + colW + gap, y, x + w, y + row1H)
                              : D2D1::RectF(x, y, x + colW, y + row1H);
    D2D1_RECT_F rToday = isRtl ? D2D1::RectF(x, y, x + colW, y + row1H)
                              : D2D1::RectF(x + colW + gap, y, x + w, y + row1H);

    DrawNextReminderHero(rt, rNext,  (m_hoveredCard == 1));
    DrawTodayGoalCard   (rt, rToday, (m_hoveredCard == 2));
    y += row1H + 12.f * m_dpiScale;

    // ── 3. Row 2: Streak Card & Weekly Total Activity Card ──────────────────────
    float row2H = 110.f * m_dpiScale;
    D2D1_RECT_F rStreak = isRtl ? D2D1::RectF(x + colW + gap, y, x + w, y + row2H)
                               : D2D1::RectF(x, y, x + colW, y + row2H);
    D2D1_RECT_F rTotal  = isRtl ? D2D1::RectF(x, y, x + colW, y + row2H)
                               : D2D1::RectF(x + colW + gap, y, x + w, y + row2H);

    DrawStreakCard(rt, rStreak, (m_hoveredCard == 3));
    DrawTotalCard (rt, rTotal,  (m_hoveredCard == 4));
    y += row2H + 12.f * m_dpiScale;

    // ── 4. Row 3: Today's Journey Timeline (Full Width) ─────────────────────────
    float row3H = 96.f * m_dpiScale;
    D2D1_RECT_F rTimeline = D2D1::RectF(x, y, x + w, y + row3H);
    DrawProgressTimeline(rt, rTimeline, (m_hoveredCard == 5));
}

void HomePage::DrawGreeting(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    if (!m_brush) return;
    bool isRtl = Localization::Instance().IsRTL();

    // Time-aware greeting
    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);
    int hour = lt.tm_hour;

    std::wstring greet;
    if (hour >= 4 && hour < 12) {
        greet = isRtl ? L"صباح الخير والبركة" : L"Good Morning";
    } else if (hour >= 12 && hour < 17) {
        greet = isRtl ? L"مساء الخير والسكينة" : L"Good Afternoon";
    } else {
        greet = isRtl ? L"طابت ليلتك بذكر الله" : L"Good Evening";
    }

    std::wstring sub = isRtl ? L"اجعل لذكر الله نصيبًا من يومك • تقبّل الله طاعتكم"
                             : L"Make the remembrance of Allah a peaceful part of your day";

    m_brush->SetColor(ds::TextPrimary());
    if (m_tfGreet) {
        m_tfGreet->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_tfGreet->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F gr = D2D1::RectF(r.left, r.top, r.right, r.top + 24.f * m_dpiScale);
        rt->DrawTextW(greet.c_str(), (UINT32)greet.size(), m_tfGreet, gr, m_brush);
    }

    m_brush->SetColor(ds::TextSecondary());
    if (m_tfSub) {
        m_tfSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F sr = D2D1::RectF(r.left, r.top + 25.f * m_dpiScale, r.right, r.bottom);
        rt->DrawTextW(sub.c_str(), (UINT32)sub.size(), m_tfSub, sr, m_brush);
    }
}

void HomePage::DrawStatusBadge(ID2D1RenderTarget* rt, float x, float y, float w) {
    if (!m_brush || !m_masterToggle) return;

    float badgeH = 34.f * m_dpiScale;
    D2D1_RECT_F r = D2D1::RectF(x, y, x + w, y + badgeH);
    m_statusBadgeRect = r;

    float rx = badgeH / 2.f;
    bool isOn = Settings::Instance().GetCopy().remindersEnabled && !ds::Accent::IsAppPaused();
    bool isRtl = Localization::Instance().IsRTL();

    // Pill background
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(isOn ? ds::Accent::Primary(0.45f) : ds::BorderSubtle());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.75f);

    float tw = ds::Dims::ToggleW * m_dpiScale;
    float th = ds::Dims::ToggleH * m_dpiScale;
    float ty = y + (badgeH - th) / 2.f;

    // Glowing LED Dot
    float dotR = 3.5f * m_dpiScale;
    if (isRtl) {
        float tx = r.left + 8.f * m_dpiScale;
        m_masterToggle->SetBounds(D2D1::RectF(tx, ty, tx + tw, ty + th));
        m_masterToggle->SetIsOn(isOn);
        m_masterToggle->Draw(rt, m_brush, nullptr);

        float dotX = r.right - 14.f * m_dpiScale;
        float dotY = y + badgeH / 2.f;
        m_brush->SetColor(isOn ? ds::Accent::Primary() : ds::Rgba(0x75, 0x80, 0x77));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), dotR, dotR), m_brush);

        if (m_tfBadge) {
            std::wstring st = Localization::Instance().Get(isOn ? L"home.active" : L"home.paused");
            m_brush->SetColor(isOn ? ds::TextPrimary() : ds::TextSecondary());
            D2D1_RECT_F tr = D2D1::RectF(tx + tw + 4.f * m_dpiScale, y, dotX - 6.f * m_dpiScale, y + badgeH);
            m_tfBadge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawTextW(st.c_str(), (UINT32)st.size(), m_tfBadge, tr, m_brush);
        }
    } else {
        float tx = r.right - 8.f * m_dpiScale - tw;
        m_masterToggle->SetBounds(D2D1::RectF(tx, ty, tx + tw, ty + th));
        m_masterToggle->SetIsOn(isOn);
        m_masterToggle->Draw(rt, m_brush, nullptr);

        float dotX = r.left + 14.f * m_dpiScale;
        float dotY = y + badgeH / 2.f;
        m_brush->SetColor(isOn ? ds::Accent::Primary() : ds::Rgba(0x75, 0x80, 0x77));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), dotR, dotR), m_brush);

        if (m_tfBadge) {
            std::wstring st = Localization::Instance().Get(isOn ? L"home.active" : L"home.paused");
            m_brush->SetColor(isOn ? ds::TextPrimary() : ds::TextSecondary());
            D2D1_RECT_F tr = D2D1::RectF(dotX + 6.f * m_dpiScale, y, tx - 4.f * m_dpiScale, y + badgeH);
            m_tfBadge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawTextW(st.c_str(), (UINT32)st.size(), m_tfBadge, tr, m_brush);
        }
    }
}

void HomePage::DrawNextReminderHero(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover) {
    DrawCard(rt, r, isHover);
    if (!m_brush) return;

    float pad = 14.f * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    bool isPaused = ds::Accent::IsAppPaused();

    time_t next = Scheduler::Instance().GetNextReminderTime();
    std::wstring timeStr = L"—";
    std::wstring inStr   = L"";

    if (isPaused) {
        timeStr = L"—";
        inStr = Localization::Instance().Get(L"home.paused");
    } else if (next > 0) {
        struct tm lt;
        localtime_s(&lt, &next);
        wchar_t tbuf[16];
        wcsftime(tbuf, 16, L"%I:%M %p", &lt);
        timeStr = tbuf;

        time_t now = time(nullptr);
        long long diff = (long long)(next - now);
        if (diff > 0) {
            long long mins = diff / 60;
            if (isRtl) {
                if (mins < 60) inStr = L"بعد " + std::to_wstring(mins) + L" دقيقة";
                else inStr = L"بعد " + std::to_wstring(mins/60) + L"س " + std::to_wstring(mins%60) + L"د";
            } else {
                if (mins < 60) inStr = L"In " + std::to_wstring(mins) + L"m";
                else inStr = L"In " + std::to_wstring(mins/60) + L"h " + std::to_wstring(mins%60) + L"m";
            }
        }
    }

    // Header Row: Card Title on one side, Countdown Pill on other side
    float hY = r.top + pad;
    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.next_reminder");
        float lblW = 120.f * m_dpiScale;
        D2D1_RECT_F lr = isRtl ? D2D1::RectF(r.right - pad - lblW, hY, r.right - pad, hY + 16.f * m_dpiScale)
                              : D2D1::RectF(r.left + pad, hY, r.left + pad + lblW, hY + 16.f * m_dpiScale);
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, lr, m_brush);
    }

    // Countdown Badge Pill
    if (!inStr.empty() && m_tfBadge) {
        float pillW = 100.f * m_dpiScale;
        float pillH = 20.f * m_dpiScale;
        float px = isRtl ? (r.left + pad) : (r.right - pad - pillW);
        D2D1_RECT_F pillR = D2D1::RectF(px, hY - 2.f * m_dpiScale, px + pillW, hY - 2.f * m_dpiScale + pillH);

        m_brush->SetColor(ds::ActiveNavBg());
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillR, pillH / 2.f, pillH / 2.f), m_brush);
        m_brush->SetColor(ds::Accent::Primary(0.4f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(pillR, pillH / 2.f, pillH / 2.f), m_brush, 0.75f);

        m_brush->SetColor(ds::Accent::Primary());
        rt->DrawTextW(inStr.c_str(), (UINT32)inStr.size(), m_tfBadge, pillR, m_brush);
    }

    // Core Time Display
    float timeY = hY + 20.f * m_dpiScale;
    m_brush->SetColor(ds::TextPrimary());
    if (m_tfBigNum) {
        m_tfBigNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F tr = D2D1::RectF(r.left + pad, timeY, r.right - pad, timeY + 34.f * m_dpiScale);
        rt->DrawTextW(timeStr.c_str(), (UINT32)timeStr.size(), m_tfBigNum, tr, m_brush);
    }

    // Bottom Row: Upcoming Dhikr Preview snippet + Trigger Alert Button
    float botY = timeY + 34.f * m_dpiScale;
    float btnW = 90.f * m_dpiScale;
    float btnH = 24.f * m_dpiScale;

    float btnX = isRtl ? (r.left + pad) : (r.right - pad - btnW);
    m_btnTriggerRect = D2D1::RectF(btnX, botY, btnX + btnW, botY + btnH);

    // Quick Test Alert Button
    bool isTriggered = (m_triggerFeedbackTime != 0 && GetTickCount() - m_triggerFeedbackTime < 2000);
    bool isBtnHover = (m_hoveredCard == 6);

    D2D1_ROUNDED_RECT brr = D2D1::RoundedRect(m_btnTriggerRect, 4.f * m_dpiScale, 4.f * m_dpiScale);
    if (isTriggered) {
        m_brush->SetColor(ds::Accent::Primary());
        rt->FillRoundedRectangle(brr, m_brush);
        m_brush->SetColor(ds::Rgba(0x08, 0x14, 0x0A));
    } else {
        m_brush->SetColor(isBtnHover ? ds::CardHover() : ds::Surface());
        rt->FillRoundedRectangle(brr, m_brush);
        m_brush->SetColor(isBtnHover ? ds::Accent::Primary(0.8f) : ds::BorderSubtle());
        rt->DrawRoundedRectangle(brr, m_brush, 0.75f);
        m_brush->SetColor(isBtnHover ? ds::Accent::Primary() : ds::TextSecondary());
    }

    std::wstring btnLabel = isTriggered ? Localization::Instance().Get(L"home.sent")
                                        : Localization::Instance().Get(L"home.quick_reminder");
    if (m_tfBtn) {
        rt->DrawTextW(btnLabel.c_str(), (UINT32)btnLabel.size(), m_tfBtn, m_btnTriggerRect, m_brush);
    }

    // Preview Quote
    const ContentItem* previewItem = ContentEngine::Instance().GetNextPreview();
    if (previewItem && !previewItem->text.empty() && m_tfSmall) {
        m_brush->SetColor(isPaused ? ds::TextSecondary() : ds::Accent::Primary(0.80f));
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_tfSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        std::wstring snippet = previewItem->text.substr(0, 36);
        if (previewItem->text.size() > 36) snippet += L"...";
        std::wstring preview = L"« " + snippet + L" »";

        float prevW = (r.right - r.left) - pad * 2.f - btnW - 8.f * m_dpiScale;
        D2D1_RECT_F prevR = isRtl ? D2D1::RectF(btnX + btnW + 8.f * m_dpiScale, botY, r.right - pad, botY + btnH)
                                  : D2D1::RectF(r.left + pad, botY, btnX - 8.f * m_dpiScale, botY + btnH);
        rt->DrawTextW(preview.c_str(), (UINT32)preview.size(), m_tfSmall, prevR, m_brush);
    }
}

void HomePage::DrawTodayGoalCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover) {
    DrawCard(rt, r, isHover);
    if (!m_brush) return;

    float pad = 14.f * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    int delivered = m_todayDelivered;
    auto ds_ref = Settings::Instance().GetCopy();
    int dow = 0;
    { struct tm lt; time_t now = time(nullptr); localtime_s(&lt, &now); dow = lt.tm_wday; }
    auto& daysch = ds_ref.weeklySchedule[dow];
    int windowMin = (daysch.endHour * 60 + daysch.endMin) - (daysch.startHour * 60 + daysch.startMin);
    int goal = (windowMin > 0 && daysch.frequencyMinutes > 0) ? windowMin / daysch.frequencyMinutes : 20;
    goal = std::max(1, goal);
    float progress = std::min(1.f, (float)delivered / (float)goal);
    int pct = (int)(progress * 100.f);

    // Header Row: Card Title on one side, % Completed Pill on other side
    float hY = r.top + pad;
    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.todays_reminders");
        float lblW = 120.f * m_dpiScale;
        D2D1_RECT_F lr = isRtl ? D2D1::RectF(r.right - pad - lblW, hY, r.right - pad, hY + 16.f * m_dpiScale)
                              : D2D1::RectF(r.left + pad, hY, r.left + pad + lblW, hY + 16.f * m_dpiScale);
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, lr, m_brush);
    }

    // % Completed Badge Pill
    if (m_tfBadge) {
        float pillW = 86.f * m_dpiScale;
        float pillH = 20.f * m_dpiScale;
        float px = isRtl ? (r.left + pad) : (r.right - pad - pillW);
        D2D1_RECT_F pillR = D2D1::RectF(px, hY - 2.f * m_dpiScale, px + pillW, hY - 2.f * m_dpiScale + pillH);

        m_brush->SetColor(ds::ActiveNavBg());
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillR, pillH / 2.f, pillH / 2.f), m_brush);
        m_brush->SetColor(ds::Accent::Primary(0.4f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(pillR, pillH / 2.f, pillH / 2.f), m_brush, 0.75f);

        std::wstring pctStr = std::to_wstring(pct) + L"% " + Localization::Instance().Get(L"home.completed");
        m_brush->SetColor(ds::Accent::Primary());
        rt->DrawTextW(pctStr.c_str(), (UINT32)pctStr.size(), m_tfBadge, pillR, m_brush);
    }

    // Core Delivered Fraction
    float statY = hY + 20.f * m_dpiScale;
    m_brush->SetColor(ds::TextPrimary());
    std::wstring countStr = std::to_wstring(delivered) + (isRtl ? L" \\ " : L" / ") + std::to_wstring(goal);
    if (m_tfBigNum) {
        m_tfBigNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        D2D1_RECT_F cr = D2D1::RectF(r.left + pad, statY, r.right - pad, statY + 34.f * m_dpiScale);
        rt->DrawTextW(countStr.c_str(), (UINT32)countStr.size(), m_tfBigNum, cr, m_brush);
    }

    // Modern Rounded Progress Bar
    float barY = statY + 36.f * m_dpiScale;
    float barH = 6.f * m_dpiScale;
    D2D1_RECT_F barR = D2D1::RectF(r.left + pad, barY, r.right - pad, barY + barH);
    DrawProgressBar(rt, barR, progress);

    // Subtitle under progress bar
    float subY = barY + barH + 4.f * m_dpiScale;
    if (m_tfSmall) {
        m_brush->SetColor(ds::TextMuted());
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring subTarget = Localization::Instance().Get(L"home.daily_target") + std::to_wstring(goal);
        D2D1_RECT_F subR = D2D1::RectF(r.left + pad, subY, r.right - pad, subY + 14.f * m_dpiScale);
        rt->DrawTextW(subTarget.c_str(), (UINT32)subTarget.size(), m_tfSmall, subR, m_brush);
    }
}

void HomePage::DrawProgressBar(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, float progress) {
    float rx = (r.bottom - r.top) / 2.f;
    bool isRtl = Localization::Instance().IsRTL();

    // Track
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush);
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(r, rx, rx), m_brush, 0.5f);

    // Fill
    if (progress > 0.01f) {
        float totalW = r.right - r.left;
        float fillW = totalW * progress;
        D2D1_RECT_F fill = isRtl ? D2D1::RectF(r.right - fillW, r.top, r.right, r.bottom)
                                 : D2D1::RectF(r.left, r.top, r.left + fillW, r.bottom);

        m_brush->SetColor(ds::Accent::Primary());
        rt->FillRoundedRectangle(D2D1::RoundedRect(fill, rx, rx), m_brush);
    }
}

void HomePage::DrawFlameGraphic(ID2D1RenderTarget* rt, float cx, float cy, float w, float h) {
    // Ambient radial glow behind the flame
    float glowR = w * 1.5f;
    D2D1_GRADIENT_STOP stops[2] = {
        { 0.0f, ds::Rgba(0xFF, 0x8A, 0x00, 0.28f) },
        { 1.0f, ds::Rgba(0xFF, 0x8A, 0x00, 0.0f)  }
    };
    ID2D1GradientStopCollection* pStops = nullptr;
    if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pStops))) {
        ID2D1RadialGradientBrush* pRadBrush = nullptr;
        D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props = D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(cx, cy), D2D1::Point2F(0, 0), glowR, glowR);
        if (SUCCEEDED(rt->CreateRadialGradientBrush(props, pStops, &pRadBrush))) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), glowR, glowR), pRadBrush);
            pRadBrush->Release();
        }
        pStops->Release();
    }

    // Outer warm flame
    ID2D1PathGeometry* outerFlame = nullptr;
    if (m_d2dFactory_ && SUCCEEDED(m_d2dFactory_->CreatePathGeometry(&outerFlame))) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(outerFlame->Open(&sink))) {
            float topY = cy - h * 0.5f;
            float botY = cy + h * 0.5f;

            sink->BeginFigure(D2D1::Point2F(cx, topY), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(cx + w * 0.45f, topY + h * 0.35f),
                D2D1::Point2F(cx + w * 0.55f, botY - h * 0.25f),
                D2D1::Point2F(cx + w * 0.3f, botY)));
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(cx, botY + h * 0.05f),
                D2D1::Point2F(cx - w * 0.3f, botY),
                D2D1::Point2F(cx - w * 0.3f, botY)));
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(cx - w * 0.55f, botY - h * 0.25f),
                D2D1::Point2F(cx - w * 0.45f, topY + h * 0.35f),
                D2D1::Point2F(cx, topY)));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();

            m_brush->SetColor(ds::Rgba(0xFF, 0x7A, 0x00));
            rt->FillGeometry(outerFlame, m_brush);
        }
        outerFlame->Release();
    }

    // Inner glowing golden ember core
    ID2D1PathGeometry* innerCore = nullptr;
    if (m_d2dFactory_ && SUCCEEDED(m_d2dFactory_->CreatePathGeometry(&innerCore))) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(innerCore->Open(&sink))) {
            float iw = w * 0.5f;
            float ih = h * 0.5f;
            float itopY = cy - ih * 0.2f;
            float ibotY = cy + ih * 0.9f;

            sink->BeginFigure(D2D1::Point2F(cx, itopY), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(cx + iw * 0.45f, itopY + ih * 0.35f),
                D2D1::Point2F(cx + iw * 0.4f, ibotY - ih * 0.2f),
                D2D1::Point2F(cx, ibotY)));
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(cx - iw * 0.4f, ibotY - ih * 0.2f),
                D2D1::Point2F(cx - iw * 0.45f, itopY + ih * 0.35f),
                D2D1::Point2F(cx, itopY)));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();

            m_brush->SetColor(ds::Rgba(0xFF, 0xD0, 0x20));
            rt->FillGeometry(innerCore, m_brush);
        }
        innerCore->Release();
    }
}

void HomePage::DrawStreakCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover) {
    DrawCard(rt, r, isHover);
    if (!m_brush) return;

    float pad = 14.f * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    // Header Row: Card Title on one side, Star Badge on other side
    float hY = r.top + pad;
    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.current_streak");
        float lblW = 120.f * m_dpiScale;
        D2D1_RECT_F lr = isRtl ? D2D1::RectF(r.right - pad - lblW, hY, r.right - pad, hY + 16.f * m_dpiScale)
                              : D2D1::RectF(r.left + pad, hY, r.left + pad + lblW, hY + 16.f * m_dpiScale);
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, lr, m_brush);
    }

    // Badge Pill
    if (m_tfBadge) {
        float pillW = 68.f * m_dpiScale;
        float pillH = 20.f * m_dpiScale;
        float px = isRtl ? (r.left + pad) : (r.right - pad - pillW);
        D2D1_RECT_F pillR = D2D1::RectF(px, hY - 2.f * m_dpiScale, px + pillW, hY - 2.f * m_dpiScale + pillH);

        m_brush->SetColor(ds::Rgba(0x28, 0x1E, 0x0E));
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillR, pillH / 2.f, pillH / 2.f), m_brush);
        m_brush->SetColor(ds::Rgba(0xFF, 0x95, 0x00, 0.4f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(pillR, pillH / 2.f, pillH / 2.f), m_brush, 0.75f);

        std::wstring badgeStr = isRtl ? L"✦ مستمر" : L"✦ Active";
        m_brush->SetColor(ds::Rgba(0xFF, 0xB0, 0x20));
        rt->DrawTextW(badgeStr.c_str(), (UINT32)badgeStr.size(), m_tfBadge, pillR, m_brush);
    }

    // Core Streak Number
    float statY = hY + 20.f * m_dpiScale;
    m_brush->SetColor(ds::TextPrimary());
    std::wstring streakStr = std::to_wstring(m_streak);
    if (m_tfBigNum) {
        m_tfBigNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        float numW = 60.f * m_dpiScale;
        D2D1_RECT_F nr = isRtl ? D2D1::RectF(r.right - pad - numW, statY, r.right - pad, statY + 34.f * m_dpiScale)
                              : D2D1::RectF(r.left + pad, statY, r.left + pad + numW, statY + 34.f * m_dpiScale);
        rt->DrawTextW(streakStr.c_str(), (UINT32)streakStr.size(), m_tfBigNum, nr, m_brush);

        // Days label beside streak number
        if (m_tfSub) {
            m_brush->SetColor(ds::TextSecondary());
            std::wstring daysLbl = Localization::Instance().Get(L"home.days");
            D2D1_RECT_F dr = isRtl ? D2D1::RectF(r.right - pad - numW - 70.f * m_dpiScale, statY + 6.f * m_dpiScale, r.right - pad - numW - 4.f * m_dpiScale, statY + 30.f * m_dpiScale)
                                  : D2D1::RectF(r.left + pad + numW + 6.f * m_dpiScale, statY + 6.f * m_dpiScale, r.left + pad + numW + 70.f * m_dpiScale, statY + 30.f * m_dpiScale);
            rt->DrawTextW(daysLbl.c_str(), (UINT32)daysLbl.size(), m_tfSub, dr, m_brush);
        }
    }

    // Flame graphic on opposite side
    float flameW = 28.f * m_dpiScale;
    float flameH = 36.f * m_dpiScale;
    float flameCx = isRtl ? (r.left + pad + flameW / 2.f + 10.f * m_dpiScale)
                          : (r.right - pad - flameW / 2.f - 10.f * m_dpiScale);
    float flameCy = statY + 16.f * m_dpiScale;
    DrawFlameGraphic(rt, flameCx, flameCy, flameW, flameH);

    // Motivational micro-caption
    float subY = statY + 36.f * m_dpiScale;
    if (m_tfSmall) {
        m_brush->SetColor(ds::TextMuted());
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring motto = Localization::Instance().Get(L"home.streak_motto");
        D2D1_RECT_F mr = D2D1::RectF(r.left + pad, subY, r.right - pad, subY + 14.f * m_dpiScale);
        rt->DrawTextW(motto.c_str(), (UINT32)motto.size(), m_tfSmall, mr, m_brush);
    }
}

void HomePage::DrawTotalCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover) {
    DrawCard(rt, r, isHover);
    if (!m_brush) return;

    float pad = 14.f * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    // Header Row: Card Title on one side, Range Badge on other side
    float hY = r.top + pad;
    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.total_reminders");
        float lblW = 120.f * m_dpiScale;
        D2D1_RECT_F lr = isRtl ? D2D1::RectF(r.right - pad - lblW, hY, r.right - pad, hY + 16.f * m_dpiScale)
                              : D2D1::RectF(r.left + pad, hY, r.left + pad + lblW, hY + 16.f * m_dpiScale);
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, lr, m_brush);
    }

    // Range Pill
    if (m_tfBadge) {
        float pillW = 80.f * m_dpiScale;
        float pillH = 20.f * m_dpiScale;
        float px = isRtl ? (r.left + pad) : (r.right - pad - pillW);
        D2D1_RECT_F pillR = D2D1::RectF(px, hY - 2.f * m_dpiScale, px + pillW, hY - 2.f * m_dpiScale + pillH);

        m_brush->SetColor(ds::ActiveNavBg());
        rt->FillRoundedRectangle(D2D1::RoundedRect(pillR, pillH / 2.f, pillH / 2.f), m_brush);
        m_brush->SetColor(ds::Accent::Primary(0.4f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(pillR, pillH / 2.f, pillH / 2.f), m_brush, 0.75f);

        std::wstring rangeStr = Localization::Instance().Get(L"home.this_week");
        m_brush->SetColor(ds::Accent::Primary());
        rt->DrawTextW(rangeStr.c_str(), (UINT32)rangeStr.size(), m_tfBadge, pillR, m_brush);
    }

    // Core Total Number
    float statY = hY + 20.f * m_dpiScale;
    m_brush->SetColor(ds::TextPrimary());
    std::wstring totalStr = std::to_wstring(m_total);
    if (m_tfBigNum) {
        m_tfBigNum->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        float numW = 70.f * m_dpiScale;
        D2D1_RECT_F nr = isRtl ? D2D1::RectF(r.right - pad - numW, statY, r.right - pad, statY + 34.f * m_dpiScale)
                              : D2D1::RectF(r.left + pad, statY, r.left + pad + numW, statY + 34.f * m_dpiScale);
        rt->DrawTextW(totalStr.c_str(), (UINT32)totalStr.size(), m_tfBigNum, nr, m_brush);

        // Subtitle beside total number
        if (m_tfSmall) {
            m_brush->SetColor(ds::TextSecondary());
            std::wstring thisWk = Localization::Instance().Get(L"home.this_week");
            D2D1_RECT_F dr = isRtl ? D2D1::RectF(r.right - pad - numW - 80.f * m_dpiScale, statY + 10.f * m_dpiScale, r.right - pad - numW - 4.f * m_dpiScale, statY + 28.f * m_dpiScale)
                                  : D2D1::RectF(r.left + pad + numW + 6.f * m_dpiScale, statY + 10.f * m_dpiScale, r.left + pad + numW + 80.f * m_dpiScale, statY + 28.f * m_dpiScale);
            rt->DrawTextW(thisWk.c_str(), (UINT32)thisWk.size(), m_tfSmall, dr, m_brush);
        }
    }

    // 7-day mini bar chart on opposite side
    float chartW = 74.f * m_dpiScale;
    float chartH = 34.f * m_dpiScale;
    float chartX = isRtl ? (r.left + pad) : (r.right - pad - chartW);
    float chartY = statY + 2.f * m_dpiScale;
    DrawMiniBarChart(rt, D2D1::RectF(chartX, chartY, chartX + chartW, chartY + chartH));

    // Sub-caption
    float subY = statY + 36.f * m_dpiScale;
    if (m_tfSmall) {
        m_brush->SetColor(ds::TextMuted());
        m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring praise = isRtl ? L"تقبل الله طاعتكم ورفع درجاتكم" : L"May Allah accept your good deeds";
        D2D1_RECT_F pr = D2D1::RectF(r.left + pad, subY, r.right - pad, subY + 14.f * m_dpiScale);
        rt->DrawTextW(praise.c_str(), (UINT32)praise.size(), m_tfSmall, pr, m_brush);
    }
}

void HomePage::DrawMiniBarChart(ID2D1RenderTarget* rt, const D2D1_RECT_F& r) {
    auto stats = m_dailyStats;

    int maxVal = 1;
    std::vector<int> vals;
    for (auto& s : stats) { vals.push_back(s.delivered); maxVal = std::max(maxVal, s.delivered); }
    while ((int)vals.size() < 7) vals.push_back(0);
    std::reverse(vals.begin(), vals.end());

    float chartH = r.bottom - r.top - 10.f * m_dpiScale;
    float chartW = r.right  - r.left;
    float gap    = chartW / 7.f;
    float barW   = gap * 0.55f;
    bool isRtl   = Localization::Instance().IsRTL();

    static const wchar_t* kDaysAr[7] = { L"س", L"ح", L"ن", L"ث", L"ر", L"خ", L"ج" };
    static const wchar_t* kDaysEn[7] = { L"S", L"M", L"T", L"W", L"T", L"F", L"S" };

    for (int i = 0; i < 7; ++i) {
        float normH = (float)vals[i] / (float)maxVal;
        float bh    = std::max(3.f * m_dpiScale, normH * chartH);
        float bx    = r.left + gap * i + (gap - barW) / 2.f;
        float by    = r.top + chartH - bh;

        bool isToday = (i == 6);
        m_brush->SetColor(isToday ? ds::Accent::Primary() : ds::Accent::Primary(0.28f));
        D2D1_RECT_F bar = D2D1::RectF(bx, by, bx + barW, r.top + chartH);
        rt->FillRoundedRectangle(D2D1::RoundedRect(bar, 2.f * m_dpiScale, 2.f * m_dpiScale), m_brush);

        // Day letter below each bar
        if (m_tfSmall) {
            m_brush->SetColor(isToday ? ds::Accent::Primary() : ds::TextMuted());
            m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            D2D1_RECT_F lr = D2D1::RectF(bx - 2.f * m_dpiScale, r.top + chartH + 1.f * m_dpiScale, bx + barW + 2.f * m_dpiScale, r.bottom);
            const wchar_t* dStr = isRtl ? kDaysAr[i] : kDaysEn[i];
            rt->DrawTextW(dStr, 1, m_tfSmall, lr, m_brush);
        }
    }
}

void HomePage::DrawProgressTimeline(ID2D1RenderTarget* rt, const D2D1_RECT_F& area, bool isHover) {
    DrawCard(rt, area, isHover);
    if (!m_brush) return;

    float pad = 14.f * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    // Header Row: Timeline Title on one side, Schedule Window on other side
    float hY = area.top + pad;
    m_brush->SetColor(ds::TextSecondary());
    if (m_tfLabel) {
        m_tfLabel->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring lbl = Localization::Instance().Get(L"home.journey_title");
        float lblW = 160.f * m_dpiScale;
        D2D1_RECT_F lr = isRtl ? D2D1::RectF(area.right - pad - lblW, hY, area.right - pad, hY + 16.f * m_dpiScale)
                              : D2D1::RectF(area.left + pad, hY, area.left + pad + lblW, hY + 16.f * m_dpiScale);
        rt->DrawTextW(lbl.c_str(), (UINT32)lbl.size(), m_tfLabel, lr, m_brush);
    }

    // Schedule window indicator
    struct tm lt; time_t now = time(nullptr); localtime_s(&lt, &now);
    auto s = Settings::Instance().GetCopy();
    int dow = lt.tm_wday;
    auto& ds_ref = s.weeklySchedule[dow];
    int startMin = ds_ref.startHour * 60 + ds_ref.startMin;
    int endMin   = ds_ref.endHour   * 60 + ds_ref.endMin;
    int nowMin   = lt.tm_hour * 60 + lt.tm_min;
    int totalMin = std::max(1, endMin - startMin);

    if (m_tfSmall) {
        m_brush->SetColor(ds::TextMuted());
        std::wstring winStr = isRtl ? L"الفترة اليومية: 8:00 ص — 11:00 م" : L"Daily Window: 8:00 AM — 11:00 PM";
        float winW = 200.f * m_dpiScale;
        D2D1_RECT_F wr = isRtl ? D2D1::RectF(area.left + pad, hY, area.left + pad + winW, hY + 16.f * m_dpiScale)
                              : D2D1::RectF(area.right - pad - winW, hY, area.right - pad, hY + 16.f * m_dpiScale);
        m_tfSmall->SetTextAlignment(isRtl ? DWRITE_TEXT_ALIGNMENT_LEADING : DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawTextW(winStr.c_str(), (UINT32)winStr.size(), m_tfSmall, wr, m_brush);
    }

    // Track Geometry
    float lineY = area.top + 46.f * m_dpiScale;
    float lineLeft  = area.left + pad + 24.f * m_dpiScale;
    float lineRight = area.right - pad - 24.f * m_dpiScale;
    float lineW     = lineRight - lineLeft;

    // Dark base rail
    m_brush->SetColor(ds::Surface());
    rt->DrawLine(D2D1::Point2F(lineLeft, lineY), D2D1::Point2F(lineRight, lineY), m_brush, 3.f * m_dpiScale);
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawLine(D2D1::Point2F(lineLeft, lineY), D2D1::Point2F(lineRight, lineY), m_brush, 1.f * m_dpiScale);

    // Dynamic filled progress line
    float dayProgress = std::clamp((float)(nowMin - startMin) / (float)totalMin, 0.0f, 1.0f);
    if (dayProgress > 0.01f) {
        float curX = isRtl ? (lineRight - lineW * dayProgress) : (lineLeft + lineW * dayProgress);
        m_brush->SetColor(ds::Accent::Primary());
        D2D1_POINT_2F p1 = isRtl ? D2D1::Point2F(lineRight, lineY) : D2D1::Point2F(lineLeft, lineY);
        D2D1_POINT_2F p2 = D2D1::Point2F(curX, lineY);
        rt->DrawLine(p1, p2, m_brush, 3.f * m_dpiScale);
    }

    // 5 Key Day Milestones
    constexpr int kCheckpoints = 5;
    float dotR = 5.f * m_dpiScale;

    static const wchar_t* kLabelsAr[5] = { L"الصباح", L"الضحى", L"الظهيرة", L"العصر", L"المساء" };
    static const wchar_t* kTimesAr[5]  = { L"8:00 ص", L"11:00 ص", L"2:00 م", L"5:30 م", L"9:00 م" };
    static const wchar_t* kLabelsEn[5] = { L"Morning", L"Forenoon", L"Noon", L"Afternoon", L"Evening" };
    static const wchar_t* kTimesEn[5]  = { L"8:00 AM", L"11:00 AM", L"2:00 PM", L"5:30 PM", L"9:00 PM" };
    static const int kMinCheck[5]      = { 480, 660, 840, 1050, 1260 };

    for (int i = 0; i < kCheckpoints; ++i) {
        float t  = (float)i / (float)(kCheckpoints - 1);
        float cx = isRtl ? (lineRight - lineW * t) : (lineLeft + lineW * t);
        float cy = lineY;

        bool passed  = nowMin >= kMinCheck[i];
        bool current = !passed && (i == 0 || nowMin >= kMinCheck[i - 1]);

        if (passed) {
            // Completed node
            m_brush->SetColor(ds::Accent::Primary());
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR, dotR), m_brush);
            m_brush->SetColor(ds::Accent::IsAppPaused() ? ds::Rgba(0x14, 0x17, 0x15) : ds::Rgba(0x06, 0x10, 0x08));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR * 0.45f, dotR * 0.45f), m_brush);
        } else if (current) {
            // Radiant pulse ring on active current position
            m_brush->SetColor(ds::Accent::Primary(0.25f));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR * 1.8f, dotR * 1.8f), m_brush);
            m_brush->SetColor(ds::Accent::Primary());
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR * 1.2f, dotR * 1.2f), m_brush, 1.5f);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR * 0.5f, dotR * 0.5f), m_brush);
        } else {
            // Upcoming node
            m_brush->SetColor(ds::Surface());
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR, dotR), m_brush);
            m_brush->SetColor(ds::BorderSubtle());
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotR, dotR), m_brush, 1.f);
        }

        // Labels under node
        float txtW = 60.f * m_dpiScale;
        D2D1_RECT_F titleR = D2D1::RectF(cx - txtW / 2.f, cy + 8.f * m_dpiScale, cx + txtW / 2.f, cy + 22.f * m_dpiScale);
        D2D1_RECT_F timeR  = D2D1::RectF(cx - txtW / 2.f, cy + 22.f * m_dpiScale, cx + txtW / 2.f, cy + 34.f * m_dpiScale);

        if (m_tfSmall) {
            m_tfSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_brush->SetColor(passed ? ds::TextPrimary() : (current ? ds::Accent::Primary() : ds::TextMuted()));
            const wchar_t* lStr = isRtl ? kLabelsAr[i] : kLabelsEn[i];
            rt->DrawTextW(lStr, (UINT32)wcslen(lStr), m_tfSmall, titleR, m_brush);

            m_brush->SetColor(ds::TextMuted());
            const wchar_t* tmStr = isRtl ? kTimesAr[i] : kTimesEn[i];
            rt->DrawTextW(tmStr, (UINT32)wcslen(tmStr), m_tfSmall, timeR, m_brush);
        }
    }
}

bool HomePage::HitTestInteractive(int x, int y) {
    if (x >= m_statusBadgeRect.left && x <= m_statusBadgeRect.right &&
        y >= m_statusBadgeRect.top  && y <= m_statusBadgeRect.bottom) {
        return true;
    }
    if (x >= m_btnTriggerRect.left && x <= m_btnTriggerRect.right &&
        y >= m_btnTriggerRect.top  && y <= m_btnTriggerRect.bottom) {
        return true;
    }
    return false;
}

bool HomePage::OnMouseMove(int x, int y) {
    int oldHover = m_hoveredCard;

    if (x >= m_btnTriggerRect.left && x <= m_btnTriggerRect.right &&
        y >= m_btnTriggerRect.top  && y <= m_btnTriggerRect.bottom) {
        m_hoveredCard = 6;
    } else {
        m_hoveredCard = 0;
    }

    if (oldHover != m_hoveredCard) {
        MainWindow::Instance().Invalidate();
        return true;
    }
    return false;
}

void HomePage::OnMouseLeave() {
    if (m_hoveredCard != 0) {
        m_hoveredCard = 0;
        MainWindow::Instance().Invalidate();
    }
}

bool HomePage::OnLButtonDown(int x, int y) {
    // 1. Status Badge toggle
    if (x >= m_statusBadgeRect.left && x <= m_statusBadgeRect.right &&
        y >= m_statusBadgeRect.top  && y <= m_statusBadgeRect.bottom) {
        if (m_masterToggle && m_masterToggle->OnClick) {
            m_masterToggle->OnClick();
            return true;
        }
    }

    // 2. Quick Test Alert Button
    if (x >= m_btnTriggerRect.left && x <= m_btnTriggerRect.right &&
        y >= m_btnTriggerRect.top  && y <= m_btnTriggerRect.bottom) {
        Scheduler::Instance().TriggerNow();
        m_triggerFeedbackTime = GetTickCount();
        MainWindow::Instance().Invalidate();
        return true;
    }

    return false;
}

} // namespace dhikr
