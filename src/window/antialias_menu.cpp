#include "mikudancestudio/antialias.hpp"
#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/build_profile.hpp"
#include <algorithm>

namespace mikudancestudio::antialias {
namespace {
MMDApp* owner=nullptr;
HMENU parent=nullptr, popup=nullptr;
HHOOK hook=nullptr;
int position=-1;
bool selected=false;
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
    const wchar_t* original=app->EnglishUI()==2?L"MMD 原始":app->EnglishUI()==1?L"MMD original":L"MMD オリジナル";
    const wchar_t* temporal=app->EnglishUI()==2?L"TAA（静态累积）":app->EnglishUI()==1?L"TAA (static accumulation)":L"TAA（静止画蓄積）";
    const wchar_t* labels[]={original,L"MSAA 2×",L"MSAA 4×",L"MSAA 8×",L"FXAA",temporal};
    for(unsigned i=0;i<unsigned(Algorithm::Count);++i) {
        AppendMenuW(popup,MF_STRING|(Supported(app,Algorithm(i))?MF_ENABLED:MF_GRAYED),kFirstCommand+i,labels[i]);
        MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_FTYPE;item.fType=MFT_RADIOCHECK;
        SetMenuItemInfoW(popup,kFirstCommand+i,FALSE,&item);
    }
    CheckMenuRadioItem(popup,kFirstCommand,kFirstCommand+unsigned(Algorithm::Count)-1,
        kFirstCommand+unsigned(app->antialiasing.algorithm),MF_BYCOMMAND);
}
void Detach() {
    if(hook){UnhookWindowsHookEx(hook);hook=nullptr;}
    if(IsMenu(parent) && position>=0) {
        MENUITEMINFOW item{sizeof(item)};item.fMask=MIIM_SUBMENU;item.hSubMenu=nullptr;
        SetMenuItemInfoW(parent,position,TRUE,&item);
    }
    if(IsMenu(popup))DestroyMenu(popup);
    popup=parent=nullptr;position=-1;selected=false;
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
        if(message.message==WM_LBUTTONUP) {
            RECT rect{};
            activate=GetMenuItemRect(owner->MainWindow(),parent,position,&rect) && PtInRect(&rect,message.pt);
            // Clicking the arrow opens the submenu; the label remains a toggle.
            if(activate && message.pt.x>=rect.right-GetSystemMetrics(SM_CXMENUCHECK)-8)activate=false;
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
    } else if(message==WM_EXITMENULOOP || message==WM_DESTROY) {
        Detach();if(message==WM_DESTROY)owner=nullptr;
    } else if(message==WM_COMMAND && HIWORD(wp)==0 && LOWORD(wp)>=kFirstCommand &&
              LOWORD(wp)<kFirstCommand+unsigned(Algorithm::Count)) {
        std::wstring error;
        if(!Select(app,Algorithm(LOWORD(wp)-kFirstCommand),error))
            MessageBoxW(app->MainWindow(),error.c_str(),L"Anti-aliasing",MB_OK|MB_ICONERROR);
        return true;
    }
    return false;
}
}
