#define NOMINMAX
#include "mikudancestudio/animation_curve_host.hpp"
#include "mikudancestudio/accessory_layout.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <cmath>
#include <commctrl.h>
using namespace mikudancestudio;
static void Check(bool b,const char*s){if(!b){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
static void CheckScaleLabelRepaint(MMDApp& app,HWND parent){
    const HWND label=CreateWindowExW(0,L"STATIC",L"Scale",WS_CHILD,640,708,60,18,parent,
        reinterpret_cast<HMENU>(570),GetModuleHandleW(nullptr),nullptr);
    Check(label!=nullptr,"hidden scale caption");
    HDC dc=CreateCompatibleDC(nullptr);
    BITMAPINFO bitmap{};bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth=60;bitmap.bmiHeader.biHeight=-18;
    bitmap.bmiHeader.biPlanes=1;bitmap.bmiHeader.biBitCount=32;
    void* pixels=nullptr;HBITMAP image=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&pixels,nullptr,0);
    auto oldImage=SelectObject(dc,image);auto oldFont=SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));
    MMDApp* previousApp=g_Block;g_Block=&app;app.ViewportRect().bottom=700;
    const auto paint=[&](const wchar_t* text){HandleCtlColor(label,dc);TextOutW(dc,0,0,text,lstrlenW(text));GdiFlush();};
    std::memset(pixels,0xA0,60*18*4);paint(L"Scale");paint(L"\u7f29\u653e");
    std::vector<unsigned char> repeated(60*18*4);std::memcpy(repeated.data(),pixels,repeated.size());
    std::memset(pixels,0xA0,60*18*4);paint(L"\u7f29\u653e");
    Check(std::memcmp(repeated.data(),pixels,repeated.size())==0,"scale caption clears old glyphs on repaint");
    g_Block=previousApp;SelectObject(dc,oldFont);SelectObject(dc,oldImage);
    DeleteObject(image);DeleteDC(dc);DestroyWindow(label);
}
int main(){
    auto app=std::make_unique<MMDApp>();
    auto cam=std::make_unique<mdl::CameraKey[]>(mdl::kTimelineKeyCapacity);
    auto light=std::make_unique<mdl::LightKey[]>(mdl::kTimelineKeyCapacity);
    auto shadow=std::make_unique<mdl::SelfShadowKey[]>(mdl::kTimelineKeyCapacity);
    auto gravity=std::make_unique<mdl::GravityKey[]>(mdl::kTimelineKeyCapacity);
    auto acc=std::make_unique<mdl::AccessoryKey[]>(mdl::kTimelineKeyCapacity);
    mdl::AccessoryRecord object{};
    app->CameraKeys()=cam.get();app->LightKeys()=light.get();app->ShadowKeys()=shadow.get();app->GravityKeys()=gravity.get();
    app->AccessorySlot(0)=&object;app->AccessoryKeys(0)=acc.get();app->EnglishUI()=2;
    cam[0].fov=45;cam[0].parentModel=-1;acc[0].parentModel=-1;acc[0].scale=1;acc[0].opacity=1;
    std::vector<curve_editor::Channel> channels;std::wstring error;
    Check(curve_host::Read(*app,curve_host::Kind::Accessory,0,channels,error),"read accessory");
    auto key=channels[0].track.keys[0];key.frame=20;key.value=10;key.interpolation=animation_curve::Interpolation::Linear;
    channels[0].track.keys.push_back(key);
    Check(curve_host::Write(*app,curve_host::Kind::Accessory,0,channels,error),"add native accessory key");
    Check(acc[0].next==1&&acc[1].frame==20&&acc[1].position[0]==10,"native timeline receives new frame");
    curve_host::Apply(app.get(),curve_host::Kind::Accessory,10,0);Check(object.position[0]==5,"accessory playback interpolates");
    curve_host::Apply(app.get(),curve_host::Kind::Accessory,10,0);Check(object.position[0]==5,"no accumulated deformation");
    pmm_extension::Document doc;curve_host::Capture(*app,doc);curve_host::Reset(app.get());curve_host::Load(*app,doc);
    curve_host::Apply(app.get(),curve_host::Kind::Accessory,10,0);Check(object.position[0]==5,"PMM curve reload");
    // A native edit must not be lost when the floating editor refreshes.
    acc[1].position[0]=14;
    Check(curve_host::Read(*app,curve_host::Kind::Accessory,0,channels,error),"refresh external edit");
    Check(channels[0].track.keys.back().value==14,"native changes reconcile");
    channels[0].track.keys.pop_back();
    Check(curve_host::Write(*app,curve_host::Kind::Accessory,0,channels,error),"delete key");
    Check(acc[0].next==0,"last nonzero native frame deleted");
    channels[0].track.keys.clear();Check(!curve_host::Write(*app,curve_host::Kind::Accessory,0,channels,error),"reject removing sentinel transactionally");
    Check(acc[0].next==0,"failed edit leaves native data intact");
    for(auto k:{curve_host::Kind::Camera,curve_host::Kind::Light,curve_host::Kind::Shadow,curve_host::Kind::Gravity}){
        Check(curve_host::Read(*app,k,0,channels,error),"read global track");
        size_t c=k==curve_host::Kind::Shadow?1:0;
        key=channels[c].track.keys[0];key.frame=30;key.value=k==curve_host::Kind::Shadow?0.05:10;
        key.interpolation=animation_curve::Interpolation::Linear;channels[c].track.keys.push_back(key);
        Check(curve_host::Write(*app,k,0,channels,error),"global key insertion");curve_host::Apply(app.get(),k,15);
    }
    Check(app->CameraPositionAxis(0)==5,"camera curve evaluation");
    Check(app->LightDirection()[0]==5,"light curve evaluation");
    Check(std::fabs(app->ShadowDistance()-0.025)<1.e-6,"shadow curve evaluation");
    Check(app->GravityMagnitude()==5,"gravity curve evaluation");
    // Authored curves are PMM extension data, independent of whether the
    // legacy camera interpolation can currently be decoded by the editor.
    Check(curve_host::Read(*app,curve_host::Kind::Camera,0,channels,error),"read authored camera");
    channels[0].track.keys[0].outSlope=0.75;
    channels[0].track.keys[0].outWeight=0.3;
    channels[0].track.keys.back().inSlope=-0.25;
    channels[0].track.keys.back().interpolation=animation_curve::Interpolation::Bezier;
    Check(curve_host::Write(*app,curve_host::Kind::Camera,0,channels,error),"write custom camera curve");
    const auto authored=channels[0].track;
    const auto originalControl=cam[1].interpolation[0][0];
    cam[1].interpolation[0][0]=255;
    try{curve_host::Capture(*app,doc);}
    catch(const std::exception&){Check(false,"capture authored data despite unavailable native interpolation");}
    curve_host::Reset(app.get());curve_host::Load(*app,doc);
    Check(curve_host::Read(*app,curve_host::Kind::Camera,0,channels,error),"reload custom data despite unavailable native interpolation");
    Check(channels[0].track.keys[0].outSlope==authored.keys[0].outSlope&&
          channels[0].track.keys[0].outWeight==authored.keys[0].outWeight&&
          channels[0].track.keys.back().inSlope==authored.keys.back().inSlope,
          "custom tangent data survives PMM roundtrip");
    cam[1].interpolation[0][0]=originalControl;
    Check(curve_host::Read(*app,curve_host::Kind::Camera,0,channels,error),"native reconciliation resumes");
    // Hidden real parent/control integration: the launcher must be parented
    // during WM_CREATE even before MMDApp::Hwnd is assigned by window_init.
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
    HWND parent=CreateWindowExW(0,L"STATIC",L"curve host test",WS_OVERLAPPEDWINDOW,0,0,1100,760,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(parent!=nullptr,"hidden host window");
    HMENU menu=CreateMenu(),file=CreatePopupMenu();AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(file),L"File");SetMenu(parent,menu);
    for(int id=471;id<=487;++id){
        const bool combo=id==471||id==474||id==475;
        auto control=CreateWindowExW(0,combo?L"COMBOBOX":id>=478&&id<=485?L"EDIT":L"BUTTON",L"",WS_CHILD|(combo?CBS_DROPDOWNLIST:0),868,620,100,22,parent,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),0);
    }
    curve_host::CreateControls(app.get(),parent);
    Check(GetParent(GetDlgItem(parent,curve_editor::kAccessoryCurve))==parent,"launcher created before app HWND assignment");
    app->Hwnd()=parent;app->state.optflag[0]=1;app->state.optflag[3]=1;
    CheckScaleLabelRepaint(*app,parent);
    curve_host::LayoutControls(app.get());
    for(unsigned lang=0;lang<3;++lang){app->EnglishUI()=uint8_t(lang);curve_host::Localize(app.get());
        wchar_t text[64]{};GetWindowTextW(GetDlgItem(parent,472),text,64);Check(wcslen(text)>0,"localized load label");
        Check(GetMenuState(file,curve_editor::kAccessoryMotionMenu,MF_BYCOMMAND)!=UINT(-1),"explicit accessory VMD menu exists");
    }
    curve_host::Open(app.get(),false);
    Check(GetForegroundWindow()!=parent,"hidden integration does not activate host");
    curve_editor::CloseOwned(parent);
    DestroyWindow(parent);app->Hwnd()=nullptr;
    curve_host::ForgetAccessory(app.get(),0);Check(!curve_host::Has(app.get(),curve_host::Kind::Accessory,0),"deleted object clears curves");
    curve_host::Reset(app.get());
    std::puts("animation_curve_host_tests: all checks passed");
}
