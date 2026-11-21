#pragma once
#include "../MainWindow.h"
#include <string>

namespace dhikr {

class AboutPage : public PageBase {
public:
    AboutPage();
    ~AboutPage();
    void Draw(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds) override;
    bool OnMouseMove(int x, int y) override;
    bool OnLButtonDown(int x, int y) override;
    bool OnLButtonUp(int x, int y) override;
    void OnMouseLeave() override;
    bool HitTestInteractive(int x, int y) override;
    void ResetD2D() override {
        if (m_brush) { m_brush->Release(); m_brush = nullptr; }
        if (m_bmpQrVodafone) { m_bmpQrVodafone->Release(); m_bmpQrVodafone = nullptr; }
        if (m_bmpQrIban) { m_bmpQrIban->Release(); m_bmpQrIban = nullptr; }
    }

private:
    void DrawCrescentLogo(ID2D1RenderTarget* rt, float cx, float cy, float radius);
    void EnsureQrBitmaps(ID2D1RenderTarget* rt);
    void DrawQrModal(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds);

    IDWriteFactory*    m_dwf     = nullptr;
    IDWriteTextFormat* m_tfTitle         = nullptr;
    IDWriteTextFormat* m_tfSectionHeader = nullptr;
    IDWriteTextFormat* m_tfSectionSub    = nullptr;
    IDWriteTextFormat* m_tfSub           = nullptr;
    IDWriteTextFormat* m_tfBody          = nullptr;
    IDWriteTextFormat* m_tfSmall         = nullptr;
    IDWriteTextFormat* m_tfBtn           = nullptr;
    IDWriteTextFormat* m_tfMono          = nullptr;
    IDWriteTextFormat* m_tfBadge         = nullptr;
    ID2D1SolidColorBrush* m_brush = nullptr;
    float m_dpiScale = 1.f;

    ID2D1Bitmap* m_bmpQrVodafone = nullptr;
    ID2D1Bitmap* m_bmpQrIban     = nullptr;

    // Interaction state:
    // 0 = none
    // 1 = email link, 2 = copy email
    // 3 = vodafone copy, 4 = vodafone QR button
    // 5 = IBAN copy, 6 = IBAN QR button
    // 10 = modal close button, 11 = modal copy button
    int m_hoveredBtn = 0;
    int m_pressedBtn = 0;
    int m_justCopiedBtn = 0;
    DWORD m_copyTime = 0;

    int m_activeModal = 0; // 0=none, 1=vodafone, 2=iban

    D2D1_RECT_F m_rectEmail         {};
    D2D1_RECT_F m_rectCopy          {};

    // Support section rects
    D2D1_RECT_F m_rectVodafoneCard  {};
    D2D1_RECT_F m_btnVodafoneCopy   {};
    D2D1_RECT_F m_btnVodafoneQr     {};

    D2D1_RECT_F m_rectIbanCard      {};
    D2D1_RECT_F m_btnIbanCopy       {};
    D2D1_RECT_F m_btnIbanQr         {};

    // Modal rects
    D2D1_RECT_F m_modalRect         {};
    D2D1_RECT_F m_modalCloseBtn     {};
    D2D1_RECT_F m_modalCopyBtn      {};
};

} // namespace dhikr
