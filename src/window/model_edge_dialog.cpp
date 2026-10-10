// ===========================================================================
// Model-edge / English-name dialog (menu 259, sub_464BD0 family)
// ===========================================================================
// Split out of src/window/command_view_menu.cpp (the menu-251..302 command
// family).  This is MMD's "input English notation" dialog (template
// 0x29A EN / 0x299 JP, modeless at app slot 0xA0B50): it edits the English
// model name / comment and the English names of the bones, morphs and
// display groups of the active model.
//
// Dialog layout (control ids, template res 666/665):
//   667 0x29B edit  model English name      668 0x29C edit  model EN comment
//   669 0x29D combo bone list               672 0x2A0 edit  bone EN name
//   673 0x2A1 combo morph list              676 0x2A4 edit  morph EN name
//   677 0x2A5 combo display-group list      680 0x2A8 edit  group EN name
//   0x29E/0x29F, 0x2A2/0x2A3, 0x2A6/0x2A7 next/prev buttons of the combos.
//
// The dialog procedure ModelEdgeDlgProc (was Sub464BD0) and the edit
// subclass ModelEdgeEditSubclassProc (was Sub45ECC0) live in
// command_view_menu.cpp; the subclass commits an edit on Enter through the
// collector CollectEnglishNameEdit below.  Every function keeps its original x86 VA.
// =========================================================================//
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstdint>
#include <cstring>

#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/model.hpp"
#include "mikudancestudio/ported_funcs.hpp"

namespace mikudancestudio {
namespace {

// ---- dialog controls (template 0x29A EN / 0x299 JP) ----------------------
constexpr int kEditModelNameEn    = 667;  // 0x29B
constexpr int kEditModelCommentEn = 668;  // 0x29C
constexpr int kComboBoneName      = 669;  // 0x29D
constexpr int kEditBoneNameEn     = 672;  // 0x2A0
constexpr int kComboMorphName     = 673;  // 0x2A1
constexpr int kEditMorphNameEn    = 676;  // 0x2A4
constexpr int kComboGroupName     = 677;  // 0x2A5
constexpr int kEditGroupNameEn    = 680;  // 0x2A8

// ---- main-window controls touched while the dialog is up -----------------
constexpr int kMainComboModel  = 436;  // 0x1B4 model combo (disabled on open)
constexpr int kMainComboGround = 474;  // 0x1DA
constexpr int kMainComboNormal = 449;  // 0x1C1
constexpr int kMainRadioFirst  = 490;  // BM_SETCHECK: 490 on, 491..493 off

// Fixed heads of the three main-window combos rebuilt by the collector
// (x64 .rdata 0x54B6B8 / 0x54B768 / 0x551074).
const char kHeadModelCombo[] = "camera/light/accessory";
const char kHeadGroundCombo[] = "ground";
const char kHeadNormalCombo[] = "normal";

// The morph combo lists morphs[1..]; the group combo lists display groups
// starting after the fixed "Root"/"表情" slots - slot 3 when the model owns
// facial frame groups, slot 2 otherwise.
int MorphNameSlot(MMDApp* app) {
    return app->ModelEdgeComboCursor(1) + 1;
}

int GroupNameSlot(MMDApp* app, mdl::ModelRecord* model) {
    return app->ModelEdgeComboCursor(2) +
           (model->facialFrameCount != 0 ? 3 : 2);
}

// Mirror the English names into the facial display-frame rows (each row's
// targetIndex addresses the morph table).
void MirrorFacialFrameNames(mdl::ModelRecord* model) {
    mdl::FrameGroup* frames = model->displayFrames;
    mdl::MorphRecord* morphs = model->morphs;
    for (int i = 0; i < model->facialFrameCount; ++i) {
        if (frames[i].targetIndex < model->morphCount)
            strcpy_s(frames[i].nameEn, 0x14,
                     morphs[frames[i].targetIndex].nameEn);
    }
}

// Same mirror for the bone display-frame rows (the 46-byte table behind
// model->rbGroups; targetIndex addresses the bone table).  The row count is
// the dword also used by the loaders (model->rigidBodyCount).
void MirrorBoneFrameNames(mdl::ModelRecord* model) {
    mdl::FrameGroup* frames =
        static_cast<mdl::FrameGroup*>(model->rbGroups);
    mdl::BoneRecord* bones = model->boneTable;
    const std::uint32_t count = model->rigidBodyCount;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (frames[i].targetIndex < model->boneCount)
            strcpy_s(frames[i].nameEn, 0x14,
                     bones[frames[i].targetIndex].nameEn);
    }
}

// Select-all + replace + select-all + focus: the edit refresh shared by the
// three combo helpers (EM_SETSEL(0, len), EM_REPLACESEL, EM_SETSEL, focus).
void RefreshNameEdit(HWND edit, const char* text) {
    SendMessageA(edit, EM_SETSEL, 0, GetWindowTextLengthA(edit));
    SendMessageA(edit, EM_REPLACESEL, 0, reinterpret_cast<LPARAM>(text));
    SendMessageA(edit, EM_SETSEL, 0, GetWindowTextLengthA(edit));
    SetFocus(edit);
}

}  // namespace

// ===========================================================================
// 0x0043BED0 - English-name collector (original: sub_43BED0)
// ===========================================================================
// Commits one English-name edit into the model.  idx picks the target
// (matching the subclassed edits 0x29B/0x2A0/0x2A4/0x2A8):
//   0: model English name (nameEn, 20 bytes).  In English UI mode the three
//      main-window combos 436/474/449 are also rebuilt from the model slots
//      in combo order (the 1-based order byte comboSelIndex, orders 1..254,
//      first matching slot per order), restoring their old cursors.
//   1: bone English name of combo cursor 0; the bone display-frame rows
//      mirror the new name from their target bone.
//   2: morph English name (combo cursor 1 -> morph index cursor+1), only
//      when the model has facial frame groups; the facial rows mirror, then
//      PostLanguageSweep + PostLoadInit refresh the panel.
//   3: display-group English name (combo cursor 2 -> group slot cursor+3 /
//      cursor+2), capped at the 50-byte group record.
// Edit texts are capped at the record widths before the copy (19 chars for
// the 20-byte names, 49 for the 50-byte group name).
//
// NOTE: command_view_menu.cpp carries only a forward declaration of this
// function for its subclass call sites; this body is the real port used by
// the dialog helpers in this file.
// =========================================================================//
void CollectEnglishNameEdit(MMDApp* app, HWND hEdit, int idx) {  // was Sub43BED0, VA 0x0043BED0
    mdl::ModelRecord* model = mdl::Mdl(app->SelectedModel());
    char text[256];

    if (idx == 0) {  // model English name
        GetWindowTextA(hEdit, text, 256);
        if (std::strlen(text) >= 0x14)
            text[19] = '\0';
        // nameEn is a 20-byte field of ModelRecord, yet the declared size 0x32
        // (50) allowed strcpy_s to accept a 49-character source and write beyond the
        // field; only the manual truncation above kept the copy in bounds.
        // sizeof reports the true field width, and the copied text remains
        // unchanged for the 19-character inputs the truncation permits.
        // Defect B2-07.
        strcpy_s(model->nameEn, sizeof(model->nameEn), text);

        if (app->state.englishUI != 0) {
            HWND modelCombo = GetDlgItem(app->state.hwnd, kMainComboModel);
            HWND groundCombo = GetDlgItem(app->state.hwnd, kMainComboGround);
            HWND normalCombo = GetDlgItem(app->state.hwnd, kMainComboNormal);
            const int selModel =
                static_cast<int>(SendMessageA(modelCombo, CB_GETCURSEL, 0, 0));
            const int selGround =
                static_cast<int>(SendMessageA(groundCombo, CB_GETCURSEL, 0, 0));
            const int selNormal =
                static_cast<int>(SendMessageA(normalCombo, CB_GETCURSEL, 0, 0));
            SendMessageA(modelCombo, CB_RESETCONTENT, 0, 0);
            SendMessageA(groundCombo, CB_RESETCONTENT, 0, 0);
            SendMessageA(normalCombo, CB_RESETCONTENT, 0, 0);
            SendMessageA(modelCombo, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(kHeadModelCombo));
            SendMessageA(groundCombo, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(kHeadGroundCombo));
            SendMessageA(normalCombo, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(kHeadNormalCombo));
            // x64 sub_7FF7CB4AC790+0x4AC9B4..0x4ACA92: order runs 1..254
            // (inc r12d; cmp r12d,0FFh), the slot scan to 0xFF (cmp
            // edx,0FFh @0x4AC9ED) - kModelSlotCount wide on both walks
            // (255 == kModelSlotCount on x64; the literal also kept the x86
            // build walking past its 100-slot array).
            for (int order = 1; order < kModelSlotCount; ++order) {
                // first model slot carrying this combo-order byte
                mdl::ModelRecord* ordered = nullptr;
                for (int slot = 0; slot < kModelSlotCount; ++slot) {
                    unsigned char* other = app->ModelSlot(slot);
                    if (other != nullptr &&
                        mdl::Mdl(other)->comboSelIndex == order) {
                        ordered = mdl::Mdl(other);
                        break;
                    }
                }
                if (ordered == nullptr)
                    continue;
                SendMessageA(modelCombo, CB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(ordered->nameEn));
                SendMessageA(groundCombo, CB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(ordered->nameEn));
                SendMessageA(normalCombo, CB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(ordered->nameEn));
            }
            SendMessageA(modelCombo, CB_SETCURSEL, selModel, 0);
            SendMessageA(groundCombo, CB_SETCURSEL, selGround, 0);
            SendMessageA(normalCombo, CB_SETCURSEL, selNormal, 0);
        }
        return;
    }

    switch (idx) {
    case 1: {  // bone English name
        const int cursor = app->ModelEdgeComboCursor(0);
        if (cursor < 0 || static_cast<std::uint32_t>(cursor) >= model->boneCount)
            break;
        GetWindowTextA(hEdit, text, 256);
        if (std::strlen(text) >= 0x14)
            text[19] = '\0';
        mdl::BoneRecord* bones = model->boneTable;
        strcpy_s(bones[cursor].nameEn, 0x14, text);
        MirrorBoneFrameNames(model);
        break;
    }
    case 2: {  // morph English name (facial models only)
        const int slot = MorphNameSlot(app);
        if (model->facialFrameCount != 0 && slot > 0 &&
            static_cast<std::uint32_t>(slot) < model->morphCount) {
            GetWindowTextA(hEdit, text, 256);
            if (std::strlen(text) >= 0x14)
                text[19] = '\0';
            strcpy_s(model->morphs[slot].nameEn, 0x14, text);
            PostLanguageSweep(app);                        // 0x42F1E0
            MirrorFacialFrameNames(model);
            PostLoadInit(app->SelectedModel());            // 0x49C850
        }
        break;
    }
    case 3: {  // display-group English name
        const int slot = GroupNameSlot(app, model);
        if (slot < 0 || static_cast<std::uint32_t>(slot) >= model->groupCount)
            break;
        GetWindowTextA(hEdit, text, 256);
        if (std::strlen(text) >= 0x32)
            text[49] = '\0';
        mdl::DisplayGroup* groups = mdl::DisplayGroups(app->SelectedModel());
        strcpy_s(groups[slot].nameEn, 0x32, text);
        break;
    }
    default:
        break;
    }
}

// ===========================================================================
// 0x0043C430 - model-edge dialog init (original: sub_43C430)
// ===========================================================================
// WM_INITDIALOG body of the English-name dialog:
//   edit 667  <- model nameEn; edit 668 <- model commentEn with line breaks
//               normalized to CRLF ("\r" pairs and lone "\n" both become
//               "\r\n" for the multiline edit);
//   combo 669 <- bone English names bones[0..boneCount-1], select 0;
//   edit 672  <- bones[0].nameEn;
//   combo 673 <- morph English names morphs[1..morphCount-1], select 0;
//   edit 676  <- morphs[1].nameEn (only when facial frame groups exist);
//   combo 677 <- display-group English names starting at group slot 3/2
//               (with/without facial frame groups) up to groupCount-1;
//   edit 680  <- groups[first].nameEn; combo 677 select 0.
// Main window: model combo 436 disabled, radio 490 checked and 491..493
// cleared, editMode reset to 0.  Model: bone selection flags cleared, bone 0
// marked selected (selectedBone = 0).  Finishes with the panel sweep and
// zeroes the three combo cursors.
// =========================================================================//
void InitModelEdgeDialog(HWND hDlg) {  // was Sub43C430, VA 0x0043C430
    MMDApp* app = g_Block;
    mdl::ModelRecord* model = mdl::Mdl(app->SelectedModel());
    mdl::BoneRecord* bones = model->boneTable;
    mdl::MorphRecord* morphs = model->morphs;
    char buf[512];

    // model English name
    strcpy_s(buf, 0x200, model->nameEn);
    SendMessageA(GetDlgItem(hDlg, kEditModelNameEn), EM_REPLACESEL, 0,
                 reinterpret_cast<LPARAM>(buf));

    // model English comment: expand CR / LF to CRLF for the edit control
    std::size_t out = 0;
    for (const char* p = model->commentEn; *p != '\0';) {
        if (*p == '\r') {  // stored CRLF pair -> one CRLF
            buf[out++] = '\r';
            buf[out++] = '\n';
            p += 2;
        } else if (*p == '\n') {  // lone LF -> CRLF
            buf[out++] = '\r';
            buf[out++] = '\n';
            ++p;
        } else {
            buf[out++] = *p++;
        }
    }
    buf[out] = '\0';
    SendMessageA(GetDlgItem(hDlg, kEditModelCommentEn), EM_REPLACESEL, 0,
                 reinterpret_cast<LPARAM>(buf));

    // bone combo + edit
    HWND boneCombo = GetDlgItem(hDlg, kComboBoneName);
    SendMessageA(boneCombo, CB_RESETCONTENT, 0, 0);
    for (std::uint32_t j = 0; j < model->boneCount; ++j)
        SendMessageA(boneCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(bones[j].nameEn));
    SendMessageA(boneCombo, CB_SETCURSEL, 0, 0);
    HWND boneEdit = GetDlgItem(hDlg, kEditBoneNameEn);
    if (model->boneCount != 0) {
        strcpy_s(buf, 0x200, bones[0].nameEn);
        SendMessageA(boneEdit, EM_REPLACESEL, 0,
                     reinterpret_cast<LPARAM>(buf));
    } else {
        EnableWindow(boneCombo, FALSE);
        EnableWindow(boneEdit, FALSE);
    }

    // morph combo + edit (combo skips morph slot 0)
    HWND morphCombo = GetDlgItem(hDlg, kComboMorphName);
    SendMessageA(morphCombo, CB_RESETCONTENT, 0, 0);
    for (std::uint32_t k = 1; k < model->morphCount; ++k)
        SendMessageA(morphCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(morphs[k].nameEn));
    SendMessageA(morphCombo, CB_SETCURSEL, 0, 0);
    HWND morphEdit = GetDlgItem(hDlg, kEditMorphNameEn);
    if (model->facialFrameCount != 0 && model->morphCount > 1) {
        strcpy_s(buf, 0x200, morphs[1].nameEn);
        SendMessageA(morphEdit, EM_REPLACESEL, 0,
                     reinterpret_cast<LPARAM>(buf));
    } else {
        EnableWindow(morphCombo, FALSE);
        EnableWindow(morphEdit, FALSE);
    }

    // display-group combo + edit (starts after the fixed slots)
    HWND groupCombo = GetDlgItem(hDlg, kComboGroupName);
    SendMessageA(groupCombo, CB_RESETCONTENT, 0, 0);
    mdl::DisplayGroup* groups = mdl::DisplayGroups(app->SelectedModel());
    const std::uint8_t firstGroup =
        model->facialFrameCount != 0 ? 3 : 2;
    for (std::uint8_t g = firstGroup; g < model->groupCount; ++g)
        SendMessageA(groupCombo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(groups[g].nameEn));
    HWND groupEdit = GetDlgItem(hDlg, kEditGroupNameEn);
    if (firstGroup < model->groupCount) {
        strcpy_s(buf, 0x100, groups[firstGroup].nameEn);
        SendMessageA(groupEdit, EM_REPLACESEL, 0,
                     reinterpret_cast<LPARAM>(buf));
    } else {
        EnableWindow(groupCombo, FALSE);
        EnableWindow(groupEdit, FALSE);
    }
    SendMessageA(groupCombo, CB_SETCURSEL, 0, 0);

    // main window: lock the model combo, select the "model" radio group
    EnableWindow(GetDlgItem(app->state.hwnd, kMainComboModel), FALSE);
    SendMessageA(GetDlgItem(app->state.hwnd, kMainRadioFirst + 1),
                 BM_SETCHECK, 0, 0);
    SendMessageA(GetDlgItem(app->state.hwnd, kMainRadioFirst + 2),
                 BM_SETCHECK, 0, 0);
    SendMessageA(GetDlgItem(app->state.hwnd, kMainRadioFirst + 3),
                 BM_SETCHECK, 0, 0);
    SendMessageA(GetDlgItem(app->state.hwnd, kMainRadioFirst),
                 BM_SETCHECK, 1, 0);
    app->state.editMode = 0;

    // model: select bone 0
    unsigned char* selection = model->boneSelection;
    for (std::uint32_t i = 0; i < model->boneCount; ++i)
        selection[i] = 0;
    if (model->boneCount != 0) {
        selection[0] = 1;
        model->selectedBone = 0;
    } else {
        model->selectedBone = -1;
    }

    PostLanguageSweep(app);                                // 0x42F1E0
    app->ModelEdgeComboCursor(0) = 0;
    app->ModelEdgeComboCursor(1) = 0;
    app->ModelEdgeComboCursor(2) = 0;
}

// ===========================================================================
// 0x0045F240 - model-edge dialog apply (original: sub_45F240)
// ===========================================================================
// OK/close body of the English-name dialog.  Reads the English comment,
// strips the CRs of the edit's CRLF line breaks and stores it into the
// model's commentEn (256 bytes).  Commits the three name edits through the
// collector (0x43BED0 idx 0/1/2), then the group name edit (identical to
// collector idx 3: cursor 2 -> group slot +3/+2, capped at 49 chars).
// Re-runs both frame-row name mirrors (facial rows from the morph table,
// bone rows from the bone table - the collector already did them; the
// original repeats both loops) and finishes with the panel sweep, the panel
// rebuild PostLoadInit and the second-stage sweep.
// =========================================================================//
void ApplyModelEdgeDialog(HWND hDlg) {  // was Sub45F240, VA 0x0045F240
    MMDApp* app = g_Block;
    mdl::ModelRecord* model = mdl::Mdl(app->SelectedModel());
    char text[256];

    // English comment: model stores LF line breaks, edit uses CRLF
    GetWindowTextA(GetDlgItem(hDlg, kEditModelCommentEn), text, 256);
    if (std::strlen(text) >= 0x100)
        text[255] = '\0';
    char clean[256];
    char* d = clean;
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p != '\r')
            *d++ = *p;
    }
    *d = '\0';
    strcpy_s(model->commentEn, 0x100, clean);

    // model / bone / morph English names
    CollectEnglishNameEdit(app, GetDlgItem(hDlg, kEditModelNameEn), 0);    // 0x43BED0
    if (model->boneCount != 0)
        CollectEnglishNameEdit(app, GetDlgItem(hDlg, kEditBoneNameEn), 1); // 0x43BED0
    if (model->facialFrameCount != 0 && model->morphCount > 1)
        CollectEnglishNameEdit(app, GetDlgItem(hDlg, kEditMorphNameEn), 2); // 0x43BED0

    // display-group English name (collector idx-3 commit)
    mdl::DisplayGroup* groups = mdl::DisplayGroups(app->SelectedModel());
    const int groupSlot = GroupNameSlot(app, model);
    if (groupSlot >= 0 && static_cast<std::uint32_t>(groupSlot) < model->groupCount) {
        GetWindowTextA(GetDlgItem(hDlg, kEditGroupNameEn), text, 256);
        if (std::strlen(text) >= 0x32)
            text[49] = '\0';
        strcpy_s(groups[groupSlot].nameEn, 0x32, text);
    }

    // frame rows mirror the English names (repeat of the collector loops)
    MirrorFacialFrameNames(model);
    MirrorBoneFrameNames(model);

    PostLanguageSweep(app);                                // 0x42F1E0
    PostLoadInit(app->SelectedModel());                    // 0x49C850
    PostLanguageSweep2(app);                               // 0x40D070
}

// ===========================================================================
// 0x0045F050 - model-edge combo 669 refresh (original: sub_45F050)
// ===========================================================================
// Bone-combo change: commits the bone-name edit (collector idx 1), stores
// the new combo selection as cursor 0, clears the model's bone selection
// flags and marks the bone selected - except for bones whose type byte is
// 6, 7 or 9 (special/invisible bones), which leave the model without a
// selected bone (selectedBone = -1).  Panel sweep, then the bone-name edit
// is refreshed from bones[cursor].nameEn and focused.
// =========================================================================//
void SelectModelEdgeBone(HWND hDlg) {  // was Sub45F050, VA 0x0045F050
    MMDApp* app = g_Block;
    mdl::ModelRecord* model = mdl::Mdl(app->SelectedModel());

    CollectEnglishNameEdit(app, GetDlgItem(hDlg, kEditBoneNameEn), 1);    // 0x43BED0

    const int cursor = static_cast<int>(
        SendMessageA(GetDlgItem(hDlg, kComboBoneName), CB_GETCURSEL, 0, 0));
    app->ModelEdgeComboCursor(0) = cursor;

    unsigned char* selection = model->boneSelection;
    for (std::uint32_t i = 0; i < model->boneCount; ++i)
        selection[i] = 0;

    const mdl::BoneType kind = model->boneTable[cursor].type;
    if (kind == mdl::BoneType::Effector || kind == mdl::BoneType::InertTip ||
        kind == mdl::BoneType::CoRotate) {
        model->selectedBone = -1;
    } else {
        selection[cursor] = 1;
        model->selectedBone = cursor;
    }
    PostLanguageSweep(app);                                // 0x42F1E0

    char buf[256];
    strcpy_s(buf, 0x100, model->boneTable[cursor].nameEn);
    RefreshNameEdit(GetDlgItem(hDlg, kEditBoneNameEn), buf);
}

// ===========================================================================
// 0x0045EF10 - model-edge combo 673 refresh (original: sub_45EF10)
// ===========================================================================
// Morph-combo change, effective only while the model has facial frame
// groups: commits the morph-name edit (collector idx 2 - which already runs
// the panel sweep and PostLoadInit), stores the new selection as cursor 1
// and refreshes the morph-name edit from morphs[cursor+1].nameEn.
// =========================================================================//
void SelectModelEdgeMorph(HWND hDlg) {  // was Sub45EF10, VA 0x0045EF10
    MMDApp* app = g_Block;
    mdl::ModelRecord* model = mdl::Mdl(app->SelectedModel());
    if (model->facialFrameCount == 0)
        return;

    CollectEnglishNameEdit(app, GetDlgItem(hDlg, kEditMorphNameEn), 2);   // 0x43BED0

    app->ModelEdgeComboCursor(1) = static_cast<int>(
        SendMessageA(GetDlgItem(hDlg, kComboMorphName), CB_GETCURSEL, 0, 0));
    PostLanguageSweep(app);                                // 0x42F1E0

    char buf[256];
    strcpy_s(buf, 0x100, model->morphs[MorphNameSlot(app)].nameEn);
    RefreshNameEdit(GetDlgItem(hDlg, kEditMorphNameEn), buf);
}

// ===========================================================================
// 0x0045EDC0 - model-edge combo 677 refresh (original: sub_45EDC0)
// ===========================================================================
// Group-combo change: first commits the current edit text into the group
// slot named by the OLD cursor 2 (slot +3/+2, capped at 49 chars), then
// stores the new selection as cursor 2, runs the panel sweep and refreshes
// the group-name edit from the newly selected group slot.
// =========================================================================//
void SelectModelEdgeGroup(HWND hDlg) {  // was Sub45EDC0, VA 0x0045EDC0
    MMDApp* app = g_Block;
    mdl::ModelRecord* model = mdl::Mdl(app->SelectedModel());

    // commit the edit at the old group slot (collector idx-3 commit)
    char text[256];
    GetWindowTextA(GetDlgItem(hDlg, kEditGroupNameEn), text, 256);
    if (std::strlen(text) >= 0x32)
        text[49] = '\0';
    mdl::DisplayGroup* groups = mdl::DisplayGroups(app->SelectedModel());
    strcpy_s(groups[GroupNameSlot(app, model)].nameEn, 0x32, text);

    app->ModelEdgeComboCursor(2) = static_cast<int>(
        SendMessageA(GetDlgItem(hDlg, kComboGroupName), CB_GETCURSEL, 0, 0));
    PostLanguageSweep(app);                                // 0x42F1E0

    char buf[256];
    strcpy_s(buf, 0x100, groups[GroupNameSlot(app, model)].nameEn);
    RefreshNameEdit(GetDlgItem(hDlg, kEditGroupNameEn), buf);
}

}  // namespace mikudancestudio
