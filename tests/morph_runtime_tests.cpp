// MAINTENANCE: regression-keep; purpose and cleanup conditions in tests/README.md.
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

using namespace mikudancestudio;
static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main() {
    auto model = std::make_unique<mdl::ModelRecord>();
    auto* bytes = reinterpret_cast<unsigned char*>(model.get());
    std::array<mdl::BoneRecord, 128> bones{};
    std::array<mdl::MorphRecord, 3> morphs{};
    std::array<mdl::PmxBoneMorphEntry, 2> offsets{};
    // Only two entries, both referencing the final bone. The old allocation
    // held two records but the physics pass wrote record 127 into another heap
    // allocation. Repeated entries must also accumulate/apply only once.
    for (auto& entry : offsets) {
        entry.boneIndex = 127;
        entry.translation[0] = 2.f;
        entry.rotation[3] = 1.f;
    }
    mdl::PmxGroupMorphEntry refs[] = {{0, .5f}, {-1, 1.f}, {99, 1.f}};
    mdl::PmxBoneMorphEntry invalid{};
    invalid.boneIndex = 128; invalid.rotation[3] = 1.f;
    morphs[0].type = 2; morphs[0].value = .5f;
    morphs[0].boneCount = 2; morphs[0].boneEntries = offsets.data();
    morphs[1].type = 0; morphs[1].value = .5f;
    morphs[1].groupCount = 3; morphs[1].groupEntries = refs;
    morphs[2].type = 2; morphs[2].value = 1.f;
    morphs[2].boneCount = 1; morphs[2].boneEntries = &invalid;
    model->boneCount = static_cast<unsigned>(bones.size());
    model->boneTable = bones.data();
    model->morphs = morphs.data(); model->morphCount = 3;
    model->physicsMode = 2;
    model->maxBoneLayer = -1; // Isolate accumulation from the transform/IK solver.
    for (auto& bone : bones) bone.rotQuat[3] = 1.f;
    for (int reload = 0; reload < 20; ++reload) {
        InitializeBoneMorphOffsets(bytes); // Same allocation path as LoadPMX.
        Check(mdl::BoneMorphOffsetCount(bytes) == 128, "work pool covers every bone index");
        for (int frame = 0; frame < 4; ++frame) {
            ModelApplyMorphs(bytes);
            SetPhysicsMode(bytes, 0, nullptr, 0);
            Check(std::abs(bones[127].physicsOffset[0] - 3.f) < 1e-6f,
                  "direct and grouped morphs combine once on a sparse high-index bone");
            Check(bones[0].physicsOffset[0] == 0.f && bones[126].physicsOffset[0] == 0.f,
                  "unaffected bones stay at rest");
            Check(bones[127].physicsQuat[3] == 1.f, "identity rotation survives repeated frames");
        }
    }
    morphs[0].value = morphs[1].value = morphs[2].value = 0.f;
    ModelApplyMorphs(bytes); SetPhysicsMode(bytes, 0, nullptr, 0);
    Check(bones[127].physicsOffset[0] == 0.f, "zero morph weights reset previous offsets");
    model->morphCount = 0;
    InitializeBoneMorphOffsets(bytes);
    Check(!mdl::BoneMorphOffsets(bytes) && mdl::BoneMorphOffsetCount(bytes) == 0,
          "reload without bone morphs clears old work storage");
    std::puts("Bone morph runtime regressions passed");
}
