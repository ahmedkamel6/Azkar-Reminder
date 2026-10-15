#include "LibraryPage.h"
#include "../DesignSystem.h"
#include "../../core/Application.h"
#include "../../core/ContentEngine.h"
#include "../../core/Localization.h"
#include <algorithm>
#include <shellapi.h>
#include <commdlg.h>
#include <ctime>

namespace dhikr {

LibraryPage::LibraryPage() {
    auto* dwf = Application::Instance().DWriteFactory();
    if (!dwf) return;
    m_dwf = dwf;
    HDC hdc = GetDC(nullptr);
    m_dpiScale = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, hdc);

    const wchar_t* fontName = ds::Font::Arabic();
    auto mk = [&](float sz, DWRITE_FONT_WEIGHT wt, const wchar_t* locale, IDWriteTextFormat** out) {
        dwf->CreateTextFormat(fontName, nullptr, wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, sz * m_dpiScale, locale, out);
    };
    mk(ds::FontSize::PageTitle, DWRITE_FONT_WEIGHT_SEMI_BOLD, L"en-US", &m_tfTitle);
    mk(ds::FontSize::Body + 1.f, DWRITE_FONT_WEIGHT_REGULAR,   L"ar-SA", &m_tfMain);
    mk(ds::FontSize::Small,     DWRITE_FONT_WEIGHT_REGULAR,   L"en-US", &m_tfSub);
    mk(ds::FontSize::Small,     DWRITE_FONT_WEIGHT_SEMI_BOLD, L"en-US", &m_tfCat);
    mk(ds::FontSize::PageTitle, DWRITE_FONT_WEIGHT_REGULAR,   L"en-US", &m_tfEmpty);

    // Icon font — dynamically resolve Segoe Fluent Icons or Segoe MDL2 Assets
    const wchar_t* iconFont = ds::Icon::GetFontFamily(dwf);
    dwf->CreateTextFormat(
        iconFont, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        32.f * m_dpiScale, L"en-US", &m_tfIcon);
    if (m_tfIcon) {
        m_tfIcon->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfIcon->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    dwf->CreateTextFormat(
        iconFont, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.f * m_dpiScale, L"en-US", &m_tfIconSmall);
    if (m_tfIconSmall) {
        m_tfIconSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfIconSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    if (m_tfMain) {
        m_tfMain->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        m_tfMain->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }
    if (m_tfEmpty) {
        m_tfEmpty->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfEmpty->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    if (m_tfCat) {
        m_tfCat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_tfCat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
}

LibraryPage::~LibraryPage() {
    if (m_tfTitle) { m_tfTitle->Release(); m_tfTitle = nullptr; }
    if (m_tfMain)  { m_tfMain->Release();  m_tfMain  = nullptr; }
    if (m_tfSub)   { m_tfSub->Release();   m_tfSub   = nullptr; }
    if (m_tfCat)   { m_tfCat->Release();   m_tfCat   = nullptr; }
    if (m_tfEmpty) { m_tfEmpty->Release(); m_tfEmpty = nullptr; }
    if (m_tfIcon)  { m_tfIcon->Release();  m_tfIcon  = nullptr; }
    if (m_tfIconSmall) { m_tfIconSmall->Release(); m_tfIconSmall = nullptr; }
    if (m_brush)   { m_brush->Release();   m_brush   = nullptr; }
}

void LibraryPage::DrawEmptyState(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_tfEmpty || !m_brush || !m_tfIcon) return;
    
    m_brush->SetColor(ds::Border());
    rt->DrawTextW(ds::Icon::Library, 1, m_tfIcon, 
        D2D1::RectF(bounds.left, bounds.top + 40*m_dpiScale, bounds.right, bounds.bottom), m_brush);

    m_brush->SetColor(ds::TextSecondary());
    std::wstring msg = Localization::Instance().Get(L"library.empty");
    rt->DrawTextW(msg.c_str(), (UINT32)msg.size(), m_tfEmpty, 
        D2D1::RectF(bounds.left, bounds.top + 100*m_dpiScale, bounds.right, bounds.bottom), m_brush);
}

void LibraryPage::DrawFilterPills(ID2D1RenderTarget* rt, float x, float y, float w) {
    std::wstring filterStrings[] = {
        Localization::Instance().Get(L"library.filter_all"),
        Localization::Instance().Get(L"library.filter_morning"),
        Localization::Instance().Get(L"library.filter_evening"),
        Localization::Instance().Get(L"library.filter_daily"),
        Localization::Instance().Get(L"library.filter_custom")
    };

    bool isRtl = Localization::Instance().IsRTL();
    float pillH = 26.f * m_dpiScale;
    float curX = isRtl ? (x + w) : x;

    for (int i = 0; i < 5; ++i) {
        float pillW = (filterStrings[i].size() * 8.5f + 24.f) * m_dpiScale;
        float px = isRtl ? (curX - pillW) : curX;
        D2D1_RECT_F pr = D2D1::RectF(px, y, px + pillW, y + pillH);

        bool active = (m_currentFilter == i);
        float rx = pillH / 2.f;

        m_brush->SetColor(active ? ds::Accent::Primary(0.18f) : ds::Surface());
        rt->FillRoundedRectangle(D2D1::RoundedRect(pr, rx, rx), m_brush);

        m_brush->SetColor(active ? ds::Accent::Primary() : ds::Border());
        rt->DrawRoundedRectangle(D2D1::RoundedRect(pr, rx, rx), m_brush, active ? 1.f : 0.5f);

        if (m_tfCat) {
            m_brush->SetColor(active ? ds::Accent::Primary() : ds::TextSecondary());
            rt->DrawTextW(filterStrings[i].c_str(), (UINT32)filterStrings[i].size(), m_tfCat, pr, m_brush);
        }

        if (isRtl) curX -= (pillW + 8.f * m_dpiScale);
        else curX += (pillW + 8.f * m_dpiScale);
    }
}

void LibraryPage::DrawContentItem(ID2D1RenderTarget* rt, float x, float& y, float w, size_t index, bool hovered) {
    auto items = ContentEngine::Instance().GetAllItems();
    if (index >= items.size()) return;
    auto& item = items[index];

    float padX = ds::Space::M * m_dpiScale;
    float padY = ds::Space::S * m_dpiScale;
    float itemH = 74.f * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();

    // Background
    float rx = ds::Dims::CardRadius * m_dpiScale;
    m_brush->SetColor(hovered ? ds::CardHover() : ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x+w, y+itemH), rx, rx), m_brush);
    m_brush->SetColor(hovered ? ds::BorderHighlight() : ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x, y, x+w, y+itemH), rx, rx), m_brush, hovered ? 1.0f : 0.75f);

    // Accent strip for enabled items
    if (item.enabled) {
        m_brush->SetColor(ds::Accent::Primary(0.85f));
        float ax = isRtl ? (x + w - 3*m_dpiScale) : x;
        D2D1_RECT_F accent = D2D1::RectF(ax, y + 8*m_dpiScale, ax + 3*m_dpiScale, y + itemH - 8*m_dpiScale);
        rt->FillRoundedRectangle(D2D1::RoundedRect(accent, 2, 2), m_brush);
    }

    // Left controls zone (Delete icon)
    float delSize = 28.f * m_dpiScale;
    float delLeft = x + (isRtl ? (32.f * m_dpiScale) : (16.f * m_dpiScale));
    float delTop  = y + (itemH - delSize) / 2.f;
    D2D1_RECT_F delR = D2D1::RectF(delLeft, delTop, delLeft + delSize, delTop + delSize);

    // Delete icon button (Fluent Trash Can in polished circular badge on far left)
    if (hovered && m_tfIconSmall) {
        float radius = delSize / 2.f;
        bool isHoverDel = m_hoveredDelBtn && (m_hoveredItem == (int)index);
        if (isHoverDel) {
            m_brush->SetColor(ds::Rgba(0xDC, 0x26, 0x26, 0.25f)); // Soft crimson hover glow
            rt->FillRoundedRectangle(D2D1::RoundedRect(delR, radius, radius), m_brush);
            m_brush->SetColor(ds::Rgba(0xEF, 0x44, 0x44, 0.70f)); // Red border
            rt->DrawRoundedRectangle(D2D1::RoundedRect(delR, radius, radius), m_brush, 1.f);
            m_brush->SetColor(ds::Rgba(0xFF, 0xFF, 0xFF, 1.0f)); // Pure white trash icon
        } else {
            m_brush->SetColor(ds::Rgba(0x18, 0x22, 0x1A, 0.85f)); // Dark subtle surface
            rt->FillRoundedRectangle(D2D1::RoundedRect(delR, radius, radius), m_brush);
            m_brush->SetColor(ds::Border());
            rt->DrawRoundedRectangle(D2D1::RoundedRect(delR, radius, radius), m_brush, 0.75f);
            m_brush->SetColor(ds::TextSecondary()); // Muted icon
        }
        rt->DrawTextW(L"\xE74D", 1, m_tfIconSmall, delR, m_brush);
    }

    // Main text (Arabic, RTL) - well clear of left controls
    if (m_tfMain && !item.text.empty()) {
        m_brush->SetColor(item.enabled ? ds::TextPrimary() : ds::TextMuted());
        float textLeft = delLeft + delSize + 14.f * m_dpiScale;
        float textRight = x + w - (isRtl ? (32.f * m_dpiScale) : (20.f * m_dpiScale));
        D2D1_RECT_F tr = D2D1::RectF(textLeft, y + padY + 2.f * m_dpiScale, textRight, y + itemH - padY);
        rt->DrawTextW(item.text.c_str(),
            (UINT32)std::min((size_t)140, item.text.size()),
            m_tfMain, tr, m_brush);
    }

    y += itemH + ds::Space::S * m_dpiScale;
}

void LibraryPage::DrawAddModal(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush || !m_showAddModal) return;

    // Dim background
    m_brush->SetColor(ds::Rgba(0, 0, 0, 0.70f));
    rt->FillRectangle(bounds, m_brush);

    // Modal Card
    float mw = 480.f * m_dpiScale;
    float mh = 220.f * m_dpiScale;
    float mx = bounds.left + (bounds.right - bounds.left - mw) / 2.f;
    float my = bounds.top + (bounds.bottom - bounds.top - mh) / 2.f;

    D2D1_RECT_F mr = D2D1::RectF(mx, my, mx + mw, my + mh);
    m_brush->SetColor(ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(mr, 12.f*m_dpiScale, 12.f*m_dpiScale), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(mr, 12.f*m_dpiScale, 12.f*m_dpiScale), m_brush, 1.f);

    bool isRtl = Localization::Instance().IsRTL();

    // Dialog Title
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring title = Localization::Instance().Get(L"library.dialog_title");
        D2D1_RECT_F tr = D2D1::RectF(mx + 20*m_dpiScale, my + 16*m_dpiScale, mx + mw - 20*m_dpiScale, my + 44*m_dpiScale);
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle, tr, m_brush);
    }

    // Input Box for Dhikr Text
    D2D1_RECT_F inR = D2D1::RectF(mx + 20*m_dpiScale, my + 52*m_dpiScale, mx + mw - 20*m_dpiScale, my + 145*m_dpiScale);
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(inR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush);
    m_brush->SetColor(ds::Accent::Primary(0.6f));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(inR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush, 1.f);

    // Text content inside input box
    if (m_tfMain) {
        D2D1_RECT_F textR = D2D1::RectF(inR.left + 10*m_dpiScale, inR.top + 8*m_dpiScale, inR.right - 10*m_dpiScale, inR.bottom - 8*m_dpiScale);
        if (m_inputDhikrText.empty()) {
            m_brush->SetColor(ds::TextMuted());
            std::wstring ph = isRtl ? L"اكتب الذكر هنا..." : L"Type dhikr text here...";
            rt->DrawTextW(ph.c_str(), (UINT32)ph.size(), m_tfMain, textR, m_brush);
        } else {
            m_brush->SetColor(ds::TextPrimary());
            std::wstring cursorText = m_inputDhikrText + L"|";
            rt->DrawTextW(cursorText.c_str(), (UINT32)cursorText.size(), m_tfMain, textR, m_brush);
        }
    }

    // Action Buttons: Save and Cancel
    float bw = 100.f * m_dpiScale;
    float bh = 34.f * m_dpiScale;
    float by = my + mh - 50*m_dpiScale;

    // Save Button
    D2D1_RECT_F saveR = isRtl ? D2D1::RectF(mx + 20*m_dpiScale, by, mx + 20*m_dpiScale + bw, by + bh)
                              : D2D1::RectF(mx + mw - 20*m_dpiScale - bw, by, mx + mw - 20*m_dpiScale, by + bh);
    m_brush->SetColor(ds::Accent::Primary());
    rt->FillRoundedRectangle(D2D1::RoundedRect(saveR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush);
    if (m_tfCat) {
        m_brush->SetColor(ds::Rgba(0x08, 0x0A, 0x09));
        std::wstring sStr = Localization::Instance().Get(L"common.save");
        rt->DrawTextW(sStr.c_str(), (UINT32)sStr.size(), m_tfCat, saveR, m_brush);
    }

    // Cancel Button
    D2D1_RECT_F cancelR = isRtl ? D2D1::RectF(mx + 30*m_dpiScale + bw, by, mx + 30*m_dpiScale + bw * 2.f, by + bh)
                                : D2D1::RectF(mx + mw - 30*m_dpiScale - bw * 2.f, by, mx + mw - 30*m_dpiScale - bw, by + bh);
    m_brush->SetColor(ds::Card());
    rt->FillRoundedRectangle(D2D1::RoundedRect(cancelR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush);
    m_brush->SetColor(ds::Border());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(cancelR, 6.f*m_dpiScale, 6.f*m_dpiScale), m_brush, 0.5f);
    if (m_tfCat) {
        m_brush->SetColor(ds::TextSecondary());
        std::wstring cStr = Localization::Instance().Get(L"common.cancel");
        rt->DrawTextW(cStr.c_str(), (UINT32)cStr.size(), m_tfCat, cancelR, m_brush);
    }
}

static std::vector<size_t> GetFilteredIndices(int filter) {
    auto allItems = ContentEngine::Instance().GetAllItems();
    std::vector<size_t> filtered;
    filtered.reserve(allItems.size());
    for (size_t i = 0; i < allItems.size(); ++i) {
        auto& it = allItems[i];
        if (filter == 0) {
            filtered.push_back(i);
        } else if (filter == 1 && (it.category.find("morning") != std::string::npos || it.category.find("صباح") != std::string::npos)) {
            filtered.push_back(i);
        } else if (filter == 2 && (it.category.find("evening") != std::string::npos || it.category.find("مساء") != std::string::npos)) {
            filtered.push_back(i);
        } else if (filter == 3 && (it.category.find("morning") == std::string::npos &&
                                   it.category.find("evening") == std::string::npos &&
                                   it.category.find("صباح") == std::string::npos &&
                                   it.category.find("مساء") == std::string::npos &&
                                   it.id.rfind("custom_", 0) != 0)) {
            filtered.push_back(i);
        } else if (filter == 4 && (it.id.rfind("custom_", 0) == 0)) {
            filtered.push_back(i);
        }
    }
    return filtered;
}

void LibraryPage::Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) {
    if (!m_brush) rt->CreateSolidColorBrush(D2D1::ColorF(1,1,1), &m_brush);

    float pad = ds::Space::XL * m_dpiScale;
    float x = bounds.left + pad;
    float y = bounds.top  + pad;
    float w = bounds.right - bounds.left - pad * 2.f;
    bool isRtl = Localization::Instance().IsRTL();
    
    m_contentX = x;
    m_contentW = w;
    m_contentY = y;

    if (m_tfTitle) m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfSub) m_tfSub->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    if (m_tfCat) m_tfCat->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    // Title
    if (m_tfTitle) {
        m_brush->SetColor(ds::TextPrimary());
        m_tfTitle->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring title = Localization::Instance().Get(L"library.title");
        rt->DrawTextW(title.c_str(), (UINT32)title.size(), m_tfTitle, D2D1::RectF(x, y, x+w, y+28*m_dpiScale), m_brush);
    }
    
    // Top action buttons (+ Add Dhikr, Import MD, Delete All)
    float btnY = y;
    float btnW = 105.f * m_dpiScale;
    float btnH = ds::Dims::BtnH * m_dpiScale;
    float rx = ds::Dims::BtnRadius * m_dpiScale;
    
    D2D1_RECT_F addRect, importRect, delRect;
    if (isRtl) {
        delRect    = D2D1::RectF(x, btnY, x+btnW, btnY+btnH);
        importRect = D2D1::RectF(x+btnW+8*m_dpiScale, btnY, x+btnW*2+8*m_dpiScale, btnY+btnH);
        addRect    = D2D1::RectF(x+btnW*2+16*m_dpiScale, btnY, x+btnW*3+16*m_dpiScale, btnY+btnH);
    } else {
        delRect    = D2D1::RectF(x+w-btnW, btnY, x+w, btnY+btnH);
        importRect = D2D1::RectF(x+w-btnW*2-8*m_dpiScale, btnY, x+w-btnW-8*m_dpiScale, btnY+btnH);
        addRect    = D2D1::RectF(x+w-btnW*3-16*m_dpiScale, btnY, x+w-btnW*2-16*m_dpiScale, btnY+btnH);
    }

    // 1. + Add Dhikr Button
    auto addCol = m_hoveredBtn == 2 ? ds::Accent::Dark() : ds::Accent::Primary();
    m_brush->SetColor(addCol);
    rt->FillRoundedRectangle(D2D1::RoundedRect(addRect, rx, rx), m_brush);
    if (m_tfCat) {
        float lum = 0.299f * addCol.r + 0.587f * addCol.g + 0.114f * addCol.b;
        m_brush->SetColor(lum >= 0.58f ? ds::Rgba(0x08, 0x0A, 0x09) : ds::Rgba(0xFF, 0xFF, 0xFF));
        std::wstring addStr = Localization::Instance().Get(L"library.add_dhikr");
        rt->DrawTextW(addStr.c_str(), (UINT32)addStr.size(), m_tfCat, addRect, m_brush);
    }

    // 2. Import MD Button
    m_brush->SetColor(m_hoveredBtn == 0 ? ds::Rgba(0x22, 0x2C, 0x24) : ds::Surface());
    rt->FillRoundedRectangle(D2D1::RoundedRect(importRect, rx, rx), m_brush);
    m_brush->SetColor(m_hoveredBtn == 0 ? ds::Accent::Primary() : ds::Rgba(0x4A, 0x58, 0x4E, 1.0f));
    rt->DrawRoundedRectangle(D2D1::RoundedRect(importRect, rx, rx), m_brush, 1.2f);
    if (m_tfCat) {
        m_brush->SetColor(ds::Rgba(0xFF, 0xFF, 0xFF, 1.0f));
        std::wstring impStr = Localization::Instance().Get(L"library.import_md");
        rt->DrawTextW(impStr.c_str(), (UINT32)impStr.size(), m_tfCat, importRect, m_brush);
    }

    // 3. Delete All Button (Refined dark crimson outline)
    m_brush->SetColor(m_hoveredBtn == 1 ? ds::BtnDangerHover() : ds::BtnDanger());
    rt->FillRoundedRectangle(D2D1::RoundedRect(delRect, rx, rx), m_brush);
    m_brush->SetColor(ds::BtnDangerBorder());
    rt->DrawRoundedRectangle(D2D1::RoundedRect(delRect, rx, rx), m_brush, 1.0f);
    if (m_tfCat) {
        m_brush->SetColor(ds::BtnDangerText());
        std::wstring delStr = Localization::Instance().Get(L"library.delete_all");
        rt->DrawTextW(delStr.c_str(), (UINT32)delStr.size(), m_tfCat, delRect, m_brush);
    }

    y += 44.f * m_dpiScale;

    // Filter pills
    DrawFilterPills(rt, x, y, w);
    y += 38.f * m_dpiScale;

    // Filter items based on m_currentFilter
    auto allItems = ContentEngine::Instance().GetAllItems();
    std::vector<size_t> filteredIndices = GetFilteredIndices(m_currentFilter);

    if (allItems.empty() || filteredIndices.empty()) {
        m_scrollY = 0.f;
        DrawEmptyState(rt, D2D1::RectF(x, y, x+w, bounds.bottom - bounds.top));
        if (m_showAddModal) DrawAddModal(rt, bounds);
        return;
    }

    // Count label
    if (m_tfSub) {
        m_tfSub->SetReadingDirection(isRtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        m_tfSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring cntStr = std::to_wstring(filteredIndices.size()) + L" " + Localization::Instance().Get(L"library.items_loaded");
        m_brush->SetColor(ds::TextMuted());
        rt->DrawTextW(cntStr.c_str(), (UINT32)cntStr.size(), m_tfSub,
            D2D1::RectF(x, y, x+w, y+18*m_dpiScale), m_brush);
    }
    y += 26.f * m_dpiScale;

    // Clip region for scrolling
    float scrollableH = bounds.bottom - y - 16*m_dpiScale;
    D2D1_RECT_F clipR = D2D1::RectF(x, y, x+w, y + scrollableH);
    rt->PushAxisAlignedClip(clipR, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float itemH = 74.f * m_dpiScale + ds::Space::S * m_dpiScale;
    m_maxScroll = std::max(0.f, (float)filteredIndices.size() * itemH - scrollableH + 16.f * m_dpiScale);
    int maxVisible = (int)(scrollableH / itemH) + 2;
    int startIdx = std::max(0, (int)(m_scrollY / itemH));

    for (int i = startIdx; i < std::min((int)filteredIndices.size(), startIdx + maxVisible + 2); ++i) {
        size_t actualIdx = filteredIndices[i];
        float iy = y - m_scrollY + i * itemH;
        DrawContentItem(rt, x, iy, w, actualIdx, (int)actualIdx == m_hoveredItem);
    }

    rt->PopAxisAlignedClip();

    // Draw modal dialog over page if active
    if (m_showAddModal) {
        DrawAddModal(rt, bounds);
    }
}

bool LibraryPage::OnChar(wchar_t ch) {
    if (!m_showAddModal) return false;

    if (ch == L'\b') { // Backspace
        if (!m_inputDhikrText.empty()) {
            m_inputDhikrText.pop_back();
            return true;
        }
    } else if (ch >= 32) { // Printable characters
        m_inputDhikrText.push_back(ch);
        return true;
    }
    return false;
}

bool LibraryPage::OnKeyDown(WPARAM key) {
    if (!m_showAddModal) return false;

    if (key == VK_ESCAPE) {
        m_showAddModal = false;
        m_inputDhikrText.clear();
        MainWindow::Instance().Invalidate();
        return true;
    } else if (key == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (OpenClipboard(nullptr)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
                if (pszText) {
                    m_inputDhikrText += pszText;
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
            MainWindow::Instance().Invalidate();
            return true;
        }
    } else if (key == VK_RETURN) {
        if (!m_inputDhikrText.empty()) {
            ContentItem newItem;
            newItem.id = "custom_" + std::to_string(GetTickCount64()) + "_" + std::to_string(rand());
            newItem.text = m_inputDhikrText;
            newItem.category = "custom";
            newItem.priority = 1;
            newItem.repetitions = m_selectedReps;
            newItem.enabled = true;
            ContentEngine::Instance().AddItem(newItem);
            m_inputDhikrText.clear();
            m_showAddModal = false;
            MainWindow::Instance().OnSettingsChanged();
            MainWindow::Instance().Invalidate();
            return true;
        }
    }
    return false;
}

bool LibraryPage::OnLButtonDown(int x, int y) {
    // If modal is active, handle modal clicks
    if (m_showAddModal) {
        float mw = 480.f * m_dpiScale;
        float mh = 220.f * m_dpiScale;
        float mx = m_contentX + (m_contentW - mw) / 2.f;
        float my = m_contentY + (500.f * m_dpiScale - mh) / 2.f;
        bool isRtl = Localization::Instance().IsRTL();

        // Save Button
        float bw = 100.f * m_dpiScale;
        float bh = 34.f * m_dpiScale;
        float by = my + mh - 50*m_dpiScale;
        D2D1_RECT_F saveR = isRtl ? D2D1::RectF(mx + 20*m_dpiScale, by, mx + 20*m_dpiScale + bw, by + bh)
                                  : D2D1::RectF(mx + mw - 20*m_dpiScale - bw, by, mx + mw - 20*m_dpiScale, by + bh);
        if (x >= saveR.left && x <= saveR.right && y >= saveR.top && y <= saveR.bottom) {
            if (!m_inputDhikrText.empty()) {
                ContentItem newItem;
                newItem.id = "custom_" + std::to_string(GetTickCount64()) + "_" + std::to_string(rand());
                newItem.text = m_inputDhikrText;
                newItem.category = "custom";
                newItem.priority = 1;
                newItem.repetitions = 1;
                newItem.enabled = true;
                ContentEngine::Instance().AddItem(newItem);
                m_inputDhikrText.clear();
                m_showAddModal = false;
                MainWindow::Instance().OnSettingsChanged();
                MainWindow::Instance().Invalidate();
            }
            return true;
        }

        // Cancel Button
        D2D1_RECT_F cancelR = isRtl ? D2D1::RectF(mx + 30*m_dpiScale + bw, by, mx + 30*m_dpiScale + bw * 2.f, by + bh)
                                    : D2D1::RectF(mx + mw - 30*m_dpiScale - bw * 2.f, by, mx + mw - 30*m_dpiScale - bw, by + bh);
        if (x >= cancelR.left && x <= cancelR.right && y >= cancelR.top && y <= cancelR.bottom) {
            m_showAddModal = false;
            m_inputDhikrText.clear();
            MainWindow::Instance().Invalidate();
            return true;
        }
        return true; // absorb click while modal is up
    }

    float btnY = m_contentY;
    float btnW = 105.f * m_dpiScale;
    float btnH = ds::Dims::BtnH * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    
    D2D1_RECT_F addRect, importRect, delRect;
    if (isRtl) {
        delRect    = D2D1::RectF(m_contentX, btnY, m_contentX+btnW, btnY+btnH);
        importRect = D2D1::RectF(m_contentX+btnW+8*m_dpiScale, btnY, m_contentX+btnW*2+8*m_dpiScale, btnY+btnH);
        addRect    = D2D1::RectF(m_contentX+btnW*2+16*m_dpiScale, btnY, m_contentX+btnW*3+16*m_dpiScale, btnY+btnH);
    } else {
        delRect    = D2D1::RectF(m_contentX+m_contentW-btnW, btnY, m_contentX+m_contentW, btnY+btnH);
        importRect = D2D1::RectF(m_contentX+m_contentW-btnW*2-8*m_dpiScale, btnY, m_contentX+m_contentW-btnW-8*m_dpiScale, btnY+btnH);
        addRect    = D2D1::RectF(m_contentX+m_contentW-btnW*3-16*m_dpiScale, btnY, m_contentX+m_contentW-btnW*2-16*m_dpiScale, btnY+btnH);
    }

    // Click + Add Dhikr
    if (x >= addRect.left && x <= addRect.right && y >= addRect.top && y <= addRect.bottom) {
        m_showAddModal = true;
        m_inputDhikrText.clear();
        MainWindow::Instance().Invalidate();
        return true;
    }

    // Filter pills clicks
    float filterY = m_contentY + 44.f * m_dpiScale;
    float curX = isRtl ? (m_contentX + m_contentW) : m_contentX;
    std::wstring filterStrings[] = {
        Localization::Instance().Get(L"library.filter_all"),
        Localization::Instance().Get(L"library.filter_morning"),
        Localization::Instance().Get(L"library.filter_evening"),
        Localization::Instance().Get(L"library.filter_daily"),
        Localization::Instance().Get(L"library.filter_custom")
    };
    for (int i = 0; i < 5; ++i) {
        float pillW = (filterStrings[i].size() * 8.5f + 24.f) * m_dpiScale;
        float px = isRtl ? (curX - pillW) : curX;
        if (x >= px && x <= px + pillW && y >= filterY && y <= filterY + 26.f * m_dpiScale) {
            m_currentFilter = i;
            m_scrollY = 0;
            m_targetScrollY = 0;
            MainWindow::Instance().Invalidate();
            return true;
        }
        if (isRtl) curX -= (pillW + 8.f * m_dpiScale);
        else curX += (pillW + 8.f * m_dpiScale);
    }

    // Delete All
    if (x >= delRect.left && x <= delRect.right && y >= delRect.top && y <= delRect.bottom) {
        std::wstring title = Localization::Instance().Get(L"library.delete_title");
        std::wstring msg   = Localization::Instance().Get(L"library.delete_confirm");
        m_hoveredItem = -1;
        m_hoveredDelBtn = false;
        m_hoveredBtn = -1;
        int r = MessageBoxW(Application::Instance().MainWindow(), msg.c_str(), title.c_str(), MB_YESNO | MB_ICONWARNING);
        if (r == IDYES) {
            ContentEngine::Instance().ClearAll();
            m_scrollY = 0.f;
            m_targetScrollY = 0.f;
            MainWindow::Instance().OnSettingsChanged();
            MainWindow::Instance().Invalidate();
        }
        return true;
    }
    
    // Import MD File
    if (x >= importRect.left && x <= importRect.right && y >= importRect.top && y <= importRect.bottom) {
        OPENFILENAMEW ofn = {};
        wchar_t szFile[MAX_PATH] = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = Application::Instance().MainWindow();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
        ofn.lpstrFilter = L"All Supported Datasets (*.json;*.md;*.txt)\0*.json;*.md;*.txt\0JSON Datasets (*.json)\0*.json\0Markdown Files (*.md)\0*.md\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        
        if (GetOpenFileNameW(&ofn) == TRUE) {
            ContentLoadResult result = ContentEngine::Instance().Load(ofn.lpstrFile);
            if (result == ContentLoadResult::Success) {
                ContentEngine::Instance().Save();
                MainWindow::Instance().OnSettingsChanged();
                MainWindow::Instance().Invalidate();
                MessageBoxW(Application::Instance().MainWindow(),
                    Localization::Instance().Get(L"library.import_success").c_str(),
                    Localization::Instance().Get(L"library.title").c_str(), MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(Application::Instance().MainWindow(),
                    Localization::Instance().Get(L"library.import_error").c_str(),
                    Localization::Instance().Get(L"common.error").c_str(), MB_OK | MB_ICONERROR);
            }
        }
        return true;
    }

    if (m_hoveredItem >= 0) {
        int targetIndex = m_hoveredItem;
        bool isDelBtn   = m_hoveredDelBtn;

        // Reset hover states before opening any modal dialog to prevent re-entrancy issues
        m_hoveredItem   = -1;
        m_hoveredDelBtn = false;

        const auto allItems = ContentEngine::Instance().GetAllItems();
        if (targetIndex >= 0 && targetIndex < (int)allItems.size()) {
            if (isDelBtn) {
                std::string targetId = allItems[targetIndex].id;
                std::wstring title = Localization::Instance().Get(L"library.delete_title");
                std::wstring msg   = Localization::Instance().Get(L"library.delete_item_confirm");
                int r = MessageBoxW(Application::Instance().MainWindow(), msg.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION);
                if (r == IDYES) {
                    if (!targetId.empty()) {
                        ContentEngine::Instance().DeleteItemById(targetId);
                    } else {
                        ContentEngine::Instance().DeleteItem((size_t)targetIndex);
                    }
                }
            } else {
                ContentEngine::Instance().ToggleItemEnabled((size_t)targetIndex);
            }
            MainWindow::Instance().OnSettingsChanged();
            MainWindow::Instance().Invalidate();
            return true;
        }
    }
    return false;
}

bool LibraryPage::OnMouseWheel(int delta) {
    if (m_contentW == 0 || m_showAddModal || m_maxScroll <= 0.f) return false;

    float scrollStep = (delta / 120.f) * (80.f * m_dpiScale);
    m_targetScrollY = std::clamp(m_targetScrollY - scrollStep, 0.f, m_maxScroll);
    float diff = m_targetScrollY - m_scrollY;
    if (std::abs(diff) < 0.4f) {
        m_scrollY = m_targetScrollY;
    } else {
        m_scrollY += diff * 0.25f;
    }
    return true;
}

bool LibraryPage::OnScrollTick() {
    m_targetScrollY = std::clamp(m_targetScrollY, 0.f, m_maxScroll);
    float diff = m_targetScrollY - m_scrollY;
    if (std::abs(diff) < 0.4f) {
        if (m_scrollY != m_targetScrollY) {
            m_scrollY = m_targetScrollY;
            return true;
        }
        return false;
    }
    m_scrollY += diff * 0.24f;
    return true;
}

bool LibraryPage::OnMouseMove(int x, int y) {
    if (m_contentX == 0) return false;
    if (m_showAddModal) return false;
    
    float btnY = m_contentY;
    float btnW = 105.f * m_dpiScale;
    float btnH = ds::Dims::BtnH * m_dpiScale;
    bool isRtl = Localization::Instance().IsRTL();
    
    D2D1_RECT_F addRect, importRect, delRect;
    if (isRtl) {
        delRect    = D2D1::RectF(m_contentX, btnY, m_contentX+btnW, btnY+btnH);
        importRect = D2D1::RectF(m_contentX+btnW+8*m_dpiScale, btnY, m_contentX+btnW*2+8*m_dpiScale, btnY+btnH);
        addRect    = D2D1::RectF(m_contentX+btnW*2+16*m_dpiScale, btnY, m_contentX+btnW*3+16*m_dpiScale, btnY+btnH);
    } else {
        delRect    = D2D1::RectF(m_contentX+m_contentW-btnW, btnY, m_contentX+m_contentW, btnY+btnH);
        importRect = D2D1::RectF(m_contentX+m_contentW-btnW*2-8*m_dpiScale, btnY, m_contentX+m_contentW-btnW-8*m_dpiScale, btnY+btnH);
        addRect    = D2D1::RectF(m_contentX+m_contentW-btnW*3-16*m_dpiScale, btnY, m_contentX+m_contentW-btnW*2-16*m_dpiScale, btnY+btnH);
    }

    int newHoveredBtn = -1;
    if (x >= addRect.left && x <= addRect.right && y >= addRect.top && y <= addRect.bottom) {
        newHoveredBtn = 2;
    } else if (x >= delRect.left && x <= delRect.right && y >= delRect.top && y <= delRect.bottom) {
        newHoveredBtn = 1;
    } else if (x >= importRect.left && x <= importRect.right && y >= importRect.top && y <= importRect.bottom) {
        newHoveredBtn = 0;
    }
    
    int oldHoverBtn = m_hoveredBtn;
    m_hoveredBtn = newHoveredBtn;
    
    float itemH = 74.f * m_dpiScale + ds::Space::S * m_dpiScale;
    float cardH = 74.f * m_dpiScale;
    float listTop = m_contentY + 44.f * m_dpiScale + 38.f * m_dpiScale + 26.f * m_dpiScale;
    
    int oldHoverItem = m_hoveredItem;
    bool oldHoverDelBtn = m_hoveredDelBtn;
    m_hoveredDelBtn = false;

    if (x >= m_contentX && x <= m_contentX + m_contentW && y >= listTop) {
        float relY = (y - listTop) + m_scrollY;
        if (relY >= 0.f) {
            int idx = (int)(relY / itemH);
            float cardRelY = relY - idx * itemH;
            
            std::vector<size_t> filtered = GetFilteredIndices(m_currentFilter);
            if (idx >= 0 && idx < (int)filtered.size() && cardRelY <= cardH) {
                m_hoveredItem = (int)filtered[idx];

                float delSize = 28.f * m_dpiScale;
                float delLeft = m_contentX + (isRtl ? (32.f * m_dpiScale) : (16.f * m_dpiScale));
                float delTop = (listTop - m_scrollY + idx * itemH) + (cardH - delSize) / 2.f;
                float delRight = delLeft + delSize;
                float delBottom = delTop + delSize;

                if (x >= delLeft - 4.f*m_dpiScale && x <= delRight + 4.f*m_dpiScale &&
                    y >= delTop - 4.f*m_dpiScale && y <= delBottom + 4.f*m_dpiScale) {
                    m_hoveredDelBtn = true;
                }
            } else {
                m_hoveredItem = -1;
            }
        } else {
            m_hoveredItem = -1;
        }
    } else {
        m_hoveredItem = -1;
    }

    return (m_hoveredBtn != oldHoverBtn || m_hoveredItem != oldHoverItem || m_hoveredDelBtn != oldHoverDelBtn);
}

void LibraryPage::OnMouseLeave() { 
    m_hoveredItem = -1; 
    m_hoveredBtn = -1; 
    m_hoveredDelBtn = false;
}

bool LibraryPage::HitTestInteractive(int x, int y) {
    return m_hoveredItem >= 0 || m_hoveredBtn >= 0 || m_showAddModal;
}

} // namespace dhikr
