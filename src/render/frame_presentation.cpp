#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>

#include <algorithm>

#include "mikudancestudio/frame_presentation.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/ported_funcs.hpp"

namespace mikudancestudio {
namespace { thread_local bool nativeBakePreview=false; }
bool NativeBakePreviewActive(){return nativeBakePreview;}
bool BakePreview::Due(bool last){
    if(!app_||!app_->Renderer()||!app_->Renderer()->device)return false;
    const auto now=GetTickCount64();
    if(!last&&now<next_)return false;
    next_=now+66;return true;
}
void BakePreview::Present(){
    if(!app_||!app_->Renderer()||!app_->Renderer()->device)return;
    auto* device=app_->Renderer()->device;
    FillFramePresentationMargins(app_,device);
    const RECT rect=FramePresentationRect(app_);
    const HWND window=app_->FloatingWindow()?app_->FloatingWindow():app_->MainWindow();
    device->Present(&rect,&rect,window,nullptr);
}
void BakePreview::RenderNative(){
    struct Guard{
        MMDApp* app;bool old=nativeBakePreview;
        int pass,count;void* object;
        explicit Guard(MMDApp* a):app(a),pass(a->state.activeRenderPass),count(a->state.renderPassCount),object(a->state.activeRenderObject){}
        ~Guard(){nativeBakePreview=old;app->state.activeRenderPass=pass;app->state.renderPassCount=count;app->state.activeRenderObject=object;}
    } guard(app_);
    nativeBakePreview=true;RenderFrameScene(app_);Present();
}

RECT FramePresentationRect(MMDApp* app) {
    RECT result = app->ViewportRect();
    if (app->RecordingWindow() != nullptr) return result;
    if (app->FullscreenMode() != 0) {
        GetClientRect(app->MainWindow(), &result);
        return result;
    }

    const bool floating = app->FloatingWindow() != nullptr;
    const HWND window = floating ? app->FloatingWindow() : app->MainWindow();
    RECT client{};
    if (!GetClientRect(window, &client))
        return result;

    // Keep the GDI controls above/below the scene outside the present region.
    result.left = std::max<LONG>(0, floating ? 0 : app->SidebarWidth() + 9);
    result.right = std::max(result.left, client.right);
    return result;
}

void FillFramePresentationMargins(MMDApp* app, IDirect3DDevice9* device) {
    if (app->RecordingWindow() != nullptr) return;
    const RECT scene = app->ViewportRect();
    const RECT present = FramePresentationRect(app);
    if (EqualRect(&present, &scene))
        return;

    IDirect3DSurface9* backBuffer = nullptr;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
                                    &backBuffer)))
        return;
    D3DSURFACE_DESC desc{};
    if (SUCCEEDED(backBuffer->GetDesc(&desc))) {
        const RECT bounds{0, 0, static_cast<LONG>(desc.Width),
                         static_cast<LONG>(desc.Height)};
        const RECT margins[] = {
            {present.left, present.top, present.right, scene.top},
            {present.left, scene.bottom, present.right, present.bottom},
            {present.left, present.top, scene.left, present.bottom},
            {scene.right, present.top, present.right, present.bottom},
        };
        for (const RECT& margin : margins) {
            RECT clipped{};
            if (IntersectRect(&clipped, &margin, &bounds))
                device->ColorFill(backBuffer, &clipped, D3DCOLOR_XRGB(0, 0, 0));
        }
    }
    backBuffer->Release();
    // Do not use Clear or change the viewport here: MME owns the active
    // render target, and a zero-rectangle Clear clears the entire scene.
}

}  // namespace mikudancestudio
