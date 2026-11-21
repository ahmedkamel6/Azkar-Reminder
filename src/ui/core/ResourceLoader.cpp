#include "ResourceLoader.h"
#include "../../core/Logger.h"

namespace dhikr {

ID2D1Bitmap* ResourceLoader::LoadBitmapFromResource(
    HINSTANCE hInstance,
    int resourceId,
    IWICImagingFactory* pWicFactory,
    ID2D1RenderTarget* pRenderTarget,
    const wchar_t* resourceType,
    UINT targetWidth,
    UINT targetHeight)
{
    if (!pWicFactory || !pRenderTarget) {
        LOG_WARN("ResourceLoader: WIC factory or RenderTarget is null");
        return nullptr;
    }

    if (!hInstance) {
        hInstance = GetModuleHandleW(nullptr);
    }

    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), resourceType);
    if (!hRes) {
        LOG_WARN("ResourceLoader: FindResource failed for ID " + std::to_string(resourceId));
        return nullptr;
    }

    HGLOBAL hData = LoadResource(hInstance, hRes);
    if (!hData) {
        LOG_WARN("ResourceLoader: LoadResource failed for ID " + std::to_string(resourceId));
        return nullptr;
    }

    void* pBuffer = LockResource(hData);
    DWORD dwSize = SizeofResource(hInstance, hRes);
    if (!pBuffer || dwSize == 0) {
        LOG_WARN("ResourceLoader: LockResource returned empty buffer for ID " + std::to_string(resourceId));
        return nullptr;
    }

    IWICStream* pStream = nullptr;
    HRESULT hr = pWicFactory->CreateStream(&pStream);
    if (FAILED(hr) || !pStream) {
        LOG_WARN("ResourceLoader: CreateStream failed");
        return nullptr;
    }

    hr = pStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pBuffer), dwSize);
    if (FAILED(hr)) {
        LOG_WARN("ResourceLoader: InitializeFromMemory failed");
        pStream->Release();
        return nullptr;
    }

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pWicFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder);
    pStream->Release();
    if (FAILED(hr) || !pDecoder) {
        LOG_WARN("ResourceLoader: CreateDecoderFromStream failed");
        return nullptr;
    }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr) || !pFrame) {
        LOG_WARN("ResourceLoader: GetFrame failed");
        pDecoder->Release();
        return nullptr;
    }

    // 1. Convert source frame to 32bpp PBGRA (Premultiplied Alpha) FIRST
    // Scaling in premultiplied color space prevents dark edge fringes and preserve anti-aliased alpha blending
    IWICFormatConverter* pConverter = nullptr;
    hr = pWicFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr) || !pConverter) {
        LOG_WARN("ResourceLoader: CreateFormatConverter failed");
        pFrame->Release();
        pDecoder->Release();
        return nullptr;
    }

    hr = pConverter->Initialize(
        pFrame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        LOG_WARN("ResourceLoader: FormatConverter Initialize failed");
        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        return nullptr;
    }

    // 2. High-quality bicubic downsampler on premultiplied pixels
    IWICBitmapSource* pFinalSource = pConverter;
    IWICBitmapScaler* pScaler = nullptr;
    if (targetWidth > 0 && targetHeight > 0) {
        hr = pWicFactory->CreateBitmapScaler(&pScaler);
        if (SUCCEEDED(hr) && pScaler) {
            // WICBitmapInterpolationModeFant performs full area-averaging when downsampling
            // large assets (1024px) to compact UI emblems (28–60px), avoiding jagged rings and aliasing.
            hr = pScaler->Initialize(pConverter, targetWidth, targetHeight, WICBitmapInterpolationModeFant);
            if (SUCCEEDED(hr)) {
                pFinalSource = pScaler;
            }
        }
    }

    ID2D1Bitmap* pBitmap = nullptr;
    hr = pRenderTarget->CreateBitmapFromWicBitmap(pFinalSource, nullptr, &pBitmap);
    if (FAILED(hr)) {
        LOG_WARN("ResourceLoader: CreateBitmapFromWicBitmap failed");
    }

    if (pScaler) pScaler->Release();
    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();

    return pBitmap;
}

std::string ResourceLoader::LoadStringFromResource(
    HINSTANCE hInstance,
    int resourceId,
    const wchar_t* resourceType)
{
    if (!hInstance) {
        hInstance = GetModuleHandleW(nullptr);
    }

    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), resourceType);
    if (!hRes) return {};

    HGLOBAL hData = LoadResource(hInstance, hRes);
    if (!hData) return {};

    const char* pBuffer = reinterpret_cast<const char*>(LockResource(hData));
    DWORD dwSize = SizeofResource(hInstance, hRes);
    if (!pBuffer || dwSize == 0) return {};

    return std::string(pBuffer, dwSize);
}

} // namespace dhikr
