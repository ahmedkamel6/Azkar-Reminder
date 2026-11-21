#include "AboutPage.h"
#include "../MainWindow.h"
#include "../DesignSystem.h"
#include "../core/LogoRenderer.h"
#include "../core/ResourceLoader.h"
#include "../../core/Application.h"
#include "../../core/ApplicationMetadata.h"
#include "../../core/Localization.h"
#include "../../resource.h"
#include <shellapi.h>

namespace dhikr {

AboutPage::AboutPage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;
    m_dpiScale = MainWindow::Instance().GetDpiScale();
    if (m_dpiScale <= 0.f) m_dpiScale = 1.f;

    auto* cairoCol = Application::Instance().CairoFontCollection();
    auto mk = [&](float sz, DWRITE_FONT_WEIGHT wt, DWRITE_TEXT_ALIGNMENT ta, DWRITE_PARAGRAPH_ALIGNMENT pa, IDWriteTextFormat** out, const wchar_t* fontName = nullptr) {
        dwf->CreateTextFormat(fontName ? fontName : ds::Font::Arabic(), fontName ? nullptr : cairoCol, wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sz * m_dpiScale, L"en-US", out);
        if (*out) {
            (*out)->SetTextAlignment(ta);
            (*out)->SetParagraphAlignment(pa);
        }
    };
    mk(20.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfTitle);
    mk(16.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfSectionHeader);
    mk(11.5f, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfSectionSub);
    mk(ds::FontSize::Body, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfSub);
    mk(ds::FontSize::Body, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfBody);
    mk(ds::FontSize::Small, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfSmall);
    mk(ds::FontSize::BtnLabel, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfBtn);
    mk(11.f, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfMono, L"Consolas");
    mk(9.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, &m_tfBadge);
}

AboutPage::~AboutPage() {
    if (m_tfTitle)         { m_tfTitle->Release();         m_tfTitle         = nullptr; }
    if (m_tfSectionHeader) { m_tfSectionHeader->Release(); m_tfSectionHeader = nullptr; }
    if (m_tfSectionSub)    { m_tfSectionSub->Release();    m_tfSectionSub    = nullptr; }
    if (m_tfSub)           { m_tfSub->Release();           m_tfSub           = nullptr; }
    if (m_tfBody)          { m_tfBody->Release();          m_tfBody          = nullptr; }
    if (m_tfSmall)         { m_tfSmall->Release();         m_tfSmall         = nullptr; }
    if (m_tfBtn)           { m_tfBtn->Release();           m_tfBtn           = nullptr; }
    if (m_tfMono)          { m_tfMono->Release();          m_tfMono          = nullptr; }
    if (m_tfBadge)         { m_tfBadge->Release();         m_tfBadge         = nullptr; }
    if (m_brush)           { m_brush->Release();           m_brush           = nullptr; }
    if (m_bmpQrVodafone)   { m_bmpQrVodafone->Release();   m_bmpQrVodafone   = nullptr; }
    if (m_bmpQrIban)       { m_bmpQrIban->Release();       m_bmpQrIban       = nullptr; }
}

void AboutPage::EnsureQrBitmaps(ID2D1RenderTarget* rt) {
    if (!m_bmpQrVodafone) {
        m_bmpQrVodafone = ResourceLoader::LoadBitmapFromResource(
            MainWindow::Instance().GetHInstance(),
            IDR_QR_VODAFONE,
            MainWindow::Instance().GetWicFactory(),
            rt,
            RT_RCDATA
        );
    }
    if (!m_bmpQrIban) {
        m_bmpQrIban = ResourceLoader::LoadBitmapFromResource(
            MainWindow::Instance().GetHInstance(),
            IDR_QR_IBAN,
            MainWindow::Instance().GetWicFactory(),
            rt,
            RT_RCDATA
        );
    }
}

void AboutPage::DrawCrescentLogo(ID2D1RenderTarget* rt, float cx, float cy, float radius) {
    D2D1_RECT_F dest = D2D1::RectF(cx - radius, cy - radius, cx + radius, cy + radius);
    MainWindow::Instance().DrawEffectiveLogo(rt, dest, false, 1.0f);
}

void AboutPage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);
    EnsureQrBitmaps(rt);

    bool isRtl = Localization::Instance().IsRTL();
    DWRITE_READING_DIRECTION rd = isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    if (m_tfTitle) m_tfTitle->SetReadingDirection(rd);
    if (m_tfSectionHeader) m_tfSectionHeader->SetReadingDirection(rd);
    if (m_tfSectionSub) m_tfSectionSub->SetReadingDirection(rd);
    if (m_tfSub) m_tfSub->SetReadingDirection(rd);
    if (m_tfBody) m_tfBody->SetReadingDirection(rd);
    if (m_tfSmall) m_tfSmall->SetReadingDirection(rd);
    if (m_tfBtn) m_tfBtn->SetReadingDirection(rd);

    float cx = (bounds.left + bounds.right)  / 2.f;
    float w  = bounds.right - bounds.left;

    // Vertical Centering: calculate total content height and optical center
    float viewH = bounds.bottom - bounds.top;
    float totalContentH = 394.f * m_dpiScale;
    float startY = bounds.top + std::max(16.f * m_dpiScale, (viewH - totalContentH) / 2.f);

    // ── 1. Hero Logo & Header Section ─────────────────────────────────────────
    float logoR = 30.f * m_dpiScale;
    float logoY = startY;

    // Soft, feather-soft Radial Gradient Glow behind crescent logo
    float glowR = logoR * 2.4f;
    D2D1_GRADIENT_STOP stops[3] = {
        { 0.0f, ds::Accent::Primary(0.28f) },
        { 0.45f, ds::Accent::Primary(0.08f) },
        { 1.0f, ds::Accent::Primary(0.0f)  }
    };
    ID2D1GradientStopCollection* pStops = nullptr;
    HRESULT hr = rt->CreateGradientStopCollection(stops, 3, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pStops);
    if (SUCCEEDED(hr) && pStops) {
        ID2D1RadialGradientBrush* pRadBrush = nullptr;
        D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radProps = D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(cx, logoY + logoR),
            D2D1::Point2F(0, 0),
            glowR, glowR
        );
        hr = rt->CreateRadialGradientBrush(radProps, pStops, &pRadBrush);
        if (SUCCEEDED(hr) && pRadBrush) {
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, logoY + logoR), glowR, glowR), pRadBrush);
            pRadBrush->Release();
        }
        pStops->Release();
    }

    DrawCrescentLogo(rt, cx, logoY + logoR, logoR);

    float y = logoY + logoR * 2 + 8.f * m_dpiScale;

    // App Name
    float titleH = 26.f * m_dpiScale;
    D2D1_RECT_F titleR = D2D1::RectF(bounds.left, y, bounds.right, y + titleH);
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        std::wstring name = ApplicationMetadata::AppName;
        rt->DrawTextW(name.c_str(), (UINT32)name.size(), m_tfTitle, titleR, m_brush);
    }
    y += titleH + 2.f * m_dpiScale;

    // Tagline & Version in one harmonious row
    float tagH = 18.f * m_dpiScale;
    if (m_tfSub) {
        m_brush->SetColor(ds::TextSecondary());
        std::wstring version = Localization::Instance().Get(L"about.version") + L" " + ApplicationMetadata::AppVersion;
        std::wstring tagline = Localization::Instance().Get(L"home.subtitle");
        std::wstring fullSub = tagline + L"  •  " + version;
        rt->DrawTextW(fullSub.c_str(), (UINT32)fullSub.size(), m_tfSub,
            D2D1::RectF(bounds.left + 20*m_dpiScale, y, bounds.right - 20*m_dpiScale, y + tagH), m_brush);
    }
    y += tagH + 14.f * m_dpiScale;

    // ── 2. Prominent "Support the Developer" Section Header ────────────────────
    // Elegant accent divider with jewel
    float divW = 120.f * m_dpiScale;
    m_brush->SetColor(ds::BorderSubtle());
    rt->DrawLine(D2D1::Point2F(cx - divW, y), D2D1::Point2F(cx - 16.f*m_dpiScale, y), m_brush, 0.75f);
    rt->DrawLine(D2D1::Point2F(cx + 16.f*m_dpiScale, y), D2D1::Point2F(cx + divW, y), m_brush, 0.75f);
    m_brush->SetColor(ds::Accent::Primary(0.8f));
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, y), 3.f*m_dpiScale, 3.f*m_dpiScale), m_brush);

    y += 10.f * m_dpiScale;

    // Support Section Title (16pt, SemiBold, high-contrast, prominent!)
    if (m_tfSectionHeader) {
        m_brush->SetColor(ds::TextPrimary());
        std::wstring supportTitle = Localization::Instance().Get(L"about.support");
        rt->DrawTextW(supportTitle.c_str(), (UINT32)supportTitle.size(), m_tfSectionHeader,
            D2D1::RectF(bounds.left, y, bounds.right, y + 24.f * m_dpiScale), m_brush);
    }
    y += 24.f * m_dpiScale;

    // Support Subtitle
    if (m_tfSectionSub) {
        m_brush->SetColor(ds::TextSecondary());
        std::wstring supportSub = Localization::Instance().Get(L"about.support_sub");
        rt->DrawTextW(supportSub.c_str(), (UINT32)supportSub.size(), m_tfSectionSub,
            D2D1::RectF(bounds.left + 24*m_dpiScale, y, bounds.right - 24*m_dpiScale, y + 18.f * m_dpiScale), m_brush);
    }
    y += 20.f * m_dpiScale;

    // ── 3. Two Modern FinTech Cards (Vodafone Cash + Banque Misr IBAN) ─────────
    float cardGap = 16.f * m_dpiScale;
    float maxCardsW = std::min(w - 32.f * m_dpiScale, 600.f * m_dpiScale);
    float cardW = (maxCardsW - cardGap) / 2.f;
    float cardH = 114.f * m_dpiScale;
    float totalCardsW = cardW * 2.f + cardGap;
    float startCardsX = cx - totalCardsW / 2.f;

    m_rectVodafoneCard = D2D1::RectF(startCardsX, y, startCardsX + cardW, y + cardH);
    m_rectIbanCard     = D2D1::RectF(startCardsX + cardW + cardGap, y, startCardsX + totalCardsW, y + cardH);

    // Helper for buttons
    auto drawCardButton = [&](int btnIdx, const D2D1_RECT_F& r, const std::wstring& text, bool isPrimary) {
        D2D1_RECT_F cr = r;
        if (m_pressedBtn == btnIdx) { cr.left += 1; cr.top += 1; cr.right -= 1; cr.bottom -= 1; }
        
        bool isHover = (m_hoveredBtn == btnIdx);
        bool isCopied = (m_justCopiedBtn == btnIdx && GetTickCount() - m_copyTime < 2000);

        if (isPrimary) {
            // Emerald Accent Button
            m_brush->SetColor(isHover ? ds::Accent::Primary(0.85f) : ds::Accent::Primary());
            rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush);
            m_brush->SetColor(ds::Rgba(0x06, 0x0D, 0x08));
        } else {
            // Surface Outline Button
            m_brush->SetColor(isHover ? ds::CardHover() : ds::Surface());
            rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush);
            m_brush->SetColor(isCopied ? ds::Accent::Primary() : (isHover ? ds::Accent::Primary(0.6f) : ds::Border()));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(cr, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush, 1.f);
            m_brush->SetColor(isCopied ? ds::Accent::Primary() : ds::TextPrimary());
        }

        std::wstring label = isCopied ? Localization::Instance().Get(L"about.copied") : text;
        rt->DrawTextW(label.c_str(), (UINT32)label.size(), m_tfBtn, cr, m_brush);
    };

    // ── Card 1: Vodafone Cash ──
    {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(m_rectVodafoneCard, 10.f*m_dpiScale, 10.f*m_dpiScale);
        bool isHoverCard = (m_hoveredBtn == 3 || m_hoveredBtn == 4);
        m_brush->SetColor(isHoverCard ? ds::CardHover() : ds::Card());
        rt->FillRoundedRectangle(rr, m_brush);
        m_brush->SetColor(isHoverCard ? ds::Rgba(0xE6, 0x30, 0x30, 0.6f) : ds::BorderSubtle());
        rt->DrawRoundedRectangle(rr, m_brush, 1.f);

        // Header Row: Service Badge (Red tint) + Type Tag
        float pad = 12.f * m_dpiScale;
        float hY = m_rectVodafoneCard.top + pad;
        float badgeW = 96.f * m_dpiScale;
        float badgeH = 18.f * m_dpiScale;
        
        float badgeX = isRtl ? (m_rectVodafoneCard.right - pad - badgeW) : (m_rectVodafoneCard.left + pad);
        D2D1_RECT_F bR = D2D1::RectF(badgeX, hY, badgeX + badgeW, hY + badgeH);
        m_brush->SetColor(ds::Rgba(0x35, 0x12, 0x12));
        rt->FillRoundedRectangle(D2D1::RoundedRect(bR, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush);
        m_brush->SetColor(ds::Rgba(0xE6, 0x30, 0x30, 0.4f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(bR, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush, 0.75f);
        m_brush->SetColor(ds::Rgba(0xFF, 0x68, 0x68));
        std::wstring vfName = Localization::Instance().Get(L"about.vodafone");
        if (m_tfBadge) rt->DrawTextW(vfName.c_str(), (UINT32)vfName.size(), m_tfBadge, bR, m_brush);

        // Type Tag on opposite side
        float tagX = isRtl ? (m_rectVodafoneCard.left + pad) : (m_rectVodafoneCard.right - pad - 80.f*m_dpiScale);
        D2D1_RECT_F tR = D2D1::RectF(tagX, hY, tagX + 80.f*m_dpiScale, hY + badgeH);
        m_brush->SetColor(ds::TextMuted());
        std::wstring wTag = Localization::Instance().Get(L"about.wallet");
        if (m_tfBadge) rt->DrawTextW(wTag.c_str(), (UINT32)wTag.size(), m_tfBadge, tR, m_brush);

        // Center Phone Number Box
        float numBoxY = hY + badgeH + 8.f * m_dpiScale;
        float numBoxH = 28.f * m_dpiScale;
        D2D1_RECT_F nbR = D2D1::RectF(m_rectVodafoneCard.left + pad, numBoxY, m_rectVodafoneCard.right - pad, numBoxY + numBoxH);
        m_brush->SetColor(ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(nbR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush);
        m_brush->SetColor(ds::BorderSubtle());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(nbR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush, 0.5f);

        // Formatted display number: 0114 399 5232
        std::wstring vfDisplay = L"0114 399 5232";
        m_brush->SetColor(ds::TextPrimary());
        if (m_tfMono) {
            DWRITE_READING_DIRECTION rdM = m_tfMono->GetReadingDirection();
            m_tfMono->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
            rt->DrawTextW(vfDisplay.c_str(), (UINT32)vfDisplay.size(), m_tfMono, nbR, m_brush);
            m_tfMono->SetReadingDirection(rdM);
        }

        // Action Buttons Row
        float btnY = numBoxY + numBoxH + 10.f * m_dpiScale;
        float bH = 26.f * m_dpiScale;
        float bW = (cardW - pad * 2.f - 10.f * m_dpiScale) / 2.f;

        m_btnVodafoneCopy = D2D1::RectF(m_rectVodafoneCard.left + pad, btnY, m_rectVodafoneCard.left + pad + bW, btnY + bH);
        m_btnVodafoneQr   = D2D1::RectF(m_rectVodafoneCard.left + pad + bW + 10.f*m_dpiScale, btnY, m_rectVodafoneCard.right - pad, btnY + bH);

        drawCardButton(3, m_btnVodafoneCopy, Localization::Instance().Get(L"about.copy_num"), false);
        drawCardButton(4, m_btnVodafoneQr,   Localization::Instance().Get(L"about.scan_qr"), true);
    }

    // ── Card 2: Banque Misr IBAN ──
    {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(m_rectIbanCard, 10.f*m_dpiScale, 10.f*m_dpiScale);
        bool isHoverCard = (m_hoveredBtn == 5 || m_hoveredBtn == 6);
        m_brush->SetColor(isHoverCard ? ds::CardHover() : ds::Card());
        rt->FillRoundedRectangle(rr, m_brush);
        m_brush->SetColor(isHoverCard ? ds::Accent::Primary(0.6f) : ds::BorderSubtle());
        rt->DrawRoundedRectangle(rr, m_brush, 1.f);

        // Header Row: Service Badge (Amber/Gold tint) + Beneficiary Name
        float pad = 12.f * m_dpiScale;
        float hY = m_rectIbanCard.top + pad;
        float badgeW = 86.f * m_dpiScale;
        float badgeH = 18.f * m_dpiScale;
        
        float badgeX = isRtl ? (m_rectIbanCard.right - pad - badgeW) : (m_rectIbanCard.left + pad);
        D2D1_RECT_F bR = D2D1::RectF(badgeX, hY, badgeX + badgeW, hY + badgeH);
        m_brush->SetColor(ds::Rgba(0x28, 0x22, 0x0E));
        rt->FillRoundedRectangle(D2D1::RoundedRect(bR, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush);
        m_brush->SetColor(ds::Rgba(0xDA, 0xA5, 0x20, 0.4f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(bR, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush, 0.75f);
        m_brush->SetColor(ds::Rgba(0xFA, 0xBA, 0x32));
        std::wstring bankName = ApplicationMetadata::BankName;
        if (m_tfBadge) rt->DrawTextW(bankName.c_str(), (UINT32)bankName.size(), m_tfBadge, bR, m_brush);

        // Beneficiary Name on opposite side
        float nameX = isRtl ? (m_rectIbanCard.left + pad) : (m_rectIbanCard.right - pad - 120.f*m_dpiScale);
        D2D1_RECT_F nR = D2D1::RectF(nameX, hY, nameX + 120.f*m_dpiScale, hY + badgeH);
        m_brush->SetColor(ds::TextSecondary());
        std::wstring benName = ApplicationMetadata::BeneficiaryName;
        if (m_tfBadge) {
            DWRITE_READING_DIRECTION rdB = m_tfBadge->GetReadingDirection();
            m_tfBadge->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
            rt->DrawTextW(benName.c_str(), (UINT32)benName.size(), m_tfBadge, nR, m_brush);
            m_tfBadge->SetReadingDirection(rdB);
        }

        // Center IBAN Box
        float numBoxY = hY + badgeH + 8.f * m_dpiScale;
        float numBoxH = 28.f * m_dpiScale;
        D2D1_RECT_F nbR = D2D1::RectF(m_rectIbanCard.left + pad, numBoxY, m_rectIbanCard.right - pad, numBoxY + numBoxH);
        m_brush->SetColor(ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(nbR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush);
        m_brush->SetColor(ds::BorderSubtle());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(nbR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush, 0.5f);

        // Formatted IBAN
        std::wstring ibanStr = ApplicationMetadata::IBANFormatted;
        m_brush->SetColor(ds::TextPrimary());
        if (m_tfMono) {
            DWRITE_READING_DIRECTION rdM = m_tfMono->GetReadingDirection();
            m_tfMono->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
            rt->DrawTextW(ibanStr.c_str(), (UINT32)ibanStr.size(), m_tfMono, nbR, m_brush);
            m_tfMono->SetReadingDirection(rdM);
        }

        // Action Buttons Row
        float btnY = numBoxY + numBoxH + 10.f * m_dpiScale;
        float bH = 26.f * m_dpiScale;
        float bW = (cardW - pad * 2.f - 10.f * m_dpiScale) / 2.f;

        m_btnIbanCopy = D2D1::RectF(m_rectIbanCard.left + pad, btnY, m_rectIbanCard.left + pad + bW, btnY + bH);
        m_btnIbanQr   = D2D1::RectF(m_rectIbanCard.left + pad + bW + 10.f*m_dpiScale, btnY, m_rectIbanCard.right - pad, btnY + bH);

        drawCardButton(5, m_btnIbanCopy, Localization::Instance().Get(L"about.copy_iban"), false);
        drawCardButton(6, m_btnIbanQr,   Localization::Instance().Get(L"about.scan_qr"), true);
    }

    y += cardH + 16.f * m_dpiScale;

    // ── 4. Sadaqah Jariyah Dedication Banner ───────────────────────────────────
    float quoteW = std::min(w - 40.f * m_dpiScale, 520.f * m_dpiScale);
    float quoteH = 24.f * m_dpiScale;
    D2D1_RECT_F qR = D2D1::RectF(cx - quoteW / 2.f, y, cx + quoteW / 2.f, y + quoteH);
    m_brush->SetColor(ds::Rgba(0x10, 0x1A, 0x13, 0.6f));
    rt->FillRoundedRectangle(D2D1::RoundedRect(qR, 12.f*m_dpiScale, 12.f*m_dpiScale), m_brush);
    m_brush->SetColor(ds::Accent::Primary(0.25f));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(qR, 12.f*m_dpiScale, 12.f*m_dpiScale), m_brush, 0.75f);

    m_brush->SetColor(ds::TextSecondary());
    std::wstring sadaqah = isRtl ? L"« صدقة جارية عني وعن والديّ وأهلي • تقبل الله منا ومنكم »"
                                 : L"« A continuous charity for my family and me • May Allah accept »";
    if (m_tfSmall) {
        rt->DrawTextW(sadaqah.c_str(), (UINT32)sadaqah.size(), m_tfSmall, qR, m_brush);
    }
    y += quoteH + 10.f * m_dpiScale;

    // ── 5. Developer Contact Info (Email) ──────────────────────────────────────
    if (m_tfSmall) {
        std::wstring email = ApplicationMetadata::DeveloperEmail;
        float emailW = 230.f * m_dpiScale;
        float btnW = 56.f * m_dpiScale;
        float gap = 8.f * m_dpiScale;
        float totalW = emailW + gap + btnW;
        float startX = cx - totalW / 2.f;

        m_rectEmail = D2D1::RectF(startX, y, startX + emailW, y + 20.f * m_dpiScale);
        m_rectCopy  = D2D1::RectF(startX + emailW + gap, y, startX + totalW, y + 20.f * m_dpiScale);

        if (m_hoveredBtn == 1) {
            m_brush->SetColor(ds::CardHover());
            rt->FillRoundedRectangle(D2D1::RoundedRect(m_rectEmail, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush);
        }

        m_brush->SetColor(ds::Accent::Primary());
        DWRITE_READING_DIRECTION rdE = m_tfSmall->GetReadingDirection();
        m_tfSmall->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        rt->DrawTextW(email.c_str(), (UINT32)email.size(), m_tfSmall, m_rectEmail, m_brush);
        m_tfSmall->SetReadingDirection(rdE);

        // Copy button
        if (m_tfBtn) {
            D2D1_RECT_F cr = m_rectCopy;
            if (m_pressedBtn == 2) { cr.left += 1; cr.top += 1; cr.right -= 1; cr.bottom -= 1; }
            bool isCopied = (m_justCopiedBtn == 2 && GetTickCount() - m_copyTime < 2000);
            m_brush->SetColor(m_hoveredBtn == 2 ? ds::CardHover() : ds::Surface());
            rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush);
            m_brush->SetColor(isCopied ? ds::Accent::Primary() : ds::Border());
            rt->DrawRoundedRectangle(D2D1::RoundedRect(cr, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush, 1.f);

            std::wstring btnLabel = isCopied ? Localization::Instance().Get(L"about.copied") : Localization::Instance().Get(L"about.copy");
            m_brush->SetColor(isCopied ? ds::Accent::Primary() : ds::TextPrimary());
            rt->DrawTextW(btnLabel.c_str(), (UINT32)btnLabel.size(), m_tfBtn, cr, m_brush);
        }
    }

    // ── 6. QR Code Modal Dialog (if opened) ────────────────────────────────────
    if (m_activeModal != 0) {
        DrawQrModal(rt, bounds);
    }
}

void AboutPage::DrawQrModal(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    float cx = (bounds.left + bounds.right) / 2.f;
    float cy = (bounds.top + bounds.bottom) / 2.f;

    // Semi-transparent backdrop overlay
    m_brush->SetColor(D2D1::ColorF(0.f, 0.f, 0.f, 0.72f));
    rt->FillRectangle(bounds, m_brush);

    // Modal dialog box
    float modalW = 320.f * m_dpiScale;
    float modalH = 370.f * m_dpiScale;
    m_modalRect = D2D1::RectF(cx - modalW / 2.f, cy - modalH / 2.f, cx + modalW / 2.f, cy + modalH / 2.f);

    D2D1_ROUNDED_RECT rrModal = D2D1::RoundedRect(m_modalRect, 12.f * m_dpiScale, 12.f * m_dpiScale);
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(rrModal, m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(rrModal, m_brush, 1.5f);

    // Close button (X)
    float closeSz = 24.f * m_dpiScale;
    m_modalCloseBtn = D2D1::RectF(m_modalRect.right - closeSz - 12.f*m_dpiScale, m_modalRect.top + 12.f*m_dpiScale,
                                  m_modalRect.right - 12.f*m_dpiScale, m_modalRect.top + closeSz + 12.f*m_dpiScale);

    if (m_hoveredBtn == 10) {
        m_brush->SetColor(ds::CardHover());
        rt->FillRoundedRectangle(D2D1::RoundedRect(m_modalCloseBtn, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush);
    }
    m_brush->SetColor(ds::TextSecondary());
    if (m_tfBtn) {
        rt->DrawTextW(L"\x2715", 1, m_tfBtn, m_modalCloseBtn, m_brush);
    }

    // Modal Title & Subtitle
    float my = m_modalRect.top + 16.f * m_dpiScale;
    m_brush->SetColor(ds::Accent::Primary());
    std::wstring mTitle = (m_activeModal == 1) ? Localization::Instance().Get(L"about.vodafone") : L"Banque Misr \x2022 IBAN";
    DWRITE_READING_DIRECTION rd = m_tfTitle->GetReadingDirection();
    m_tfTitle->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    rt->DrawTextW(mTitle.c_str(), (UINT32)mTitle.size(), m_tfBtn,
        D2D1::RectF(m_modalRect.left + 20*m_dpiScale, my, m_modalRect.right - 20*m_dpiScale, my + 20.f*m_dpiScale), m_brush);
    m_tfTitle->SetReadingDirection(rd);

    my += 22.f * m_dpiScale;
    m_brush->SetColor(ds::TextSecondary());
    std::wstring mSub = (m_activeModal == 1) ? std::wstring(ApplicationMetadata::VodafoneCashNum) : std::wstring(ApplicationMetadata::BeneficiaryName);
    rt->DrawTextW(mSub.c_str(), (UINT32)mSub.size(), m_tfSmall,
        D2D1::RectF(m_modalRect.left + 15*m_dpiScale, my, m_modalRect.right - 15*m_dpiScale, my + 16.f*m_dpiScale), m_brush);

    my += 22.f * m_dpiScale;

    // QR Image Card (Clean high-contrast background with padding)
    float qrBoxSz = 175.f * m_dpiScale;
    D2D1_RECT_F qrBox = D2D1::RectF(cx - qrBoxSz / 2.f, my, cx + qrBoxSz / 2.f, my + qrBoxSz);
    m_brush->SetColor(D2D1::ColorF(1.f, 1.f, 1.f)); // White background for optimal camera scan
    rt->FillRoundedRectangle(D2D1::RoundedRect(qrBox, 8.f*m_dpiScale, 8.f*m_dpiScale), m_brush);

    ID2D1Bitmap* qrBmp = (m_activeModal == 1) ? m_bmpQrVodafone : m_bmpQrIban;
    if (qrBmp) {
        float pad = 10.f * m_dpiScale;
        D2D1_RECT_F qrInner = D2D1::RectF(qrBox.left + pad, qrBox.top + pad, qrBox.right - pad, qrBox.bottom - pad);
        rt->DrawBitmap(qrBmp, qrInner, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    }

    my += qrBoxSz + 12.f * m_dpiScale;

    // Scan Hint
    m_brush->SetColor(ds::TextMuted());
    std::wstring scanHint = Localization::Instance().Get(L"about.scan_hint");
    rt->DrawTextW(scanHint.c_str(), (UINT32)scanHint.size(), m_tfSmall,
        D2D1::RectF(m_modalRect.left + 15*m_dpiScale, my, m_modalRect.right - 15*m_dpiScale, my + 16.f*m_dpiScale), m_brush);

    my += 24.f * m_dpiScale;

    // Action button inside modal (Copy data)
    float copyBtnW = 140.f * m_dpiScale;
    float copyBtnH = 28.f * m_dpiScale;
    m_modalCopyBtn = D2D1::RectF(cx - copyBtnW / 2.f, my, cx + copyBtnW / 2.f, my + copyBtnH);

    D2D1_RECT_F cr = m_modalCopyBtn;
    if (m_pressedBtn == 11) { cr.left += 1; cr.top += 1; cr.right -= 1; cr.bottom -= 1; }
    m_brush->SetColor(m_hoveredBtn == 11 ? ds::CardHover() : ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(cr, 4.f*m_dpiScale, 4.f*m_dpiScale), m_brush, 1.f);

    std::wstring copyLabel = (m_activeModal == 1) ? Localization::Instance().Get(L"about.copy_num") : Localization::Instance().Get(L"about.copy_iban");
    if (m_justCopiedBtn == 11 && GetTickCount() - m_copyTime < 2000) {
        copyLabel = Localization::Instance().Get(L"about.copied");
        m_brush->SetColor(ds::Accent::Primary());
    } else {
        m_brush->SetColor(ds::TextPrimary());
    }
    rt->DrawTextW(copyLabel.c_str(), (UINT32)copyLabel.size(), m_tfBtn, cr, m_brush);
}

bool AboutPage::OnMouseMove(int x, int y) {
    float fx = (float)x; float fy = (float)y;
    int hover = 0;

    if (m_activeModal != 0) {
        // Only check modal controls
        if (fx >= m_modalCloseBtn.left && fx <= m_modalCloseBtn.right && fy >= m_modalCloseBtn.top && fy <= m_modalCloseBtn.bottom) {
            hover = 10;
        } else if (fx >= m_modalCopyBtn.left && fx <= m_modalCopyBtn.right && fy >= m_modalCopyBtn.top && fy <= m_modalCopyBtn.bottom) {
            hover = 11;
        }
    } else {
        if (fx >= m_rectEmail.left && fx <= m_rectEmail.right && fy >= m_rectEmail.top && fy <= m_rectEmail.bottom) {
            hover = 1;
        } else if (fx >= m_rectCopy.left && fx <= m_rectCopy.right && fy >= m_rectCopy.top && fy <= m_rectCopy.bottom) {
            hover = 2;
        } else if (fx >= m_btnVodafoneCopy.left && fx <= m_btnVodafoneCopy.right && fy >= m_btnVodafoneCopy.top && fy <= m_btnVodafoneCopy.bottom) {
            hover = 3;
        } else if (fx >= m_btnVodafoneQr.left && fx <= m_btnVodafoneQr.right && fy >= m_btnVodafoneQr.top && fy <= m_btnVodafoneQr.bottom) {
            hover = 4;
        } else if (fx >= m_btnIbanCopy.left && fx <= m_btnIbanCopy.right && fy >= m_btnIbanCopy.top && fy <= m_btnIbanCopy.bottom) {
            hover = 5;
        } else if (fx >= m_btnIbanQr.left && fx <= m_btnIbanQr.right && fy >= m_btnIbanQr.top && fy <= m_btnIbanQr.bottom) {
            hover = 6;
        }
    }

    if (m_hoveredBtn != hover) {
        m_hoveredBtn = hover;
        MainWindow::Instance().Invalidate();
        return true;
    }
    return false;
}

bool AboutPage::OnLButtonDown(int x, int y) {
    if (m_activeModal != 0) {
        float fx = (float)x; float fy = (float)y;
        if (m_hoveredBtn != 0) {
            m_pressedBtn = m_hoveredBtn;
            MainWindow::Instance().Invalidate();
            return true;
        } else if (fx < m_modalRect.left || fx > m_modalRect.right || fy < m_modalRect.top || fy > m_modalRect.bottom) {
            // Click outside closes modal
            m_activeModal = 0;
            MainWindow::Instance().Invalidate();
            return true;
        }
        return true; // Eat click inside modal
    }

    if (m_hoveredBtn != 0) {
        m_pressedBtn = m_hoveredBtn;
        MainWindow::Instance().Invalidate();
        return true;
    }
    return false;
}

static void CopyTextToClipboard(const std::wstring& str) {
    if (str.empty() || !OpenClipboard(nullptr)) return;
    EmptyClipboard();
    const size_t bytes = (str.size() + 1) * sizeof(wchar_t);
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hg) {
        CloseClipboard();
        return;
    }
    void* p = GlobalLock(hg);
    if (!p) {
        GlobalFree(hg);
        CloseClipboard();
        return;
    }
    memcpy(p, str.c_str(), bytes);
    GlobalUnlock(hg);
    if (!SetClipboardData(CF_UNICODETEXT, hg)) {
        GlobalFree(hg);
    }
    CloseClipboard();
}

bool AboutPage::OnLButtonUp(int x, int y) {
    int pressed = m_pressedBtn;
    m_pressedBtn = 0;

    if (m_activeModal != 0) {
        if (pressed == 10 && m_hoveredBtn == 10) {
            m_activeModal = 0;
            MainWindow::Instance().Invalidate();
            return true;
        } else if (pressed == 11 && m_hoveredBtn == 11) {
            if (m_activeModal == 1) {
                CopyTextToClipboard(ApplicationMetadata::VodafoneCashNum);
            } else {
                CopyTextToClipboard(ApplicationMetadata::IBAN);
            }
            m_justCopiedBtn = 11;
            m_copyTime = GetTickCount();
            MainWindow::Instance().Invalidate();
            return true;
        }
        return true;
    }

    if (pressed != 0 && pressed == m_hoveredBtn) {
        if (pressed == 1) { // Click email
            std::wstring mailto = L"mailto:" + std::wstring(ApplicationMetadata::DeveloperEmail);
            ShellExecuteW(nullptr, L"open", mailto.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        } else if (pressed == 2) { // Copy email
            CopyTextToClipboard(ApplicationMetadata::DeveloperEmail);
            m_justCopiedBtn = 2;
            m_copyTime = GetTickCount();
        } else if (pressed == 3) { // Copy Vodafone Cash number
            CopyTextToClipboard(ApplicationMetadata::VodafoneCashNum);
            m_justCopiedBtn = 3;
            m_copyTime = GetTickCount();
        } else if (pressed == 4) { // Open Vodafone QR modal
            m_activeModal = 1;
        } else if (pressed == 5) { // Copy IBAN
            CopyTextToClipboard(ApplicationMetadata::IBAN);
            m_justCopiedBtn = 5;
            m_copyTime = GetTickCount();
        } else if (pressed == 6) { // Open IBAN QR modal
            m_activeModal = 2;
        }
        MainWindow::Instance().Invalidate();
        return true;
    }
    MainWindow::Instance().Invalidate();
    return pressed != 0;
}

void AboutPage::OnMouseLeave() {
    if (m_hoveredBtn != 0 || m_pressedBtn != 0) {
        m_hoveredBtn = 0;
        m_pressedBtn = 0;
        MainWindow::Instance().Invalidate();
    }
}

bool AboutPage::HitTestInteractive(int x, int y) {
    if (m_activeModal != 0) return true; // entire screen modal absorbs input

    float fx = (float)x; float fy = (float)y;
    return (fx >= m_rectEmail.left && fx <= m_rectEmail.right && fy >= m_rectEmail.top && fy <= m_rectEmail.bottom) ||
           (fx >= m_rectCopy.left && fx <= m_rectCopy.right && fy >= m_rectCopy.top && fy <= m_rectCopy.bottom) ||
           (fx >= m_btnVodafoneCopy.left && fx <= m_btnVodafoneCopy.right && fy >= m_btnVodafoneCopy.top && fy <= m_btnVodafoneCopy.bottom) ||
           (fx >= m_btnVodafoneQr.left && fx <= m_btnVodafoneQr.right && fy >= m_btnVodafoneQr.top && fy <= m_btnVodafoneQr.bottom) ||
           (fx >= m_btnIbanCopy.left && fx <= m_btnIbanCopy.right && fy >= m_btnIbanCopy.top && fy <= m_btnIbanCopy.bottom) ||
           (fx >= m_btnIbanQr.left && fx <= m_btnIbanQr.right && fy >= m_btnIbanQr.top && fy <= m_btnIbanQr.bottom);
}

} // namespace dhikr
