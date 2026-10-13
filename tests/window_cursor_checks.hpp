#pragma once

// Exercise native window procedures without moving the user's pointer or focus.
namespace cursor_checks {
inline HWND foreground = nullptr, capture = nullptr;
inline bool held = false;
inline int warps = 0, visibility = 0;
inline HCURSOR cursor = nullptr;
inline HWND WINAPI Foreground() { return foreground; }
inline HWND WINAPI Capture() { return capture; }
inline SHORT WINAPI Key(int key) { return held && key == VK_LBUTTON ? SHORT(0x8000) : 0; }
inline POINT pointer{};
inline BOOL WINAPI Position(LPPOINT p) { *p=pointer; return TRUE; }
inline BOOL WINAPI Warp(int x, int y) { pointer={x,y}; ++warps; return TRUE; }
inline HCURSOR WINAPI Cursor(HCURSOR next) { auto previous = cursor; cursor = next; return previous; }
inline int WINAPI Visibility(BOOL show) { return visibility += show ? 1 : -1; }
inline void Run(MMDApp* app) {
    const auto saved = std::make_unique<MMDAppState>(app->state);
    const auto install = [](auto api, auto callback) {
        Check(MH_CreateHook(reinterpret_cast<void*>(api), reinterpret_cast<void*>(callback), nullptr) == MH_OK,
              "install isolated cursor observation");
        Check(MH_EnableHook(reinterpret_cast<void*>(api)) == MH_OK, "enable cursor observation");
    };
    install(&GetForegroundWindow, &Foreground); install(&GetCapture, &Capture);
    install(&GetKeyState, &Key); install(&GetAsyncKeyState, &Key);
    install(&GetCursorPos, &Position); install(&SetCursorPos, &Warp); install(&SetCursor, &Cursor); install(&ShowCursor, &Visibility);
    const auto main = app->MainWindow();
    foreground = main;
    app->state.optflag[0] = 1;
    app->PlaybackActive() = 0;
    const RECT view = app->ViewportRect();
    app->MouseX() = app->PreviousMouseX() = view.right - 70;
    app->MouseY() = app->PreviousMouseY() = view.bottom - 65;
    app->LeftMouseButtonState() = 0;
    held = true;
    MouseInteractionBegin(app);
    Check(warps == 0 && app->ViewToolDragOperation() == ViewportToolAction::None,
          "non-client press cannot start a drag using stale viewport coordinates");
    const auto mode = app->EditMode();
    MicWndProc(main, WM_MOUSEACTIVATE, reinterpret_cast<WPARAM>(main), MAKELPARAM(HTRIGHT, WM_LBUTTONDOWN));
    Check(warps == 0 && app->EditMode() == mode, "border activation leaves the cursor and editing mode alone");
    cursor = LoadCursorA(nullptr, IDC_SIZEWE);
    app->ViewportToolHovered() = 0;
    Check(MicWndProc(main, WM_SETCURSOR, reinterpret_cast<WPARAM>(main), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)) == TRUE &&
          cursor == LoadCursorA(nullptr, IDC_ARROW), "detached viewport clears the resize cursor on client entry");
    capture = main;
    app->LeftMouseButtonState() = 0;
    Check(HandleViewportToolPress(app), "a real captured viewport press still starts the tool");
    Check(warps == 1, "captured tool starts one centred gesture");
    capture = nullptr;
    app->state.mouseJumped = 0;
    app->MouseY() = view.bottom + 40;
    RecentreCenteredDragCursor(app);
    Check(warps == 1, "capture loss prevents recentering over another window");
    HandleViewportToolRelease(app);
    Check(warps == 1 && visibility == 0, "capture cancellation restores visibility without warping");
    RECT client{}; GetClientRect(main,&client);
    for (int operation : {1,2}) {
        capture=foreground=main;held=true;app->LeftMouseButtonState()=0;
        app->CameraMode()=1;app->CameraParentModel()=-1;
        app->MouseX()=client.right-(operation==1?48:18);app->MouseY()=view.top-12;
        const int before=warps;
        Check(HandleViewportToolPress(app),"header zoom/pan starts a captured drag");
        Check(OwnsCenteredViewportDrag(app) && warps==before+1,"header buttons enter unlimited centered dragging");
        float expected=app->CameraDistance();
        const float panX=app->CameraPositionX(),panY=app->CameraPositionY(),panZ=app->CameraPositionZ();
        for(int repeat=0;repeat<12;++repeat) {
            const int x=operation==1?(view.left+view.right)/2:view.right+20;
            const int y=view.top-20;
            const int dy=y-app->PreviousMouseY();
            if(operation==1)expected-=dy*.1f;
            pointer={x,y};ClientToScreen(main,&pointer);
            MouseInteractionBegin(app);
            Check(app->MouseY()==(view.top+view.bottom)/2 && app->PreviousMouseY()==app->MouseY(),
                  "crossing top edge recenters without introducing a jump");
            if(operation==1)Check(std::abs(app->CameraDistance()-expected)<.01f,"zoom continues accumulating after each boundary crossing");
            MouseInteractionEnd(app);
        }
        if(operation==2)Check(app->CameraPositionX()!=panX || app->CameraPositionY()!=panY || app->CameraPositionZ()!=panZ,
                              "pan continues changing the camera beyond window edges");
        for(int edge=0;edge<4;++edge) {
            const int x=edge==0?view.left:edge==1?view.right-1:(view.left+view.right)/2;
            const int y=edge==2?view.top:edge==3?view.bottom-1:(view.top+view.bottom)/2;
            pointer={x,y};ClientToScreen(main,&pointer);
            MouseInteractionBegin(app);MouseInteractionEnd(app);
            Check(app->MouseX()==(view.left+view.right)/2 && app->MouseY()==(view.top+view.bottom)/2,
                  "touching each viewport edge is sufficient to continue dragging");
        }
        const float distance=app->CameraDistance();
        MouseInteractionBegin(app);MouseInteractionEnd(app);
        Check(app->CameraDistance()==distance,"recentered stationary cursor adds no zoom");
        held=false;HandleViewportToolRelease(app);
        Check(!OwnsCenteredViewportDrag(app) && visibility==0,"header release restores cursor visibility");
        app->LeftMouseButtonState()=0;held=true;
        app->MouseX()=client.right-(operation==1?48:18);app->MouseY()=view.top-12;
        Check(HandleViewportToolPress(app),"header tool can start again");
        const int cancelWarps=warps;capture=nullptr;
        HandleViewportToolRelease(app);
        Check(warps==cancelWarps && visibility==0,"header capture loss restores visibility without moving pointer");
    }
    for (void* api : {reinterpret_cast<void*>(&GetForegroundWindow), reinterpret_cast<void*>(&GetCapture),
                     reinterpret_cast<void*>(&GetKeyState), reinterpret_cast<void*>(&GetAsyncKeyState),
                     reinterpret_cast<void*>(&GetCursorPos), reinterpret_cast<void*>(&SetCursorPos), reinterpret_cast<void*>(&SetCursor),
                     reinterpret_cast<void*>(&ShowCursor)}) {
        MH_DisableHook(api); MH_RemoveHook(api);
    }
    app->state = *saved;
    std::puts("PASS detached cursor and non-client drag regressions");
}
}
