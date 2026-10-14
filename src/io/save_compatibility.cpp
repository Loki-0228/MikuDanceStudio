#define NOMINMAX
#include "mikudancestudio/save_compatibility.hpp"
#include "mikudancestudio/extended_names.hpp"
#include "mikudancestudio/frame_limits.hpp"
#include "mikudancestudio/display_capacity.hpp"
#include "mikudancestudio/key_pool.hpp"
#include "mikudancestudio/bone_key_pool.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/text_encoding.hpp"
#include "mikudancestudio/panel_controls.hpp"
#include <Windows.h>
#include <array>
#include <filesystem>
#include <set>

namespace mikudancestudio::save_compatibility {
namespace {
template<class K> bool Fits(const K* keys,std::size_t dense,std::size_t capacity,std::size_t original) {
    if(dense>original)return false;
    std::size_t used = 0;
    for(std::size_t i=0;keys&&i<capacity;++i){
        if(i>=dense&&!keys[i].frame)continue;
        if (++used > original || keys[i].previous >= capacity || keys[i].next >= capacity || keys[i].frame > frame_limits::legacyMaximum) return false;
    }
    return true;
}
bool PathFits(const wchar_t* value){std::string bytes;return text_encoding::EncodeExact(value,932,bytes)&&bytes.size()<256;}
bool RawName(const char* value,std::size_t size,std::set<std::string>& seen){
    const std::string bytes(value,strnlen_s(value,size));std::wstring wide;std::string roundtrip;
    if(text_encoding::HasUtf8Bom(bytes.c_str())||!text_encoding::Decode(bytes.c_str(),932,wide)||
       !text_encoding::EncodeExact(wide.c_str(),932,roundtrip)||roundtrip!=bytes)return false;
    return seen.insert(bytes).second;
}
}
Report InspectPmm(MMDApp& app){
    Report report;
    if (app.CurrentFrame() > frame_limits::legacyMaximum ||
        app.TimelineStartFrame() > frame_limits::legacyMaximum ||
        app.LastRegisteredFrame() > frame_limits::legacyMaximum) report.needsNewPmm = true;
    for (const auto control : {panel::kPlayStartFrameEdit, panel::kPlayStopFrameEdit}) {
        char text[256]{}; std::int32_t frame = 0;
        GetWindowTextA(GetDlgItem(app.MainWindow(), control), text, sizeof text);
        if (frame_limits::Parse(text, frame) && frame > frame_limits::legacyMaximum) report.needsNewPmm = true;
    }
    for(int slot=0;slot<kModelSlotCount;++slot){
        const auto* raw=app.ModelSlot(slot);if(!raw)continue;const auto& m=*mdl::Mdl(raw);
        if(!Fits(m.boneKeys,m.boneCount,mdl::BoneKeyCapacity(m),mdl::kBoneKeyCapacity)||
           !Fits(m.morphKeys,m.morphCount,mdl::MorphKeyCapacity(m),mdl::kMorphKeyCapacity)||
           !Fits(m.displayKeys,1,mdl::KeyPoolCapacity(m.displayKeys),mdl::kDisplayKeyCapacity)||
           mdl::DisplayGroupCount(m)>255||mdl::FacialFrameCount(m)>255)report.needsNewPmm=true;
        report.namesNeedExtension|=!extended_names::LegacyNamesFit(m,false);
        std::set<std::string> bones,morphs;
        for(unsigned i=0;i<m.boneCount;++i)if(!RawName(m.boneTable[i].name,20,bones))report.canCopyLegacy=false;
        for(unsigned i=0;i<m.morphCount;++i)if(!RawName(m.morphs[i].name,20,morphs))report.canCopyLegacy=false;
        if(!PathFits(m.path)){report.canCopyLegacy=false;report.needsNewPmm=true;}
        std::set<std::string> modelName;
        if(!RawName(m.name,sizeof(m.name),modelName))report.canCopyLegacy=false;
    }
    if(!Fits(app.CameraKeys(),1,mdl::KeyPoolCapacity(app.CameraKeys()),10000)||
       !Fits(app.LightKeys(),1,mdl::KeyPoolCapacity(app.LightKeys()),10000)||
       !Fits(app.ShadowKeys(),1,mdl::KeyPoolCapacity(app.ShadowKeys()),10000)||
       !Fits(app.GravityKeys(),1,mdl::KeyPoolCapacity(app.GravityKeys()),10000))report.needsNewPmm=true;
    for(int slot=0;slot<255;++slot){
        if(!app.AccessorySlot(slot))continue;
        if(!Fits(app.AccessoryKeys(slot),1,mdl::KeyPoolCapacity(app.AccessoryKeys(slot)),10000))report.needsNewPmm=true;
        if(!PathFits(app.AccessorySlot(slot)->sourcePath)){report.canCopyLegacy=false;report.needsNewPmm=true;}
    }
    if(!PathFits(app.WavePath())||!PathFits(app.AviBackgroundPath())||!PathFits(app.PictureBackgroundPath())){report.canCopyLegacy=false;report.needsNewPmm=true;}
    if(report.needsNewPmm)report.canCopyLegacy=false;
    return report;
}
Report InspectMotion(const mdl::ModelRecord& m){
    Report report;report.namesNeedExtension=!extended_names::LegacyNamesFit(m,true);
    std::set<std::string> bones,morphs;
    for(unsigned i=0;i<m.boneCount;++i)
        if(strnlen_s(m.boneTable[i].name,20)>15||!RawName(m.boneTable[i].name,15,bones))report.canCopyLegacy=false;
    for(unsigned i=0;i<m.morphCount;++i)
        if(strnlen_s(m.morphs[i].name,20)>15||!RawName(m.morphs[i].name,15,morphs))report.canCopyLegacy=false;
    return report;
}
bool CanSavePmd(const mdl::ModelRecord& m) {
    const auto fixedGroups = m.displayRootBone ? 3u : 2u;
    if (mdl::DisplayGroupCount(m) > 255 || mdl::FacialFrameCount(m) > 255 ||
        m.boneCount > 65535 || m.morphCount > 65535 ||
        !extended_names::LegacyNamesFit(m, false)) return false;
    const auto* rows = static_cast<const mdl::FrameGroup*>(m.rbGroups);
    for (std::uint32_t i = 0; rows && i < m.rigidBodyCount; ++i)
        if (mdl::FrameGroupIndex(rows[i]) >= 255 + fixedGroups ||
            mdl::FrameTargetIndex(rows[i]) > 65535) return false;
    return true;
}
bool CopyPrefix(const wchar_t* source,std::uint64_t bytes,std::wstring& destination){
    try {
    destination.clear();const std::filesystem::path original(source);
    HANDLE input=CreateFileW(source,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(input==INVALID_HANDLE_VALUE)return false;
    HANDLE output=INVALID_HANDLE_VALUE;std::filesystem::path path;
    struct Handles {
        HANDLE& input; HANDLE& output;
        ~Handles(){if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);}
    } handles{input,output};
    for(std::uint32_t suffix=0;suffix<UINT32_MAX;++suffix){
        path=original.parent_path()/(original.stem().wstring()+L".compat"+(suffix?L"."+std::to_wstring(suffix):L"")+original.extension().wstring());
        output=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(output!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_FILE_EXISTS)break;
    }
    bool success=output!=INVALID_HANDLE_VALUE;std::array<std::uint8_t,65536> buffer{};
    while(success&&bytes){const DWORD want=static_cast<DWORD>((std::min)(bytes,std::uint64_t(buffer.size())));DWORD got=0,written=0;
        success=ReadFile(input,buffer.data(),want,&got,nullptr)&&got==want&&WriteFile(output,buffer.data(),got,&written,nullptr)&&written==got;bytes-=got;}
    if(success)success=FlushFileBuffers(output)!=FALSE;
    CloseHandle(input);input=INVALID_HANDLE_VALUE;if(output!=INVALID_HANDLE_VALUE){CloseHandle(output);output=INVALID_HANDLE_VALUE;if(!success)DeleteFileW(path.c_str());}
    if(success)destination=path.wstring();return success;
    } catch (const std::exception&) { return false; }
}
void Notify(MMDApp& app,const Report& report,const std::wstring& copy,bool attempted){
    if(!report.needsNewPmm&&!report.namesNeedExtension)return;
    const unsigned lang=app.EnglishUI()<=2?app.EnglishUI():1;
    const wchar_t* saved[]={L"完全なデータを保存しました。元の MMD 形式ではすべての名前またはキーフレームを保持できません。",L"The complete data was saved. Original MMD cannot preserve all names or keyframes.",L"完整数据已保存。原版 MMD 格式无法保留全部名称或关键帧。"};
    const wchar_t* copied[]={L"\n互換コピーを保存しました。名前は旧形式の短い表記です。\n",L"\nA compatible copy was saved using the legacy short names.\n",L"\n已自动保存兼容副本，其中名称采用旧格式的短名称。\n"};
    const wchar_t* refused[]={L"\n互換コピーは作成していません。旧形式の容量、名前の衝突または参照パスの制約により、内容を欠落させず保存できません。",L"\nNo compatible copy was created: legacy capacity, ambiguous names or reference paths prevent a usable complete copy.",L"\n未生成兼容副本：旧版容量、名称冲突或引用路径限制使其无法完整且正确地保存。"};
    const wchar_t* failed[]={L"\n互換コピーの書き込みに失敗しました。完全なファイルは保存されています。",L"\nWriting the compatible copy failed. The complete file was saved.",L"\n兼容副本写入失败，完整文件已保存。"};
    std::wstring message=saved[lang];message+=!copy.empty()?copied[lang]:attempted?failed[lang]:refused[lang];message+=copy;
    if(app.MainWindow())MessageBoxW(app.MainWindow(),message.c_str(),L"MMS",MB_OK|MB_ICONINFORMATION);
}
}
