# 帮助菜单白块与独立窗口置顶

## 白块原因

帮助菜单的「操作柄位置」使用 owner-draw 绘制田字格。原实现修改菜单共用 HDC 的文字颜色与背景模式，却没有在返回前恢复。默认选择右下角时，最后绘制的格子使用白色高亮文字，因此后续菜单项可能以白字绘制到白底上。

这与菜单项目未加载不同：项目仍然存在，但绘制状态被前一项影响。修复保留田字格的直接选择方式，通过 `SaveDC` / `RestoreDC` 在该项绘制结束时恢复进入时的 HDC 状态；保存失败时不修改 DC。

## 前后验证方式

主端探针调用实际 `viewport_corner_menu::HandleDrawItem`，使用内存 DC 和 DIB，不打开可见菜单、不读写用户设置。每轮传入文字颜色 `RGB(17,34,51)`、背景模式 `OPAQUE`，依次检查四种操作柄位置；绘制后再用同一 HDC 绘制相邻菜单文字，并回读其可见像素。

旧实现来自 `3837b7d`。四种位置均未恢复背景模式及文字颜色；默认右下角还使后续文字可见像素数变成 0。修复前测试返回 1，原始记录保留在 `.local/artifacts/help-menu-white-20260928/dc-before.log`。

同一探针在修复后四种位置全部通过，均恢复原文字颜色和 `OPAQUE` 背景模式，后续文字均有 488 个可见像素。前后编译使用同一 MSVC x64、C++17、`/O2 /MD` 条件。源码、原始构建日志、测试日志及可执行文件均保留在本次诊断目录。

正式回归 `menu_draw_state_tests` 进一步覆盖四种位置和四种菜单项状态，并交替使用透明、不透明文字背景。它同时验证字体、文字对齐、背景颜色和裁剪区域没有被污染，再回读后续文字。使用旧生产文件时 16 组全部失败；修复后 16 组全部通过，后续文字均有 264 个可见像素。两种探针选用的字体不同，因此像素数量不同，不作为相互比较的画质指标。

通过 CMake 构建并执行的三个回归为 `menu_draw_state_tests`、`menu_language_regressions`、`viewport_corner_layout_tests`，结果 3/3 通过，见 `ctest.log`。本轮验证使用内存绘制与后台测试，没有前台打开真实下拉菜单做人工验收。

## 独立窗口置顶的范围

命令 281 只对「显示 → 独立窗口」打开的视窗调用 `SetWindowPos(HWND_TOPMOST/HWND_NOTOPMOST)`，不会把主程序窗口置顶。使用嵌入主窗口的视窗时，切换这个选项没有可见变化。

这是原有功能，本轮菜单整理把它移到了帮助菜单。中文名称改为「独立窗口置顶」，英文改为 `keep separate window on top`，使名称与实际作用一致。置顶行为、独立窗口开关及原有用户设置保存方式均未改动。

两套菜单资源的三语名称、动态菜单插入和子菜单句柄稳定性检查通过；资源生成后的 48 个对话框和 2 个菜单验证通过。记录见本次诊断目录的 `label-test.log` 与 `resources-verify.log`。

## 编译版本

VS 2022 x64 Release 主程序已编译到 `.local/releases/mms-help-menu-20260928/MikuMikuDance.exe`。原 `build/Release/MikuMikuDance.exe` 正在运行，本轮没有中止或覆盖它。

主程序 SHA-256：`9CBC1DF2A595F4E92F80F5274D1D691CF32E44D1DF3E9F2EDC87C2E6AAC9939F`。构建日志为 `.local/artifacts/help-menu-white-20260928/main-build.log`。新目录的运行 DLL 保留原 `build/Release` 安装组合，不代表重新构建或替换了 MMV 后端；文件哈希与源码提交记录在新目录的 `build-manifest.json` 中。
