// ===========================================================================
// VA 0x00434B60 - VMD motion file loader  (0x147F bytes)
// ===========================================================================
// __thiscall on the APP block (not the model):
//
//   LoadVmdMotion(app, FileName)   FileName = ANSI path (the original's
//                               GetOpenFileNameA buffer; the ported file
//                               dialog hands over a wide path, so the
//                               LoadVmdFile wrapper below converts with
//                               CP_ACP first - documented deviation)
//
// Two file flavors share the entry: "Vocaloid Motion Data file" (old,
// 10-byte model name, ver=1) and "Vocaloid Motion Data 0002" (20-byte
// name, ver=2).  app+760 selects the target:
//
//   != 0  CAMERA/LIGHT path - the motion goes to the app's global tracks:
//         bone/morph sections are parsed and discarded, then camera keys
//         (old format expands its single shared 4-byte interpolation
//         curve into all 24 control bytes), light keys (sub_411900) and,
//         for ver 2, self-shadow keys (sub_4120B0).  Tail: ReloadModels +
//         three list refreshers.
//   == 0  MODEL path - model name check against ModelRecord::name (20-byte SJIS;
//         "カメラ・照明" is rejected with its own message, anything else
//         prompts OK/CANCEL), then:
//           * all three key arrays' allocation marks are cleared
//             (bone +56 of kBoneKeyCapacity x60B records, morph +16 of
//             20000x20B, IK master +20 of 1000x28B),
//           * the 30-slot undo/motion ring at model+9964 (28B slots
//             {+0 type=2, +4 zeroed, +12 current frame, +16 ptr to a
//             36B-per-bone initial-pose snapshot, +20 ptr to 192B-per-key
//             raw storage}) advances model+12724 (wraps at 30) and refills
//             the slot; the snapshot copies each bone's +320 position,
//             +332 quaternion and a byte from the model+11672 array,
//           * keyVisitMap (model+14596 x86 / +0x3CAC x64, one byte per
//             bone-key record) is zeroed, sub_4A4940 resets the
//             key allocator,
//           * bone keys: 15-byte name (LF stripped), frame, position,
//             quaternion, then a 4-channel interpolation block - channel
//             0 arrives as two u16s (the second compared against 3939)
//             and channels 1..3 as u32s, each followed by three u32s -
//             registered via sub_49D880 while the "still ok" flag holds,
//           * sub_4A49A0 links/sorts, morph keys via sub_49F190
//             (non-finite values clamped to 0), app+647532 max-frame bump,
//           * three discarded counts, then the model-registration/IK
//             section (frame, visible byte, count <= 10000, 21B records:
//             20 bytes + one flag byte) via sub_49F8C0, freed immediately,
//             max-frame bump again.
//
// Tail for the model path: _close, PanelPaint (0x414610), SelectionReeval
// (0x430510), then SeekModelFrame(model, app+0x980 current frame, app+0xA0CC4
// physics mode) - the frame-seek ported in model_frame_seek.cpp.
//
// The register chain now calls the real (app, rec) overloads of
// 0x410AA0 / 0x411900 / 0x4120B0 defined in src/window/command_control_400.cpp
// (the by-value stack records of the original are passed as pointers -
// documented stub-era deviation absorbed by those ports).  The remaining
// registrar family (0x49D880, 0x49F190, 0x49F8C0, 0x4A49A0, 0x4A4A00) is
// ported in src/model/key_registrars.cpp.
//
// Reference: IDA live disassembly of MikuMikuDance.exe v932 (sole source of
// truth; ../translated/ reference files deviate).
// =========================================================================//
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "mikudancestudio/bone_key_pool.hpp"
#include "mikudancestudio/accessory_motion_io.hpp"
#include "mikudancestudio/bone_scale.hpp"
#include "mikudancestudio/bone_scale_io.hpp"
#include "mikudancestudio/morph_curve.hpp"
#include <Windows.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <float.h>
#include <io.h>
#include <sys/stat.h>
#include <vector>

#include "mikudancestudio/globals.hpp"
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/panel_controls.hpp"
#include "mikudancestudio/text_encoding.hpp"

namespace mikudancestudio {
// loader-tail helpers, ported in other translation units
void SelectionReeval(MMDApp* app);                   // 0x430510 (ui_selection_reeval.cpp)
void RefreshLightPanel(MMDApp* app);                         // 0x411070 (ui_frame_refresh.cpp)
void RefreshSelfShadowPanel(MMDApp* app);                         // 0x411B90 (ui_frame_refresh.cpp)
void ApplyGravityTrack(MMDApp* app);                        // 0x412330 (track_apply.cpp)

namespace {

int MotionMessageBox(HWND owner, int language, const wchar_t* japanese,
                     const wchar_t* english, const wchar_t* chinese,
                     UINT flags) {
    const wchar_t* caption = language == 1 ? L"Load motion data"
        : language == 2 ? L"载入动作数据" : L"モーション読込";
    const wchar_t* body = language == 1 ? english
        : language == 2 ? chinese : japanese;
    return MessageBoxW(owner, body, caption, flags);
}

// SJIS constants from .rdata (byte-exact)
const char kCameraLightName[] =  // 0x52BCB8 "カメラ・照明"
    "\x83\x4a\x83\x81\x83\x89\x81\x45\x8f\xc6\x96\xbe";
const char kJpTitleMotionLoad[] =  // 0x52CD58 "モーション読込"
    "\x83\x82\x81\x5b\x83\x56\x83\x87\x83\x93\x83\x66\x81\x5b\x83\x5e"
    "\x93\xc7\x8d\x9e";
const char kJpNotVmd[] =  // 0x52CACC
    "\x82\xb1\x82\xcc\x83\x74\x83\x40\x83\x43\x83\x8b\x82\xcd\x20Vocaloid "
    "Motion Data file \x82\xc5\x82\xcd\x82\xa0\x82\xe8\x82\xdc\x82\xb9"
    "\x82\xf1";
const char kJpCannotOpen[] =  // 0x52CDD4 "...:%d"
    "\x83\x74\x83\x40\x83\x43\x83\x8b\x82\xaa\x93\xc7\x82\xdd\x8d\x9e"
    "\x82\xdf\x82\xdc\x82\xb9\x82\xf1:%d";
const char kJpNoCamLight[] =  // 0x52CCA4
    "\x82\xb1\x82\xcc\x83\x82\x81\x5b\x83\x56\x83\x87\x83\x93\x83\x66"
    "\x81\x5b\x83\x5e\x82\xc9\x82\xcd\x83\x4a\x83\x81\x83\x89\x81\x45"
    "\x8f\xc6\x96\xbe\x82\xcc\x83\x66\x81\x5b\x83\x5e\x82\xcd\x82\xa0"
    "\x82\xe8\x82\xdc\x82\xb9\x82\xf1";
const char kJpIsCamLight[] =  // 0x52CC20
    "\x82\xb1\x82\xcc\x83\x82\x81\x5b\x83\x56\x83\x87\x83\x93\x83\x66"
    "\x81\x5b\x83\x5e\x82\xcd\x83\x4a\x83\x81\x83\x89\x81\x45\x8f\xc6"
    "\x96\xbe\x97p\x82\xcc\x83\x66\x81\x5b\x83\x5e\x82\xc5\x82\xb7\n\n"
    "\x83\x4a\x83\x81\x83\x89\x95\xd2\x8f\x57\x83\x82\x81\x5b\x83\x68"
    "\x82\xc5\x93\xc7\x82\xdd\x8d\x9e\x82\xf1\x82\xc5\x89\xba\x82\xb3"
    "\x82\xa2";
const char kJpNoCamLightShadow[] =  // 0x52CD10
    "\x82\xb1\x82\xcc\x83\x82\x81\x5b\x83\x56\x83\x87\x83\x93\x83\x66"
    "\x81\x5b\x83\x5e\x82\xc9\x82\xcd\x81\x41\x83\x4a\x83\x81\x83\x89"
    "\x81\x45\x8f\xc6\x96\xbe\x81\x45\x83\x5a\x83\x8b\x83\x74\x89\x65"
    "\x82\xcc\x83\x66\x81\x5b\x83\x5e\x82\xcd\x8a\xdc\x82\xdc\x82\xea"
    "\x82\xdc\x82\xb9\x82\xf1";
const char kJpModelMismatch[] =  // 0x52CB48 "... %s ..."
    "\x82\xb1\x82\xcc\x83\x74\x83\x40\x83\x43\x83\x8b\x82\xcd\x20%s\x97"
    "p\x82\xcc\x83\x82\x81\x5b\x83\x56\x83\x87\x83\x93\x83\x66\x81\x5b"
    "\x83\x5e\x82\xc5\x82\xb7\n\x93\xaf\x82\xb6\x83\x7b\x81\x5b\x83\x93"
    "\x96\xbc\x82\xcc\x83\x82\x81\x5b\x83\x56\x83\x87\x83\x93\x82\xcc"
    "\x82\xdd\x82\xf0\x8e\xe6\x82\xe8\x8d\x9e\x82\xde\x8e\x96\x82\xc9"
    "\x82\xc8\x82\xe8\x82\xdc\x82\xb7\n\x91\xb1\x8d\x73\x82\xb5\x82\xdc"
    "\x82\xb7\x82\xa9\x81\x48";

}  // namespace

// ---- VA 0x00434B60 --------------------------------------------------------
int LoadVmdMotion(MMDApp* app, const char* fileName) {  // was Sub434B60, VA 0x00434B60
    auto& s = *app;
    unsigned char* const model = app->SelectedModel();

    int fileHandle;
    unsigned int count = 0;  // DstBuf - reused for every section count
    char text[256];
    unsigned char rec[96];
    unsigned int scratch;    // k
    unsigned short word;     // v68

    const HWND hwnd = s.state.hwnd;
    const int language = s.EnglishUI();

    const errno_t openErr =
        _sopen_s(&fileHandle, fileName,
                 0x8000 /*_O_BINARY*/, 0x40 /*_O_RDONLY*/, 0x80 /*_S_IREAD*/);
        if (openErr != 0) {
        wchar_t message[256]{};
        if (language == 1)
            swprintf_s(message, L"Cannot open file: %d", openErr);
        else if (language == 2)
            swprintf_s(message, L"无法打开文件：%d", openErr);
        else
            swprintf_s(message, L"ファイルを読み込めません：%d", openErr);
        MessageBoxW(hwnd, message, language == 1 ? L"Load motion data"
            : language == 2 ? L"载入动作数据" : L"モーション読込", MB_OK | MB_ICONWARNING);
        return 0;
    }

    // header + model name (the name read overwrites the header buffer)
    _read(fileHandle, text, 30);
    text[30] = 0;
    int ver = 0;
    unsigned int flavor = 0;
    int nameLen;
    if (std::strcmp(text, "Vocaloid Motion Data file") == 0) {
        ver = 1; flavor = 1; nameLen = 10;
    } else if (std::strcmp(text, "Vocaloid Motion Data 0002") == 0) {
        ver = 2; flavor = 2; nameLen = 20;
    } else {
        MotionMessageBox(hwnd, language,
            L"Vocaloid Motion Data ファイルではありません。",
            L"This is not a Vocaloid Motion Data file.",
            L"这不是 Vocaloid Motion Data 动作文件。", MB_OK | MB_ICONWARNING);
        _close(fileHandle);
        return 0;
    }
    _read(fileHandle, text, nameLen);
    text[nameLen] = 0;

    if (s.state.optflag[0] != 0) {
        // ==== camera / light / self-shadow motion =========================
        if (ver == 2 && std::memcmp(text, kCameraLightName, 13) != 0) {
            MotionMessageBox(hwnd, language,
                L"このモーションファイルにカメラ・照明・セルフシャドウのデータがありません。",
                L"This motion file has no camera, light, or self-shadow data.",
                L"此动作文件不含镜头、灯光或自阴影数据。", MB_OK | MB_ICONWARNING);
            _close(fileHandle);
            return 0;
        }
        // discard the bone keys: 15-byte name + 24 dwords each
        _read(fileHandle, &count, 4);
        for (unsigned int i = 0; i < count; ++i) {
            _read(fileHandle, text, 15);
            for (int w = 0; w < 24; ++w) _read(fileHandle, &scratch, 4);
        }
        // discard the morph keys: 15 + 4 + 4
        _read(fileHandle, &count, 4);
        for (unsigned int j = 0; j < count; ++j) {
            _read(fileHandle, text, 15);
            _read(fileHandle, &scratch, 4);
            _read(fileHandle, &scratch, 4);
        }
        if (_read(fileHandle, &count, 4) == 0) {
            MotionMessageBox(hwnd, language,
                L"このモーションファイルにカメラ・照明のデータがありません。",
                L"This motion file has no camera or light data.",
                L"此动作文件不含镜头或灯光数据。", MB_OK | MB_ICONWARNING);
            _close(fileHandle);
            return 0;
        }
        if (ver == 1) {
            unsigned char ok = 1;
            for (unsigned int k = 0; k < count; ++k) {
                std::memset(rec, 0, sizeof rec);
                *reinterpret_cast<int*>(rec + 64) = -1;
                *reinterpret_cast<int*>(rec + 68) = 0;
                _read(fileHandle, rec + 0, 4);    // frame
                _read(fileHandle, rec + 60, 4);   // distance
                _read(fileHandle, rec + 4, 4);    // eye xyz
                _read(fileHandle, rec + 8, 4);
                _read(fileHandle, rec + 12, 4);
                _read(fileHandle, rec + 16, 4);   // target xyz
                _read(fileHandle, rec + 20, 4);
                _read(fileHandle, rec + 24, 4);
                _read(fileHandle, rec + 33, 1);   // shared 4-byte curve
                _read(fileHandle, rec + 45, 1);
                _read(fileHandle, rec + 39, 1);
                _read(fileHandle, rec + 51, 1);
                // x64 @0x7FF7CB48D5A5..0x7FF7CB48D5EF: 每个共享字节只填充
                // 其后的 5 字节（20 次赋值，rec+33/39/45/51 起各成一组
                // 6 字节均匀曲线）；rec[57] 保持清零不被触碰
                for (int c = 1; c <= 5; ++c) {
                    rec[33 + c] = rec[33]; rec[45 + c] = rec[45];
                    rec[39 + c] = rec[39]; rec[51 + c] = rec[51];
                }
                *reinterpret_cast<int*>(rec + 28) = 45;  // fov default
                rec[32] = 0;                             // view flag
                if (ok && !RegisterCameraKey(app, rec)) ok = 0;
            }
        } else {
            unsigned char ok = 1;
            for (unsigned int k = 0; k < count; ++k) {
                std::memset(rec, 0, sizeof rec);
                *reinterpret_cast<int*>(rec + 64) = -1;
                *reinterpret_cast<int*>(rec + 68) = 0;
                _read(fileHandle, rec + 0, 4);
                _read(fileHandle, rec + 60, 4);
                _read(fileHandle, rec + 4, 4);
                _read(fileHandle, rec + 8, 4);
                _read(fileHandle, rec + 12, 4);
                _read(fileHandle, rec + 16, 4);
                _read(fileHandle, rec + 20, 4);
                _read(fileHandle, rec + 24, 4);
                for (int c = 0; c < 6; ++c) {     // interleaved curve bytes
                    _read(fileHandle, rec + c + 33, 1);
                    _read(fileHandle, rec + c + 45, 1);
                    _read(fileHandle, rec + c + 39, 1);
                    _read(fileHandle, rec + c + 51, 1);
                }
                _read(fileHandle, rec + 28, 4);   // fov
                _read(fileHandle, &word, 1);
                rec[32] = static_cast<unsigned char>(word != 0);
                if (ok && !RegisterCameraKey(app, rec)) ok = 0;
            }
        }
        // light keys
        _read(fileHandle, &count, 4);
        unsigned char lightOk = 1;
        for (unsigned int k = 0; k < count; ++k) {
            std::memset(rec, 0, sizeof rec);
            _read(fileHandle, rec + 0, 4);    // frame
            _read(fileHandle, rec + 16, 4);   // color rgb
            _read(fileHandle, rec + 20, 4);
            _read(fileHandle, rec + 24, 4);
            _read(fileHandle, rec + 4, 4);    // position xyz
            _read(fileHandle, rec + 8, 4);
            _read(fileHandle, rec + 12, 4);
            if (lightOk && !RegisterLightKey(app, rec)) lightOk = 0;
        }
        // self-shadow keys (ver 2 only)
        unsigned char shadowOk = 1;
        if (flavor == 2 && _read(fileHandle, &count, 4) > 0) {
            for (unsigned int m = 0; m < count; ++m) {
                std::memset(rec, 0, sizeof rec);
                _read(fileHandle, rec + 0, 4);
                _read(fileHandle, rec + 4, 1);
                _read(fileHandle, rec + 8, 4);
                if (shadowOk && !RegisterSelfShadowKey(app, rec)) shadowOk = 0;
            }
        }
        // MMS: `.x` accessory keys behind the standard end code
        // (accessory_motion_io.hpp).  Anything else there is ignored, as the
        // original reader ignores it.
        bool accessoriesApplied = false;
        {
            const long long standardEnd = _telli64(fileHandle);
            const long long fileEnd = _lseeki64(fileHandle, 0, SEEK_END);
            if (standardEnd >= 0 && fileEnd > standardEnd &&
                fileEnd - standardEnd <= 64ll * 1024 * 1024) {
                std::vector<std::uint8_t> tail(
                    static_cast<std::size_t>(fileEnd - standardEnd));
                _lseeki64(fileHandle, standardEnd, SEEK_SET);
                std::vector<accessory_motion_io::Track> tracks;
                if (_read(fileHandle, tail.data(),
                          static_cast<unsigned int>(tail.size())) ==
                        static_cast<int>(tail.size()) &&
                    accessory_motion_io::Decode(tail.data(), tail.size(), tracks))
                    accessoriesApplied = accessory_motion_io::Apply(app, tracks) > 0;
            }
        }
        _close(fileHandle);
        PanelPaint(app);
        SelectionReeval(app);
        ReloadModels(app);                              // 0x42E640
        RefreshLightPanel(app);
        RefreshSelfShadowPanel(app);
        if (accessoriesApplied)
            SyncAccessoryEditPanel(app);
        ApplyGravityTrack(app);
        return 0;  // original chains 0x412330's return value
    }

    // ==== model motion =====================================================
    if (std::strcmp(text, mdl::Mdl(model)->name) != 0) {
        if (std::memcmp(text, kCameraLightName, 13) == 0) {
            MotionMessageBox(hwnd, language,
                L"このモーションはカメラ・照明用です。カメラ編集モードで読み込んでください。",
                L"This is a camera/light motion. Load it in camera editing mode.",
                L"这是镜头/灯光动作，请切换到镜头编辑模式后载入。", MB_OK | MB_ICONWARNING);
            _close(fileHandle);
            return 0;
        }
        std::wstring sourceModelName;
        if (!text_encoding::Decode(text, 932, sourceModelName))
            sourceModelName = L"?";
        wchar_t prompt[512]{};
        if (language == 1)
            swprintf_s(prompt, L"This motion file is for '%s'.\nOnly matching bone names will be imported.\nContinue?", sourceModelName.c_str());
        else if (language == 2)
            swprintf_s(prompt, L"此动作文件属于“%s”。\n只会导入骨骼名称匹配的动作。\n是否继续？", sourceModelName.c_str());
        else
            swprintf_s(prompt, L"このモーションファイルは「%s」用です。\n同じボーン名のモーションのみ読み込みます。\n続行しますか？", sourceModelName.c_str());
        if (MessageBoxW(hwnd, prompt,
                        language == 1 ? L"Load motion data"
                            : language == 2 ? L"载入动作数据" : L"モーション読込",
                        MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
            _close(fileHandle);
            return 0;
        }
    }
    _read(fileHandle, &count, 4);
    const unsigned int boneKeyCount = count;

    // clear the allocation marks of all three key arrays
    mikudancestudio::mdl::BoneKey* const bkeys = mikudancestudio::mdl::BoneKeys(model);
    mikudancestudio::mdl::MorphKey* const mkeys = mikudancestudio::mdl::MorphKeys(model);
    mikudancestudio::mdl::DisplayKey* const ikeys = mikudancestudio::mdl::DisplayKeys(model);
    // The capacity is resolved once, outside the sweep: a per-iteration lookup
    // would take the registry lock for every slot.
    const int boneKeyCapacity = static_cast<int>(mdl::BoneKeyCapacity(*mdl::Mdl(model)));
    for (int i = 0; i < boneKeyCapacity; ++i) bkeys[i].allocated = 0;
    const auto morphCapacity1 = mdl::MorphKeyCapacity(mkeys);
    for (int i = 0; i < morphCapacity1; ++i) mkeys[i].allocated = 0;
    for (int i = 0; i < 1000; ++i) ikeys[i].allocated = 0;

    EnableWindow(GetDlgItem(hwnd, panel::kUndoButton), TRUE);
    EnableWindow(GetDlgItem(hwnd, panel::kRedoButton), FALSE);
    mdl::ModelRecord& record = *mdl::Mdl(model);
    record.undoDirty = 1;
    record.redoDirty = 0;
    std::uint32_t& ringIdx = record.undoState[0];
    if (++ringIdx >= 30) ringIdx = 0;
    record.undoState[1] = ringIdx;
    bone_scale::CaptureUndo(model);

    auto& undo = mikudancestudio::mdl::Mdl(model)->undoRings[0].slots[ringIdx];
    undo.operation = 2;
    undo.dirty = 0;
    undo.frame = s.state.currentFrame;
    if (undo.bonePose != nullptr)
        free(undo.bonePose);
    const int boneCnt = static_cast<int>(record.boneCount);
    // x64 @0x7FF7CB48DE70..0x7FF7CB48E013: 每骨骼一个 36 字节槽位
    // (boneIndex / trans / rotQuat / 物理标志)，源端等价于按
    // BoneRecord 类型化字段读取（x64 骨骼表 624 字节 stride 下
    // trans@+328、rotQuat@+340），物理标志取 model+12584[b]
    auto* const snap = static_cast<mikudancestudio::mdl::BonePoseSnapshot*>(
        operator new(boneCnt * sizeof(mikudancestudio::mdl::BonePoseSnapshot)));
    undo.bonePose = snap;
    std::memset(snap, 0,
                static_cast<std::size_t>(boneCnt) *
                    sizeof(mikudancestudio::mdl::BonePoseSnapshot));
    if (boneCnt > 0) {
        mikudancestudio::mdl::BoneRecord* const bones =
            mikudancestudio::mdl::Bones(model);
        const unsigned char* const flags =
            mikudancestudio::mdl::Mdl(model)->bonePhysicsState;
        for (int b = 0; b < boneCnt; ++b) {
            mikudancestudio::mdl::BonePoseSnapshot& slot = snap[b];
            slot.boneIndex = b;
            std::memcpy(slot.position, bones[b].trans, sizeof slot.position);
            std::memcpy(slot.rotation, bones[b].rotQuat, sizeof slot.rotation);
            slot.physicsDisabled = flags[b];
        }
    }
    if (undo.auxiliaryPose != nullptr)
        free(undo.auxiliaryPose);
    unsigned char* const rawKeys = static_cast<unsigned char*>(
        operator new(192 * boneKeyCount));
    undo.auxiliaryPose = rawKeys;
    std::memset(rawKeys, 0, 192 * boneKeyCount);
    mdl::BoneKeyVisitClear(*mdl::Mdl(model));
    ResetBoneKeyCursor(model);
    (void)rawKeys;  // consumed by the registrar family (0x49D880 et al.)

    // bone keys
    //
    // MMS addition: this import replaces the keyframes it registers, so a key
    // it overwrites takes the default LINEAR scale curve instead of keeping one
    // a previous session left on that slot.  bone_scale_io::ApplyImportedExtension
    // runs afterwards and writes the curves a v2 tail carries (a v1 tail or no
    // tail leaves the default).  The scope touches nothing else: the standard
    // VMD body, the morph block and the bone-scale container are unchanged.
    bone_scale::DefaultCurveScope defaultScaleCurve;
    unsigned char boneOk = 1;
    for (unsigned int k = 0; k < boneKeyCount; ++k) {
        _read(fileHandle, text, 15);
        text[15] = 0;
        if (char* nl = std::strchr(text, 10)) *nl = 0;
        strcpy_s(reinterpret_cast<char*>(rec), 30, text);
        _read(fileHandle, rec + 32, 4);   // frame
        _read(fileHandle, rec + 52, 4);   // position
        _read(fileHandle, rec + 56, 4);
        _read(fileHandle, rec + 60, 4);
        _read(fileHandle, rec + 36, 4);   // quaternion
        _read(fileHandle, rec + 40, 4);
        _read(fileHandle, rec + 44, 4);
        _read(fileHandle, rec + 48, 4);
        for (int ch = 0; ch < 4; ++ch) {
            if (ch != 0) {
                _read(fileHandle, &scratch, 4);
                rec[ch + 65] = static_cast<unsigned char>(scratch);
            } else {
                _read(fileHandle, &word, 2);
                rec[65] = static_cast<unsigned char>(word);
                _read(fileHandle, &word, 2);
                rec[64] = static_cast<unsigned char>(word == 3939);
            }
            _read(fileHandle, &scratch, 4);
            rec[ch + 69] = static_cast<unsigned char>(scratch);
            _read(fileHandle, &scratch, 4);
            rec[ch + 73] = static_cast<unsigned char>(scratch);
            _read(fileHandle, &scratch, 4);
            rec[ch + 77] = static_cast<unsigned char>(scratch);
        }
        if (boneOk) {
            if (!RegisterBoneKey(model, rec,
                           s.state.currentFrame, 0))
                boneOk = 0;
        }
    }
    ResetMorphKeyCursor(model);

    // morph keys
    _read(fileHandle, &count, 4);
    unsigned char morphOk = 1;
    for (unsigned int k = 0; k < count; ++k) {
        _read(fileHandle, text, 15);
        text[15] = 0;
        if (char* nl = std::strchr(text, 10)) *nl = 0;
        strcpy_s(reinterpret_cast<char*>(rec), 30, text);
        _read(fileHandle, rec + 32, 4);   // frame
        _read(fileHandle, rec + 36, 4);   // value
        if (!_finite(*reinterpret_cast<float*>(rec + 36)))
            *reinterpret_cast<float*>(rec + 36) = 0.0f;
        if (morphOk) {
            if (!RegisterMorphKeyFromRecord(model, rec,
                           s.state.currentFrame))
                morphOk = 0;
        }
    }
    unsigned int modelMax = record.maxFrame;
    if (s.LastRegisteredFrame() < modelMax)
        s.LastRegisteredFrame() = modelMax;

    // camera/light/self-shadow counts are parsed and discarded
    _read(fileHandle, &count, 4);
    _read(fileHandle, &count, 4);
    _read(fileHandle, &count, 4);
    if (_read(fileHandle, &count, 4) > 0) {
        ResetDisplayKeyCursor(model);
        unsigned char ikOk = 1;
        for (unsigned int k = 0; k < count; ++k) {
            _read(fileHandle, rec + 0, 4);   // frame
            _read(fileHandle, &word, 1);     // visible
            rec[4] = static_cast<unsigned char>(word != 0);
            _read(fileHandle, rec + 8, 4);   // IK entry count
            int n = *reinterpret_cast<int*>(rec + 8);
            if (*reinterpret_cast<unsigned int*>(rec + 8) > 0x2710u) break;
            unsigned char* entries = nullptr;
            if (n > 0)
                entries = static_cast<unsigned char*>(
                    operator new(21 * n));
            for (int e = 0; e < n; ++e) {
                _read(fileHandle, entries + 21 * e, 20);
                _read(fileHandle, &word, 1);
                entries[21 * e + 20] = static_cast<unsigned char>(word != 0);
            }
            *reinterpret_cast<int*>(rec + 16) = 0;
            if (ikOk) {
                if (!RegisterDisplayKeyFromRecord(model, *reinterpret_cast<int*>(rec), rec[4],
                               n, entries, 0, nullptr,
                               s.state.currentFrame))
                    ikOk = 0;
            }
            if (entries) free(entries);
        }
        modelMax = record.maxFrame;
        if (s.LastRegisteredFrame() < modelMax)
            s.LastRegisteredFrame() = modelMax;
    }

    // Standard keys have been registered.  Two checked tails may follow the
    // standard body, in this order: the exact morph curve block this port
    // writes, then the bone-scale extension container bone_scale_io owns.  The
    // block is consumed first, so the offset handed to the container is still
    // the byte count its writer fingerprinted.
    {
        const __int64 standardEnd = _telli64(fileHandle);
        __int64 tailStart = standardEnd;
        std::vector<std::uint8_t> block;
        if (standardEnd >= 0) {
            std::vector<std::uint8_t> bytes(16);
            if (_read(fileHandle, bytes.data(), 16) == 16) {
                const std::size_t total = morph_curve::VmdBlockSize(bytes.data(), bytes.size());
                if (total >= bytes.size() && total <= morph_curve::kMaximumPayloadBytes + 16) {
                    bytes.resize(total);
                    const unsigned rest = static_cast<unsigned>(total - 16);
                    if (rest == 0 ||
                        _read(fileHandle, bytes.data() + 16, rest) == static_cast<int>(rest)) {
                        std::vector<std::uint8_t> payload;
                        if (morph_curve::DecodeVmdBlock(bytes.data(), bytes.size(), payload) == total) {
                            block.swap(bytes);
                            tailStart = _telli64(fileHandle);
                        }
                    }
                }
            }
            // An absent or damaged block must not move the boundary: the read
            // position is restored either way.
            if (block.empty()) {
                _lseeki64(fileHandle, standardEnd, SEEK_SET);
                tailStart = standardEnd;
            }
        }
        if (!block.empty() && model != nullptr) {
            morph_curve::ApplyVmd(model, block.data(), block.size(),
                                  static_cast<int>(s.state.currentFrame));
        }
        wchar_t widePath[MAX_PATH] = {};
        MultiByteToWideChar(CP_ACP, 0, fileName, -1, widePath, MAX_PATH);
        bone_scale_io::VmdExtension extension;
        if (tailStart >= 0 && bone_scale_io::LoadVmdExtension(widePath, extension, tailStart) ==
                bone_scale_io::ExtensionLoad::Loaded)
            bone_scale_io::ApplyImportedExtension(model, extension, s.state.currentFrame);
    }

    _close(fileHandle);
    PanelPaint(app);                                    // 0x414610
    SelectionReeval(app);                               // 0x430510
    return SeekModelFrame(model, s.state.currentFrame,
                     s.PlaybackPhysicsMode());
}

// Wide-path entry used by the ported file dialog (0x461300's GetOpenFileNameA
// equivalent).  CP_ACP conversion matches what the ANSI dialog produced.
void LoadVmdFile(const wchar_t* path) {
    if (g_Block == nullptr) return;
    char ansi[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, path, -1, ansi, MAX_PATH, nullptr, nullptr);
    LoadVmdMotion(g_Block, ansi);
}

}  // namespace mikudancestudio
