#include "mikudancestudio/adaptive_shadow.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/build_profile.hpp"

#if MMD_ORIGINAL_PROFILE
namespace mikudancestudio {
void LoadShadowResolutionPreference(MMDApp* app) {
    // The original build never consumes the full build's adaptive preference.
    app->AdaptiveShadowResolution() = false;
}
void SaveShadowResolutionPreference(MMDApp*) {}
void UpdateAdaptiveShadowMap(MMDApp*, const float[16]) {}
int ShadowSliderPosition(MMDApp* app) {
    // Preserve the original truncation followed by a positive half-step test.
    const double raw = 10000.0 - double(app->ShadowDistance()) * 100000.0;
    int position = static_cast<int>(raw);
    if (raw - double(position) >= 0.5) ++position;
    return position;
}
void SetShadowSliderPosition(MMDApp* app, int position) {
    app->ShadowDistance() = float((10000.0 - position) / 100000.0);
    app->SceneModified() = 1;
}
}
#else
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/native_mesh.hpp"
#include "mikudancestudio/d3dx_dyn.hpp"
#include "mikudancestudio/d3dx_mesh.hpp"
#include "mikudancestudio/d3d_object_guard.hpp"
#include <cstdio>
#include <limits>
#include <string>

extern "C" float* ExpGetAcsWorldMat(float*, int);
namespace mikudancestudio {
namespace {
std::wstring PreferencePath(MMDApp* app) {
    return std::wstring(app->ExeDir()) + L"\\Data\\adaptive_shadow.ini";
}
using Matrix = d3dx::D3DXMATRIXF;
void SampleDepth(IDirect3DVertexBuffer9* buffer, UINT stride, UINT count,
                 const Matrix& worldView, const Matrix& projection, float& nearest) {
    if (!buffer || !PlausibleD3dObject(buffer) || stride < 12) return;
    D3DVERTEXBUFFER_DESC desc{};
    if (FAILED(buffer->GetDesc(&desc))) return;
    count = (std::min)(count, desc.Size / stride);
    void* data = nullptr;
    if (!count || FAILED(buffer->Lock(0, 0, &data, D3DLOCK_READONLY))) return;
    const UINT step = (std::max)(1u, count / 4096u);
    for (UINT i = 0; i < count; i += step) {
        const auto* p = reinterpret_cast<const float*>(static_cast<const char*>(data) + i * stride);
        float view[4]{}, clip[4]{};
        d3dx::Get().vec3Transform(view, p, &worldView);
        if (!std::isfinite(view[2]) || view[2] <= 0) continue;
        d3dx::Get().vec3Transform(clip, view, &projection);
        if (clip[3] > 0 && clip[2] >= 0 && clip[2] <= clip[3] &&
            std::fabs(clip[0]) <= clip[3] * 1.05f && std::fabs(clip[1]) <= clip[3] * 1.05f)
            nearest = (std::min)(nearest, view[2]);
    }
    buffer->Unlock();
}
}

void LoadShadowResolutionPreference(MMDApp* app) {
    FILE* file = nullptr; int enabled = 0, precision = 5000;
    if (_wfopen_s(&file, PreferencePath(app).c_str(), L"rt") == 0) {
        if (fscanf_s(file, "%d", &enabled) != 1) enabled = 0;
        if (fscanf_s(file, "%d", &precision) != 1) precision = 5000;
        fclose(file);
    }
    app->AdaptiveShadowResolution() = enabled == 1;
    app->DynamicShadowPrecision() = (std::clamp)(precision, 0, 9999);
}
void SaveShadowResolutionPreference(MMDApp* app) {
    CreateDirectoryW((std::wstring(app->ExeDir()) + L"\\Data").c_str(), nullptr);
    FILE* file = nullptr;
    if (_wfopen_s(&file, PreferencePath(app).c_str(), L"wt") == 0) {
        fprintf(file, "%d %d\n", app->AdaptiveShadowResolution() ? 1 : 0, app->DynamicShadowPrecision());
        fclose(file);
    }
}

int ShadowSliderPosition(MMDApp* app) {
    return app->AdaptiveShadowResolution() ? app->DynamicShadowPrecision()
        : int(std::lround(10000.0 - double(app->ShadowDistance()) * 100000.0));
}
void SetShadowSliderPosition(MMDApp* app, int position) {
    if (app->AdaptiveShadowResolution()) {
        app->DynamicShadowPrecision() = (std::clamp)(position, 0, 9999);
    } else {
        app->ShadowDistance() = float((10000.0 - position) / 100000.0);
        app->SceneModified() = 1;
    }
}

void UpdateAdaptiveShadowMap(MMDApp* app, const float frame[16]) {
    auto* r = app->Renderer();
    auto* device = r->device;
    D3DCAPS9 caps{};
    if (!device || FAILED(device->GetDeviceCaps(&caps))) return;
    unsigned limit = app->ShadowResolutionLimit();
    while (limit > caps.MaxTextureWidth || limit > caps.MaxTextureHeight) limit /= 2;
    if (limit < 512) return;
    unsigned size = limit;
    if (app->AdaptiveShadowResolution()) {
        Matrix view{}, projection{}, worldView{};
        D3DVIEWPORT9 viewport{};
        device->GetTransform(D3DTS_VIEW, reinterpret_cast<D3DMATRIX*>(&view));
        device->GetTransform(D3DTS_PROJECTION, reinterpret_cast<D3DMATRIX*>(&projection));
        device->GetViewport(&viewport);
        auto& api = d3dx::Get();
        api.multiply(&worldView, reinterpret_cast<const Matrix*>(frame), &view);
        float nearest = std::numeric_limits<float>::infinity();
        for (int slot = 0; slot < kModelSlotCount; ++slot) {
            auto* model = native_mesh::RenderModel(app->ModelSlot(slot));
            if (!model || app->ModelNonDisplayMode()) continue;
            const auto& record = *mdl::Mdl(model);
            if (!record.loadComplete) continue;
            const UINT stride = 32 + 16 * (std::min)(4u, UINT(record.pmxAdditionalUvCount));
            SampleDepth(static_cast<IDirect3DVertexBuffer9*>(record.vertexBuffer), stride,
                record.vertexCount, worldView, projection, nearest);
        }
        int ordinal = 0;
        for (int slot = 0; slot < kModelSlotCount; ++slot) {
            auto* accessory = native_mesh::RenderAccessory(app->AccessorySlot(slot));
            if (!accessory) continue;
            const int index = ordinal++;
            if (!accessory->visible || accessory->opacity <= 0 || !accessory->mesh ||
                !PlausibleD3dObject(accessory->mesh) || d3dx::MeshFvf(accessory->mesh) != 274) continue;
            IDirect3DVertexBuffer9* buffer = nullptr;
            if (SUCCEEDED(d3dx::MeshVertexBuffer(accessory->mesh, &buffer)) && buffer) {
                Matrix local{}, transformed{};
                ExpGetAcsWorldMat(&local.m[0][0], index);
                api.multiply(&transformed, &local, &worldView);
                SampleDepth(buffer, 32, UINT_MAX, transformed, projection, nearest);
                buffer->Release();
            }
        }
        const float scale = (std::max)(0.00001f, app->ShadowDistance());
        const float depth = projection.m[3][3] == 1.f ? 1.f : (std::max)(1.f, nearest);
        // Estimate the screen pixels covered by the unwarped shadow extent.
        // Keep the fine-shadow setting as the quality ceiling, including export.
        const float required = std::isfinite(nearest)
            ? 1.25f * viewport.Height * std::fabs(projection.m[1][1]) / (scale * depth) : 512.f;
        size = AdaptiveShadowSize(required * DynamicShadowMultiplier(app->DynamicShadowPrecision()),
                                  unsigned(r->renderTargetWidth), limit);
    }
    if (r->hdrTexture && r->shadowSurface && r->shadowDepthSurface && unsigned(r->renderTargetWidth) == size &&
        unsigned(r->renderTargetHeight) == size) return;
    IDirect3DTexture9* texture = nullptr;
    IDirect3DSurface9* surface = nullptr;
    IDirect3DSurface9* depth = nullptr;
    while (size >= 512) {
        if (SUCCEEDED(d3dx::Get().createTexture(device, size, size, 1, D3DUSAGE_RENDERTARGET,
                D3DFMT_R32F, D3DPOOL_DEFAULT, &texture)) &&
            SUCCEEDED(texture->GetSurfaceLevel(0, &surface)) &&
            SUCCEEDED(device->CreateDepthStencilSurface(size, size, D3DFMT_D24X8,
                D3DMULTISAMPLE_NONE, 0, FALSE, &depth, nullptr))) break;
        if (depth) { depth->Release(); depth = nullptr; }
        if (surface) { surface->Release(); surface = nullptr; }
        if (texture) { texture->Release(); texture = nullptr; }
        size /= 2;
    }
    if (!texture || !surface || !depth) return;
    if (r->hdrTexture) r->hdrTexture->Release();
    if (r->shadowSurface) r->shadowSurface->Release();
    if (r->shadowDepthSurface) r->shadowDepthSurface->Release();
    r->hdrTexture = texture; r->shadowSurface = surface; r->shadowDepthSurface = depth;
    r->renderTargetWidth = r->renderTargetHeight = size;
}
}

#endif
