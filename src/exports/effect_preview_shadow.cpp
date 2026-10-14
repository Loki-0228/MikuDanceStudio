#include "mikudancestudio/effect_preview_shadow.hpp"
#include "mikudancestudio/effect_apng.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/build_profile.hpp"
#include "mikudancestudio/runtime_log.hpp"
#include "effect_preview_shadow_source.hpp"
#include <MinHook.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace mikudancestudio::effect_preview_shadow {
#if defined(_WIN64)
namespace {
using Handle = const char*;
struct ParameterDesc {
    const char *name, *semantic;
    unsigned typeClass, type, rows, columns, elements, annotations, members, flags, bytes;
};
static_assert(sizeof(ParameterDesc) == 56);
struct Buffer : IUnknown {
    virtual void* WINAPI GetBufferPointer() = 0;
    virtual DWORD WINAPI GetBufferSize() = 0;
};
template<class R, class... A> R Call(void* object, unsigned slot, A... args) {
    using Method = R(WINAPI*)(void*, A...);
    return reinterpret_cast<Method>((*static_cast<void***>(object))[slot])(object, args...);
}
template<class T> T& At(void* object, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset);
}
Handle Parameter(void* effect, const char* name) { return Call<Handle>(effect, 9, Handle(nullptr), name); }
Handle Annotation(void* effect, Handle parameter, const char* name) { return Call<Handle>(effect, 19, parameter, name); }
bool StringAnnotation(void* effect, Handle parameter, const char* name, const char* expected) {
    const auto annotation = Annotation(effect, parameter, name);
    const char* value = nullptr;
    return annotation && SUCCEEDED(Call<HRESULT>(effect, 51, annotation, &value)) &&
        value && std::strcmp(value, expected) == 0;
}
bool Dimensions(void* effect, Handle parameter, unsigned& size) {
    int width = 0, height = 0;
    if (const auto dimensions = Annotation(effect, parameter, "Dimensions")) {
        int values[2]{};
        if (FAILED(Call<HRESULT>(effect, 29, dimensions, values, UINT(2)))) return false;
        width = values[0]; height = values[1];
    } else {
        const auto w = Annotation(effect, parameter, "Width"), h = Annotation(effect, parameter, "Height");
        if (!w || !h || FAILED(Call<HRESULT>(effect, 27, w, &width)) ||
            FAILED(Call<HRESULT>(effect, 27, h, &height))) return false;
    }
    if (width != height || width < 512 || width > 16384) return false;
    size = unsigned(width); return true;
}

// MMEffect 0.37 x64 SAS ABI, checked against the shipped binary with REA.
// Entry 0xB880 reloads changed effects and calls 0x14270 on viewport changes.
// Entry 0x14270 rebuilds only records with +0x64 set, including the original
// offscreen depth-registry and effect-pool rebinding. No D3D proxy is replaced.
using Update = HRESULT(__fastcall*)(void*, int, unsigned char*, int, unsigned);
using Unload = void(__fastcall*)(void*);
using Rebuild = HRESULT(__fastcall*)(void*);
using CreateFile = HRESULT(WINAPI*)(IDirect3DDevice9*, LPCWSTR, const void*, void*, DWORD, void*, void**, Buffer**);
using Preprocess = HRESULT(WINAPI*)(LPCWSTR, const void*, void*, Buffer**, Buffer**);
using Create = HRESULT(WINAPI*)(IDirect3DDevice9*, const void*, UINT, const void*, void*, DWORD, void*, void**, Buffer**);
Update originalUpdate = nullptr;
Unload originalUnload = nullptr;
Rebuild rebuild = nullptr;
CreateFile originalCreate = nullptr;
Preprocess preprocess = nullptr;
Create create = nullptr;
DWORD thread = 0;
bool installed = false, attempted = false, disabled = false;
double factor = 1;
struct Resource { void* record = nullptr; unsigned original = 0; };
struct State {
    void* effect = nullptr;
    std::array<Resource, 4> resources{};
    Handle uniform = nullptr;
    bool soft = false;
    unsigned current = 0;
};
std::unordered_map<void*, State> states;

State Identify(void* sas) {
    State state;
    state.effect = At<void*>(sas, 0);
    if (!state.effect) return state;
    const auto soft = Parameter(state.effect, "SoftShadowRT");
    state.soft = soft && StringAnnotation(state.effect, soft, "Description", "OffScreen RenderTarget for SoftShadow.fx");
    const auto ray = Parameter(state.effect, "MMSPreviewShadowSize");
    if (!state.soft && !ray) return state;
    state.uniform = state.soft ? Parameter(state.effect, "ViewportSize") : ray;
    if (!state.uniform) return state;
    constexpr const char* softNames[]{"SoftShadowRT", "ScnMapX", "ScnMapY", "DepthBuffer"};
    constexpr const char* rayNames[]{"PSSM1", "PSSM2", "PSSM3", "PSSM4"};
    auto* begin = At<unsigned char*>(sas, 0x98);
    auto* end = At<unsigned char*>(sas, 0xA0);
    if (!begin || end < begin || size_t(end - begin) > 4096 * 0x28 || (end - begin) % 0x28) return State{state.effect};
    for (unsigned i = 0; i < 4; ++i) {
        const auto parameter = Parameter(state.effect, state.soft ? softNames[i] : rayNames[i]);
        ParameterDesc desc{};
        if (!parameter || FAILED(Call<HRESULT>(state.effect, 4, parameter, &desc)) ||
            !desc.semantic || !Dimensions(state.effect, parameter, state.resources[i].original)) return State{state.effect};
        const char* semantic = !state.soft || i == 0 ? "OFFSCREENRENDERTARGET" :
            i == 3 ? "RENDERDEPTHSTENCILTARGET" : "RENDERCOLORTARGET";
        if (_stricmp(desc.semantic, semantic) ||
            (!state.soft && !StringAnnotation(state.effect, parameter, "Description", "Cascade shadow map for ray"))) return State{state.effect};
        for (auto* entry = begin; entry != end; entry += 0x28) {
            if (At<Handle>(entry, 8) == parameter) {
                state.resources[i].record = At<void*>(entry, 0x18); break;
            }
        }
        auto* record = state.resources[i].record;
        if (!record || At<unsigned char>(record, 0x64) ||
            state.resources[i].original != state.resources[0].original) return State{state.effect};
    }
    if (!state.soft) {
        int samplingSize = 0;
        const auto annotation = Annotation(state.effect, state.uniform, "OriginalSize");
        if (!annotation || FAILED(Call<HRESULT>(state.effect, 27, annotation, &samplingSize)) ||
            unsigned(samplingSize) != state.resources[0].original) return State{state.effect};
    }
    state.current = state.resources[0].original;
    runtime_log::Write("MME_PREVIEW_SHADOW recognized kind=%s size=%u", state.soft ? "soft" : "ray", state.current);
    return state;
}

HRESULT Resize(void* sas, State& state, unsigned size) {
    const unsigned width = At<unsigned>(sas, 0x20), height = At<unsigned>(sas, 0x24);
    if (!width || !height) return E_FAIL;
    // Use the MME viewport rebuild path once per size transition. Restore the
    // fixed-resource flags before returning so regular frames do no extra work.
    std::array<std::array<float, 2>, 4> ratios;
    for (unsigned i = 0; i < 4; ++i) {
        auto* r = state.resources[i].record;
        ratios[i] = {At<float>(r, 0x50), At<float>(r, 0x54)};
        // +0.25 prevents integer truncation to size-1 after float multiply.
        At<float>(r, 0x50) = (float(size) + .25f) / float(width);
        At<float>(r, 0x54) = (float(size) + .25f) / float(height);
        At<unsigned char>(r, 0x64) = 1;
    }
    const HRESULT result = rebuild(sas);
    for (unsigned i = 0; i < 4; ++i) {
        auto* r = state.resources[i].record;
        At<unsigned char>(r, 0x64) = 0;
        At<float>(r, 0x50) = ratios[i][0]; At<float>(r, 0x54) = ratios[i][1];
    }
    if (result != S_OK) return FAILED(result) ? result : E_FAIL;
    if (state.soft) {
        const float value[]{float(size), float(size)};
        if (FAILED(Call<HRESULT>(state.effect, 32, state.uniform, value, UINT(2)))) return E_FAIL;
    } else if (FAILED(Call<HRESULT>(state.effect, 30, state.uniform, float(size)))) return E_FAIL;
    state.current = size;
    runtime_log::Write("MME_PREVIEW_SHADOW resized kind=%s original=%u actual=%u", state.soft ? "soft" : "ray", state.resources[0].original, size);
    return S_OK;
}
HRESULT __fastcall HookUpdate(void* sas, int mode, unsigned char* changed, int width, unsigned height) {
    const HRESULT result = originalUpdate(sas, mode, changed, width, height);
    if (result != S_OK || GetCurrentThreadId() != thread || !At<void*>(sas, 0)) return result;
    try {
        auto found = states.find(sas);
        if (found == states.end() || found->second.effect != At<void*>(sas, 0))
            found = states.insert_or_assign(sas, Identify(sas)).first;
        auto& state = found->second;
        if (!state.resources[3].record) return result;
        const unsigned size = PreviewShadowSize(state.resources[0].original, disabled ? 1 : factor);
        // OnResetDevice recreates fixed targets at their declared dimensions,
        // even if the preview percentage and effect object did not change.
        const bool dimensionsMatch = std::all_of(state.resources.begin(), state.resources.end(),
            [size](const Resource& resource) {
                return At<unsigned>(resource.record, 0x58) == size &&
                       At<unsigned>(resource.record, 0x5c) == size;
            });
        if (size == state.current && dimensionsMatch) return result;
        HRESULT resized = Resize(sas, state, size);
        if (FAILED(resized)) {
            disabled = true;
            runtime_log::Write("MME_PREVIEW_SHADOW fallback hr=0x%08lX", resized);
            resized = Resize(sas, state, state.resources[0].original);
        }
        *changed = 1;
        return FAILED(resized) ? resized : result;
    } catch (...) {
        disabled = true;
        runtime_log::Write("MME_PREVIEW_SHADOW fallback reason=allocation");
        return result;
    }
}
void __fastcall HookUnload(void* sas) {
    if (GetCurrentThreadId() == thread) states.erase(sas);
    originalUnload(sas);
}
HRESULT WINAPI HookCreate(IDirect3DDevice9* device, LPCWSTR path, const void* macros, void* include,
    DWORD flags, void* pool, void** effect, Buffer** errors) {
    try {
        // Only ray's main effect requires an injected sampling parameter.
        // Unknown/mutated shader text is compiled by the original entry point.
        if (GetCurrentThreadId() == thread && path && !include &&
            _wcsicmp(std::filesystem::path(path).filename().c_str(), L"ray.fx") == 0) {
            Microsoft::WRL::ComPtr<Buffer> source, diagnostics;
            if (SUCCEEDED(preprocess(path, macros, include, &source, &diagnostics)) && source) {
                std::string text(static_cast<const char*>(source->GetBufferPointer()), source->GetBufferSize());
                while (!text.empty() && text.back() == '\0') text.pop_back();
                if (AdaptRaySource(text)) {
                    Microsoft::WRL::ComPtr<Buffer> compiledErrors;
                    void* compiled = nullptr;
                    const HRESULT hr = create(device, text.data(), UINT(text.size()), nullptr, nullptr,
                        flags, pool, &compiled, &compiledErrors);
                    if (SUCCEEDED(hr) && compiled) {
                        *effect = compiled;
                        if (errors) *errors = compiledErrors.Detach();
                        runtime_log::Write("MME_PREVIEW_SHADOW ray_sampling adapted");
                        return hr;
                    }
                    if (compiled) static_cast<IUnknown*>(compiled)->Release();
                    runtime_log::Write("MME_PREVIEW_SHADOW ray_sampling fallback hr=0x%08lX", hr);
                }
            }
        }
    } catch (...) { /* Original compiler owns the fallback and diagnostics. */ }
    return originalCreate(device, path, macros, include, flags, pool, effect, errors);
}
bool CheckImage(unsigned char* image) {
    constexpr unsigned char update[]{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c,0x24};
    constexpr unsigned char unload[]{0x4c,0x8b,0xdc,0x56,0x57,0x41,0x54,0x48,0x81,0xec,0xb0,0x00,0x00,0x00,0x48,0xc7};
    constexpr unsigned char resize[]{0x41,0x54,0x48,0x83,0xec,0x50,0x48,0x83,0x39,0x00,0x4c,0x8b,0xe1,0x75,0x09,0x33};
    return !std::memcmp(image + 0xb880, update, sizeof(update)) &&
        !std::memcmp(image + 0xb210, unload, sizeof(unload)) &&
        !std::memcmp(image + 0x14270, resize, sizeof(resize));
}
}
void Install() {
    if (attempted || build_profile::original) return;
    attempted = true;
    if (std::getenv("MMS_NO_MME_PREVIEW_SHADOW")) return;
    // APNG installation already pins and verifies the exact DLL SHA-256, PE
    // identity and loaded code. Its hook does not touch these SAS entry points.
    if (!IsEffectApngCompatibilityInstalled()) return;
    auto* image = reinterpret_cast<unsigned char*>(GetModuleHandleW(L"MMEffect.dll"));
    const auto d3dx = GetModuleHandleW(L"d3dx9_43.dll");
    if (!image || !d3dx || !CheckImage(image)) return;
    originalCreate = reinterpret_cast<CreateFile>(GetProcAddress(d3dx, "D3DXCreateEffectFromFileW"));
    preprocess = reinterpret_cast<Preprocess>(GetProcAddress(d3dx, "D3DXPreprocessShaderFromFileW"));
    create = reinterpret_cast<Create>(GetProcAddress(d3dx, "D3DXCreateEffect"));
    auto* slot = reinterpret_cast<void* volatile*>(image + 0xa4728);
    if (!originalCreate || !preprocess || !create || *slot != reinterpret_cast<void*>(originalCreate)) return;
    const auto initialized = MH_Initialize();
    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) return;
    if (MH_CreateHook(image + 0xb880, HookUpdate, reinterpret_cast<void**>(&originalUpdate)) != MH_OK) return;
    if (MH_CreateHook(image + 0xb210, HookUnload, reinterpret_cast<void**>(&originalUnload)) != MH_OK) {
        MH_RemoveHook(image + 0xb880); return;
    }
    thread = GetCurrentThreadId(); rebuild = reinterpret_cast<Rebuild>(image + 0x14270);
    if (MH_EnableHook(image + 0xb880) != MH_OK || MH_EnableHook(image + 0xb210) != MH_OK) {
        MH_RemoveHook(image + 0xb880); MH_RemoveHook(image + 0xb210); return;
    }
    DWORD protection = 0;
    if (!VirtualProtect(const_cast<void**>(slot), sizeof(void*), PAGE_READWRITE, &protection)) {
        MH_RemoveHook(image + 0xb880); MH_RemoveHook(image + 0xb210); return;
    }
    auto* previous = InterlockedCompareExchangePointer(slot, reinterpret_cast<void*>(HookCreate), reinterpret_cast<void*>(originalCreate));
    DWORD ignored = 0;
    VirtualProtect(const_cast<void**>(slot), sizeof(void*), protection, &ignored);
    if (previous != reinterpret_cast<void*>(originalCreate)) {
        MH_RemoveHook(image + 0xb880); MH_RemoveHook(image + 0xb210); return;
    }
    installed = true;
    runtime_log::Write("MME_PREVIEW_SHADOW installed");
}
void PrepareFrame(MMDApp* app) {
    if (!installed) return;
    factor = 1;
    if (app && !app->RecordingWindow() && !app->viewportRender.automatic &&
        std::isfinite(app->viewportRender.percent) && app->viewportRender.percent > 0)
        factor = std::min(1., app->viewportRender.percent / 100.);
}
std::vector<TargetInfo> InspectTargets() {
    std::vector<TargetInfo> result;
    if (!installed || GetCurrentThreadId() != thread) return result;
    for (const auto& pair : states) {
        const auto& state = pair.second;
        if (!state.resources[3].record) continue;
        for (const auto& resource : state.resources) {
            auto* surface = At<IDirect3DSurface9*>(resource.record, 8);
            D3DSURFACE_DESC desc{};
            if (surface) surface->GetDesc(&desc);
            result.push_back({resource.original, desc.Width, desc.Height, state.soft});
        }
    }
    return result;
}
#else
void Install() {}
void PrepareFrame(MMDApp*) {}
std::vector<TargetInfo> InspectTargets() { return {}; }
#endif
}
