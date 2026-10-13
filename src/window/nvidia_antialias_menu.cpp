#include "mikudancestudio/nvidia_antialias_menu.hpp"
#include "mikudancestudio/nvidia_antialias_fix.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/build_profile.hpp"

namespace mikudancestudio::nvidia_antialias_menu {
namespace {
constexpr UINT kAboutCommand = 201;

HMENU FindHelp(MMDApp* app) {
    const HMENU bar = app->FullscreenMode() ? app->state.savedMenu : GetMenu(app->MainWindow());
    for (int i = 0; i < GetMenuItemCount(bar); ++i) {
        const HMENU popup = GetSubMenu(bar, i);
        if (popup && GetMenuState(popup, kAboutCommand, MF_BYCOMMAND) != UINT(-1))
            return popup;
    }
    return nullptr;
}

void Refresh(MMDApp* app) {
    const HMENU help = FindHelp(app);
    if (!help) return;
    const bool exists = GetMenuState(help, kInstallCommand, MF_BYCOMMAND) != UINT(-1);
    if (!nvidia_antialias_fix::ShouldOfferInstall()) {
        if (exists) DeleteMenu(help, kInstallCommand, MF_BYCOMMAND);
        return;
    }
    const wchar_t* label = app->EnglishUI() == 2 ? L"安装 NVIDIA 修复双面绘制补丁…"
        : app->EnglishUI() == 1 ? L"Install NVIDIA double-sided rendering fix…"
        : L"NVIDIA 両面描画の修正をインストール…";
    if (exists) {
        ModifyMenuW(help, kInstallCommand, MF_BYCOMMAND | MF_STRING, kInstallCommand, label);
    } else {
        // About identifies Help across language changes and plugin menu insertion.
        InsertMenuW(help, kAboutCommand, MF_BYCOMMAND | MF_STRING, kInstallCommand, label);
    }
}
}

bool HandleMessage(MMDApp* app, UINT message, WPARAM wp, LPARAM lp) {
    if (build_profile::original || !app || !app->MainWindow()) return false;
    if (message == WM_INITMENU) {
        Refresh(app);
    } else if (message == WM_COMMAND && HIWORD(wp) == 0 && lp == 0 && LOWORD(wp) == kInstallCommand) {
        std::wstring error;
        const bool installed = nvidia_antialias_fix::InstallForCurrentExecutable(app->MainWindow(), error, app->EnglishUI());
        Refresh(app);
        const wchar_t* title = L"NVIDIA";
        if (installed) {
            std::wstring text = app->EnglishUI() == 2
                ? L"已确认 NVIDIA 双面绘制修复已启用。\n下次正常启动程序时，驱动将使用该设置。"
                : app->EnglishUI() == 1
                ? L"The NVIDIA double-sided rendering fix is enabled.\nThe driver will use it the next time you start the program normally."
                : L"NVIDIA 両面描画の修正が有効になりました。\n次回の通常起動時にドライバーがこの設定を使用します。";
            if (!error.empty()) text += L"\n\n" + error;
            MessageBoxW(app->MainWindow(), text.c_str(), title,
                MB_OK | (error.empty() ? MB_ICONINFORMATION : MB_ICONWARNING));
        } else if (!error.empty()) {
            MessageBoxW(app->MainWindow(), error.c_str(), title, MB_OK | MB_ICONERROR);
        }
        return true;
    }
    return false;
}
}
