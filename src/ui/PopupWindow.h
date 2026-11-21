#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <memory>
#include <unordered_set>
#include "../core/ContentEngine.h"

namespace dhikr {

class ScreenshotExporter;

// ── Popup animation state ─────────────────────────────────────────────────────
enum class PopupAnimState { Hidden, Entering, Visible, Hovering, Closing };

// ── PopupWindow ───────────────────────────────────────────────────────────────
// Layered, topmost, non-activating popup notification.
// Matches the supplied design reference pixel-accurately.
class PopupWindow {
public:
    static PopupWindow& Instance();

    bool Create(HINSTANCE hInstance);

    // Show with given content item (force=true bypasses visibility checks for previews)
    void Show(const ContentItem* item, bool force = false);

    // Hide immediately
    void Hide();

    // Check if currently visible
    bool IsVisible() const { return m_state != PopupAnimState::Hidden; }

    // Window proc (static -> member dispatch)
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // Multi-monitor support
    struct MonitorInfo {
        int index = 0;
        HMONITOR hMon = nullptr;
        RECT rcWork {};
        RECT rcMonitor {};
        bool isPrimary = false;
        std::wstring name;
    };
    static std::vector<MonitorInfo> EnumerateMonitors();

    PopupWindow(const PopupWindow&) = delete;
    PopupWindow& operator=(const PopupWindow&) = delete;

private:
    friend class ScreenshotExporter;
    PopupWindow() = default;
    ~PopupWindow();

    // Window management
    void RegisterClass(HINSTANCE hInstance);
    bool CreateLayeredWindow(HINSTANCE hInstance);

    // Position calculation
    RECT CalculatePosition(int width, int height);
    RECT CalculatePositionForWorkArea(int width, int height, const RECT& work);
    HMONITOR GetTargetMonitor();

    // Rendering
    bool InitD2D();
    void ReleaseD2D();
    void Render(float animProgress);  // 0.0–1.0 for enter, 1.0–0.0 for exit
    void DrawBackground(ID2D1RenderTarget* rt, float alpha, float sweepT = 0.f);
    void DrawAccentBar(ID2D1RenderTarget* rt, float alpha);
    void DrawCrescentMark(ID2D1RenderTarget* rt, float alpha);
    void DrawContent(ID2D1RenderTarget* rt, float alpha);
    void DrawCounterPill(ID2D1RenderTarget* rt, float alpha);
    void DrawCloseButton(ID2D1RenderTarget* rt, float alpha);
    void DrawCopyButton(ID2D1RenderTarget* rt, float alpha);
    void DrawPauseButton(ID2D1RenderTarget* rt, float alpha);
    void DrawGrainTexture(ID2D1RenderTarget* rt, float alpha);
    void UpdateLayered(float alpha, float slideOffsetX = 0.f, float slideOffsetY = 0.f, float scaleX = 1.0f, float scaleY = 1.0f, float sweepT = 0.f);

    // Message handlers
    LRESULT OnTimer(UINT_PTR timerId);
    LRESULT OnMouseMove(HWND hwnd, int x, int y);
    LRESULT OnMouseLeave(HWND hwnd);
    LRESULT OnLButtonDown(HWND hwnd, int x, int y);
    LRESULT OnSetCursor(HWND hwnd, UINT hitTest, UINT message);
    LRESULT OnDpiChanged(HWND hwnd, UINT newDpi, const RECT* newRect);

    // Helpers
    bool HitTestClose(int x, int y) const;
    bool HitTestCopy(int x, int y) const;
    bool HitTestPause(int x, int y) const;
    bool HitTestCounter(int x, int y) const;
    void StartEnterAnim();
    void StartExitAnim();
    void SetupAutoDismiss();
    void CancelAutoDismiss();
    float ComputeContentHeight(const ContentItem& item);
    void CleanupCachedDC();

    // ── State ──────────────────────────────────────────────────────────────
    HWND              m_hwnd        = nullptr;
    HINSTANCE         m_hInst       = nullptr;
    PopupAnimState    m_state       = PopupAnimState::Hidden;

    struct SecondaryPopup {
        HWND hwnd = nullptr;
        int  x = 0;
        int  y = 0;
    };
    std::vector<SecondaryPopup> m_secondaryPopups;
    void HideAll();

    // Animation progress 0.0–1.0
    float             m_animT       = 0.f;
    ULONGLONG         m_animStart   = 0;
    float             m_animDuration = 300.f;

    // Hover state
    bool              m_hovering    = false;
    std::unordered_set<HWND> m_trackedHwnds;
    float             m_hoverT      = 0.f;

    // Content
    const ContentItem* m_item       = nullptr;
    ContentItem       m_itemCopy;

    // Dimensions (DPI-scaled)
    int               m_width       = 420;
    int               m_height      = 120;
    int               m_screenX     = 0;
    int               m_screenY     = 0; // final resting position
    float             m_dpiScale    = 1.f;

    // Button rects (logical px)
    RECT              m_closeRect   {};
    RECT              m_copyRect    {};
    RECT              m_pauseRect   {};
    RECT              m_counterRect {};
    bool              m_copyFeedback = false;
    int               m_currentCounter = 1;
    int               m_targetCounter  = 1;
    float             m_sweepProgress  = 0.f;

    // Cached GDI resources to eliminate 60fps allocation churn
    HDC               m_hCachedMemDC     = nullptr;
    HBITMAP           m_hCachedBitmap    = nullptr;
    HBITMAP           m_hCachedOldBitmap = nullptr;
    int               m_cachedBmpWidth   = 0;
    int               m_cachedBmpHeight  = 0;

    // D2D resources
    ID2D1Factory*          m_d2dFactory        = nullptr;
    ID2D1DCRenderTarget*   m_dcRenderTarget    = nullptr;
    IDWriteFactory*        m_dwriteFactory     = nullptr;
    IDWriteTextFormat*     m_textFmtMain       = nullptr;
    IDWriteTextFormat*     m_textFmtMainLarge  = nullptr;
    IDWriteTextFormat*     m_textFmtMainMedium = nullptr;
    IDWriteTextFormat*     m_textFmtMainCompact= nullptr;
    IDWriteTextFormat*     m_textFmtSub        = nullptr;
    IDWriteTextFormat*     m_textFmtSmall      = nullptr;
    IWICImagingFactory*    m_wicFactory        = nullptr;
    ID2D1Bitmap*           m_logoBitmaps[6]    = { nullptr }; // 0:Green, 1:Yellow, 2:Blue, 3:Red, 4:Silver, 5:Gold
    ID2D1Bitmap*           m_logoDormantBitmap = nullptr;
    ID2D1Bitmap*           GetEffectiveLogo() const;

    // Grain bitmap (pre-generated)
    void              GenerateGrainBitmap();
    ID2D1Bitmap*      m_grainBitmap = nullptr;

    static constexpr wchar_t kClassName[] = L"DhikrPopup";
};

} // namespace dhikr
