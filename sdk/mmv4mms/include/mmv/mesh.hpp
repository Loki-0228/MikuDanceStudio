#pragma once
#include "mmv/mmv.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace mmv {
// Private adapter flag; the old MMV C header and record sizes stay unchanged.
inline constexpr uint32_t kObjectTransparentMode=2u;
inline constexpr uint32_t kObjectOpaqueEdgeColor=4u;
inline constexpr uint32_t kObjectAdditive=8u;
inline void Identity(float* m) { std::fill(m,m+16,0.f); m[0]=m[5]=m[10]=m[15]=1; }
struct Texture {
    std::wstring path;
    std::vector<float> pixels;
    uint32_t width=0,height=0;
    // Private C++ GPU lease. Never exposed through MmvTexture/C ABI.
    std::shared_ptr<void> nativeTexture;
    std::shared_ptr<void> nativeDepth; // private D3D9 scene depth for subsequent post FX
    MmvTexture View() const { return {path.empty()?nullptr:path.c_str(),pixels.empty()?nullptr:pixels.data(),width,height}; }
};
struct Material {
    MmvMaterial value{};
    std::string key,name,nameEn;
    Texture texture,sphere,toon;
    // Private C++ renderer channels. The public MmvMaterial ABI is unchanged.
    float textureMultiply[4]={1,1,1,1},textureAdd[4]={},sphereMultiply[4]={1,1,1,1},sphereAdd[4]={},toonMultiply[4]={1,1,1,1},toonAdd[4]={};
    bool neutralToon=false;
    float emissive[3]={}; // accessory D3D emissive; PMX uses its native ambient
    void Copy(const MmvMaterial& m) {
        value=m; key=m.key?m.key:"";name=m.name?m.name:"";nameEn=m.nameEn?m.nameEn:"";
        neutralToon=false;
        std::fill_n(emissive,3,0.f);
        std::fill_n(textureMultiply,4,1.f);std::fill_n(textureAdd,4,0.f);
        std::fill_n(sphereMultiply,4,1.f);std::fill_n(sphereAdd,4,0.f);
        std::fill_n(toonMultiply,4,1.f);std::fill_n(toonAdd,4,0.f);
        auto copy=[](Texture& a,const MmvTexture& b) {
            a.path=b.path?b.path:L"";a.width=b.width;a.height=b.height;
            a.pixels.clear();a.nativeTexture.reset();a.nativeDepth.reset();
            if(b.rgba&&b.width&&b.height) a.pixels.assign(b.rgba,b.rgba+size_t(b.width)*b.height*4);
        };
        copy(texture,m.texture);copy(sphere,m.sphere);copy(toon,m.toon);
    }
    MmvMaterial View() const {
        auto m=value;m.key=key.c_str();m.name=name.c_str();m.nameEn=nameEn.c_str();
        m.texture=texture.View();m.sphere=sphere.View();m.toon=toon.View();return m;
    }
};
// Optional private geometry lease. No graphics interfaces enter the C ABI.
// A renderer must call resolve immediately before binding borrowed handles.
// Unsupported layouts continue to consume Mesh::vertices/triangles.
enum NativeGeometryLayoutMask : uint32_t {
    kNativeGeometryMain=1u,kNativeGeometryOutline=2u,kNativeGeometrySmooth=4u
};
struct NativeGeometryAttribute {
    uint32_t semantic=0,semanticIndex=0,offset=0,components=0,format=0;
};
struct NativeGeometry {
    uint64_t token=0,device=0,deviceEpoch=0,session=0,evaluation=0;
    uint64_t object=0,generation=0,topologyRevision=0,poseRevision=0,geometryRevision=0;
    uint64_t vertexBuffer=0,indexBuffer=0;
    uint32_t backend=0,layout=0,usage=0,vertexCount=0,indexCount=0,stride=0,indexFormat=0;
    std::vector<NativeGeometryAttribute> attributes;
    float boundsMin[3]{},boundsMax[3]{};
    // Retain until every synchronous scene/post draw has finished.
    std::shared_ptr<void> lifetime;
    std::function<bool()> resolve;
};
struct Mesh {
    std::vector<MmvVertex> vertices;
    std::vector<MmvTriangle> triangles;
    std::vector<Material> materials;
    mutable std::vector<MmvMaterial> materialViews;
    std::shared_ptr<const NativeGeometry> nativeGeometry;
    uint64_t revision=0;
    uint32_t vertexCapacity=0,triangleCapacity=0;
    void Copy(const MmvMesh& m) {
        nativeGeometry.reset();
        if(m.vertexCount)vertices.assign(m.vertices,m.vertices+m.vertexCount);else vertices.clear();
        if(m.triangleCount)triangles.assign(m.triangles,m.triangles+m.triangleCount);else triangles.clear();
        materials.resize(m.materialCount);
        for(size_t i=0;i<materials.size();++i)materials[i].Copy(m.materials[i]);
        revision=m.revision;vertexCapacity=m.vertexCapacity;triangleCapacity=m.triangleCapacity;
    }
    MmvMesh View() const {
        materialViews.clear();materialViews.reserve(materials.size());
        for(const auto& m:materials)materialViews.push_back(m.View());
        MmvMesh m{};m.vertices=vertices.data();m.vertexCount=uint32_t(vertices.size());
        m.triangles=triangles.data();m.triangleCount=uint32_t(triangles.size());
        m.materials=materialViews.data();m.materialCount=uint32_t(materialViews.size());m.revision=revision;
        m.vertexCapacity=std::max(vertexCapacity,m.vertexCount);m.triangleCapacity=std::max(triangleCapacity,m.triangleCount);
        if(!vertices.empty()) {std::copy(vertices[0].position,vertices[0].position+3,m.boundsMin);std::copy(m.boundsMin,m.boundsMin+3,m.boundsMax);}
        for(const auto& v:vertices)for(int k=0;k<3;++k){m.boundsMin[k]=std::min(m.boundsMin[k],v.position[k]);m.boundsMax[k]=std::max(m.boundsMax[k],v.position[k]);}
        return m;
    }
};
inline void Validate(const MmvMesh& m) {
    constexpr uint32_t limit=16u*1024u*1024u;
    if(m.vertexCount>limit||m.triangleCount>limit||m.materialCount>65536||
       (m.vertexCount&&!m.vertices)||(m.triangleCount&&!m.triangles)||(m.materialCount&&!m.materials))
        throw std::runtime_error("Invalid MMV mesh ranges");
    for(uint32_t i=0;i<m.vertexCount;++i){const auto& v=m.vertices[i];
        for(int j=0;j<3;++j)if(!std::isfinite(v.position[j])||!std::isfinite(v.normal[j]))throw std::runtime_error("Non-finite MMV vertex");
        for(float f:v.uv)if(!std::isfinite(f))throw std::runtime_error("Non-finite UV");
        for(float f:v.additionalUv)if(!std::isfinite(f))throw std::runtime_error("Non-finite additional UV");
        if(!std::isfinite(v.edgeScale))throw std::runtime_error("Non-finite edge scale");
    }
    for(uint32_t i=0;i<m.triangleCount;++i){const auto& t=m.triangles[i];
        if(t.material>=m.materialCount||t.vertex[0]>=m.vertexCount||t.vertex[1]>=m.vertexCount||t.vertex[2]>=m.vertexCount)
            throw std::runtime_error("MMV output index/material out of range");
    }
    for(uint32_t i=0;i<m.materialCount;++i){const auto& a=m.materials[i];
        if(!a.key||!*a.key)throw std::runtime_error("Empty material identifier");
        for(float f:a.diffuse)if(!std::isfinite(f))throw std::runtime_error("Non-finite material");
        for(float f:a.ambient)if(!std::isfinite(f))throw std::runtime_error("Non-finite material");
        for(float f:a.specular)if(!std::isfinite(f))throw std::runtime_error("Non-finite material");
        for(float f:a.edgeColor)if(!std::isfinite(f))throw std::runtime_error("Non-finite edge color");
        if(!std::isfinite(a.power)||!std::isfinite(a.edgeSize))throw std::runtime_error("Non-finite material size");
        for(const auto* t:{&a.texture,&a.sphere,&a.toon})if(t->rgba){
            if(!t->width||!t->height||t->width>8192||t->height>8192)throw std::runtime_error("Invalid texture dimensions");
            for(size_t j=0;j<size_t(t->width)*t->height*4;++j)if(!std::isfinite(t->rgba[j]))throw std::runtime_error("Non-finite texture");
        }
    }
}
}
