#include "mikudancestudio/viewport_render_scale.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/frame_presentation.hpp"
#include "mikudancestudio/ui_translation.hpp"
#include "mikudancestudio/build_profile.hpp"
#include <CommCtrl.h>
#include <algorithm>
#include <cwchar>

namespace mikudancestudio::viewport_render_scale {
namespace {
const wchar_t* Text(MMDApp* app, const wchar_t* zh, const wchar_t* en, const wchar_t* ja) {
    return app->EnglishUI() == 2 ? zh : app->EnglishUI() == 0 ? ja : en;
}
const wchar_t* Title(MMDApp* app) {
    return Text(app, L"视口渲染比例", L"Viewport resolution scale", L"描画解像度の倍率");
}
void WritePercent(HWND dialog, double percent) {
    wchar_t text[64]{};
    swprintf_s(text, L"%.12g", percent);
    SetDlgItemTextW(dialog, Percent, text);
    SendDlgItemMessageW(dialog, Slider, TBM_SETPOS, TRUE, LPARAM(std::clamp(percent, 25.0, 100.0)));
}
bool ReadPercent(HWND dialog, double& percent) {
    wchar_t text[128]{};
    GetDlgItemTextW(dialog, Percent, text, 128);
    return ParsePercent(text, percent);
}
void Show4KWarning(HWND dialog, MMDApp* app, bool above4K) {
    SetDlgItemTextW(dialog, Warning, above4K ? Text(app,
        L"警告：超过 4K", L"Warning: above 4K", L"警告：4Kを超えています") : L"");
    ShowWindow(GetDlgItem(dialog, Warning), above4K ? SW_SHOWNA : SW_HIDE);
}
void UpdateReadout(HWND dialog, MMDApp* app) {
    const bool automatic = IsDlgButtonChecked(dialog, Automatic) == BST_CHECKED;
    EnableWindow(GetDlgItem(dialog, Slider), !automatic);
    EnableWindow(GetDlgItem(dialog, Percent), !automatic);
    EnableWindow(GetDlgItem(dialog, Apply), !automatic);
    double percent = app->viewportRender.percent;
    if (!automatic && !ReadPercent(dialog, percent)) {
        SetDlgItemTextW(dialog, Dimensions, Text(app, L"请输入大于 0 的有效百分比。",
            L"Enter a percentage greater than 0.", L"0より大きい数値を入力してください。"));
        Show4KWarning(dialog, app, false);
        return;
    }
    const Size size = PreviewSize(app, automatic, percent);
    wchar_t text[180]{};
    swprintf_s(text, Text(app, L"渲染尺寸：%u × %u 像素", L"Render resolution: %u × %u px",
        L"描画解像度：%u × %u px"), size.width, size.height);
    SetDlgItemTextW(dialog, Dimensions, text);
    Show4KWarning(dialog, app, Above4K(size));
}
void RefreshPreview(MMDApp* app) {
    InvalidateRect(app->FloatingWindow() ? app->FloatingWindow() : app->MainWindow(), nullptr, FALSE);
    auto* renderer = app->Renderer();
    if (!renderer || !renderer->device) return;
    const auto previous = app->state.messageSeen;
    app->state.messageSeen = 1;
    RenderFrameScene(app);
    app->state.messageSeen = previous;
    FillFramePresentationMargins(app, renderer->device);
    const RECT rect = FramePresentationRect(app);
    renderer->device->Present(&rect, &rect,
        app->FloatingWindow() ? app->FloatingWindow() : app->MainWindow(), nullptr);
}
bool ApplyValue(HWND dialog, MMDApp* app) {
    const bool automatic = IsDlgButtonChecked(dialog, Automatic) == BST_CHECKED;
    double percent = app->viewportRender.percent;
    if (!automatic && !ReadPercent(dialog, percent)) { UpdateReadout(dialog, app); return false; }
    std::wstring error;
    if (!Configure(app, automatic, percent, error)) {
        CheckDlgButton(dialog, Automatic, app->viewportRender.automatic ? BST_CHECKED : BST_UNCHECKED);
        UpdateReadout(dialog, app);
        MessageBoxW(dialog, error.c_str(), Title(app), MB_OK | MB_ICONWARNING);
        return false;
    }
    WritePercent(dialog, percent);
    UpdateReadout(dialog, app);
    RefreshPreview(app);
    return true;
}
}
INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    auto* app = reinterpret_cast<MMDApp*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        app = reinterpret_cast<MMDApp*>(lp);
        SetWindowLongPtrW(dialog, DWLP_USER, lp);
        SetWindowTextW(dialog, Title(app));
        SetDlgItemTextW(dialog, Automatic, Text(app, L"自动：匹配当前窗口尺寸",
            L"Automatic: match window size", L"自動：ウィンドウサイズに合わせる"));
        SetDlgItemTextW(dialog, Apply, Text(app, L"应用", L"Apply", L"適用"));
        // The slider and Apply button commit immediately; this button only closes.
        SetDlgItemTextW(dialog, IDCANCEL, Text(app, L"关闭", L"Close", L"閉じる"));
        SendDlgItemMessageW(dialog, Slider, TBM_SETRANGE, TRUE, MAKELPARAM(25, 100));
        SendDlgItemMessageW(dialog, Slider, TBM_SETPAGESIZE, 0, 5);
        SendDlgItemMessageW(dialog, Percent, EM_SETLIMITTEXT, 120, 0);
        WritePercent(dialog, app->viewportRender.percent);
        CheckDlgButton(dialog, Automatic, app->viewportRender.automatic ? BST_CHECKED : BST_UNCHECKED);
        UpdateReadout(dialog, app);
        return TRUE;
    }
    if (!app) return FALSE;
    if (message == WM_CTLCOLORSTATIC && reinterpret_cast<HWND>(lp) == GetDlgItem(dialog, Warning)) {
        HDC dc = reinterpret_cast<HDC>(wp);
        SetTextColor(dc, RGB(192, 0, 0));
        SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
    }
    if (message == WM_HSCROLL && reinterpret_cast<HWND>(lp) == GetDlgItem(dialog, Slider)) {
        if (!IsWindowEnabled(GetDlgItem(dialog, Slider))) return TRUE;
        if (LOWORD(wp) != TB_ENDTRACK) {
            WritePercent(dialog, double(SendDlgItemMessageW(dialog, Slider, TBM_GETPOS, 0, 0)));
            UpdateReadout(dialog, app);
        }
        // ENDTRACK covers mouse release and keyboard adjustment. Target
        // allocation never runs while the slider thumb is still held down.
        if (LOWORD(wp) == TB_ENDTRACK) ApplyValue(dialog, app);
        return TRUE;
    }
    if (message == WM_CLOSE) { EndDialog(dialog, IDCANCEL); return TRUE; }
    if (message != WM_COMMAND) return FALSE;
    switch (LOWORD(wp)) {
    case Percent:
        if (HIWORD(wp) == EN_CHANGE) UpdateReadout(dialog, app);
        return TRUE;
    case Automatic:
        ApplyValue(dialog, app); return TRUE;
    case Apply:
    case IDOK:
        ApplyValue(dialog, app); return TRUE;
    case IDCANCEL:
        EndDialog(dialog, IDCANCEL); return TRUE;
    }
    return FALSE;
}
void RefreshMenu(MMDApp* app) {
    if (build_profile::original || !app || !IsWindow(app->MainWindow())) return;
    const HMENU bar = GetMenu(app->MainWindow());
    const std::wstring caption = std::wstring(Title(app)) + L"…";
    for (int i = 0; i < GetMenuItemCount(bar); ++i) {
        HMENU view = GetSubMenu(bar, i);
        if (!view || GetMenuState(view, 212, MF_BYCOMMAND) == UINT(-1)) continue;
        if (GetMenuState(view, kCommand, MF_BYCOMMAND) == UINT(-1)) {
            for (int pos = 0; pos < GetMenuItemCount(view); ++pos) {
                if (GetMenuItemID(view, pos) == 212) {
                    InsertMenuW(view, pos + 1, MF_BYPOSITION | MF_STRING, kCommand, caption.c_str()); break;
                }
            }
        }
        ModifyMenuW(view, kCommand, MF_BYCOMMAND | MF_STRING, kCommand, caption.c_str());
        break;
    }
}
bool HandleCommand(MMDApp* app, unsigned command) {
    if (build_profile::original || command != kCommand) return false;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    UiDialogBoxParamW(static_cast<HINSTANCE>(app->HInstance()), MAKEINTRESOURCEW(kDialog),
        app->MainWindow(), DialogProc, reinterpret_cast<LPARAM>(app));
    return true;
}
}
