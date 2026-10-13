#include "../src/render/nvidia_antialias_profile.hpp"
#include "../src/render/nvidia_antialias_elevation.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
using namespace mikudancestudio::nvidia_antialias_fix;
using namespace mikudancestudio::nvidia_antialias_fix::detail;
namespace {
const std::wstring path=L"D:\\MMS only\\MikuMikuDance.exe";
struct Database {
    bool exists=false, present=false, failSave=false, mismatched=false;
    bool appFound=true, globalPresent=false, basePresent=false;
    U32 value=1, apps=1, predefined=0;
    U32 globalValue=1, baseValue=1, location=0, valuePredefined=0;
    int failGet=0;
    unsigned saves=0, sets=0, removes=0, destroys=0;
} db;
void Check(bool value,const char* name){if(!value){std::printf("FAIL %s\n",name);std::exit(1);}}
Handle target=reinterpret_cast<Handle>(123);
Handle globalTarget=reinterpret_cast<Handle>(124),baseTarget=reinterpret_cast<Handle>(125);
int __cdecl Create(Handle* s){*s=reinterpret_cast<Handle>(1);return 0;}
int __cdecl Destroy(Handle){++db.destroys;return 0;}
int __cdecl Load(Handle){return 0;}
int __cdecl Save(Handle){++db.saves;return db.failSave?-137:0;}
int __cdecl Find(Handle,const wchar_t* name,Handle* p){
    Check(name==ProfileName(path),"only own path-hashed profile requested");
    *p=target;return db.exists?0:kProfileNotFound;
}
int __cdecl CreateProfile(Handle,Profile* p,Handle* h){
    Check(p->name==ProfileName(path),"new profile has scoped name");db.exists=true;*h=target;return 0;
}
int __cdecl Info(Handle,Handle h,Profile* p){
    Check(h==target,"never global profile");wcscpy_s(p->name,ProfileName(path).c_str());
    p->apps=db.apps;p->predefined=db.predefined;return 0;
}
int __cdecl Enum(Handle,Handle,U32 first,U32* count,Application* p){
    Check(first==0&&*count==1,"check exact application association");
    wcscpy_s(p->name,db.mismatched?L"C:\\Other\\game.exe":NormalizePath(path).c_str());return 0;
}
int __cdecl CreateApp(Handle,Handle,Application* p){
    Check(p->name==path,"full executable path associated, not basename");return 0;
}
int __cdecl FindApp(Handle,const wchar_t* p,Handle* h,Application*){
    Check(p==path,"driver matching checked with full path");*h=target;return db.appFound?0:kExecutableNotFound;
}
int __cdecl Global(Handle,Handle* h){*h=globalTarget;return 0;}
int __cdecl Base(Handle,Handle* h){*h=baseTarget;return 0;}
int __cdecl Get(Handle,Handle h,U32 id,Setting* p){
    Check(id==kFix,"query only AA fix");
    if(db.failGet)return db.failGet;
    if(h==globalTarget){p->current.value=db.globalValue;p->location=1;return db.globalPresent?0:kSettingNotFound;}
    if(h==baseTarget){p->current.value=db.baseValue;p->location=2;return db.basePresent?0:kSettingNotFound;}
    Check(h==target,"scoped profile handle");
    p->current.value=db.value;p->location=db.location;p->currentPredefined=db.valuePredefined;
    return db.present?0:kSettingNotFound;
}
int __cdecl Set(Handle,Handle h,Setting* p){
    Check(h==target&&p->id==0x000858f7&&p->type==0&&p->current.value==0,"enable uses exact DWORD zero");
    ++db.sets;db.present=true;db.value=p->current.value;return 0;
}
int __cdecl Remove(Handle,Handle h,U32 id){Check(h==target&&id==kFix,"restore removes only fix");++db.removes;db.present=false;return 0;}
Api Mock(){return {Create,Destroy,Load,Save,Find,CreateProfile,Info,Enum,CreateApp,FindApp,Get,Set,Remove,Global,Base};}
}
int main(int argc,char** argv){
    if(argc==2 && std::string(argv[1])=="--inspect") {
        auto state=Inspect();std::printf("NVAPI read-only available=%d enabled=%d error=%ls\n",state.available,state.enabled,state.error.c_str());
        return state.available?0:77;
    }
    std::wstring error;
    Check(EnsureForAdapter(0x1002,error)&&error.empty(),"AMD skips NVAPI and succeeds");
    Check(!ShouldOfferInstall(),"AMD never offers installation");
    Check(!InstallForCurrentExecutable(nullptr,error),"AMD installation command cannot launch a helper");
    Check(!InstallForCurrentExecutable(nullptr,error,1) && error==L"The current renderer is not using an NVIDIA GPU.",
        "English UI gets an English install error without requesting elevation");
    Check(!InstallForCurrentExecutable(nullptr,error,0) && error==L"現在のレンダラーは NVIDIA GPU を使用していません。",
        "Japanese UI gets a Japanese install error without requesting elevation");
    Check(EnsureForAdapter(0x8086,error)&&error.empty(),"Intel skips NVAPI and succeeds");
    Api api=Mock();State state;bool on=true,off=false;
    Check(Execute(api,path,nullptr,state)&&!db.saves&&!db.exists,"read-only inspection never creates or saves");
    Check(Execute(api,path,&on,state)&&state.enabled&&db.saves==1&&db.sets==1,"default enables scoped profile and reads back");
    Check(Execute(api,path,&on,state)&&db.saves==1&&db.sets==1,"already enabled is idempotent");
    Check(Execute(api,path,&off,state)&&!state.enabled&&db.removes==1&&db.saves==2,"restore preserves profile and unrelated settings");
    db.apps=2;state={};Check(!Execute(api,path,&on,state)&&db.saves==2,"shared profile is never changed");
    db.apps=1;db.predefined=1;Check(!Execute(api,path,&on,state)&&db.saves==2,"predefined profile is never changed");
    db.predefined=0;db.mismatched=true;Check(!Execute(api,path,&on,state)&&db.saves==2,"wrong application is never changed");
    db.mismatched=false;db.failSave=true;Check(!Execute(api,path,&on,state)&&!state.error.empty(),"driver save failure propagates");
    db={};Check(!Execute(api,L"game.exe",&on,state)&&!db.saves&&!db.exists,"relative paths rejected before driver access");
    db={};db.appFound=false;db.basePresent=true;db.baseValue=0;
    Check(InspectEffective(api,path,state)&&state.enabled&&!db.saves&&!db.sets&&!db.exists,"original global patch detected without creating an app profile");
    db.globalPresent=true;db.globalValue=1;
    Check(InspectEffective(api,path,state)&&!state.enabled,"global override off wins over base on");
    db.appFound=true;db.present=true;db.value=0;db.location=2;db.valuePredefined=1;
    Check(InspectEffective(api,path,state)&&state.enabled,"inherited predefined enabled value is accepted");
    db.value=1;db.baseValue=0;db.globalValue=0;
    Check(InspectEffective(api,path,state)&&!state.enabled,"application off wins over installed global patch");
    db.failGet=-175;
    Check(!InspectEffective(api,path,state)&&state.driverStatus==-175&&!state.enabled,"read failure cannot become a success record");
    db={};db.appFound=false;
    Check(InspectEffective(api,path,state)&&!state.enabled&&!db.sets&&!db.saves,"missing patch remains visible without driver writes");
    Check(IsSiblingApplication(L"D:\\MMS only\\MMSNvidiaAAFix.exe",path),"helper accepts only its sibling host");
    Check(!IsSiblingApplication(L"C:\\Other\\MMSNvidiaAAFix.exe",path),"helper rejects a different directory");
    Check(!IsSiblingApplication(L"D:\\MMS only\\MMSNvidiaAAFix.exe",L"D:\\MMS only\\other.exe"),"helper rejects another executable name");
    Check(!IsSiblingApplication(L"D:\\MMS only\\MMSNvidiaAAFix.exe",L"D:\\MMS only\\..\\MikuMikuDance.exe"),"helper rejects uncanonical parent traversal");
    std::wstring canonical;
    const bool canonicalValid=CanonicalFile(ModulePath(),canonical);
    if(!canonicalValid)std::printf("canonical diagnostics: module=%ls result=%ls windows=%lu\n",ModulePath().c_str(),canonical.c_str(),GetLastError());
    Check(canonicalValid&&canonical==CanonicalModulePath(),"startup inspection and helper registration share the canonical executable identity");
    std::puts("PASS scoped NVIDIA AA profile, effective inheritance detection, helper identity, failures, and AMD/Intel fallback");return 0;
}
