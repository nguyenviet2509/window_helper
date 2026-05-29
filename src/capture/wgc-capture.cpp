// WGC capture implementation. BGRA8, free-threaded frame pool, staging copy -> cv::Mat.
#include "wgc-capture.h"

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <opencv2/core.hpp>
#include <cstdio>

using Microsoft::WRL::ComPtr;
namespace wgc = winrt::Windows::Graphics::Capture;
namespace wgdx = winrt::Windows::Graphics::DirectX;
namespace wgdx11 = winrt::Windows::Graphics::DirectX::Direct3D11;

// Helper: get raw ID3D11Texture2D from IDirect3DSurface (WinRT).
static ComPtr<ID3D11Texture2D> SurfaceToTexture(const wgdx11::IDirect3DSurface& surf) {
    auto access = surf.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    ComPtr<ID3D11Texture2D> tex;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&tex)));
    return tex;
}

struct WgcCapture::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    wgdx11::IDirect3DDevice winrtDevice{ nullptr };
    wgc::GraphicsCaptureItem item{ nullptr };
    wgc::Direct3D11CaptureFramePool pool{ nullptr };
    wgc::GraphicsCaptureSession session{ nullptr };
    winrt::event_token frameToken{};
    bool aptInit = false;

    ComPtr<ID3D11Texture2D> staging;
    UINT stagingW = 0, stagingH = 0;
};

WgcCapture::WgcCapture() : impl_(std::make_unique<Impl>()) {}
WgcCapture::~WgcCapture() { stop(); }

bool WgcCapture::start(HWND target) {
    if (!target || !IsWindow(target)) return false;
    target_ = target;

    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        impl_->aptInit = true;
    } catch (...) {
        // already initialized in some other mode; tolerate.
    }

    // D3D11 device.
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   nullptr, 0, D3D11_SDK_VERSION,
                                   &impl_->device, &fl, &impl_->ctx);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxgi;
    if (FAILED(impl_->device.As(&dxgi))) return false;

    winrt::com_ptr<::IInspectable> insp;
    if (FAILED(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(),
              reinterpret_cast<::IInspectable**>(winrt::put_abi(insp)))))
        return false;
    impl_->winrtDevice = insp.as<wgdx11::IDirect3DDevice>();

    // Create GraphicsCaptureItem for HWND.
    auto interop = winrt::get_activation_factory<wgc::GraphicsCaptureItem,
                                                 IGraphicsCaptureItemInterop>();
    wgc::GraphicsCaptureItem item{ nullptr };
    if (FAILED(interop->CreateForWindow(target, winrt::guid_of<wgc::GraphicsCaptureItem>(),
                                        winrt::put_abi(item))))
        return false;
    impl_->item = item;

    auto sz = item.Size();
    impl_->pool = wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
        impl_->winrtDevice, wgdx::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, sz);

    impl_->frameToken = impl_->pool.FrameArrived(
        [this](wgc::Direct3D11CaptureFramePool const& pool, auto&&) {
            auto frame = pool.TryGetNextFrame();
            if (!frame) return;
            auto tex = SurfaceToTexture(frame.Surface());

            D3D11_TEXTURE2D_DESC desc{};
            tex->GetDesc(&desc);

            // (re)create staging if size changed.
            if (!impl_->staging || impl_->stagingW != desc.Width || impl_->stagingH != desc.Height) {
                D3D11_TEXTURE2D_DESC sd = desc;
                sd.Usage = D3D11_USAGE_STAGING;
                sd.BindFlags = 0;
                sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                sd.MiscFlags = 0;
                impl_->staging.Reset();
                if (FAILED(impl_->device->CreateTexture2D(&sd, nullptr, &impl_->staging))) return;
                impl_->stagingW = desc.Width;
                impl_->stagingH = desc.Height;
            }
            impl_->ctx->CopyResource(impl_->staging.Get(), tex.Get());

            D3D11_MAPPED_SUBRESOURCE map{};
            if (FAILED(impl_->ctx->Map(impl_->staging.Get(), 0, D3D11_MAP_READ, 0, &map))) return;

            Frame f;
            f.bgra.create((int)desc.Height, (int)desc.Width, CV_8UC4);
            const uint8_t* src = static_cast<const uint8_t*>(map.pData);
            for (UINT y = 0; y < desc.Height; ++y) {
                std::memcpy(f.bgra.ptr(y), src + y * map.RowPitch, desc.Width * 4u);
            }
            impl_->ctx->Unmap(impl_->staging.Get(), 0);

            f.seq = ++seq_;
            f.ts = std::chrono::steady_clock::now();
            GetClientRect(target_, &f.windowRect);
            slot_.push(std::move(f));
        });

    impl_->session = impl_->pool.CreateCaptureSession(impl_->item);
    // Cursor capture OFF (best-effort; older SDKs may not have setter).
    try { impl_->session.IsCursorCaptureEnabled(false); } catch (...) {}
    impl_->session.StartCapture();
    return true;
}

void WgcCapture::stop() {
    if (!impl_) return;
    try {
        if (impl_->pool && impl_->frameToken) {
            impl_->pool.FrameArrived(impl_->frameToken);
            impl_->frameToken = {};
        }
        if (impl_->session) { impl_->session.Close(); impl_->session = nullptr; }
        if (impl_->pool)    { impl_->pool.Close();    impl_->pool = nullptr; }
    } catch (...) {}
    slot_.stop();
    impl_->item = nullptr;
    impl_->winrtDevice = nullptr;
    impl_->staging.Reset();
    impl_->ctx.Reset();
    impl_->device.Reset();
}

bool WgcCapture::acquire(Frame& out, int timeoutMs) {
    return slot_.pop(out, timeoutMs);
}
