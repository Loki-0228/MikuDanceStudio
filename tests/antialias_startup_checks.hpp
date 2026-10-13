// Hold real HLSL compilation to verify responsiveness, lazy demand and reset.
using CompileShader = decltype(&graphics_runtime::Compile);
CompileShader originalShaderCompile = nullptr;
HANDLE aaCompilerEntered = nullptr, aaCompilerProceed = nullptr, aaCompilerReturned = nullptr;
DWORD aaUiThread = 0;
std::atomic<unsigned> fxaaCompiles{0}, taaCompiles{0};
std::atomic<bool> aaWrongThread{false};
HRESULT GateAntialiasCompiler(const void* source, SIZE_T size, const char* name,
    const D3D_SHADER_MACRO* defines, ID3DInclude* include, const char* entry,
    const char* target, UINT flags1, UINT flags2, ID3DBlob** code, ID3DBlob** error) {
    const bool aa = name && std::strcmp(name, "antialias") == 0;
    if (aa) {
        if (GetCurrentThreadId() == aaUiThread) aaWrongThread = true;
        if (std::strcmp(entry,"fxaa") == 0) ++fxaaCompiles; else ++taaCompiles;
        SetEvent(aaCompilerEntered);
        WaitForSingleObject(aaCompilerProceed,30000);
    }
    const auto result=originalShaderCompile(source,size,name,defines,include,entry,target,flags1,flags2,code,error);
    if (aa) SetEvent(aaCompilerReturned);
    return result;
}
void InstallAntialiasCompilerGate() {
    aaUiThread=GetCurrentThreadId();
    aaCompilerEntered=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    aaCompilerProceed=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    aaCompilerReturned=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    Check(aaCompilerEntered && aaCompilerProceed && aaCompilerReturned,"create compiler gate");
    Check(MH_CreateHook(&graphics_runtime::Compile,&GateAntialiasCompiler,
        reinterpret_cast<void**>(&originalShaderCompile))==MH_OK,"gate real AA compiler");
}
void CheckAntialiasStartup(MMDApp* app) {
    const auto frame=[&] {
        app->state.messageSeen=10;app->DeltaTime()=1.f/30;FrameDriver(app);
        MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    };
    Check(!app->antialiasing.enabled,"test starts with disabled AA preference");
    frame();
    Check(!fxaaCompiles && !taaCompiles,"disabled startup does not compile either AA shader");
    std::wstring error;
    Check(antialias::Select(app,antialias::Algorithm::Fxaa,error),"select FXAA while disabled");
    frame();
    Check(!fxaaCompiles && !taaCompiles,"selecting disabled algorithm does not initialize it");
    auto begin=GetTickCount64();
    Check(antialias::Toggle(app,error),"enable FXAA without waiting");
    Check(GetTickCount64()-begin<1000,"menu enable returns while compiler is held");
    Check(WaitForSingleObject(aaCompilerEntered,3000)==WAIT_OBJECT_0,"enabled FXAA starts worker");
    begin=GetTickCount64();
    for(int i=0;i<4;++i)frame();
    Check(GetTickCount64()-begin<1000 && !aaWrongThread,"preview frames and messages continue while compiler is held");
    Check(fxaaCompiles==1 && !taaCompiles,"FXAA does not initialize unused TAA");
    Check(PostDeviceReset(app),"device reset while compilation is pending");
    Check(antialias::Toggle(app,error),"disable while worker is pending");
    frame();
    SetEvent(aaCompilerProceed);
    Check(WaitForSingleObject(aaCompilerReturned,10000)==WAIT_OBJECT_0,"worker finishes privately after disable");
    for(int i=0;i<4;++i){Sleep(16);frame();}
    Check(!app->antialiasing.enabled,"worker completion does not re-enable disabled AA");
    Check(antialias::Toggle(app,error),"re-enable completed FXAA");
    for(int i=0;i<4;++i)frame();
    Check(fxaaCompiles==1 && !taaCompiles,"reset and re-enable reuse compiled bytecode");
    ResetEvent(aaCompilerEntered);ResetEvent(aaCompilerProceed);ResetEvent(aaCompilerReturned);
    Check(antialias::Select(app,antialias::Algorithm::Taa,error),"enable TAA asynchronously");
    Check(WaitForSingleObject(aaCompilerEntered,3000)==WAIT_OBJECT_0,"TAA compiles only on demand");
    begin=GetTickCount64();frame();
    Check(GetTickCount64()-begin<1000 && !aaWrongThread,"TAA compilation does not block a preview frame");
    SetEvent(aaCompilerProceed);
    Check(WaitForSingleObject(aaCompilerReturned,10000)==WAIT_OBJECT_0,"TAA compiler completes");
    for(int i=0;i<18;++i){Sleep(16);frame();}
    Check(antialias::AccumulatedSamples(app)>=8,"completed TAA automatically starts temporal accumulation");
    Check(fxaaCompiles==1 && taaCompiles==1,"each enabled algorithm compiles once");
    MH_DisableHook(&graphics_runtime::Compile);
    CloseHandle(aaCompilerEntered);CloseHandle(aaCompilerProceed);CloseHandle(aaCompilerReturned);
}
