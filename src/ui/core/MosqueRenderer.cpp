#include "MosqueRenderer.h"
#include "../DesignSystem.h"
#include "../../core/Localization.h"
#include <vector>
#include <cmath>

namespace dhikr {

TimePeriod MosqueRenderer::GetCurrentPeriod() {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_s(&lt, &now);
    int tMin = lt.tm_hour * 60 + lt.tm_min;

    if (tMin >= 5 * 60 && tMin < 11 * 60 + 30) {
        return TimePeriod::Morning; // 5:00 AM - 11:30 AM (أذكار الصباح)
    } else if (tMin >= 11 * 60 + 30 && tMin < 17 * 60 + 30) {
        return TimePeriod::Day;     // 11:30 AM - 5:30 PM (أذكار النهار والعصر)
    } else if (tMin >= 17 * 60 + 30 && tMin < 19 * 60 + 30) {
        return TimePeriod::Sunset;  // 5:30 PM - 7:30 PM (أذكار المغرب والغروب)
    } else {
        return TimePeriod::Night;   // 7:30 PM - 5:00 AM (أذكار المساء والليل)
    }
}

const wchar_t* MosqueRenderer::GetPeriodBadge(TimePeriod period) {
    bool isRtl = Localization::Instance().IsRTL();
    if (isRtl) {
        switch (period) {
            case TimePeriod::Morning: return L"أذكار الصباح ☀️";
            case TimePeriod::Day:     return L"أذكار اليوم 🌤";
            case TimePeriod::Sunset:  return L"أذكار المساء 🌇";
            case TimePeriod::Night:   return L"أذكار المساء 🌙";
        }
        return L"أذكار 🌙";
    } else {
        switch (period) {
            case TimePeriod::Morning: return L"Morning Dhikr ☀️";
            case TimePeriod::Day:     return L"Daytime Dhikr 🌤";
            case TimePeriod::Sunset:  return L"Sunset Dhikr 🌇";
            case TimePeriod::Night:   return L"Evening Dhikr 🌙";
        }
        return L"Dhikr 🌙";
    }
}

void MosqueRenderer::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds, float dpiScale) {
    if (!rt) return;

    TimePeriod period = GetCurrentPeriod();
    ID2D1Factory* factory = nullptr;
    rt->GetFactory(&factory);
    if (!factory) return;

    // ── 1. Atmospheric Sky Gradient across the right portion ──────────────────
    D2D1_GRADIENT_STOP skyStops[3];
    D2D1_COLOR_F ambientColor;

    if (period == TimePeriod::Morning) {
        // Golden Dawn / Morning Mist
        skyStops[0] = { 0.0f, ds::Rgba(0x06, 0x1A, 0x13, 0.95f) };
        skyStops[1] = { 0.5f, ds::Rgba(0x0C, 0x2E, 0x20, 0.92f) };
        skyStops[2] = { 1.0f, ds::Rgba(0x1B, 0x48, 0x2A, 0.90f) };
        ambientColor = ds::Rgba(0xF5, 0x9E, 0x0B, 0.22f); // Warm dawn amber
    } else if (period == TimePeriod::Day) {
        // Serene Teal-Azure Daylight
        skyStops[0] = { 0.0f, ds::Rgba(0x07, 0x1E, 0x18, 0.95f) };
        skyStops[1] = { 0.5f, ds::Rgba(0x0B, 0x33, 0x28, 0.92f) };
        skyStops[2] = { 1.0f, ds::Rgba(0x11, 0x45, 0x36, 0.90f) };
        ambientColor = ds::Rgba(0x34, 0xD3, 0x99, 0.18f); // Daylight aura
    } else if (period == TimePeriod::Sunset) {
        // Violet & Amber Twilight
        skyStops[0] = { 0.0f, ds::Rgba(0x16, 0x0F, 0x20, 0.95f) };
        skyStops[1] = { 0.5f, ds::Rgba(0x32, 0x16, 0x22, 0.92f) };
        skyStops[2] = { 1.0f, ds::Rgba(0x52, 0x24, 0x14, 0.90f) };
        ambientColor = ds::Rgba(0xF9, 0x73, 0x16, 0.26f); // Sunset horizon glow
    } else {
        // Deep Midnight Velvet with Moon
        skyStops[0] = { 0.0f, ds::Rgba(0x04, 0x0C, 0x08, 0.96f) };
        skyStops[1] = { 0.5f, ds::Rgba(0x08, 0x1C, 0x13, 0.92f) };
        skyStops[2] = { 1.0f, ds::Rgba(0x0C, 0x27, 0x1B, 0.88f) };
        ambientColor = ds::Rgba(0xFE, 0xF0, 0x8A, 0.22f); // Moonlit halo
    }

    ID2D1GradientStopCollection* skyStopCol = nullptr;
    if (SUCCEEDED(rt->CreateGradientStopCollection(skyStops, 3, &skyStopCol)) && skyStopCol) {
        ID2D1LinearGradientBrush* skyBrush = nullptr;
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES sprop = {
            D2D1::Point2F(bounds.right, bounds.top),
            D2D1::Point2F(bounds.right, bounds.bottom)
        };
        if (SUCCEEDED(rt->CreateLinearGradientBrush(sprop, skyStopCol, &skyBrush)) && skyBrush) {
            rt->FillRectangle(bounds, skyBrush);
            skyBrush->Release();
        }
        skyStopCol->Release();
    }

    // ── 2. Ambient Orb (Sun / Moon Halo) ───────────────────────────────────────
    float orbX = bounds.right - 70.f * dpiScale;
    float orbY = (period == TimePeriod::Sunset) ? (bounds.bottom - 45.f * dpiScale) : (bounds.top + 40.f * dpiScale);
    float orbR = 80.f * dpiScale;

    D2D1_GRADIENT_STOP orbStops[] = {
        { 0.0f, ambientColor },
        { 1.0f, D2D1::ColorF(0, 0, 0, 0.0f) }
    };
    ID2D1GradientStopCollection* orbStopCol = nullptr;
    if (SUCCEEDED(rt->CreateGradientStopCollection(orbStops, 2, &orbStopCol)) && orbStopCol) {
        ID2D1RadialGradientBrush* orbBrush = nullptr;
        D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES rprop = {
            D2D1::Point2F(orbX, orbY),
            D2D1::Point2F(0.0f, 0.0f),
            orbR, orbR
        };
        if (SUCCEEDED(rt->CreateRadialGradientBrush(rprop, orbStopCol, &orbBrush)) && orbBrush) {
            rt->FillRectangle(bounds, orbBrush);
            orbBrush->Release();
        }
        orbStopCol->Release();
    }

    // ── 3. Night Mode: Crescent Moon & Starlight ──────────────────────────────
    ID2D1SolidColorBrush* solidBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &solidBrush);

    if (period == TimePeriod::Night && solidBrush) {
        // Glowing Crescent Moon
        float moonR = 11.f * dpiScale;
        ID2D1PathGeometry* moonGeo = nullptr;
        if (SUCCEEDED(factory->CreatePathGeometry(&moonGeo)) && moonGeo) {
            ID2D1GeometrySink* sink = nullptr;
            if (SUCCEEDED(moonGeo->Open(&sink)) && sink) {
                // Outer arc
                sink->BeginFigure(D2D1::Point2F(orbX, orbY - moonR), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddArc(D2D1::ArcSegment(
                    D2D1::Point2F(orbX, orbY + moonR),
                    D2D1::SizeF(moonR, moonR), 0.f,
                    D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                // Inner cutout arc
                sink->AddArc(D2D1::ArcSegment(
                    D2D1::Point2F(orbX, orbY - moonR),
                    D2D1::SizeF(moonR * 0.85f, moonR * 0.85f), 0.f,
                    D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                sink->Release();

                solidBrush->SetColor(ds::Rgba(0xFE, 0xF0, 0x8A, 0.95f));
                rt->FillGeometry(moonGeo, solidBrush);
            }
            moonGeo->Release();
        }

        // Delicate Twinkle Stars
        struct Star { float dx, dy, r, a; };
        Star stars[] = {
            { -70.f, -15.f, 1.4f, 0.65f },
            { -40.f, -22.f, 1.2f, 0.45f },
            { -95.f,  10.f, 1.6f, 0.70f },
            {  35.f, -10.f, 1.2f, 0.50f },
            { -120.f, -5.f, 1.3f, 0.40f },
            { -25.f,  30.f, 1.1f, 0.35f }
        };
        for (const auto& s : stars) {
            solidBrush->SetColor(ds::Rgba(255, 255, 255, s.a));
            float sx = orbX + s.dx * dpiScale;
            float sy = orbY + s.dy * dpiScale;
            if (sx >= bounds.left && sx <= bounds.right && sy >= bounds.top && sy <= bounds.bottom) {
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(sx, sy), s.r * dpiScale, s.r * dpiScale), solidBrush);
            }
        }
    }

    // ── 4. Distant Mountain Ridge ─────────────────────────────────────────────
    ID2D1PathGeometry* mountainGeo = nullptr;
    if (SUCCEEDED(factory->CreatePathGeometry(&mountainGeo)) && mountainGeo) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(mountainGeo->Open(&sink)) && sink) {
            float baseY = bounds.bottom;
            float mLeft = bounds.right - 260.f * dpiScale;
            sink->BeginFigure(D2D1::Point2F(mLeft, baseY), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(mLeft + 50.f * dpiScale, baseY - 45.f * dpiScale),
                D2D1::Point2F(mLeft + 120.f * dpiScale, baseY - 35.f * dpiScale),
                D2D1::Point2F(mLeft + 180.f * dpiScale, baseY - 50.f * dpiScale)));
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(mLeft + 220.f * dpiScale, baseY - 60.f * dpiScale),
                D2D1::Point2F(bounds.right - 20.f * dpiScale, baseY - 35.f * dpiScale),
                D2D1::Point2F(bounds.right, baseY - 38.f * dpiScale)));
            sink->AddLine(D2D1::Point2F(bounds.right, baseY));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();

            if (solidBrush) {
                solidBrush->SetColor(ds::Rgba(0x0A, 0x1E, 0x15, 0.45f));
                rt->FillGeometry(mountainGeo, solidBrush);
            }
        }
        mountainGeo->Release();
    }

    // ── 5. Mosque Silhouette Vector Geometry ──────────────────────────────────
    D2D1_COLOR_F mosqueColor = (period == TimePeriod::Sunset)
        ? ds::Rgba(0x0A, 0x06, 0x0E, 0.94f)
        : ds::Rgba(0x04, 0x0D, 0x08, 0.94f);

    ID2D1PathGeometry* mosqueGeo = nullptr;
    if (SUCCEEDED(factory->CreatePathGeometry(&mosqueGeo)) && mosqueGeo) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(mosqueGeo->Open(&sink)) && sink) {
            float baseY   = bounds.bottom;
            float domeCX  = bounds.right - 95.f  * dpiScale;
            float domeW   = 36.f * dpiScale;
            float domeH   = 48.f * dpiScale;
            float domeBY  = baseY - 16.f * dpiScale;

            float min1X   = bounds.right - 150.f * dpiScale;
            float min2X   = bounds.right - 35.f  * dpiScale;
            float minW    = 10.f * dpiScale;
            float min1Top = bounds.top   + 32.f  * dpiScale;
            float min2Top = bounds.top   + 38.f  * dpiScale;

            sink->BeginFigure(D2D1::Point2F(bounds.right - 210.f * dpiScale, baseY), D2D1_FIGURE_BEGIN_FILLED);

            // Left Colonnade
            sink->AddLine(D2D1::Point2F(bounds.right - 190.f * dpiScale, baseY - 12.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X - minW/2, baseY - 12.f * dpiScale));

            // Minaret 1 (Left tall tower)
            sink->AddLine(D2D1::Point2F(min1X - minW/2, baseY));
            sink->AddLine(D2D1::Point2F(min1X - minW/2, min1Top + 24.f * dpiScale));
            // Balcony 1
            sink->AddLine(D2D1::Point2F(min1X - minW/2 - 2.5f*dpiScale, min1Top + 24.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X - minW/2 - 2.5f*dpiScale, min1Top + 21.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X - minW/2 + 1.f*dpiScale, min1Top + 21.f * dpiScale));
            // Shaft upper
            sink->AddLine(D2D1::Point2F(min1X - minW/2 + 1.f*dpiScale, min1Top + 9.f * dpiScale));
            // Balcony 2
            sink->AddLine(D2D1::Point2F(min1X - minW/2 - 2.f*dpiScale, min1Top + 9.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X - minW/2 - 2.f*dpiScale, min1Top + 7.f * dpiScale));
            // Conical spire & Crescent finial
            sink->AddLine(D2D1::Point2F(min1X, min1Top));
            sink->AddLine(D2D1::Point2F(min1X + minW/2 + 2.f*dpiScale, min1Top + 7.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X + minW/2 + 2.f*dpiScale, min1Top + 9.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X + minW/2 - 1.f*dpiScale, min1Top + 9.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X + minW/2 - 1.f*dpiScale, min1Top + 21.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X + minW/2 + 2.5f*dpiScale, min1Top + 21.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X + minW/2 + 2.5f*dpiScale, min1Top + 24.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X + minW/2, min1Top + 24.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min1X + minW/2, baseY - 14.f * dpiScale));

            // Side Dome 1 (Left)
            float sDome1X = domeCX - 32.f * dpiScale;
            sink->AddLine(D2D1::Point2F(sDome1X - 16.f * dpiScale, baseY - 14.f * dpiScale));
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(sDome1X - 16.f * dpiScale, baseY - 32.f * dpiScale),
                D2D1::Point2F(sDome1X, baseY - 36.f * dpiScale),
                D2D1::Point2F(sDome1X, baseY - 40.f * dpiScale)));
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(sDome1X, baseY - 36.f * dpiScale),
                D2D1::Point2F(sDome1X + 16.f * dpiScale, baseY - 32.f * dpiScale),
                D2D1::Point2F(sDome1X + 16.f * dpiScale, baseY - 16.f * dpiScale)));

            // Main Dome Base & Onion Arch Curve
            sink->AddLine(D2D1::Point2F(domeCX - domeW, domeBY));
            // Left half of grand dome arch
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(domeCX - domeW * 1.15f, domeBY - domeH * 0.45f),
                D2D1::Point2F(domeCX - domeW * 0.60f, domeBY - domeH * 0.90f),
                D2D1::Point2F(domeCX, domeBY - domeH)));
            // Spire Finial on Main Dome
            sink->AddLine(D2D1::Point2F(domeCX, domeBY - domeH - 8.f * dpiScale));
            sink->AddLine(D2D1::Point2F(domeCX, domeBY - domeH));
            // Right half of grand dome arch
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(domeCX + domeW * 0.60f, domeBY - domeH * 0.90f),
                D2D1::Point2F(domeCX + domeW * 1.15f, domeBY - domeH * 0.45f),
                D2D1::Point2F(domeCX + domeW, domeBY)));

            // Side Dome 2 (Right)
            float sDome2X = domeCX + 32.f * dpiScale;
            sink->AddLine(D2D1::Point2F(sDome2X - 16.f * dpiScale, baseY - 16.f * dpiScale));
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(sDome2X - 16.f * dpiScale, baseY - 32.f * dpiScale),
                D2D1::Point2F(sDome2X, baseY - 36.f * dpiScale),
                D2D1::Point2F(sDome2X, baseY - 40.f * dpiScale)));
            sink->AddBezier(D2D1::BezierSegment(
                D2D1::Point2F(sDome2X, baseY - 36.f * dpiScale),
                D2D1::Point2F(sDome2X + 16.f * dpiScale, baseY - 32.f * dpiScale),
                D2D1::Point2F(sDome2X + 16.f * dpiScale, baseY - 14.f * dpiScale)));

            // Minaret 2 (Right tall tower)
            sink->AddLine(D2D1::Point2F(min2X - minW/2, baseY - 14.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X - minW/2, min2Top + 24.f * dpiScale));
            // Balcony 1
            sink->AddLine(D2D1::Point2F(min2X - minW/2 - 2.5f*dpiScale, min2Top + 24.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X - minW/2 - 2.5f*dpiScale, min2Top + 21.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X - minW/2 + 1.f*dpiScale, min2Top + 21.f * dpiScale));
            // Shaft upper
            sink->AddLine(D2D1::Point2F(min2X - minW/2 + 1.f*dpiScale, min2Top + 9.f * dpiScale));
            // Balcony 2
            sink->AddLine(D2D1::Point2F(min2X - minW/2 - 2.f*dpiScale, min2Top + 9.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X - minW/2 - 2.f*dpiScale, min2Top + 7.f * dpiScale));
            // Conical spire
            sink->AddLine(D2D1::Point2F(min2X, min2Top));
            sink->AddLine(D2D1::Point2F(min2X + minW/2 + 2.f*dpiScale, min2Top + 7.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X + minW/2 + 2.f*dpiScale, min2Top + 9.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X + minW/2 - 1.f*dpiScale, min2Top + 9.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X + minW/2 - 1.f*dpiScale, min2Top + 21.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X + minW/2 + 2.5f*dpiScale, min2Top + 21.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X + minW/2 + 2.5f*dpiScale, min2Top + 24.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X + minW/2, min2Top + 24.f * dpiScale));
            sink->AddLine(D2D1::Point2F(min2X + minW/2, baseY));

            sink->AddLine(D2D1::Point2F(bounds.right, baseY));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            sink->Release();

            if (solidBrush) {
                solidBrush->SetColor(mosqueColor);
                rt->FillGeometry(mosqueGeo, solidBrush);
            }
        }
        mosqueGeo->Release();
    }

    // ── 6. Left Fading Blend Mask ─────────────────────────────────────────────
    // Smoothly dissolve the illustration toward the left so that the Arabic
    // Dhikr text floating in front is 100% crisp and readable.
    D2D1_GRADIENT_STOP maskStops[] = {
        { 0.00f, ds::Aurora::CardTop() },
        { 0.40f, ds::Aurora::CardTop() },
        { 0.70f, ds::Rgba(0x13, 0x1D, 0x16, 0.45f) },
        { 1.00f, D2D1::ColorF(0, 0, 0, 0.0f) }
    };
    ID2D1GradientStopCollection* maskStopCol = nullptr;
    if (SUCCEEDED(rt->CreateGradientStopCollection(maskStops, 4, &maskStopCol)) && maskStopCol) {
        ID2D1LinearGradientBrush* maskBrush = nullptr;
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES mprop = {
            D2D1::Point2F(bounds.left, bounds.top),
            D2D1::Point2F(bounds.right, bounds.top)
        };
        if (SUCCEEDED(rt->CreateLinearGradientBrush(mprop, maskStopCol, &maskBrush)) && maskBrush) {
            rt->FillRectangle(bounds, maskBrush);
            maskBrush->Release();
        }
        maskStopCol->Release();
    }

    if (solidBrush) {
        solidBrush->Release();
    }
    factory->Release();
}

} // namespace dhikr
