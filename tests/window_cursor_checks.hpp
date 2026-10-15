#pragma once
#include <array>
#include <cmath>

// Exercise native window procedures without moving the user's pointer or focus.
namespace cursor_checks {
inline HWND foreground = nullptr, capture = nullptr;
inline bool held = false, control = false;
inline int warps = 0, visibility = 0;
inline HCURSOR cursor = nullptr;
inline HWND WINAPI Foreground() { return foreground; }
inline HWND WINAPI Capture() { return capture; }
inline SHORT WINAPI Key(int key) {
    return (held && key == VK_LBUTTON) || (control && key == VK_CONTROL) ? SHORT(0x8000) : 0;
}
inline POINT pointer{};
inline BOOL WINAPI Position(LPPOINT p) { *p=pointer; return TRUE; }
inline BOOL WINAPI Warp(int x, int y) { pointer={x,y}; ++warps; return TRUE; }
inline HCURSOR WINAPI Cursor(HCURSOR next) { auto previous = cursor; cursor = next; return previous; }
inline int WINAPI Visibility(BOOL show) { return visibility += show ? 1 : -1; }
inline void Run(MMDApp* app) {
    const auto saved = std::make_unique<MMDAppState>(app->state);
    const auto savedGesture = app->backgroundGesture;
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
    const RECT frame = FrameOverlayRect(app);
    const float scale = FrameOverlayScale(app);
    app->MouseX() = app->PreviousMouseX() = frame.right - int(70 * scale);
    app->MouseY() = app->PreviousMouseY() = frame.bottom - int(65 * scale);
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
    const auto headerPoint = [&](int operation) {
        auto panels = FramePanelLayout(app);
        if (MMD_ORIGINAL_PROFILE) panels.overlay.right = client.right;
        app->MouseX() = panels.TopX(operation == 1 ? 48 : 18);
        app->MouseY() = panels.TopY(-12);
    };
    for (int operation : {1,2}) {
        capture=foreground=main;held=true;app->LeftMouseButtonState()=0;
        app->CameraMode()=1;app->CameraParentModel()=-1;
        headerPoint(operation);
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
        headerPoint(operation);
        Check(HandleViewportToolPress(app),"header tool can start again");
        const int cancelWarps=warps;capture=nullptr;
        HandleViewportToolRelease(app);
        Check(warps==cancelWarps && visibility==0,"header capture loss restores visibility without moving pointer");
    }
    const auto camera = [&] {
        return std::array<float, 6>{app->CameraDistance(), app->CameraPositionX(), app->CameraPositionY(),
            app->CameraPositionZ(), app->ViewOffsetX(), app->ViewOffsetY()};
    };
    const auto cameraBefore = camera();
    const float savedScale = app->Renderer()->viewScale;
    const auto move = [&](int dx, int dy) {
        pointer = {app->PreviousMouseX() + dx, app->PreviousMouseY() + dy};
        ClientToScreen(main, &pointer);
        MouseInteractionBegin(app); MouseInteractionEnd(app);
    };
    const auto press = [&](int operation) {
        capture = foreground = main; held = control = true;
        app->LeftMouseButtonState() = 0; app->CtrlModifierState() = 1;
        headerPoint(operation);
        Check(HandleViewportToolPress(app), "Ctrl header drag starts at the displayed icon");
    };
    app->PictureWidth() = app->AviFrameWidth() = 160;
    app->PictureHeight() = app->AviFrameHeight() = 96;
    // Video takes priority over a visible picture; hiding it exposes the picture.
    for (bool video : {false, true}) {
        app->PictureBackgroundEnabled() = 1; app->AviBackgroundEnabled() = video;
        auto& x = video ? app->AviOffsetX() : app->PictureOffsetX();
        auto& y = video ? app->AviOffsetY() : app->PictureOffsetY();
        auto& mediaScale = video ? app->AviScale() : app->PictureScale();
        for (float viewportScale : {.6f, 1.2f, 2.4f}) {
            app->Renderer()->viewScale = viewportScale;
            x = 20; y = 15; mediaScale = 1;
            press(2);
            // Releasing Ctrl during a gesture must never switch to the camera.
            control = false;
            for (int i = 0; i < 10; ++i) move(1, -1);
            const double factor = 1.2000000476837158 / viewportScale;
            Check(x == std::llround(20 + 10 * factor) && y == std::llround(15 - 10 * factor),
                  "background follows small pointer movements at every viewport scale");
            Check(camera() == cameraBefore, "background pan leaves all camera values unchanged");
            held = false; HandleViewportToolRelease(app);
            const double centreX = x + 80 * mediaScale, centreY = y + 48 * mediaScale;
            press(1); move(0, -20);
            Check(mediaScale > 1, "Ctrl zoom enlarges background when dragging upward");
            Check(std::abs(x + 80 * mediaScale - centreX) <= .51 &&
                  std::abs(y + 48 * mediaScale - centreY) <= .51, "zoom preserves background centre");
            move(0, 20);
            Check(std::abs(mediaScale - 1) < .00001, "reverse zoom restores background size");
            Check(camera() == cameraBefore, "background zoom leaves all camera values unchanged");
            float* vertices = nullptr;
            auto* buffer = video ? app->AviOverlayVertices() : app->PictureOverlayVertices();
            Check(buffer && SUCCEEDED(buffer->Lock(0, 0, reinterpret_cast<void**>(&vertices), D3DLOCK_READONLY)),
                  "read background geometry after drag");
            Check(std::abs(vertices[14] - (view.left + x / factor - 1)) < .01 &&
                  std::abs(vertices[1] - (view.top + y / factor - 1)) < .01,
                  "drag immediately updates the actual rendered background vertices");
            buffer->Unlock();
            capture = nullptr; held = false; HandleViewportToolRelease(app);
            Check(visibility == 0 && app->backgroundGesture.target == MMDApp::BackgroundGesture::Target::Camera,
                  "cancelling background gesture restores the pointer and clears its target");
        }
        if (video) {
            const int pictureX = app->PictureOffsetX(), pictureY = app->PictureOffsetY();
            press(2); move(11, 7); held = false; HandleViewportToolRelease(app);
            Check(app->PictureOffsetX() == pictureX && app->PictureOffsetY() == pictureY,
                  "topmost video gesture does not move the picture underneath");
        }
    }
    app->PictureBackgroundEnabled() = app->AviBackgroundEnabled() = 0;
    press(2); move(20, 20); held = false; HandleViewportToolRelease(app);
    Check(camera() == cameraBefore, "Ctrl drag without visible media does not move camera");
    app->Renderer()->viewScale = savedScale;
    control = false;
    for (void* api : {reinterpret_cast<void*>(&GetForegroundWindow), reinterpret_cast<void*>(&GetCapture),
                     reinterpret_cast<void*>(&GetKeyState), reinterpret_cast<void*>(&GetAsyncKeyState),
                     reinterpret_cast<void*>(&GetCursorPos), reinterpret_cast<void*>(&SetCursorPos), reinterpret_cast<void*>(&SetCursor),
                     reinterpret_cast<void*>(&ShowCursor)}) {
        MH_DisableHook(api); MH_RemoveHook(api);
    }
    app->state = *saved;
    app->backgroundGesture = savedGesture;
    std::puts("PASS camera and Ctrl background gestures, scaled header hit testing, cursor cancellation and geometry");
}
}
