// Deliberate test-only readback. Production statistics never synchronize the
// GPU to inspect velocities; acceptance tests inspect the actual motion field.
struct AntialiasMotionField {
    unsigned width=0,height=0;
    std::vector<std::array<float,4>> pixels;
    float MaximumPixels() const {
        float maximum=0;
        for(const auto& value:pixels)if(value[2]>0&&value[3]>0) {
            const float x=value[0]*width,y=value[1]*height;
            if(std::isfinite(x)&&std::isfinite(y))maximum=std::max(maximum,std::sqrt(x*x+y*y));
        }
        return maximum;
    }
};
AntialiasMotionField ReadAntialiasMotionField(MMDApp* app) {
    using Microsoft::WRL::ComPtr;
    AntialiasMotionField field;
    auto* texture=antialias::motion::GetTexture(app);
    Check(texture!=nullptr,"latest motion field is available for explicit test capture");
    if(!texture)return field;
    ComPtr<IDirect3DDevice9> device;ComPtr<IDirect3DSurface9> source,system;
    D3DSURFACE_DESC desc{};texture->GetLevelDesc(0,&desc);texture->GetDevice(&device);
    Check(SUCCEEDED(texture->GetSurfaceLevel(0,&source))&&
        SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&system,nullptr))&&
        SUCCEEDED(device->GetRenderTargetData(source.Get(),system.Get())),"read actual GPU motion field in test only");
    if(!system)return field;
    D3DLOCKED_RECT locked{};
    Check(SUCCEEDED(system->LockRect(&locked,nullptr,D3DLOCK_READONLY)),"inspect actual motion pixels");
    if(!locked.pBits)return field;
    field.width=desc.Width;field.height=desc.Height;field.pixels.resize(std::size_t(desc.Width)*desc.Height);
    for(unsigned y=0;y<desc.Height;++y)std::memcpy(field.pixels.data()+std::size_t(y)*desc.Width,
        static_cast<const char*>(locked.pBits)+std::size_t(y)*locked.Pitch,desc.Width*16);
    system->UnlockRect();return field;
}
