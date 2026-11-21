#include "ScreenshotExporter.h"
#include "MainWindow.h"
#include "PopupWindow.h"
#include "DesignSystem.h"
#include "pages/HomePage.h"
#include "../core/Application.h"
#include "../core/Settings.h"
#include "../core/Localization.h"
#include "../core/ContentEngine.h"
#include "../core/Logger.h"
#include "../resource.h"

#include <windows.h>
#include <wincodec.h>
#include <d2d1.h>
#include <dwrite.h>
#include <filesystem>
#include <iostream>
#include <vector>

namespace dhikr {

static std::string WideToNarrow(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return {};
    std::string s(sz, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), sz, nullptr, nullptr);
    return s;
}

static HRESULT SaveWicBitmapToFile(IWICImagingFactory* pFactory, IWICBitmapSource* pBitmap, const std::wstring& filename) {
    if (!pFactory || !pBitmap) return E_POINTER;

    IWICStream* pStream = nullptr;
    HRESULT hr = pFactory->CreateStream(&pStream);
    if (FAILED(hr)) return hr;

    hr = pStream->InitializeFromFilename(filename.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        pStream->Release();
        return hr;
    }

    IWICBitmapEncoder* pEncoder = nullptr;
    hr = pFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &pEncoder);
    if (FAILED(hr)) {
        pStream->Release();
        return hr;
    }

    hr = pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        pEncoder->Release();
        pStream->Release();
        return hr;
    }

    IWICBitmapFrameEncode* pFrame = nullptr;
    hr = pEncoder->CreateNewFrame(&pFrame, nullptr);
    if (SUCCEEDED(hr)) {
        hr = pFrame->Initialize(nullptr);
        UINT w = 0, h = 0;
        pBitmap->GetSize(&w, &h);
        pFrame->SetSize(w, h);

        IWICFormatConverter* pConverter = nullptr;
        hr = pFactory->CreateFormatConverter(&pConverter);
        if (SUCCEEDED(hr)) {
            hr = pConverter->Initialize(
                pBitmap,
                GUID_WICPixelFormat32bppBGRA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0f,
                WICBitmapPaletteTypeCustom
            );
            if (SUCCEEDED(hr)) {
                WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
                pFrame->SetPixelFormat(&format);
                pFrame->WriteSource(pConverter, nullptr);
                pFrame->Commit();
                pEncoder->Commit();
            }
            pConverter->Release();
        }
        pFrame->Release();
    }

    pEncoder->Release();
    pStream->Release();
    return hr;
}

bool ScreenshotExporter::RenderMainWindow(MainWindow& mainWin, int pageId, ColorTheme theme, const std::wstring& lang, const std::wstring& outPath, float scale) {
    Settings::Instance().Modify([&](AppSettings& s) {
        s.colorTheme = theme;
        s.language = lang;
    });
    Localization::Instance().SetLanguage(lang);

    mainWin.m_activePage = pageId;
    if (mainWin.m_pages[pageId]) {
        mainWin.m_pages[pageId]->OnActivated();
    }

    // Set sample statistics on HomePage if it's the home page
    if (pageId == PAGE_HOME && mainWin.m_pages[PAGE_HOME]) {
        auto* home = dynamic_cast<HomePage*>(mainWin.m_pages[PAGE_HOME].get());
        if (home) {
            home->m_todayDelivered = 18;
            home->m_streak = 7;
            home->m_total = 124;
            home->m_dailyStats.clear();
            home->m_dailyStats.push_back({ "Sun", 12 });
            home->m_dailyStats.push_back({ "Mon", 15 });
            home->m_dailyStats.push_back({ "Tue", 18 });
            home->m_dailyStats.push_back({ "Wed", 14 });
            home->m_dailyStats.push_back({ "Thu", 20 });
            home->m_dailyStats.push_back({ "Fri", 17 });
            home->m_dailyStats.push_back({ "Sat", 18 });
        }
    }

    int w = (int)(920 * scale);
    int h = (int)(620 * scale);
    mainWin.m_width = w;
    mainWin.m_height = h;
    mainWin.m_dpiScale = scale;

    if (mainWin.m_navRail) {
        mainWin.m_navRail->SetDpiScale(scale);
        mainWin.m_navRail->SetBounds(mainWin.SidebarRect());
    }

    auto cr = mainWin.ContentRect();
    for (auto& p : mainWin.m_pages) {
        if (p) {
            p->OnSize(cr);
            p->ResetD2D();
        }
    }

    auto* pWicFactory = mainWin.m_wicFactory;
    auto* pD2DFactory = mainWin.m_d2dFactory;
    if (!pWicFactory || !pD2DFactory) return false;

    IWICBitmap* pWicBmp = nullptr;
    HRESULT hr = pWicFactory->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &pWicBmp);
    if (FAILED(hr) || !pWicBmp) return false;

    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    ID2D1RenderTarget* rt = nullptr;
    hr = pD2DFactory->CreateWicBitmapRenderTarget(pWicBmp, rtp, &rt);
    if (FAILED(hr) || !rt) {
        pWicBmp->Release();
        return false;
    }

    // Swap logos to nullptr temporarily so DrawEffectiveLogo uses LogoRenderer::Draw(rt)
    ID2D1Bitmap* oldLogos[6];
    ID2D1Bitmap* oldSmallLogos[6];
    for (int i = 0; i < 6; ++i) {
        oldLogos[i] = mainWin.m_logoBitmaps[i];
        oldSmallLogos[i] = mainWin.m_logoSmallBitmaps[i];
        mainWin.m_logoBitmaps[i] = nullptr;
        mainWin.m_logoSmallBitmaps[i] = nullptr;
    }
    ID2D1Bitmap* oldDormantLogo = mainWin.m_logoDormantBitmap;
    ID2D1Bitmap* oldDormantSmallLogo = mainWin.m_logoDormantSmallBitmap;
    mainWin.m_logoDormantBitmap = nullptr;
    mainWin.m_logoDormantSmallBitmap = nullptr;

    ID2D1SolidColorBrush* wicBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &wicBrush);
    ID2D1SolidColorBrush* oldBrush = mainWin.m_brush;
    mainWin.m_brush = wicBrush;

    rt->BeginDraw();
    rt->Clear(ds::Background());
    mainWin.DrawBackground(rt);
    mainWin.DrawSidebar(rt);
    mainWin.DrawCurrentPage(rt);
    hr = rt->EndDraw();

    // Restore MainWindow state
    if (wicBrush) wicBrush->Release();
    mainWin.m_brush = oldBrush;
    mainWin.m_logoDormantBitmap = oldDormantLogo;
    mainWin.m_logoDormantSmallBitmap = oldDormantSmallLogo;
    for (int i = 0; i < 6; ++i) {
        mainWin.m_logoBitmaps[i] = oldLogos[i];
        mainWin.m_logoSmallBitmaps[i] = oldSmallLogos[i];
    }
    for (auto& p : mainWin.m_pages) {
        if (p) p->ResetD2D();
    }

    bool ok = SUCCEEDED(hr);
    if (ok) {
        hr = SaveWicBitmapToFile(pWicFactory, pWicBmp, outPath);
        ok = SUCCEEDED(hr);
        if (!ok) {
            LOG_ERROR("SaveWicBitmapToFile failed for: " + WideToNarrow(outPath) + " hr=0x" + std::to_string((unsigned long)hr));
        } else {
            LOG_INFO("Saved screenshot: " + WideToNarrow(outPath));
        }
    } else {
        LOG_ERROR("EndDraw failed for: " + WideToNarrow(outPath) + " hr=0x" + std::to_string((unsigned long)hr));
    }

    rt->Release();
    pWicBmp->Release();
    return ok;
}

bool ScreenshotExporter::RenderPopup(PopupWindow& popupWin, ColorTheme theme, const std::wstring& outPath, bool withBackdrop, float scale) {
    Settings::Instance().Modify([theme](AppSettings& s) {
        s.colorTheme = theme;
    });

    ContentItem sample;
    sample.text = L"سُبْحَانَ اللَّهِ وَبِحَمْدِهِ ، سُبْحَانَ اللَّهِ الْعَظِيمِ";
    sample.source = L"صحيح البخاري ومسلم - كلمتان خفيفتان على اللسان ثقيلتان في الميزان";
    sample.repetitions = 33;
    sample.category = "tasbih";

    popupWin.m_item = &sample;
    popupWin.m_currentCounter = 1;
    popupWin.m_targetCounter = 33;

    int pw = (int)(460 * scale);
    int ph = (int)(130 * scale);
    popupWin.m_width = pw;
    popupWin.m_height = ph;
    popupWin.m_dpiScale = scale;

    int cw = withBackdrop ? (pw + (int)(80 * scale)) : pw;
    int ch = withBackdrop ? (ph + (int)(70 * scale)) : ph;

    auto* pWicFactory = popupWin.m_wicFactory;
    auto* pD2DFactory = popupWin.m_d2dFactory;
    if (!pWicFactory || !pD2DFactory) return false;

    IWICBitmap* pWicBmp = nullptr;
    HRESULT hr = pWicFactory->CreateBitmap(cw, ch, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &pWicBmp);
    if (FAILED(hr) || !pWicBmp) return false;

    D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    ID2D1RenderTarget* rt = nullptr;
    hr = pD2DFactory->CreateWicBitmapRenderTarget(pWicBmp, rtp, &rt);
    if (FAILED(hr) || !rt) {
        pWicBmp->Release();
        return false;
    }

    // Swap logos to nullptr temporarily so DrawCrescentMark uses LogoRenderer::Draw(rt)
    ID2D1Bitmap* oldLogos[6];
    for (int i = 0; i < 6; ++i) {
        oldLogos[i] = popupWin.m_logoBitmaps[i];
        popupWin.m_logoBitmaps[i] = nullptr;
    }

    rt->BeginDraw();

    if (withBackdrop) {
        // Soft atmospheric desktop backdrop gradient
        D2D1_RECT_F fullRect = D2D1::RectF(0, 0, (float)cw, (float)ch);
        ID2D1SolidColorBrush* bgBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0x0A / 255.f, 0x0E / 255.f, 0x0C / 255.f, 1.0f), &bgBrush);
        if (bgBrush) {
            rt->FillRectangle(fullRect, bgBrush);
            bgBrush->Release();
        }

        // Drop shadow for floating popup
        float ox = (float)(cw - pw) / 2.f;
        float oy = (float)(ch - ph) / 2.f;

        D2D1_RECT_F shadowRect = D2D1::RectF(ox - 4.f*scale, oy + 4.f*scale, ox + pw + 4.f*scale, oy + ph + 8.f*scale);
        ID2D1SolidColorBrush* shadowBrush = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.45f), &shadowBrush);
        if (shadowBrush) {
            rt->FillRoundedRectangle(D2D1::RoundedRect(shadowRect, 16.f*scale, 16.f*scale), shadowBrush);
            shadowBrush->Release();
        }

        rt->SetTransform(D2D1::Matrix3x2F::Translation(ox, oy));
    } else {
        rt->Clear(D2D1::ColorF(0, 0, 0, 0.0f));
    }

    popupWin.DrawBackground(rt, 1.0f);
    popupWin.DrawAccentBar(rt, 1.0f);
    popupWin.DrawCrescentMark(rt, 1.0f);
    popupWin.DrawContent(rt, 1.0f);
    popupWin.DrawCounterPill(rt, 1.0f);
    popupWin.DrawCloseButton(rt, 1.0f);
    popupWin.DrawCopyButton(rt, 1.0f);
    popupWin.DrawPauseButton(rt, 1.0f);
    popupWin.DrawGrainTexture(rt, 1.0f);

    rt->SetTransform(D2D1::Matrix3x2F::Identity());
    hr = rt->EndDraw();

    for (int i = 0; i < 6; ++i) {
        popupWin.m_logoBitmaps[i] = oldLogos[i];
    }

    bool ok = SUCCEEDED(hr);
    if (ok) {
        hr = SaveWicBitmapToFile(pWicFactory, pWicBmp, outPath);
        ok = SUCCEEDED(hr);
        if (!ok) {
            LOG_ERROR("SaveWicBitmapToFile failed for popup: " + WideToNarrow(outPath) + " hr=0x" + std::to_string((unsigned long)hr));
        } else {
            LOG_INFO("Saved popup screenshot: " + WideToNarrow(outPath));
        }
    } else {
        LOG_ERROR("EndDraw failed for popup: " + WideToNarrow(outPath) + " hr=0x" + std::to_string((unsigned long)hr));
    }

    rt->Release();
    pWicBmp->Release();
    return ok;
}

bool ScreenshotExporter::ExportAll(const std::wstring& outputDir) {
    namespace fs = std::filesystem;
    try {
        fs::create_directories(outputDir);
    } catch (...) {}

    auto& mainWin = MainWindow::Instance();
    auto& popupWin = PopupWindow::Instance();

    // Preserve original settings
    const auto origSettings = Settings::Instance().GetCopy();
    ColorTheme origTheme = origSettings.colorTheme;
    std::wstring origLang = origSettings.language;
    int origPage = mainWin.m_activePage;

    LOG_INFO("Exporting High-Quality Screenshots...");

    // 1. Home Dashboard - Emerald Green (الواجهة الرئيسية - الثيم الزمردي)
    RenderMainWindow(mainWin, PAGE_HOME, ColorTheme::Green, L"ar",
                     outputDir + L"/01_الواجهة_الرئيسية_Home_الزمردي.png");

    // 2. Reminders & Schedule - Emerald Green (التذكيرات والجدول)
    RenderMainWindow(mainWin, PAGE_REMINDERS, ColorTheme::Green, L"ar",
                     outputDir + L"/02_التذكيرات_والجدول_Reminders_الزمردي.png");

    // 3. Dhikr Library - Emerald Green (مكتبة الأذكار)
    RenderMainWindow(mainWin, PAGE_LIBRARY, ColorTheme::Green, L"ar",
                     outputDir + L"/03_مكتبة_الأذكار_Library_الزمردي.png");

    // 4. Settings - Emerald Green (الإعدادات)
    RenderMainWindow(mainWin, PAGE_SETTINGS, ColorTheme::Green, L"ar",
                     outputDir + L"/04_الإعدادات_Settings_الزمردي.png");

    // 5. Home Dashboard - Radiant Gold Theme (الواجهة الرئيسية - الثيم الذهبي)
    RenderMainWindow(mainWin, PAGE_HOME, ColorTheme::Gold, L"ar",
                     outputDir + L"/05_الواجهة_الرئيسية_Home_الذهبي.png");

    // 6. Home Dashboard - Electric Cyan Blue Theme (الواجهة الرئيسية - الثيم الأزرق السماوي)
    RenderMainWindow(mainWin, PAGE_HOME, ColorTheme::Blue, L"ar",
                     outputDir + L"/06_الواجهة_الرئيسية_Home_الأزرق.png");

    // 7. Home Dashboard - Platinum Silver Theme (الواجهة الرئيسية - الثيم الفضي)
    RenderMainWindow(mainWin, PAGE_HOME, ColorTheme::Silver, L"ar",
                     outputDir + L"/07_الواجهة_الرئيسية_Home_الفضي.png");

    // 8. Notification Popup - Emerald Green (إشعار الذكر الزمردي بخلفية شفافة)
    RenderPopup(popupWin, ColorTheme::Green,
                outputDir + L"/08_إشعار_الذكر_Notification_Popup_الزمردي.png", false);

    // 9. Notification Popup - Gold Theme (إشعار الذكر بالثيم الذهبي)
    RenderPopup(popupWin, ColorTheme::Gold,
                outputDir + L"/09_إشعار_الذكر_Notification_Popup_الذهبي.png", false);

    // 10. Notification Popup on Desktop Backdrop (إشعار الذكر على سطح المكتب مع الظل)
    RenderPopup(popupWin, ColorTheme::Green,
                outputDir + L"/10_إشعار_الذكر_على_سطح_المكتب_Popup_Desktop.png", true);

    // Restore original settings
    Settings::Instance().Modify([&](AppSettings& s) {
        s.colorTheme = origTheme;
        s.language = origLang;
    });
    Localization::Instance().SetLanguage(origLang);
    mainWin.m_activePage = origPage;

    LOG_INFO("All screenshots exported successfully to: " + WideToNarrow(outputDir));
    return true;
}

} // namespace dhikr
