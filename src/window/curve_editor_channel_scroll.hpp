#pragma once

// The native tree owns its scroll range, keyboard navigation and wheel input.
// A sibling covers only its non-client scrollbar, so tree hit testing and
// accessibility retain the native item rectangles.
namespace curve_editor_channel_scroll {
constexpr int kId = 1110;
constexpr UINT_PTR kSubclass = 0x43534352;
struct State {
    HWND tree = nullptr;
    bool dragging = false, hover = false;
    int grab = 0, repeat = -1;
};
struct Metrics {
    RECT bounds{}, thumb{};
    SCROLLINFO info{sizeof(SCROLLINFO), SIF_ALL};
    int arrow = 0, travel = 0, maximum = 0;
};
inline void Layout(HWND tree);
inline Metrics Measure(HWND bar, const State& state) {
    Metrics m; GetClientRect(bar, &m.bounds);
    GetScrollInfo(state.tree, SB_VERT, &m.info);
    m.arrow = std::min(int(m.bounds.right), int(m.bounds.bottom / 2));
    const int length = std::max(0, int(m.bounds.bottom) - 2 * m.arrow);
    const int range = std::max(1, m.info.nMax - m.info.nMin + 1);
    m.maximum = std::max(m.info.nMin, m.info.nMax - int(m.info.nPage) + 1);
    const int size = std::min(length, std::max(m.arrow * 2, int(double(length) * m.info.nPage / range)));
    m.travel = std::max(0, length - size);
    const int top = m.arrow + (m.maximum > m.info.nMin
        ? MulDiv(m.travel, m.info.nPos - m.info.nMin, m.maximum - m.info.nMin) : 0);
    m.thumb = {0, top, m.bounds.right, top + size};
    return m;
}
inline void Paint(HWND bar, State& state, HDC dc) {
    using namespace curve_editor_theme;
    const auto m = Measure(bar, state);
    Fill(dc, m.bounds, Mix(paper, shell, .3));
    const bool enabled = m.maximum > m.info.nMin && IsWindowEnabled(state.tree);
    const int center = m.bounds.right / 2, arrowSize = std::max(2, center / 2);
    for (int direction : {-1, 1}) {
        const int y = direction < 0 ? m.arrow / 2 : m.bounds.bottom - m.arrow / 2;
        POINT points[] = {{center - arrowSize, y - direction * 2},
                          {center + arrowSize, y - direction * 2}, {center, y + direction * 2}};
        HBRUSH brush = CreateSolidBrush(enabled ? muted : Mix(paper, muted, .35));
        auto oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Polygon(dc, points, 3); SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
    }
    if (enabled && m.thumb.bottom > m.thumb.top) {
        const int width = std::max(4, int(m.bounds.right) * (state.hover || state.dragging ? 3 : 2) / 5);
        HBRUSH brush = CreateSolidBrush(state.dragging ? accent : (state.hover ? text : muted));
        auto oldBrush = SelectObject(dc, brush), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        RoundRect(dc, center - width / 2, m.thumb.top, center + (width + 1) / 2, m.thumb.bottom, width, width);
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(brush);
    }
}
inline void ScrollTo(State& state, int position) {
    // TVGN_FIRSTVISIBLE avoids WM_VSCROLL's 16-bit thumb-position limit.
    SCROLLINFO info{sizeof(SCROLLINFO), SIF_ALL}; GetScrollInfo(state.tree, SB_VERT, &info);
    position = std::clamp(position, info.nMin, std::max(info.nMin, info.nMax - int(info.nPage) + 1));
    HTREEITEM item = TreeView_GetFirstVisible(state.tree);
    int delta = position - info.nPos;
    while (item && delta != 0) {
        auto next = delta > 0 ? TreeView_GetNextVisible(state.tree, item) : TreeView_GetPrevVisible(state.tree, item);
        if (!next) break;
        item = next; delta += delta > 0 ? -1 : 1;
    }
    if (item) TreeView_SelectSetFirstVisible(state.tree, item);
}
inline void Step(HWND bar, State& state) {
    if (state.repeat < 0) return;
    SendMessageW(state.tree, WM_VSCROLL, state.repeat, 0);
    InvalidateRect(bar, nullptr, FALSE);
}
inline LRESULT CALLBACK TreeProc(HWND tree, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
    const LRESULT result = DefSubclassProc(tree, message, wp, lp);
    HWND bar = reinterpret_cast<HWND>(data);
    if (message == WM_NCDESTROY) RemoveWindowSubclass(tree, TreeProc, id);
    else if (message == WM_VSCROLL || message == WM_MOUSEWHEEL || message == WM_KEYDOWN ||
             message == WM_SIZE || message == WM_ENABLE || message == WM_PAINT ||
             (message >= TV_FIRST && message < TV_FIRST + 100))
        InvalidateRect(bar, nullptr, FALSE);
    if (message == WM_SIZE || message == TVM_EXPAND || message == TVM_INSERTITEMW ||
        message == TVM_DELETEITEM || message == TVM_SETITEMHEIGHT || message == TVM_SELECTITEM)
        Layout(tree);
    return result;
}
inline LRESULT CALLBACK BarProc(HWND bar, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
    auto& state = *reinterpret_cast<State*>(data);
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(bar, &ps); Paint(bar, state, dc); EndPaint(bar, &ps); return 0;
    }
    case WM_PRINTCLIENT: Paint(bar, state, reinterpret_cast<HDC>(wp)); return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        const auto m = Measure(bar, state); if (m.maximum <= m.info.nMin || !IsWindowEnabled(state.tree)) return 0;
        SetFocus(state.tree); SetCapture(bar);
        const int y = GET_Y_LPARAM(lp);
        state.dragging = y >= m.thumb.top && y < m.thumb.bottom;
        if (state.dragging) state.grab = y - m.thumb.top;
        else {
            state.repeat = y < m.arrow ? SB_LINEUP : y >= m.bounds.bottom - m.arrow ? SB_LINEDOWN :
                y < m.thumb.top ? SB_PAGEUP : SB_PAGEDOWN;
            Step(bar, state); SetTimer(bar, 1, 400, nullptr);
        }
        InvalidateRect(bar, nullptr, FALSE); return 0;
    }
    case WM_MOUSEMOVE: {
        if (!state.hover) {state.hover = true; TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, bar, 0}; TrackMouseEvent(&t);}
        if (state.dragging) {
            const auto m = Measure(bar, state);
            if (m.travel > 0) ScrollTo(state, m.info.nMin + MulDiv(std::clamp(GET_Y_LPARAM(lp) - state.grab - m.arrow, 0, m.travel), m.maximum - m.info.nMin, m.travel));
        }
        InvalidateRect(bar, nullptr, FALSE); return 0;
    }
    case WM_MOUSELEAVE: state.hover = false; InvalidateRect(bar, nullptr, FALSE); return 0;
    case WM_TIMER:
        if (wp == 1) {
            SetTimer(bar, 1, 60, nullptr);
            POINT point{}; GetCursorPos(&point); ScreenToClient(bar, &point); const auto m = Measure(bar, state);
            if (PtInRect(&m.bounds, point) &&
                ((state.repeat == SB_LINEUP && point.y < m.arrow) ||
                 (state.repeat == SB_LINEDOWN && point.y >= m.bounds.bottom - m.arrow) ||
                 (state.repeat == SB_PAGEUP && point.y >= m.arrow && point.y < m.thumb.top) ||
                 (state.repeat == SB_PAGEDOWN && point.y >= m.thumb.bottom && point.y < m.bounds.bottom - m.arrow))) Step(bar, state);
            return 0;
        } break;
    case WM_LBUTTONUP: if (GetCapture() == bar) ReleaseCapture(); return 0;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        state.dragging = false; state.repeat = -1; KillTimer(bar, 1);
        if (message == WM_CANCELMODE && GetCapture() == bar) ReleaseCapture();
        InvalidateRect(bar, nullptr, FALSE); return 0;
    case WM_MOUSEWHEEL: return SendMessageW(state.tree, message, wp, lp);
    case WM_NCDESTROY:
        KillTimer(bar, 1); RemoveWindowSubclass(state.tree, TreeProc, kSubclass);
        RemoveWindowSubclass(bar, BarProc, id); delete &state; break;
    }
    return DefSubclassProc(bar, message, wp, lp);
}
inline void Attach(HWND tree) {
    HWND bar = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, 10, 10, GetParent(tree), reinterpret_cast<HMENU>(static_cast<INT_PTR>(kId)), GetModuleHandleW(nullptr), nullptr);
    if (!bar) return;
    auto* state = new State; state->tree = tree;
    SetWindowSubclass(bar, BarProc, kSubclass, reinterpret_cast<DWORD_PTR>(state));
    SetWindowSubclass(tree, TreeProc, kSubclass, reinterpret_cast<DWORD_PTR>(bar));
}
inline void Layout(HWND tree) {
    HWND bar = GetDlgItem(GetParent(tree), kId); if (!bar) return;
    SCROLLBARINFO info{sizeof(info)};
    if (!(GetWindowLongPtrW(tree,GWL_STYLE)&WS_VSCROLL) || !GetScrollBarInfo(tree, OBJID_VSCROLL, &info)) {ShowWindow(bar, SW_HIDE); return;}
    RECT r = info.rcScrollBar; MapWindowPoints(nullptr, GetParent(tree), reinterpret_cast<POINT*>(&r), 2);
    SetWindowPos(bar, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(bar, nullptr, FALSE);
}
}
