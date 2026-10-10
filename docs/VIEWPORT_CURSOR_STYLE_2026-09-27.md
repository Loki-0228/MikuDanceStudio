# 视口光标图标统一

旋转环与移动箭头使用与 XYZ 按钮一致的实色、黑色细描边和浅色外缘。移动光标不再绘制白色或红色手形，原点改为方块。三角箭头内部没有高光、阴影或渐变。

旋转环使用 70% 不透明度（179/255），包括绿色普通状态与黄色高亮状态。旋转横轴保持蓝色、竖轴保持红色，使用浓色、不透明填充。移动向上仍为绿色，向右仍为红色；原有黄色与中心红色高亮保留。

## 实现

`src/render/viewport_cursor_style.cpp` 直接绘制图集右半的八个光标单元，不从旧图像推断轮廓或继承色块。8×8 超采样按旋转 90×90、移动 75×75 的显示尺寸计算覆盖范围，颜色按 alpha 加权，圆环边界按径向距离计算。单元位置、UV、屏幕原点与输入判断未改。

`src/window/ui_init.cpp` 在图集加载后调用一次重绘。`src/render/frame_scene.cpp` 在精灵绘制期间启用线性缩小、线性放大与无 mip 采样，然后恢复调用方的采样状态。源 PNG、轴网格和图集左半保持原样。纹理仍为 managed，设备 reset 保留内容。

## 验证

后台隐藏 D3D9 HAL 设备实际绘制默认与高亮状态，浅、深背景一起回读。最终预览为 `.local/artifacts/viewport-icon-style/after/flat70-final-preview.png`。该文件名用于区分之前包含旧图残留的预览。

已通过的检查：

- 图集左半逐像素差异为 0；行尾 padding 与无效尺寸输入保持原值。
- 圆环内部 alpha 为 179，红色旋转轴内部 alpha 为 255。
- 四个移动状态均无旧手形位置残留；上下、左右三角形内部全部为单一纯色。
- 即使调用方留有点采样，精灵使用线性采样，绘制后恢复原采样状态。
- 原有坐标轴与骨骼覆盖层回归在修改前通过，共 2 项。

正式程序的整包构建仍未通过：第一轮遇到同一工作区其他改动中的 `mdl::BoneKeyCapacity` 接口错误，日志在 `after/build.log`。相关依赖补齐后再次构建，遇到 `src/window/alembic_export_dialog.cpp:148` 附近的字符串语法错误，日志在 `after/build-final.log`。没有改写这些并行任务的代码，也没有把旧 EXE 当作新版交付。因此整包集成与真实 PMM/MME 场景验收标为未完成。

图标模块独立验证使用修改前生产静态库，并在修改后程序中编译本次 `viewport_cursor_style.cpp` 与 `frame_scene.cpp`，通过真实 `DrawFrameOverlayBatches` 绘制。两侧使用同一 `probe.cpp`、相同嵌入图集、MSVC x64 Release `/O2 /Ob2 /DNDEBUG /MD`、560×440 渲染目标、18 个四边形、30 次预热与 600 次采样。没有显示窗口或采集桌面。

| 交替顺序 | CPU 提交毫秒/次 | 等待 GPU 完成的毫秒/次 |
| --- | ---: | ---: |
| 修改前 A1 | 0.000820 | 0.075897 |
| 修改后 B1 | 0.000775 | 0.073988 |
| 修改后 B2 | 0.000770 | 0.061547 |
| 修改前 A2 | 0.000821 | 0.051304 |

数据来自 `before/abba1.log`、`after/abba2.log`、`after/abba3.log`、`before/abba4.log`。GPU 完成时间有明显波动，本次不据此宣称性能提升。这是覆盖层提交与 GPU 排空测量，不是播放 FPS、独立 GPU 时间或整场景性能。初始化日志的 `init_ms` 还包含 CPU 检查，不能当作生产初始化耗时。

## 本地产物与版本

产物根目录为 `.local/artifacts/viewport-icon-style/`，不入库，保留用于本次复现。

- `before/` 保留修改前生产 EXE、生产静态库、测试 EXE、CMake 缓存、Git 状态和构建日志。初始提交为 `541ab70f8997036a21befd5760d5911a35253577`，当时包含 `src/mmv/editor.cpp` 与 `tests/mmv_editor_tests.cpp` 的已有改动。
- `after/` 保存当前模块独立验证程序、图集二进制回读、PNG、整包失败日志与工作树状态。
- `probe.cpp`、`compile-before.cmd`、`compile-after.cmd` 与两侧 `compile.rsp` 用于复现；`compare.py` 与 `check-arrow-fills.py` 校验图集区域与纯色三角内部。
- `probe-hashes.json`、`before/executable-hashes.json` 与 `after/hashes.json` 记录验证程序、修改前主程序和源码哈希。最终源码与提交号见 `final-manifest.json`。
- DSH 子任务为 `6eece9c2-5356-4360-bfc4-6f070771e8b6`，实际 cwd 为 `D:/MMD-Next/MikuDanceStudio-MME`。正式结果保存在 `dsh-result.json`。主任务复核后将初稿的旧图分类算法替换为直接几何绘制，并完成后续抗锯齿与透明度调整。
