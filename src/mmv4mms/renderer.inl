void ReadRenderToon(IDirect3DTexture9* texture,::mmv::Texture& out) {
    if(!texture)return;D3DSURFACE_DESC desc{};D3DLOCKED_RECT lock{};
    if(FAILED(texture->GetLevelDesc(0,&desc))||(desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8)||desc.Width>4096||desc.Height>4096||FAILED(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)))throw std::runtime_error("MMV shared toon read failed");
    struct Unlock {IDirect3DTexture9* texture;~Unlock(){texture->UnlockRect(0);}} unlock{texture};out.path.clear();out.width=desc.Width;out.height=desc.Height;out.pixels.resize(size_t(desc.Width)*desc.Height*4);
    for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x){const auto* p=static_cast<const unsigned char*>(lock.pBits)+y*lock.Pitch+x*4;auto* d=out.pixels.data()+(size_t(y)*desc.Width+x)*4;d[0]=p[2]/255.f;d[1]=p[1]/255.f;d[2]=p[0]/255.f;d[3]=desc.Format==D3DFMT_X8R8G8B8?1.f:p[3]/255.f;}
}
int Render(MMDApp* app,const float world[16]) {
    Initialize(app);rendererColor.Reset();if(!Active())return 0;
    auto* device=app->Renderer()->device;if(!device)return -1;
    ComPtr<IDirect3DStateBlock9> state;ComPtr<IDirect3DSurface9> target,depth,extra[3];D3DVIEWPORT9 viewport{};
    if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state))||FAILED(device->GetRenderTarget(0,&target))||FAILED(device->GetViewport(&viewport)))return -1;
    device->GetDepthStencilSurface(&depth);for(unsigned i=0;i<3;++i)device->GetRenderTarget(i+1,&extra[i]);
    struct Restore {IDirect3DDevice9* device;IDirect3DStateBlock9* state;IDirect3DSurface9* target;IDirect3DSurface9* depth;D3DVIEWPORT9 viewport;ComPtr<IDirect3DSurface9>* extra;
        void Apply(){device->SetDepthStencilSurface(nullptr);for(unsigned i=0;i<3;++i)device->SetRenderTarget(i+1,nullptr);device->SetRenderTarget(0,target);for(unsigned i=0;i<3;++i)device->SetRenderTarget(i+1,extra[i].Get());device->SetDepthStencilSurface(depth);state->Apply();device->SetViewport(&viewport);}
        ~Restore(){Apply();}
    } restore{device,state.Get(),target.Get(),depth.Get(),viewport,extra};
    try {
        std::vector<std::unique_ptr<Snapshot>> owned;std::vector<MmvObject> objects;std::vector<MmvObjectRenderData> renderData;
        std::vector<float> opacity;std::map<std::wstring,uint32_t> instances;
        uint32_t demand=MMV_INPUT_DEMAND_ALL;
        if(api.runtime.queryInputDemand){
            // Resolve needs from fresh identities and control metadata before
            // allocating the large posed/bind-pose geometry arrays.
            constexpr uint32_t metadataDemand=MMV_INPUT_DEMAND_MATERIALS|MMV_INPUT_DEMAND_BONES|
                MMV_INPUT_DEMAND_MORPHS|MMV_INPUT_DEMAND_RIGIDS|MMV_INPUT_DEMAND_JOINTS;
            std::vector<std::unique_ptr<Snapshot>> metadata;std::vector<MmvObject> views;
            std::map<std::wstring,uint32_t> metadataInstances;
            for(int i=0;i<kModelSlotCount;++i)if(auto* model=app->ModelSlot(i))
                metadata.push_back(ModelSnapshot(model,metadataInstances[L"M:"+std::wstring(mdl::Mdl(model)->path)]++,world,metadataDemand));
            for(int i=0;i<255;++i)if(auto* accessory=app->AccessorySlot(i);accessory&&accessory->mesh)
                metadata.push_back(AccessorySnapshot(accessory,metadataInstances[L"A:"+std::wstring(accessory->sourcePath)]++,metadataDemand));
            for(auto& item:metadata)views.push_back(item->View());
            MmvFrame query{};query.structureSize=sizeof(query);query.frameId=frameNumber+1;
            query.objects=views.data();query.objectCount=uint32_t(views.size());
            query.width=viewport.Width;query.height=viewport.Height;
            query.timeSeconds=app->state.playbackActive?app->state.playbackCursorSeconds:double(app->CurrentFrame())/30.;
            device->GetTransform(D3DTS_PROJECTION,reinterpret_cast<D3DMATRIX*>(query.projection));
            uint32_t requested=MMV_INPUT_DEMAND_ALL;
            if(api.runtime.queryInputDemand(plugin,&query,&requested)==MMV_OK){
                // Native scene drawing always needs these channels, including
                // when an older plugin reports no custom effect consumer.
                demand=requested|MMV_INPUT_DEMAND_VERTICES|MMV_INPUT_DEMAND_TRIANGLES|MMV_INPUT_DEMAND_MATERIALS|
                    MMV_INPUT_DEMAND_BONES|MMV_INPUT_DEMAND_MORPHS;
            }
        }
        for(int i=0;i<kModelSlotCount;++i)if(auto* model=app->ModelSlot(i)) {
            // Hidden PMM models can be MME controllers (for example ray_controller).
            // Preserve their identity, bones and morphs; ModelSnapshot carries
            // the visibility flag separately from scene membership.
            owned.push_back(ModelSnapshot(model,instances[L"M:"+std::wstring(mdl::Mdl(model)->path)]++,world,demand));
        }
        for(int i=0;i<255;++i)if(auto* accessory=app->AccessorySlot(i)) {
            if(accessory->mesh)owned.push_back(AccessorySnapshot(accessory,instances[L"A:"+std::wstring(accessory->sourcePath)]++,demand));
        }
        std::set<const void*> live;
        for(const auto& item:owned)live.insert(reinterpret_cast<const void*>(uintptr_t(item->object.id)));
        for(auto it=generations.begin();it!=generations.end();)if(!live.count(it->first)) {
            Notify(MMV_OBJECT_REMOVED,nullptr,reinterpret_cast<uint64_t>(it->first));physicsSnapshots.erase(reinterpret_cast<uint64_t>(it->first));it=generations.erase(it);
        }else ++it;
        std::vector<MmvRenderMaterialChannels> channels;
        for(auto& item:owned){if(item->object.kind==MMV_MODEL){auto* source=reinterpret_cast<unsigned char*>(uintptr_t(item->object.id));const auto& model=*mdl::Mdl(source);
            for(unsigned m=0;m<item->mesh.materials.size()&&m<model.materialCount;++m){const auto& material=mdl::Materials(source)[m];const int toon=static_cast<int8_t>(material.toonReference);IDirect3DTexture9* shared=nullptr;
                if(toon==-1)shared=app->ToonTexture(0);
                else if(toon>=0&&toon<10){
                    const int slot=SharedToonSlot(mdl::PmdToonFileNames(source)[toon]);
                    if(slot)shared=app->ToonTexture(slot);
                    else if(model.physicsMode!=2){wchar_t name[256]{};MultiByteToWideChar(CP_ACP,0,mdl::PmdToonFileNames(source)[toon],-1,name,256);item->mesh.materials[m].toon.path=(std::filesystem::path(model.path).parent_path()/name).wstring();}
                }
                ReadRenderToon(shared,item->mesh.materials[m].toon);
                if(model.physicsMode==2&&model.materialMorphPools.additive&&model.materialMorphPools.multiplicative){const auto& add=model.materialMorphPools.additive[m].channels;const auto& mul=model.materialMorphPools.multiplicative[m].channels;MmvRenderMaterialChannels c{};c.objectIndex=uint32_t(objects.size());c.materialIndex=m;memcpy(c.textureMultiply,mul.textureTint,16);memcpy(c.textureAdd,add.textureTint,16);memcpy(c.sphereMultiply,mul.sphereTint,16);memcpy(c.sphereAdd,add.sphereTint,16);memcpy(c.toonMultiply,mul.toonTint,16);memcpy(c.toonAdd,add.toonTint,16);channels.push_back(c);}
            }}objects.push_back(item->View());renderData.push_back(item->renderData);opacity.push_back(item->object.kind==MMV_ACCESSORY?reinterpret_cast<const mdl::AccessoryRecord*>(uintptr_t(item->object.id))->opacity:1.f);}
        MmvMmsFrame input{};input.structureSize=sizeof(input);input.abiVersion=MMV_MMS_ABI_VERSION;input.device=reinterpret_cast<uint64_t>(device);
        input.materialChannels=channels.data();input.materialChannelCount=uint32_t(channels.size());
        MaterialPassContext materialPassContext{app,&objects};
        input.materialPassContext=&materialPassContext;input.readMaterialPass=&ReadMaterialPass;
        auto& frame=input.scene;frame.structureSize=sizeof(frame);frame.frameId=++frameNumber;frame.objects=objects.data();frame.objectCount=uint32_t(objects.size());frame.objectRenderData=renderData.data();frame.objectOpacity=opacity.data();
        frame.elapsedSeconds=app->PlaybackActive()?std::max(0.f,app->DeltaTime()):0;
        snapshotNativePhysics=1;
        frame.timeSeconds=app->state.playbackActive?app->state.playbackCursorSeconds:double(app->CurrentFrame())/30.;
        D3DMATRIX view{};device->GetTransform(D3DTS_VIEW,&view);auto& dx=d3dx::Get();if(!dx.Load())throw std::runtime_error("D3DX unavailable for MMV view");
        dx.multiply(reinterpret_cast<d3dx::D3DXMATRIXF*>(frame.view),reinterpret_cast<const d3dx::D3DXMATRIXF*>(world),reinterpret_cast<const d3dx::D3DXMATRIXF*>(&view));
        device->GetTransform(D3DTS_PROJECTION,reinterpret_cast<D3DMATRIX*>(frame.projection));std::copy_n(app->LightDirection(),3,frame.lightDirection);std::copy_n(app->LightColor(),3,frame.lightColor);
        frame.width=viewport.Width;frame.height=viewport.Height;input.background[0]=input.background[1]=input.background[2]=app->state.blackBackgroundEnabled?0.f:1.f;input.background[3]=TransparentExportActive(app)?0.f:1.f;
        // MMV replaces the normal scene before its BeginScene, so generate the
        // native map here, once, using this frame's posed buffers and projection.
        // No CPU depth readback or reconstruction with a second shadow camera.
        MmvMmsSelfShadow nativeShadow{};nativeShadow.structureSize=sizeof(nativeShadow);
        nativeShadow.version=MMV_MMS_SELF_SHADOW_VERSION;nativeShadow.encoding=MMV_MMS_SHADOW_NATIVE_PROJECTED_DEPTH;
        nativeShadow.frameId=frame.frameId;
        MmvMmsShadowSampling shadowSampling{};shadowSampling.structureSize=sizeof(shadowSampling);
        shadowSampling.version=MMV_MMS_SHADOW_SAMPLING_VERSION;shadowSampling.frameId=frame.frameId;
        ComPtr<IDirect3DTexture9> shadowTexture, builtinShadowTexture;
        if(app->Renderer()->effect&&UseEffectModelRenderer(app)) {
            Check(device->BeginScene(),"Begin native MMV shadow");
            struct EndNativeScene {
                IDirect3DDevice9* device;bool active=true;
                HRESULT Finish(){active=false;return device->EndScene();}
                ~EndNativeScene(){if(active)device->EndScene();}
            } scene{device};
            const bool rendered=RenderShadowMapChecked(app,world);
            const HRESULT ended=scene.Finish();
            restore.Apply();
            Check(ended,"End native MMV shadow");
            if(!rendered)throw std::runtime_error("MMS native shadow pass failed; MMV frame discarded");
            shadowTexture=shadow_filter::VisibleTexture(app);
            D3DSURFACE_DESC description{};
            if(!shadowTexture||FAILED(shadowTexture->GetLevelDesc(0,&description))||description.Format!=D3DFMT_R32F)
                throw std::runtime_error("Invalid MMS native shadow texture");
            nativeShadow.mode=app->ShadowMode();nativeShadow.width=description.Width;nativeShadow.height=description.Height;
            nativeShadow.texture=reinterpret_cast<uint64_t>(shadowTexture.Get());
            std::memcpy(nativeShadow.lightViewProjection,&app->LightViewProjection(),sizeof(nativeShadow.lightViewProjection));
        }
        if(nativeShadow.mode) {
            const auto sampling=shadow_filter::ReadSamplingState(app);
            builtinShadowTexture=sampling.texture;
            shadowSampling.texture=reinterpret_cast<uint64_t>(builtinShadowTexture.Get());
            shadowSampling.enhanced=sampling.enhanced?1u:0u;
            std::copy_n(sampling.mapSize,4,shadowSampling.mapSize);
            shadowSampling.prefiltered=sampling.prefiltered?1u:0u;
            std::copy_n(sampling.coarseSize,2,shadowSampling.coarseSize);
            input.nativeShadowSampling=&shadowSampling;
        }
        input.nativeSelfShadow=&nativeShadow;
        input.shadowMode=nativeShadow.mode;input.shadowWidth=nativeShadow.width;input.shadowHeight=nativeShadow.height;
        MmvRenderDecorations decoration{};decoration.structureSize=sizeof(decoration);
        decoration.gridEnabled=app->GroundGridEnabled();decoration.nativeStencil=app->Renderer()->d3dInitialized;
        decoration.pmxEdgeGate=app->Renderer()->postProcessEnabled;decoration.accessorySplit=app->AccessoryRenderSplitOrder();
        decoration.groundShadowEnabled=app->GroundShadowEnabled()&&(app->PlaybackActive()||app->UsesViewportTool())&&std::isfinite(frame.lightDirection[1])&&frame.lightDirection[1]<0;
        decoration.transparentGroundShadow=app->ProjectedShadowBlendEnabled();
        ::mmv::Identity(decoration.world);::mmv::Identity(decoration.groundProjection);decoration.groundProjection[5]=0;decoration.groundProjection[13]=app->CameraPositionY()<0?-.1f:.1f;
        const float lightY=std::min(frame.lightDirection[1],-.0001f);
        decoration.groundProjection[4]=std::clamp(-frame.lightDirection[0]/lightY,-32.f,32.f);decoration.groundProjection[6]=std::clamp(-frame.lightDirection[2]/lightY,-32.f,32.f);
        const auto ground=app->ProjectedShadowMaterial();memcpy(decoration.groundColor,&ground.Diffuse,16);memcpy(decoration.groundAmbient,&ground.Ambient,16);memcpy(decoration.sceneLightAmbient,&app->SceneLight().Ambient,16);
        DWORD value=0;device->GetRenderState(D3DRS_AMBIENT,&value);decoration.ambientArgb=value;
        device->GetRenderState(D3DRS_COLORVERTEX,&value);decoration.colorVertex=value;decoration.lighting=1;
        device->GetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,&value);decoration.diffuseMaterialSource=value;device->GetRenderState(D3DRS_AMBIENTMATERIALSOURCE,&value);decoration.ambientMaterialSource=value;
        device->GetRenderState(D3DRS_SPECULARMATERIALSOURCE,&value);decoration.specularMaterialSource=value;device->GetRenderState(D3DRS_EMISSIVEMATERIALSOURCE,&value);decoration.emissiveMaterialSource=value;decoration.modelGroundTextureArgb=app->ProjectedShadowRestoreTexture()?0xa6ffffffu:0xffffffffu;
        std::vector<MmvRenderColoredVertex> lines,plane;std::vector<MmvRenderSceneObject> order;
        auto line=[&](float x,float y,float z,float a,float b,float c,uint32_t color){lines.push_back({{x,y,z},color});lines.push_back({{a,b,c},color});};
        line(0,.1f,0,0,.1f,-65,0xff0000ff);line(0,.1f,0,65,.1f,0,0xffff0000);line(0,.1f,0,0,65,0,0xff00ff00);line(0,.1f,0,0,.1f,50,0xffb4b4b4);line(0,.1f,0,-50,.1f,0,0xffb4b4b4);
        for(int t=1;t<=10;++t){const float p=t*5.f;line(p,.1f,-50,p,.1f,50,0xffb4b4b4);line(-p,.1f,-50,-p,.1f,50,0xffb4b4b4);line(-50,.1f,p,50,.1f,p,0xffb4b4b4);line(-50,.1f,-p,50,.1f,-p,0xffb4b4b4);}
        plane={{{-50,0,-50},0x00ffffff},{{-50,0,50},0x00ffffff},{{50,0,-50},0x00ffffff},{{50,0,-50},0x00ffffff},{{-50,0,50},0x00ffffff},{{50,0,50},0x00ffffff}};
        std::set<int> modelOrders;
        for(unsigned i=0;i<objects.size();++i){if(objects[i].kind==MMV_ACCESSORY){const auto* a=reinterpret_cast<const mdl::AccessoryRecord*>(uintptr_t(objects[i].id));order.push_back({i,uint32_t(a->order),uint32_t(a->shadowEnabled!=0)});}else{const auto* m=reinterpret_cast<const mdl::ModelRecord*>(uintptr_t(objects[i].id));order.push_back({i,uint32_t(m->comboSelIndex),uint32_t(!m->displayState&&modelOrders.insert(m->comboSelIndex).second)});}}
        decoration.lines=lines.data();decoration.lineCount=uint32_t(lines.size());decoration.plane=plane.data();decoration.planeCount=uint32_t(plane.size());decoration.objects=order.data();decoration.objectCount=uint32_t(order.size());input.decorations=&decoration;
        std::vector<MmvAccessoryControl> accessoryControls;
        for(const auto& object:objects)if(object.kind==MMV_ACCESSORY) {
            const auto* accessory=reinterpret_cast<const mdl::AccessoryRecord*>(uintptr_t(object.id));
            MmvAccessoryControl value{sizeof(value),0,object.id};
            std::copy_n(accessory->position,3,value.position);std::copy_n(accessory->rotation,3,value.rotation);
            value.scale=accessory->scale;value.opacity=accessory->opacity;accessoryControls.push_back(value);
        }
        input.accessoryControls=accessoryControls.data();input.accessoryControlCount=uint32_t(accessoryControls.size());
        MmvMmsOutput output{sizeof(output)};const int result=api.render(plugin,&input,&output);
        if(result!=MMV_OK||!output.color||output.frameId!=frame.frameId||output.width!=frame.width||output.height!=frame.height){Log(nullptr,2,output.diagnostic?output.diagnostic:"MMV renderer rejected frame");return -1;}
        rendererColor=reinterpret_cast<IDirect3DTexture9*>(output.color);return 1;
    }catch(const std::exception& e){Log(nullptr,2,e.what());return -1;}
}
bool Present(MMDApp* app) {
    if(!rendererColor)return false;auto* device=app->Renderer()->device;ComPtr<IDirect3DStateBlock9> state;
    if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state)))return false;
    D3DVIEWPORT9 vp{};device->GetViewport(&vp);
    struct Vertex {float x,y,z,w,u,v;};const float left=float(vp.X)-.5f,top=float(vp.Y)-.5f,right=left+vp.Width,bottom=top+vp.Height;
    const Vertex vertices[]={{left,top,0,1,0,0},{right,top,0,1,1,0},{left,bottom,0,1,0,1},{right,bottom,0,1,1,1}};
    device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);device->SetTexture(0,rendererColor.Get());
    device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);device->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);
    const HRESULT result=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));state->Apply();return SUCCEEDED(result);
}
