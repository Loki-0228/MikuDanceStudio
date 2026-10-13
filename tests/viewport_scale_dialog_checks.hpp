// Included in the hidden production scene harness.
double ScaleClock() {
    LARGE_INTEGER now{}, frequency{}; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&frequency);
    return double(now.QuadPart) / frequency.QuadPart;
}
struct ScaleGpuSample {
    Microsoft::WRL::ComPtr<IDirect3DQuery9> start, end, frequency, disjoint;
    bool pending = false;
};
std::vector<ScaleGpuSample> scaleGpu(8);
std::vector<double> scaleCpuTimes, scaleGpuTimes;
unsigned scaleSceneFrames=0, scalePresentedFrames=0;
unsigned scaleTargetWidth=0, scaleTargetHeight=0;
bool scaleMeasure=false;
bool scaleSkipScene=false,scaleFailPresent=false;
void (*originalScaleScene)(MMDApp*)=nullptr;
void ObserveScaleScene(MMDApp* app) {
    if(scaleSkipScene) {scaleSkipScene=false;frame_rate::BeginScene(app);return;}
    ScaleGpuSample* sample=nullptr;
    if(scaleMeasure) for(auto& slot:scaleGpu) {
        if(slot.pending) {
            UINT64 start=0,end=0,frequency=0;BOOL disjoint=FALSE;
            if(slot.end->GetData(&end,sizeof(end),0)==S_OK &&
                slot.start->GetData(&start,sizeof(start),0)==S_OK &&
                slot.frequency->GetData(&frequency,sizeof(frequency),0)==S_OK &&
                slot.disjoint->GetData(&disjoint,sizeof(disjoint),0)==S_OK) {
                if(!disjoint && frequency && end>=start)scaleGpuTimes.push_back(double(end-start)*1000/frequency);
                slot.pending=false;
            }
        }
        if(!slot.pending && !sample)sample=&slot;
    }
    if(sample) {
        sample->disjoint->Issue(D3DISSUE_BEGIN);
        sample->frequency->Issue(D3DISSUE_END);
        sample->start->Issue(D3DISSUE_END);
    }
    const double start=ScaleClock();
    originalScaleScene(app);
    if(scaleMeasure) { scaleCpuTimes.push_back((ScaleClock()-start)*1000);++scaleSceneFrames; }
    if(sample) {sample->end->Issue(D3DISSUE_END);sample->disjoint->Issue(D3DISSUE_END);sample->pending=true;}
}
Present scaleOriginalPresent=nullptr;
HRESULT WINAPI ScaleDialogPresent(IDirect3DDevice9* device,const RECT* src,const RECT* dst,HWND window,const RGNDATA* data) {
    if(scaleFailPresent) {scaleFailPresent=false;return D3DERR_INVALIDCALL;}
    const HRESULT hr=ObservePresent(device,src,dst,window,data);
    if(SUCCEEDED(hr))++scalePresentedFrames;
    return hr;
}
ScaleDraw scaleOriginalDraw=nullptr;
HRESULT WINAPI ScaleDialogDraw(IDirect3DDevice9* device,D3DPRIMITIVETYPE type,INT base,UINT min,UINT vertices,UINT start,UINT primitives) {
    if(g_Block) {
        Microsoft::WRL::ComPtr<IDirect3DSurface9> target;D3DSURFACE_DESC desc{};
        device->GetRenderTarget(0,&target);target->GetDesc(&desc);
        if(target.Get()==g_Block->Renderer()->backbufferSurface) {
            D3DVIEWPORT9 viewport{};device->GetViewport(&viewport);
            scaleTargetWidth=viewport.Width;scaleTargetHeight=viewport.Height;
        }
    }
    return scaleOriginalDraw(device,type,base,min,vertices,start,primitives);
}
HDC scalePaintDC=nullptr;HWND scalePaintWindow=nullptr;
decltype(&BeginPaint) scaleBeginPaint=nullptr;
decltype(&EndPaint) scaleEndPaint=nullptr;
HDC WINAPI ScaleBeginPaint(HWND window,LPPAINTSTRUCT ps) {
    if(scalePaintDC&&window==scalePaintWindow) {
        *ps={};ps->hdc=scalePaintDC;GetClientRect(window,&ps->rcPaint);return scalePaintDC;
    }
    return scaleBeginPaint(window,ps);
}
BOOL WINAPI ScaleEndPaint(HWND window,const PAINTSTRUCT* ps) {
    return scalePaintDC&&window==scalePaintWindow ? TRUE : scaleEndPaint(window,ps);
}
void VerifyScaleOwnerPaint(MMDApp* app,const char* label) {
    Check(!capturedPixels.empty(),"capture current scene before owner paint");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=captureWidth;
    info.bmiHeader.biHeight=-LONG(captureHeight);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    HGDIOBJ old=SelectObject(dc,bitmap);memcpy(bits,capturedPixels.data(),capturedPixels.size()*4);
    scalePaintDC=dc;scalePaintWindow=app->FloatingWindow()?app->FloatingWindow():app->MainWindow();
    const bool enabled=IsWindowEnabled(app->MainWindow());EnableWindow(app->MainWindow(),FALSE);
    SendMessageW(scalePaintWindow,WM_PAINT,0,0);
    scalePaintDC=nullptr;scalePaintWindow=nullptr;EnableWindow(app->MainWindow(),enabled);
    GdiFlush();
    const RECT frame=FramePresentationRect(app);
    std::size_t differing=0,coloredMargins=0;
    const RECT scene=app->ViewportRect();
    for(LONG y=frame.top;y<frame.bottom;++y)for(LONG x=frame.left;x<frame.right;++x) {
        const std::size_t i=std::size_t(y)*captureWidth+x;
        differing+=(static_cast<DWORD*>(bits)[i]&0xffffff)!=capturedPixels[i];
        if((x<scene.left||x>=scene.right||y<scene.top||y>=scene.bottom)&&capturedPixels[i]!=0)++coloredMargins;
    }
    std::printf("PAINT %s changed_scene_or_hud_pixels=%zu colored_margin_pixels=%zu\n",label,differing,coloredMargins);
    Check(differing==0,"disabled-owner GDI repaint cannot erase full-frame HUD or FPS");
    Check(coloredMargins>0,"captured HUD or FPS is actually inside the black margins");
    std::ofstream file(captureDirectory/(std::string(label)+".bgra"),std::ios::binary);
    file.write(static_cast<char*>(bits),capturedPixels.size()*4);
    std::ofstream dims(captureDirectory/"dimensions.txt");dims<<captureWidth<<" "<<captureHeight<<"\n";
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
}
int scaleDialogStage=-1, scaleSavedFrame=0;
std::uint64_t scaleDialogPresentedBase=0;
double scaleStageStart=0;
std::ofstream scaleReport;
HWND scaleDialogWindow=nullptr;
void CALLBACK ScaleDialogTick(HWND dialog,UINT,UINT_PTR,DWORD) {
    const int scales[]={100,25,25,100};
    auto* app=g_Block;
    if(scaleDialogStage>=0 && ScaleClock()-scaleStageStart<2.0)return;
    if(scaleDialogStage>=0) {
        scaleMeasure=false;
        const double elapsed=ScaleClock()-scaleStageStart;
        double cpu=0,gpu=0;for(double ms:scaleCpuTimes)cpu+=ms;for(double ms:scaleGpuTimes)gpu+=ms;
        cpu/=std::max<std::size_t>(1,scaleCpuTimes.size());gpu/=std::max<std::size_t>(1,scaleGpuTimes.size());
        std::printf("MEASURE block=%d percent=%d presents=%u scenes=%u seconds=%.3f actual_fps=%.2f HUD_fps=%d cpu_scene_ms=%.4f gpu_timeline_ms=%.4f gpu_samples=%zu target=%ux%u\n",
            scaleDialogStage,scales[scaleDialogStage],scalePresentedFrames,scaleSceneFrames,elapsed,
            scalePresentedFrames/elapsed,app->FramesPerSecond(),cpu,gpu,scaleGpuTimes.size(),scaleTargetWidth,scaleTargetHeight);
        scaleReport<<scaleDialogStage<<"\t"<<scales[scaleDialogStage]<<"\t"<<scalePresentedFrames/elapsed<<"\t"
            <<app->FramesPerSecond()<<"\t"<<cpu<<"\t"<<gpu<<"\t"<<scaleTargetWidth<<"\t"<<scaleTargetHeight<<"\n";scaleReport.flush();
        Check(scaleSceneFrames>10 && scalePresentedFrames>10,"modal scale dialog continuously redraws real scene frames");
        Check(app->frameRate.presentedFrames-scaleDialogPresentedBase==scalePresentedFrames,
            "modal preview shares the successful scene-present counter with normal playback");
        Check(app->FramesPerSecond()>0 && app->FramesPerSecond()!=777,"modal preview updates FPS from actual presents");
        Check(app->CurrentFrame()==scaleSavedFrame,"modal preview never advances the timeline");
        Check(!IsWindowEnabled(app->MainWindow()),"scale settings keep their modal owner disabled");
        if(scales[scaleDialogStage]==25) {
            const auto expected=viewport_render_scale::PreviewSize(app,false,25);
            Check(scaleTargetWidth==expected.width&&scaleTargetHeight==expected.height,"25 percent scene uses the real reduced render target");
            VerifyScaleOwnerPaint(app,("open-dialog-"+std::to_string(scaleDialogStage)).c_str());
        }
    }
    if(++scaleDialogStage>=4) {
        KillTimer(dialog,9997);SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);return;
    }
    CheckDlgButton(dialog,viewport_render_scale::Automatic,BST_UNCHECKED);
    SetDlgItemTextW(dialog,viewport_render_scale::Percent,scales[scaleDialogStage]==100?L"100":L"25");
    SendMessageW(dialog,WM_COMMAND,viewport_render_scale::Apply,0);
    scaleTargetWidth=scaleTargetHeight=0;scaleCpuTimes.clear();scaleGpuTimes.clear();
    scaleSceneFrames=scalePresentedFrames=0;for(auto& slot:scaleGpu)slot.pending=false;
    scaleDialogPresentedBase=app->frameRate.presentedFrames;
    scaleStageStart=ScaleClock();scaleMeasure=true;captureRequested=true;
    // Force the asynchronous owner repaint that used to erase the margins.
    InvalidateRect(app->MainWindow(),nullptr,FALSE);
}
LRESULT CALLBACK ScaleDialogWatch(int code,WPARAM wp,LPARAM lp) {
    if(code==HC_ACTION) {
        auto* call=reinterpret_cast<CWPRETSTRUCT*>(lp);
        if(call->message==WM_INITDIALOG&&GetDlgItem(call->hwnd,viewport_render_scale::Percent)) {
            scaleDialogWindow=call->hwnd;SetTimer(call->hwnd,9997,100,ScaleDialogTick);
        }
    }
    return CallNextHookEx(nullptr,code,wp,lp);
}

int WINAPI ConfirmScaleDanceImport(HWND owner,LPCWSTR text,LPCWSTR caption,UINT flags) {
    if((flags&MB_TYPEMASK)==MB_OKCANCEL && caption &&
        (!wcscmp(caption,L"Load motion data")||!wcscmp(caption,L"载入动作数据")||!wcscmp(caption,L"モーション読込"))) {
        std::puts("PASS authorize the supplied dance motion's matching bone-name import");return IDOK;
    }
    return Unexpected(owner,text,caption,flags);
}
void CheckScaleDance(MMDApp* app,const wchar_t* motion) {
    const auto imported=captureDirectory/L"CH4NGE-import.vmd";
    std::filesystem::copy_file(motion,imported,std::filesystem::copy_options::overwrite_existing);
    {
        std::fstream file(imported,std::ios::binary|std::ios::in|std::ios::out);
        file.seekp(30);file.write(mdl::Mdl(app->SelectedModel())->name,20);
    }
    app->SetSelectedModelSlot(0);app->CameraMode()=0;
    MH_DisableHook(&MessageBoxW);MH_RemoveHook(&MessageBoxW);
    Check(MH_CreateHook(&MessageBoxW,&ConfirmScaleDanceImport,nullptr)==MH_OK&&MH_EnableHook(&MessageBoxW)==MH_OK,
        "confirm only the authorized motion-name mismatch in this hidden test");
    LoadVmdFile(imported.c_str());
    MH_DisableHook(&MessageBoxW);MH_RemoveHook(&MessageBoxW);
    Check(MH_CreateHook(&MessageBoxW,&Unexpected,nullptr)==MH_OK&&MH_EnableHook(&MessageBoxW)==MH_OK,
        "restore unexpected-dialog guard after motion import");
    auto* record=mdl::Mdl(app->SelectedModel());
    Check(record->boneCount>0&&record->boneKeys,"import actual CH4NGE motion into the Miku skeleton");
    app->CameraPositionY()=app->ViewOffsetY()=10;app->CameraDistance()=-42;
    app->CameraPositionX()=app->ViewOffsetX()=app->CameraPositionZ()=0;
    app->CameraYaw()=app->CameraPitch()=app->CameraRoll()=0;
    app->PlaybackPhysicsMode()=2;
    std::wstring aaError;Check(antialias::Select(app,antialias::Algorithm::MmdOriginal,aaError),"dance uses original MSAA baseline");
    std::ofstream report(captureDirectory/"dance-measurements.tsv");
    report<<"block\tpercent\tframes\tfull_sampling_ms\tthroughput_fps\tcpu_scene_ms\tgpu_scene_timeline_ms\tdraw_w\tdraw_h\n";
    std::uint64_t poseHash=0;unsigned changingPoses=0;
    const auto visit=[&](bool capture) {
        std::string error;
        const bool ok=scene_sampling::Visit(*app,0,119,30,[&](double seconds,bool) {
            const int frame=int(std::lround(seconds*30));
            std::uint64_t hash=1469598103934665603ull;
            const auto* bones=mdl::Bones(app->SelectedModel());
            for(unsigned b=0;b<record->boneCount;++b) {
                const auto* bytes=reinterpret_cast<const unsigned char*>(bones[b].matLocal);
                for(unsigned i=0;i<sizeof bones[b].matLocal;++i) {hash^=bytes[i];hash*=1099511628211ull;}
            }
            if(poseHash&&poseHash!=hash)++changingPoses;poseHash=hash;
            app->state.messageSeen=10;app->DeltaTime()=1.f/30;
            RenderFrameScene(app);
            captureRequested=capture&&(frame==0||frame==30||frame==60||frame==90||frame==119);
            const RECT rect=FramePresentationRect(app);
            const auto hr=app->Renderer()->device->Present(&rect,&rect,app->MainWindow(),nullptr);
            frame_rate::Presented(app,SUCCEEDED(hr));
            if(capture&&(frame==0||frame==30||frame==60||frame==90||frame==119)) {
                std::ofstream output(captureDirectory/("dance-frame-"+std::to_string(frame)+".bgra"),std::ios::binary);
                output.write(reinterpret_cast<const char*>(capturedPixels.data()),capturedPixels.size()*4);
            }
            return true;
        },error);
        if(!ok)std::printf("DANCE ERROR %s\n",error.c_str());
        Check(ok,"evaluate actual motion and private native physics for dance frames 0 through 119");
    };
    visit(true);Check(changingPoses>100,"more than 100 consecutive real dance poses change");
    for(int block=0;block<4;++block) {
        const int percent=(block==1||block==2)?25:100;
        std::wstring error;Check(viewport_render_scale::Configure(app,false,percent,error),"set dance resolution");
        visit(false); // Warm exactly the same animation and render resources.
        scaleCpuTimes.clear();scaleGpuTimes.clear();scaleSceneFrames=scalePresentedFrames=0;
        scaleTargetWidth=scaleTargetHeight=0;for(auto& sample:scaleGpu)sample.pending=false;
        const auto presentedBefore=app->frameRate.presentedFrames;
        scaleMeasure=true;const double begin=ScaleClock();
        for(int repeat=0;repeat<4;++repeat)visit(false);
        const double elapsed=ScaleClock()-begin;scaleMeasure=false;
        double cpu=0,gpu=0;for(double ms:scaleCpuTimes)cpu+=ms;for(double ms:scaleGpuTimes)gpu+=ms;
        cpu/=std::max<std::size_t>(1,scaleCpuTimes.size());gpu/=std::max<std::size_t>(1,scaleGpuTimes.size());
        std::printf("DANCE block=%d percent=%d frames=%u full_sampling_ms=%.3f throughput_fps=%.2f cpu_scene_ms=%.4f gpu_scene_timeline_ms=%.4f draw=%ux%u changed_poses=%u\n",
            block,percent,scaleSceneFrames,elapsed*1000,scaleSceneFrames/elapsed,cpu,gpu,scaleTargetWidth,scaleTargetHeight,changingPoses);
        report<<block<<"\t"<<percent<<"\t"<<scaleSceneFrames<<"\t"<<elapsed*1000<<"\t"<<scaleSceneFrames/elapsed<<"\t"
            <<cpu<<"\t"<<gpu<<"\t"<<scaleTargetWidth<<"\t"<<scaleTargetHeight<<"\n";report.flush();
        Check(scaleSceneFrames==480&&scalePresentedFrames==480,"each dance comparison renders the same four 120-frame sequences");
        Check(app->frameRate.presentedFrames-presentedBefore==480,"animated scene FPS counts exactly its 480 successful display frames");
    }
    app->PlaybackPhysicsMode()=0;app->FramesPerSecond()=777;
}

void CheckScaleActualFrameCounting(MMDApp* app) {
    std::wstring error;Check(antialias::Select(app,antialias::Algorithm::MmdOriginal,error),"use spatial AA for idle counter regression");
    const auto saving=app->state.frameVolumeControlEnabled;
    const auto playback=app->PlaybackActive();
    app->PlaybackActive()=0;app->state.frameVolumeControlEnabled=1;
    for(int i=0;i<4;++i){app->state.messageSeen=10;FrameDriver(app);}
    frame_rate::Reset(app);const unsigned before=scalePresentedFrames;
    for(int i=0;i<100;++i){app->state.messageSeen=0;FrameDriver(app);}
    Check(scalePresentedFrames==before&&app->frameRate.presentedFrames==0,
        "one hundred real performance-saving FrameDriver iterations count no display frames");
    app->state.messageSeen=10;FrameDriver(app);
    Check(app->frameRate.presentedFrames==1,"normal FrameDriver counts its one actual scene present");
    frame_rate::Presented(app,true);
    Check(app->frameRate.presentedFrames==1,"a repeated overlay present cannot inflate scene FPS");
    scaleSkipScene=true;app->state.messageSeen=10;FrameDriver(app);
    Check(app->frameRate.presentedFrames==1,"failed scene work followed by a stale host Present is not counted");
    scaleFailPresent=true;app->state.messageSeen=10;FrameDriver(app);
    Check(app->frameRate.presentedFrames==1,"unsuccessful host Present is not counted");
    BakePreview bake(app);app->state.messageSeen=10;bake.RenderNative();
    Check(app->frameRate.presentedFrames==2,"native bake preview contributes one actually displayed scene");
    bake.Present();
    Check(app->frameRate.presentedFrames==2,"repeating a bake preview without rendering adds no scene frame");
    app->RecordingWindow()=CreateWindowW(L"STATIC",L"Hidden FPS export",WS_POPUP,0,0,640,480,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(app->RecordingWindow()!=nullptr,"create hidden recording fixture");
    for(int i=0;i<8;++i){app->state.messageSeen=10;RenderFrameScene(app);frame_rate::Presented(app,true);}
    Check(app->frameRate.presentedFrames==0&&app->FramesPerSecond()==0,
        "offline recording frames and convergence samples do not become editor FPS");
    DestroyWindow(app->RecordingWindow());app->RecordingWindow()=nullptr;
    if(std::getenv("MMS_SCALE_FPS_DX12")&&!GetModuleHandleW(L"MMEffect.dll")) {
        EnableD3D12Native(true,false);frame_rate::Reset(app);
        for(int i=0;i<3;++i){app->state.messageSeen=10;FrameDriver(app);}
        std::printf("FPS DX12 status=%s presented=%llu\n",D3D12NativeStatus(),
            static_cast<unsigned long long>(app->frameRate.presentedFrames));
        Check(app->frameRate.presentedFrames==3,"native DX12 or its supported fallback counts once per real frame");
        EnableD3D12Native(false,false);ShutdownD3D12Native();
    }
    app->PlaybackActive()=playback;app->state.frameVolumeControlEnabled=saving;
    frame_rate::Reset(app);app->FramesPerSecond()=777;
}

void CheckViewportScaleDialog(MMDApp* app,const wchar_t* output) {
    captureDirectory=output;std::filesystem::create_directories(captureDirectory);
    app->FpsOverlayEnabled()=1;app->FramesPerSecond()=777;app->FpsLimit()=1000;
    app->PlaybackPhysicsMode()=0;app->GroundGridEnabled()=0;
    app->RenderWidth()=1280;app->RenderHeight()=1024;RefreshMainWindowViewport(app);
    app->state.messageSeen=10;FrameDriver(app);
    scaleSavedFrame=app->CurrentFrame();
    auto** vtable=*reinterpret_cast<void***>(app->Renderer()->device);
    Check(MH_CreateHook(vtable[17],&ScaleDialogPresent,reinterpret_cast<void**>(&originalPresent))==MH_OK &&
        MH_EnableHook(vtable[17])==MH_OK,"observe modal scene presents");
    Check(MH_CreateHook(vtable[82],&ScaleDialogDraw,reinterpret_cast<void**>(&scaleOriginalDraw))==MH_OK &&
        MH_EnableHook(vtable[82])==MH_OK,"observe reduced-resolution scene target");
    Check(MH_CreateHook(&RenderFrameScene,&ObserveScaleScene,reinterpret_cast<void**>(&originalScaleScene))==MH_OK &&
        MH_EnableHook(&RenderFrameScene)==MH_OK,"measure actual scene CPU/GPU submission");
    Check(MH_CreateHook(&BeginPaint,&ScaleBeginPaint,reinterpret_cast<void**>(&scaleBeginPaint))==MH_OK &&
        MH_EnableHook(&BeginPaint)==MH_OK &&
        MH_CreateHook(&EndPaint,&ScaleEndPaint,reinterpret_cast<void**>(&scaleEndPaint))==MH_OK &&
        MH_EnableHook(&EndPaint)==MH_OK,"capture production GDI painting into a private bitmap");
    Microsoft::WRL::ComPtr<IDirect3DSurface9> back;
    Microsoft::WRL::ComPtr<IDirect3DDevice9> native;
    app->Renderer()->device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);back->GetDevice(&native);back.Reset();
    bool querySupport=true;
    for(auto& sample:scaleGpu) {
        querySupport &= SUCCEEDED(native->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&sample.start)) &&
            SUCCEEDED(native->CreateQuery(D3DQUERYTYPE_TIMESTAMP,&sample.end)) &&
            SUCCEEDED(native->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ,&sample.frequency)) &&
            SUCCEEDED(native->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT,&sample.disjoint));
    }
    if(!querySupport)scaleGpu.clear();
    CheckScaleActualFrameCounting(app);
    if(const wchar_t* motion=_wgetenv(L"MMS_SCALE_DANCE_VMD"))CheckScaleDance(app,motion);
    scaleReport.open(captureDirectory/"measurements.tsv");
    scaleReport<<"block\tpercent\tpresent_fps\thud_fps\tcpu_scene_ms\tgpu_timeline_ms\ttarget_w\ttarget_h\n";
    HHOOK watch=SetWindowsHookExW(WH_CALLWNDPROCRET,ScaleDialogWatch,nullptr,GetCurrentThreadId());
    Check(watch!=nullptr,"observe scale dialog without showing or activating it");
    viewport_render_scale::HandleCommand(app,viewport_render_scale::kCommand);
    Check(scaleDialogStage==4,"complete real open-dialog ABBA preview");
    UnhookWindowsHookEx(watch);scaleReport.close();
    // Verify the detached preview uses the same protected presentation area.
    Check(MH_CreateHook(&SetWindowPos,&HiddenHudWindowPos,reinterpret_cast<void**>(&originalHudWindowPos))==MH_OK &&
        MH_EnableHook(&SetWindowPos)==MH_OK,"keep detached test windows hidden");
    InitFlagSubsystem(app);RefreshMainWindowViewport(app);
    Check(app->FloatingWindow()&&!IsWindowVisible(app->FloatingWindow()),"create hidden detached preview");
    std::wstring error;Check(viewport_render_scale::Configure(app,false,25,error),"scale detached preview");
    app->state.messageSeen=10;captureRequested=true;FrameDriver(app);
    VerifyScaleOwnerPaint(app,"detached-preview");
    SaveFlagSubsystem(app);RefreshMainWindowViewport(app);MH_DisableHook(&SetWindowPos);
    Check(!IsWindowVisible(app->MainWindow()),"all testing remains hidden");
    scaleGpu.clear();native.Reset();
    MH_DisableHook(&RenderFrameScene);MH_DisableHook(&BeginPaint);MH_DisableHook(&EndPaint);
    MH_DisableHook(vtable[17]);MH_DisableHook(vtable[82]);
}
