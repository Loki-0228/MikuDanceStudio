// Included in the hidden native/MME production scene harness.
unsigned scaledSceneDraws = 0;
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
            app->DeltaTime()=1.f/30;app->state.messageSeen=10;FrameDriver(app);
        }
        Check(!captureRequested && !capturedPixels.empty(),"production preview presented");
        Check(automatic || scaledSceneDraws>0,"model actually draws into scaled target");
        Check(EqualRect(&display,&app->ViewportRect()) && app->RenderWidth()==exportW && app->RenderHeight()==exportH,
            "display geometry and export size unchanged");
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
    MH_DisableHook(table[17]);MH_DisableHook(table[82]);
    if (!GetModuleHandleW(L"MMEffect.dll")) {
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
