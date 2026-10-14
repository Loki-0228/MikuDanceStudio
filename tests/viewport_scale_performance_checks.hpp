// Included by the hidden native/MME host. Timing excludes captures and resize.
void CheckViewportScalePerformance(MMDApp* app, const wchar_t* output) {
    using Microsoft::WRL::ComPtr;
    const auto clock=[] {
        LARGE_INTEGER now{}, frequency{};
        QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
        return double(now.QuadPart)/frequency.QuadPart;
    };
    const std::filesystem::path directory(output);
    std::filesystem::create_directories(directory);
    std::ofstream report(directory/"performance.tsv");
    report<<"enhanced\tadaptive\tblock\tpercent\tframes\tfps\tshadow_size\tfiltered_size\n";
    app->FpsLimit()=1000;app->PlaybackPhysicsMode()=0;app->GroundGridEnabled()=0;
    app->EditMode()=ViewportEditMode::Camera;app->state.selfShadowEnabled=1;
    app->SelfShadowMode()=1;app->ShadowResolutionLimit()=4096;
    std::wstring error;
    Check(antialias::Select(app,antialias::Algorithm::MmdOriginal,error),"fixed AA for scale throughput");
    auto* device=app->Renderer()->device;
    ComPtr<IDirect3DSurface9> back;
    ComPtr<IDirect3DDevice9> native;
    device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);back->GetDevice(&native);back.Reset();
    ComPtr<IDirect3D9> d3d;D3DDEVICE_CREATION_PARAMETERS creation{};D3DADAPTER_IDENTIFIER9 adapter{};
    Check(SUCCEEDED(native->GetDirect3D(&d3d)) && SUCCEEDED(native->GetCreationParameters(&creation)) &&
        SUCCEEDED(d3d->GetAdapterIdentifier(creation.AdapterOrdinal,0,&adapter)),"identify benchmark adapter");
    std::printf("SCALE_ADAPTER name=%s vendor=%04lx device=%04lx type=%u behavior=%08lx\n",
        adapter.Description,adapter.VendorId,adapter.DeviceId,unsigned(creation.DeviceType),creation.BehaviorFlags);
    ComPtr<IDirect3DQuery9> drain;
    Check(SUCCEEDED(native->CreateQuery(D3DQUERYTYPE_EVENT,&drain)),"GPU completion query");
    const auto flush=[&] {
        Check(SUCCEEDED(drain->Issue(D3DISSUE_END)),"submit GPU completion");
        const auto start=clock();HRESULT hr=S_FALSE;
        while((hr=drain->GetData(nullptr,0,D3DGETDATA_FLUSH))==S_FALSE && clock()-start<10)Sleep(0);
        Check(hr==S_OK,"finish queued GPU work before timing");
    };
    const auto frame=[&] {
        app->DeltaTime()=0;app->state.messageSeen=10;
        RenderFrameScene(app);
        const RECT rect=FramePresentationRect(app);
        const HRESULT hr=device->Present(&rect,&rect,app->MainWindow(),nullptr);
        frame_rate::Presented(app,SUCCEEDED(hr));
        if(FAILED(hr))Check(false,"benchmark frame presented");
    };
    for(bool enhanced:{false,true})for(bool adaptive:{false,true}) {
        app->ShadowFilterEnhanced()=enhanced;app->AdaptiveShadowResolution()=adaptive;
        unsigned fullShadow=0,fullFiltered=0;
        for(int block=0;block<4;++block) {
            const int percent=block==1||block==2?25:100;
            Check(viewport_render_scale::Configure(app,false,percent,error),"configure scale throughput");
            const double warm=clock();do{frame();}while(clock()-warm<.5);
            flush();
            frame_rate::Reset(app);
            unsigned frames=0;const double start=clock();
            do {frame();++frames;}while(clock()-start<1.5);
            flush();const double elapsed=clock()-start;
            Check(app->frameRate.presentedFrames==frames,"throughput counts completed scene presents");
            D3DSURFACE_DESC coarse{},filtered{};
            auto* renderer=app->Renderer();
            Check(renderer->hdrTexture && SUCCEEDED(renderer->hdrTexture->GetLevelDesc(0,&coarse)),"actual shadow dimensions");
            auto* visible=shadow_filter::VisibleTexture(app);
            Check(visible && SUCCEEDED(visible->GetLevelDesc(0,&filtered)),"actual shared shadow dimensions");
            if(block==0){fullShadow=coarse.Width;fullFiltered=filtered.Width;}
            if(std::getenv("MMS_SCALE_EXPECT_SHADOW_BUDGET")) {
                if(percent==25) {
                    Check(coarse.Width<=std::max(512u,fullShadow/(adaptive?2:4)),"low preview reduces shadow rasterization");
                    Check(filtered.Width<=std::max(512u,fullFiltered/(adaptive?2:4)),"low preview reduces enhanced depth filtering");
                } else if(block==3) {
                    Check(coarse.Width==fullShadow && filtered.Width==fullFiltered,"100 percent restores shadow quality");
                }
            }
            std::printf("SCALE_PERF enhanced=%d adaptive=%d block=%d percent=%d fps=%.3f shadow=%u filtered=%u\n",
                enhanced,adaptive,block,percent,frames/elapsed,coarse.Width,filtered.Width);
            report<<enhanced<<'\t'<<adaptive<<'\t'<<block<<'\t'<<percent<<'\t'<<frames<<'\t'
                <<frames/elapsed<<'\t'<<coarse.Width<<'\t'<<filtered.Width<<'\n';report.flush();
        }
    }
}
