// MAINTENANCE: hardware-manual; purpose and cleanup conditions in tests/README.md.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d9on12.h>
#include <wrl/client.h>
#include <cstdio>
#include <initializer_list>
using Microsoft::WRL::ComPtr;
int main() {
    auto dll=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto create=reinterpret_cast<PFN_Direct3DCreate9On12>(GetProcAddress(dll,"Direct3DCreate9On12"));
    if (!create) return 2;
    D3D9ON12_ARGS args{}; args.Enable9On12=TRUE;
    ComPtr<IDirect3D9> d3d; d3d.Attach(create(D3D_SDK_VERSION,&args,1));
    if (!d3d) return 3;
    HWND window=CreateWindowW(L"STATIC",L"interop",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    D3DPRESENT_PARAMETERS pp{}; pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=64; pp.BackBufferHeight=64; pp.BackBufferFormat=D3DFMT_A8R8G8B8;
    ComPtr<IDirect3DDevice9> device;
    HRESULT hr=d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device);
    std::printf("create=%08lx\n",hr); if (FAILED(hr)) return 4;
    ComPtr<IDirect3DDevice9On12> interop; hr=device.As(&interop);
    std::printf("interop=%08lx\n",hr); if (FAILED(hr)) return 5;
    ComPtr<ID3D12Device> gpu; interop->GetD3D12Device(IID_PPV_ARGS(&gpu));
    D3D12_COMMAND_QUEUE_DESC q{}; ComPtr<ID3D12CommandQueue> queue;
    if (FAILED(gpu->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)))) return 6;
    for (auto pool : {D3DPOOL_MANAGED,D3DPOOL_DEFAULT}) {
        ComPtr<IDirect3DVertexBuffer9> vb;
        hr=device->CreateVertexBuffer(4096,0,D3DFVF_XYZ,pool,&vb,nullptr);
        std::printf("pool=%u create_vb=%08lx\n",unsigned(pool),hr);
        if (FAILED(hr)) continue;
        ComPtr<ID3D12Resource> resource;
        hr=interop->UnwrapUnderlyingResource(vb.Get(),queue.Get(),IID_PPV_ARGS(&resource));
        std::printf("unwrap_vb=%08lx\n",hr);
        if (SUCCEEDED(hr)) {
            const auto desc=resource->GetDesc();
            std::printf("flags=%u width=%llu\n",unsigned(desc.Flags),desc.Width);
            std::printf("return=%08lx\n",interop->ReturnUnderlyingResource(vb.Get(),0,nullptr,nullptr));
        }
    }
    DestroyWindow(window);
}
