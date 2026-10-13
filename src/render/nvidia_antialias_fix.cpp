#include "nvidia_antialias_elevation.hpp"
#include <shellapi.h>
#include "mikudancestudio/runtime_log.hpp"

namespace mikudancestudio::nvidia_antialias_fix {
namespace {
bool selectedNvidia = false;
bool explanationShown = false;
const wchar_t* Localized(int language, const wchar_t* chinese, const wchar_t* english, const wchar_t* japanese) {
    return language == 2 ? chinese : language == 1 ? english : japanese;
}
std::wstring DetectionError(int language, const State& state) {
    std::wstring text = Localized(language, L"无法确认 NVIDIA 驱动的修复状态。",
        L"Could not check whether the NVIDIA rendering fix is enabled.",
        L"NVIDIA 描画修正の適用状態を確認できませんでした。");
    if (!state.error.empty()) text += L"\n\n" + state.error;
    return text;
}
bool TestDisabled() {
    wchar_t value[8]{};
    return GetEnvironmentVariableW(L"MMS_NVIDIA_AA_FIX", value, 8) == 1 && value[0] == L'0';
}
std::wstring RecordPath() {
    const auto module = detail::CanonicalModulePath();
    const auto slash = module.find_last_of(L"\\/");
    return slash != std::wstring::npos ? module.substr(0, slash) + L"\\Data\\nvidia_antialias_fix.ini" : L"";
}
bool WriteRecord(const wchar_t* key, const wchar_t* value) {
    const auto path = RecordPath();
    if (path.empty()) return false;
    const auto data = path.substr(0, path.find_last_of(L'\\'));
    if (!CreateDirectoryW(data.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    return WritePrivateProfileStringW(L"NVIDIA", key, value, path.c_str()) != FALSE;
}
bool Refresh(State& state) {
    const auto path = detail::CanonicalModulePath();
    if (path.empty()) { state.error = L"Cannot resolve executable path"; return false; }
    const bool success = detail::RunForPath(path, nullptr, state);
    // The record is informational. It is never accepted instead of driver data.
    if (success) {
        const auto record = RecordPath();
        const bool recorded = GetPrivateProfileIntW(L"NVIDIA", L"Installed", 0, record.c_str()) == 1;
        if (recorded != state.enabled && !WriteRecord(L"Installed", state.enabled ? L"1" : nullptr))
            runtime_log::Write("AA NVIDIA detection: could not update Data/nvidia_antialias_fix.ini");
    }
    return success;
}
bool LaunchHelper(HWND owner, const std::wstring& helper, const std::wstring& target, std::wstring& error, int language) {
    const std::wstring parameters = L"--target \"" + target + L"\"";
    const std::wstring directory = helper.substr(0, helper.find_last_of(L'\\'));
    SHELLEXECUTEINFOW launch{};
    launch.cbSize = sizeof(launch);
    launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    launch.hwnd = owner;
    launch.lpVerb = L"runas";
    launch.lpFile = helper.c_str();
    launch.lpParameters = parameters.c_str();
    launch.lpDirectory = directory.c_str();
    launch.nShow = SW_HIDE;
    if (!ShellExecuteExW(&launch)) {
        const DWORD code = GetLastError();
        if (code != ERROR_CANCELLED)
            error = std::wstring(Localized(language, L"无法启动 NVIDIA 补丁安装程序。",
                L"Could not start the NVIDIA fix installer.", L"NVIDIA 描画修正のインストーラーを起動できませんでした。")) +
                L"\nWindows " + std::to_wstring(code);
        return false;
    }
    if (!launch.hProcess) {
        error = Localized(language, L"无法取得 NVIDIA 补丁安装程序的状态。",
            L"Could not check the NVIDIA fix installer's status.", L"NVIDIA 描画修正のインストーラーの状態を確認できませんでした。");
        return false;
    }
    const DWORD waited = WaitForSingleObject(launch.hProcess, 30000);
    DWORD code = ERROR_GEN_FAILURE;
    const bool completed = waited == WAIT_OBJECT_0 && GetExitCodeProcess(launch.hProcess, &code);
    CloseHandle(launch.hProcess);
    if (!completed || code != ERROR_SUCCESS) {
        error = waited == WAIT_TIMEOUT ? Localized(language,
            L"NVIDIA 补丁安装尚未完成，请稍后重新打开帮助菜单检查。",
            L"Installation is still in progress. Open the Help menu again shortly to check the result.",
            L"インストールはまだ完了していません。しばらく待ってからヘルプメニューを開き、結果を確認してください。") :
            std::wstring(Localized(language, L"NVIDIA 补丁安装未成功，当前渲染不受影响。",
                L"The NVIDIA fix could not be installed. You can continue rendering.",
                L"NVIDIA 描画修正をインストールできませんでした。描画は引き続き利用できます。")) + L"\nWindows " + std::to_wstring(code);
        return false;
    }
    return true;
}
}

State Inspect() {
    State state;
    const auto path = detail::CanonicalModulePath();
    if (path.empty()) state.error = L"Cannot resolve executable path";
    else detail::RunForPath(path, nullptr, state);
    return state;
}
bool EnsureForAdapter(std::uint32_t vendorId, std::wstring& error) {
    error.clear();
    selectedNvidia = vendorId == 0x10de && !TestDisabled();
    if (!selectedNvidia) return true;
    State state;
    const bool success = Refresh(state);
    error = state.error;
    if (success) runtime_log::Write("AA NVIDIA compatibility detection: effective fix=%d; startup did not change driver settings", int(state.enabled));
    return success;
}
bool ShouldOfferInstall() {
    if (!selectedNvidia || TestDisabled()) return false;
    State state;
    return Refresh(state) && state.available && !state.enabled;
}
bool InstallForCurrentExecutable(void* window, std::wstring& error, int language) {
    error.clear();
    if (!selectedNvidia || TestDisabled()) {
        error = Localized(language, L"当前渲染器未使用 NVIDIA 显卡。",
            L"The current renderer is not using an NVIDIA GPU.", L"現在のレンダラーは NVIDIA GPU を使用していません。");
        return false;
    }
    State state;
    if (!Refresh(state)) { error = DetectionError(language, state); return false; }
    if (state.enabled) return true;
    const auto target = detail::CanonicalModulePath();
    std::wstring helper;
    if (target.empty()) {
        error = Localized(language, L"无法确认当前程序路径。", L"Could not resolve this program's path.",
            L"このプログラムの保存場所を確認できませんでした。"); return false;
    }
    const auto helperPath = target.substr(0, target.find_last_of(L'\\') + 1) + detail::kHelperName;
    if (!detail::CanonicalFile(helperPath, helper) || !detail::IsSiblingApplication(helper, target)) {
        error = Localized(language, L"程序目录缺少有效的 MMSNvidiaAAFix.exe，请保留完整发行包。",
            L"MMSNvidiaAAFix.exe is missing or invalid. Keep it in the same folder as MikuMikuDance.exe.",
            L"MMSNvidiaAAFix.exe が見つからないか、無効です。MikuMikuDance.exe と同じフォルダーに配置してください。"); return false;
    }
    const auto record = RecordPath();
    const bool explained = explanationShown ||
        GetPrivateProfileIntW(L"NVIDIA", L"AdministratorNoticeShown", 0, record.c_str()) == 1;
    if (!explained) {
        const int answer = MessageBoxW(static_cast<HWND>(window),
            Localized(language,
                L"安装 NVIDIA 双面绘制修复需要一次 Windows 管理员授权。\n\n补丁仅设置当前程序的 NVIDIA 配置。完成后可继续正常使用；部分驱动需要下次启动程序才生效。",
                L"Installing the NVIDIA double-sided rendering fix requires Windows administrator approval once.\n\nThe fix changes only this program's NVIDIA profile. You can continue using the program afterward. Some drivers apply the change the next time you start it.",
                L"NVIDIA 両面描画修正のインストールには、Windows の管理者権限による承認が一度必要です。\n\n変更するのは、このプログラム専用の NVIDIA プロファイルだけです。完了後もそのまま利用できます。ドライバーによっては、次回起動時に変更が反映されます。"),
            Localized(language, L"安装 NVIDIA 修复双面绘制补丁", L"Install NVIDIA Rendering Fix", L"NVIDIA 両面描画修正のインストール"),
            MB_OKCANCEL | MB_ICONINFORMATION);
        explanationShown = true;
        WriteRecord(L"AdministratorNoticeShown", L"1");
        if (answer != IDOK) return false;
    }
    if (!LaunchHelper(static_cast<HWND>(window), helper, target, error, language)) return false;
    if (!Refresh(state) || !state.enabled) {
        error = state.error.empty() ? Localized(language,
            L"驱动未确认补丁已启用，帮助菜单保留安装选项。",
            L"The driver has not confirmed that the fix is enabled. The installation option will remain in the Help menu.",
            L"ドライバーで修正の適用を確認できませんでした。ヘルプメニューのインストール項目は引き続き表示されます。") : DetectionError(language, state);
        return false;
    }
    if (!WriteRecord(L"Installed", L"1"))
        error = Localized(language, L"驱动补丁已启用，但无法写入 Data/nvidia_antialias_fix.ini。",
            L"The fix is enabled, but its record could not be saved to Data/nvidia_antialias_fix.ini.",
            L"修正は適用されていますが、Data/nvidia_antialias_fix.ini に記録を保存できませんでした。");
    runtime_log::Write("AA NVIDIA compatibility installed for this executable and verified from the driver");
    return true;
}
}
