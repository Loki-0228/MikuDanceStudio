// MAINTENANCE: regression-keep; hidden D3D camera attachment regression.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <cmath>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/d3dx_dyn.hpp"
#include "mikudancestudio/panel_controls.hpp"
#include "mikudancestudio/scene_ownership.hpp"
using namespace mikudancestudio;
namespace mikudancestudio { void SampleGlobalPlaybackTracks(MMDApp*); }
int main(int argc, char** argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    if(!d3dx::Get().Load())return 2;
    HWND window=CreateWindowW(L"STATIC",L"Camera follow regression",WS_POPUP,
        0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    auto* d3d=Direct3DCreate9(D3D_SDK_VERSION);
    if(!window||!d3d)return 2;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=pp.BackBufferHeight=256;pp.BackBufferFormat=D3DFMT_A8R8G8B8;
    IDirect3DDevice9* device=nullptr;
    if(FAILED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device)))return 2;
    auto app=std::make_unique<MMDApp>();app->state={};g_Block=app.get();
    auto renderer=std::make_unique<D3DRenderer>();renderer->device=device;renderer->viewScale=1.f;
    app->Renderer()=renderer.get();app->state.hwnd=window;app->CameraFov()=45.f;
    app->CameraPerspective()=0;app->CameraReferenceMode()=CameraAttachmentReference::None;
    if (argc > 1 && std::strcmp(argv[1], "--lighting-only") == 0) {
        int failures = 0;
        const auto check = [&](bool ok, const char* label) {
            std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); failures += !ok;
        };
        app->CameraParentModel() = -1;
        app->CameraDistance() = -45.f;
        app->CameraYaw() = .6f; app->CameraPitch() = -.2f;
        app->LightDirection()[0] = .3f; app->LightDirection()[1] = -.7f; app->LightDirection()[2] = -.9f;
        app->LightColor()[0] = .2f; app->LightColor()[1] = .4f; app->LightColor()[2] = .8f;
        app->ApplyTimelineLightState();
        app->CameraMode() = 1; SetupFrameWorldTransform(app.get());
        D3DLIGHT9 reference{}; device->GetLight(0, &reference);
        for (int follow : {1, 0, 1, 0}) {
            app->CameraMode() = 0; app->state.followCameraEnabled = follow;
            SetupFrameWorldTransform(app.get());
            D3DLIGHT9 light{}; device->GetLight(0, &light);
            check(std::memcmp(&reference.Direction, &light.Direction, sizeof(light.Direction)) == 0,
                  follow ? "tracked view keeps scene light direction" : "free view keeps scene light direction");
            check(std::memcmp(&reference.Ambient, &light.Ambient, sizeof(light.Ambient)) == 0,
                  "model view keeps scene brightness and colour");
        }
        mdl::LightKey lights[2]{};
        lights[0].next = 1; lights[1].frame = 30;
        lights[0].direction[0] = -.8f; lights[1].direction[0] = .8f;
        lights[0].direction[1] = lights[1].direction[1] = -.5f;
        lights[0].color[0] = .1f; lights[1].color[0] = .9f;
        mdl::SelfShadowKey shadows[2]{};
        shadows[0].next = 1; shadows[0].mode = 1; shadows[0].distance = .04f;
        shadows[1].frame = 30; shadows[1].mode = 2; shadows[1].distance = .08f;
        app->LightKeys() = lights; app->ShadowKeys() = shadows;
        app->LightTrackCursor() = 1; app->LightTrackActive() = 1;
        app->ShadowTrackCursor() = 1; app->ShadowTrackActive() = 1;
        const float yaw = app->CameraYaw(), distance = app->CameraDistance();
        app->PlaybackCursorSeconds() = .5f;
        SampleGlobalPlaybackTracks(app.get());
        check(std::fabs(app->LightDirection()[0]) < 1e-5f && std::fabs(app->LightColor()[0] - .5f) < 1e-5f,
              "free model playback interpolates scene light");
        app->PlaybackCursorSeconds() = 1.f;
        SampleGlobalPlaybackTracks(app.get());
        check(app->ShadowMode() == 2 && std::fabs(app->ShadowDistance() - .08f) < 1e-5f,
              "free model playback follows self-shadow keyframes");
        check(app->CameraYaw() == yaw && app->CameraDistance() == distance,
              "light animation leaves free camera alone");
        check(!IsWindowVisible(window), "lighting regression remains hidden");
        app->LightKeys() = nullptr; app->ShadowKeys() = nullptr;
        app->Renderer() = nullptr; g_Block = nullptr;
        device->Release(); d3d->Release(); DestroyWindow(window);
        return failures ? 1 : 0;
    }
    auto model=std::make_unique<mdl::ModelRecord>();*model={};
    mdl::BoneRecord bones[2]{};model->boneTable=bones;model->boneCount=2;
    app->ModelSlot(0)=reinterpret_cast<unsigned char*>(model.get());
    app->CameraParentModel()=0;app->CameraParentBone()=1;
    int failures=0;
    // Exercise the same selection-change dispatcher as the two UI combos.
    HWND parentCombo=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        0,0,100,100,window,reinterpret_cast<HMENU>(panel::kMainComboNormal),nullptr,nullptr);
    HWND boneCombo=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        0,0,100,100,window,reinterpret_cast<HMENU>(panel::kBoneRegisterCombo),nullptr,nullptr);
    SendMessageW(parentCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"None"));
    SendMessageW(parentCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Test model"));
    model->comboSelIndex=1;std::strcpy(bones[0].name,"root");std::strcpy(bones[1].name,"target");
    app->CameraMode()=1;app->CameraParentModel()=-1;app->CameraDistance()=-45.f;
    app->GlobalTrackSelected(GlobalTimelineTrack::Camera)=1;
    SendMessageW(parentCombo,CB_SETCURSEL,1,0);
    CommandDispatch(parentCombo,MAKEWPARAM(panel::kMainComboNormal,CBN_SELCHANGE));
    const bool bound=app->CameraParentModel()==0;
    const bool depth=app->CameraPositionZ()<-1.f;
    std::printf("UI model-binding=%s visible-depth=%s Z=%.3f\n",bound?"PASS":"FAIL",depth?"PASS":"FAIL",app->CameraPositionZ());
    failures+=!bound;failures+=!depth;
    SendMessageW(boneCombo,CB_SETCURSEL,1,0);
    CommandDispatch(boneCombo,MAKEWPARAM(panel::kBoneRegisterCombo,CBN_SELCHANGE));
    const bool chosen=app->CameraParentModel()==0&&app->CameraParentBone()==1;
    std::printf("UI bone-binding=%s\n",chosen?"PASS":"FAIL");failures+=!chosen;
    // Child invalidation depends on parent visibility. Keep this test-owned
    // window off screen and inactive while exercising the visible UI path.
    SetWindowPos(window,HWND_BOTTOM,-32000,-32000,256,256,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    RedrawWindow(window,nullptr,nullptr,RDW_VALIDATE|RDW_ALLCHILDREN);
    { ScopedSceneMutation mutation(*app); }
    const bool childRepaint=GetUpdateRect(parentCombo,nullptr,FALSE)!=0&&
        GetUpdateRect(boneCombo,nullptr,FALSE)!=0;
    std::printf("scene-mutation-child-repaint=%s\n",childRepaint?"PASS":"FAIL");failures+=!childRepaint;
    ShowWindow(window,SW_HIDE);
    auto closeEnough=[](float a,float b){return std::fabs(a-b)<1e-3f;};
    auto sample=[&](bool refresh=true){
        if(refresh)PostModelReload(app.get());SetupFrameWorldTransform(app.get());
        d3dx::D3DXMATRIXF world{},view{},composed{};
        device->GetTransform(D3DTS_WORLD,reinterpret_cast<D3DMATRIX*>(&world));
        device->GetTransform(D3DTS_VIEW,reinterpret_cast<D3DMATRIX*>(&view));
        d3dx::Get().multiply(&composed,&world,&view);
        const auto&bone=bones[1];
        const float x=bone.position[0]*bone.matInit[0]+bone.position[1]*bone.matInit[4]+bone.position[2]*bone.matInit[8]+bone.matInit[12];
        const float y=bone.position[0]*bone.matInit[1]+bone.position[1]*bone.matInit[5]+bone.position[2]*bone.matInit[9]+bone.matInit[13];
        const float z=bone.position[0]*bone.matInit[2]+bone.position[1]*bone.matInit[6]+bone.position[2]*bone.matInit[10]+bone.matInit[14];
        return std::array<float,3>{x*composed.m[0][0]+y*composed.m[1][0]+z*composed.m[2][0]+composed.m[3][0],
            x*composed.m[0][1]+y*composed.m[1][1]+z*composed.m[2][1]+composed.m[3][1],
            x*composed.m[0][2]+y*composed.m[1][2]+z*composed.m[2][2]+composed.m[3][2]};
    };
    for(int mode=0;mode<3;++mode){
        app->CameraMode()=mode==2?0:1;app->state.followCameraEnabled=mode==0?0:1;
        std::memset(bones,0,sizeof bones);
        for(auto&bone:bones)bone.matInit[0]=bone.matInit[5]=bone.matInit[10]=bone.matInit[15]=1.f;
        bones[1].position[0]=5.f;bones[1].position[1]=7.f;bones[1].position[2]=11.f;
        app->CameraPositionX()=2.f;app->CameraPositionY()=3.f;app->CameraPositionZ()=-30.f;
        app->ViewOffsetX()=app->ViewOffsetY()=0;app->CameraDistance()=-45.f;
        const auto before=sample();
        bones[1].matInit[12]=4.f;bones[1].matInit[13]=-2.f;bones[1].matInit[14]=3.f;
        const auto moved=sample();
        const bool follows=closeEnough(before[0],moved[0])&&closeEnough(before[1],moved[1])&&closeEnough(before[2],moved[2]);
        app->CameraPositionX()+=3.f;const auto offset=sample(false);
        const bool editable=closeEnough(offset[0],moved[0]-3.f)&&closeEnough(offset[1],moved[1])&&closeEnough(offset[2],moved[2]);
        std::printf("mode=%d follow=%s offset=%s before=(%.3f,%.3f,%.3f) moved=(%.3f,%.3f,%.3f) offset=(%.3f,%.3f,%.3f)\n",
            mode,follows?"PASS":"FAIL",editable?"PASS":"FAIL",before[0],before[1],before[2],moved[0],moved[1],moved[2],offset[0],offset[1],offset[2]);
        failures+=!follows;failures+=!editable;
        d3dx::D3DXMATRIXF rotated{};d3dx::Get().rotY(&rotated,.7f);
        rotated.m[3][0]=4.f;rotated.m[3][1]=-2.f;rotated.m[3][2]=3.f;
        std::memcpy(bones[1].matInit,&rotated,sizeof rotated);
        const auto turned=sample(false);
        const bool rotation=closeEnough(turned[0],offset[0])&&closeEnough(turned[1],offset[1])&&closeEnough(turned[2],offset[2]);
        const bool retained=app->CameraPositionX()==5.f&&app->CameraPositionY()==3.f&&app->CameraPositionZ()==-30.f;
        std::printf("mode=%d rotated-parent=%s persistent-offset=%s\n",mode,rotation?"PASS":"FAIL",retained?"PASS":"FAIL");
        failures+=!rotation;failures+=!retained;
    }
    // Returning to free model view keeps a retained binding without following it.
    app->CameraMode()=0;app->state.followCameraEnabled=0;
    SetupFrameWorldTransform(app.get());D3DMATRIX freeBefore{},freeAfter{};
    device->GetTransform(D3DTS_WORLD,&freeBefore);
    bones[1].matInit[12]+=20.f;SetupFrameWorldTransform(app.get());
    device->GetTransform(D3DTS_WORLD,&freeAfter);
    const bool freeView=std::memcmp(&freeBefore,&freeAfter,sizeof freeBefore)==0;
    std::printf("free-model-view=%s\n",freeView?"PASS":"FAIL");failures+=!freeView;
    app->ModelSlot(0)=nullptr;app->Renderer()=nullptr;g_Block=nullptr;
    device->Release();d3d->Release();DestroyWindow(window);
    return failures?1:0;
}
