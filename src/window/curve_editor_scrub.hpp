// Numeric-field interaction. Only changes the edit text; the
// existing Apply command remains the data/undo transaction boundary.
namespace curve_editor_scrub {
constexpr UINT TestBegin=WM_APP+510,TestSample=WM_APP+511,TestEnd=WM_APP+512;
struct State {
    bool pending=false,dragging=false,testing=false,finishing=false;
    int cursorHides=0,wraps=0;
    double value=0,low=-1e300,high=1e300;
    POINT origin{},last{};
    std::wstring original;
};
inline bool Weight(HWND h){int id=GetDlgCtrlID(h);return id==1053||id==1055;}
inline double Rate(HWND h,WPARAM modifiers){
    double rate=Weight(h)?0.001:0.05;
    if(modifiers&MK_SHIFT)rate*=10;else if(modifiers&MK_CONTROL)rate*=0.1;
    return rate;
}
inline void PaintGrip(HWND h,HDC dc){
    RECT r{};GetClientRect(h,&r);r.left=r.right-22;
    curve_editor_theme::Fill(dc,r,curve_editor_theme::paper);
    int x=(r.left+r.right)/2,y=(r.top+r.bottom)/2;
    HPEN pen=CreatePen(PS_SOLID,1,IsWindowEnabled(h)?curve_editor_theme::text:curve_editor_theme::muted);
    auto old=SelectObject(dc,pen);
    MoveToEx(dc,x,y-6,nullptr);LineTo(dc,x,y+6);
    MoveToEx(dc,x-3,y-3,nullptr);LineTo(dc,x,y-6);LineTo(dc,x+3,y-3);
    MoveToEx(dc,x-3,y+3,nullptr);LineTo(dc,x,y+6);LineTo(dc,x+3,y+3);
    SelectObject(dc,old);DeleteObject(pen);
}
inline bool Begin(HWND h,State& s,bool test){
    wchar_t text[128]{};GetWindowTextW(h,text,128);wchar_t* end=nullptr;
    double v=wcstod(text,&end);
    if(end==text||!std::isfinite(v)||!IsWindowEnabled(h))return false;
    s.original=text;s.value=v;s.pending=true;s.dragging=false;s.testing=test;s.wraps=0;
    if(Weight(h)){
        double bounds[2]={0.0001,0.9999};
        SendMessageW(GetParent(h),WM_APP+393,GetDlgCtrlID(h),reinterpret_cast<LPARAM>(bounds));
        s.low=bounds[0];s.high=bounds[1];
    }else{s.low=-1e300;s.high=1e300;}
    if(test){s.last=s.origin={100,100};}
    else {GetCursorPos(&s.origin);s.last=s.origin;SetFocus(h);SetCapture(h);}
    return true;
}
inline void Finish(HWND h,State& s,bool cancel){
    if(!s.pending)return;
    s.finishing=true;
    if(cancel)SetWindowTextW(h,s.original.c_str());
    if(!s.testing){
        if(s.dragging)SetCursorPos(s.origin.x,s.origin.y);
        while(s.cursorHides>0){ShowCursor(TRUE);--s.cursorHides;}
        if(GetCapture()==h)ReleaseCapture();
    }
    s.pending=false;s.dragging=false;s.testing=false;s.finishing=false;
    InvalidateRect(h,nullptr,FALSE);
}
inline void Sample(HWND h,State& s,POINT point,RECT bounds,WPARAM modifiers){
    if(!s.pending)return;
    if(!s.dragging){
        if(std::abs(point.y-s.origin.y)<3)return;
        s.dragging=true;
        if(!s.testing){do{++s.cursorHides;}while(ShowCursor(FALSE)>=0);}
    }
    const int dy=s.last.y-point.y;s.last=point;
    s.value=std::clamp(s.value+dy*Rate(h,modifiers),s.low,s.high);
    wchar_t text[128]{};swprintf_s(text,L"%.10g",s.value);SetWindowTextW(h,text);
    if(point.y<bounds.top+8||point.y>bounds.bottom-8){
        point.y=(bounds.top+bounds.bottom)/2;
        // Reset the previous sample before warping, so the warp adds no value.
        s.last=point;++s.wraps;
        if(!s.testing)SetCursorPos(point.x,point.y);
    }
}
inline LRESULT CALLBACK Proc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    State& s=*reinterpret_cast<State*>(data);
    if(msg==WM_NCDESTROY){Finish(h,s,true);RemoveWindowSubclass(h,Proc,id);delete &s;return DefSubclassProc(h,msg,wp,lp);}
    if(msg==TestBegin)return Begin(h,s,true);
    if(msg==TestSample){Sample(h,s,POINT{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)},RECT{0,0,200,200},wp);return s.wraps;}
    if(msg==TestEnd){int wraps=s.wraps;Finish(h,s,wp!=0);return wraps;}
    if(msg==WM_LBUTTONDOWN){
        RECT r{};GetClientRect(h,&r);
        if(GetFocus()!=h||GET_X_LPARAM(lp)>=r.right-22){if(Begin(h,s,false))return 0;}
    }
    if(msg==WM_LBUTTONDBLCLK){Finish(h,s,true);SetFocus(h);SendMessageW(h,EM_SETSEL,0,-1);return 0;}
    if(msg==WM_MOUSEMOVE&&s.pending){
        POINT point{};GetCursorPos(&point);
        MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&info);
        Sample(h,s,point,info.rcMonitor,wp);return 0;
    }
    if(msg==WM_LBUTTONUP&&s.pending){bool moved=s.dragging;Finish(h,s,false);if(!moved)SendMessageW(h,EM_SETSEL,0,-1);return 0;}
    if(msg==WM_KEYDOWN&&wp==VK_ESCAPE&&s.pending){Finish(h,s,true);return 0;}
    if((msg==WM_CANCELMODE||msg==WM_CAPTURECHANGED||msg==WM_KILLFOCUS)&&s.pending&&!s.finishing)Finish(h,s,true);
    if(msg==WM_SETCURSOR){POINT p{};GetCursorPos(&p);ScreenToClient(h,&p);RECT r{};GetClientRect(h,&r);if(p.x>=r.right-22){SetCursor(LoadCursorW(nullptr,reinterpret_cast<LPCWSTR>(IDC_SIZENS)));return TRUE;}}
    LRESULT result=DefSubclassProc(h,msg,wp,lp);
    if(msg==WM_PAINT){HDC dc=GetDC(h);PaintGrip(h,dc);ReleaseDC(h,dc);}
    if(msg==WM_PRINTCLIENT)PaintGrip(h,reinterpret_cast<HDC>(wp));
    return result;
}
inline void Apply(HWND root){
    for(int id:{1049,1051,1053,1055}){
        HWND h=GetDlgItem(root,id);SetWindowSubclass(h,Proc,0x53435242,reinterpret_cast<DWORD_PTR>(new State));
        SendMessageW(h,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(5,26));
    }
}
}
