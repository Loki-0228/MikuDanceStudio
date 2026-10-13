#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
// Test fixture only: no driver API and no registration side effects.
extern "C" __declspec(dllexport) HRESULT WINAPI MMSNvidiaAAFixQueryInstalled(BOOL* installed) {
    if (!installed) return E_POINTER;
    wchar_t value[8]{};
    GetEnvironmentVariableW(L"MMS_TEST_PATCH_STATE", value, 8);
    *installed = value[0] == L'1';
    return value[0] == L'!' ? HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) : value[0] == L'?' ? E_FAIL : S_OK;
}
