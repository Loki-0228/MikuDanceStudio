// MAINTENANCE: regression-keep; hidden D3D9 receiver-plane and coverage checks.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "mikudancestudio/d3dx_effect.hpp"

using namespace mikudancestudio::d3dx;
using Microsoft::WRL::ComPtr;
namespace {
constexpr unsigned kSize=128;
void Check(HRESULT hr,const char* operation) {
    if(FAILED(hr)){std::printf("%s: 0x%08lx\n",operation,static_cast<unsigned long>(hr));throw std::runtime_error(operation);}
}
struct Vertex {float p[3],n[3],uv[2];};
struct Effect {
    void* value=nullptr;
    Effect()=default;
    Effect(const Effect&)=delete;
    Effect(Effect&& other)noexcept:value(other.value){other.value=nullptr;}
    ~Effect(){if(value)static_cast<IUnknown*>(value)->Release();}
};
Effect Load(IDirect3DDevice9* device,const std::filesystem::path& file) {
    std::ifstream stream(file,std::ios::binary);
    if(!stream)throw std::runtime_error("Cannot read production shader");
    const std::string source{std::istreambuf_iterator<char>(stream),{}};
    static HMODULE module=LoadLibraryW(L"d3dx9_43.dll");
    using Create=HRESULT(WINAPI*)(IDirect3DDevice9*,const void*,UINT,const void*,const void*,DWORD,void*,void**,void**);
    const auto create=reinterpret_cast<Create>(GetProcAddress(module,"D3DXCreateEffect"));
    if(!create)throw std::runtime_error("D3DXCreateEffect unavailable");
    Effect fx;void* errors=nullptr;
    const auto hr=create(device,source.data(),UINT(source.size()),nullptr,nullptr,0,nullptr,&fx.value,&errors);
    if(errors){using Data=void*(WINAPI*)(void*);std::printf("%s\n",static_cast<const char*>(EffectMethod<Data,3>(errors)(errors)));static_cast<IUnknown*>(errors)->Release();}
    Check(hr,"Compile production shader");return fx;
}
std::vector<DWORD> Draw(IDirect3DDevice9* device,void* effect,unsigned mapSize,int mode,float slope,bool split,bool legacy,float depthGap=0,int pattern=0,float phase=0) {
    ComPtr<IDirect3DTexture9> shadow;
    Check(device->CreateTexture(mapSize,mapSize,1,0,D3DFMT_R32F,D3DPOOL_MANAGED,&shadow,nullptr),"Create shadow texture");
    D3DLOCKED_RECT pixels{};Check(shadow->LockRect(0,&pixels,nullptr,0),"Lock shadow texture");
    for(unsigned y=0;y<mapSize;++y)for(unsigned x=0;x<mapSize;++x){
        auto* row=reinterpret_cast<float*>(static_cast<char*>(pixels.pBits)+y*pixels.Pitch);
        row[x]=split?(x<mapSize/2?.2f:.8f):.5f+slope*(2.f*x/mapSize-1.f)-depthGap;
        if(pattern==1)row[x]=float(x)<mapSize*.5f+(float(y)-mapSize*.5f)*.35f?.2f:.8f;
        if(pattern==2)row[x]=(x==mapSize/2)?.2f:.8f;
    }
    Check(shadow->UnlockRect(0),"Unlock shadow texture");
    D3DXMATRIXF identity{};for(unsigned i=0;i<4;++i)identity.m[i][i]=1;
    Check(FxSetMatrix(effect,"matWorldViewProj",&identity),"Set camera projection");
    auto lightProjection=identity;lightProjection.m[3][0]=2*phase/mapSize;
    Check(FxSetMatrix(effect,"matLightViewProj",&lightProjection),"Set light projection");
    const float size[4]={float(mapSize),float(mapSize),0,0};
    if(!legacy)Check(FxSetFloatArray(effect,"MdsShadowMapSize",size,4),"Set actual shadow resolution");
    const float white[4]={1,1,1,.37f},dark[4]={.2f,.2f,.2f,1},light[4]={0,0,-1,1},zero[4]={0,0,0,1},eye[4]={0,0,4,1};
    Check(FxSetFloatArray(effect,"EgColor",white,4),"Set material");
    Check(FxSetFloatArray(effect,"ToonColor",dark,4),"Set toon");
    Check(FxSetFloatArray(effect,"LightDir",light,4),"Set light");
    Check(FxSetFloatArray(effect,"SpcColor",zero,4),"Set specular");
    Check(FxSetFloatArray(effect,"Place",eye,4),"Set eye");
    Check(FxSetInt(effect,"parthf",mode==2),"Set shadow mode");
    Check(FxSetInt(effect,"transp",0),"Set material opacity");
    Check(FxSetTechnique(effect,"BufferShadowTec"),"Select native receiver");
    const Vertex vertices[]={{{-1,1,.5f-slope},{0,0,1},{0,0}},{{1,1,.5f+slope},{0,0,1},{1,0}},
                             {{-1,-1,.5f-slope},{0,0,1},{0,1}},{{1,-1,.5f+slope},{0,0,1},{1,1}}};
    Check(device->SetTexture(0,shadow.Get()),"Bind shadow map");
    Check(device->SetRenderState(D3DRS_ZENABLE,FALSE),"Disable depth testing");
    Check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"Set culling");
    Check(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE),"Disable blending");
    Check(device->SetFVF(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1),"Set vertex format");
    Check(device->BeginScene(),"Begin scene");
    UINT passes=0;Check(FxBegin(effect,&passes),"Begin effect");
    for(UINT p=0;p<passes;++p){Check(FxBeginPass(effect,p),"Begin pass");
        Check(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)),"Draw receiver");
        Check(FxEndPass(effect),"End pass");}
    Check(FxEnd(effect),"End effect");Check(device->EndScene(),"End scene");
    ComPtr<IDirect3DSurface9> target,readback;Check(device->GetRenderTarget(0,&target),"Get render target");
    Check(device->CreateOffscreenPlainSurface(kSize,kSize,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr),"Create readback");
    Check(device->GetRenderTargetData(target.Get(),readback.Get()),"Read GPU pixels");
    Check(readback->LockRect(&pixels,nullptr,D3DLOCK_READONLY),"Lock readback");
    std::vector<DWORD> output(kSize*kSize);
    for(unsigned y=0;y<kSize;++y)std::copy_n(reinterpret_cast<const DWORD*>(static_cast<const char*>(pixels.pBits)+y*pixels.Pitch),kSize,output.data()+y*kSize);
    Check(readback->UnlockRect(),"Unlock readback");Check(device->SetTexture(0,nullptr),"Unbind map");
    return output;
}
double EdgeCurvature(const std::vector<DWORD>& pixels) {
    std::vector<double> contour;
    for(unsigned y=16;y<112;++y){
        for(unsigned x=16;x<111;++x){
            const double a=(pixels[y*kSize+x]>>16)&255,b=(pixels[y*kSize+x+1]>>16)&255;
            if(a<=153&&b>153){contour.push_back(x+(153-a)/(b-a));break;}
        }
    }
    if(contour.size()!=96)throw std::runtime_error("Missing diagonal shadow contour");
    double sum=0;
    for(size_t i=1;i+1<contour.size();++i){const double d=contour[i-1]-2*contour[i]+contour[i+1];sum+=d*d;}
    return std::sqrt(sum/(contour.size()-2));
}
void Save(const std::filesystem::path& path,const std::vector<DWORD>& pixels) {
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<kSize<<" "<<kSize<<"\n255\n";
    for(auto pixel:pixels){const char rgb[]{char(pixel>>16),char(pixel>>8),char(pixel)};out.write(rgb,3);}
    if(!out)throw std::runtime_error("Cannot write GPU readback image");
}
}
int wmain(int argc,wchar_t** argv) {
    if(argc<2||argc>4){std::fputs("usage: native_shadow_tests <production SM3 shader> [--legacy] [output-directory]\n",stderr);return 2;}
    const bool legacy=argc>=3&&!wcscmp(argv[2],L"--legacy");
    const std::filesystem::path output=argc>(legacy?3:2)?argv[legacy?3:2]:L"";
    if(!output.empty())std::filesystem::create_directories(output);
    HWND window=CreateWindowExW(0,L"STATIC",L"native shadow test",WS_OVERLAPPED,0,0,kSize,kSize,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    try {
        ComPtr<IDirect3D9> d3d;d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
        if(!d3d)throw std::runtime_error("D3D9 unavailable");
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
        pp.BackBufferWidth=kSize;pp.BackBufferHeight=kSize;pp.BackBufferFormat=D3DFMT_A8R8G8B8;
        ComPtr<IDirect3DDevice9> device;
        Check(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device),"Create hidden HAL device");
        auto effect=Load(device.Get(),argv[1]);unsigned failures=0;
        for(unsigned resolution:{32u,64u})for(int mode:{1,2}){
            for(float slope:{-.15f,0.f,.15f}){
                const auto image=Draw(device.Get(),effect.value,resolution,mode,slope,false,legacy);
                unsigned dark=0,wrongAlpha=0,borderDark=0;
                for(unsigned y=16;y<112;++y)for(unsigned x=16;x<112;++x){const DWORD p=image[y*kSize+x];dark+=((p>>16)&255)<250;wrongAlpha+=std::abs(int(p>>24)-94)>1;}
                for(unsigned y=0;y<kSize;++y)for(unsigned x=0;x<kSize;++x)
                    if(x<16||x>=112||y<16||y>=112)borderDark+=((image[y*kSize+x]>>16)&255)<250;
                std::printf("plane size=%u mode=%d slope=%.2f dark=%u border_dark=%u wrong_alpha=%u\n",resolution,mode,slope,dark,borderDark,wrongAlpha);
                failures+=dark!=0||borderDark!=0||wrongAlpha!=0;
            }
            const auto image=Draw(device.Get(),effect.value,resolution,mode,0,true,legacy);
            unsigned intermediate=0,wrongSides=0;
            for(unsigned x=16;x<112;++x){const int red=(image[64*kSize+x]>>16)&255;
                if(red>55&&red<250)++intermediate;
                if((x<52&&std::abs(red-51)>1)||(x>72&&red<250))++wrongSides;
            }
            std::printf("coverage size=%u mode=%d transition_pixels=%u wrong_sides=%u\n",resolution,mode,intermediate,wrongSides);
            failures+=intermediate==0||intermediate>8||wrongSides!=0;
        }
        if(!legacy)for(unsigned resolution:{32u,64u}){
            // At this separation, the original mode-specific depth tolerance
            // accepts mode 1 and rejects mode 2. Filtering must preserve it.
            const auto mode1=Draw(device.Get(),effect.value,resolution,1,0,false,false,.00015f);
            const auto mode2=Draw(device.Get(),effect.value,resolution,2,0,false,false,.00015f);
            const unsigned lit=(mode1[64*kSize+64]>>16)&255;
            const unsigned shadow=(mode2[64*kSize+64]>>16)&255;
            std::printf("mode_difference size=%u mode1=%u mode2=%u\n",resolution,lit,shadow);
            failures+=lit<250||shadow>55;
        }
        if(!legacy)for(int mode:{1,2}){
            double curvature=0;
            for(float phase:{0.f,.09375f,.1875f,.28125f,.375f,.46875f,.5625f,.65625f}){
                const auto diagonal=Draw(device.Get(),effect.value,16,mode,0,false,false,0,1,phase);
                const double value=EdgeCurvature(diagonal);curvature+=value;
                std::printf("diagonal mode=%d phase=%.3f curvature=%.6f\n",mode,phase,value);
                if(phase==0&&!output.empty())Save(output/("diagonal-mode"+std::to_string(mode)+".ppm"),diagonal);
            }
            // The jaggedness metric measures second differences of the actual
            // 50% contour, so a uniformly blurred straight edge cannot mask it.
            std::printf("diagonal_mean mode=%d curvature=%.6f\n",mode,curvature/8);
            failures+=curvature/8>.18;
            const auto thin=Draw(device.Get(),effect.value,32,mode,0,false,false,0,2);
            const unsigned middle=(thin[64*kSize+64]>>16)&255;
            std::printf("thin_shadow mode=%d center=%u\n",mode,middle);
            failures+=middle>130;
            for(unsigned resolution:{128u,192u,256u}){
                const auto plane=Draw(device.Get(),effect.value,resolution,mode,.15f,false,false);
                unsigned dark=0;for(unsigned y=16;y<112;++y)for(unsigned x=16;x<112;++x)dark+=((plane[y*kSize+x]>>16)&255)<250;
                std::printf("minified_plane size=%u mode=%d dark=%u\n",resolution,mode,dark);failures+=dark!=0;
            }
        }
        DestroyWindow(window);std::printf("native_shadow failures=%u\n",failures);return failures?1:0;
    } catch(const std::exception& e){DestroyWindow(window);std::fprintf(stderr,"%s\n",e.what());return 1;}
}
