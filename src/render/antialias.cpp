#include "mikudancestudio/antialias.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/graphics_runtime.hpp"
#include "mikudancestudio/runtime_log.hpp"
#include "mikudancestudio/build_profile.hpp"
#include "antialias_shader.hpp"
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <unordered_map>

namespace mikudancestudio::antialias {
using Microsoft::WRL::ComPtr;
namespace {
struct Target {
    ComPtr<IDirect3DTexture9> scene, history[2];
    ComPtr<IDirect3DSurface9> color, depth, historySurface[2];
    ComPtr<IDirect3DPixelShader9> fxaa, temporal;
    D3DSURFACE_DESC description{};
    D3DFORMAT depthFormat=D3DFMT_UNKNOWN;
    std::uint64_t signature=0;
    unsigned samples=0, historyIndex=0;
};
std::unordered_map<MMDApp*,Target> targets;
std::filesystem::path StorePath() {
    wchar_t path[32768]{};
    DWORD n=GetEnvironmentVariableW(L"MMS_ANTIALIAS_CONFIG",path,32768);
    if(n && n<32768)return path;
    n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
    return n && n<32768?std::filesystem::path(path)/L"MikuDanceStudio"/L"antialias.ini":std::filesystem::path();
}
void Save(MMDApp* app) {
    auto path=StorePath();if(path.empty())return;
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
    const auto& settings=app->antialiasing;
    WritePrivateProfileStringW(L"Antialias",L"Algorithm",std::to_wstring(unsigned(settings.algorithm)).c_str(),path.c_str());
    WritePrivateProfileStringW(L"Antialias",L"Enabled",settings.enabled?L"1":L"0",path.c_str());
}
bool PostProcess(const Settings& settings) {
    return settings.enabled && (settings.algorithm==Algorithm::Fxaa || settings.algorithm==Algorithm::Taa);
}
unsigned Samples(MMDApp* app,Algorithm algorithm) {
    switch(algorithm) {
    case Algorithm::Msaa2:return 2;
    case Algorithm::Msaa4:return 4;
    case Algorithm::Msaa8:return 8;
    default:return app->antialiasing.originalSamples;
    }
}
struct ShaderCompilation {
    std::vector<DWORD> bytecode;
    HANDLE completed=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    std::atomic<bool> done{false};
    HRESULT result=E_FAIL;
    ~ShaderCompilation(){if(completed)CloseHandle(completed);}
};
// The UI owns this cache. Workers own only CPU bytecode, never a device or app.
// Retain it across resize/device reset instead of recompiling both algorithms.
std::shared_ptr<ShaderCompilation> shaders[2];
std::shared_ptr<ShaderCompilation> RequestShader(unsigned index) {
    if(shaders[index])return shaders[index];
    auto work=std::make_shared<ShaderCompilation>();shaders[index]=work;
    const char* entry=index?"temporal":"fxaa";
    runtime_log::Write("AA compile queued %s",entry);
    try {
        if(!work->completed)throw std::bad_alloc();
        std::thread([work,entry] {
            const auto start=GetTickCount64();
            try {
                ComPtr<ID3DBlob> code,error;
                work->result=graphics_runtime::Compile(kShader,sizeof(kShader)-1,"antialias",nullptr,nullptr,
                    entry,"ps_3_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);
                if(SUCCEEDED(work->result) && code) {
                    const auto* words=static_cast<const DWORD*>(code->GetBufferPointer());
                    work->bytecode.assign(words,words+code->GetBufferSize()/sizeof(DWORD));
                } else {
                    if(SUCCEEDED(work->result))work->result=E_FAIL;
                    runtime_log::Write("AA compile %s hr=0x%08lX %s",entry,work->result,
                        error?static_cast<const char*>(error->GetBufferPointer()):"");
                }
            }catch(...){work->result=E_OUTOFMEMORY;}
            runtime_log::Write("AA compile finished %s ms=%llu",entry,GetTickCount64()-start);
            work->done.store(true,std::memory_order_release);
            SetEvent(work->completed);
        }).detach();
    }catch(...) {
        work->result=E_OUTOFMEMORY;work->done.store(true,std::memory_order_release);
        if(work->completed)SetEvent(work->completed);
    }
    return work;
}
HRESULT Ensure(MMDApp* app) {
    auto fxaa=RequestShader(0);
    auto temporal=app->antialiasing.algorithm==Algorithm::Taa?RequestShader(1):nullptr;
    for(const auto& shader:{fxaa,temporal})if(shader) {
        // Export waits for the requested output; preview never waits for HLSL.
        if(app->RecordingWindow() && !shader->done.load(std::memory_order_acquire))
            WaitForSingleObject(shader->completed,INFINITE);
        if(!shader->done.load(std::memory_order_acquire))return S_FALSE;
        if(FAILED(shader->result))return shader->result;
    }
    auto* device=app->Renderer()->device;
    ComPtr<IDirect3DSurface9> color,depth;
    HRESULT hr=device->GetRenderTarget(0,&color);if(FAILED(hr))return hr;
    if(FAILED(hr=device->GetDepthStencilSurface(&depth)))return hr;
    D3DSURFACE_DESC desc{},dd{};
    if(FAILED(hr=color->GetDesc(&desc)) || FAILED(hr=depth->GetDesc(&dd)))return hr;
    auto found=targets.find(app);
    if(found!=targets.end() && found->second.description.Width==desc.Width &&
       found->second.description.Height==desc.Height && found->second.description.Format==desc.Format &&
       found->second.depthFormat==dd.Format &&
       (app->antialiasing.algorithm!=Algorithm::Taa || found->second.history[0]))return S_OK;
    Target next;next.description=desc;next.depthFormat=dd.Format;
    const auto texture=[&](ComPtr<IDirect3DTexture9>& tex,ComPtr<IDirect3DSurface9>& surface) {
        HRESULT result=device->CreateTexture(desc.Width,desc.Height,1,D3DUSAGE_RENDERTARGET,
            desc.Format,D3DPOOL_DEFAULT,&tex,nullptr);
        return FAILED(result)?result:tex->GetSurfaceLevel(0,&surface);
    };
    if(FAILED(hr=texture(next.scene,next.color)))return hr;
    if(app->antialiasing.algorithm==Algorithm::Taa)
        for(int i=0;i<2;++i)if(FAILED(hr=texture(next.history[i],next.historySurface[i])))return hr;
    if(FAILED(hr=device->CreateDepthStencilSurface(desc.Width,desc.Height,dd.Format,
        D3DMULTISAMPLE_NONE,0,TRUE,&next.depth,nullptr)))return hr;
    if(FAILED(hr=device->CreatePixelShader(fxaa->bytecode.data(),&next.fxaa)))return hr;
    if(temporal && FAILED(hr=device->CreatePixelShader(temporal->bytecode.data(),&next.temporal)))return hr;
    targets[app]=std::move(next);return S_OK;
}
void Hash(std::uint64_t& value,const void* data,std::size_t size) {
    const auto* bytes=static_cast<const unsigned char*>(data);
    for(std::size_t i=0;i<size;++i)value=(value^bytes[i])*1099511628211ull;
}
std::uint64_t Signature(MMDApp* app,IDirect3DDevice9* device) {
    std::uint64_t hash=14695981039346656037ull;
    for(auto kind:{D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION}) {
        D3DMATRIX matrix{};device->GetTransform(kind,&matrix);Hash(hash,&matrix,sizeof(matrix));
    }
    Hash(hash,&app->CurrentFrame(),sizeof(int));
    Hash(hash,&app->ViewportRect(),sizeof(RECT));
    for(int slot=0;slot<kModelSlotCount;++slot)if(auto* model=app->ModelSlot(slot)) {
        Hash(hash,&model,sizeof(model));
        const auto* record=mdl::Mdl(model);const auto* bones=mdl::Bones(model);
        for(unsigned b=0;b<record->boneCount && bones;++b)Hash(hash,bones[b].matLocal,sizeof(bones[b].matLocal));
        const auto* morphs=mdl::Morphs(model);
        for(unsigned m=0;m<record->morphCount && morphs;++m)Hash(hash,&morphs[m].value,sizeof(float));
    }
    return hash;
}
float Halton(unsigned index,unsigned base) {
    float result=0,fraction=1;
    while(index){fraction/=base;result+=fraction*(index%base);index/=base;}
    return result;
}
void FullscreenState(IDirect3DDevice9* device) {
    device->SetVertexShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
    for(auto state:{D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_STENCILENABLE,D3DRS_ALPHABLENDENABLE,
        D3DRS_ALPHATESTENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_FOGENABLE,D3DRS_LIGHTING,
        D3DRS_CLIPPLANEENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_MULTISAMPLEANTIALIAS})device->SetRenderState(state,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
    for(unsigned sampler=0;sampler<2;++sampler) {
        device->SetSamplerState(sampler,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
        device->SetSamplerState(sampler,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
        device->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        device->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        device->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
        device->SetSamplerState(sampler,D3DSAMP_SRGBTEXTURE,FALSE);
    }
}
HRESULT Draw(IDirect3DDevice9* device,IDirect3DTexture9* source,IDirect3DPixelShader9* shader,
             const D3DVIEWPORT9& viewport,unsigned width,unsigned height) {
    struct Vertex{float x,y,z,w,u,v;};
    const float l=float(viewport.X),t=float(viewport.Y),r=l+viewport.Width,b=t+viewport.Height;
    const Vertex quad[]={{l-.5f,t-.5f,0,1,l/width,t/height},{r-.5f,t-.5f,0,1,r/width,t/height},
        {l-.5f,b-.5f,0,1,l/width,b/height},{r-.5f,b-.5f,0,1,r/width,b/height}};
    device->SetViewport(&viewport);device->SetTexture(0,source);device->SetPixelShader(shader);
    if(!shader) {
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
        device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
        device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
        device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
        device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    }
    return device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(Vertex));
}
}

void Initialize(MMDApp* app) {
    if(build_profile::original || !app || app->antialiasing.initialized || !app->Renderer())return;
    auto& s=app->antialiasing;const auto& pp=app->Renderer()->presentParameters;
    s.initialized=true;s.originalSamples=unsigned(pp.MultiSampleType);s.originalQuality=pp.MultiSampleQuality;
    const auto path=StorePath();
    unsigned algorithm=GetPrivateProfileIntW(L"Antialias",L"Algorithm",0,path.c_str());
    s.enabled=GetPrivateProfileIntW(L"Antialias",L"Enabled",1,path.c_str())!=0;
    // Startup keeps the existing device configuration. Explicit sample counts
    // are applied lazily by the first frame, after UI/device creation finishes.
    if(algorithm<unsigned(Algorithm::Count) && Supported(app,Algorithm(algorithm)))s.algorithm=Algorithm(algorithm);
    RefreshMenu(app);ApplyRenderState(app);
}
bool Supported(MMDApp* app,Algorithm algorithm) {
    if(!app || !app->Renderer() || !app->Renderer()->device || algorithm>=Algorithm::Count)return false;
    auto* r=app->Renderer();
    if(algorithm==Algorithm::Fxaa || algorithm==Algorithm::Taa) {
        D3DCAPS9 caps{};return SUCCEEDED(r->device->GetDeviceCaps(&caps)) && caps.PixelShaderVersion>=D3DPS_VERSION(3,0);
    }
    unsigned samples=Samples(app,algorithm);if(!samples)return false;
    D3DDEVICE_CREATION_PARAMETERS created{};
    if(FAILED(r->device->GetCreationParameters(&created)))return false;
    const auto& pp=r->presentParameters;
    DWORD color=0,depth=0;
    return SUCCEEDED(r->d3d9->CheckDeviceMultiSampleType(created.AdapterOrdinal,created.DeviceType,
        pp.BackBufferFormat,pp.Windowed,D3DMULTISAMPLE_TYPE(samples),&color)) && color &&
        SUCCEEDED(r->d3d9->CheckDeviceMultiSampleType(created.AdapterOrdinal,created.DeviceType,
        pp.AutoDepthStencilFormat,pp.Windowed,D3DMULTISAMPLE_TYPE(samples),&depth)) && depth;
}
void ApplyRenderState(MMDApp* app) {
    if(build_profile::original || !app || !app->Renderer() || !app->Renderer()->device)return;
    app->Renderer()->device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,
        app->antialiasing.enabled && !PostProcess(app->antialiasing));
}
void Release(MMDApp* app){targets.erase(app);}
bool Initializing(MMDApp* app) {
    if(build_profile::original || !app || !PostProcess(app->antialiasing))return false;
    const auto found=targets.find(app);
    return found==targets.end() || (app->antialiasing.algorithm==Algorithm::Taa && !found->second.history[0]);
}
unsigned AccumulatedSamples(MMDApp* app) {
    const auto found=targets.find(app);return found==targets.end()?0:found->second.samples;
}
bool Select(MMDApp* app,Algorithm algorithm,std::wstring& error) {
    Initialize(app);error.clear();
    if(algorithm>=Algorithm::Count || (app->antialiasing.enabled && !Supported(app,algorithm))) {
        error=app->EnglishUI()==2?L"当前设备不支持此抗锯齿算法。":L"This anti-aliasing mode is not supported by the device.";return false;
    }
    const auto before=app->antialiasing;
    app->antialiasing.algorithm=algorithm;
    auto* r=app->Renderer();const auto previous=r->presentParameters;
    const unsigned samples=Samples(app,algorithm);
    const unsigned quality=algorithm==Algorithm::MmdOriginal?before.originalQuality:0;
    bool ok=true;
    if(algorithm<=Algorithm::Msaa8 && (samples!=unsigned(previous.MultiSampleType) || quality!=previous.MultiSampleQuality)) {
        r->presentParameters.MultiSampleType=D3DMULTISAMPLE_TYPE(samples);r->presentParameters.MultiSampleQuality=quality;
        ok=PostDeviceReset(app);
    }
    if(ok && PostProcess(app->antialiasing))ok=SUCCEEDED(Ensure(app));
    if(!ok) {
        app->antialiasing=before;r->presentParameters=previous;Release(app);PostDeviceReset(app);
        error=app->EnglishUI()==2?L"无法启用此抗锯齿算法，已恢复之前的设置。":L"Unable to enable anti-aliasing. The previous settings were restored.";
    } else {
        if(!PostProcess(app->antialiasing))Release(app);
        else if(auto found=targets.find(app);found!=targets.end())found->second.samples=0;
        Save(app);
    }
    ApplyRenderState(app);RefreshMenu(app);PostViewRefresh(app);return ok;
}
bool Toggle(MMDApp* app,std::wstring& error) {
    Initialize(app);const bool was=app->antialiasing.enabled;
    app->antialiasing.enabled=!was;
    if(!Select(app,app->antialiasing.algorithm,error)) {
        app->antialiasing.enabled=was;ApplyRenderState(app);RefreshMenu(app);return false;
    }
    return true;
}
void PrepareFrame(MMDApp* app) {
    if(build_profile::original)return;
    Initialize(app);
    // A completed worker cannot post to a potentially destroyed window. Keep
    // polling on the UI thread until the requested frame resources exist.
    if(Initializing(app))app->state.messageSeen=1;
    const auto& s=app->antialiasing;const auto& pp=app->Renderer()->presentParameters;
    if(s.algorithm<=Algorithm::Msaa8 && (Samples(app,s.algorithm)!=unsigned(pp.MultiSampleType) ||
        (s.algorithm==Algorithm::MmdOriginal?s.originalQuality:0)!=pp.MultiSampleQuality)) {
        std::wstring error;
        if(!Select(app,s.algorithm,error))runtime_log::Write("AA restore preference failed");
    }
}

struct FrameScope::Impl {
    MMDApp* app;
    IDirect3DDevice9* device;
    ComPtr<IDirect3DSurface9> previous,depth;
    D3DVIEWPORT9 viewport{};
    D3DMATRIX projection{};
    IDirect3DSurface9* previousSlot=nullptr;
    IDirect3DSurface9* depthSlot=nullptr;
    bool ready=false,restored=false,temporal=false,bindingsChanged=false;
    explicit Impl(MMDApp* value):app(value),device(value->Renderer()->device) {}
    void Restore() {
        if(restored)return;restored=true;
        device->SetDepthStencilSurface(nullptr);device->SetRenderTarget(0,previous.Get());
        device->SetDepthStencilSurface(depth.Get());device->SetViewport(&viewport);
        device->SetTransform(D3DTS_PROJECTION,&projection);
        app->Renderer()->backbufferSurface=previousSlot;app->Renderer()->depthStencilSurface=depthSlot;
        ApplyRenderState(app);
    }
    ~Impl(){if(bindingsChanged)Restore();}
};
FrameScope::FrameScope(MMDApp* app) {
    if(build_profile::original)return;
    Initialize(app);ApplyRenderState(app);
    if(!PostProcess(app->antialiasing))return;
    impl_=std::make_unique<Impl>(app);auto& p=*impl_;
    const HRESULT allocation=Ensure(app);
    if(allocation==S_FALSE){impl_.reset();return;}
    if(FAILED(allocation)) {
        // A stored preference can outlive a compiler/GPU change. Keep drawing
        // the scene and uncheck AA instead of leaving a stale viewport forever.
        runtime_log::Write("AA unavailable hr=0x%08lX",allocation);
        impl_.reset();app->antialiasing.enabled=false;ApplyRenderState(app);RefreshMenu(app);
        const wchar_t* message=app->EnglishUI()==2?L"无法创建抗锯齿画面，已暂时关闭抗锯齿。":
            app->EnglishUI()==0?L"アンチエイリアスを初期化できないため、一時的に無効にしました。":
            L"Anti-aliasing could not be initialized and has been temporarily disabled.";
        MessageBoxW(app->MainWindow(),message,L"Anti-aliasing",MB_OK|MB_ICONERROR);
        return;
    }
    auto& t=targets.at(app);
    if(FAILED(p.device->GetRenderTarget(0,&p.previous)) || FAILED(p.device->GetDepthStencilSurface(&p.depth)) ||
       FAILED(p.device->GetViewport(&p.viewport)) || FAILED(p.device->GetTransform(D3DTS_PROJECTION,&p.projection)))return;
    p.previousSlot=app->Renderer()->backbufferSurface;p.depthSlot=app->Renderer()->depthStencilSurface;
    p.bindingsChanged=true;
    const auto signature=app->antialiasing.algorithm==Algorithm::Taa?Signature(app,p.device):0;
    p.temporal=app->antialiasing.algorithm==Algorithm::Taa && !app->PlaybackActive() && !app->RecordingWindow() &&
        signature==t.signature;
    if(!p.temporal)t.samples=0;
    t.signature=signature;
    if(p.temporal) {
        const unsigned sample=t.samples%8+1;
        const float dx=2*(Halton(sample,2)-.5f)/p.viewport.Width,dy=-2*(Halton(sample,3)-.5f)/p.viewport.Height;
        auto projection=p.projection;
        for(int i=0;i<4;++i){projection.m[i][0]+=dx*projection.m[i][3];projection.m[i][1]+=dy*projection.m[i][3];}
        p.device->SetTransform(D3DTS_PROJECTION,&projection);
    }
    p.device->SetDepthStencilSurface(nullptr);
    p.ready=SUCCEEDED(p.device->SetRenderTarget(0,t.color.Get())) &&
        SUCCEEDED(p.device->SetDepthStencilSurface(t.depth.Get())) && SUCCEEDED(p.device->SetViewport(&p.viewport));
    app->Renderer()->backbufferSurface=t.color.Get();app->Renderer()->depthStencilSurface=t.depth.Get();
}
FrameScope::~FrameScope()=default;
bool FrameScope::Ready()const{return !impl_ || impl_->ready;}
bool FrameScope::Complete() {
    if(!impl_ || !impl_->ready)return false;
    auto& p=*impl_;auto& t=targets.at(p.app);
    // Native ownership avoids invoking MME's frame callbacks a second time.
    ComPtr<IDirect3DDevice9> native;
    if(FAILED(t.color->GetDevice(&native))){p.Restore();return false;}
    auto* device=native.Get();ComPtr<IDirect3DStateBlock9> saved;
    if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&saved))){p.Restore();return false;}
    p.Restore();
    HRESULT hr=device->BeginScene();
    if(SUCCEEDED(hr)) {
        FullscreenState(device);
        const auto& v=p.viewport;const auto& desc=t.description;
        const float constants[]={1.f/desc.Width,1.f/desc.Height,p.temporal?float(std::min(t.samples,7u))/(std::min(t.samples,7u)+1):0,0,
            (v.X+.5f)/desc.Width,(v.Y+.5f)/desc.Height,(v.X+v.Width-.5f)/desc.Width,(v.Y+v.Height-.5f)/desc.Height};
        device->SetPixelShaderConstantF(0,constants,2);
        if(p.temporal) {
            const unsigned next=1-t.historyIndex;
            device->SetDepthStencilSurface(nullptr);device->SetTexture(1,t.history[t.historyIndex].Get());
            hr=device->SetRenderTarget(0,t.historySurface[next].Get());
            if(SUCCEEDED(hr))hr=Draw(device,t.scene.Get(),t.temporal.Get(),v,desc.Width,desc.Height);
            device->SetTexture(1,nullptr);device->SetRenderTarget(0,p.previous.Get());
            if(SUCCEEDED(hr))hr=Draw(device,t.history[next].Get(),nullptr,v,desc.Width,desc.Height);
            if(SUCCEEDED(hr)){t.historyIndex=next;++t.samples;}
        } else hr=Draw(device,t.scene.Get(),t.fxaa.Get(),v,desc.Width,desc.Height);
        device->EndScene();
    }
    saved->Apply();
    // State blocks exclude the target/depth bindings and may restore jitter.
    device->SetDepthStencilSurface(nullptr);device->SetRenderTarget(0,p.previous.Get());device->SetDepthStencilSurface(p.depth.Get());
    device->SetViewport(&p.viewport);device->SetTransform(D3DTS_PROJECTION,&p.projection);ApplyRenderState(p.app);
    if(FAILED(hr))runtime_log::Write("AA resolve failed hr=0x%08lX",hr);
    return SUCCEEDED(hr);
}
}
