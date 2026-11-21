#pragma once
#include "../MainWindow.h"
#include "../PopupWindow.h"
#include "../components/DropdownPopup.h"
#include "../../core/Logger.h"
#include <vector>

namespace dhikr {

class SettingsPage : public PageBase {
public:
    SettingsPage();
    ~SettingsPage();

    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnLButtonUp(int x, int y) override;
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
    void DrawSectionHeader(ID2D1RenderTarget* rt, float x, float& y, float w, const wchar_t* locKey, const wchar_t* icon, float bodyH = 0.f);
    void DrawToggle(ID2D1RenderTarget* rt, float x, float y, bool on, bool hover = false);
    void DrawLabel(ID2D1RenderTarget* rt, float x, float y, const wchar_t* text, float controlReserveW = 200.f);
    void DrawDropdown(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* val, bool hover = false);
    void DrawButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* label, bool accent, bool hover = false, bool pressed = false);
    void DrawIconButton(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* iconGlyph, bool hover = false, bool pressed = false);
    void DrawButtonWithIcon(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, const wchar_t* iconGlyph, const wchar_t* label, bool accent, bool hover = false, bool pressed = false);
    void DrawSlider(ID2D1RenderTarget* rt, float x, float y, float w, float value);
    void DrawPositionGrid(ID2D1RenderTarget* rt, float x, float y);
    void DrawColorThemeSelector(ID2D1RenderTarget* rt, float x, float y, float w);
    void CommitSettings(bool notifyLanguage = false);

    int   m_hoveredElement = -1;
    int   m_pressedElement = -1;
    bool  m_isDraggingVolume = false;
    float m_dpiScale    = 1.f;
    float m_contentX=0, m_contentY=0, m_contentW=0;
    float m_viewportH   = 0.f;
    float m_scrollY     = 0.f;
    float m_targetScrollY = 0.f;
    float m_maxScroll   = 0.f;
    bool  m_hoveredSave = false;

    std::vector<PopupWindow::MonitorInfo> m_cachedMonitors;
    AppSettings m_pendingSettings;
    bool m_hasUnsavedChanges = false;

    IDWriteFactory*    m_dwf       = nullptr;
    IDWriteTextFormat* m_tfTitle   = nullptr;
    IDWriteTextFormat* m_tfSection = nullptr;
    IDWriteTextFormat* m_tfLabel   = nullptr;
    IDWriteTextFormat* m_tfValue   = nullptr;
    IDWriteTextFormat* m_tfBtn     = nullptr;
    IDWriteTextFormat* m_tfIcon    = nullptr;
    IDWriteTextFormat* m_tfSectionIcon = nullptr;
    ID2D1SolidColorBrush* m_brush  = nullptr;
};

} // namespace dhikr
