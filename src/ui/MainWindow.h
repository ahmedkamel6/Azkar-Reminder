#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <memory>
#include "core/InputController.h"
#include "core/UIState.h"
#include "controls/NavigationRail.h"

namespace dhikr {

class ScreenshotExporter;
struct ContentItem;

// ── Page abstract base ────────────────────────────────────────────────────────
class PageBase {
public:
    virtual ~PageBase() = default;
    virtual void Create(HWND parent, HINSTANCE hInst, const D2D1_RECT_F& bounds) {}
    virtual void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) = 0;
    virtual void OnSize(const D2D1_RECT_F& bounds) {}
    virtual bool OnLButtonDown(int x, int y) { return false; }
    virtual bool OnLButtonUp(int x, int y) { return false; }
    virtual bool OnMouseMove(int x, int y) { return false; }
    virtual bool OnMouseWheel(int delta) { return false; }
    virtual void OnMouseLeave() {}
    virtual bool OnChar(wchar_t ch) { return false; }
    virtual bool OnKeyDown(WPARAM key) { return false; }
    virtual bool HitTestInteractive(int x, int y) { return false; }
    virtual void OnSettingsChanged() {}
    virtual void OnActivated() {}
    virtual float GetScrollY() const { return 0.f; }
    virtual void SetScrollY(float y) {}
    virtual bool OnScrollTick() { return false; }
    virtual void SetDpiScale(float dpi) {}
    virtual void ResetD2D() {}
};
// ── MainWindow ────────────────────────────────────────────────────────────────
// Primary application window: sidebar + content area.
// Rendered with Direct2D; custom controls are owner-drawn.
class MainWindow {
public:
    static MainWindow& Instance();

    bool Create(HINSTANCE hInstance);
    void Show();
    void Hide();
    HWND GetHwnd() const { return m_hwnd; }

    // Navigate to a page (PAGE_* constants from resource.h)
    void NavigateTo(int pageId);

    // Force redraw
    void Invalidate() { InvalidateRect(m_hwnd, nullptr, FALSE); }
    void UpdateAppIcon(ColorTheme themeOverride = (ColorTheme)-1);
    ColorTheme GetTargetTheme() const { return m_targetTheme; }
    float GetDpiScale() const { return m_dpiScale; }

    ID2D1Bitmap* GetLogoBitmap() const { return m_logoBitmaps[0]; }
    ID2D1Bitmap* GetLogoSmallBitmap() const { return m_logoSmallBitmaps[0]; }
    ID2D1Bitmap* GetLogoDormantBitmap() const { return m_logoDormantBitmap; }
    ID2D1Bitmap* GetLogoDormantSmallBitmap() const { return m_logoDormantSmallBitmap; }
    ID2D1Bitmap* GetEffectiveLogo(bool isSmall = false) const;
    ID2D1Bitmap* GetThemeLogo(ColorTheme theme, bool isSmall = false) const;
    void DrawEffectiveLogo(ID2D1RenderTarget* rt, const D2D1_RECT_F& dest, bool isSmall = false, float alpha = 1.0f) const;
    void TransitionToTheme(ColorTheme newTheme);
    IWICImagingFactory* GetWicFactory() const { return m_wicFactory; }
    HINSTANCE GetHInstance() const { return m_hInst; }

    LRESULT OnSettingsChanged();
    LRESULT OnMouseWheel(int delta);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

private:
    friend class ScreenshotExporter;
    MainWindow() = default;
    ~MainWindow();

    void RegisterClass(HINSTANCE hInstance);
    bool CreateWindow_(HINSTANCE hInstance);
    bool InitD2D();
    bool InitTextFormats();
    void InitPages(HINSTANCE hInstance);
    void ReleaseD2D();

    // Drawing
    void OnPaint();
    void DrawBackground(ID2D1RenderTarget* rt);
    void DrawSidebar(ID2D1RenderTarget* rt);
    void DrawSidebarItem(ID2D1RenderTarget* rt, int index,
                         const wchar_t* label, float y, bool active, bool hover);
    void DrawSidebarBottom(ID2D1RenderTarget* rt);
    void DrawTitleBar(ID2D1RenderTarget* rt);
    void DrawCurrentPage(ID2D1RenderTarget* rt);

    // Sidebar icon helpers (inline geometric icons)
    void DrawNavIcon(ID2D1RenderTarget* rt, int pageId, float cx, float cy,
                     float size, D2D1_COLOR_F color);

    // Message handlers
    LRESULT OnSize(int w, int h);
    LRESULT OnNcCalcSize(WPARAM wp, LPARAM lp);
    LRESULT OnNcHitTest(int x, int y);
    LRESULT OnCommand(WPARAM wp);
    LRESULT OnReminder(bool forcePreview = false);
    LRESULT OnTimer(UINT_PTR timerId);
    LRESULT OnMouseMove(int x, int y);
    LRESULT OnMouseLeave();
    LRESULT OnLButtonDown(int x, int y);
    LRESULT OnSetCursor(HWND hwnd, UINT hitTest, UINT message);
    LRESULT OnPowerBroadcast(WPARAM wp);
    LRESULT OnTrayCallback(LPARAM lp);
    LRESULT OnGetMinMaxInfo(MINMAXINFO* mmi);
    LRESULT OnLButtonUp(int x, int y);
    LRESULT OnClose();
    LRESULT OnDestroy();
    LRESULT OnUpdateAvailable();      // WM_UPDATE_AVAILABLE from Updater
    LRESULT OnDpiChanged(UINT newDpi, const RECT* newRect);

    // Layout helpers
    D2D1_RECT_F SidebarRect() const;
    D2D1_RECT_F ContentRect() const;
    D2D1_RECT_F NavItemRect(int index) const;
    int   HitTestNavItem(int x, int y) const;

    // ── State ───────────────────────────────────────────────────────────────
    HWND            m_hwnd      = nullptr;
    HINSTANCE       m_hInst     = nullptr;
    int             m_width     = 880; // from DesignSystem
    int             m_height    = 620;
    int             m_activePage = 0; // PAGE_HOME
    int             m_hoveredNavItem = -1;
    float           m_dpiScale  = 1.f;
    bool            m_mouseTracking = false;

    // Page instances
    std::unique_ptr<PageBase> m_pages[5]; // one per nav item

    // Full-screen distraction queue
    std::unique_ptr<ContentItem> m_pendingFullscreenReminder;

    // D2D resources
    ID2D1Factory*           m_d2dFactory    = nullptr;
    ID2D1HwndRenderTarget*  m_renderTarget  = nullptr;
    IDWriteFactory*         m_dwriteFactory = nullptr;

    // Cached brushes (recreated on target recreate)
    ID2D1SolidColorBrush*   m_brush         = nullptr;

    // Text formats
    IDWriteTextFormat*      m_tfNavLabel    = nullptr;
    IDWriteTextFormat*      m_tfAppName     = nullptr;
    IDWriteTextFormat*      m_tfVersion     = nullptr;
    IDWriteTextFormat*      m_tfIcon        = nullptr; // Segoe MDL2 Assets

    IWICImagingFactory*     m_wicFactory    = nullptr;
    ID2D1Bitmap*            m_logoBitmaps[6]      = { nullptr }; // 0:Green, 1:Yellow, 2:Blue, 3:Red, 4:Silver, 5:Gold
    ID2D1Bitmap*            m_logoSmallBitmaps[6] = { nullptr };
    ID2D1Bitmap*            m_logoDormantBitmap      = nullptr;
    ID2D1Bitmap*            m_logoDormantSmallBitmap = nullptr;

    // Theme transition animation
    static constexpr UINT_PTR TIMER_THEME_TRANSITION = 1005;
    ColorTheme              m_prevTheme          = ColorTheme::Green;
    ColorTheme              m_targetTheme        = ColorTheme::Green;
    ULONGLONG               m_themeAnimStartTime = 0;
    float                   m_themeAnimDuration  = 350.0f; // 350ms smooth transition

    HICON                   m_hCurrentIconBig    = nullptr;
    HICON                   m_hCurrentIconSmall  = nullptr;

    std::shared_ptr<NavigationRail> m_navRail;

    static constexpr wchar_t kClassName[] = L"DhikrMain";
    static constexpr int kNavItems = 5;
};

} // namespace dhikr
