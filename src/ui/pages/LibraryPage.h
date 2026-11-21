#pragma once
#include "../MainWindow.h"
namespace dhikr {
class LibraryPage : public PageBase {
public:
    LibraryPage();
    ~LibraryPage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnMouseMove(int x, int y) override;
    bool OnMouseWheel(int delta) override;
    void OnMouseLeave() override;
    bool OnChar(wchar_t ch) override;
    bool OnKeyDown(WPARAM key) override;
    bool HitTestInteractive(int x, int y) override;
    float GetScrollY() const override { return m_scrollY; }
    void SetScrollY(float y) override { m_scrollY = y; m_targetScrollY = y; }
    bool OnScrollTick() override;
    void ResetD2D() override { if (m_brush) { m_brush->Release(); m_brush = nullptr; } }
private:
    void DrawContentItem(ID2D1RenderTarget* rt, float x, float& y, float w, size_t index, bool hovered);
    void DrawEmptyState(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds);
    void DrawFilterPills(ID2D1RenderTarget* rt, float x, float y, float w);
    void DrawAddModal(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds);

    IDWriteFactory*    m_dwf    = nullptr;
    IDWriteTextFormat* m_tfTitle  = nullptr;
    IDWriteTextFormat* m_tfMain   = nullptr;
    IDWriteTextFormat* m_tfSub    = nullptr;
    IDWriteTextFormat* m_tfCat    = nullptr;
    IDWriteTextFormat* m_tfEmpty  = nullptr;
    IDWriteTextFormat* m_tfIcon   = nullptr;
    IDWriteTextFormat* m_tfIconSmall = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;
    float m_dpiScale = 1.f;
    int   m_hoveredItem = -1;
    int   m_hoveredBtn  = -1;
    bool  m_hoveredDelBtn = false;
    float m_scrollY = 0.f;
    float m_targetScrollY = 0.f;
    float m_maxScroll = 0.f;
    float m_contentX = 0.f;
    float m_contentY = 0.f;
    float m_contentW = 0.f;

    // Filter & Custom Dhikr Modal state
    int          m_currentFilter  = 0; // 0=All, 1=Morning/Evening, 2=Tasbih, 3=Custom
    bool         m_showAddModal   = false;
    std::wstring m_inputDhikrText = L"";
    int          m_selectedReps   = 1;  // 1, 3, 33, 100
    int          m_selectedCatIdx = 0;  // 0=General, 1=Morning/Evening, 2=Tasbih
};
} // namespace dhikr
