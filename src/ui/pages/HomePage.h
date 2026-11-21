#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <memory>
#include "../MainWindow.h"
#include "../controls/BasicControls.h"
#include "../../core/Database.h"

namespace dhikr {

// ── HomePage ─────────────────────────────────────────────────────────────────
// Displays: greeting, active status, 4-card 2x2 dashboard grid, and today's journey timeline.
// Redesigned to match the obsidian dark theme and the modern layout from dhachbord.png.
class HomePage : public PageBase {
public:
    HomePage();
    ~HomePage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    void ResetD2D() override {
        if (m_brush) { m_brush->Release(); m_brush = nullptr; }
        if (m_d2dFactory_) { m_d2dFactory_->Release(); m_d2dFactory_ = nullptr; }
    }
    bool OnLButtonDown(int x, int y) override;
    bool OnMouseMove(int x, int y) override;
    void OnMouseLeave() override;
    void OnSettingsChanged() override;
    void OnActivated() override;
    bool HitTestInteractive(int x, int y) override;

    friend class ScreenshotExporter;

private:
    std::shared_ptr<ToggleControl> m_masterToggle;
    void DrawGreeting(ID2D1RenderTarget* rt, const D2D1_RECT_F& area);
    void DrawStatusBadge(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawNextReminderHero(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover);
    void DrawTodayGoalCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover);
    void DrawStreakCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover);
    void DrawTotalCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover);
    void DrawProgressTimeline(ID2D1RenderTarget* rt, const D2D1_RECT_F& area, bool isHover);
    void DrawCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isHover);
    void DrawMiniBarChart(ID2D1RenderTarget* rt, const D2D1_RECT_F& r);
    void DrawProgressBar(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, float progress);
    void DrawFlameGraphic(ID2D1RenderTarget* rt, float cx, float cy, float w, float h);

    float m_dpiScale = 1.f;
    bool  m_initialized = false;
    
    int   m_hoveredCard = 0; // 0: None, 1: Next, 2: Today, 3: Streak, 4: Total, 5: Timeline, 6: TriggerBtn
    D2D1_RECT_F m_statusBadgeRect = {0,0,0,0};
    D2D1_RECT_F m_btnTriggerRect  = {0,0,0,0};
    DWORD m_triggerFeedbackTime   = 0;

    int m_todayDelivered = -1;
    int m_streak = -1;
    int m_total = -1;
    std::vector<dhikr::DailyStat> m_dailyStats;

    IDWriteFactory*    m_dwf       = nullptr;
    IDWriteTextFormat* m_tfGreet   = nullptr; // large greeting (20pt)
    IDWriteTextFormat* m_tfSub     = nullptr; // secondary text (11.5pt)
    IDWriteTextFormat* m_tfBigNum  = nullptr; // big stat numbers (24pt)
    IDWriteTextFormat* m_tfMedNum  = nullptr; // medium numbers (17pt)
    IDWriteTextFormat* m_tfLabel   = nullptr; // card labels (10.5pt)
    IDWriteTextFormat* m_tfSmall   = nullptr; // small meta text (9.5pt)
    IDWriteTextFormat* m_tfTime    = nullptr; // time display
    IDWriteTextFormat* m_tfBadge   = nullptr; // pill/badge text (9.5pt SemiBold)
    IDWriteTextFormat* m_tfBtn     = nullptr; // button text (10pt SemiBold)

    ID2D1SolidColorBrush* m_brush  = nullptr;
    static ID2D1Factory* m_d2dFactory_;
};

} // namespace dhikr
