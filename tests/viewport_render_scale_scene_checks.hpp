// Included in the hidden native/MME production scene harness.
unsigned scaledSceneDraws = 0;
unsigned hudDraws = 0;
decltype(&SetWindowPos) originalHudWindowPos=nullptr;
BOOL WINAPI HiddenHudWindowPos(HWND window,HWND after,int x,int y,int width,int height,UINT flags) {
    return originalHudWindowPos(window,after,x,y,width,height,(flags&~SWP_SHOWWINDOW)|SWP_NOACTIVATE);
}
using HudDraw = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
HudDraw originalHudDraw = nullptr;
HRESULT WINAPI ObserveHudDraw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type,
    UINT first, UINT primitives) {
    Microsoft::WRL::ComPtr<IDirect3DVertexBuffer9> buffer;
    UINT offset=0,stride=0;
    device->GetStreamSource(0,&buffer,&offset,&stride);
    if(buffer && (buffer.Get()==g_Block->OverlayVertices() ||
        buffer.Get()==g_Block->SpriteOverlayVertices())) {
        ++hudDraws;
        Check(!viewport_render_scale::Rendering(g_Block),"HUD draws after preview composite");
        Microsoft::WRL::ComPtr<IDirect3DSurface9> target,back;
        device->GetRenderTarget(0,&target);device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);
        Check(target.Get()==back.Get(),"HUD uses actual display backbuffer");
        const auto rect=g_Block->ViewportRect();D3DVIEWPORT9 viewport{};device->GetViewport(&viewport);
        Check(viewport.X==unsigned(rect.left) && viewport.Y==unsigned(rect.top) &&
            viewport.Width==unsigned(rect.right-rect.left) && viewport.Height==unsigned(rect.bottom-rect.top),
            "HUD viewport matches display coordinates");
        DWORD mask=0,aa=0,blend=0;
        device->GetRenderState(D3DRS_MULTISAMPLEMASK,&mask);
        device->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS,&aa);
        device->GetRenderState(D3DRS_BLENDOP,&blend);
        Check(mask==0xFFFFFFFFu && aa==TRUE && blend==D3DBLENDOP_ADD,
            "HUD explicitly uses all samples and normal alpha composition");
    }
    return originalHudDraw(device,type,first,primitives);
}
using ScaleDraw = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
ScaleDraw originalScaleDraw = nullptr;
HRESULT WINAPI ObserveScaleDraw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT base,
    UINT min, UINT vertices, UINT start, UINT primitives) {
    if (viewport_render_scale::Rendering(g_Block)) {
        Microsoft::WRL::ComPtr<IDirect3DSurface9> target;
        device->GetRenderTarget(0, &target);
        if (target.Get() == g_Block->Renderer()->backbufferSurface) {
            D3DSURFACE_DESC desc{}; target->GetDesc(&desc);
            D3DVIEWPORT9 viewport{}; device->GetViewport(&viewport);
            Check(desc.Width == viewport.Width && desc.Height == viewport.Height,
                "real scene draws fill the scaled target");
            ++scaledSceneDraws;
        }
    }
    return originalScaleDraw(device,type,base,min,vertices,start,primitives);
}
void CheckViewportRenderScale(MMDApp* app, const wchar_t* output) {
    captureDirectory=output; std::filesystem::create_directories(captureDirectory);
    auto** table=*reinterpret_cast<void***>(app->Renderer()->device);
    Check(MH_CreateHook(table[17],&ObservePresent,reinterpret_cast<void**>(&originalPresent))==MH_OK &&
        MH_EnableHook(table[17])==MH_OK,"observe displayed frames");
    Check(MH_CreateHook(table[82],&ObserveScaleDraw,reinterpret_cast<void**>(&originalScaleDraw))==MH_OK &&
        MH_EnableHook(table[82])==MH_OK,"observe real scene draw targets");
    Microsoft::WRL::ComPtr<IDirect3DSurface9> back;
    Microsoft::WRL::ComPtr<IDirect3DDevice9> native;
    app->Renderer()->device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);back->GetDevice(&native);
    auto** nativeTable=*reinterpret_cast<void***>(native.Get());
    Check(MH_CreateHook(nativeTable[81],&ObserveHudDraw,reinterpret_cast<void**>(&originalHudDraw))==MH_OK &&
        MH_EnableHook(nativeTable[81])==MH_OK,"observe native HUD draw resolution");
    back.Reset(); // Do not keep a reset-blocking backbuffer reference in the observer.
    app->FpsOverlayEnabled()=1;
    std::size_t baseline=0;
    const RECT display=app->ViewportRect();
    const int exportW=app->RenderWidth(),exportH=app->RenderHeight();
    for (const double percent : {100.,50.,25.,150.,100.}) {
        const bool automatic=percent==100;
        std::wstring error;
        Check(viewport_render_scale::Configure(app,automatic,percent,error),"configure production preview");
        scaledSceneDraws=0;
        for(int i=0;i<2;++i) {
            captureRequested=i==1;
            app->FramesPerSecond()=126;app->FpsOverlayElapsedSeconds()=0;
            const auto frameCount=app->state.fpsOverlayFrameCount;
            app->DeltaTime()=1.f/30;app->state.messageSeen=10;FrameDriver(app);
            Check(app->state.fpsOverlayFrameCount==frameCount+1 && app->FramesPerSecond()==126,
                "HUD does not add frames or replace the scene FPS counter");
        }
        Check(!captureRequested && !capturedPixels.empty(),"production preview presented");
        Check(automatic || scaledSceneDraws>0,"model actually draws into scaled target");
        Check(hudDraws>0,"real HUD batches drawn at display resolution");hudDraws=0;
        Check(EqualRect(&display,&app->ViewportRect()) && app->RenderWidth()==exportW && app->RenderHeight()==exportH,
            "display geometry and export size unchanged");
        app->MouseX()=short(display.right-70*app->Renderer()->viewScale);
        app->MouseY()=short(display.bottom-145*app->Renderer()->viewScale);
        app->LeftMouseButtonState()=0;
        Check(HandleCoordinateStripPress(app),"HUD caption hit test uses unchanged display pixels");
        HandleViewportToolRelease(app);
        std::size_t colored=0;
        for(LONG y=display.top;y<display.bottom;++y) for(LONG x=display.left;x<display.right;++x) {
            const DWORD pixel=capturedPixels[std::size_t(y)*captureWidth+x];
            // Count the blue model material, excluding gray grid lines whose
            // one-render-pixel thickness grows when the preview is upscaled.
            const unsigned blue=pixel&255,green=(pixel>>8)&255,red=(pixel>>16)&255;
            if (blue>red+25 && blue>green+20 && red<170) ++colored;
        }
        if(!baseline)baseline=colored;
        std::printf("SCALE %.0f%% draws=%u pixels=%zu baseline=%zu display=%ldx%ld\n",percent,scaledSceneDraws,colored,baseline,
            display.right-display.left,display.bottom-display.top);
        std::ofstream pixels(captureDirectory/("scale-"+std::to_string(int(percent))+".bgra"),std::ios::binary);
        pixels.write(reinterpret_cast<const char*>(capturedPixels.data()),capturedPixels.size()*4);
        std::ofstream dimensions(captureDirectory/"dimensions.txt");dimensions<<captureWidth<<" "<<captureHeight<<"\n";
        pixels.close(); dimensions.close();
        Check(colored>1000 && colored>baseline*8/10 && colored<baseline*12/10,"scene coverage preserved across render scales");
    }
    Check(PostDeviceReset(app),"production reset after scale changes");
    Check(!IsWindowVisible(app->MainWindow()),"production scene stays hidden");
    // Screen-space atlas pixels must not enter the scene AA or scaled target.
    // The fixture leaves both HUD corners clear. Disable the ground grid so
    // scene sampling cannot affect the comparison regions around the controls.
    app->GroundGridEnabled()=0;app->GroundShadowEnabled()=0;
    for(auto algorithm : {antialias::Algorithm::MmdOriginal,antialias::Algorithm::Msaa4,
        antialias::Algorithm::Fxaa,antialias::Algorithm::Taa}) {
        if(std::getenv("MMS_HUD_FINAL_SMOKE") && algorithm!=antialias::Algorithm::Msaa4)continue;
        if(!antialias::Supported(app,algorithm))continue;
        std::wstring error;
        Check(antialias::Select(app,algorithm,error),"select HUD comparison AA");
        const auto until=GetTickCount64()+10000;
        while(antialias::Initializing(app)&&GetTickCount64()<until) {
            app->state.messageSeen=10;FrameDriver(app);Sleep(1);
        }
        Check(!antialias::Initializing(app),"HUD comparison AA ready");
        std::vector<std::uint32_t> reference;
        for(double percent : {100.,50.,25.,12.5,150.}) {
            if(std::getenv("MMS_HUD_FINAL_SMOKE") && percent!=100 && percent!=25)continue;
            Check(viewport_render_scale::Configure(app,percent==100,percent,error),"configure HUD pixel comparison");
            // Text batches are intentionally produced for the following frame.
            // Let the last caption/input change reach that batch before capture.
            for(int frame=0;frame<2;++frame) {
                app->FramesPerSecond()=126;app->FpsOverlayElapsedSeconds()=0;app->state.messageSeen=10;
                captureRequested=frame==1;FrameDriver(app);
            }
            Check(!captureRequested && !capturedPixels.empty(),"capture displayed HUD pixels");
            std::vector<std::uint32_t> hud;
            const RECT regions[]={{display.left,display.bottom-55,display.left+220,display.bottom},
                {display.right-170,display.bottom-170,display.right,display.bottom}};
            for(const auto& region:regions)
                for(LONG y=region.top;y<region.bottom;++y)for(LONG x=region.left;x<region.right;++x)
                    hud.push_back(capturedPixels[std::size_t(y)*captureWidth+x]);
            if(reference.empty())reference=hud;
            std::ofstream file(captureDirectory/("hud-aa"+std::to_string(unsigned(algorithm))+"-"+std::to_string(int(percent))+".bgra"),std::ios::binary);
            file.write(reinterpret_cast<const char*>(capturedPixels.data()),capturedPixels.size()*4);
            file.close();
            std::size_t differences=0;for(std::size_t i=0;i<hud.size();++i)differences+=hud[i]!=reference[i];
            std::printf("HUD AA=%u scale=%g differing_pixels=%zu\n",unsigned(algorithm),percent,differences);
            Check(hud==reference,"HUD pixels are identical from 12.5 to 150 percent scene resolution");
        }
    }
    hudDraws=0;
    app->RecordingWindow()=CreateWindowW(L"STATIC",L"Hidden HUD export",WS_POPUP,0,0,640,480,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(app->RecordingWindow()!=nullptr,"create hidden HUD export window");
    app->state.messageSeen=10;RenderFrameScene(app);
    Check(hudDraws==0,"recorded image excludes editor HUD and FPS");
    DestroyWindow(app->RecordingWindow());app->RecordingWindow()=nullptr;
    Check(MH_CreateHook(&SetWindowPos,&HiddenHudWindowPos,reinterpret_cast<void**>(&originalHudWindowPos))==MH_OK &&
        MH_EnableHook(&SetWindowPos)==MH_OK,"prevent production floating-window show flags");
    InitFlagSubsystem(app);RefreshMainWindowViewport(app);
    Check(app->FloatingWindow()&&!IsWindowVisible(app->FloatingWindow()),"create hidden production floating viewport");
    std::wstring floatingError;
    Check(viewport_render_scale::Configure(app,false,25,floatingError),"scale floating preview");
    app->state.messageSeen=10;FrameDriver(app);
    Check(hudDraws>0,"floating viewport draws HUD at display resolution");
    SaveFlagSubsystem(app);RefreshMainWindowViewport(app);
    MH_DisableHook(&SetWindowPos);
    back.Reset();native.Reset();
    MH_DisableHook(nativeTable[81]);
    MH_DisableHook(table[17]);MH_DisableHook(table[82]);
    if (!std::getenv("MMS_HUD_FINAL_SMOKE") && !GetModuleHandleW(L"MMEffect.dll")) {
        EnableD3D12Native(true,true);
        for (double percent : {50.,150.,100.}) {
            std::wstring error;
            Check(viewport_render_scale::Configure(app,percent==100,percent,error),"configure DX12 preview");
            app->state.messageSeen=10;RenderFrameScene(app);
            const auto path=captureDirectory/("dx12-"+std::to_string(int(percent))+".bmp");
            const bool validated=ValidateD3D12Native(path.c_str());
            std::printf("DX12 status: %s\n",D3D12NativeStatus());
            if (!validated && std::strcmp(D3D12NativeStatus(),"No hardware D3D12 adapter")==0) {
                std::puts("SKIP DX12 runtime: no hardware D3D12 adapter");
                break;
            }
            Check(validated,"validate actual DX12 frame");
            std::ifstream file(path,std::ios::binary);BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
            file.read(reinterpret_cast<char*>(&header),sizeof(header));file.read(reinterpret_cast<char*>(&info),sizeof(info));
            const auto expected=viewport_render_scale::PreviewSize(app,percent==100,percent);
            Check(bool(file)&&unsigned(info.biWidth)==expected.width&&unsigned(-info.biHeight)==expected.height,
                "DX12 render buffer uses requested scale");
        }
        EnableD3D12Native(false,false);ShutdownD3D12Native();
    }
}
