// Real dialog, Direct3D readback, image encoders and recording lifecycle.
// Run in a disposable directory. --hidden-production allows GPU testing on
// the input desktop with visibility/activation blocked for all test windows.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "mikudancestudio/image_sequence_export.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/globals.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/panel_controls.hpp"
#include "mikudancestudio/ui_translation.hpp"
#include "mikudancestudio/d3dx_dyn.hpp"
#include "mikudancestudio/antialias.hpp"
#include "mikudancestudio/antialias_motion.hpp"
#include "mikudancestudio/global_key_layout.hpp"
#include "mikudancestudio/runtime_log.hpp"
#include "../src/window/image_sequence_dialog.hpp"
#include <MinHook.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>
#include <cstdio>
#include <cmath>
#include <cstring>

using namespace mikudancestudio;
namespace seq = mikudancestudio::image_sequence;
namespace fs = std::filesystem;
namespace {
unsigned checks = 0, warnings = 0;
bool expectWarning = false;
bool dialogOnly = false;
fs::path output;
void Check(bool value, const char* reason) {
    ++checks;
    if (!value) { std::printf("FAIL %s\n", reason); std::exit(1); }
}
int WINAPI MessageW(HWND, LPCWSTR text, LPCWSTR, UINT) {
    if (expectWarning) { ++warnings; return IDOK; }
    std::fwprintf(stderr, L"FAIL unexpected message: %ls\n", text); std::exit(2);
}
int WINAPI MessageA(HWND, LPCSTR text, LPCSTR, UINT) {
    std::printf("FAIL unexpected message: %s\n", text); std::exit(2);
}
decltype(&ShowWindow) originalShowWindow;
BOOL WINAPI Hidden(HWND window, int) { return originalShowWindow(window, SW_HIDE); }
LRESULT CALLBACK HideCreated(int code, WPARAM wp, LPARAM lp) {
    if (code == HCBT_CREATEWND) reinterpret_cast<CBT_CREATEWND*>(lp)->lpcs->style &= ~WS_VISIBLE;
    if (code == HCBT_ACTIVATE) return 1;
    return CallNextHookEx(nullptr, code, wp, lp);
}
std::wstring Read(HWND dialog, int id) {
    wchar_t text[512]{}; GetDlgItemTextW(dialog, id, text, 512); return text;
}
void Capture(HWND dialog, const fs::path& path) {
    if (dialogOnly) {
        originalShowWindow(dialog, SW_SHOWNOACTIVATE);
        UpdateWindow(dialog);
    }
    RECT rect{}; GetWindowRect(dialog, &rect);
    const int width = rect.right - rect.left, height = rect.bottom - rect.top;
    HDC dc = GetWindowDC(dialog), memory = CreateCompatibleDC(dc);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    auto old = SelectObject(memory, bitmap);
    SendMessageW(dialog, WM_PRINT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT | PRF_NONCLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    BITMAPFILEHEADER file{}; file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER); file.bfSize = file.bfOffBits + width * height * 4;
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<char*>(&file), sizeof(file));
    stream.write(reinterpret_cast<char*>(&info.bmiHeader), sizeof(BITMAPINFOHEADER));
    stream.write(static_cast<char*>(pixels), width * height * 4);
    Check(bool(stream), "save dialog screenshot");
    SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(dialog, dc);
}
seq::DialogContext* dialogContext = nullptr;
bool cancelDialog = false;
INT_PTR CALLBACK Probe(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    if (message != WM_INITDIALOG) return seq::DialogProc(dialog, message, wp, lp);
    seq::DialogProc(dialog, message, wp, lp);
    auto& c = *dialogContext;
    Check(Read(dialog, panel::kAviWidthEdit) == L"1920" && Read(dialog, panel::kAviHeightEdit) == L"1080", "dialog dimensions initialized once");
    Check(SendDlgItemMessageW(dialog, seq::FormatCombo, CB_GETCOUNT, 0, 0) == 7, "seven single-image formats");
    wchar_t selected[32]{};
    const auto selection = SendDlgItemMessageW(dialog, seq::FormatCombo, CB_GETCURSEL, 0, 0);
    SendDlgItemMessageW(dialog, seq::FormatCombo, CB_GETLBTEXT, selection, reinterpret_cast<LPARAM>(selected));
    Check(wcscmp(selected, L"PNG") == 0, "PNG is the displayed default format");
    Check(Read(dialog, seq::Example).find(L"frame_0001.png") != std::wstring::npos, "500-frame filename uses four digits");
    SetDlgItemInt(dialog, panel::kAviEndFrameEdit, 499, FALSE);
    Check(Read(dialog, seq::Example).find(L"frame_001.png") != std::wstring::npos, "preview shrinks below leading-five threshold");
    SetDlgItemInt(dialog, panel::kAviEndFrameEdit, 500, FALSE);
    SetDlgItemTextW(dialog, seq::Prefix, L"镜头A_");
    Check(Read(dialog, seq::Example).find(L"镜头A_0001.png") != std::wstring::npos, "Unicode prefix preview");
    Capture(dialog, output / (L"dialog-" + std::to_wstring(c.app->EnglishUI()) + L".bmp"));
    if (cancelDialog) {
        seq::DialogProc(dialog, WM_COMMAND, IDCANCEL, 0);
        return TRUE;
    }
    SetDlgItemInt(dialog, panel::kAviWidthEdit, 640, FALSE);
    Check(Read(dialog, panel::kAviHeightEdit) == L"360", "size editing preserves video aspect ratio");
    SetDlgItemTextW(dialog, panel::kAviFpsEdit, L"nan");
    expectWarning = true; seq::DialogProc(dialog, WM_COMMAND, IDOK, 0); expectWarning = false;
    Check(!seq::Active(c.app), "invalid FPS does not arm export");
    SetDlgItemTextW(dialog, panel::kAviFpsEdit, L"29.97");
    CheckDlgButton(dialog, panel::kAviCloseAfterExportCheckbox, BST_CHECKED);
    seq::DialogProc(dialog, WM_COMMAND, IDOK, 0);
    return TRUE;
}
void DialogTests(MMDApp* app) {
    for (int language = 0; language < 3; ++language) {
        app->EnglishUI() = static_cast<unsigned char>(language);
        for (bool cancel : {false, true}) {
            seq::DialogContext c{app}; c.options.directory = output.wstring();
            c.options.width = 1920; c.options.height = 1080; c.options.end = 500;
            dialogContext = &c; cancelDialog = cancel;
            const auto result = UiDialogBoxParamA(GetModuleHandleW(nullptr), MAKEINTRESOURCEA(seq::kDialog),
                app->MainWindow(), Probe, reinterpret_cast<LPARAM>(&c));
            Check(result == (cancel ? IDCANCEL : IDOK), "real dialog completes through production callback");
            Check(seq::Active(app) == !cancel, "cancel does not prepare export");
            if (!cancel) Check(c.options.width == 640 && c.options.height == 360 &&
                c.options.prefix == L"镜头A_" && c.options.fps == 29.97f && c.options.closeAfterExport,
                "dialog commits all displayed settings");
            seq::Abort(app);
        }
        seq::RefreshMenu(app); seq::RefreshMenu(app);
        HMENU file = GetSubMenu(GetMenu(app->MainWindow()), 0);
        unsigned count = 0;
        for (int i = 0; i < GetMenuItemCount(file); ++i) count += GetMenuItemID(file, i) == seq::kMenuCommand;
        Check(count == 1, "menu survives language refresh without duplication");
    }
    Check(warnings == 3, "all three languages reject invalid FPS");
}
void NamesAndValidation(MMDApp* app) {
    seq::Options o; o.directory = output.wstring(); o.width = 64; o.height = 48;
    const std::pair<int, const wchar_t*> cases[] = {
        {0,L"frame_1.png"}, {4,L"frame_1.png"}, {5,L"frame_01.png"},
        {49,L"frame_01.png"}, {50,L"frame_001.png"},
        {499,L"frame_001.png"}, {500,L"frame_0001.png"},
        {1100,L"frame_0001.png"}, {4999,L"frame_0001.png"},
        {5000,L"frame_00001.png"}, {10000,L"frame_00001.png"},
    };
    for (const auto& item : cases) {
        o.end = item.first;
        Check(fs::path(seq::FramePath(o, 1)).filename() == item.second, "leading-digit rounding boundary");
    }
    o.end = 499; o.fps = 240;
    Check(fs::path(seq::FramePath(o, 1)).filename() == L"frame_0001.png", "high FPS has enough numbering digits");
    std::wstring error;
    for (const auto* prefix : {L"../bad", L"bad\\name", L"bad:", L"bad*", L"trailing.", L"trailing "}) {
        o.prefix = prefix; Check(!seq::Validate(app, o, error) && !error.empty(), "invalid filename rejected");
    }
    o.prefix = L""; Check(seq::Validate(app, o, error), "empty prefix permitted");
    o.fps = 0; Check(!seq::Validate(app, o, error), "zero FPS rejected");
    o.fps = 30; o.start = 600; Check(!seq::Validate(app, o, error), "reversed range rejected");
}
void Start(MMDApp* app, const seq::Options& o) {
    std::wstring error;
    if (!seq::Prepare(app, o, error)) std::fwprintf(stderr, L"%ls\n", error.c_str());
    Check(seq::Active(app), "prepare real export");
    app->AviOutputWidth() = o.width; app->AviOutputHeight() = o.height;
    app->AviRecordStartFrame() = o.start; app->AviRecordEndFrame() = o.end;
    app->AviRecordFps() = o.fps; app->AviIncludeWave() = 0; app->AviStereoOutput() = 0;
    app->AviCloseAfterExport() = false;
    StartAviRecordWindow(app);
    Check(app->RecordingWindow() && app->state.recordPlaybackStartPending, "start through video recording entry");
}
void RunExport(MMDApp* app, seq::Options o, bool cancel = false) {
    const int oldW = app->RenderWidth(), oldH = app->RenderHeight();
    const int oldScreenW = app->Renderer()->screenWidth, oldScreenH = app->Renderer()->screenHeight;
    Start(app, o);
    unsigned steps = 0;
    while (seq::Active(app) && steps++ < 150) {
        app->state.messageSeen = 1; FrameDriver(app);
        if (cancel && steps == 5) FinishAviRecord(app);
    }
    Check(!seq::Active(app) && !app->RecordingWindow() && !app->FrameStepPlayback(), "export releases recording state");
    Check(app->RenderWidth() == oldW && app->RenderHeight() == oldH, "render resolution restored");
    Check(app->Renderer()->screenWidth == oldScreenW && app->Renderer()->screenHeight == oldScreenH, "device preview resolution restored");
    std::uint64_t files = 0;
    for (const auto& file : fs::directory_iterator(o.directory)) {
        if (file.path().filename().wstring().find(o.prefix) == 0 && file.path().extension() == seq::kFormats[o.format].extension) ++files;
    }
    Check(cancel ? files > 0 && files < seq::FrameCount(o) : files == seq::FrameCount(o), "inclusive range and output FPS produce correct image count");
    for (std::uint64_t number = 1; number <= files; ++number) Check(fs::file_size(seq::FramePath(o, number)) > 0, "complete consecutive images");
    std::printf("PASS format=%ls fps=%.3f range=%d..%d images=%llu cancel=%d\n",
        seq::kFormats[o.format].name, o.fps, o.start, o.end, static_cast<unsigned long long>(files), cancel);
}
#include "antialias_image_sequence_checks.hpp"
}
int wmain(int argc, wchar_t** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Check(argc == 2 || argc == 3, "isolated artifact directory required"); output = fs::absolute(argv[1]);
    fs::create_directories(output);
    wchar_t desktop[256]{}; DWORD bytes = 0;
    Check(GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, desktop, sizeof desktop, &bytes) &&
        (_wcsicmp(desktop, L"Default") != 0 || (argc == 3 && wcscmp(argv[2], L"--hidden-production") == 0)),
        "private desktop or explicit hidden GPU test required");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    Check(MH_Initialize() == MH_OK, "initialize message guards");
    MH_CreateHook(reinterpret_cast<void*>(&MessageBoxW), reinterpret_cast<void*>(&MessageW), nullptr);
    MH_CreateHook(reinterpret_cast<void*>(&MessageBoxA), reinterpret_cast<void*>(&MessageA), nullptr);
    Check(MH_CreateHook(reinterpret_cast<void*>(&ShowWindow), reinterpret_cast<void*>(&Hidden), reinterpret_cast<void**>(&originalShowWindow)) == MH_OK, "install visibility guard");
    Check(MH_EnableHook(MH_ALL_HOOKS) == MH_OK, "enable visibility guard");
    auto hide = SetWindowsHookExW(WH_CBT, HideCreated, nullptr, GetCurrentThreadId());
    Check(hide != nullptr, "install activation guard");
    auto* app = new MMDApp; g_Block = app; app->state = MMDAppState{}; app->InitDefaults(); app->EnglishUI() = 2;
    dialogOnly = argc == 3 && wcscmp(argv[2], L"--dialogs-only") == 0;
    if (dialogOnly) {
        app->MainWindow() = CreateWindowW(L"STATIC", L"Image sequence dialog fixture", WS_POPUP,
            0, 0, 300, 200, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(app->MainWindow() != nullptr, "create hidden dialog owner");
        SetMenu(app->MainWindow(), LoadMenuW(GetModuleHandleW(nullptr), L"SAMPLE02E"));
        DialogTests(app);
        DestroyWindow(app->MainWindow()); g_Block = nullptr; delete app; UnhookWindowsHookEx(hide);
        std::printf("PASS image-sequence visible private-dialog checks=%u\n", checks);
        return 0;
    }
    Check(InitMainWindowAndD3D(app, GetModuleHandleW(nullptr), SW_HIDE), "initialize production renderer");
    Check(!IsWindowVisible(app->MainWindow()), "production host remains hidden");
    NamesAndValidation(app); DialogTests(app); app->EnglishUI() = 2;
    seq::Options o; o.directory = output.wstring(); o.width = 64; o.height = 48; o.end = 1;
    for (int format = 0; format < std::size(seq::kFormats); ++format) {
        o.format = format; o.prefix = L"中文_" + std::to_wstring(format) + L"_";
        RunExport(app, o);
        std::wstring error;
        const auto originalSize = fs::file_size(seq::FramePath(o, 1));
        Check(!seq::Prepare(app, o, error) && !error.empty() && !seq::Active(app), "existing image blocks export");
        Check(fs::file_size(seq::FramePath(o, 1)) == originalSize, "existing image unchanged");
    }
    o.format = 2; o.start = 10; o.end = 39;
    for (float fps : {24.f, 30.f, 60.f, 29.97f}) {
        o.fps = fps; o.prefix = L"fps_" + std::to_wstring(fps) + L"_"; RunExport(app, o);
    }
    o.fps = 30; o.prefix = L"cancel_"; RunExport(app, o, true);
    o.prefix = L"collision_"; std::wstring error;
    Check(seq::Prepare(app, o, error), "prepare before late collision");
    { std::ofstream sentinel(fs::path(seq::FramePath(o, 1))); sentinel << "keep"; }
    Check(!seq::WriteFrame(app, app->Renderer()->backbufferSurface, error) && !error.empty(), "late collision stops output");
    Check(fs::file_size(seq::FramePath(o, 1)) == 4, "late collision cannot overwrite image");
    seq::Abort(app);
    o.prefix = L"incomplete_"; Check(seq::Prepare(app, o, error), "prepare incomplete sequence");
    Check(!seq::Finish(app, true, error) && !error.empty() && !seq::Active(app), "incomplete sequence cannot report success");
    CheckTemporalImageSequence(app);
    for (const auto& entry : fs::directory_iterator(output)) Check(entry.path().extension() != L".tmp", "no temporary frames remain");
    ShutdownCleanup(app); g_Block = nullptr; delete app; UnhookWindowsHookEx(hide);
    std::printf("PASS image-sequence checks=%u\n", checks);
}
