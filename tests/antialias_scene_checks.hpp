// Kept in the real-MME host so every capture uses the production scene path.
#include "antialias_temporal_checks.hpp"
#include "antialias_stability_checks.hpp"
using AntialiasCheckFormat=HRESULT(WINAPI*)(IDirect3D9*,UINT,D3DDEVTYPE,D3DFORMAT,DWORD,D3DRESOURCETYPE,D3DFORMAT);
AntialiasCheckFormat originalAntialiasCheckFormat=nullptr;
unsigned rejectedAntialiasFloatQueries=0;
HRESULT WINAPI RejectAntialiasFloatTarget(IDirect3D9* d3d,UINT adapter,D3DDEVTYPE type,D3DFORMAT adapterFormat,
    DWORD usage,D3DRESOURCETYPE resource,D3DFORMAT format) {
    if(format==D3DFMT_A32B32G32R32F && (usage&D3DUSAGE_RENDERTARGET)) {
        ++rejectedAntialiasFloatQueries;return D3DERR_NOTAVAILABLE;
    }
    return originalAntialiasCheckFormat(d3d,adapter,type,adapterFormat,usage,resource,format);
}
void CheckAntialiasScene(MMDApp* app, const wchar_t* directory) {
    if(std::getenv("MMS_AA_STABILITY")) { CheckAntialiasStability(app,directory);return; }
    CheckTemporalResolveShader();
    captureDirectory = directory;
    std::filesystem::create_directories(captureDirectory);
    auto* r = app->Renderer();
    // Freeze this saved pose so algorithm comparisons do not include hair
    // physics evolving while the test traverses the menu.
    app->PlaybackPhysicsMode()=0;
    std::printf("PMM camera=%g,%g,%g distance=%g rotation=%g,%g,%g render=%dx%d viewport=%ld,%ld,%ld,%ld\n",
        app->CameraPositionX(), app->CameraPositionY(), app->CameraPositionZ(), app->CameraDistance(),
        app->CameraPitch(), app->CameraYaw(), app->CameraRoll(), app->RenderWidth(), app->RenderHeight(),
        app->ViewportRect().left, app->ViewportRect().top, app->ViewportRect().right, app->ViewportRect().bottom);
    auto** vtable = *reinterpret_cast<void***>(r->device);
    Check(MH_CreateHook(vtable[17], &ObservePresent, reinterpret_cast<void**>(&originalPresent)) == MH_OK &&
        MH_EnableHook(vtable[17]) == MH_OK, "capture saved PMM view");
    const auto capture=[&](const char* name,int frames=4) {
        const auto until=GetTickCount64()+10000;
        const bool wasEnabled=app->antialiasing.enabled;
        while(antialias::Initializing(app) && GetTickCount64()<until) {
            app->DeltaTime()=1.f/30;app->state.messageSeen=10;FrameDriver(app);Sleep(1);
        }
        Check(!antialias::Initializing(app) && app->antialiasing.enabled==wasEnabled,
            "requested AA is ready before comparison capture");
        const auto frameStart=GetTickCount64();
        for (int frame=0;frame<frames;++frame) {
            captureRequested=frame==frames-1;
            app->DeltaTime()=1.f/30;app->state.messageSeen=10;FrameDriver(app);
        }
        const auto frameMs=GetTickCount64()-frameStart;
        Check(!captureRequested && !capturedPixels.empty(), "saved PMM pixels captured");
        std::ofstream file(captureDirectory/(std::string(name)+".bgra"),std::ios::binary);
        file.write(reinterpret_cast<const char*>(capturedPixels.data()),capturedPixels.size()*4);
        std::ofstream size(captureDirectory/(std::string(name)+".txt"));size<<captureWidth<<" "<<captureHeight;
        std::printf("AA CAPTURE %s mode=%u enabled=%u samples=%u frames=%d ms=%llu\n",name,unsigned(app->antialiasing.algorithm),
            app->antialiasing.enabled,unsigned(r->presentParameters.MultiSampleType),frames,frameMs);
    };
    const auto select=[&](antialias::Algorithm mode) {
        SendMessageW(app->MainWindow(),WM_COMMAND,antialias::kFirstCommand+unsigned(mode),0);
        Check(app->antialiasing.algorithm==mode,"menu selects actual algorithm");
    };
    select(antialias::Algorithm::MmdOriginal);
    if(!app->antialiasing.enabled)CommandDispatch(nullptr,MAKEWPARAM(277,0));
    capture("original-aa");
    CommandDispatch(nullptr,MAKEWPARAM(277,0));capture("disabled");
    const auto disabled=capturedPixels;
    select(antialias::Algorithm::Fxaa);
    Check(!app->antialiasing.enabled,"algorithm selection preserves disabled state");
    CommandDispatch(nullptr,MAKEWPARAM(277,0));capture("fxaa");
    Check(disabled!=capturedPixels,"FXAA changes actual rendered edge pixels");
    auto* formatQuery=(*reinterpret_cast<void***>(r->d3d9))[10];
    Check(MH_CreateHook(formatQuery,&RejectAntialiasFloatTarget,reinterpret_cast<void**>(&originalAntialiasCheckFormat))==MH_OK &&
        MH_EnableHook(formatQuery)==MH_OK,"simulate unavailable standard D3D9 floating-point target");
    std::wstring capabilityError;
    Check(!antialias::Supported(app,antialias::Algorithm::Taa) &&
        !antialias::Select(app,antialias::Algorithm::Taa,capabilityError) && !capabilityError.empty(),
        "TAA rejects a device without floating-point render targets");
    Check(!antialias::Supported(app,antialias::Algorithm::SmaaT2xStandard) &&
        !antialias::Supported(app,antialias::Algorithm::SmaaT2xEnhanced) &&
        antialias::Supported(app,antialias::Algorithm::SmaaStandard) && antialias::Supported(app,antialias::Algorithm::Smaa),
        "SMAA T2x requires standard motion targets while both 1x presets remain available");
    Check(rejectedAntialiasFloatQueries>0 && app->antialiasing.algorithm==antialias::Algorithm::Fxaa && app->antialiasing.enabled,
        "unsupported temporal hardware preserves the working spatial AA mode");
    MH_DisableHook(formatQuery);MH_RemoveHook(formatQuery);
    Check(PostDeviceReset(app),"reset FXAA device");capture("fxaa-reset");
    select(antialias::Algorithm::Taa);capture("taa",18);
    std::printf("TAA samples=%u\n",antialias::AccumulatedSamples(app));
    Check(antialias::AccumulatedSamples(app)>=8,"TAA accumulates real jittered static frames");
    app->CameraYaw()+=.02f;capture("taa-camera-moved",1);
    Check(antialias::AccumulatedSamples(app)>=9,"camera motion reprojects temporal history instead of disabling TAA");
    const auto movingStart=antialias::AccumulatedSamples(app);
    for(int frame=0;frame<12;++frame) {
        app->CameraYaw()+=.004f;capture("taa-camera-playing",1);
    }
    Check(antialias::AccumulatedSamples(app)>=movingStart+12,"continuous camera motion retains temporal samples");
    const auto cameraMotion=antialias::motion::GetStats(app);
    std::printf("TAA camera draws=%u matched=%u motion_px=%g captures=%u cache_hits=%u\n",cameraMotion.draws,cameraMotion.matchedDraws,
        cameraMotion.maxMotionPixels,cameraMotion.vertexCaptures,cameraMotion.vertexCacheHits);
    Check(cameraMotion.matchedDraws>0 && cameraMotion.maxMotionPixels>.01f,
        "camera motion produces real geometry velocity for history reprojection");
    app->CameraYaw()-=.068f;
    const auto* record=mdl::Mdl(app->SelectedModel());
    auto* bones=mdl::Bones(app->SelectedModel());
    Check(bones && record->boneCount,"animated TAA fixture has a skinned bone");
    std::vector<float> savedBones(std::size_t(record->boneCount)*16);
    for(unsigned bone=0;bone<record->boneCount;++bone)
        std::memcpy(savedBones.data()+std::size_t(bone)*16,bones[bone].matInit,16*sizeof(float));
    const auto savedFrame=app->CurrentFrame();const auto savedPlayback=app->PlaybackActive();
    const auto playbackStart=antialias::AccumulatedSamples(app);
    const auto beforePlayback=capturedPixels;
    app->PlaybackActive()=1;
    for(int frame=0;frame<12;++frame) {
        // Exercise the production playback rendering branch with deterministic
        // bone deformation; wall-clock animation updates cannot hide a reset.
        for(unsigned bone=0;bone<record->boneCount;++bone) {
            std::memcpy(bones[bone].matInit,savedBones.data()+std::size_t(bone)*16,16*sizeof(float));
            bones[bone].matInit[12]+=(frame+1)*.02f*(1+bone%3);
        }
        app->CurrentFrame()=savedFrame+frame+1;
        app->state.messageSeen=10;RenderFrameScene(app);
        captureRequested=true;r->device->Present(nullptr,nullptr,nullptr,nullptr);
        Check(!captureRequested,"animated production frame reaches presentation");
    }
    Check(antialias::AccumulatedSamples(app)>=playbackStart+12,
        "playback with changing skinned positions keeps temporal accumulation active");
    const auto boneMotion=antialias::motion::GetStats(app);
    std::printf("TAA deformed draws=%u matched=%u motion_px=%g captures=%u cache_hits=%u\n",boneMotion.draws,boneMotion.matchedDraws,
        boneMotion.maxMotionPixels,boneMotion.vertexCaptures,boneMotion.vertexCacheHits);
    Check(boneMotion.matchedDraws>0 && boneMotion.maxMotionPixels>.01f,
        "deformed skinned vertices produce real motion vectors during playback");
    Check(capturedPixels!=beforePlayback,"animated TAA test changes actual rendered pixels");
    for(unsigned bone=0;bone<record->boneCount;++bone)
        std::memcpy(bones[bone].matInit,savedBones.data()+std::size_t(bone)*16,16*sizeof(float));
    app->CurrentFrame()=savedFrame;app->PlaybackActive()=savedPlayback;
    select(antialias::Algorithm::Msaa2);capture("msaa2");
    Check(r->presentParameters.MultiSampleType==D3DMULTISAMPLE_2_SAMPLES,"MSAA 2 uses 2 samples");
    select(antialias::Algorithm::Msaa4);capture("msaa4");
    Check(r->presentParameters.MultiSampleType==D3DMULTISAMPLE_4_SAMPLES,"MSAA 4 uses 4 samples");
    select(antialias::Algorithm::Msaa8);capture("msaa8");
    select(antialias::Algorithm::Smaa);capture("smaa");
    Check(disabled!=capturedPixels,"SMAA changes actual rendered edge pixels");
    Check(PostDeviceReset(app),"reset SMAA device");capture("smaa-reset");
    std::wstring smaaError;
    Check(viewport_render_scale::Configure(app,false,150,smaaError),"combine SMAA with viewport scaling");capture("smaa-scale150");
    Check(viewport_render_scale::Configure(app,true,100,smaaError),"restore viewport scale after SMAA");
    select(antialias::Algorithm::SmaaStandard);capture("smaa-standard");
    for(const auto mode:{antialias::Algorithm::SmaaT2xStandard,antialias::Algorithm::SmaaT2xEnhanced}) {
        select(mode);
        const auto before=app->CurrentFrame();app->CurrentFrame()=0;
        capture(mode==antialias::Algorithm::SmaaT2xStandard?"smaa-t2x-standard":"smaa-t2x-enhanced",2);
        Check(app->CurrentFrame()==0 && antialias::RequiredSamples(app)==2 && antialias::AccumulatedSamples(app)>=2,
            "SMAA T2x accumulates both actual submissions in a static frame-zero scene");
        app->CameraYaw()+=.004f;capture("smaa-t2x-camera",1);
        const auto moving=antialias::motion::GetStats(app);
        std::printf("SMAA T2x mode=%u draws=%u matched=%u motion_px=%g captures=%u cache_hits=%u\n",
            unsigned(mode),moving.draws,moving.matchedDraws,moving.maxMotionPixels,moving.vertexCaptures,moving.vertexCacheHits);
        Check(moving.matchedDraws>0 && moving.maxMotionPixels>.01f && antialias::AccumulatedSamples(app)>=3,
            "SMAA T2x retains both phases while reprojecting real camera motion");
        app->CameraYaw()-=.004f;app->CurrentFrame()=before;
    }
    select(antialias::Algorithm::Fxaa);
    Check(app->antialiasing.lastSmaaAlgorithm==antialias::Algorithm::SmaaT2xEnhanced,
        "switching to FXAA preserves the last selected SMAA variant");
    SendMessageW(app->MainWindow(),WM_COMMAND,antialias::kSmaaCommand,0);
    Check(app->antialiasing.algorithm==antialias::Algorithm::SmaaT2xEnhanced,
        "direct SMAA parent command restores the last selected variant");
    select(antialias::Algorithm::Fxaa);
    CommandDispatch(nullptr,MAKEWPARAM(277,0));capture("fxaa-disabled");
    Check(app->antialiasing.algorithm==antialias::Algorithm::Fxaa && !app->antialiasing.enabled,
        "main item toggles without forgetting algorithm");
    CommandDispatch(nullptr,MAKEWPARAM(277,0));capture("fxaa-reenabled");
    std::wstring error;
    Check(viewport_render_scale::Configure(app,false,150,error),"combine AA with viewport scaling");capture("fxaa-scale150");
    Check(viewport_render_scale::Configure(app,true,100,error),"restore viewport scale");
    // Exercise the production recording branch without starting a codec or
    // touching a user output file. It ends before editor overlays are drawn.
    app->RecordingWindow()=CreateWindowW(L"STATIC",L"Hidden AA export",WS_POPUP,0,0,640,480,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(app->RecordingWindow()!=nullptr,"hidden recording surface created");
    Check(EnsureCaptureRenderTarget(app),"initialize the production recording target before AA begins");
    Microsoft::WRL::ComPtr<IDirect3DSurface9> recordingTarget;
    r->device->GetRenderTarget(0,&recordingTarget);
    app->state.messageSeen=10;RenderFrameScene(app);
    captureRequested=true;r->device->Present(nullptr,nullptr,nullptr,nullptr);
    Check(!captureRequested,"FXAA completes recording frame before presentation");
    Microsoft::WRL::ComPtr<IDirect3DSurface9> restored;
    r->device->GetRenderTarget(0,&restored);
    Check(restored.Get()==recordingTarget.Get(),"recording restores production target ownership");
    restored.Reset();recordingTarget.Reset();
    select(antialias::Algorithm::Smaa);
    Check(EnsureCaptureRenderTarget(app),"restore capture target after the SMAA device reset");
    r->device->GetRenderTarget(0,&recordingTarget);
    app->state.messageSeen=10;RenderFrameScene(app);
    captureRequested=true;r->device->Present(nullptr,nullptr,nullptr,nullptr);
    Check(!captureRequested,"SMAA completes recording frame before presentation");
    restored.Reset();r->device->GetRenderTarget(0,&restored);
    Check(restored.Get()==recordingTarget.Get(),"SMAA recording restores production target ownership");
    restored.Reset();recordingTarget.Reset();
    select(antialias::Algorithm::Taa);
    const auto recordingFrame=app->CurrentFrame();
    for(int frame=0;frame<10;++frame) {
        app->CurrentFrame()=recordingFrame+frame;
        app->state.messageSeen=10;RenderFrameScene(app);
    }
    Check(antialias::AccumulatedSamples(app)>=8,"recording continues temporal accumulation across changing frames");
    app->CurrentFrame()=recordingFrame;
    DestroyWindow(app->RecordingWindow());app->RecordingWindow()=nullptr;
    select(antialias::Algorithm::MmdOriginal);
    select(antialias::Algorithm::Fxaa);
    for(int language=0;language<3;++language) {
        app->EnglishUI()=language;RefreshMenuLanguage(app);
        SendMessageW(app->MainWindow(),WM_ENTERMENULOOP,0,0);
        HMENU bar=GetMenu(app->MainWindow()),parent=nullptr,popup=nullptr;int index=-1;
        for(int i=0;i<GetMenuItemCount(bar);++i) {
            auto child=GetSubMenu(bar,i);
            for(int j=0;j<GetMenuItemCount(child);++j) {
                MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_ID|MIIM_SUBMENU|MIIM_STATE;
                GetMenuItemInfoW(child,j,TRUE,&item);
                if(item.wID==277){parent=child;popup=item.hSubMenu;index=j;}
            }
        }
        Check(popup && GetMenuItemCount(popup)==4,"algorithm flyout has original MSAA, FXAA, TAA and grouped SMAA");
        const int smaaPosition=GetMenuItemCount(popup)-1;
        auto smaaPopup=GetSubMenu(popup,smaaPosition);
        Check(smaaPopup && GetMenuItemCount(smaaPopup)==4,"four SMAA variants are available in the right-hand submenu");
        Check((GetMenuState(smaaPopup,antialias::kFirstCommand+unsigned(app->antialiasing.lastSmaaAlgorithm),MF_BYCOMMAND)&MF_CHECKED)!=0,
            "SMAA submenu marks the remembered variant even while another algorithm is active");
        Check((GetMenuState(popup,antialias::kFirstCommand+unsigned(antialias::Algorithm::Fxaa),MF_BYCOMMAND)&MF_CHECKED)!=0,
            "selected algorithm has radio check");
        SendMessageW(app->MainWindow(),WM_MENUSELECT,MAKEWPARAM(smaaPosition,MF_POPUP),reinterpret_cast<LPARAM>(popup));
        MSG smaaEnter{};smaaEnter.message=WM_KEYDOWN;smaaEnter.wParam=VK_RETURN;
        Check(antialias::MenuFilter(MSGF_MENU,0,reinterpret_cast<LPARAM>(&smaaEnter))==1,
            "Enter on the SMAA label applies the remembered variant");
        MSG smaaPending{};
        Check(PeekMessageW(&smaaPending,app->MainWindow(),WM_COMMAND,WM_COMMAND,PM_REMOVE) && LOWORD(smaaPending.wParam)==antialias::kSmaaCommand,
            "SMAA split menu posts its direct selection command");
        SendMessageW(app->MainWindow(),WM_MENUSELECT,MAKEWPARAM(index,MF_POPUP),reinterpret_cast<LPARAM>(parent));
        MSG enter{};enter.message=WM_KEYDOWN;enter.wParam=VK_RETURN;
        Check(antialias::MenuFilter(MSGF_MENU,0,reinterpret_cast<LPARAM>(&enter))==1,"Enter on parent keeps toggle action");
        MSG pending{};
        Check(PeekMessageW(&pending,app->MainWindow(),WM_COMMAND,WM_COMMAND,PM_REMOVE) && LOWORD(pending.wParam)==277,
            "split menu posts original AA toggle command");
        SendMessageW(app->MainWindow(),WM_EXITMENULOOP,0,0);
        Check(GetSubMenu(parent,index)==nullptr,"flyout detached on exit without changing original item");
        DispatchMessageW(&pending);Check(!app->antialiasing.enabled,"keyboard activates toggle");
        CommandDispatch(nullptr,MAKEWPARAM(277,0));
    }
    if(std::getenv("MMS_SURFACE_MISSING_DX12")) {
        const auto status=skinning::GetStatus();
        Check(blockedSurfaceDx12>0 && status.gpuUpdates==0 && status.cpuUpdates>0,
            "all AA algorithms work after actual missing-DX12 fallback");
    }
    MH_DisableHook(vtable[17]);
}
