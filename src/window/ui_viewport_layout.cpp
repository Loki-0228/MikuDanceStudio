#include "mikudancestudio/build_profile.hpp"
// ===========================================================================
// VA 0x0040CAC0 - LayoutViewportPanels  (original: sub_40CAC0, 0x5A9 bytes,
// was Sub40CAC0)
// ===========================================================================
// Repositions the camera/light/accessory overlay strip at the top of the 3D
// viewport (ids 536..550) and the window-top button row (ids 551..557).
// Called from HandleWindowSize (0x443300 tail), the command dispatch
// (0x47E8A0 case 212), sub_442EB0 and MicWndProc (0x466A10).
//
// Base rect: the separate-viewport window when this+0xA0D38 is a live HWND,
// otherwise the main window with a left inset of sidebar(this+0xA06C8)+9.
// The camera strip anchors left (x = inset + K, shown while inset+K <=
// right), the button row anchors right (x = right - K, shown while
// right-K >= inset); controls that do not fit are parked off-screen at
// (-9000,-9000) with size 1x1, exactly like the original.
// Y bases come from the viewport hide-rect fields: strip rows sit at
// this+0xA0D4C + 3..11, buttons at this+0xA0D44 - 22 (or -19 for id 554).
// Tail: ids 536..557 get the IsWindowVisible -> SW_HIDE -> SW_SHOW
// re-toggle so repainted overlays pick up their new positions.
// =========================================================================//
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>

#include "mikudancestudio/mmd_app.hpp"
#include "mikudancestudio/ported_funcs.hpp"
#include "mikudancestudio/ui_language.hpp"

namespace mikudancestudio {

namespace {

HFONT ScaleLabelBoldFont(MMDApp* app) {
    // Match the 14px toolbar captions drawn through DrawUiGlyph. Only the
    // weight changes; using the system's smaller 12px control font made this
    // otherwise identical caption look undersized.
    static HFONT cached[3]{};
    const int language = app->EnglishUI();
    const int slot = language >= kUiJapanese && language <= kUiChinese
        ? language : kUiJapanese;
    if (cached[slot] != nullptr)
        return cached[slot];

    LOGFONTW font{};
    font.lfHeight = -14;
    font.lfWeight = FW_BOLD;
    if (slot == kUiChinese) {
        font.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(font.lfFaceName, L"Microsoft YaHei UI");
    } else if (slot == kUiEnglish) {
        font.lfCharSet = SHIFTJIS_CHARSET;
        wcscpy_s(font.lfFaceName, L"Arial");
    } else {
        font.lfCharSet = SHIFTJIS_CHARSET;
        wcscpy_s(font.lfFaceName, L"MS PGothic");
    }
    cached[slot] = CreateFontIndirectW(&font);
    return cached[slot];
}

int ScaleLabelTextWidth(HWND label, MMDApp* app) {
    if (label == nullptr)
        return 36;
    wchar_t text[64]{};
    if (GetWindowTextW(label, text, 64) <= 0)
        wcscpy_s(text, L"Scale");
    HDC dc = GetDC(label);
    if (dc == nullptr)
        return 36;
    HFONT font = ScaleLabelBoldFont(app);
    HGDIOBJ previous = font != nullptr ? SelectObject(dc, font) : nullptr;
    SIZE extent{};
    GetTextExtentPoint32W(dc, text, lstrlenW(text), &extent);
    if (previous != nullptr)
        SelectObject(dc, previous);
    ReleaseDC(label, dc);
    // Static text has no edit margins; keep only a 3px visual gap before X.
    return std::max(20, static_cast<int>(extent.cx) + 3);
}

}  // namespace

void LayoutViewportPanels(MMDApp* app) {  // was Sub40CAC0, VA 0x0040CAC0
    if (app->FullscreenMode() != 0) return;
    auto& s = *app;

    HWND separate = s.FloatingWindow();                           // 658744
    HWND base;
    int inset;
    RECT rc;
    if (separate != nullptr) {                                    // 0x40CADB
        base = separate;
        GetClientRect(base, &rc);
        inset = 0;
    } else {
        base = s.MainWindow();                                    // 657080
        GetClientRect(base, &rc);
        inset = s.SidebarWidth() + 9;                             // 657096
    }
    const int right = rc.right;
    const RECT& viewport = s.ViewportRect();
    const int stripY = viewport.bottom;                           // 658764
    const int rowY = viewport.top;                                // 658756

    // Camera strip: x = inset + K, shown while inset + K <= right.
    auto moveLeft = [&](int id, int k, int dy, int w, int h) {
        if (inset + k <= right)
            MoveWindow(GetDlgItem(base, id), inset + k, stripY + dy, w, h, TRUE);
        else
            MoveWindow(GetDlgItem(base, id), -9000, -9000, 1, 1, TRUE);
    };
    // Top button row: x = right - K, shown while right - K >= inset.
    auto moveRight = [&](int id, int k, int dy, int w, int h) {
        if (right - k >= inset)
            MoveWindow(GetDlgItem(base, id), right - k, rowY + dy, w, h, TRUE);
        else
            MoveWindow(GetDlgItem(base, id), -9000, -9000, 1, 1, TRUE);
    };

    moveLeft(536,   5,  3, 55, 27);   // 0x40CB25 model-select strip label
    moveLeft(537, 140,  9, 16, 17);   // X
    moveLeft(544, 157, 11, 53, 14);   // X value
    moveLeft(538, 213,  9, 16, 17);   // Y
    moveLeft(545, 230, 11, 53, 14);   // Y value
    moveLeft(539, 286,  9, 16, 17);   // Z
    moveLeft(546, 303, 11, 53, 14);   // Z value
    const bool modelScaleVisible = app->state.optflag[0] == 0;
    if (modelScaleVisible) {
        // Bone rotation values use the same 2-decimal text width as position
        // and scale. Reflow the Y/Z columns to keep 3px between each group.
        moveLeft(540, 400,  9, 16, 17);   // X
        moveLeft(547, 417, 11, 53, 14);
        moveLeft(541, 473,  9, 16, 17);   // Y
        moveLeft(548, 490, 11, 53, 14);
        moveLeft(542, 546,  9, 16, 17);   // Z
        moveLeft(549, 563, 11, 53, 14);
    } else {
        // Camera angle values use the same width as camera position. Move Y/Z
        // along the position-column pitch rather than squeezing the fields.
        moveLeft(540, 400,  9, 16, 17);   // X
        moveLeft(547, 417, 11, 53, 14);
        moveLeft(541, 473,  9, 16, 17);   // Y
        moveLeft(548, 490, 11, 53, 14);
        moveLeft(542, 546,  9, 16, 17);   // Z
        moveLeft(549, 563, 11, 53, 14);
    }
    if (modelScaleVisible) {
        moveLeft(543, -9000, 0, 1, 1);
        moveLeft(550, -9000, 0, 1, 1);
    } else {
        moveLeft(543, 619,  8, 36, 21);   // distance follows the widened angles
        moveLeft(550, 656, 11, 53, 14);
    }
#if !MMD_ORIGINAL_PROFILE
    // In model mode, position, rotation and scale values use identical 53x14
    // edit boxes. The scale caption alone is sized from its localized text.
    // Install the final font before MoveWindow repaints the caption. Drawing
    // once with the control font and again with the bold font caused ghosts.
    for (int id : {570, 571, 572, 574, 575, 576, 577}) {
        HWND control = GetDlgItem(base, id);
        if (control != nullptr && app->state.hFontUI != nullptr) {
            ApplyUiFontToControl(app, control, id);
            if (id == 570)
                SendMessageW(control, WM_SETFONT,
                             reinterpret_cast<WPARAM>(ScaleLabelBoldFont(app)), FALSE);
        }
    }
    const int scaleTitleWidth = ScaleLabelTextWidth(GetDlgItem(base, 570), app);
    const int scaleTitleX = 640;
    const int scaleEditWidth = 53;
    const int scaleXAxis = scaleTitleX + scaleTitleWidth;
    const int scaleXField = scaleXAxis + 17;
    const int scaleYAxis = scaleXField + scaleEditWidth + 3;
    const int scaleYField = scaleYAxis + 17;
    const int scaleZAxis = scaleYField + scaleEditWidth + 3;
    const int scaleZField = scaleZAxis + 17;
    const bool scaleFits = modelScaleVisible &&
        inset + scaleZField + scaleEditWidth <= right;
    auto moveScale = [&](int id, int x, int dy, int w, int h) {
        if (scaleFits)
            MoveWindow(GetDlgItem(base, id), inset + x, stripY + dy, w, h, TRUE);
        else
            MoveWindow(GetDlgItem(base, id), -9000, -9000, 1, 1, TRUE);
    };
    moveScale(570, scaleTitleX, 8, scaleTitleWidth, 18);  // 14px toolbar font
    moveScale(571, scaleXAxis, 9, 16, 17);  // X
    moveScale(572, scaleXField, 11, scaleEditWidth, 14);
    moveScale(574, scaleYAxis,  9, 16, 17);  // 3px gap after the prior field
    moveScale(575, scaleYField, 11, scaleEditWidth, 14);
    moveScale(576, scaleZAxis,  9, 16, 17);
    moveScale(577, scaleZField, 11, scaleEditWidth, 14);
#endif

    moveRight(557, 140, -22, 50, 20); // 0x40D018 coordinate-axis toggle
    moveRight(552, 195, -22, 50, 20); // power-save
    moveRight(551, 250, -22, 50, 20); // info display
    moveRight(556, 305, -22, 50, 20);
    moveRight(553, 360, -22, 40, 20);
    moveRight(554, 398, -19, 35, 14); // frame edit box
    moveRight(555, 441, -22, 40, 20);

    for (int i = 536; i <= 557; ++i) {                            // 0x40D053
        HWND overlay = GetDlgItem(base, i);
        if (IsWindowVisible(overlay)) {
            ShowWindow(overlay, SW_HIDE);
            ShowWindow(overlay, SW_SHOW);
        }
    }
#if !MMD_ORIGINAL_PROFILE
    for (int id : {570, 571, 572, 574, 575, 576, 577}) {
        HWND control = GetDlgItem(base, id);
        if (IsWindowVisible(control)) {
            ShowWindow(control, SW_HIDE);
            ShowWindow(control, SW_SHOW);
        }
    }
#endif
}

}  // namespace mikudancestudio
