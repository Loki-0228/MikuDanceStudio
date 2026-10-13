// MAINTENANCE: regression-keep; hidden real-MME display and material-order checks.
// Exercise the real mapping dialog without displaying or activating a window.
#define NOMINMAX
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/viewport_render_scale.hpp"
#include "mikudancestudio/d3d12_native.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/text_encoding.hpp"
#include "mikudancestudio/ui_language.hpp"
#include "mikudancestudio/effect_display.hpp"
#include "mikudancestudio/model.hpp"
#include <MinHook.h>
#include <commctrl.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <wrl/client.h>

using namespace mikudancestudio;
namespace {
void Check(bool ok, const char* label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) std::exit(2);
}
decltype(&ShowWindow) originalShow;
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
int WINAPI Unexpected(HWND, LPCWSTR text, LPCWSTR, UINT) {
    std::fwprintf(stderr, L"FAIL unexpected dialog: %ls\n", text); std::exit(2);
}
int WINAPI UnexpectedAnsi(HWND, LPCSTR text, LPCSTR, UINT) {
    std::string utf8; text_encoding::EncodeExact(text_encoding::Display(text).c_str(), CP_UTF8, utf8);
    std::fprintf(stderr, "FAIL unexpected dialog: %s\n", utf8.c_str()); std::exit(2);
}
int tick = 0, rowsSeen = 0;
bool done = false;
bool bilingualFixture = false;
HWND editor = nullptr;
using DrawIndexed = HRESULT (WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
DrawIndexed originalDraw = nullptr;
std::vector<unsigned> drawnMaterials;
using Present = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
Present originalPresent = nullptr;
bool captureRequested = false;
std::vector<std::uint32_t> capturedPixels;
unsigned captureWidth = 0, captureHeight = 0;
std::filesystem::path captureDirectory;
HRESULT WINAPI ObservePresent(IDirect3DDevice9* device, const RECT* source, const RECT* destination,
                              HWND window, const RGNDATA* region) {
    if (captureRequested) {
        Microsoft::WRL::ComPtr<IDirect3DSurface9> back, resolved, cpu;
        Check(SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)), "get actual pre-Present backbuffer");
        D3DSURFACE_DESC desc{}; Check(SUCCEEDED(back->GetDesc(&desc)), "read backbuffer description");
        Check(desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8, "BGRA comparison format");
        auto* surface = back.Get();
        if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
            Check(SUCCEEDED(device->CreateRenderTarget(desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE,
                0, FALSE, &resolved, nullptr)) && SUCCEEDED(device->StretchRect(surface, nullptr, resolved.Get(), nullptr, D3DTEXF_NONE)),
                "resolve actual backbuffer without rebinding");
            surface = resolved.Get();
        }
        Check(SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
            D3DPOOL_SYSTEMMEM, &cpu, nullptr)) && SUCCEEDED(device->GetRenderTargetData(surface, cpu.Get())), "capture actual frame pixels");
        D3DLOCKED_RECT lock{}; Check(SUCCEEDED(cpu->LockRect(&lock, nullptr, D3DLOCK_READONLY)), "lock comparison pixels");
        captureWidth = desc.Width; captureHeight = desc.Height;
        capturedPixels.resize(std::size_t(desc.Width) * desc.Height);
        for (unsigned y = 0; y < desc.Height; ++y) {
            const auto* row = reinterpret_cast<const DWORD*>(static_cast<const unsigned char*>(lock.pBits) + y * lock.Pitch);
            for (unsigned x = 0; x < desc.Width; ++x) capturedPixels[std::size_t(y) * desc.Width + x] = row[x] & 0xFFFFFF;
        }
        cpu->UnlockRect(); captureRequested = false;
    }
    return originalPresent(device, source, destination, window, region);
}
HRESULT WINAPI ObserveDraw(IDirect3DDevice9* device, D3DPRIMITIVETYPE type, INT base, UINT min,
                            UINT vertices, UINT start, UINT primitives) {
    if (g_Block && g_Block->ActiveRenderObject() == g_Block->SelectedModel() &&
        (g_Block->ActiveRenderPass() == AccessoryRenderPass::FixedFunction ||
         g_Block->ActiveRenderPass() == AccessoryRenderPass::Effect)) {
        const auto material = mdl::Mdl(g_Block->SelectedModel())->toonShared;
        if (drawnMaterials.empty() || drawnMaterials.back() != material) drawnMaterials.push_back(material);
    }
    return originalDraw(device, type, base, min, vertices, start, primitives);
}
void CheckAlphaOrder(MMDApp* app) {
    auto** vtable = *reinterpret_cast<void***>(app->Renderer()->device);
    auto* target = vtable[82]; // IDirect3DDevice9::DrawIndexedPrimitive
    Check(MH_CreateHook(target, &ObserveDraw, reinterpret_cast<void**>(&originalDraw)) == MH_OK &&
          MH_EnableHook(target) == MH_OK, "observe actual D3D material draws");
    Check(MH_CreateHook(vtable[17], &ObservePresent, reinterpret_cast<void**>(&originalPresent)) == MH_OK &&
          MH_EnableHook(vtable[17]) == MH_OK, "observe actual pre-Present pixels");
    app->GroundGridEnabled() = false; app->GroundShadowEnabled() = false;
    for (int shadow : {0, 2}) {
        app->SelfShadowMode() = shadow;
        std::vector<std::uint32_t> firstOff;
        unsigned run = 0;
        for (auto precision : {TransparentSortPrecision::Off, TransparentSortPrecision::Low, TransparentSortPrecision::Off}) {
            app->TransparentMaterialSortPrecision() = precision;
            drawnMaterials.clear();
            captureRequested = true;
            app->DeltaTime() = 1.f / 30; app->state.messageSeen = 10; FrameDriver(app);
            std::printf("DRAW shadow=%d precision=%d:", shadow, int(precision));
            for (auto material : drawnMaterials) std::printf(" %u", material);
            std::puts("");
            const std::vector<unsigned> expected = MMD_ORIGINAL_PROFILE
                ? std::vector<unsigned>{0, 1} : std::vector<unsigned>{1, 0};
            Check(drawnMaterials == expected, "material draws follow the profile's native pass order with sorting off and on");
            Check(!captureRequested && !capturedPixels.empty(), "capture came from a real Present");
            if (run == 0) firstOff = capturedPixels;
            if (run == 2) Check(firstOff == capturedPixels, "On to Off preserves every frame RGB pixel");
            std::uint64_t hash = 14695981039346656037ull;
            for (auto pixel : capturedPixels) hash = (hash ^ pixel) * 1099511628211ull;
            std::printf("PIXELS shadow=%d run=%u precision=%d width=%u height=%u hash=%016llx\n",
                shadow, run, int(precision), captureWidth, captureHeight, static_cast<unsigned long long>(hash));
            if (!captureDirectory.empty()) {
                std::filesystem::create_directories(captureDirectory);
                const auto name = L"shadow-" + std::to_wstring(shadow) + L"-run-" + std::to_wstring(run) + L".bgra";
                std::ofstream file(captureDirectory / name, std::ios::binary);
                file.write(reinterpret_cast<const char*>(capturedPixels.data()), capturedPixels.size() * 4);
                Check(bool(file), "write actual comparison pixels");
            }
            ++run;
        }
    }
    MH_DisableHook(target);
    MH_DisableHook(vtable[17]);
}
std::uint64_t PaintHash(HWND list) {
    RECT rect{}; GetClientRect(list, &rect);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rect.right; info.bmiHeader.biHeight = -rect.bottom;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    HDC dc = CreateCompatibleDC(nullptr); void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    Check(bitmap && pixels, "allocate control readback");
    HGDIOBJ old = SelectObject(dc, bitmap);
    SendMessageW(list, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
    GdiFlush();
    std::uint64_t hash = 14695981039346656037ull;
    const auto* bytes = static_cast<unsigned char*>(pixels);
    for (int i = 0; i < rect.right * rect.bottom * 4; ++i) hash = (hash ^ bytes[i]) * 1099511628211ull;
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
    return hash;
}
void CheckDisplay(HWND list) {
    wchar_t original[1024]{};
    LVITEMW item{}; item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = 2;
    item.pszText = original; item.cchTextMax = 1024;
    Check(SendMessageW(list, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&item)), "read original material item");
    const auto identity = item.lParam;
    const std::wstring stored = original;
    auto* model = mdl::Mdl(g_Block->SelectedModel());
    const auto native = text_encoding::MaterialName(*model, 0, false);
    const auto english = text_encoding::MaterialName(*model, 0, true);
    SetModelInfoLanguagePreference(kModelInfoLanguageEnglish);
    SetModelNameAutoTranslation(false);
    Check(EffectListDisplayLabel(g_Block, list, 1) == L"Display Model", "MME model label uses metadata");
    Check(EffectListDisplayLabel(g_Block, list, 2) == L"  `-- 0: CustomArm", "English material metadata is displayed");
    const auto rawHash = PaintHash(list);
    SendMessageW(g_Block->MainWindow(), WM_COMMAND, 311, 0);
    Check(gModelNameAutoTranslation, "enable translation through actual menu");
    Check(EffectListDisplayLabel(g_Block, list, 2) == L"  `-- 0: arm_L", "MME displays dictionary translation");
    const auto translatedHash = PaintHash(list);
    Check(rawHash != translatedHash, "actual MME control pixels change with translated labels");
    SendMessageW(g_Block->MainWindow(), WM_COMMAND, 311, 0);
    Check(!gModelNameAutoTranslation, "translation remains switchable with PMX and MME loaded");
    Check(PaintHash(list) == rawHash, "disabling translation restores original display pixels");
    SetModelInfoLanguagePreference(kModelInfoLanguageAuto);
    g_Block->EnglishUI() = kUiChinese;
    Check(EffectListDisplayLabel(g_Block, list, 2) == L"  `-- 0: 左腕", "Chinese auto preference uses Japanese material name");
    wchar_t finalText[1024]{}; item.pszText = finalText;
    SendMessageW(list, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&item));
    Check(stored == finalText && item.lParam == identity, "MME stored text and assignment identity remain unchanged");
    Check(text_encoding::MaterialName(*model, 0, false) == native &&
          text_encoding::MaterialName(*model, 0, true) == english,
          "raw model material names remain unchanged");
    Check((GetMenuState(GetMenu(g_Block->MainWindow()), 311, MF_BYCOMMAND) & (MF_GRAYED | MF_DISABLED)) == 0,
          "translation preference is enabled after loading PMX with MME");
    std::printf("DISPLAY hashes original=%llx translated=%llx\n", rawHash, translatedHash);
}
void CALLBACK Probe(HWND, UINT, UINT_PTR timer, DWORD) {
    ++tick;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM) -> BOOL {
        HMENU menu = GetMenu(window);
        if (menu && GetMenuState(menu, 40028, MF_BYCOMMAND) != UINT(-1) &&
            GetMenuState(menu, 40007, MF_BYCOMMAND) != UINT(-1)) editor = window;
        return TRUE;
    }, 0);
    if (editor) {
        EnumChildWindows(editor, [](HWND child, LPARAM) -> BOOL {
            wchar_t cls[64]{}; GetClassNameW(child, cls, 64);
            if (_wcsicmp(cls, WC_LISTVIEWW)) return TRUE;
            const int count = ListView_GetItemCount(child);
            std::printf("LIST tick=%d id=%d rows=%d\n", tick, GetDlgCtrlID(child), count);
            for (int row = 0; row < count; ++row) {
                wchar_t text[1024]{};
                LVITEMW item{}; item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_INDENT;
                item.iItem = row; item.pszText = text; item.cchTextMax = 1024;
                SendMessageW(child, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&item));
                std::string utf8; text_encoding::EncodeExact(text, CP_UTF8, utf8);
                std::printf("ROW %d indent=%d data=%llx text=%s\n", row, item.iIndent,
                    static_cast<unsigned long long>(item.lParam), utf8.c_str());
                ++rowsSeen;
            }
            if (tick == 1 && count > 1) ListView_SetItemState(child, 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            if (tick == 2 && bilingualFixture) { Check(count == 3, "fixture has one expanded material"); CheckDisplay(child); }
            return TRUE;
        }, 0);
        if (tick == 1) PostMessageW(editor, WM_COMMAND, 40006, 0);
        else { PostMessageW(editor, WM_COMMAND, 40011, 0); done = true; KillTimer(nullptr, timer); }
    }
    if (tick > 10) { std::puts("FAIL mapping dialog not found"); std::exit(2); }
}
#include "viewport_render_scale_scene_checks.hpp"
}
int wmain(int argc, wchar_t** argv) {
    Check(argc >= 2 && argc <= 4, "model path supplied");
    bilingualFixture = argc == 3 && std::wcscmp(argv[2], L"--bilingual") == 0;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    Check(MH_Initialize() == MH_OK, "initialize hidden-window hook");
    Check(MH_CreateHook(&ShowWindow, &Hide, reinterpret_cast<void**>(&originalShow)) == MH_OK, "hook ShowWindow");
    Check(MH_CreateHook(&MessageBoxW, &Unexpected, nullptr) == MH_OK, "guard unexpected dialogs");
    Check(MH_CreateHook(&MessageBoxA, &UnexpectedAnsi, nullptr) == MH_OK, "guard legacy dialogs");
    Check(MH_EnableHook(MH_ALL_HOOKS) == MH_OK, "enable hidden-window guards");
    HHOOK guard = SetWindowsHookExW(WH_CBT, HideCreated, nullptr, GetCurrentThreadId());
    Check(guard != nullptr, "prevent window activation");
    auto* app = new MMDApp; g_Block = app;
    app->state = MMDAppState{}; app->InitDefaults();
    Check(InitMainWindowAndD3D(app, GetModuleHandleW(nullptr), SW_HIDE), "start production host");
    wchar_t native[4]{};
    const bool nativeOrder = GetEnvironmentVariableW(L"MMS_MME_ORDER_NATIVE", native, 4) == 1 && native[0] == L'1';
    Check(nativeOrder ? !GetModuleHandleW(L"MMEffect.dll") && !GetModuleHandleW(L"MMHack.dll") :
        GetModuleHandleW(L"MMEffect.dll") && GetModuleHandleW(L"MMHack.dll"),
        nativeOrder ? "native baseline excludes MME DLLs" : "real MME DLLs loaded");
    LoadModelFile(app, argv[1], false);
    Check(app->SelectedModel() != nullptr, "load textured PMX");
    for (int i = 0; i < 3; ++i) { app->DeltaTime() = 1.f / 30; app->state.messageSeen = 10; FrameDriver(app); }
    if (argc == 4 && std::wcscmp(argv[2], L"--viewport-scale") == 0) {
        CheckViewportRenderScale(app, argv[3]);
        ShutdownCleanup(app); delete app; g_Block = nullptr; UnhookWindowsHookEx(guard); return 0;
    }
    if (argc >= 3 && std::wcscmp(argv[2], L"--alpha-order") == 0) {
        if (argc == 4) captureDirectory = argv[3];
        CheckAlphaOrder(app);
        ShutdownCleanup(app); delete app; g_Block = nullptr; UnhookWindowsHookEx(guard); return 0;
    }
    Check(SetTimer(nullptr, 0, 100, Probe) != 0, "inspect dialog asynchronously");
    SendMessageW(app->MainWindow(), WM_COMMAND, 40005, 0);
    while (!done) { MSG message{}; if (GetMessageW(&message, nullptr, 0, 0) <= 0) break; TranslateMessage(&message); DispatchMessageW(&message); }
    Check(done && rowsSeen > 1, "inspect actual expanded material rows");
    Check(!IsWindowVisible(app->MainWindow()), "host stays hidden");
    ShutdownCleanup(app); delete app; g_Block = nullptr;
    UnhookWindowsHookEx(guard);
    return 0;
}
