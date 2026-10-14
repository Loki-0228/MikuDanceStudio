// Continuous, frozen-scene captures expose periodic jitter hidden by a single
// final screenshot. Invoked only by the isolated scene acceptance harness.
int WINAPI AcceptDanceName(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags) {
    if((flags&MB_TYPEMASK)==MB_OKCANCEL && title &&
        (!wcscmp(title,L"Load motion data") || !wcscmp(title,L"载入动作数据") || !wcscmp(title,L"モーション読込")))
        return IDOK;
    return Unexpected(owner,text,title,flags);
}
void CheckAntialiasStability(MMDApp* app, const wchar_t* directory) {
    captureDirectory=directory;std::filesystem::create_directories(captureDirectory);
    app->PlaybackPhysicsMode()=0;app->FpsOverlayEnabled()=0;
    wchar_t motionPath[32768]{};
    const bool dance=GetEnvironmentVariableW(L"MMS_AA_DANCE_VMD",motionPath,32768)>0;
    if(dance) {
        const auto imported=captureDirectory/L"dance-import.vmd";
        std::filesystem::copy_file(motionPath,imported,std::filesystem::copy_options::overwrite_existing);
        app->SetSelectedModelSlot(0);app->CameraMode()=0;
        std::fstream file(imported,std::ios::binary|std::ios::in|std::ios::out);
        file.seekp(30);file.write(mdl::Mdl(app->SelectedModel())->name,20);file.close();
        Check(MH_RemoveHook(&MessageBoxW)==MH_OK && MH_CreateHook(&MessageBoxW,&AcceptDanceName,nullptr)==MH_OK &&
            MH_EnableHook(&MessageBoxW)==MH_OK,"confirm authorized cross-model dance import only in hidden test");
        LoadVmdFile(imported.c_str());
        Check(MH_RemoveHook(&MessageBoxW)==MH_OK && MH_CreateHook(&MessageBoxW,&Unexpected,nullptr)==MH_OK &&
            MH_EnableHook(&MessageBoxW)==MH_OK,"restore unexpected-dialog guard");
        app->CameraPositionX()=app->ViewOffsetX()=0;app->CameraPositionY()=app->ViewOffsetY()=10;app->CameraPositionZ()=0;
        app->CameraDistance()=-42;app->CameraPitch()=0;app->CameraYaw()=0;app->CameraRoll()=0;
        app->CurrentFrame()=300;SeekSelectedModelToCurrentFrame(app);
        app->CameraMode()=1; // Hide editing markers in the camera preview.
        app->ViewOffsetY()=0; // Camera focus already supplies the vertical offset.
        const auto scene=captureDirectory/L"miku-dance.pmm";
        wcscpy_s(app->EnvFileName(),256,scene.c_str());SaveSceneFile(app);
        std::printf("DANCE fixture model=%s motion=%ls frozen_frame=300\n",mdl::Mdl(app->SelectedModel())->name,motionPath);
    }
    const auto table=*reinterpret_cast<void***>(app->Renderer()->device);
    Check(MH_CreateHook(table[17],&ObservePresent,reinterpret_cast<void**>(&originalPresent))==MH_OK &&
        MH_EnableHook(table[17])==MH_OK,"capture continuous frozen frames");
    const RECT view=app->ViewportRect();
    std::ofstream metrics(captureDirectory/"stability.tsv");
    metrics<<"algorithm\tscale\tframe\tsamples\tdifferent_pixels\tmean_rgb_delta\tprojection_changed\tdraws\tmatched\tmotion_px\n";
    for(double scale:{100.,25.})for(auto mode:{antialias::Algorithm::Taa,antialias::Algorithm::Smaa,
        antialias::Algorithm::SmaaStandard,antialias::Algorithm::SmaaT2xStandard,antialias::Algorithm::SmaaT2xEnhanced}) {
        std::wstring error;
        Check(viewport_render_scale::Configure(app,scale==100,scale,error),"configure stability scale");
        Check(antialias::Select(app,mode,error),"select stability AA");
        const auto until=GetTickCount64()+10000;
        while(antialias::Initializing(app) && GetTickCount64()<until) {
            app->DeltaTime()=0;app->state.messageSeen=10;FrameDriver(app);Sleep(1);
        }
        Check(!antialias::Initializing(app),"stability AA initialized");
        std::vector<std::uint32_t> previous;
        for(int frame=0;frame<24;++frame) {
            D3DMATRIX before{},after{};
            app->Renderer()->device->GetTransform(D3DTS_PROJECTION,&before);
            app->DeltaTime()=0;app->state.messageSeen=10;captureRequested=true;FrameDriver(app);
            app->Renderer()->device->GetTransform(D3DTS_PROJECTION,&after);
            Check(!captureRequested && !capturedPixels.empty(),"capture static AA frame");
            unsigned differences=0;double delta=0;
            if(!previous.empty())for(LONG y=view.top;y<view.bottom;++y)for(LONG x=view.left;x<view.right;++x) {
                const auto i=std::size_t(y)*captureWidth+x;
                differences+=(previous[i]&0xffffff)!=(capturedPixels[i]&0xffffff);
                for(unsigned channel=0;channel<3;++channel)
                    delta+=std::abs(int((previous[i]>>(8*channel))&255)-int((capturedPixels[i]>>(8*channel))&255));
            }
            const auto motion=antialias::motion::GetStats(app);
            metrics<<unsigned(mode)<<'\t'<<scale<<'\t'<<frame<<'\t'<<antialias::AccumulatedSamples(app)<<'\t'
                <<differences<<'\t'<<delta/(3.0*(view.right-view.left)*(view.bottom-view.top))<<'\t'
                <<(std::memcmp(&before,&after,sizeof(before))!=0)<<'\t'<<motion.draws<<'\t'<<motion.matchedDraws<<'\t'<<motion.maxMotionPixels<<'\n';
            if(frame>=16) {
                const auto name="aa"+std::to_string(unsigned(mode))+"-scale"+std::to_string(int(scale))+"-frame"+std::to_string(frame);
                std::ofstream image(captureDirectory/(name+".bgra"),std::ios::binary);
                image.write(reinterpret_cast<const char*>(capturedPixels.data()),capturedPixels.size()*4);
                image.flush();Check(image.good(),"save complete static frame evidence");
            }
            previous=capturedPixels;
        }
    }
    if(dance) {
        std::ofstream playing(captureDirectory/"dance.tsv");
        playing<<"algorithm\tframe\tsamples\tdraws\tmatched\tmotion_px\n";
        std::wstring error;
        Check(viewport_render_scale::Configure(app,false,50,error),"dance uses 50 percent scene resolution");
        for(auto mode:{antialias::Algorithm::Taa,antialias::Algorithm::Smaa,antialias::Algorithm::SmaaT2xEnhanced}) {
            Check(antialias::Select(app,mode,error),"select dance AA");
            const auto until=GetTickCount64()+10000;
            while(antialias::Initializing(app) && GetTickCount64()<until) {
                app->state.messageSeen=10;FrameDriver(app);Sleep(1);
            }
            std::vector<std::uint32_t> first;
            float maximumMotion=0;
            for(int frame=0;frame<96;++frame) {
                app->CurrentFrame()=300+frame;SeekSelectedModelToCurrentFrame(app);
                app->CameraYaw()=frame<48?0:(frame-48)*.003f;
                app->DeltaTime()=0;app->state.messageSeen=10;captureRequested=true;FrameDriver(app);
                const auto motion=antialias::motion::GetStats(app);
                const float actualMotion=mode==antialias::Algorithm::Smaa?0:ReadAntialiasMotionField(app).MaximumPixels();
                maximumMotion=std::max(maximumMotion,actualMotion);
                playing<<unsigned(mode)<<'\t'<<app->CurrentFrame()<<'\t'<<antialias::AccumulatedSamples(app)<<'\t'
                    <<motion.draws<<'\t'<<motion.matchedDraws<<'\t'<<actualMotion<<'\n';
                Check(!captureRequested && !capturedPixels.empty(),"capture real VMD dancing frame");
                if(frame==0)first=capturedPixels;
                const auto name="dance-aa"+std::to_string(unsigned(mode))+"-"+std::to_string(frame);
                std::ofstream image(captureDirectory/(name+".bgra"),std::ios::binary);
                image.write(reinterpret_cast<const char*>(capturedPixels.data()),capturedPixels.size()*4);
                image.flush();Check(image.good(),"save complete dance frame evidence");
            }
            Check(first!=capturedPixels,"VMD and camera movement change actual rendered pose");
            if(mode!=antialias::Algorithm::Smaa)Check(maximumMotion>.01f,"real dance has tracked geometry motion");
        }
    }
    std::ofstream(captureDirectory/"dimensions.txt")<<captureWidth<<' '<<captureHeight;
    MH_DisableHook(table[17]);
}
