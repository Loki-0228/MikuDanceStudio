#include "../render/nvidia_antialias_elevation.hpp"
#include <shellapi.h>
namespace detail = mikudancestudio::nvidia_antialias_fix::detail;

// Invoked only through ShellExecuteEx(runas), never from an app-wide elevated
// restart. The only accepted operation is enabling one ID on the sibling host.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return ERROR_INVALID_PARAMETER;
    const bool argumentsValid = argc == 3 && std::wstring(argv[1]) == L"--target";
    const std::wstring requested = argumentsValid ? argv[2] : L"";
    LocalFree(argv);
    if (!argumentsValid) return ERROR_INVALID_PARAMETER;
    std::wstring helper, application;
    if (!detail::CanonicalFile(detail::ModulePath(), helper) ||
        !detail::CanonicalFile(requested, application) ||
        detail::NormalizePath(requested) != detail::NormalizePath(application) ||
        !detail::IsSiblingApplication(helper, application)) return ERROR_INVALID_PARAMETER;
    if (!detail::IsElevated()) return ERROR_ELEVATION_REQUIRED;
    const bool enabled = true;
    mikudancestudio::nvidia_antialias_fix::State state;
    return detail::RunForPath(application, &enabled, state) && state.enabled ? ERROR_SUCCESS : ERROR_GEN_FAILURE;
}
