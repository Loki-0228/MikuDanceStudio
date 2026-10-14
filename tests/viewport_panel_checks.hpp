// Hidden production UI: real control rectangles, fonts, GDI paint and D3D HUD.
void CheckViewportPanels(MMDApp* app, const wchar_t* output) {
    const std::filesystem::path directory(output);
    std::filesystem::create_directories(directory);
    auto* device = app->Renderer()->device;
    void* present = (*reinterpret_cast<void***>(device))[17];
    Check(MH_CreateHook(present, &ObservePresent, reinterpret_cast<void**>(&originalPresent)) == MH_OK &&
        MH_EnableHook(present) == MH_OK, "capture actual panel scene outside desktop");
    const HWND window = app->MainWindow();
    const WindowDpiScope dpi(window);
    app->PlaybackActive() = 0; app->PlaybackPhysicsMode() = 0; app->FpsOverlayEnabled() = 1;
    app->SidebarRatio() = 0; app->SidebarWidth() = 250;
    const int exportWidth = app->RenderWidth(), exportHeight = app->RenderHeight();
    struct Case { const char* name; int width, height; bool square; };
    const Case cases[] = {{"wide",1500,900,false}, {"letterbox",820,1100,false},
        {"tiny",560,1000,false}, {"no-margin",820,650,true},
        {"compact",1080,900,false}, {"compact-letterbox",760,1100,false}};
    for (const auto& c : cases) {
        RECT outer{0,0,c.width,c.height};
        AdjustWindowRectEx(&outer, GetWindowLongW(window,GWL_STYLE), TRUE, GetWindowLongW(window,GWL_EXSTYLE));
        SetWindowPos(window,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,
            SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        app->RenderWidth() = c.square ? 1024 : 1920; app->RenderHeight() = c.square ? 1024 : 1080;
        app->Renderer()->presentParameters.BackBufferWidth = c.width;
        app->Renderer()->presentParameters.BackBufferHeight = c.height;
        Check(PostDeviceReset(app), "reset hidden viewport size");
        RefreshMainWindowViewport(app);
        for (int language : {kUiChinese,kUiEnglish,kUiJapanese}) for (int mode : {1,0}) {
            app->EnglishUI() = language; app->state.optflag[0] = mode;
            LocalizeUI(app); PostLanguageSweep(app);
            RECT client{};GetClientRect(window,&client);
            app->SidebarWidth() = std::strcmp(c.name,"tiny")==0 ? client.right-309 : 250;
            RefreshMainWindowViewport(app);
            const RECT scene = app->ViewportRect();
            LayoutViewportPanels(app);
            const auto layout = FramePanelLayout(app);
            Check(EqualRect(&scene,&app->ViewportRect()), "toolbars never move or resize the scene");
            Check(layout.overlay.top <= scene.top && layout.overlay.bottom >= scene.bottom,
                "extra rows occupy only letterbox margins");
            if (std::strcmp(c.name,"letterbox") == 0)
                Check(layout.topWrapped && layout.bottomWrapped && layout.topScale==1 && layout.bottomScale==1,
                    "narrow letterbox uses two full-size toolbar rows");
            if (c.square) Check(!layout.topWrapped && !layout.bottomWrapped,
                "without vertical margins the scene remains untouched");
            if (mode==0 && std::strncmp(c.name,"compact",7)==0)
                Check(layout.bottomScale==1 && layout.fieldReduction>0 &&
                    layout.bottomWrapped==(std::strcmp(c.name,"compact-letterbox")==0),
                    "shorter numeric fields preserve full-size text on one or two rows");
            Check(std::strcmp(c.name,"tiny")==0 ? FrameOverlayScale(app)<1 : FrameOverlayScale(app)==1,
                "HUD shrinks only when its blocks do not fit");
            std::vector<RECT> controls;
            for (int id : {536,537,538,539,540,541,542,544,545,546,547,548,549,
                    551,552,553,554,555,556,557,543,550,570,571,572,574,575,576,577}) {
                if ((mode==0 && (id==543||id==550)) || (mode!=0 && id>=570)) continue;
                const HWND child=GetDlgItem(window,id); Check(child!=nullptr,"toolbar control exists");
                RECT rect{};GetWindowRect(child,&rect);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&rect),2);
                const RECT& band=id>=551&&id<=557 ? layout.topBand : layout.bottomBand;
                Check(rect.left>=band.left && rect.right<=band.right && rect.top>=band.top && rect.bottom<=band.bottom,
                    "every toolbar control fits its visible band");
                for(const auto& previous:controls){RECT overlap{};
                    const bool intersects=IntersectRect(&overlap,&rect,&previous)!=0;
                    if(intersects)std::printf("OVERLAP %s language=%d mode=%d id=%d rect=%ld,%ld,%ld,%ld previous=%ld,%ld,%ld,%ld\n",
                        c.name,language,mode,id,rect.left,rect.top,rect.right,rect.bottom,previous.left,previous.top,previous.right,previous.bottom);
                    Check(!intersects,"toolbar controls never overlap");}
                controls.push_back(rect);
                const float scale=id>=551&&id<=557 ? layout.topScale : layout.bottomScale;
                if(scale==1 && layout.fieldReduction==0 && id>=544&&id<=549)Check(rect.right-rect.left==53,
                    "numeric edits retain their normal width");
                wchar_t className[16]{};GetClassNameW(child,className,16);
                if(_wcsicmp(className,L"Edit")==0) {
                    const HDC dc=GetDC(child);
                    const auto font=reinterpret_cast<HFONT>(SendMessageW(child,WM_GETFONT,0,0));
                    const auto previous=SelectObject(dc,font);TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);
                    SelectObject(dc,previous);ReleaseDC(child,dc);
                    RECT client{};GetClientRect(child,&client);
                    Check(client.bottom-client.top>=metrics.tmHeight+2,
                        "numeric edit client height fits the complete font with padding");
                }
            }
            app->MouseX()=layout.TopX(48);app->MouseY()=layout.TopY(-12);
            Check(ViewportToolAtPointForTest(app)==1,"zoom icon hit follows its row and scale without moving pointer");
            app->MouseX()=layout.TopX(18);Check(ViewportToolAtPointForTest(app)==2,"pan icon hit follows its row and scale");
            app->ViewportToolOperation()=ViewportToolAction::None;
            std::wstring error;
            Check(viewport_render_scale::Configure(app,true,100,error),"normal scene scale");
            for(int frame=0;frame<2;++frame){frame_rate::Reset(app);app->FramesPerSecond()=126;
                app->DeltaTime()=0;app->state.messageSeen=10;captureRequested=frame==1;FrameDriver(app);}
            Check(!captureRequested&&!capturedPixels.empty(),"capture final display");
            // Composite the real GDI toolbar and child controls over the actual
            // D3D readback, using a memory DC; never capture or show the desktop.
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=captureWidth;info.bmiHeader.biHeight=-LONG(captureHeight);
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
            HDC dc=CreateCompatibleDC(nullptr);void* pixels=nullptr;
            HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
            const auto old=SelectObject(dc,bitmap);std::memcpy(pixels,capturedPixels.data(),capturedPixels.size()*4);
            HandlePaletteChanged2(dc);
            for(int id=536;id<=577;++id) {
                if(id>557&&id<570)continue;
                const HWND child=GetDlgItem(window,id);if(!child)continue;
                RECT rect{};GetWindowRect(child,&rect);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&rect),2);
                if(rect.left<layout.overlay.left||rect.right>layout.overlay.right||rect.bottom-rect.top<2)continue;
                const int saved=SaveDC(dc);SetViewportOrgEx(dc,rect.left,rect.top,nullptr);
                SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_ERASEBKGND|PRF_NONCLIENT);
                RestoreDC(dc,saved);
            }
            GdiFlush();
            const std::string name=std::string(c.name)+"-"+std::to_string(language)+"-"+std::to_string(mode);
            std::ofstream raw(directory/(name+".bgra"),std::ios::binary);
            raw.write(static_cast<const char*>(pixels),capturedPixels.size()*4);
            std::ofstream dimensions(directory/(name+".txt"));dimensions<<captureWidth<<' '<<captureHeight<<' '
                <<layout.overlay.left<<' '<<layout.topBand.top<<' '<<layout.bottomBand.bottom;
            SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
            const float hudScale=FrameOverlayScale(app);
            Check(viewport_render_scale::Configure(app,false,25,error),"quarter scene scale");
            app->state.messageSeen=10;FrameDriver(app);
            Check(FrameOverlayScale(app)==hudScale && EqualRect(&scene,&app->ViewportRect()),
                "preview resolution cannot alter HUD size or toolbar wrapping");
            std::printf("PANELS %s top_rows=%d bottom_rows=%d top_scale=%.3f bottom_scale=%.3f hud=%.3f\n",
                name.c_str(),layout.topWrapped?2:1,layout.bottomWrapped?2:1,layout.topScale,layout.bottomScale,hudScale);
        }
    }
    app->RenderWidth()=exportWidth;app->RenderHeight()=exportHeight;
    MH_RemoveHook(present);
    Check(!IsWindowVisible(window),"UI regression never shows the window");
}
