#pragma once
#include "../MainWindow.h"
#include "../../core/Settings.h"

namespace dhikr {

class RemindersPage : public PageBase {
public:
    RemindersPage();
    ~RemindersPage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnLButtonUp(int x, int y);
    bool OnMouseMove(int x, int y) override;
    bool OnMouseWheel(int delta) override;
    void OnMouseLeave() override;
    bool HitTestInteractive(int x, int y) override;
    void OnSettingsChanged() override;
    void OnActivated() override;
    float GetScrollY() const override { return m_scrollY; }
    void SetScrollY(float y) override { m_scrollY = y; m_targetScrollY = y; }
    bool OnScrollTick() override;
    void ResetD2D() override { if (m_brush) { m_brush->Release(); m_brush = nullptr; } }

private:
    void DrawToggle(ID2D1RenderTarget* rt, float x, float y, bool on, bool hover);
    void DrawRowToggle(ID2D1RenderTarget* rt, float cx, float cy, bool on);
    void DrawSmallTimeField(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, int h, int m, bool enabled);
    void DrawSaveButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r);
    void DrawWeekRow(ID2D1RenderTarget* rt, int dayIndex, float x, float y, float w);

    D2D1_RECT_F GetToggleRect() const;
    D2D1_RECT_F GetSaveRect() const;
    D2D1_RECT_F GetPreviewRect() const;

    IDWriteFactory*       m_dwf     = nullptr;
    IDWriteTextFormat*    m_tfTitle = nullptr;
    IDWriteTextFormat*    m_tfSub   = nullptr;
    IDWriteTextFormat*    m_tfLabel = nullptr;
    IDWriteTextFormat*    m_tfValue = nullptr;
    IDWriteTextFormat*    m_tfBtn   = nullptr;
    IDWriteTextFormat*    m_tfDay   = nullptr;
    ID2D1SolidColorBrush* m_brush   = nullptr;

    float m_dpiScale = 1.f;
    float m_contentX = 0, m_contentY = 0, m_contentW = 0;
    float m_viewH = 0;
    float m_totalContentH = 0;
    float m_scrollY = 0.f;
    float m_targetScrollY = 0.f;

    bool  m_hoverToggle  = false;
    bool  m_hoverSave    = false;
    bool  m_hoverPreview = false;

    int   m_hoveredRow     = -1;
    // Interactive element codes:
    // 0-6: day toggle
    // 10-16: day start time
    // 20-26: day end time
    // 30-36: day frequency
    // 101: master start time picker
    // 102: master end time picker
    // 103: master frequency dropdown
    // 104: quiet hours dropdown
    // 105: pause dropdown
    int   m_hoveredElement = -1;
    int   m_pressedElement = -1;

    AppSettings m_pendingSettings;
    bool m_hasUnsavedChanges = false;
};

} // namespace dhikr
