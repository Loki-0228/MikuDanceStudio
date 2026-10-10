// Hidden production startup and recording. Never capture the desktop.
#define NOMINMAX
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/globals.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/wic_mov_recorder.hpp"
#include <MinHook.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace mikudancestudio;
namespace {
int failures = 0;
void Check(bool ok, const char* label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}
void Require(bool ok, const char* label) {
    Check(ok, label);
    if (!ok) std::exit(2);
}
int WINAPI UnexpectedA(HWND, LPCSTR text, LPCSTR, UINT) {
    std::printf("FAIL unexpected dialog: %s\n", text); std::exit(2);
}
int WINAPI UnexpectedW(HWND, LPCWSTR text, LPCWSTR, UINT) {
    std::fwprintf(stderr, L"FAIL unexpected dialog: %ls\n", text); std::exit(2);
}
decltype(&ShowWindow) originalShow = nullptr;
BOOL WINAPI KeepHidden(HWND window, int) { return originalShow(window, SW_HIDE); }
decltype(&GetMonitorInfoW) originalMonitorInfo = nullptr;
const RECT workArea{-960, 40, 0, 580};
bool fitWorkArea = false;
BOOL WINAPI MonitorInfo(HMONITOR monitor, LPMONITORINFO info) {
    const BOOL result = originalMonitorInfo(monitor, info);
    if (result && fitWorkArea) info->rcWork = workArea;
    return result;
}
using PresentFunction = HRESULT (WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
PresentFunction originalPresent = nullptr;
unsigned presents = 0;
bool fullSource = true, fitsClient = true;
HRESULT WINAPI ObservePresent(IDirect3DDevice9* device, const RECT* source,
                              const RECT* destination, HWND window, const RGNDATA* dirty) {
    if (g_Block && window && window == g_Block->RecordingWindow()) {
        ++presents;
        fullSource &= source && source->left == 0 && source->top == 0 &&
            source->right == 1920 && source->bottom == 1080;
        RECT client{};
        fitsClient &= GetClientRect(window, &client) &&
            (!destination || EqualRect(destination, &client));
    }
    return originalPresent(device, source, destination, window, dirty);
}
template<class Function> void Hook(Function target, Function detour, Function* original = nullptr) {
    Require(MH_CreateHook(reinterpret_cast<void*>(target), reinterpret_cast<void*>(detour),
        reinterpret_cast<void**>(original)) == MH_OK, "create test hook");
    Require(MH_EnableHook(reinterpret_cast<void*>(target)) == MH_OK, "enable test hook");
}
}

int main(int argc, char** argv) {
    Require(argc == 3 || argc == 4, "expected startup width and height arguments");
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetProcessDPIAware();
    Require(MH_Initialize() == MH_OK, "initialize hidden-window guards");
    Hook(&ShowWindow, &KeepHidden, &originalShow);
    Hook(&MessageBoxA, &UnexpectedA);
    Hook(&MessageBoxW, &UnexpectedW);
    auto* app = new MMDApp;
    g_Block = app;
    app->state = MMDAppState{};
    app->InitDefaults();
    Require(InitMainWindowAndD3D(app, GetModuleHandleW(nullptr), SW_HIDE), "initialize production host");
    Check(!IsWindowVisible(app->MainWindow()), "startup window stays hidden");
    std::printf("STARTUP %d %d\n", app->RenderWidth(), app->RenderHeight());
    Check(app->RenderWidth() == std::atoi(argv[1]) && app->RenderHeight() == std::atoi(argv[2]),
          "startup preserves saved dimensions or uses 1080p defaults");
    if (argc == 4) {
        const int savedWidth = app->RenderWidth(), savedHeight = app->RenderHeight();
        app->DeltaTime() = 1.f / 30;
        app->state.messageSeen = 1;
        FrameDriver(app);
        Hook(&GetMonitorInfoW, &MonitorInfo, &originalMonitorInfo);
        auto* device = app->Renderer()->device;
        auto** vtable = *reinterpret_cast<void***>(device);
        Hook(reinterpret_cast<PresentFunction>(vtable[17]), &ObservePresent, &originalPresent);
        const auto output = std::filesystem::current_path() / L"preview-1080p.mov";
        wcscpy_s(app->AviOutputPath(), 256, output.c_str());
        app->Recorder()->selectedCodec = kNativePngMovCodec;
        app->AviOutputWidth() = 1920; app->AviOutputHeight() = 1080;
        app->AviRecordStartFrame() = 0; app->AviRecordEndFrame() = 2;
        app->AviRecordFps() = 30;
        app->AviIncludeWave() = 0; app->AviCloseAfterExport() = 0;
        fitWorkArea = true;
        StartAviRecordWindow(app);
        fitWorkArea = false;
        Require(IsPngMovRecording(app->Recorder()), "start production MOV recording");
        const HWND preview = app->RecordingWindow();
        Check(!IsWindowVisible(preview), "recording preview stays hidden");
        RECT bounds{}, client{};
        GetWindowRect(preview, &bounds); GetClientRect(preview, &client);
        std::printf("PREVIEW %ld,%ld %ldx%ld client=%ldx%ld\n", bounds.left, bounds.top,
            bounds.right - bounds.left, bounds.bottom - bounds.top, client.right, client.bottom);
        Check(bounds.left >= workArea.left && bounds.top >= workArea.top &&
              bounds.right <= workArea.right && bounds.bottom <= workArea.bottom,
              "preview fits monitor work area with negative coordinates");
        Check(std::llabs(1080LL * client.right - 1920LL * client.bottom) <= 1920 + 1080,
              "preview preserves export aspect ratio within one pixel rounding");
        Check(app->RenderWidth() == 1920 && app->RenderHeight() == 1080,
              "capture keeps requested 1080p dimensions");
        for (int i = 0; i < 30 && IsPngMovRecording(app->Recorder()); ++i) {
            app->state.messageSeen = 1;
            FrameDriver(app);
        }
        Check(presents > 0 && fullSource, "present uses full recording source");
        Check(presents > 0 && fitsClient, "present scales to preview client area without cropping");
        Check(!IsPngMovRecording(app->Recorder()) && std::filesystem::exists(output),
              "recording completes and publishes MOV");
        Check(!app->RecordingWindow() && app->RenderWidth() == savedWidth && app->RenderHeight() == savedHeight,
              "recording restores existing render resolution");
    }
    Check(!IsWindowVisible(app->MainWindow()), "production host remains hidden");
    ShutdownCleanup(app);
    g_Block = nullptr;
    delete app;
    return failures ? 1 : 0;
}
