#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
struct IDirect3DDevice9;
namespace mikudancestudio {
class MMDApp;
namespace deformed_mesh_capture {
struct Mesh {
    std::string name;
    // Optional source-index patch. UINT32_MAX means this vertex is not selected.
    // Patch playback changes only positions/normals of addressed source vertices.
    bool vertexPatch = false;
    std::vector<std::uint32_t> sourceVertices;
    std::vector<std::array<float,3>> positions;
    std::vector<std::array<float,3>> normals;
    std::vector<std::array<float,2>> uvs;
    std::vector<std::int32_t> indices;
};
class Session {
public:
    explicit Session(MMDApp& app, bool selectedOnly);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    void BeginFrame();
    const std::vector<Mesh>& EndFrame();
    const std::string& Error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Scope only the main material draw. Shadow maps, outlines and screen passes
// are excluded. The system-device hook sees the shader selected inside MME.
class MaterialScope {
public:
    MaterialScope(const void* object, unsigned material, bool enabled);
    ~MaterialScope();
private:
    const void* oldObject_;
    unsigned oldMaterial_;
};
bool Active();
void SetCamera(IDirect3DDevice9* device, const float world[16]);
}
}
