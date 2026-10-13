// Kept in the real-MME host so every capture uses the production scene path.
void CheckAntialiasScene(MMDApp* app, const wchar_t* directory) {
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
        for (int frame=0;frame<frames;++frame) {
            captureRequested=frame==frames-1;
            app->DeltaTime()=1.f/30;app->state.messageSeen=10;FrameDriver(app);
        }
        Check(!captureRequested && !capturedPixels.empty(), "saved PMM pixels captured");
        std::ofstream file(captureDirectory/(std::string(name)+".bgra"),std::ios::binary);
        file.write(reinterpret_cast<const char*>(capturedPixels.data()),capturedPixels.size()*4);
        std::ofstream size(captureDirectory/(std::string(name)+".txt"));size<<captureWidth<<" "<<captureHeight;
        std::printf("AA CAPTURE %s mode=%u enabled=%u samples=%u\n",name,unsigned(app->antialiasing.algorithm),
            app->antialiasing.enabled,unsigned(r->presentParameters.MultiSampleType));
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
    Check(PostDeviceReset(app),"reset FXAA device");capture("fxaa-reset");
    select(antialias::Algorithm::Taa);capture("taa",18);
    std::printf("TAA samples=%u\n",antialias::AccumulatedSamples(app));
    Check(antialias::AccumulatedSamples(app)>=8,"TAA accumulates real jittered static frames");
    app->CameraYaw()+=.02f;capture("taa-camera-moved",1);
    Check(antialias::AccumulatedSamples(app)==0,"camera motion invalidates temporal history");app->CameraYaw()-=.02f;
    select(antialias::Algorithm::Msaa2);capture("msaa2");
    Check(r->presentParameters.MultiSampleType==D3DMULTISAMPLE_2_SAMPLES,"MSAA 2 uses 2 samples");
    select(antialias::Algorithm::Msaa4);capture("msaa4");
    Check(r->presentParameters.MultiSampleType==D3DMULTISAMPLE_4_SAMPLES,"MSAA 4 uses 4 samples");
    select(antialias::Algorithm::Msaa8);capture("msaa8");
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
    app->state.messageSeen=10;RenderFrameScene(app);
    captureRequested=true;r->device->Present(nullptr,nullptr,nullptr,nullptr);
    Check(!captureRequested,"FXAA completes recording frame before presentation");
    Microsoft::WRL::ComPtr<IDirect3DSurface9> restored;
    r->device->GetRenderTarget(0,&restored);
    Check(restored.Get()==r->backbufferSurface,"recording restores production target ownership");
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
        Check(popup && GetMenuItemCount(popup)==int(antialias::Algorithm::Count),"algorithm flyout attached in every language");
        Check((GetMenuState(popup,antialias::kFirstCommand+unsigned(antialias::Algorithm::Fxaa),MF_BYCOMMAND)&MF_CHECKED)!=0,
            "selected algorithm has radio check");
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
