#define NOMINMAX
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/globals.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/wic_mov_recorder.hpp"
#include "mikudancestudio/native_media.hpp"
#include "mikudancestudio/transparent_capture.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <MinHook.h>
using namespace mikudancestudio;
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
void Check(bool ok, const char* why) { if (!ok) { std::printf("FAIL %s\n", why); std::exit(1); } }
LRESULT CALLBACK Hidden(int code, WPARAM wp, LPARAM lp) {
    if (code == HCBT_CREATEWND) reinterpret_cast<CBT_CREATEWND*>(lp)->lpcs->style &= ~WS_VISIBLE;
    if (code == HCBT_ACTIVATE) { ShowWindow(reinterpret_cast<HWND>(wp), SW_HIDE); return 1; }
    return CallNextHookEx(nullptr, code, wp, lp);
}
int WINAPI UnexpectedA(HWND, LPCSTR text, LPCSTR, UINT) { std::printf("Unexpected dialog: %s\n", text); std::exit(2); }
int WINAPI UnexpectedW(HWND, LPCWSTR text, LPCWSTR, UINT) { std::fwprintf(stderr, L"Unexpected dialog: %s\n", text); std::exit(2); }
decltype(&ShowWindow) originalShowWindow = nullptr;
BOOL WINAPI KeepHidden(HWND window, int) { return originalShowWindow(window, SW_HIDE); }
template<class T> void Put(std::ofstream& f, T value) { f.write(reinterpret_cast<char*>(&value), sizeof(value)); }
void Text(std::ofstream& f, const wchar_t* text) { Put(f, std::uint32_t(wcslen(text) * 2)); f.write(reinterpret_cast<const char*>(text), wcslen(text) * 2); }
void Fixture(const fs::path& path) {
    std::ofstream f(path, std::ios::binary); f.write("PMX ", 4); Put(f, 2.f);
    for (std::uint8_t v : {8,0,0,1,1,1,1,1,1}) Put(f,v);
    Text(f,L"Alpha triangle"); Text(f,L"Alpha triangle"); Text(f,L""); Text(f,L""); Put(f,3u);
    const float xy[][2] = {{-5,5},{5,5},{0,15}};
    for(int i=0;i<3;++i) {
        Put(f,xy[i][0]); Put(f,xy[i][1]); Put(f,0.f);
        Put(f,0.f); Put(f,0.f); Put(f,-1.f); Put(f,0.f); Put(f,0.f);
        Put(f,std::uint8_t(0)); Put(f,std::int8_t(0)); Put(f,1.f);
    }
    Put(f,3u); Put(f,std::uint8_t(0)); Put(f,std::uint8_t(1)); Put(f,std::uint8_t(2)); Put(f,0u);
    Put(f,1u); Text(f,L"Half transparent"); Text(f,L"Half transparent");
    for(float v:{1.f,0.f,0.f,.5f,0.f,0.f,0.f,1.f,.4f,.4f,.4f}) Put(f,v);
    Put(f,std::uint8_t(1)); for(int i=0;i<5;++i) Put(f,0.f);
    Put(f,std::int8_t(-1)); Put(f,std::int8_t(-1)); Put(f,std::uint8_t(0)); Put(f,std::uint8_t(1)); Put(f,std::uint8_t(0)); Text(f,L""); Put(f,3u);
    Put(f,1u); Text(f,L"root"); Text(f,L"root"); for(int i=0;i<3;++i) Put(f,0.f);
    Put(f,std::int8_t(-1)); Put(f,0u); Put(f,std::uint16_t(0x1a)); for(int i=0;i<3;++i) Put(f,0.f);
    Put(f,0u); Put(f,1u); Text(f,L"Root"); Text(f,L"Root"); Put(f,std::uint8_t(1)); Put(f,1u); Put(f,std::uint8_t(0)); Put(f,std::int8_t(0));
    Put(f,0u); Put(f,0u); Check(bool(f),"write alpha PMX fixture");
}
std::vector<unsigned char> Read(IDirect3DDevice9* device, IDirect3DSurface9* source) {
    D3DSURFACE_DESC desc{}; Check(source && SUCCEEDED(source->GetDesc(&desc)), "source target");
    ComPtr<IDirect3DSurface9> resolve, system;
    Check(SUCCEEDED(device->CreateRenderTarget(desc.Width, desc.Height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &resolve, nullptr)), "resolve surface");
    Check(SUCCEEDED(device->StretchRect(source, nullptr, resolve.Get(), nullptr, D3DTEXF_NONE)), "resolve alpha target");
    Check(SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &system, nullptr)), "readback surface");
    Check(SUCCEEDED(device->GetRenderTargetData(resolve.Get(), system.Get())), "GPU alpha readback");
    D3DLOCKED_RECT locked{}; Check(SUCCEEDED(system->LockRect(&locked,nullptr,D3DLOCK_READONLY)), "lock target");
    std::vector<unsigned char> pixels(std::size_t(desc.Width)*desc.Height*4);
    for(UINT y=0;y<desc.Height;++y) std::memcpy(pixels.data()+std::size_t(y)*desc.Width*4,static_cast<unsigned char*>(locked.pBits)+std::size_t(y)*locked.Pitch,desc.Width*4);
    system->UnlockRect(); return pixels;
}
void Blocks(IDirect3DDevice9* device, IDirect3DSurface9* target) {
    ComPtr<IDirect3DSurface9> original; device->GetRenderTarget(0,&original);
    D3DVIEWPORT9 old{}; device->GetViewport(&old);
    Check(SUCCEEDED(device->SetRenderTarget(0,target)),"bind capture for pixel fixture");
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
    D3DVIEWPORT9 viewport{0,0,64,48,0,1}; device->SetViewport(&viewport);
    device->SetVertexShader(nullptr); device->SetPixelShader(nullptr); device->SetTexture(0,nullptr);
    device->SetRenderState(D3DRS_ZENABLE,FALSE); device->SetRenderState(D3DRS_STENCILENABLE,FALSE);
    device->SetRenderState(D3DRS_LIGHTING,FALSE); device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE); device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD); device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
    SetCoverageAlphaBlend(device);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1); device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1); device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    struct Vertex { float x,y,z,w; DWORD color; };
    Check(SUCCEEDED(device->BeginScene()),"begin pixel fixture");
    for(int y=0;y<2;++y) for(int x=0;x<2;++x) {
        const DWORD color = D3DCOLOR_ARGB(x ? 255 : 128,y ? 0 : 255,0,y ? 255 : 0);
        const float left=(x ? 40.f : 16.f)-.5f,right=(x ? 64.f : 40.f)-.5f,top=y*24.f-.5f,bottom=top+24;
        Vertex vertices[]={{left,top,0,1,color},{right,top,0,1,color},{left,bottom,0,1,color},{right,bottom,0,1,color}};
        Check(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex))),"draw alpha fixture");
    }
    device->EndScene(); device->SetRenderTarget(0,original.Get()); device->SetViewport(&old);
}
int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"fixture directory required"); std::setvbuf(stdout,nullptr,_IONBF,0);
    const fs::path directory=fs::absolute(argv[1]);
    const auto initialized=MH_Initialize(); Check(initialized==MH_OK || initialized==MH_ERROR_ALREADY_INITIALIZED,"dialog guard init");
    MH_CreateHook(reinterpret_cast<void*>(&MessageBoxA),reinterpret_cast<void*>(&UnexpectedA),nullptr);
    MH_CreateHook(reinterpret_cast<void*>(&MessageBoxW),reinterpret_cast<void*>(&UnexpectedW),nullptr);
    MH_EnableHook(reinterpret_cast<void*>(&MessageBoxA)); MH_EnableHook(reinterpret_cast<void*>(&MessageBoxW));
    Check(MH_CreateHook(reinterpret_cast<void*>(&ShowWindow), reinterpret_cast<void*>(&KeepHidden),
        reinterpret_cast<void**>(&originalShowWindow)) == MH_OK, "hide recording window guard");
    Check(MH_EnableHook(reinterpret_cast<void*>(&ShowWindow)) == MH_OK, "enable hidden recording guard");
    auto hide=SetWindowsHookExW(WH_CBT,Hidden,nullptr,GetCurrentThreadId());
    auto* app=new MMDApp; g_Block=app; app->state=MMDAppState{}; app->InitDefaults(); app->EnglishUI()=1;
    Check(InitMainWindowAndD3D(app,GetModuleHandleW(nullptr),SW_HIDE),"hidden production host");
    Check(!IsWindowVisible(app->MainWindow()),"host stays hidden");
    app->DeltaTime()=1.f/30; app->state.messageSeen=1; FrameDriver(app);
    wcscpy_s(app->AviBackgroundPath(),256,(directory/L"corners.mp4").c_str()); LoadAviFile(app);
    Check(app->AviBackgroundEnabled()==1 && media::HasVideo(app),"production MP4 import and texture refresh");
    auto* device=app->Renderer()->device;
    app->AviOutputWidth()=64; app->AviOutputHeight()=48;
    std::wstring error; const auto output=directory/L"render-alpha.mov";
    Check(StartPngMovRecording(app->Recorder(),output.c_str(),64,48,30,error),"start production recorder");
    app->RecordingWindow()=CreateWindowW(L"STATIC",L"Hidden capture",WS_POPUP,0,0,64,48,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    app->state.messageSeen=1; RenderFrameScene(app);
    auto clear=Read(device,TransparentCaptureSurface(app));
    for(std::size_t i=3;i<clear.size();i+=4) Check(clear[i]==0,"transparent scene excludes loaded background");
    const auto model=directory/L"alpha-triangle.pmx"; Fixture(model); LoadModelFile(app,model.c_str(),false);
    Check(app->ModelSlot(0)!=nullptr,"load semitransparent model");
    app->state.messageSeen=1; RenderFrameScene(app);
    const auto triangle=Read(device,TransparentCaptureSurface(app));
    std::size_t empty=0,half=0;
    for(std::size_t i=3;i<triangle.size();i+=4) { empty+=triangle[i]==0; half+=triangle[i]>=126 && triangle[i]<=129; }
    std::printf("Model alpha: empty=%zu half=%zu total=%zu\n",empty,half,triangle.size()/4);
    Check(empty>100 && half>100,"production model retains half opacity and clear background");
    for(int frame=0;frame<30;++frame) {
        Blocks(device,TransparentCaptureSurface(app));
        Check(RecordingReadbackPass(app),"production GPU readback and PNG submission");
    }
    Check(FinishPngMovRecording(app->Recorder(),30,error),"finalize rendered PNG MOV");
    ReleaseTransparentCapture(app);
    DestroyWindow(app->RecordingWindow()); app->RecordingWindow()=nullptr;
    for (int fps : {24, 30, 60}) {
        const auto recording = directory / (L"timeline-" + std::to_wstring(fps) + L".mov");
        wcscpy_s(app->AviOutputPath(), 256, recording.c_str());
        app->Recorder()->selectedCodec = kNativePngMovCodec;
        app->AviRecordStartFrame() = 0; app->AviRecordEndFrame() = 29;
        app->AviRecordFps() = static_cast<float>(fps);
        app->AviIncludeWave() = 0; app->AviCloseAfterExport() = 0;
        StartAviRecordWindow(app);
        Check(IsPngMovRecording(app->Recorder()), "start through production export entry");
        for (int i = 0; i < 180 && IsPngMovRecording(app->Recorder()); ++i) {
            app->state.messageSeen = 1; FrameDriver(app);
        }
        Check(!IsPngMovRecording(app->Recorder()) && fs::exists(recording), "timeline recording completes and publishes MOV");
        Check(app->RecordingWindow() == nullptr, "recording window released");
        std::printf("PASS production timeline export %d fps\n", fps);
    }
    Check(!IsWindowVisible(app->MainWindow()),"renderer remained hidden");
    ShutdownCleanup(app); g_Block=nullptr; delete app; UnhookWindowsHookEx(hide);
    std::puts("PASS hidden production video import, transparent background, PMX alpha, GPU readback and MOV output");
}
