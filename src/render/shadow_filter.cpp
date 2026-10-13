#include "mikudancestudio/shadow_filter.hpp"
#include "mikudancestudio/build_profile.hpp"
#include "mikudancestudio/d3d_wrapper.hpp"
#include "mikudancestudio/d3dx_effect.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include <algorithm>
#include <unordered_map>
#include <utility>

namespace mikudancestudio::shadow_filter {
using Microsoft::WRL::ComPtr;
namespace {
HRESULT FirstFailure(HRESULT first, HRESULT next) {
    return FAILED(first) ? first : next;
}

struct AttemptKey {
    IDirect3DDevice9* device = nullptr;
    IDirect3DTexture9* coarse = nullptr; // Identity only; hdrTexture owns it.
    UINT width = 0, height = 0, requested = 0;
    UINT maxWidth = 0, maxHeight = 0;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    HRESULT deviceStatus = E_FAIL;
    bool operator==(const AttemptKey& other) const noexcept {
        return device == other.device && coarse == other.coarse &&
            width == other.width && height == other.height &&
            requested == other.requested && maxWidth == other.maxWidth &&
            maxHeight == other.maxHeight && format == other.format &&
            deviceStatus == other.deviceStatus;
    }
};

struct State {
    FineDepthTarget fine;
    AttemptKey key;
    bool hasKey = false;
    bool failed = false;
    bool pending = false;
    bool valid = false;
    bool directionalCoarse = false;
};
std::unordered_map<D3DRenderer*, State> states;

bool Enabled(MMDApp* app) {
    if constexpr (build_profile::original) return false;
    return app && app->ShadowFilterEnhanced() && app->ShadowMode() > 0 &&
        app->Renderer() && app->Renderer()->device &&
        app->Renderer()->shaderModelCaps >= 3;
}

bool ReadCoarse(D3DRenderer* renderer, D3DSURFACE_DESC& desc) {
    return renderer && renderer->hdrTexture &&
        SUCCEEDED(renderer->hdrTexture->GetLevelDesc(0, &desc)) &&
        desc.Width && desc.Height && desc.Format == D3DFMT_R32F;
}

bool Current(const State& state, D3DRenderer* renderer) {
    return state.hasKey && state.key.device == renderer->device &&
        state.key.coarse == renderer->hdrTexture;
}

void Fail(State& state) {
    state.pending = state.valid = state.directionalCoarse = false;
    state.fine.Release();
    state.failed = true;
}
} // namespace

void FineDepthTarget::Release() noexcept {
    depth_.Reset();
    surface_.Reset();
    texture_.Reset();
    device_.Reset();
    size_ = 0;
}

HRESULT FineDepthTarget::Ensure(IDirect3DDevice9* device, UINT coarseSize,
                                UINT requestedFineSize) {
    if (!device || !coarseSize || requestedFineSize <= coarseSize)
        return E_INVALIDARG;
    if (device_.Get() == device && size_ == requestedFineSize &&
        texture_ && surface_ && depth_) return S_OK;

    ComPtr<IDirect3DTexture9> texture;
    ComPtr<IDirect3DSurface9> surface, depth;
    HRESULT hr = device->CreateTexture(requestedFineSize, requestedFineSize, 1,
        D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &texture, nullptr);
    if (SUCCEEDED(hr)) hr = texture->GetSurfaceLevel(0, &surface);
    if (SUCCEEDED(hr)) hr = device->CreateDepthStencilSurface(requestedFineSize,
        requestedFineSize, D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, FALSE, &depth, nullptr);
    // Allocation never changes the native target. Do not retain an obsolete
    // fine allocation after a failed resize, even if it rendered last frame.
    Release();
    if (FAILED(hr)) return hr;
    device_ = device;
    texture_ = std::move(texture);
    surface_ = std::move(surface);
    depth_ = std::move(depth);
    size_ = requestedFineSize;
    return S_OK;
}

void TargetScope::ClearSaved() noexcept {
    state_.Reset();
    depth_.Reset();
    for (auto& target : targets_) target.Reset();
    device_.Reset();
    targetCount_ = 0;
    captured_ = false;
}

HRESULT TargetScope::Begin(IDirect3DDevice9* device, const FineDepthTarget& target) {
    if (captured_ || !device || !target.Surface() || !target.DepthSurface())
        return E_INVALIDARG;
    ClearSaved();
    device_ = device;
    D3DCAPS9 caps{};
    HRESULT hr = device->GetDeviceCaps(&caps);
    if (FAILED(hr)) { ClearSaved(); return hr; }
    targetCount_ = (std::clamp)(static_cast<UINT>(caps.NumSimultaneousRTs), 1u, 4u);
    hr = device->CreateStateBlock(D3DSBT_ALL, &state_);
    if (SUCCEEDED(hr)) hr = state_->Capture();
    if (SUCCEEDED(hr)) hr = device->GetViewport(&viewport_);
    if (SUCCEEDED(hr)) hr = device->GetRenderTarget(0, &targets_[0]);
    if (FAILED(hr)) { ClearSaved(); return hr; }
    for (UINT i = 1; i < targetCount_; ++i) {
        hr = device->GetRenderTarget(i, &targets_[i]);
        if (FAILED(hr) && hr != D3DERR_NOTFOUND) { ClearSaved(); return hr; }
    }
    hr = device->GetDepthStencilSurface(&depth_);
    if (FAILED(hr) && hr != D3DERR_NOTFOUND) { ClearSaved(); return hr; }
    captured_ = true;

    // D3DSBT_ALL restores these bindings. Detach aliases before attaching the
    // fine texture as a target; external effects may use any pixel sampler.
    hr = S_OK;
    for (DWORD i = 0; i < 16; ++i)
        hr = FirstFailure(hr, device->SetTexture(i, nullptr));
    if (D3DSHADER_VERSION_MAJOR(caps.VertexShaderVersion) >= 3 && caps.VertexTextureFilterCaps) {
        for (DWORD i = 0; i < 4; ++i)
            hr = FirstFailure(hr, device->SetTexture(D3DVERTEXTEXTURESAMPLER0 + i, nullptr));
    }
    hr = FirstFailure(hr, device->SetDepthStencilSurface(nullptr));
    for (UINT i = 1; i < targetCount_; ++i)
        hr = FirstFailure(hr, device->SetRenderTarget(i, nullptr));
    hr = FirstFailure(hr, device->SetRenderTarget(0, target.Surface()));
    hr = FirstFailure(hr, device->SetDepthStencilSurface(target.DepthSurface()));
    D3DVIEWPORT9 viewport{0, 0, target.Size(), target.Size(), 0.0f, 1.0f};
    hr = FirstFailure(hr, device->SetViewport(&viewport));
    hr = FirstFailure(hr, device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                      0xffffffffu, 1.0f, 0));
    if (FAILED(hr)) Restore();
    return hr;
}

HRESULT TargetScope::Restore() noexcept {
    if (!captured_) { ClearSaved(); return S_OK; }
    captured_ = false;
    HRESULT hr = device_->SetDepthStencilSurface(nullptr);
    for (UINT i = 1; i < targetCount_; ++i)
        hr = FirstFailure(hr, device_->SetRenderTarget(i, nullptr));
    hr = FirstFailure(hr, device_->SetRenderTarget(0, targets_[0].Get()));
    for (UINT i = 1; i < targetCount_; ++i)
        if (targets_[i]) hr = FirstFailure(hr, device_->SetRenderTarget(i, targets_[i].Get()));
    hr = FirstFailure(hr, device_->SetDepthStencilSurface(depth_.Get()));
    hr = FirstFailure(hr, state_->Apply());
    // SetRenderTarget resets the viewport; restore it after the state block.
    hr = FirstFailure(hr, device_->SetViewport(&viewport_));
    ClearSaved();
    return hr;
}

void Invalidate(MMDApp* app) {
    if (!app || !app->Renderer()) return;
    if (!Enabled(app)) { ReleaseRenderer(app->Renderer(), true); return; }
    const auto found = states.find(app->Renderer());
    if (found == states.end()) return;
    auto& state = found->second;
    state.pending = state.valid = state.directionalCoarse = false;
}

FineDepthTarget* Prepare(MMDApp* app, bool nativeSucceeded) {
    if (!Enabled(app)) { Invalidate(app); return nullptr; }
    auto* renderer = app->Renderer();
    auto& state = states.try_emplace(renderer).first->second;
    state.pending = state.valid = state.directionalCoarse = false;
    D3DSURFACE_DESC coarse{};
    if (!nativeSucceeded || !ReadCoarse(renderer, coarse) || coarse.Width != coarse.Height) {
        state.fine.Release();
        return nullptr;
    }
    D3DCAPS9 caps{};
    if (FAILED(renderer->device->GetDeviceCaps(&caps))) {
        state.fine.Release();
        return nullptr;
    }
    const UINT cap = (std::min)({8192u, static_cast<UINT>(caps.MaxTextureWidth),
                                static_cast<UINT>(caps.MaxTextureHeight)});
    if (!cap) { state.fine.Release(); return nullptr; }
    const UINT requested = (std::min)(cap, (std::max)(4096u,
        (std::min)(coarse.Width, 8192u) * 2u));
    const AttemptKey key{renderer->device, renderer->hdrTexture, coarse.Width,
        coarse.Height, requested, caps.MaxTextureWidth, caps.MaxTextureHeight,
        coarse.Format, renderer->device->TestCooperativeLevel()};
    if (!state.hasKey || !(state.key == key)) {
        state.fine.Release();
        state.key = key;
        state.hasKey = true;
        state.failed = false;
    }
    if (FAILED(key.deviceStatus)) { Fail(state); return nullptr; }
    if (requested <= coarse.Width) {
        state.fine.Release();
        state.directionalCoarse = true;
        return nullptr;
    }
    if (state.failed) return nullptr;
    if (FAILED(state.fine.Ensure(renderer->device, coarse.Width, requested))) {
        Fail(state);
        return nullptr;
    }
    state.pending = true;
    return &state.fine;
}

void MarkRendered(MMDApp* app, bool succeeded) {
    if (!app || !app->Renderer()) return;
    const auto found = states.find(app->Renderer());
    if (found == states.end() || !found->second.pending) return;
    auto& state = found->second;
    if (!succeeded || !Enabled(app) || !Current(state, app->Renderer())) {
        Fail(state);
        return;
    }
    state.pending = false;
    state.valid = state.fine.Texture() != nullptr;
}

IDirect3DTexture9* VisibleTexture(MMDApp* app) {
    if (!app || !app->Renderer()) return nullptr;
    auto* renderer = app->Renderer();
    const auto found = states.find(renderer);
    if (Enabled(app) && found != states.end() && Current(found->second, renderer) &&
        found->second.valid) return found->second.fine.Texture();
    return renderer->hdrTexture;
}

void ConfigureEffect(MMDApp* app, void* effect) {
    if (!effect) return;
    bool enabled = Enabled(app);
    // zw=0 selects the released dynamic spline fallback, never the C kernel.
    float size[4]{1.0f, 1.0f, 0.0f, 0.0f};
    auto* renderer = app ? app->Renderer() : nullptr;
    D3DSURFACE_DESC coarse{}, visible{};
    if (ReadCoarse(renderer, coarse)) {
        size[0] = static_cast<float>(coarse.Width);
        size[1] = static_cast<float>(coarse.Height);
        const auto found = states.find(renderer);
        if (enabled && found != states.end() && Current(found->second, renderer)) {
            auto& state = found->second;
            bool directional = state.directionalCoarse;
            if (state.valid) {
                if (SUCCEEDED(state.fine.Texture()->GetLevelDesc(0, &visible)) &&
                    visible.Width && visible.Height && visible.Format == D3DFMT_R32F) {
                    size[0] = static_cast<float>(visible.Width);
                    size[1] = static_cast<float>(visible.Height);
                    directional = true;
                } else Fail(state);
            }
            if (directional) {
                size[2] = static_cast<float>(coarse.Width);
                size[3] = static_cast<float>(coarse.Height);
            }
        }
    } else {
        enabled = false; // No reliable texel size: retain the original lookup.
    }
    d3dx::FxSetInt(effect, "MdsShadowFilterEnhanced", enabled ? 1 : 0);
    d3dx::FxSetFloatArray(effect, "MdsShadowMapSize", size, 4);
}

void ReleaseRenderer(D3DRenderer* renderer, bool forget) {
    const auto found = states.find(renderer);
    if (found == states.end()) return;
    auto& state = found->second;
    state.pending = state.valid = state.directionalCoarse = false;
    state.fine.Release();
    state.key = {};
    state.hasKey = state.failed = false;
    if (forget) states.erase(found);
}
} // namespace mikudancestudio::shadow_filter
