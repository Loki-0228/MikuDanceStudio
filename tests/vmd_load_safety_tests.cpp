// Exercise the real LoadVmdMotion entry, key registrars, and extension loaders.
// No windows or D3D device are created. Each case can also run in isolation:
// vmd_load_safety_tests.exe --case overflow|short|counts|valid|tails|camera
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <memory>
#include <share.h>
#include <string>
#include <sys/stat.h>
#include <vector>
#include "mikudancestudio/bone_scale.hpp"
#include "mikudancestudio/bone_scale_io.hpp"
#include "mikudancestudio/global_key_layout.hpp"
#include "mikudancestudio/globals.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/morph_curve.hpp"

namespace mikudancestudio { int LoadVmdMotion(MMDApp*, const char*); }
namespace {
using namespace mikudancestudio;
using Bytes = std::vector<std::uint8_t>;
void Check(bool condition, const char* description) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
template<class T> void Put(Bytes& bytes, const T& value) {
    const auto* first = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), first, first + sizeof(value));
}
void Text(Bytes& bytes, const char* text, std::size_t length) {
    const auto start = bytes.size(); bytes.resize(start + length);
    std::memcpy(bytes.data() + start, text, (std::min)(length, std::strlen(text)));
}
void U32(Bytes& bytes, std::uint32_t value) { Put(bytes, value); }
void SetU32(Bytes& bytes, std::size_t offset, std::uint32_t value) { std::memcpy(bytes.data() + offset, &value, 4); }

struct Motion {
    Bytes bytes;
    std::vector<std::size_t> counts, ends;
    std::size_t ikEntries = 0;
    explicit Motion(int version = 2, bool camera = false, bool empty = false) {
        Text(bytes, version == 1 ? "Vocaloid Motion Data file" : "Vocaloid Motion Data 0002", 30);
        Text(bytes, camera ? "\x83\x4a\x83\x81\x83\x89\x81\x45\x8f\xc6\x96\xbe" : "sample", version == 1 ? 10 : 20);
        auto count = [&]() { counts.push_back(bytes.size()); U32(bytes, empty ? 0 : 1); };
        count();
        if (!empty) {
            Text(bytes, "bone", 15); U32(bytes, 0);
            Put(bytes, 7.f); Put(bytes, 8.f); Put(bytes, 9.f);
            for (float value : {0.f, 0.f, 0.f, 1.f}) Put(bytes, value);
            bytes.resize(bytes.size() + 64);
        }
        ends.push_back(bytes.size());
        count();
        if (!empty) { Text(bytes, "face", 15); U32(bytes, 0); Put(bytes, .75f); }
        ends.push_back(bytes.size());
        count();
        if (!empty) {
            U32(bytes, 0); Put(bytes, -25.f);
            for (float value : {1.f, 2.f, 3.f, 0.f, 0.f, 0.f}) Put(bytes, value);
            bytes.resize(bytes.size() + (version == 1 ? 4 : 24), 20);
            if (version == 2) { U32(bytes, 50); bytes.push_back(0); }
        }
        ends.push_back(bytes.size());
        count();
        if (!empty) {
            U32(bytes, 0);
            for (float value : {.2f, .3f, .4f, 1.f, -1.f, 1.f}) Put(bytes, value);
        }
        ends.push_back(bytes.size());
        if (version == 2) {
            count();
            if (!empty) { U32(bytes, 0); bytes.push_back(2); Put(bytes, .04f); }
            ends.push_back(bytes.size());
        }
        count();
        if (!empty) {
            U32(bytes, 0); bytes.push_back(0);
            ikEntries = bytes.size(); U32(bytes, 1);
            Text(bytes, "bone", 20); bytes.push_back(0);
        }
        ends.push_back(bytes.size());
    }
};

struct TempFile {
    char path[MAX_PATH]{};
    TempFile() {
        char directory[MAX_PATH]{};
        Check(GetTempPathA(MAX_PATH, directory) != 0 && GetTempFileNameA(directory, "vmd", 0, path) != 0, "create temporary motion");
    }
    ~TempFile() { DeleteFileA(path); }
    void Write(const Bytes& bytes) {
        int fd = -1;
        Check(_sopen_s(&fd, path, _O_BINARY | _O_WRONLY | _O_TRUNC, _SH_DENYRW, _S_IREAD | _S_IWRITE) == 0, "open temporary motion");
        Check(bytes.empty() || _write(fd, bytes.data(), static_cast<unsigned>(bytes.size())) == static_cast<int>(bytes.size()), "write temporary motion");
        _close(fd);
    }
};

struct Fixture {
    std::unique_ptr<MMDApp> app = std::make_unique<MMDApp>();
    std::unique_ptr<D3DRenderer> renderer = std::make_unique<D3DRenderer>();
    std::unique_ptr<mdl::ModelRecord> model = std::make_unique<mdl::ModelRecord>();
    std::array<mdl::BoneRecord, 1> bones{};
    std::array<mdl::MorphRecord, 2> morphs{};
    std::array<unsigned char, 1> selected{}, physics{};
    std::vector<mdl::BoneKey> boneKeys = std::vector<mdl::BoneKey>(mdl::kBoneKeyCapacity);
    std::vector<mdl::MorphKey> morphKeys = std::vector<mdl::MorphKey>(mdl::kMorphKeyCapacity);
    std::vector<mdl::DisplayKey> displayKeys = std::vector<mdl::DisplayKey>(mdl::kDisplayKeyCapacity);
    std::vector<mdl::CameraKey> cameraKeys = std::vector<mdl::CameraKey>(mdl::kTimelineKeyCapacity);
    std::vector<mdl::LightKey> lightKeys = std::vector<mdl::LightKey>(mdl::kTimelineKeyCapacity);
    std::vector<mdl::SelfShadowKey> shadowKeys = std::vector<mdl::SelfShadowKey>(mdl::kTimelineKeyCapacity);
    MMDApp* previous = g_Block;
    TempFile file;
    explicit Fixture(bool camera = false) {
        app->state = MMDAppState{};
        std::memset(renderer.get(), 0, sizeof(*renderer)); app->Renderer() = renderer.get();
        app->ModelSlot(0) = Raw(); app->SetSelectedModelSlot(0); app->EnglishUI() = 1;
        app->state.optflag[0] = camera ? 1 : 0;
        app->CameraKeys() = cameraKeys.data(); app->LightKeys() = lightKeys.data(); app->ShadowKeys() = shadowKeys.data();
        model->boneCount = 1; model->boneTable = bones.data(); model->boneKeys = boneKeys.data();
        model->morphCount = 2; model->morphs = morphs.data(); model->morphKeys = morphKeys.data();
        model->displayKeys = displayKeys.data(); model->boneSelection = selected.data(); model->bonePhysicsState = physics.data();
        model->loadComplete = 1;
        std::strcpy(model->name, "sample"); std::strcpy(bones[0].name, "bone"); std::strcpy(morphs[1].name, "face");
        bones[0].parent = -1; bones[0].slotIndex = -1; bones[0].rotQuat[3] = 1; bones[0].type = mdl::BoneType::RotateMove;
        boneKeys[0].rotation[3] = 1; boneKeys[0].position[0] = -3;
        boneKeys[0].allocated = boneKeys.back().allocated = 1;
        morphKeys[1].value = .125f; morphKeys[1].allocated = morphKeys.back().allocated = 1;
        displayKeys[0].visible = 1; displayKeys[0].allocated = displayKeys.back().allocated = 1;
        cameraKeys[0].distance = -10; cameraKeys[0].fov = 45; cameraKeys[0].parentModel = -1;
        lightKeys[0].color[0] = .8f; shadowKeys[0].distance = .01f;
        model->undoState[0] = 4; model->undoState[1] = 3;
        auto& slot = model->undoRings[0].slots[5]; slot.operation = 9; slot.dirty = 1;
        slot.bonePose = static_cast<mdl::BonePoseSnapshot*>(std::malloc(sizeof(mdl::BonePoseSnapshot)));
        slot.auxiliaryPose = std::malloc(192);
        Check(slot.bonePose && slot.auxiliaryPose, "allocate sentinel undo");
        std::memset(slot.bonePose, 0x31, sizeof(mdl::BonePoseSnapshot)); std::memset(slot.auxiliaryPose, 0x42, 192);
        g_Block = app.get();
    }
    unsigned char* Raw() { return reinterpret_cast<unsigned char*>(model.get()); }
    ~Fixture() {
        bone_scale::Forget(Raw()); morph_curve::Forget(Raw());
        for (auto& ring : model->undoRings) for (auto& slot : ring.slots) { std::free(slot.bonePose); std::free(slot.auxiliaryPose); }
        for (auto& key : displayKeys) { std::free(key.ikStates); std::free(key.selectorStates); }
        g_Block = previous;
    }
    int Load(const Bytes& bytes) { file.Write(bytes); return LoadVmdMotion(app.get(), file.path); }
    Bytes State() const {
        Bytes bytes;
        auto append = [&](const void* data, std::size_t size) { const auto* p = static_cast<const std::uint8_t*>(data); bytes.insert(bytes.end(), p, p + size); };
        append(model.get(), sizeof(*model)); append(&app->state, sizeof(app->state));
        append(boneKeys.data(), boneKeys.size() * sizeof(boneKeys[0]));
        append(morphKeys.data(), morphKeys.size() * sizeof(morphKeys[0]));
        append(displayKeys.data(), displayKeys.size() * sizeof(displayKeys[0]));
        append(cameraKeys.data(), cameraKeys.size() * sizeof(cameraKeys[0]));
        append(lightKeys.data(), lightKeys.size() * sizeof(lightKeys[0]));
        append(shadowKeys.data(), shadowKeys.size() * sizeof(shadowKeys[0]));
        const auto& slot = model->undoRings[0].slots[5];
        append(slot.bonePose, sizeof(mdl::BonePoseSnapshot)); append(slot.auxiliaryPose, 192);
        return bytes;
    }
    void Rejected(const Bytes& bytes, const Bytes& before) {
        Check(Load(bytes) == 0, "malformed input rejected by production entry");
        Check(State() == before, "rejected input preserves all tracks, marks, undo buffers, and app state");
    }
    void Imported(const Bytes& bytes, bool populated = true) {
        Check(Load(bytes) == 1, "model production load returns completed bone seek");
        Check(model->undoState[0] == 5 && model->undoRings[0].slots[5].operation == 2, "valid input commits undo slot");
        if (populated) {
            Check(boneKeys[0].position[0] == 7 && morphKeys[1].value == .75f, "real registrars import bone and morph values");
            Check(model->undoRings[0].slots[5].dirty == 1, "one distinct overwritten bone snapshot recorded");
        }
    }
};

void Overflow() {
    Fixture fixture; const auto before = fixture.State(); Motion motion;
    motion.bytes.resize(motion.ends[0]); SetU32(motion.bytes, motion.counts[0], 0x04000000u);
    fixture.Rejected(motion.bytes, before);
}
void ShortReads() {
    for (int version : {1, 2}) for (bool camera : {false, true}) {
        Fixture fixture(camera); const auto before = fixture.State(); Motion motion(version, camera);
        for (std::size_t end : {std::size_t(0), std::size_t(29), motion.counts[0] - 1})
            fixture.Rejected(Bytes(motion.bytes.begin(), motion.bytes.begin() + end), before);
        for (std::size_t i = 0; i < motion.counts.size(); ++i) {
            for (std::size_t part : {std::size_t(1), std::size_t(2), std::size_t(3)})
                fixture.Rejected(Bytes(motion.bytes.begin(), motion.bytes.begin() + motion.counts[i] + part), before);
            fixture.Rejected(Bytes(motion.bytes.begin(), motion.bytes.begin() + motion.ends[i] - 1), before);
        }
        fixture.Rejected(Bytes(motion.bytes.begin(), motion.bytes.begin() + motion.ikEntries + 3), before);
    }
}
void Counts() {
    Fixture fixture; const auto before = fixture.State(); Motion motion;
    for (auto at : motion.counts) { auto bad = motion.bytes; SetU32(bad, at, 0xffffffffu); fixture.Rejected(bad, before); }
    auto bad = motion.bytes; SetU32(bad, motion.ikEntries, 10001); fixture.Rejected(bad, before);
    bad = motion.bytes; SetU32(bad, motion.ikEntries, 0xffffffffu); fixture.Rejected(bad, before);
}
void Valid() {
    for (int version : {1, 2}) {
        Motion motion(version);
        { Fixture fixture; fixture.Imported(motion.bytes); Check(fixture.displayKeys[0].visible == 0, "nonempty skipped sections preserve IK boundary and false visibility"); }
        for (std::size_t i = 1; i + 1 < motion.ends.size(); ++i) {
            Fixture fixture; fixture.Imported(Bytes(motion.bytes.begin(), motion.bytes.begin() + motion.ends[i]));
        }
        Motion empty(version, false, true); Fixture fixture; fixture.Imported(empty.bytes, false);
    }
}
void Tails() {
    Motion motion;
    for (const Bytes& tail : {Bytes{1, 2, 3}, Bytes{'M','M','S','M','C','R','V','1', 16, 0, 0, 0}, Bytes{'M','D','S','X','M','D','S','X'}}) {
        auto bytes = motion.bytes; bytes.insert(bytes.end(), tail.begin(), tail.end());
        Fixture fixture; fixture.Imported(bytes);
    }
    // A checksummed morph block followed by a valid scale extension proves
    // that the validated standard boundary reaches both existing codecs.
    auto bytes = motion.bytes;
    Bytes payload{'x'}, block; std::string error;
    Check(morph_curve::EncodeVmdBlock(payload, block, error), "encode bounded optional morph block");
    bytes.insert(bytes.end(), block.begin(), block.end());
    bone_scale_io::PrefixFingerprint fingerprint; fingerprint.Append(bytes.data(), bytes.size());
    bone_scale_io::VmdExtension extension; extension.prefixBytes = fingerprint.bytes(); extension.prefixCrc32 = fingerprint.crc32();
    bone_scale_io::VmdExtension::Entry entry; std::memcpy(entry.name.data(), "bone", 4);
    entry.scale.v[0] = entry.scale.v[4] = entry.scale.v[8] = 2;
    extension.entries.push_back(entry);
    const auto encoded = bone_scale_io::EncodeExtension(extension);
    bytes.insert(bytes.end(), encoded.begin(), encoded.end()); Put(bytes, std::uint64_t(encoded.size())); Text(bytes, "MDSXVMDX", 8);
    Fixture fixture; fixture.Imported(bytes);
    Check(bone_scale::Key(fixture.Raw(), 0).v[0] == 2, "scale extension follows exact morph-block boundary");
}
void Camera() {
    for (int version : {1, 2}) {
        Fixture fixture(true); Motion motion(version, true);
        Check(fixture.Load(motion.bytes) == 0, "camera path retains its historical return value");
        Check(fixture.cameraKeys[0].distance == -25 && fixture.lightKeys[0].color[0] == .2f, "camera and light production registrars import records");
        Check(fixture.cameraKeys[0].fov == (version == 1 ? 45 : 50), "old camera shared curve layout and new camera fov remain distinct");
        if (version == 2) Check(fixture.shadowKeys[0].mode == 2, "new camera path imports self shadow");
        Check(fixture.model->undoState[0] == 4, "camera load does not modify model undo");
    }
}
} // namespace

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    struct Case { const char* name; void (*run)(); };
    const Case cases[]{{"overflow", Overflow}, {"short", ShortReads}, {"counts", Counts}, {"valid", Valid}, {"tails", Tails}, {"camera", Camera}};
    if (argc == 3 && std::strcmp(argv[1], "--case") == 0) {
        for (const auto& test : cases) if (std::strcmp(argv[2], test.name) == 0) { test.run(); std::printf("VMD %s: passed\n", test.name); return 0; }
        return 2;
    }
    for (const auto& test : cases) { test.run(); std::printf("VMD %s: passed\n", test.name); }
    return 0;
}
