// Save a real triangle through the production PMD writer in either profile.
// This fixture uses the PMD loader's CPU layout, not a D3D device or window.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/ported_funcs.hpp"

namespace {
using namespace mikudancestudio;
namespace fs = std::filesystem;
int checks = 0, failures = 0;
void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
struct Files {
    fs::path directory = fs::temp_directory_path() /
        (L"MMS-pmd-indices-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    fs::path path = directory / L"triangle.pmd";
    Files() {
        if (!fs::create_directory(directory)) {
            std::fprintf(stderr, "Could not create owned fixture directory\n");
            std::exit(2);
        }
    }
    ~Files() { fs::remove(path); fs::remove(directory); }
};
std::vector<unsigned char> Read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}
struct Fixture {
    std::unique_ptr<MMDApp> app = std::make_unique<MMDApp>();
    std::unique_ptr<mdl::ModelRecord> model = std::make_unique<mdl::ModelRecord>();
    std::array<mdl::PmdVertex, 3> vertices{};
    // ModelLoadPMD stores WORD indices in four-byte, zero-filled CPU slots.
    std::array<std::uint16_t, 6> indices{{2, 0, 0, 0, 1, 0}};
    std::array<mdl::ModelMaterialRecord, 1> materials{};
    std::array<mdl::BoneRecord, 1> bones{};
    Fixture() {
        app->state = MMDAppState{}; app->EnglishUI() = 1;
        app->ModelSlot(0) = reinterpret_cast<unsigned char*>(model.get());
        app->SetSelectedModelSlot(0);
        std::strcpy(model->name, "triangle");
        model->vertexCount = 3; model->rawVertices = vertices.data();
        vertices[1].position[0] = 1; vertices[2].position[1] = 1;
        for (auto& vertex : vertices) vertex.weightPercent = 100;
        model->indexCount = 3; model->indices = indices.data();
        model->materialCount = 1; model->materials = materials.data();
        materials[0].faceVertexCount = 3;
        model->boneCount = 1; model->boneTable = bones.data();
        bones[0].parent = -1; bones[0].type = mdl::BoneType::RotateMove;
        std::strcpy(bones[0].name, "bone"); model->groupCount = 2;
    }
};
}

int main() {
    Files files; Fixture fixture;
    SaveEnhancedModel(fixture.app.get(), files.path.c_str());
    const auto bytes = Read(files.path);
    Check(bytes.size() == 1841, "triangle PMD has its complete standard body");
    Check(bytes.size() >= 7 && std::memcmp(bytes.data(), "Pmd", 3) == 0,
          "production save emits the PMD header");
    // Header 283, vertex count 4, three vertices of 38 bytes, index count 4.
    constexpr std::size_t indexCountOffset = 283 + 4 + 3 * 38;
    constexpr std::size_t indexOffset = indexCountOffset + 4;
    const std::array<unsigned char, 10> expected{{3, 0, 0, 0, 2, 0, 0, 0, 1, 0}};
    Check(bytes.size() >= indexOffset + 6 &&
          std::memcmp(bytes.data() + indexCountOffset, expected.data(), expected.size()) == 0,
          "four-byte CPU slots serialize as packed triangle indices 2, 0, 1");
    if (bytes.size() >= indexOffset + 6) {
        std::array<std::uint16_t, 3> triangle{};
        std::memcpy(triangle.data(), bytes.data() + indexOffset, 6);
        bool valid = true;
        for (const auto index : triangle) valid = valid && index < fixture.vertices.size();
        float twiceArea = 0;
        if (valid) {
            const auto& a = fixture.vertices[triangle[0]];
            const auto& b = fixture.vertices[triangle[1]];
            const auto& c = fixture.vertices[triangle[2]];
            twiceArea = (b.position[0] - a.position[0]) * (c.position[1] - a.position[1]) -
                        (b.position[1] - a.position[1]) * (c.position[0] - a.position[0]);
        }
        Check(valid && twiceArea == 1, "re-read triangle keeps its winding and nonzero area");
    }
    Check(std::wstring(fixture.model->path) == files.path.wstring(), "successful save records the output path");
    std::printf("pmd_index_save_tests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
