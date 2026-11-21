#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include "../core/Settings.h"

#include <ctime>

namespace dhikr::ds {

// ════════════════════════════════════════════════════════════════════════════
// DESIGN SYSTEM  —  Single source of truth for all visual tokens
// Pixel-accurate to the provided design reference.
// ════════════════════════════════════════════════════════════════════════════

// ── Color helpers ────────────────────────────────────────────────────────────
inline D2D1_COLOR_F Rgba(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f) {
    return D2D1::ColorF(r / 255.f, g / 255.f, b / 255.f, a);
}
inline D2D1_COLOR_F Lerp(D2D1_COLOR_F a, D2D1_COLOR_F b, float t) {
    return D2D1::ColorF(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t);
}
inline COLORREF D2DToColorRef(D2D1_COLOR_F c) {
    return RGB((int)(c.r*255), (int)(c.g*255), (int)(c.b*255));
}

// ════════════════════════════════════════════════════════════════════════════
// ACCENT COLOR (Dynamic Brand Color per Theme, turns slate gray when paused)
// ════════════════════════════════════════════════════════════════════════════
namespace Accent {
    // Smooth Theme Transition Animation State
    inline bool s_isTransitioning = false;
    inline ColorTheme s_fromTheme = ColorTheme::Green;
    inline ColorTheme s_toTheme   = ColorTheme::Green;
    inline float s_transitionT    = 1.0f;

    inline void StartTransition(ColorTheme from, ColorTheme to) {
        s_isTransitioning = true;
        s_fromTheme = from;
        s_toTheme = to;
        s_transitionT = 0.0f;
    }

    inline void SetTransitionProgress(float t) {
        s_transitionT = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    }

    inline void EndTransition(ColorTheme finalTheme) {
        s_isTransitioning = false;
        s_transitionT = 1.0f;
        s_fromTheme = finalTheme;
        s_toTheme = finalTheme;
    }

    inline bool IsTransitioning() { return s_isTransitioning; }
    inline float GetTransitionProgress() { return s_transitionT; }
    inline ColorTheme FromTheme() { return s_fromTheme; }
    inline ColorTheme ToTheme() { return s_toTheme; }

    inline bool IsAppPaused() {
        const auto s = Settings::Instance().GetCopy();
        if (!s.remindersEnabled || s.paused) return true;
        if (s.pauseUntilUnix > 0 && time(nullptr) < s.pauseUntilUnix) return true;
        return false;
    }

    inline ColorTheme CurrentTheme() {
        return Settings::Instance().GetCopy().colorTheme;
    }

    inline D2D1_COLOR_F GetThemePrimary(ColorTheme theme, float alpha = 1.0f) {
        switch (theme) {
            case ColorTheme::Yellow: return Rgba(0xFA, 0xDC, 0x26, alpha); // Radiant Solar Yellow (#FADC26)
            case ColorTheme::Blue:   return Rgba(0x38, 0xBD, 0xF8, alpha); // Electric Cyan Sky (#38BDF8)
            case ColorTheme::Red:    return Rgba(0xF8, 0x71, 0x71, alpha); // Crimson Ruby (#F87171)
            case ColorTheme::Silver: return Rgba(0xE2, 0xE8, 0xF0, alpha); // Platinum Brushed Silver (#E2E8F0)
            case ColorTheme::Gold:   return Rgba(0xD4, 0xAF, 0x37, alpha); // Regal Antique Metallic Gold (#D4AF37)
            case ColorTheme::Green:
            default:                 return Rgba(0xB7, 0xF3, 0x4A, alpha); // Radiant Emerald Lime (#B7F34A)
        }
    }

    inline D2D1_COLOR_F Primary(float alpha = 1.0f) {
        if (IsAppPaused()) {
            return Rgba(0x8E, 0x99, 0x91, alpha); // Sleek slate gray (#8E9991)
        }
        if (s_isTransitioning) {
            D2D1_COLOR_F c1 = GetThemePrimary(s_fromTheme, alpha);
            D2D1_COLOR_F c2 = GetThemePrimary(s_toTheme, alpha);
            return Lerp(c1, c2, s_transitionT);
        }
        return GetThemePrimary(CurrentTheme(), alpha);
    }

    inline D2D1_COLOR_F Dark(float alpha = 1.0f) {
        D2D1_COLOR_F p = Primary(alpha);
        return D2D1::ColorF(p.r * 0.70f, p.g * 0.70f, p.b * 0.70f, alpha);
    }

    inline D2D1_COLOR_F Glow(float alpha = 0.08f) {
        return Primary(alpha);
    }

    inline D2D1_COLOR_F Muted(float alpha = 1.0f) {
        D2D1_COLOR_F p = Primary(alpha);
        return D2D1::ColorF(p.r * 0.50f, p.g * 0.50f, p.b * 0.50f, alpha);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// DARK MODE PALETTE (Clean, high-contrast, official dark theme matching logo)
// ════════════════════════════════════════════════════════════════════════════
namespace Dark {
    // Backgrounds (calm dark obsidian neutrals)
    inline D2D1_COLOR_F Background()      { return Rgba(0x08, 0x0A, 0x09, 1.0f); }
    inline D2D1_COLOR_F Sidebar()         { return Rgba(0x10, 0x13, 0x10, 1.0f); }
    inline D2D1_COLOR_F Surface()         { return Rgba(0x15, 0x19, 0x15, 1.0f); }
    inline D2D1_COLOR_F Card()            { return Rgba(0x15, 0x19, 0x15, 1.0f); }
    inline D2D1_COLOR_F CardHover()       { return Rgba(0x1B, 0x22, 0x1C, 1.0f); }
    inline D2D1_COLOR_F ActiveNavBg()     {
        if (Accent::IsAppPaused()) return Rgba(0x1C, 0x21, 0x1D, 0.85f);
        return Rgba(0x18, 0x23, 0x19, 0.85f);
    }

    // Borders
    inline D2D1_COLOR_F Border()          { return Rgba(0x28, 0x30, 0x29); }
    inline D2D1_COLOR_F BorderSubtle()    { return Rgba(0x1E, 0x25, 0x20); }
    inline D2D1_COLOR_F BorderHighlight() { return Accent::Primary(0.40f); }

    // Text - Increased white brightness & crisp contrast
    inline D2D1_COLOR_F TextPrimary()     { return Rgba(0xFD, 0xFE, 0xFD, 1.0f); } // Pure crisp brilliant white
    inline D2D1_COLOR_F TextSecondary()   { return Rgba(0xCB, 0xD5, 0xE1, 1.0f); } // Bright slate silver
    inline D2D1_COLOR_F TextMuted()       { return Rgba(0x94, 0xA3, 0xB8, 1.0f); } // Clean readable muted text

    // Popup
    inline D2D1_COLOR_F PopupBg()         { return Rgba(0x0F, 0x14, 0x10, 0.98f); }
    inline D2D1_COLOR_F PopupBorder()     { return Rgba(0x24, 0x2F, 0x26); }

    // Danger Button
    inline D2D1_COLOR_F BtnDanger()       { return Rgba(0xDC, 0x26, 0x26); }
    inline D2D1_COLOR_F BtnDangerHover()  { return Rgba(0xEF, 0x44, 0x44); }
    inline D2D1_COLOR_F BtnDangerBorder() { return Rgba(0xF8, 0x71, 0x71, 0.40f); }
    inline D2D1_COLOR_F BtnDangerText()   { return Rgba(0xFF, 0xFF, 0xFF, 1.0f); } // High contrast pure white
}

// ════════════════════════════════════════════════════════════════════════════
// CINEMATIC AURORA & GRADIENT DESIGN TOKENS (Signature Logo Harmony)
// ════════════════════════════════════════════════════════════════════════════
namespace Aurora {
    inline D2D1_COLOR_F GetThemeCanvasStop0(ColorTheme theme) {
        switch (theme) {
            case ColorTheme::Yellow: return Rgba(0x28, 0x24, 0x06, 1.0f);
            case ColorTheme::Blue:   return Rgba(0x0D, 0x21, 0x2E, 1.0f);
            case ColorTheme::Red:    return Rgba(0x2B, 0x0F, 0x12, 1.0f);
            case ColorTheme::Silver: return Rgba(0x1C, 0x20, 0x24, 1.0f);
            case ColorTheme::Gold:   return Rgba(0x35, 0x23, 0x08, 1.0f); // Warm imperial burnished bronze gold glow
            case ColorTheme::Green:
            default:                 return Rgba(0x16, 0x2C, 0x0F, 1.0f);
        }
    }
    inline D2D1_COLOR_F GetThemeCanvasStop1(ColorTheme theme) {
        switch (theme) {
            case ColorTheme::Yellow: return Rgba(0x1C, 0x18, 0x04, 1.0f);
            case ColorTheme::Blue:   return Rgba(0x08, 0x17, 0x22, 1.0f);
            case ColorTheme::Red:    return Rgba(0x1F, 0x0B, 0x0D, 1.0f);
            case ColorTheme::Silver: return Rgba(0x14, 0x17, 0x1A, 1.0f);
            case ColorTheme::Gold:   return Rgba(0x24, 0x17, 0x05, 1.0f);
            case ColorTheme::Green:
            default:                 return Rgba(0x0F, 0x1E, 0x0A, 1.0f);
        }
    }
    inline D2D1_COLOR_F GetThemeCanvasStop2(ColorTheme theme) {
        switch (theme) {
            case ColorTheme::Yellow: return Rgba(0x12, 0x0E, 0x03, 1.0f);
            case ColorTheme::Blue:   return Rgba(0x05, 0x0E, 0x16, 1.0f);
            case ColorTheme::Red:    return Rgba(0x14, 0x07, 0x08, 1.0f);
            case ColorTheme::Silver: return Rgba(0x0D, 0x0F, 0x11, 1.0f);
            case ColorTheme::Gold:   return Rgba(0x17, 0x0E, 0x03, 1.0f);
            case ColorTheme::Green:
            default:                 return Rgba(0x09, 0x13, 0x06, 1.0f);
        }
    }
    inline D2D1_COLOR_F GetThemeCanvasStop3(ColorTheme theme) {
        switch (theme) {
            case ColorTheme::Yellow: return Rgba(0x09, 0x07, 0x02, 1.0f);
            case ColorTheme::Blue:   return Rgba(0x03, 0x07, 0x0C, 1.0f);
            case ColorTheme::Red:    return Rgba(0x0A, 0x04, 0x05, 1.0f);
            case ColorTheme::Silver: return Rgba(0x07, 0x08, 0x09, 1.0f);
            case ColorTheme::Gold:   return Rgba(0x0A, 0x06, 0x02, 1.0f);
            case ColorTheme::Green:
            default:                 return Rgba(0x04, 0x08, 0x03, 1.0f);
        }
    }

    // Canvas diagonal base stops (Smoothly animated on theme switch)
    inline D2D1_COLOR_F CanvasStop0() {
        if (Accent::IsAppPaused()) return Rgba(0x18, 0x1B, 0x19, 1.0f);
        if (Accent::IsTransitioning()) {
            return Lerp(GetThemeCanvasStop0(Accent::FromTheme()),
                        GetThemeCanvasStop0(Accent::ToTheme()),
                        Accent::GetTransitionProgress());
        }
        return GetThemeCanvasStop0(Accent::CurrentTheme());
    }
    inline D2D1_COLOR_F CanvasStop1() {
        if (Accent::IsAppPaused()) return Rgba(0x12, 0x14, 0x13, 1.0f);
        if (Accent::IsTransitioning()) {
            return Lerp(GetThemeCanvasStop1(Accent::FromTheme()),
                        GetThemeCanvasStop1(Accent::ToTheme()),
                        Accent::GetTransitionProgress());
        }
        return GetThemeCanvasStop1(Accent::CurrentTheme());
    }
    inline D2D1_COLOR_F CanvasStop2() {
        if (Accent::IsAppPaused()) return Rgba(0x0C, 0x0E, 0x0D, 1.0f);
        if (Accent::IsTransitioning()) {
            return Lerp(GetThemeCanvasStop2(Accent::FromTheme()),
                        GetThemeCanvasStop2(Accent::ToTheme()),
                        Accent::GetTransitionProgress());
        }
        return GetThemeCanvasStop2(Accent::CurrentTheme());
    }
    inline D2D1_COLOR_F CanvasStop3() {
        if (Accent::IsAppPaused()) return Rgba(0x07, 0x09, 0x08, 1.0f);
        if (Accent::IsTransitioning()) {
            return Lerp(GetThemeCanvasStop3(Accent::FromTheme()),
                        GetThemeCanvasStop3(Accent::ToTheme()),
                        Accent::GetTransitionProgress());
        }
        return GetThemeCanvasStop3(Accent::CurrentTheme());
    }

    // Popup notification diagonal acrylic stops
    inline D2D1_COLOR_F PopupStop0(float a = 1.0f) {
        if (Accent::IsAppPaused()) return Rgba(0x1A, 0x1E, 0x1B, 0.98f * a);
        switch (Accent::CurrentTheme()) {
            case ColorTheme::Yellow: return Rgba(0x28, 0x24, 0x06, 0.98f * a);
            case ColorTheme::Blue:   return Rgba(0x0D, 0x21, 0x2E, 0.98f * a);
            case ColorTheme::Red:    return Rgba(0x2B, 0x0F, 0x12, 0.98f * a);
            case ColorTheme::Silver: return Rgba(0x1C, 0x20, 0x24, 0.98f * a);
            case ColorTheme::Gold:   return Rgba(0x35, 0x23, 0x08, 0.98f * a);
            case ColorTheme::Green:
            default:                 return Rgba(0x1A, 0x30, 0x16, 0.98f * a);
        }
    }
    inline D2D1_COLOR_F PopupStop1(float a = 1.0f) {
        if (Accent::IsAppPaused()) return Rgba(0x12, 0x15, 0x13, 0.98f * a);
        switch (Accent::CurrentTheme()) {
            case ColorTheme::Yellow: return Rgba(0x1C, 0x18, 0x04, 0.98f * a);
            case ColorTheme::Blue:   return Rgba(0x08, 0x17, 0x22, 0.98f * a);
            case ColorTheme::Red:    return Rgba(0x1F, 0x0B, 0x0D, 0.98f * a);
            case ColorTheme::Silver: return Rgba(0x14, 0x17, 0x1A, 0.98f * a);
            case ColorTheme::Gold:   return Rgba(0x24, 0x17, 0x05, 0.98f * a);
            case ColorTheme::Green:
            default:                 return Rgba(0x12, 0x22, 0x10, 0.98f * a);
        }
    }
    inline D2D1_COLOR_F PopupStop2(float a = 1.0f) {
        if (Accent::IsAppPaused()) return Rgba(0x0E, 0x10, 0x0F, 0.98f * a);
        switch (Accent::CurrentTheme()) {
            case ColorTheme::Yellow: return Rgba(0x12, 0x0E, 0x03, 0.98f * a);
            case ColorTheme::Blue:   return Rgba(0x05, 0x0E, 0x16, 0.98f * a);
            case ColorTheme::Red:    return Rgba(0x14, 0x07, 0x08, 0.98f * a);
            case ColorTheme::Silver: return Rgba(0x0D, 0x0F, 0x11, 0.98f * a);
            case ColorTheme::Gold:   return Rgba(0x17, 0x0E, 0x03, 0.98f * a);
            case ColorTheme::Green:
            default:                 return Rgba(0x0D, 0x17, 0x0C, 0.98f * a);
        }
    }
    inline D2D1_COLOR_F PopupStop3(float a = 1.0f) {
        if (Accent::IsAppPaused()) return Rgba(0x08, 0x0A, 0x09, 0.98f * a);
        switch (Accent::CurrentTheme()) {
            case ColorTheme::Yellow: return Rgba(0x09, 0x07, 0x02, 0.98f * a);
            case ColorTheme::Blue:   return Rgba(0x03, 0x07, 0x0C, 0.98f * a);
            case ColorTheme::Red:    return Rgba(0x0A, 0x04, 0x05, 0.98f * a);
            case ColorTheme::Silver: return Rgba(0x07, 0x08, 0x09, 0.98f * a);
            case ColorTheme::Gold:   return Rgba(0x0A, 0x06, 0x02, 0.98f * a);
            case ColorTheme::Green:
            default:                 return Rgba(0x07, 0x0B, 0x07, 0.98f * a);
        }
    }

    // Popup rim border stops (specular glass highlight with signature logo accent)
    inline D2D1_COLOR_F PopupBorderStop0(float a = 1.0f) { return Accent::Primary(0.65f * a); }
    inline D2D1_COLOR_F PopupBorderStop1(float a = 1.0f) {
        if (Accent::IsAppPaused()) return Rgba(0x68, 0x73, 0x6B, 0.32f * a);
        return Accent::Primary(0.32f * a);
    }
    inline D2D1_COLOR_F PopupBorderStop2(float a = 1.0f) { return Rgba(0xFF, 0xFF, 0xFF, 0.08f * a); }

    // Frosted Card Linear Stops (Clean Obsidian Charcoal with ultra subtle emerald warmth)
    inline D2D1_COLOR_F CardTop()         { return Rgba(0x18, 0x1F, 0x19, 0.95f); }
    inline D2D1_COLOR_F CardBottom()      { return Rgba(0x12, 0x17, 0x13, 0.98f); }
    inline D2D1_COLOR_F CardHoverTop()    { return Rgba(0x1F, 0x27, 0x20, 0.95f); }
    inline D2D1_COLOR_F CardHoverBottom() { return Rgba(0x16, 0x1D, 0x17, 0.98f); }
}

// ════════════════════════════════════════════════════════════════════════════
// THEME ACCESSORS  (Permanent Dark Theme)
// ════════════════════════════════════════════════════════════════════════════
inline constexpr bool IsDark() { return true; }

inline D2D1_COLOR_F Background()    { return Dark::Background();    }
inline D2D1_COLOR_F Sidebar()       { return Dark::Sidebar();       }
inline D2D1_COLOR_F Surface()       { return Dark::Surface();       }
inline D2D1_COLOR_F Card()          { return Dark::Card();          }
inline D2D1_COLOR_F CardHover()     { return Dark::CardHover();     }
inline D2D1_COLOR_F ActiveNavBg()   { return Dark::ActiveNavBg();   }
inline D2D1_COLOR_F Border()        { return Dark::Border();        }
inline D2D1_COLOR_F BorderSubtle()  { return Dark::BorderSubtle();  }
inline D2D1_COLOR_F TextPrimary()   { return Dark::TextPrimary();   }
inline D2D1_COLOR_F TextSecondary() { return Dark::TextSecondary(); }
inline D2D1_COLOR_F TextMuted()     { return Dark::TextMuted();     }
inline D2D1_COLOR_F PopupBg()       { return Dark::PopupBg();       }
inline D2D1_COLOR_F PopupBorder()   { return Dark::PopupBorder();   }
inline D2D1_COLOR_F BorderHighlight() { return Dark::BorderHighlight(); }
inline D2D1_COLOR_F BtnDanger()       { return Dark::BtnDanger();       }
inline D2D1_COLOR_F BtnDangerHover()  { return Dark::BtnDangerHover();  }
inline D2D1_COLOR_F BtnDangerBorder() { return Dark::BtnDangerBorder(); }
inline D2D1_COLOR_F BtnDangerText()   { return Dark::BtnDangerText();   }

// ════════════════════════════════════════════════════════════════════════════
// FONT FAMILIES
// ════════════════════════════════════════════════════════════════════════════
namespace Font {
    inline const wchar_t* Arabic() { return L"Cairo"; }
    inline const wchar_t* Latin()  { return L"Cairo"; }
    inline const wchar_t* UI()     { return L"Cairo"; }
}

// ════════════════════════════════════════════════════════════════════════════
// SPACING SYSTEM  (4px base grid)
// ════════════════════════════════════════════════════════════════════════════
namespace Space {
    constexpr float XS  =  4.f;
    constexpr float S   =  8.f;
    constexpr float M   = 12.f;
    constexpr float L   = 16.f;
    constexpr float XL  = 24.f;
    constexpr float XXL = 32.f;
    constexpr float X3L = 40.f;
    constexpr float X4L = 48.f;
}

// ════════════════════════════════════════════════════════════════════════════
// BORDER RADIUS
// ════════════════════════════════════════════════════════════════════════════
namespace Radius {
    constexpr float XS   = 3.f;
    constexpr float S    = 6.f;
    constexpr float M    = 8.f;
    constexpr float L    = 12.f;
    constexpr float XL   = 16.f;
    constexpr float Full = 999.f;
}

// ════════════════════════════════════════════════════════════════════════════
// DIMENSIONS
// ════════════════════════════════════════════════════════════════════════════
namespace Dims {
    // Main window
    constexpr float MainWindowW  = 880.f;
    constexpr float MainWindowH  = 620.f;
    constexpr float MainWindowMinW = 740.f;
    constexpr float MainWindowMinH = 500.f;

    // Custom Title bar
    constexpr float TitleBarH    = 36.f;
    constexpr float CaptionBtnW  = 46.f;

    // Sidebar
    constexpr float SidebarW     = 180.f;
    constexpr float NavItemH     = 42.f;
    constexpr float NavIconSize  = 16.f; // Segoe MDL2 icon size
    constexpr float NavIndicatorW = 3.f;

    // Popup
    constexpr float PopupW       = 420.f;
    constexpr float PopupH       = 100.f; // grows with content
    constexpr float PopupMinH    = 96.f;
    constexpr float PopupMaxH    = 180.f;
    constexpr float PopupAccentW = 3.f;
    constexpr float PopupRadius  = 26.f;
    constexpr float PopupIconSize = 24.f;
    constexpr float PopupMargin  = 16.f; // margin from screen edge

    // Cards
    constexpr float CardRadius   = 18.f;
    constexpr float CardPadding  = 16.f;

    // Toggles
    constexpr float ToggleW      = 42.f;
    constexpr float ToggleH      = 24.f;
    constexpr float ToggleKnobD  = 18.f;

    // Progress bar
    constexpr float ProgressH    = 4.f;

    // Buttons
    constexpr float BtnH         = 36.f;
    constexpr float BtnRadius    = 18.f;

    // Input
    constexpr float InputH       = 34.f;
    constexpr float InputRadius  = 14.f;
}

// ════════════════════════════════════════════════════════════════════════════
// FONT SIZES (in DWRITE points — approximate, adjust for DPI)
// ════════════════════════════════════════════════════════════════════════════
namespace FontSize {
    constexpr float AppTitle    = 13.f;
    constexpr float PageTitle   = 22.f; // 24px equivalent
    constexpr float CardTitle   = 12.f;
    constexpr float BigNumber   = 32.f;
    constexpr float MedNumber   = 22.f;
    constexpr float Body        = 13.f; // 14px equivalent
    constexpr float Secondary   = 11.5f;// 12px equivalent
    constexpr float Small       = 10.5f;
    constexpr float PopupMain   = 17.f;
    constexpr float PopupSub    = 11.f;
    constexpr float NavLabel    = 12.5f;
    constexpr float TabLabel    = 12.f;
    constexpr float BtnLabel    = 12.f;
    constexpr float InputText   = 12.f;
    constexpr float Version     = 10.f;
}

// ════════════════════════════════════════════════════════════════════════════
// ICONS (Segoe MDL2 Assets Unicode Codepoints)
// ════════════════════════════════════════════════════════════════════════════
namespace Icon {
    // Navigation
    constexpr const wchar_t* Home         = L"\xE80F";
    constexpr const wchar_t* Reminders    = L"\xEA8F"; // Ringer/Bell
    constexpr const wchar_t* Schedule     = L"\xE787"; // Calendar
    constexpr const wchar_t* Library      = L"\xE82D"; // Library/Book
    constexpr const wchar_t* Statistics   = L"\xE9D9"; // Chart
    constexpr const wchar_t* Settings     = L"\xE713"; // Gear
    constexpr const wchar_t* About        = L"\xE946"; // Info
    constexpr const wchar_t* Moon         = L"\xE708"; // Moon / ClearNight

    // Window controls
    constexpr const wchar_t* Minimize     = L"\xE921";
    constexpr const wchar_t* Maximize     = L"\xE922";
    constexpr const wchar_t* Restore      = L"\xE923";
    constexpr const wchar_t* Close        = L"\xE8BB";

    // General UI
    constexpr const wchar_t* More         = L"\xE712"; // ...
    constexpr const wchar_t* Add          = L"\xE710"; // +
    constexpr const wchar_t* ChevronDown  = L"\xE70D";
    constexpr const wchar_t* Play         = L"\xE768";
    constexpr const wchar_t* Refresh      = L"\xE72C";
    constexpr const wchar_t* Volume       = L"\xE767";
    constexpr const wchar_t* Monitor      = L"\xE7F4";
    constexpr const wchar_t* System       = L"\xE770";

    // Safe icon font family resolution for any Windows PC (Win 10 & Win 11)
    inline const wchar_t* GetFontFamily(IDWriteFactory* dwf) {
        if (!dwf) return L"Segoe MDL2 Assets";
        IDWriteFontCollection* collection = nullptr;
        if (SUCCEEDED(dwf->GetSystemFontCollection(&collection, FALSE)) && collection) {
            UINT32 index = 0;
            BOOL exists = FALSE;
            if (SUCCEEDED(collection->FindFamilyName(L"Segoe Fluent Icons", &index, &exists)) && exists) {
                collection->Release();
                return L"Segoe Fluent Icons";
            }
            if (SUCCEEDED(collection->FindFamilyName(L"Segoe MDL2 Assets", &index, &exists)) && exists) {
                collection->Release();
                return L"Segoe MDL2 Assets";
            }
            collection->Release();
        }
        return L"Segoe MDL2 Assets";
    }
}

// ════════════════════════════════════════════════════════════════════════════
// ANIMATION TIMING (milliseconds)
// ════════════════════════════════════════════════════════════════════════════
namespace Anim {
    constexpr int PopupEnterMs   = 300;
    constexpr int PopupExitMs    = 250;
    constexpr int ThemeMs        = 200;
    constexpr int ToggleMs       = 160;
    constexpr int HoverMs        = 80;
    constexpr int NavMs          = 120;
    constexpr int PageFadeMs     = 200;
    constexpr int BtnHoverMs     = 120;
    constexpr int NavIndicatorMs = 150;
    constexpr int FrameIntervalMs = 16; // ~60fps during animations only
    constexpr float PopupSlideDistPx = 40.f;
}

// ════════════════════════════════════════════════════════════════════════════
// EASING
// ════════════════════════════════════════════════════════════════════════════
inline float EaseOut(float t) {
    return 1.f - (1.f - t) * (1.f - t);
}
inline float EaseIn(float t) {
    return t * t;
}
inline float EaseInOut(float t) {
    return t < 0.5f ? 2.f * t * t : 1.f - 2.f * (1.f - t) * (1.f - t);
}
inline float EaseOutBack(float t) {
    // Elegant, cushioned back ease without jarring overshoot (c1 = 0.50f instead of 1.70f)
    const float c1 = 0.50f;
    const float c3 = c1 + 1.f;
    return 1.f + c3 * (t - 1.f) * (t - 1.f) * (t - 1.f) + c1 * (t - 1.f) * (t - 1.f);
}
inline float SoftSpringEase(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    return 1.f - expf(-5.5f * t) * cosf(3.0f * t);
}
inline float EaseOutCubic(float t) {
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}
inline float EaseOutQuad(float t) {
    return 1.f - (1.f - t) * (1.f - t);
}
inline float EaseOutQuart(float t) {
    float inv = 1.f - t;
    return 1.f - inv * inv * inv * inv;
}
inline float EaseOutQuint(float t) {
    float inv = 1.f - t;
    return 1.f - inv * inv * inv * inv * inv;
}
inline float SpringBloomEase(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    // Harmonic spring with 3% damped overshoot
    return 1.0f - expf(-6.0f * t) * cosf(6.283f * t);
}
inline float LiquidWaveEase(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    // Calm buoyant oscillation: reaches 1.025 then softly settles to 1.000
    return 1.0f - expf(-5.0f * t) * cosf(4.712f * t);
}
inline float ZenBreathingEase(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    // Peaceful harmonic breath: gently rises, soft inhale expand, calm exhale settle
    float s = sinf(t * 3.14159265f * 0.5f);
    return s + 0.015f * sinf(t * 3.14159265f);
}
inline float RippleGenesisEase(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    // Fluid radial ripple expansion with smooth viscous settling
    return 1.0f - expf(-5.2f * t) * cosf(5.5f * t);
}

// ════════════════════════════════════════════════════════════════════════════
// ARABIC TYPOGRAPHY UTILITIES
// ════════════════════════════════════════════════════════════════════════════
inline std::wstring StripArabicTashkeel(const std::wstring& input) {
    std::wstring out;
    out.reserve(input.size());
    for (wchar_t ch : input) {
        // Filter Harakat (Fatha, Damma, Kasra, Sukun, Shadda, Tanween, etc.) & Tatweel
        if ((ch >= 0x064B && ch <= 0x065F) || ch == 0x0670 || ch == 0x0640) {
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════════════
// ARABESQUE ISLAMIC GEOMETRY  —  Subtle 8-Point Star Rosette Lattice
// ════════════════════════════════════════════════════════════════════════════
namespace Arabesque {
    inline void DrawLattice(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds, float opacity = 0.025f) {
        if (!rt || opacity <= 0.001f) return;
        ID2D1SolidColorBrush* brush = nullptr;
        rt->CreateSolidColorBrush(Rgba(0xFF, 0xFF, 0xFF, opacity), &brush);
        if (!brush) return;

        float tileSize = 64.f;
        float w = bounds.right - bounds.left;
        float h = bounds.bottom - bounds.top;

        int cols = (int)(w / tileSize) + 2;
        int rows = (int)(h / tileSize) + 2;

        for (int r = -1; r < rows; ++r) {
            for (int c = -1; c < cols; ++c) {
                float cx = bounds.left + c * tileSize + tileSize * 0.5f;
                float cy = bounds.top  + r * tileSize + tileSize * 0.5f;
                float rad = tileSize * 0.26f;

                // Square 1
                D2D1_POINT_2F s1[5] = {
                    D2D1::Point2F(cx - rad, cy - rad),
                    D2D1::Point2F(cx + rad, cy - rad),
                    D2D1::Point2F(cx + rad, cy + rad),
                    D2D1::Point2F(cx - rad, cy + rad),
                    D2D1::Point2F(cx - rad, cy - rad)
                };
                // Square 2 (rotated 45 degrees)
                float d = rad * 1.4142f * 0.7071f;
                D2D1_POINT_2F s2[5] = {
                    D2D1::Point2F(cx, cy - rad * 1.25f),
                    D2D1::Point2F(cx + rad * 1.25f, cy),
                    D2D1::Point2F(cx, cy + rad * 1.25f),
                    D2D1::Point2F(cx - rad * 1.25f, cy),
                    D2D1::Point2F(cx, cy - rad * 1.25f)
                };

                for (int i = 0; i < 4; ++i) {
                    rt->DrawLine(s1[i], s1[i+1], brush, 0.75f);
                    rt->DrawLine(s2[i], s2[i+1], brush, 0.75f);
                }
                rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad * 0.35f, rad * 0.35f), brush, 0.65f);
            }
        }
        brush->Release();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// GRAIN TEXTURE  —  precomputed tiny noise, baked into popup surface
// ════════════════════════════════════════════════════════════════════════════
namespace Grain {
    constexpr float Opacity = 0.025f; // almost invisible
    constexpr int   Size    = 128;    // tile size
}
} // namespace dhikr::ds
