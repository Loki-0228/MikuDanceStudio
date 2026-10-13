#include "mikudancestudio/antialias.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/build_profile.hpp"
#include <algorithm>

namespace mikudancestudio::antialias {
namespace {
MMDApp* owner=nullptr;
HMENU parent=nullptr, popup=nullptr, smaaPopup=nullptr;
HHOOK hook=nullptr;
int position=-1;
bool selected=false,smaaSelected=false;
constexpr int smaaPosition=6;
bool Locate(HMENU menu,HMENU& result,int& index) {
    for(int i=0;i<GetMenuItemCount(menu);++i) {
        MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_ID|MIIM_SUBMENU;
        if(!GetMenuItemInfoW(menu,i,TRUE,&item))continue;
        if(item.wID==277){result=menu;index=i;return true;}
        if(item.hSubMenu && Locate(item.hSubMenu,result,index))return true;
    }
    return false;
}
void Populate(MMDApp* app) {
    while(GetMenuItemCount(popup)>0)DeleteMenu(popup,0,MF_BYPOSITION);
    smaaPopup=nullptr;
    const wchar_t* original=app->EnglishUI()==2?L"MMD 原始":app->EnglishUI()==1?L"MMD original":L"MMD オリジナル";
    const wchar_t* temporal=app->EnglishUI()==2?L"TAA（运动矢量）":app->EnglishUI()==1?L"TAA (motion vectors)":L"TAA（モーションベクトル）";
    const wchar_t* labels[]={original,L"MSAA 2×",L"MSAA 4×",L"MSAA 8×",L"FXAA",temporal};
    for(unsigned i=0;i<unsigned(Algorithm::Smaa);++i) {
        AppendMenuW(popup,MF_STRING|(Supported(app,Algorithm(i))?MF_ENABLED:MF_GRAYED),kFirstCommand+i,labels[i]);
        MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_FTYPE;item.fType=MFT_RADIOCHECK;
        SetMenuItemInfoW(popup,kFirstCommand+i,FALSE,&item);
    }
    CheckMenuRadioItem(popup,kFirstCommand,kFirstCommand+unsigned(Algorithm::Count)-1,
        kFirstCommand+unsigned(app->antialiasing.algorithm),MF_BYCOMMAND);
    smaaPopup=CreatePopupMenu();
    const Algorithm modes[]={Algorithm::SmaaStandard,Algorithm::Smaa,Algorithm::SmaaT2xStandard,Algorithm::SmaaT2xEnhanced};
    const wchar_t* chinese[]={L"SMAA 1x（标准）",L"SMAA 1x（斜线与拐角增强）",L"SMAA T2x（标准）",L"SMAA T2x（斜线与拐角增强）"};
    const wchar_t* english[]={L"SMAA 1x (standard)",L"SMAA 1x (diagonals and corners)",L"SMAA T2x (standard)",L"SMAA T2x (diagonals and corners)"};
    const wchar_t* japanese[]={L"SMAA 1x（標準）",L"SMAA 1x（斜線・コーナー強化）",L"SMAA T2x（標準）",L"SMAA T2x（斜線・コーナー強化）"};
    const auto* smaaLabels=app->EnglishUI()==2?chinese:app->EnglishUI()==1?english:japanese;
    for(unsigned i=0;i<4;++i) {
        const auto command=kFirstCommand+unsigned(modes[i]);
        AppendMenuW(smaaPopup,MF_STRING|(Supported(app,modes[i])?MF_ENABLED:MF_GRAYED),command,smaaLabels[i]);
        MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_FTYPE|MIIM_STATE;item.fType=MFT_RADIOCHECK;
        item.fState=(Supported(app,modes[i])?MFS_ENABLED:MFS_GRAYED)|
            (app->antialiasing.lastSmaaAlgorithm==modes[i]?MFS_CHECKED:MFS_UNCHECKED);
        SetMenuItemInfoW(smaaPopup,command,FALSE,&item);
    }
    MENUITEMINFOW smaa{sizeof(smaa)};smaa.fMask=MIIM_ID|MIIM_SUBMENU|MIIM_STRING|MIIM_FTYPE|MIIM_STATE;
    smaa.wID=kSmaaCommand;smaa.hSubMenu=smaaPopup;smaa.dwTypeData=const_cast<wchar_t*>(L"SMAA");
    smaa.fType=MFT_RADIOCHECK;
    smaa.fState=((Supported(app,Algorithm::Smaa) || Supported(app,Algorithm::SmaaStandard))?MFS_ENABLED:MFS_GRAYED)|
        (app->antialiasing.algorithm>=Algorithm::Smaa?MFS_CHECKED:MFS_UNCHECKED);
    InsertMenuItemW(popup,smaaPosition,TRUE,&smaa);
}
void Detach() {
    if(hook){UnhookWindowsHookEx(hook);hook=nullptr;}
    if(IsMenu(parent) && position>=0) {
        MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_SUBMENU;item.hSubMenu=nullptr;
        SetMenuItemInfoW(parent,position,TRUE,&item);
    }
    if(IsMenu(popup))DestroyMenu(popup);
    popup=parent=smaaPopup=nullptr;position=-1;selected=smaaSelected=false;
}
}
void RefreshMenu(MMDApp* app) {
    if(build_profile::original || !app || !app->MainWindow())return;
    const HMENU menu=app->FullscreenMode()?app->state.savedMenu:GetMenu(app->MainWindow());
    HMENU found=nullptr;int index=-1;
    if(Locate(menu,found,index)) {
        MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_STATE;
        item.fState=app->antialiasing.enabled?MFS_CHECKED:MFS_UNCHECKED;
        SetMenuItemInfoW(found,index,TRUE,&item);
    }
    if(IsMenu(popup))Populate(app);
}
LRESULT CALLBACK MenuFilter(int code,WPARAM wp,LPARAM lp) {
    if(code==MSGF_MENU && owner && position>=0) {
        const auto& message=*reinterpret_cast<MSG*>(lp);
        bool activate=message.message==WM_KEYDOWN && message.wParam==VK_RETURN && selected;
        bool activateSmaa=message.message==WM_KEYDOWN && message.wParam==VK_RETURN && smaaSelected;
        if(message.message==WM_LBUTTONUP) {
            RECT rect{};
            activate=GetMenuItemRect(owner->MainWindow(),parent,position,&rect) && PtInRect(&rect,message.pt);
            // Clicking the arrow opens the submenu; the label remains a toggle.
            if(activate && message.pt.x>=rect.right-GetSystemMetrics(SM_CXMENUCHECK)-8)activate=false;
            activateSmaa=GetMenuItemRect(owner->MainWindow(),popup,smaaPosition,&rect) && PtInRect(&rect,message.pt);
            if(activateSmaa && message.pt.x>=rect.right-GetSystemMetrics(SM_CXMENUCHECK)-8)activateSmaa=false;
        }
        if(activateSmaa && !(GetMenuState(popup,kSmaaCommand,MF_BYCOMMAND)&(MF_DISABLED|MF_GRAYED))) {
            EndMenu();PostMessageW(owner->MainWindow(),WM_COMMAND,kSmaaCommand,0);return 1;
        }
        if(activate){EndMenu();PostMessageW(owner->MainWindow(),WM_COMMAND,277,0);return 1;}
    }
    return CallNextHookEx(hook,code,wp,lp);
}
bool HandleMessage(MMDApp* app,UINT message,WPARAM wp,LPARAM lp) {
    if(build_profile::original)return false;
    if(message==WM_ENTERMENULOOP) {
        Detach();owner=app;Initialize(app);RefreshMenu(app);
        if(Locate(GetMenu(app->MainWindow()),parent,position)) {
            hook=SetWindowsHookExW(WH_MSGFILTER,MenuFilter,nullptr,GetCurrentThreadId());
            if(hook) {
                popup=CreatePopupMenu();Populate(app);
                MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_SUBMENU;item.hSubMenu=popup;
                SetMenuItemInfoW(parent,position,TRUE,&item);
            }
        }
    } else if(message==WM_MENUSELECT) {
        selected=reinterpret_cast<HMENU>(lp)==parent &&
            ((HIWORD(wp)&MF_POPUP)?LOWORD(wp)==position:LOWORD(wp)==277);
        smaaSelected=reinterpret_cast<HMENU>(lp)==popup &&
            ((HIWORD(wp)&MF_POPUP)?LOWORD(wp)==smaaPosition:LOWORD(wp)==kSmaaCommand);
    } else if(message==WM_EXITMENULOOP || message==WM_DESTROY) {
        Detach();if(message==WM_DESTROY)owner=nullptr;
    } else if(message==WM_COMMAND && HIWORD(wp)==0 &&
              ((LOWORD(wp)>=kFirstCommand && LOWORD(wp)<kFirstCommand+unsigned(Algorithm::Count)) || LOWORD(wp)==kSmaaCommand)) {
        std::wstring error;
        auto algorithm=LOWORD(wp)==kSmaaCommand?app->antialiasing.lastSmaaAlgorithm:Algorithm(LOWORD(wp)-kFirstCommand);
        if(LOWORD(wp)==kSmaaCommand && !Supported(app,algorithm)) {
            if(algorithm==Algorithm::SmaaT2xStandard)algorithm=Algorithm::SmaaStandard;
            else if(algorithm==Algorithm::SmaaT2xEnhanced)algorithm=Algorithm::Smaa;
        }
        if(!Select(app,algorithm,error))
            MessageBoxW(app->MainWindow(),error.c_str(),DialogTitle(app),MB_OK|MB_ICONERROR);
        return true;
    }
    return false;
}
}
