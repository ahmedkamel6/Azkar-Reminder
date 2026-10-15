#include "LogoRenderer.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/Settings.h"
#include <algorithm>
#include <cmath>

namespace dhikr {

void LogoRenderer::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& dest, float alpha) {
    if (!rt) return;

    float cx = (dest.left + dest.right) / 2.f;
    float cy = (dest.top + dest.bottom) / 2.f;
    float w  = dest.right - dest.left;
    float h  = dest.bottom - dest.top;
    float R  = std::min(w, h) / 2.f;

    if (R <= 1.f) return;

    ID2D1SolidColorBrush* brush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &brush);
    if (!brush) return;

    bool isPaused  = ds::Accent::IsAppPaused();
    auto accentCol = ds::Accent::Primary(alpha);
    auto glowCol   = isPaused ? ds::Rgba(0x8E, 0x99, 0x91, alpha * 0.15f) : ds::Accent::Glow(alpha * 0.35f);
    auto darkBase  = isPaused ? ds::Rgba(0x14, 0x18, 0x15, alpha) : ds::Rgba(0x0E, 0x14, 0x0F, alpha);
    auto onyxDisc  = isPaused ? ds::Rgba(0x0A, 0x0C, 0x0A, alpha) : ds::Rgba(0x06, 0x08, 0x06, alpha);
    auto innerRing = isPaused ? ds::Rgba(0x28, 0x2E, 0x29, alpha * 0.8f) : ds::Rgba(0x22, 0x30, 0x24, alpha * 0.8f);
    auto ac       = Settings::Instance().GetCopy().accentColor;

    // 1. Outer Glow Aura
    brush->SetColor(glowCol);
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), R * 0.95f, R * 0.95f), brush, std::max(2.f, R * 0.08f));

    // 2. Outer Bezel & Ring
    brush->SetColor(darkBase);
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), R * 0.92f, R * 0.92f), brush);

    brush->SetColor(accentCol);
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), R * 0.92f, R * 0.92f), brush, std::max(1.2f, R * 0.045f));

    // 3. Inner Dark Disc
    brush->SetColor(onyxDisc);
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), R * 0.82f, R * 0.82f), brush);

    brush->SetColor(innerRing);
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), R * 0.82f, R * 0.82f), brush, std::max(1.f, R * 0.025f));

    // 4. Crescent Moon Path Geometry (الهلال المضيء)
    auto* d2dFactory = Application::Instance().D2DFactory();
    if (d2dFactory) {
        ID2D1PathGeometry* pathGeo = nullptr;
        if (SUCCEEDED(d2dFactory->CreatePathGeometry(&pathGeo))) {
            ID2D1GeometrySink* pSink = nullptr;
            if (SUCCEEDED(pathGeo->Open(&pSink))) {
                D2D1_POINT_2F pTop = D2D1::Point2F(cx + R * 0.26f, cy - R * 0.58f);
                D2D1_POINT_2F pBot = D2D1::Point2F(cx + R * 0.48f, cy + R * 0.16f);

                pSink->BeginFigure(pTop, D2D1_FIGURE_BEGIN_FILLED);
                // Outer curve down through left belly
                pSink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(cx - R * 0.45f, cy - R * 0.65f),
                    D2D1::Point2F(cx - R * 0.76f, cy + R * 0.22f),
                    pBot
                ));
                // Inner curve back to top
                pSink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(cx - R * 0.24f, cy + R * 0.32f),
                    D2D1::Point2F(cx - R * 0.20f, cy - R * 0.32f),
                    pTop
                ));
                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pSink->Close();
                pSink->Release();

                // Crescent Glow behind
                brush->SetColor(glowCol);
                rt->DrawGeometry(pathGeo, brush, std::max(3.f, R * 0.12f));

                // Crescent Gradient Fill
                ID2D1LinearGradientBrush* gradBrush = nullptr;
                ID2D1GradientStopCollection* stopColl = nullptr;
                D2D1_GRADIENT_STOP stops[2];
                stops[0].position = 0.0f;
                if (isPaused) {
                    stops[0].color = ds::Rgba(0xC4, 0xCC, 0xC5, alpha); // Platinum silver highlight
                    stops[1].position = 1.0f;
                    stops[1].color = ds::Rgba(0x75, 0x80, 0x77, alpha); // Slate gray base
                } else {
                    stops[0].color = D2D1::ColorF(
                        std::min(1.0f, (ac.r / 255.f) * 1.35f + 0.15f),
                        std::min(1.0f, (ac.g / 255.f) * 1.35f + 0.15f),
                        std::min(1.0f, (ac.b / 255.f) * 1.35f + 0.15f),
                        alpha);
                    stops[1].position = 1.0f;
                    stops[1].color = accentCol;
                }

                if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &stopColl))) {
                    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gradProps = D2D1::LinearGradientBrushProperties(
                        D2D1::Point2F(cx - R * 0.5f, cy - R * 0.5f),
                        D2D1::Point2F(cx + R * 0.4f, cy + R * 0.4f)
                    );
                    rt->CreateLinearGradientBrush(gradProps, stopColl, &gradBrush);
                    stopColl->Release();
                }

                if (gradBrush) {
                    rt->FillGeometry(pathGeo, gradBrush);
                    gradBrush->Release();
                } else {
                    brush->SetColor(accentCol);
                    rt->FillGeometry(pathGeo, brush);
                }

                // Subtle Outer Edge Highlight
                brush->SetColor(ds::Rgba(0xFF, 0xFF, 0xFF, alpha * 0.45f));
                rt->DrawGeometry(pathGeo, brush, std::max(0.75f, R * 0.02f));

                pathGeo->Release();
            }
        }
    }

    brush->Release();
}

HICON LogoRenderer::CreateAccentIcon(int sizePx) {
    if (sizePx <= 0) sizePx = 32;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sizePx;
    bmi.bmiHeader.biHeight = -sizePx; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    auto* d2dFactory = Application::Instance().D2DFactory();
    if (d2dFactory) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
        );

        ID2D1DCRenderTarget* dcRT = nullptr;
        if (SUCCEEDED(d2dFactory->CreateDCRenderTarget(&props, &dcRT)) && dcRT) {
            RECT rc{ 0, 0, sizePx, sizePx };
            dcRT->BindDC(hdcMem, &rc);
            dcRT->BeginDraw();
            dcRT->Clear(D2D1::ColorF(0, 0));

            D2D1_RECT_F dest = D2D1::RectF(1.f, 1.f, (float)sizePx - 1.f, (float)sizePx - 1.f);
            Draw(dcRT, dest, 1.0f);

            dcRT->EndDraw();
            dcRT->Release();
        }
    }

    SelectObject(hdcMem, hOldBitmap);

    // Create Icon from bitmap with alpha channel
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hBitmap;
    ii.hbmColor = hBitmap;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return hIcon;
}

} // namespace dhikr
