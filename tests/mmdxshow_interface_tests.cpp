// MAINTENANCE: regression-keep; purpose and cleanup conditions in tests/README.md.
#include <initguid.h>
#include "../src/mmdxshow/mmdxshow.hpp"
#include <cstdio>
#include <cstdlib>

static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int wmain(int argc, wchar_t** argv) {
    Check(argc == 2, "MMDxShow DLL path supplied");
    CoInitialize(nullptr);
    HMODULE dll = LoadLibraryW(argv[1]);
    Check(dll != nullptr, "load rebuilt MMDxShow");
    auto getClass = reinterpret_cast<HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, void**)>(
        GetProcAddress(dll, "DllGetClassObject"));
    Check(getClass != nullptr, "resolve class factory");
    IClassFactory* factory = nullptr;
    Check(SUCCEEDED(getClass(MMDXSHOW_CLSID_PushSourceDIBSq, IID_IClassFactory,
                            reinterpret_cast<void**>(&factory))), "obtain filter factory");
    IBaseFilter* filter = nullptr;
    Check(SUCCEEDED(factory->CreateInstance(nullptr, IID_IBaseFilter,
                    reinterpret_cast<void**>(&filter))), "query native IBaseFilter subobject");
    IPushSource* push = nullptr;
    Check(SUCCEEDED(filter->QueryInterface(MMDXSHOW_IID_IPushSource,
                    reinterpret_cast<void**>(&push))), "query native IPushSource subobject");
    IAMovieSetup* setup = nullptr;
    Check(SUCCEEDED(filter->QueryInterface(IID_IAMovieSetup,
                    reinterpret_cast<void**>(&setup))), "query IAMovieSetup subobject");
    setup->Release();
    IEnumPins* pins = nullptr;
    Check(SUCCEEDED(filter->EnumPins(&pins)), "enumerate output pins");
    IPin* pin = nullptr;
    Check(pins->Next(1, &pin, nullptr) == S_OK, "obtain typed IPin subobject");
    IQualityControl* quality = nullptr;
    const auto qualityHr = pin->QueryInterface(IID_IQualityControl,
                    reinterpret_cast<void**>(&quality));
    Check(SUCCEEDED(qualityHr), "query pin's IQualityControl subobject");
    quality->Release();
    float version = 0.f;
    Check(push->GetRate(&version) == S_OK && version > 1.01f, "version handshake");
    BITMAPINFOHEADER header{};
    header.biSize = sizeof(header);
    header.biWidth = header.biHeight = 2;
    header.biPlanes = 1;
    header.biBitCount = 32;
    Check(push->SetBitmapInfo(&header, 0, 30.f) == S_OK, "set bitmap fields");
    BYTE state = 0;
    Check(push->GetStreamingState(&state) == S_OK && state == 1, "frame acknowledgement field");
    DWORD pixels[4]{};
    Check(push->StartStreaming(reinterpret_cast<DWORD_PTR>(pixels)) == S_OK,
          "pass native-width frame pointer");
    Check(push->GetStreamingState(&state) == S_OK && state == 0, "frame state changes after push");
    Check(push->BeginStreaming() == S_OK && push->GetRate(&version) == E_FAIL,
          "stream-end latch uses the same typed field as the pin");
    pin->Release();
    pins->Release();
    push->Release();
    filter->Release();
    factory->Release();
    auto canUnload = reinterpret_cast<HRESULT (STDAPICALLTYPE*)()>(
        GetProcAddress(dll, "DllCanUnloadNow"));
    Check(canUnload && canUnload() == S_OK, "typed destruction releases filter and pin");
    FreeLibrary(dll);
    CoUninitialize();
    std::puts("MMDxShow interface and streaming-state regressions passed");
}
