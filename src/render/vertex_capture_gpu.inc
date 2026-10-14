// Shared shader patching above also supports a GPU-only consumer. The pool
// never overwrites a result still referenced by the current or previous frame.
struct GpuCapture::Impl {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    struct Program {
        Ptr<IDirect3DVertexShader9> source,vertex;
        Ptr<IDirect3DVertexDeclaration9> sourceDeclaration,declaration;
        Ptr<IDirect3DPixelShader9> pixel;
        unsigned streamCount=0,extraStream=0;
        std::vector<D3DVERTEXELEMENT9> elements;
    };
    struct Frame : GpuPositions {Ptr<IDirect3DSurface9> surface;};
    Ptr<IDirect3DDevice9> device;
    std::vector<Program> programs;
    std::vector<std::shared_ptr<Frame>> frames;
    static constexpr std::size_t kBudget=256u*1024u*1024u;

    bool Run(IDirect3DDevice9* d,unsigned first,unsigned count,
             std::shared_ptr<const GpuPositions>& output,std::string& error) {
        const auto ok=[&](HRESULT hr,const char* operation) {
            if(SUCCEEDED(hr))return true;
            error=std::string(operation)+": "+HResultText(hr);return false;
        };
        if(device.Get()!=d){programs.clear();frames.clear();device=d;}
        Ptr<IDirect3DVertexShader9> shader;
        Ptr<IDirect3DVertexDeclaration9> sourceDeclaration;
        if(!ok(d->GetVertexShader(&shader),"Read GPU capture shader")||!shader||
           !ok(d->GetVertexDeclaration(&sourceDeclaration),"Read GPU capture declaration")||!sourceDeclaration) {
            if(error.empty())error="GPU capture requires a vertex shader and declaration";
            return false;
        }
        auto program=std::find_if(programs.begin(),programs.end(),[&](const Program& p) {
            return p.source==shader&&p.sourceDeclaration==sourceDeclaration;
        });
        if(program==programs.end()) {
            CapturePlan plan;Program next;
            if(!PrepareCapture(d,plan,next.elements,next.streamCount,error))return false;
            auto extended=next.elements;
            extended.push_back({WORD(plan.extraStream),0,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,
                BYTE(plan.extraSemantic.usage),BYTE(plan.extraSemantic.usageIndex)});
            extended.push_back(D3DDECL_END());
            const auto pixel=BuildCapturePixelShader(plan.vs3,plan.texCoordIndex);
            if(!ok(d->CreateVertexDeclaration(extended.data(),&next.declaration),"Create GPU capture layout")||
               !ok(d->CreateVertexShader(plan.shaderCode.data(),&next.vertex),"Create GPU capture vertex shader")||
               !ok(d->CreatePixelShader(pixel.data(),&next.pixel),"Create GPU capture pixel shader"))return false;
            next.source=shader;next.sourceDeclaration=sourceDeclaration;next.extraStream=plan.extraStream;
            // Effects can generate shaders dynamically; bound the retained cache.
            if(programs.size()>=256)programs.clear();
            programs.push_back(std::move(next));program=std::prev(programs.end());
        }
        D3DCAPS9 caps{};
        if(!ok(d->GetDeviceCaps(&caps),"Read GPU capture capabilities"))return false;
        unsigned width=std::min({1024u,unsigned(caps.MaxTextureWidth),count});
        if(!width){error="GPU capture texture width unavailable";return false;}
        unsigned height=(count+width-1)/width;
        if(caps.TextureCaps&D3DPTEXTURECAPS_POW2){width=NextPowerOfTwo(width);height=NextPowerOfTwo(height);}
        if(width>caps.MaxTextureWidth||height>caps.MaxTextureHeight||std::uint64_t(width)*height*32>kBudget) {
            error="GPU capture exceeds texture or memory limits";return false;
        }
        auto reusable=std::find_if(frames.begin(),frames.end(),[&](const auto& frame) {
            return frame.use_count()==1&&frame->width==width&&frame->height==height;
        });
        std::shared_ptr<Frame> frame;
        if(reusable!=frames.end())frame=*reusable;
        else {
            std::size_t retained=0;
            for(const auto& item:frames)retained+=std::size_t(item->width)*item->height*32;
            const std::size_t bytes=std::size_t(width)*height*32;
            if(retained+bytes>kBudget) {
                frames.erase(std::remove_if(frames.begin(),frames.end(),[](const auto& item){return item.use_count()==1;}),frames.end());
                retained=0;for(const auto& item:frames)retained+=std::size_t(item->width)*item->height*32;
            }
            if(retained+bytes>kBudget){error="GPU position history exceeds memory budget";return false;}
            frame=std::make_shared<Frame>();frame->width=width;frame->height=height;
            if(!ok(d->CreateTexture(width,height,1,D3DUSAGE_RENDERTARGET,D3DFMT_A32B32G32R32F,
                    D3DPOOL_DEFAULT,&frame->texture,nullptr),"Allocate GPU position history")||
               !ok(frame->texture->GetSurfaceLevel(0,&frame->surface),"Read GPU position surface")||
               !ok(d->CreateVertexBuffer(width*height*16,D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,
                    &frame->rasterPositions,nullptr),"Allocate GPU position lookup"))return false;
            void* data=nullptr;
            if(!ok(frame->rasterPositions->Lock(0,0,&data,0),"Initialize GPU position lookup"))return false;
            auto* positions=static_cast<float*>(data);
            for(unsigned i=0;i<width*height;++i) {
                positions[4*i]=(float(i%width)+.5f)*2/width-1;
                positions[4*i+1]=1-(float(i/width)+.5f)*2/height;
                positions[4*i+2]=.5f;positions[4*i+3]=1;
            }
            frame->rasterPositions->Unlock();frames.push_back(frame);
        }
        DeviceStateGuard guard(d);
        if(!guard.Save(program->streamCount,error))return false;
        constexpr unsigned elementBytes[]={4,8,12,16,4,4,4,8,4,4,8,4,8,4,4,4,8};
        for(const auto& element:program->elements) {
            const auto& stream=guard.stream(element.Stream);
            D3DVERTEXBUFFER_DESC desc{};
            if(!stream.buffer.ptr||stream.frequency!=1||element.Type>=std::size(elementBytes)||
               FAILED(stream.buffer.ptr->GetDesc(&desc))||
               std::uint64_t(stream.offset)+std::uint64_t(first+count-1)*stream.stride+element.Offset+elementBytes[element.Type]>desc.Size) {
                error="GPU capture source stream is missing, instanced or out of bounds";return false;
            }
        }
        for(auto state:kSavedRenderStates)
            if(!ok(d->SetRenderState(state,CaptureRenderStateValue(state)),"Set GPU capture state"))return false;
        if(!ok(d->SetDepthStencilSurface(nullptr),"Unbind GPU capture depth"))return false;
        for(unsigned i=1;i<guard.renderTargetCount();++i)d->SetRenderTarget(i,nullptr);
        if(!ok(d->SetRenderTarget(0,frame->surface.Get()),"Bind GPU position history"))return false;
        const D3DVIEWPORT9 view={0,0,width,height,0,1};
        if(!ok(d->SetViewport(&view),"Set GPU position viewport")||
           !ok(d->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0),"Clear GPU positions"))return false;
        for(unsigned i=0;i<program->streamCount;++i) {
            const auto& stream=guard.stream(i);
            if(!stream.buffer.ptr||i==program->extraStream)continue;
            const auto offset=std::uint64_t(stream.offset)+std::uint64_t(first)*stream.stride;
            if(offset>UINT_MAX||!ok(d->SetStreamSource(i,stream.buffer.ptr,UINT(offset),stream.stride),"Bind GPU capture input"))return false;
        }
        if(!ok(d->SetStreamSourceFreq(program->extraStream,1),"Reset GPU lookup frequency")||
           !ok(d->SetStreamSource(program->extraStream,frame->rasterPositions.Get(),0,16),"Bind GPU position lookup")||
           !ok(d->SetVertexDeclaration(program->declaration.Get()),"Bind GPU capture declaration")||
           !ok(d->SetVertexShader(program->vertex.Get()),"Bind GPU capture vertex shader")||
           !ok(d->SetPixelShader(program->pixel.Get()),"Bind GPU capture pixel shader"))return false;
        const unsigned limit=std::max(1u,unsigned(caps.MaxPrimitiveCount));
        for(unsigned start=0;start<count;) {
            const unsigned n=std::min(limit,count-start);
            if(!ok(d->DrawPrimitive(D3DPT_POINTLIST,start,n),"Capture GPU positions"))return false;
            start+=n;
        }
        frame->firstVertex=first;frame->vertexCount=count;output=std::move(frame);return true;
    }
};
GpuCapture::GpuCapture():impl_(std::make_unique<Impl>()){}
GpuCapture::~GpuCapture()=default;
bool GpuCapture::Capture(IDirect3DDevice9* device,unsigned first,unsigned count,
    std::shared_ptr<const GpuPositions>& output,std::string& error) {
    output.reset();error.clear();
    if(!device||first>UINT_MAX-count){error="Invalid GPU capture device or vertex range";return false;}
    if(!count)return true;
    return impl_->Run(device,first,count,output,error);
}
