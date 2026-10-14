// Optional target observation runs outside the timed throughput interval.
namespace scale_targets {
struct Row {
    unsigned width, height, viewportWidth, viewportHeight, draws;
    D3DFORMAT format;
    std::uint64_t primitives;
};
std::vector<Row> rows;
bool observing = false;
using ModelUpdate = void(*)(MMDApp*, unsigned char*, const float*);
ModelUpdate modelUpdate = nullptr;
double modelMilliseconds = 0;
unsigned modelUpdates = 0;
void ObserveModelUpdate(MMDApp* app, unsigned char* model, const float* world) {
    if (!observing) { modelUpdate(app,model,world); return; }
    LARGE_INTEGER start{},end{},frequency{};
    QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
    modelUpdate(app,model,world);
    QueryPerformanceCounter(&end);
    modelMilliseconds += 1000. * double(end.QuadPart-start.QuadPart) / double(frequency.QuadPart);
    ++modelUpdates;
}
void Observe(IDirect3DDevice9* device, unsigned primitives) {
    if (!observing) return;
    Microsoft::WRL::ComPtr<IDirect3DSurface9> target;
    D3DSURFACE_DESC description{}; D3DVIEWPORT9 viewport{};
    if (FAILED(device->GetRenderTarget(0, &target)) || FAILED(target->GetDesc(&description)) ||
        FAILED(device->GetViewport(&viewport))) return;
    for (auto& row : rows) if (row.width == description.Width && row.height == description.Height &&
        row.viewportWidth == viewport.Width && row.viewportHeight == viewport.Height && row.format == description.Format) {
        ++row.draws; row.primitives += primitives; return;
    }
    rows.push_back({description.Width, description.Height, viewport.Width, viewport.Height, 1, description.Format, primitives});
}
using Indexed = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
using Primitive = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
using User = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
Indexed indexed = nullptr; Primitive primitive = nullptr; User user = nullptr;
HRESULT WINAPI DrawIndexed(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,INT b,UINT m,UINT v,UINT s,UINT n) {
    Observe(d,n); return indexed(d,t,b,m,v,s,n);
}
HRESULT WINAPI Draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT s,UINT n) {
    Observe(d,n); return primitive(d,t,s,n);
}
HRESULT WINAPI DrawUser(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT n,const void* p,UINT stride) {
    Observe(d,n); return user(d,t,n,p,stride);
}
void Install(IDirect3DDevice9* native) {
    modelMilliseconds=0;modelUpdates=0;
    Check(MH_CreateHook(&UpdateModelVertexBuffers,&ObserveModelUpdate,reinterpret_cast<void**>(&modelUpdate))==MH_OK &&
        MH_EnableHook(&UpdateModelVertexBuffers)==MH_OK,"observe model preparation outside timing");
    auto** table = *reinterpret_cast<void***>(native);
    Check(MH_CreateHook(table[81],&Draw,reinterpret_cast<void**>(&primitive))==MH_OK &&
        MH_EnableHook(table[81])==MH_OK,"observe native primitive target");
    Check(MH_CreateHook(table[82],&DrawIndexed,reinterpret_cast<void**>(&indexed))==MH_OK &&
        MH_EnableHook(table[82])==MH_OK,"observe native indexed target");
    Check(MH_CreateHook(table[83],&DrawUser,reinterpret_cast<void**>(&user))==MH_OK &&
        MH_EnableHook(table[83])==MH_OK,"observe native postprocess target");
}
void Remove(IDirect3DDevice9* native) {
    MH_RemoveHook(&UpdateModelVertexBuffers);
    auto** table = *reinterpret_cast<void***>(native);
    for (unsigned slot : {81u,82u,83u}) MH_RemoveHook(table[slot]);
}
void Report(int percent) {
    std::uint64_t primitives=0;unsigned draws=0;
    for (const auto& row : rows) { primitives+=row.primitives;draws+=row.draws; }
    std::printf("SCALE_WORK percent=%d model_updates=%u model_cpu_ms=%.3f draws=%u primitives=%llu\n",
        percent,modelUpdates,modelMilliseconds,draws,static_cast<unsigned long long>(primitives));
    for (const auto& row : rows)
        std::printf("SCALE_TARGET percent=%d target=%ux%u viewport=%ux%u format=%u draws=%u primitives=%llu\n",
            percent,row.width,row.height,row.viewportWidth,row.viewportHeight,unsigned(row.format),row.draws,
            static_cast<unsigned long long>(row.primitives));
}
}
