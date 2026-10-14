// Hidden-host diagnostics only. Coarse queries are read after a whole batch.
// Optional per-draw queries are read after each frame and perturb the pipeline.
// Timestamp intervals include GPU idle gaps, not just GPU busy time.
namespace scale_gpu {
void Require(bool ok,const char* label) { if(!ok)Check(false,label); }
using Microsoft::WRL::ComPtr;
enum Phase { Scene, Antialias, Upscale, Hud, PhaseCount, Inactive };
const char* const phaseNames[]{"scene", "antialias", "upscale", "hud"};
struct DrawSample {
    ComPtr<IDirect3DQuery9> begin, end;
    D3DSURFACE_DESC target{};
    D3DVIEWPORT9 viewport{};
    Phase phase=Inactive;
    unsigned primitives=0;
    double cpuMs=0;
};
std::vector<DrawSample> draws;
ComPtr<IDirect3DQuery9> markers[PhaseCount+1],frequency,disjoint,emptyBegin,emptyEnd,pipeline;
IDirect3DDevice9* nativeDevice=nullptr;
Phase phase=Inactive;
bool sampling=false, detailed=false, gpuQueries=true, marked[PhaseCount+1]{};
unsigned drawCount=0,models=0;
double cpuMarkers[PhaseCount+1]{},frameStart=0,modelMs=0,presentMs=0,presentStart=0;
void* completeAddress=nullptr;
std::ofstream framesFile,targetsFile,cpuFile;
struct Snapshot {
    ComPtr<IDirect3DQuery9> markers[PhaseCount+1],frequency,disjoint,emptyBegin,emptyEnd,pipeline;
    double cpuMarkers[PhaseCount+1]{},frameStart=0,modelMs=0,presentMs=0;
    unsigned models=0,drawCount=0;
};
std::vector<Snapshot> snapshots;
void SwapSnapshot(unsigned sample) {
    auto& s=snapshots.at(sample);
    for(unsigned i=0;i<=PhaseCount;++i){markers[i].Swap(s.markers[i]);std::swap(cpuMarkers[i],s.cpuMarkers[i]);}
    frequency.Swap(s.frequency);disjoint.Swap(s.disjoint);emptyBegin.Swap(s.emptyBegin);emptyEnd.Swap(s.emptyEnd);pipeline.Swap(s.pipeline);
    std::swap(frameStart,s.frameStart);std::swap(modelMs,s.modelMs);std::swap(presentMs,s.presentMs);
    std::swap(models,s.models);std::swap(drawCount,s.drawCount);
}

double Clock() {
    LARGE_INTEGER value{},freq{};QueryPerformanceCounter(&value);QueryPerformanceFrequency(&freq);
    return double(value.QuadPart)/double(freq.QuadPart);
}
void Mark(unsigned index) {
    Require(index<=PhaseCount && !marked[index],"GPU phase marker occurs once");
    cpuMarkers[index]=Clock();marked[index]=true;
    if(gpuQueries)Require(SUCCEEDED(markers[index]->Issue(D3DISSUE_END)),"issue GPU phase timestamp");
}
using Clear=HRESULT(WINAPI*)(IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR,float,DWORD);
using EndScene=HRESULT(WINAPI*)(IDirect3DDevice9*);
using Complete=bool(*)(viewport_render_scale::FrameScope*);
using ModelUpdate=void(*)(MMDApp*,unsigned char*,const float*);
Clear clear=nullptr;EndScene endScene=nullptr;Complete complete=nullptr;ModelUpdate modelUpdate=nullptr;
HRESULT WINAPI ObserveClear(IDirect3DDevice9* device,DWORD count,const D3DRECT* rects,
    DWORD flags,D3DCOLOR color,float depth,DWORD stencil) {
    if(sampling && phase==Inactive) {
        phase=Scene;Mark(Scene);
        if(gpuQueries && pipeline)Require(SUCCEEDED(pipeline->Issue(D3DISSUE_BEGIN)),"begin GPU pipeline timing");
    }
    return clear(device,count,rects,flags,color,depth,stencil);
}
HRESULT WINAPI ObserveEndScene(IDirect3DDevice9* device) {
    const bool mainScene=sampling && phase==Scene;
    const HRESULT hr=endScene(device);
    if(mainScene){Mark(Antialias);phase=Antialias;}
    return hr;
}
bool ObserveComplete(viewport_render_scale::FrameScope* frame) {
    if(sampling){Require(phase==Antialias,"preview follows the completed MME scene");Mark(Upscale);phase=Upscale;}
    const bool result=complete(frame);
    if(sampling){Require(result,"sampled preview completed");Mark(Hud);phase=Hud;}
    return result;
}
void ObserveModel(MMDApp* app,unsigned char* model,const float* world) {
    const double start=sampling?Clock():0;
    modelUpdate(app,model,world);
    if(sampling){modelMs+=(Clock()-start)*1000;++models;}
}
using Indexed=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
using Primitive=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
using User=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
using IndexedUser=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);
Indexed indexed=nullptr;Primitive primitive=nullptr;User user=nullptr;IndexedUser indexedUser=nullptr;
DrawSample* BeforeDraw(IDirect3DDevice9* device,unsigned primitives) {
    if(!sampling || !gpuQueries || !detailed)return nullptr;
    Require(phase<PhaseCount,"native draw belongs to a measured render phase");
    Require(drawCount<draws.size(),"GPU draw query capacity");
    auto& row=draws[drawCount++];row.phase=phase;row.primitives=primitives;
    ComPtr<IDirect3DSurface9> target;
    Require(SUCCEEDED(device->GetRenderTarget(0,&target)) && SUCCEEDED(target->GetDesc(&row.target)) &&
        SUCCEEDED(device->GetViewport(&row.viewport)),"identify timed native target");
    Require(SUCCEEDED(row.begin->Issue(D3DISSUE_END)),"begin native draw timestamp");
    row.cpuMs=Clock();return &row;
}
void AfterDraw(DrawSample* row,HRESULT hr) {
    if(!row)return;
    row->cpuMs=(Clock()-row->cpuMs)*1000;
    Require(SUCCEEDED(hr) && SUCCEEDED(row->end->Issue(D3DISSUE_END)),"end successful native draw timestamp");
}
HRESULT WINAPI ObserveIndexed(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,INT b,UINT m,UINT v,UINT s,UINT n) {
    auto* sample=BeforeDraw(d,n);const auto hr=indexed(d,t,b,m,v,s,n);AfterDraw(sample,hr);return hr;
}
HRESULT WINAPI ObservePrimitive(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT s,UINT n) {
    auto* sample=BeforeDraw(d,n);const auto hr=primitive(d,t,s,n);AfterDraw(sample,hr);return hr;
}
HRESULT WINAPI ObserveUser(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT n,const void* p,UINT s) {
    auto* sample=BeforeDraw(d,n);const auto hr=user(d,t,n,p,s);AfterDraw(sample,hr);return hr;
}
HRESULT WINAPI ObserveIndexedUser(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT m,UINT v,UINT n,
    const void* indices,D3DFORMAT format,const void* vertices,UINT stride) {
    auto* sample=BeforeDraw(d,n);const auto hr=indexedUser(d,t,m,v,n,indices,format,vertices,stride);AfterDraw(sample,hr);return hr;
}
template<class T> void Hook(void* address,void* replacement,T& original) {
    Require(MH_CreateHook(address,replacement,reinterpret_cast<void**>(&original))==MH_OK &&
        MH_EnableHook(address)==MH_OK,"install GPU timing observer");
}
void Install(IDirect3DDevice9* proxy,IDirect3DDevice9* native,bool drawHooks) {
    nativeDevice=native;
    auto** table=*reinterpret_cast<void***>(proxy);auto** nt=*reinterpret_cast<void***>(native);
    Hook(table[43],reinterpret_cast<void*>(&ObserveClear),clear);
    Hook(table[42],reinterpret_cast<void*>(&ObserveEndScene),endScene);
    auto method=&viewport_render_scale::FrameScope::Complete;
    static_assert(sizeof(method)==sizeof(completeAddress),"single-inheritance member address");
    std::memcpy(&completeAddress,&method,sizeof(method));
    Hook(completeAddress,reinterpret_cast<void*>(&ObserveComplete),complete);
    Hook(reinterpret_cast<void*>(&UpdateModelVertexBuffers),reinterpret_cast<void*>(&ObserveModel),modelUpdate);
    if(drawHooks) {
        Hook(nt[81],reinterpret_cast<void*>(&ObservePrimitive),primitive);
        Hook(nt[82],reinterpret_cast<void*>(&ObserveIndexed),indexed);
        Hook(nt[83],reinterpret_cast<void*>(&ObserveUser),user);
        Hook(nt[84],reinterpret_cast<void*>(&ObserveIndexedUser),indexedUser);
    }
}
void Remove(IDirect3DDevice9* proxy,IDirect3DDevice9* native,bool drawHooks) {
    auto** table=*reinterpret_cast<void***>(proxy);auto** nt=*reinterpret_cast<void***>(native);
    for(unsigned i:{42u,43u})MH_RemoveHook(table[i]);
    if(drawHooks)for(unsigned i:{81u,82u,83u,84u})MH_RemoveHook(nt[i]);
    MH_RemoveHook(completeAddress);MH_RemoveHook(&UpdateModelVertexBuffers);nativeDevice=nullptr;
}
void Initialize(IDirect3DDevice9* device,const std::filesystem::path& directory,unsigned sampleCount) {
    detailed=std::getenv("MMS_SCALE_GPU_DRAWS")!=nullptr;
    const auto query=[&](D3DQUERYTYPE type,ComPtr<IDirect3DQuery9>& result) {
        Require(SUCCEEDED(device->CreateQuery(type,&result)),"GPU timestamp query supported");
    };
    query(D3DQUERYTYPE_TIMESTAMPFREQ,frequency);query(D3DQUERYTYPE_TIMESTAMPDISJOINT,disjoint);
    query(D3DQUERYTYPE_TIMESTAMP,emptyBegin);query(D3DQUERYTYPE_TIMESTAMP,emptyEnd);
    for(auto& marker:markers)query(D3DQUERYTYPE_TIMESTAMP,marker);
    if(detailed)draws.resize(2048);
    for(auto& draw:draws){query(D3DQUERYTYPE_TIMESTAMP,draw.begin);query(D3DQUERYTYPE_TIMESTAMP,draw.end);}
    const HRESULT pipelineResult=device->CreateQuery(D3DQUERYTYPE_PIPELINETIMINGS,&pipeline);
    if(!detailed) {
        snapshots.resize(sampleCount);
        for(auto& s:snapshots) {
            query(D3DQUERYTYPE_TIMESTAMPFREQ,s.frequency);query(D3DQUERYTYPE_TIMESTAMPDISJOINT,s.disjoint);
            query(D3DQUERYTYPE_TIMESTAMP,s.emptyBegin);query(D3DQUERYTYPE_TIMESTAMP,s.emptyEnd);
            for(auto& marker:s.markers)query(D3DQUERYTYPE_TIMESTAMP,marker);
            if(pipeline)query(D3DQUERYTYPE_PIPELINETIMINGS,s.pipeline);
        }
    }
    std::printf("SCALE_GPU_SUPPORT pipeline_hr=%08lx timestamps=1 per_draw=%d\n",static_cast<unsigned long>(pipelineResult),detailed);
    framesFile.open(directory/"gpu-frames.tsv");
    framesFile<<"block\tpercent\tsample\tphase\tcpu_ms\tgpu_interval_ms\tgpu_draw_ms\tnative_draw_cpu_ms\tdraws\tprimitives\tprepare_cpu_ms\tmodel_cpu_ms\tmodel_updates\tpresent_cpu_ms\tempty_pair_ms\tgpu_idle_percent\n";
    targetsFile.open(directory/"gpu-targets.tsv");
    targetsFile<<"block\tpercent\tsample\tphase\twidth\theight\tviewport_width\tviewport_height\tformat\tdraws\tprimitives\tgpu_draw_ms\tnative_draw_cpu_ms\n";
    cpuFile.open(directory/"cpu-frames.tsv");
    cpuFile<<"block\tpercent\tsample\tphase\tcpu_ms\tprepare_cpu_ms\tmodel_cpu_ms\tmodel_updates\tpresent_cpu_ms\n";
}
void BeginFrame(bool withGpu=true) {
    gpuQueries=withGpu;
    phase=Inactive;sampling=true;drawCount=0;models=0;modelMs=presentMs=0;
    for(auto& value:marked)value=false;
    if(gpuQueries)Require(SUCCEEDED(disjoint->Issue(D3DISSUE_BEGIN)) && SUCCEEDED(frequency->Issue(D3DISSUE_END)) &&
        SUCCEEDED(emptyBegin->Issue(D3DISSUE_END)) && SUCCEEDED(emptyEnd->Issue(D3DISSUE_END)),"begin sampled GPU frame");
    frameStart=Clock();
}
void Rendered() {
    if(!sampling)return;
    Require(phase==Hud,"GPU timing covered scene AA preview and HUD");Mark(PhaseCount);
    if(gpuQueries && pipeline)Require(SUCCEEDED(pipeline->Issue(D3DISSUE_END)),"end GPU pipeline timing");
    if(gpuQueries)Require(SUCCEEDED(disjoint->Issue(D3DISSUE_END)),"end GPU frame validity query");
    sampling=false;phase=Inactive;
}
void ReportCpu(int block,int percent,int sample) {
    for(unsigned i=0;i<PhaseCount;++i)
        cpuFile<<block<<'\t'<<percent<<'\t'<<sample<<'\t'<<phaseNames[i]<<'\t'<<(cpuMarkers[i+1]-cpuMarkers[i])*1000
            <<'\t'<<(cpuMarkers[0]-frameStart)*1000<<'\t'<<modelMs<<'\t'<<models<<'\t'<<presentMs<<'\n';
}
template<class T> T Read(IDirect3DQuery9* query) {
    T result{};const double until=Clock()+10;HRESULT hr=S_FALSE;
    while((hr=query->GetData(&result,sizeof(result),D3DGETDATA_FLUSH))==S_FALSE && Clock()<until)Sleep(0);
    Require(hr==S_OK,"read completed GPU timestamp");return result;
}
bool Report(int block,int percent,int sample) {
    const auto finish=Read<UINT64>(markers[PhaseCount].Get());
    const BOOL invalid=Read<BOOL>(disjoint.Get());const UINT64 freq=Read<UINT64>(frequency.Get());
    if(invalid || !freq){std::printf("SCALE_GPU_REJECT block=%d sample=%d disjoint=%d frequency=%llu\n",block,sample,invalid,freq);return false;}
    UINT64 times[PhaseCount+1]{};times[PhaseCount]=finish;
    for(unsigned i=0;i<PhaseCount;++i)times[i]=Read<UINT64>(markers[i].Get());
    const auto delta=[&](UINT64 a,UINT64 b){Require(b>=a,"monotonic GPU timestamps");return double(b-a)*1000/double(freq);};
    const double empty=delta(Read<UINT64>(emptyBegin.Get()),Read<UINT64>(emptyEnd.Get()));
    float idle=-1;
    if(pipeline)idle=Read<D3DDEVINFO_D3D9PIPELINETIMINGS>(pipeline.Get()).GPUIdleTimePercent;
    struct Row { Phase phase;D3DSURFACE_DESC target;D3DVIEWPORT9 viewport;unsigned count=0;std::uint64_t primitives=0;double gpu=0,cpu=0; };
    std::vector<Row> rows;
    double gpuDraw[PhaseCount]{},cpuDraw[PhaseCount]{};unsigned calls[PhaseCount]{};std::uint64_t primitives[PhaseCount]{};
    for(unsigned i=0;i<drawCount;++i) {
        const auto& d=draws[i];const double gpu=delta(Read<UINT64>(d.begin.Get()),Read<UINT64>(d.end.Get()));
        gpuDraw[d.phase]+=gpu;cpuDraw[d.phase]+=d.cpuMs;++calls[d.phase];primitives[d.phase]+=d.primitives;
        auto found=std::find_if(rows.begin(),rows.end(),[&](const Row& r){return r.phase==d.phase &&
            r.target.Width==d.target.Width && r.target.Height==d.target.Height && r.target.Format==d.target.Format &&
            r.viewport.Width==d.viewport.Width && r.viewport.Height==d.viewport.Height;});
        if(found==rows.end()){rows.push_back({d.phase,d.target,d.viewport});found=rows.end()-1;}
        ++found->count;found->primitives+=d.primitives;found->gpu+=gpu;found->cpu+=d.cpuMs;
    }
    for(unsigned i=0;i<PhaseCount;++i) {
        Require(gpuDraw[i]<=delta(times[i],times[i+1])+.0001,"draw intervals stay within their GPU phase");
        framesFile<<block<<'\t'<<percent<<'\t'<<sample<<'\t'<<phaseNames[i]<<'\t'<<(cpuMarkers[i+1]-cpuMarkers[i])*1000
            <<'\t'<<delta(times[i],times[i+1])<<'\t'<<(detailed?gpuDraw[i]:-1)<<'\t'<<(detailed?cpuDraw[i]:-1)<<'\t'<<calls[i]<<'\t'<<primitives[i]
            <<'\t'<<(cpuMarkers[0]-frameStart)*1000<<'\t'<<modelMs<<'\t'<<models<<'\t'<<presentMs<<'\t'<<empty<<'\t'<<idle<<'\n';
    }
    for(const auto& r:rows)
        targetsFile<<block<<'\t'<<percent<<'\t'<<sample<<'\t'<<phaseNames[r.phase]<<'\t'<<r.target.Width<<'\t'<<r.target.Height
            <<'\t'<<r.viewport.Width<<'\t'<<r.viewport.Height<<'\t'<<unsigned(r.target.Format)<<'\t'<<r.count<<'\t'<<r.primitives<<'\t'<<r.gpu<<'\t'<<r.cpu<<'\n';
    framesFile.flush();targetsFile.flush();return true;
}
void Shutdown() {
    draws.clear();snapshots.clear();for(auto& marker:markers)marker.Reset();frequency.Reset();disjoint.Reset();
    emptyBegin.Reset();emptyEnd.Reset();pipeline.Reset();framesFile.close();targetsFile.close();cpuFile.close();
}
}
