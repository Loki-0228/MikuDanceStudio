// MAINTENANCE: diagnostic; real native-MME reassignment and reload capture.
// Run only in a staged runtime. MMS_MME_REAL_PICKER=1 requires a private desktop
// and drives the real file dialog. MMS_MME_HIDDEN_DEFAULT=1 permits hidden
// windows on the Default desktop with injected selection only. Both modes run MME's real editor,
// assignment, compilation and draws; the logs distinguish the picker evidence.
#define NOMINMAX
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/text_encoding.hpp"
#include "mikudancestudio/d3dx_dyn.hpp"
#include <MinHook.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dlgs.h>
#include <wrl/client.h>
#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

extern "C" char* ExpGetPmdBoneName(int, int);
extern "C" float* ExpGetPmdBoneWorldMat(float*, int, int);

using namespace mikudancestudio;
namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT kWidth = 640, kHeight = 480;
const char* phase = "startup";
fs::path modelPath, effectPath, outputPath;
std::string effectAnsi;
int controlBone = -1;
unsigned pickerCalls = 0, compileCalls = 0;
bool assigning = false;
bool realPicker = false;
HWND editor = nullptr;
bool editorDone = false, insideEditorTimer = false;
unsigned editorTicks = 0, editorStep = 0;
std::function<void()> captureWhileEditorOpen;

void Require(bool ok, const char* text) {
    if (!ok) {
        std::fprintf(stderr, "FAIL phase=%s %s\n", phase, text);
        std::fflush(nullptr);
        ExitProcess(7);
    }
}
std::string Utf8(const wchar_t* text) {
    std::string result;
    text_encoding::EncodeExact(text ? text : L"", CP_UTF8, result);
    return result;
}
std::string Hex(const char* text) {
    std::string result;
    for (const auto* p = reinterpret_cast<const unsigned char*>(text); p && *p; ++p) {
        char byte[4]{}; std::snprintf(byte, sizeof(byte), "%02X", *p);
        if (!result.empty()) result += ' ';
        result += byte;
    }
    return result;
}
std::string CurrentDirectory() {
    wchar_t path[32768]{};
    Require(GetCurrentDirectoryW(static_cast<DWORD>(std::size(path)), path) > 0,
            "read current directory");
    return Utf8(path);
}
void Environment(const char* event) {
    const char* locale = std::setlocale(LC_ALL, nullptr);
    std::printf("ENV phase=%s event=%s cwd=%s acp=%u crt_locale=%s thread=%lu\n",
        phase, event, CurrentDirectory().c_str(), GetACP(), locale ? locale : "(null)",
        GetCurrentThreadId());
}

decltype(&ShowWindow) originalShow = nullptr;
decltype(&GetOpenFileNameA) originalOpenFileA = nullptr;
decltype(&GetOpenFileNameW) originalOpenFileW = nullptr;
struct PickerDrive {
    bool active = false, insideTimer = false, opened = false;
    UINT_PTR timer = 0;
    ULONGLONG deadline = 0;
    HWND owner = nullptr;
    std::vector<HWND> previousWindows;
} pickerDrive;
BOOL WINAPI Hide(HWND window, int) { return originalShow(window, SW_HIDE); }
LRESULT CALLBACK HideCreated(int code, WPARAM wp, LPARAM lp) {
    if (code == HCBT_CREATEWND) {
        auto* create = reinterpret_cast<CBT_CREATEWNDW*>(lp)->lpcs;
        if (!(create->style & WS_CHILD)) {
            create->style &= ~WS_VISIBLE;
            create->x = create->y = -32000;
        }
    }
    if (code == HCBT_ACTIVATE) return 1;
    return CallNextHookEx(nullptr, code, wp, lp);
}
int WINAPI UnexpectedWide(HWND, LPCWSTR body, LPCWSTR title, UINT) {
    std::fprintf(stderr, "DIALOG phase=%s title=%s body=%s\n", phase,
        Utf8(title).c_str(), Utf8(body).c_str());
    Require(false, "unexpected message box"); return 0;
}
int WINAPI UnexpectedAnsi(HWND, LPCSTR body, LPCSTR title, UINT) {
    std::fprintf(stderr, "DIALOG_A phase=%s title_hex=%s body_hex=%s\n", phase,
        Hex(title).c_str(), Hex(body).c_str());
    Require(false, "unexpected ANSI message box"); return 0;
}
BOOL WINAPI UnexpectedWidePicker(LPOPENFILENAMEW dialog) {
    // Some system versions delegate the ANSI API to its public wide entry.
    // Preserve that real implementation only inside our observed ANSI call.
    if (pickerDrive.active) {
        Require(originalOpenFileW != nullptr, "real ANSI picker wide delegate exists");
        std::printf("PICKER_INTERNAL_W phase=%s flags=0x%08lx\n", phase, dialog ? dialog->Flags : 0ul);
        return originalOpenFileW(dialog);
    }
    Require(false, "unexpected wide file picker (expected native MME ANSI picker)");
    return FALSE;
}
void CALLBACK DriveRealPicker(HWND, UINT, UINT_PTR timer, DWORD) {
    if (!pickerDrive.active || timer != pickerDrive.timer || pickerDrive.insideTimer) return;
    pickerDrive.insideTimer = true;
    struct Leave { ~Leave() { pickerDrive.insideTimer = false; } } leave;
    if (GetTickCount64() >= pickerDrive.deadline) {
        std::fprintf(stderr, "PICKER_TIMEOUT phase=%s mode=real open_posted=%d cwd=%s\n",
            phase, pickerDrive.opened ? 1 : 0, CurrentDirectory().c_str());
        KillTimer(nullptr, timer);
        Require(false, "real ANSI file picker timed out");
    }
    HWND fileDialog = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM data) -> BOOL {
        if (IsWindowVisible(window)) {
            wchar_t visibleClass[64]{}, visibleTitle[256]{};
            GetClassNameW(window, visibleClass, 64); GetWindowTextW(window, visibleTitle, 256);
            std::printf("VISIBLE_WINDOW class=%s title=%s handle=%p\n", Utf8(visibleClass).c_str(),
                        Utf8(visibleTitle).c_str(), static_cast<void*>(window));
            originalShow(window, SW_HIDE);
            Require(!IsWindowVisible(window), "hide common dialog shown by the system without ShowWindow");
        }
        if (std::find(pickerDrive.previousWindows.begin(), pickerDrive.previousWindows.end(), window) !=
            pickerDrive.previousWindows.end()) return TRUE;
        wchar_t cls[32]{}; GetClassNameW(window, cls, static_cast<int>(std::size(cls)));
        if (std::wcscmp(cls, L"#32770") || !GetDlgItem(window, IDOK) ||
            (!GetDlgItem(window, edt1) && !GetDlgItem(window, cmb13))) return TRUE;
        HWND owner = GetWindow(window, GW_OWNER);
        for (unsigned depth = 0; owner && owner != pickerDrive.owner && depth < 16; ++depth)
            owner = GetWindow(owner, GW_OWNER);
        if (pickerDrive.owner && owner != pickerDrive.owner) return TRUE;
        auto* found = reinterpret_cast<HWND*>(data);
        Require(*found == nullptr, "one owned common-file dialog is present");
        *found = window;
        return TRUE;
    }, reinterpret_cast<LPARAM>(&fileDialog));
    if (!fileDialog || pickerDrive.opened) return;
    const HWND open = GetDlgItem(fileDialog, IDOK);
    if (!IsWindowEnabled(open)) return;
    HWND filename = GetDlgItem(fileDialog, edt1);
    if (!filename) filename = GetDlgItem(fileDialog, cmb13);
    Require(SetWindowTextW(filename, effectPath.c_str()) != FALSE,
            "enter requested FX in real file picker");
    pickerDrive.opened = true;
    std::printf("PICKER_OPEN phase=%s mode=real dialog=%p filename_control=%d path=%s cwd=%s\n",
        phase, static_cast<void*>(fileDialog), GetDlgCtrlID(filename),
        Utf8(effectPath.c_str()).c_str(), CurrentDirectory().c_str());
    Require(PostMessageW(fileDialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED),
                         reinterpret_cast<LPARAM>(open)) != FALSE,
            "press Open in real file picker");
}
BOOL WINAPI SelectEffect(LPOPENFILENAMEA dialog) {
    Require(assigning && dialog && dialog->lpstrFile, "file picker belongs to reassignment");
    ++pickerCalls;
    Environment("picker-enter");
    std::printf("PICKER phase=%s call=%u mode=%s flags=0x%08lx max_file=%lu initial_dir_hex=%s old_file_hex=%s\n",
        phase, pickerCalls, realPicker ? "real" : "injected", dialog->Flags, dialog->nMaxFile,
        Hex(dialog->lpstrInitialDir).c_str(), Hex(dialog->lpstrFile).c_str());
    Require(effectAnsi.size() + 1 < dialog->nMaxFile, "selected FX fits ANSI file buffer");
    std::memcpy(dialog->lpstrFile, effectAnsi.c_str(), effectAnsi.size() + 1);
    dialog->lpstrFile[effectAnsi.size() + 1] = '\0';
    if (realPicker) {
        Require(originalOpenFileA && !pickerDrive.active, "real ANSI picker trampoline is ready without reentry");
        pickerDrive = {};
        pickerDrive.owner = dialog->hwndOwner;
        EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM) -> BOOL {
            pickerDrive.previousWindows.push_back(window); return TRUE;
        }, 0);
        pickerDrive.deadline = GetTickCount64() + 12000;
        pickerDrive.active = true;
        pickerDrive.timer = SetTimer(nullptr, 0, 100, DriveRealPicker);
        Require(pickerDrive.timer != 0, "schedule real file picker interaction");
        // Leave all original OPENFILENAME flags and directory handling intact.
        const BOOL result = originalOpenFileA(dialog);
        const DWORD error = result ? 0 : CommDlgExtendedError();
        KillTimer(nullptr, pickerDrive.timer);
        pickerDrive.timer = 0; pickerDrive.active = false;
        Require(std::memchr(dialog->lpstrFile, '\0', dialog->nMaxFile) != nullptr,
                "real picker returned a terminated file path");
        std::wstring selected;
        const bool decoded = text_encoding::Decode(dialog->lpstrFile, CP_ACP, selected);
        std::printf("PICKER_RETURN phase=%s mode=real result=%d error=0x%08lx flags=0x%08lx path=%s path_hex=%s offset=%u extension=%u simulated_cwd_change=0\n",
            phase, result, error, dialog->Flags, decoded ? Utf8(selected.c_str()).c_str() : "(decode failed)",
            Hex(dialog->lpstrFile).c_str(), dialog->nFileOffset, dialog->nFileExtension);
        Environment("picker-return");
        Require(result != FALSE && pickerDrive.opened, "real ANSI file picker accepted Open");
        Require(decoded, "decode real picker result with its ANSI code page");
        std::error_code pathError;
        Require(fs::equivalent(fs::path(selected), effectPath, pathError) && !pathError,
                "real picker returned the requested FX file");
        return result;
    }
    const auto slash = effectAnsi.find_last_of("\\/");
    const auto dot = effectAnsi.find_last_of('.');
    dialog->nFileOffset = static_cast<WORD>(slash == std::string::npos ? 0 : slash + 1);
    dialog->nFileExtension = static_cast<WORD>(dot == std::string::npos ? 0 : dot + 1);
    if (dialog->lpstrFileTitle && dialog->nMaxFileTitle) {
        const auto title = effectAnsi.substr(dialog->nFileOffset);
        Require(title.size() < dialog->nMaxFileTitle, "FX title fits picker output");
        std::memcpy(dialog->lpstrFileTitle, title.c_str(), title.size() + 1);
    }
    // This simulates the selection's directory side effect; the mode label
    // distinguishes it from directory handling observed through the real API.
    const bool changeDirectory = !(dialog->Flags & OFN_NOCHANGEDIR);
    if (changeDirectory)
        Require(SetCurrentDirectoryW(effectPath.parent_path().c_str()) != FALSE,
                "simulate selected directory");
    std::printf("PICKER_RETURN phase=%s mode=injected path=%s offset=%u extension=%u simulated_cwd_change=%d\n",
        phase, Utf8(effectPath.c_str()).c_str(), dialog->nFileOffset,
        dialog->nFileExtension, changeDirectory ? 1 : 0);
    Environment("picker-return");
    return TRUE;
}

// ABI-only declarations match D3DX's public file-compile signature, without
// adding a dependency on legacy D3DX SDK headers to this diagnostic.
struct ShaderMacro { const char* name; const char* definition; };
using CreateEffectFile = HRESULT(WINAPI*)(IDirect3DDevice9*, LPCWSTR,
    const ShaderMacro*, void*, DWORD, void*, void**, void**);
CreateEffectFile originalCompile = nullptr;
HRESULT WINAPI ObserveCompile(IDirect3DDevice9* device, LPCWSTR path,
    const ShaderMacro* macros, void* include, DWORD flags, void* pool,
    void** effect, void** errors) {
    const unsigned call = ++compileCalls;
    std::printf("COMPILE phase=%s call=%u path=%s flags=0x%08lx cwd=%s\n",
        phase, call, Utf8(path).c_str(), flags, CurrentDirectory().c_str());
    if (macros) for (unsigned i = 0; i < 256 && macros[i].name; ++i)
        std::printf("MACRO call=%u name=%s value=%s\n", call, macros[i].name,
                    macros[i].definition ? macros[i].definition : "(null)");
    const HRESULT hr = originalCompile(device, path, macros, include, flags, pool, effect, errors);
    std::printf("COMPILE_RETURN phase=%s call=%u hr=0x%08lx effect=%p\n",
        phase, call, static_cast<unsigned long>(hr), effect ? *effect : nullptr);
    return hr;
}
void InstallCompileObserver() {
    auto& api = d3dx::Get();
    Require(api.Load(), "load production D3DX runtime");
    auto* entry = reinterpret_cast<void*>(GetProcAddress(api.module, "D3DXCreateEffectFromFileW"));
    Require(entry != nullptr, "find D3DXCreateEffectFromFileW");
    Require(MH_CreateHook(entry, &ObserveCompile, reinterpret_cast<void**>(&originalCompile)) == MH_OK &&
            MH_EnableHook(entry) == MH_OK, "observe real effect file compilation");
}

HMENU MenuWithCommand(HMENU menu, UINT command) {
    if (!menu) return nullptr;
    for (int i = 0; i < GetMenuItemCount(menu); ++i) {
        if (GetMenuItemID(menu, i) == command) return menu;
        if (auto found = MenuWithCommand(GetSubMenu(menu, i), command)) return found;
    }
    return nullptr;
}
void DisableMappingAutoSave(MMDApp* app) {
    auto menu = MenuWithCommand(GetMenu(app->MainWindow()), 40022);
    Require(menu != nullptr, "find native MME mapping autosave setting");
    const UINT before = GetMenuState(menu, 40022, MF_BYCOMMAND);
    if (before & MF_CHECKED) SendMessageW(app->MainWindow(), WM_COMMAND, 40022, 0);
    menu = MenuWithCommand(GetMenu(app->MainWindow()), 40022);
    Require(menu && !(GetMenuState(menu, 40022, MF_BYCOMMAND) & MF_CHECKED),
            "disable native MME mapping autosave before loading user model");
    std::printf("MME mapping_autosave=0 previous_checked=%d\n", (before & MF_CHECKED) ? 1 : 0);
}
void Pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        Require(message.message != WM_QUIT, "unexpected WM_QUIT");
        TranslateMessage(&message); DispatchMessageW(&message);
    }
}
std::wstring Row(HWND list, int row, int column, LPARAM* identity = nullptr) {
    wchar_t text[2048]{};
    LVITEMW item{}; item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = row; item.iSubItem = column;
    item.pszText = text; item.cchTextMax = static_cast<int>(std::size(text));
    Require(SendMessageW(list, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&item)) != 0,
            "read real MME assignment row");
    if (identity) *identity = item.lParam;
    return text;
}
void CALLBACK DriveEditor(HWND, UINT, UINT_PTR timer, DWORD) {
    if (insideEditorTimer) return;
    insideEditorTimer = true;
    struct Leave { ~Leave() { insideEditorTimer = false; } } leave;
    Require(++editorTicks < 100, "MME editor transaction timed out");
    if (editorStep == 2) return;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM) -> BOOL {
        const HMENU menu = GetMenu(window);
        if (menu && MenuWithCommand(menu, 40028) && MenuWithCommand(menu, 40007)) editor = window;
        return TRUE;
    }, 0);
    if (!editor) return;
    HWND list = nullptr;
    EnumChildWindows(editor, [](HWND child, LPARAM data) -> BOOL {
        wchar_t cls[64]{}; GetClassNameW(child, cls, 64);
        if (!_wcsicmp(cls, WC_LISTVIEWW) && ListView_GetItemCount(child) > 1) {
            *reinterpret_cast<HWND*>(data) = child; return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&list));
    if (!list) return;
    int selected = -1;
    for (int row = 0; row < ListView_GetItemCount(list); ++row) {
        LPARAM identity = 0;
        const auto name = Row(list, row, 0, &identity), fx = Row(list, row, 1);
        std::printf("ROW phase=%s row=%d identity=0x%llx name=%s effect=%s\n", phase,
            row, static_cast<unsigned long long>(identity), Utf8(name.c_str()).c_str(), Utf8(fx.c_str()).c_str());
        if (!_wcsicmp(name.c_str(), modelPath.filename().c_str())) selected = row;
    }
    Require(selected >= 0, "find exact model filename in native MME editor");
    ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(list, selected, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    Require(ListView_GetSelectedCount(list) == 1, "select exactly one model root");
    editorStep = 1;
    const unsigned previous = pickerCalls;
    assigning = true;
    SendMessageW(editor, WM_COMMAND, 40007, 0);
    assigning = false;
    Require(pickerCalls == previous + 1, "real Set Effect command invokes one ANSI picker");
    std::printf("ROW_ASSIGNED phase=%s row=%d effect=%s\n", phase, selected,
                Utf8(Row(list, selected, 1).c_str()).c_str());
    Environment("assignment-command-return");
    if (captureWhileEditorOpen) captureWhileEditorOpen();
    // Exercise MME's own Close command. Hidden-window instrumentation makes
    // IsWindowVisible unsuitable for detecting completion, so wait for this
    // synchronous command to return rather than treating hidden as closed.
    editorStep = 2;
    SendMessageW(editor, WM_COMMAND, 40011, 0);
    editorDone = true;
    KillTimer(nullptr, timer);
}
void Reassign(MMDApp* app) {
    editor = nullptr; editorDone = false; editorTicks = editorStep = 0;
    Environment("editor-open");
    const UINT_PTR timer = SetTimer(nullptr, 0, 100, DriveEditor);
    Require(timer != 0, "schedule real MME editor interaction");
    SendMessageW(app->MainWindow(), WM_COMMAND, 40005, 0);
    const ULONGLONG deadline = GetTickCount64() + 12000;
    while (!editorDone) {
        Require(GetTickCount64() < deadline, "editor closure timed out");
        Pump();
        if (!editorDone) MsgWaitForMultipleObjectsEx(0, nullptr, 25, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    KillTimer(nullptr, timer);
    Pump(); Environment("editor-closed");
}

void BoneState(MMDApp* app, bool all) {
    if (!app->SelectedModel()) {
        std::printf("BONE phase=%s unavailable=no-model\n", phase);
        return;
    }
    auto* model = mdl::Mdl(app->SelectedModel());
    for (unsigned i = 0; i < model->boneCount; ++i) {
        if (!all && static_cast<int>(i) != controlBone) continue;
        float world[16]{}; ExpGetPmdBoneWorldMat(world, 0, static_cast<int>(i));
        const auto& bone = model->boneTable[i];
        std::printf("BONE phase=%s index=%u name=%s bytes=%s origin=(%.9g,%.9g,%.9g) trans=(%.9g,%.9g,%.9g) world=(%.9g,%.9g,%.9g)\n",
            phase, i, Utf8(text_encoding::BoneName(bone, false).c_str()).c_str(), Hex(ExpGetPmdBoneName(0, i)).c_str(),
            bone.position[0], bone.position[1], bone.position[2], bone.trans[0], bone.trans[1], bone.trans[2], world[12], world[13], world[14]);
    }
}
void FixedFrame(MMDApp* app) {
    app->PlaybackActive() = 0; app->state.currentFrame = 0;
    app->state.optflag[0] = 1; app->EditMode() = ViewportEditMode::Camera;
    app->CameraReferenceMode() = CameraAttachmentReference::None;
    app->CameraParentModel() = -1;
    app->CameraPositionX() = 0; app->CameraPositionY() = 10; app->CameraPositionZ() = 0;
    app->CameraPitch() = app->CameraYaw() = app->CameraRoll() = 0;
    app->CameraDistance() = -45; app->CameraFov() = 30; app->CameraPerspective() = 0;
    app->ViewOffsetX() = app->ViewOffsetY() = 0;
    app->GroundGridEnabled() = 0; app->FpsOverlayEnabled() = 0;
    app->state.blackBackgroundEnabled = 1;
    if (controlBone >= 0) {
        auto* bytes = app->SelectedModel();
        if (!bytes || static_cast<unsigned>(controlBone) >= mdl::Mdl(bytes)->boneCount) {
            std::printf("CONTROLLER phase=%s unavailable=missing-model-or-bone fallback_default_pose=1\n", phase);
            return;
        }
        auto& bone = mdl::Bones(bytes)[controlBone];
        bone.trans[0] = .3f - bone.position[0];
        ModelApplyMorphs(bytes);
        SetPhysicsMode(bytes, 0, app->ModelSlots(), 0);
        SetPhysicsMode(bytes, 1, app->ModelSlots(), 0);
    }
}
void Frames(MMDApp* app, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        Pump(); FixedFrame(app);
        app->state.messageSeen = 1; app->DeltaTime() = 1.f / 30;
        FrameDriver(app);
    }
    BoneState(app, false);
    std::printf("FRAMES phase=%s count=%u timeline=%d camera=(0,10,0,-45,0,0,0) fov=30 shadow=%d\n",
                phase, count, app->state.currentFrame, app->SelfShadowMode());
}

struct Target {
    MMDApp* app;
    ComPtr<IDirect3DSurface9> oldTarget, oldDepth, cachedTarget, cachedDepth, color, depth;
    int oldWidth, oldHeight;
    explicit Target(MMDApp* value) : app(value), oldWidth(value->RenderWidth()), oldHeight(value->RenderHeight()) {
        auto* renderer = app->Renderer(); auto* device = renderer->device;
        Require(SUCCEEDED(device->GetRenderTarget(0, &oldTarget)), "retain original render target");
        Require(SUCCEEDED(device->GetDepthStencilSurface(&oldDepth)), "retain original depth");
        Require(SUCCEEDED(device->CreateRenderTarget(kWidth, kHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE,
            0, FALSE, &color, nullptr)), "create diagnostic render target");
        Require(SUCCEEDED(device->CreateDepthStencilSurface(kWidth, kHeight, D3DFMT_D24S8, D3DMULTISAMPLE_NONE,
            0, TRUE, &depth, nullptr)), "create diagnostic depth");
        cachedTarget = renderer->backbufferSurface; cachedDepth = renderer->depthStencilSurface;
        if (renderer->backbufferSurface) renderer->backbufferSurface->Release();
        if (renderer->depthStencilSurface) renderer->depthStencilSurface->Release();
        renderer->backbufferSurface = color.Get(); color->AddRef();
        renderer->depthStencilSurface = depth.Get(); depth->AddRef();
        app->RenderWidth() = kWidth; app->RenderHeight() = kHeight;
        Require(SUCCEEDED(device->SetRenderTarget(0, color.Get())) &&
                SUCCEEDED(device->SetDepthStencilSurface(depth.Get())), "bind diagnostic surfaces");
        const D3DVIEWPORT9 view{0, 0, kWidth, kHeight, 0, 1};
        Require(SUCCEEDED(device->SetViewport(&view)), "set diagnostic viewport");
    }
    ~Target() {
        auto* renderer = app->Renderer();
        renderer->backbufferSurface->Release(); renderer->depthStencilSurface->Release();
        renderer->backbufferSurface = cachedTarget.Detach(); renderer->depthStencilSurface = cachedDepth.Detach();
        renderer->device->SetRenderTarget(0, oldTarget.Get());
        renderer->device->SetDepthStencilSurface(oldDepth.Get());
        app->RenderWidth() = oldWidth; app->RenderHeight() = oldHeight;
    }
    std::vector<unsigned char> Capture(const wchar_t* filename) {
        auto* device = app->Renderer()->device;
        ComPtr<IDirect3DSurface9> readback;
        Require(SUCCEEDED(device->CreateOffscreenPlainSurface(kWidth, kHeight, D3DFMT_A8R8G8B8,
            D3DPOOL_SYSTEMMEM, &readback, nullptr)), "allocate BGRA readback");
        Require(SUCCEEDED(device->GetRenderTargetData(color.Get(), readback.Get())), "read actual render target");
        D3DLOCKED_RECT lock{};
        Require(SUCCEEDED(readback->LockRect(&lock, nullptr, D3DLOCK_READONLY)), "lock captured pixels");
        std::vector<unsigned char> pixels(size_t(kWidth) * kHeight * 4);
        for (UINT y = 0; y < kHeight; ++y)
            std::memcpy(pixels.data() + size_t(y) * kWidth * 4,
                        static_cast<const unsigned char*>(lock.pBits) + size_t(y) * lock.Pitch, kWidth * 4);
        Require(SUCCEEDED(readback->UnlockRect()), "unlock captured pixels");
        std::ofstream file(outputPath / filename, std::ios::binary);
        file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
        Require(bool(file), "write full BGRA image");
        size_t nonblack = 0, varied = 0;
        unsigned maximum = 0;
        for (size_t i = 0; i < pixels.size(); i += 4) {
            nonblack += (pixels[i] || pixels[i + 1] || pixels[i + 2]) ? 1 : 0;
            varied += std::memcmp(pixels.data(), pixels.data() + i, 3) != 0 ? 1 : 0;
            maximum = (std::max)(maximum, unsigned((std::max)({pixels[i], pixels[i + 1], pixels[i + 2]})));
        }
        std::printf("CAPTURE phase=%s path=%s width=%u height=%u format=BGRA8 nonblack=%zu varied=%zu max_rgb=%u\n",
            phase, Utf8((outputPath / filename).c_str()).c_str(), kWidth, kHeight, nonblack, varied, maximum);
        Require(nonblack > 0 && varied > 0, "capture contains nonempty image (not an effect correctness assertion)");
        return pixels;
    }
};
void Difference(const char* label, const std::vector<unsigned char>& before,
                const std::vector<unsigned char>& after) {
    Require(before.size() == after.size(), "matching capture dimensions");
    size_t changed = 0; unsigned maximum = 0; unsigned long long sum = 0;
    for (size_t i = 0; i < before.size(); i += 4) {
        bool pixelChanged = false;
        for (unsigned channel = 0; channel < 3; ++channel) {
            const unsigned delta = static_cast<unsigned>(std::abs(int(before[i + channel]) - int(after[i + channel])));
            pixelChanged |= delta != 0; maximum = (std::max)(maximum, delta); sum += delta;
        }
        changed += pixelChanged ? 1 : 0;
    }
    std::printf("DIFFERENCE label=%s changed_rgb_pixels=%zu max_delta=%u mean_abs_rgb=%.9g equality_asserted=0\n",
        label, changed, maximum, double(sum) / (before.size() / 4 * 3));
}
}

int wmain(int argc, wchar_t** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    wchar_t realPickerSetting[8]{};
    realPicker = GetEnvironmentVariableW(L"MMS_MME_REAL_PICKER", realPickerSetting,
        static_cast<DWORD>(std::size(realPickerSetting))) == 1 && realPickerSetting[0] == L'1';
    Require(argc == 4 || argc == 5, "usage: MikuMikuDance.exe model.pmx effect.fx output [0|2]");
    modelPath = fs::absolute(argv[1]).lexically_normal(); modelPath.make_preferred();
    effectPath = fs::absolute(argv[2]).lexically_normal(); effectPath.make_preferred();
    outputPath = fs::absolute(argv[3]).lexically_normal(); outputPath.make_preferred();
    Require(argc == 4 || std::wcscmp(argv[4], L"0") == 0 || std::wcscmp(argv[4], L"2") == 0,
            "shadow must be 0 or 2");
    const int shadow = argc == 5 ? _wtoi(argv[4]) : 0;
    Require(fs::is_regular_file(modelPath) && fs::is_regular_file(effectPath), "model and FX exist");
    Require(text_encoding::EncodeExact(effectPath.c_str(), CP_ACP, effectAnsi), "FX path is exactly representable by ANSI picker");
    fs::create_directories(outputPath);
    std::printf("INPUT model=%s effect=%s output=%s shadow=%d picker_mode=%s\n", Utf8(modelPath.c_str()).c_str(),
        Utf8(effectPath.c_str()).c_str(), Utf8(outputPath.c_str()).c_str(), shadow, realPicker ? "real" : "injected");
    wchar_t desktopName[256]{}; DWORD desktopBytes = 0;
    Require(GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME,
                desktopName, sizeof(desktopName), &desktopBytes) != FALSE,
            "read actual test desktop");
    wchar_t hiddenDefaultSetting[4]{};
    const bool hiddenDefault = GetEnvironmentVariableW(L"MMS_MME_HIDDEN_DEFAULT", hiddenDefaultSetting,
        static_cast<DWORD>(std::size(hiddenDefaultSetting))) == 1 && hiddenDefaultSetting[0] == L'1';
    if (hiddenDefault) {
        Require(!realPicker && _wcsicmp(desktopName, L"Default") == 0,
                "hidden default desktop supports only injected file selection");
        std::printf("HIDDEN_DEFAULT_DESKTOP name=%s real_picker=0\n", Utf8(desktopName).c_str());
    } else {
        Require(std::wcsncmp(desktopName, L"MMS_REPAIR_", 11) == 0,
                "run real-dialog diagnostic on a dedicated MMS_REPAIR_ private desktop");
        std::printf("PRIVATE_DESKTOP name=%s\n", Utf8(desktopName).c_str());
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    Require(MH_Initialize() == MH_OK, "initialize diagnostic hooks");
    Require(MH_CreateHook(&ShowWindow, &Hide, reinterpret_cast<void**>(&originalShow)) == MH_OK, "hide windows");
    Require(MH_CreateHook(&MessageBoxW, &UnexpectedWide, nullptr) == MH_OK &&
            MH_CreateHook(&MessageBoxA, &UnexpectedAnsi, nullptr) == MH_OK, "guard message boxes");
    Require(MH_CreateHook(&GetOpenFileNameA, &SelectEffect, reinterpret_cast<void**>(&originalOpenFileA)) == MH_OK &&
            MH_CreateHook(&GetOpenFileNameW, &UnexpectedWidePicker, reinterpret_cast<void**>(&originalOpenFileW)) == MH_OK,
            "observe native file selection with real or injected picker mode");
    Require(MH_EnableHook(MH_ALL_HOOKS) == MH_OK, "enable diagnostic hooks");
    const HHOOK guard = SetWindowsHookExW(WH_CBT, HideCreated, nullptr, GetCurrentThreadId());
    Require(guard != nullptr, "prevent top-level window activation");
    auto* app = new MMDApp; g_Block = app;
    app->state = MMDAppState{}; app->InitDefaults();
    Require(InitMainWindowAndD3D(app, GetModuleHandleW(nullptr), SW_HIDE), "start production host");
    Require(GetModuleHandleW(L"MMEffect.dll") && GetModuleHandleW(L"MMHack.dll"), "real native MME loaded");
    InstallCompileObserver();
    phase = "initialize-mme"; Frames(app, 1);
    DisableMappingAutoSave(app);
    phase = "load-model"; Environment("before-load");
    LoadModelFile(app, modelPath.c_str(), false);
    Require(app->SelectedModel() != nullptr, "load real PMX");
    const auto* model = mdl::Mdl(app->SelectedModel());
    for (unsigned i = 0; i < model->boneCount; ++i)
        if (std::strcmp(model->boneTable[i].name, "\x83\x41\x83\x6A\x83\x81\x90\xA7\x8C\xE4") == 0)
            controlBone = static_cast<int>(i);
    std::printf("CONTROLLER index=%d requested_x=0.3 fallback_default_pose=%d\n", controlBone, controlBone < 0 ? 1 : 0);
    BoneState(app, true); Environment("after-load");
    app->SelfShadowMode() = static_cast<unsigned char>(shadow);
    {
        Target target(app);
        phase = "default-companion"; Frames(app, 8); const auto baseline = target.Capture(L"default.bgra");
        captureWhileEditorOpen = [&] {
            std::printf("EDITOR_DRAW phase=%s host_enabled=%d editor_enabled=%d\n", phase,
                        IsWindowEnabled(app->MainWindow()), IsWindowEnabled(editor));
            Frames(app, 1); const auto first = target.Capture(L"assigned-editor-first.bgra");
            Difference("default-to-editor-first", baseline, first);
            Frames(app, 7); const auto steady = target.Capture(L"assigned-editor-steady.bgra");
            Difference("default-to-editor-steady", baseline, steady);
        };
        phase = "manual-first"; Reassign(app); captureWhileEditorOpen = {};
        Frames(app, 1); const auto closedFirst = target.Capture(L"assigned-closed-first.bgra");
        Difference("default-to-closed-first", baseline, closedFirst);
        Frames(app, 7); const auto assigned = target.Capture(L"assigned.bgra");
        Difference("default-to-assigned", baseline, assigned);
        phase = "reload-all"; Environment("before-reload");
        SendMessageW(app->MainWindow(), WM_COMMAND, 40004, 0);
        Frames(app, 8); const auto reloaded = target.Capture(L"reloaded.bgra");
        Difference("assigned-to-reloaded", assigned, reloaded); Difference("default-to-reloaded", baseline, reloaded);
        phase = "manual-second"; Reassign(app); Frames(app, 8); const auto assignedAgain = target.Capture(L"assigned-again.bgra");
        Difference("reloaded-to-assigned-again", reloaded, assignedAgain);
        Difference("assigned-to-assigned-again", assigned, assignedAgain);
    }
    Require(!IsWindowVisible(app->MainWindow()), "host remains hidden");
    phase = "shutdown";
    ShutdownCleanup(app); delete app; g_Block = nullptr;
    UnhookWindowsHookEx(guard); MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
    std::printf("DIAGNOSTIC_COMPLETE picker_calls=%u compile_calls=%u equality_asserted=0\n", pickerCalls, compileCalls);
    return 0;
}
