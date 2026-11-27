// 画像の読み書き (WIC)。PNG / JPEG / BMP / GIF などを ARGB8888 (アルファは乗算しない) に展開する
#include "WinInternal.hpp"
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <mutex>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

namespace win {
    std::wstring Widen(std::string_view s) {
        if (s.empty()) { return {}; }
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w((size_t)std::max(n, 0), L'\0');
        if (n > 0) { MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n); }
        return w;
    }
    std::string Narrow(std::wstring_view s) {
        if (s.empty()) { return {}; }
        const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string out((size_t)std::max(n, 0), '\0');
        if (n > 0) { WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr); }
        return out;
    }

    void EnsureCom() {
        thread_local bool done = false;
        if (done) { return; }
        done = true;
        // 既に別の方式で初期化されていても (RPC_E_CHANGED_MODE) WIC は使える
        (void)CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    }

    namespace {
        IWICImagingFactory* Factory() {
            static std::once_flag once;
            static ComPtr<IWICImagingFactory> factory;
            EnsureCom();
            std::call_once(once, [] {
                CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
            });
            return factory.Get();
        }
        bool Read(IWICBitmapDecoder* decoder, gfx::Image& out) {
            ComPtr<IWICBitmapFrameDecode> frame;
            if (FAILED(decoder->GetFrame(0, &frame))) { return false; }
            ComPtr<IWICFormatConverter> conv;
            if (FAILED(Factory()->CreateFormatConverter(&conv))) { return false; }
            if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) { return false; }
            UINT w = 0, h = 0;
            if (FAILED(conv->GetSize(&w, &h)) || w == 0 || h == 0) { return false; }
            out.W = (int)w; out.H = (int)h;
            out.Pixels.resize((size_t)w * h);
            // BGRA のバイト列は、リトルエンディアンの 0xAARRGGBB (ARGB8888) と同じ
            if (FAILED(conv->CopyPixels(nullptr, w * 4, (UINT)(out.Pixels.size() * 4), reinterpret_cast<BYTE*>(out.Pixels.data())))) {
                out = {};
                return false;
            }
            return true;
        }
    }

    bool DecodeImageFile(const std::string& path, gfx::Image& out) {
        out = {};
        IWICImagingFactory* f = Factory();
        if (!f) { return false; }
        ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(f->CreateDecoderFromFilename(Widen(path).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) { return false; }
        return Read(decoder.Get(), out);
    }
    bool DecodeImageMemory(const void* data, size_t size, gfx::Image& out) {
        out = {};
        IWICImagingFactory* f = Factory();
        if (!f || !data || size == 0) { return false; }
        ComPtr<IWICStream> stream;
        if (FAILED(f->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory((BYTE*)data, (DWORD)size))) { return false; }
        ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(f->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) { return false; }
        return Read(decoder.Get(), out);
    }
    bool ImageFileSize(const std::string& path, int& w, int& h) {
        w = h = 0;
        IWICImagingFactory* f = Factory();
        if (!f) { return false; }
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(f->CreateDecoderFromFilename(Widen(path).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) { return false; }
        if (FAILED(decoder->GetFrame(0, &frame))) { return false; }
        UINT uw = 0, uh = 0;
        if (FAILED(frame->GetSize(&uw, &uh))) { return false; }
        w = (int)uw; h = (int)uh;
        return w > 0 && h > 0;
    }
    bool EncodePng(const std::string& path, const gfx::Image& image) {
        IWICImagingFactory* f = Factory();
        if (!f || !image) { return false; }
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        if (FAILED(f->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(Widen(path).c_str(), GENERIC_WRITE))) { return false; }
        if (FAILED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) || FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) { return false; }
        if (FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr))) { return false; }
        if (FAILED(frame->SetSize((UINT)image.W, (UINT)image.H))) { return false; }
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        if (FAILED(frame->SetPixelFormat(&format))) { return false; }
        if (format != GUID_WICPixelFormat32bppBGRA) { return false; }
        if (FAILED(frame->WritePixels((UINT)image.H, (UINT)image.W * 4, (UINT)(image.Pixels.size() * 4), (BYTE*)image.Pixels.data()))) { return false; }
        return SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    }
}
