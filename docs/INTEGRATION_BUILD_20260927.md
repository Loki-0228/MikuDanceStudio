# 本轮集成 build

本轮代码提交到正式 `main`，未推送。主程序生产源码对应 `35d1e4b`；其后的 `ef7343f` 只加强测试基准和更新文档。

## 运行文件与目录

- 新主程序：`.local/releases/mms-retier-20260927/MikuMikuDance.exe`。
- SHA-256：`25CC0F27D7A62E4E42EEF1B4B52DE074B991B6191DFEF2363EEA6160D657A4D3`。
- 使用现有 VS 2022 x64 Release 配置，以 MSBuild `OutDir` 指向上述目录。原 `build/Release/MikuMikuDance.exe` 被用户程序占用，未中止该程序，也未覆盖它。
- 同目录 DLL 来自原 `build/Release`，保留当前安装组合；它们不是本轮重新部署的 MMV 版本。各文件哈希见同目录 `build-manifest.json`。
- MMV 两种既有构建目标均已编译，分别位于 `build/mmv/dx12/Release/MMV.dll` 和 `build/mmv/compat/Release/MMV.dll`。
- 无 MME 回读验收目录 `.local/releases/mmv-no-mme-validation/` 使用测试宿主和夹具插件，不能当作发行程序使用。

## 已整合与验证

| 项目 | 结果与记录 |
| --- | --- |
| 透明合批、三档强度下调 | CPU 回归及最高档普通/4×MSAA 对照通过；最高精度与上限不变。见 `TRANSPARENT_SORT_STRENGTHS_20260927.md` |
| 显示菜单混合语言 | 旧版复现、新版两套菜单的三语检查通过。见 `MENU_LANGUAGE_FIX_20260927.md` |
| 普通骨骼点击 | 前后测试复现并验证修复。见 `BONE_CLICK_SELECTION_20260927.md` |
| VPD 缩放尾部 | 80 项格式检查与 160 项集成检查通过 |
| 操作柄四角停靠 | 绘制回读、命中和用户设置测试通过。见 `VIEWPORT_HANDLE_CORNERS_20260927.md` |
| 顶部骨骼缩放读数 | 18 次 GPU 回读、数值轴向与刷新传输检查通过。见 `BONE_SCALE_READOUT_20260927.md` |
| 运算器设备列表 | 菜单测试通过；额外检测到但 D3D9 无法使用的设备显示为不可选信息 |
| MMV 双击/拖入赋予 | 编辑器测试 4,887 项通过 |
| 无 MME 的 0 层输出 | 固定、自阴影及设备重建后的回读通过。三组恒等重复均 0 像素差；输出差分别为 5,104、364、4,959 像素。见 `MMV_WITHOUT_MME.md` |
| FX 同目录 DLL 打包 | Bullet3 仓库提交 `15415f99`，4/4 检查通过，目录为 `D:/MMD-Next/MMV-Bullet3/build/fx-package/MMV/Bullet3/` |

DSH 各项结果由主端复核；无 MME 的末轮诊断由主端接手完成，原 DSH 会话已中止继续读取，不代表工作被删除。

## 保留的限制

通用透明测试有 8 个既存阴影遮罩断言失败，前后版本相同，未删断言。新增 MME 登记门禁的缓存回放前后对照尚未单独完成；真实多层 FX 和可见菜单的人工操作不在本轮后台验收中。

CPU+DX9 MMV 的另一会话改动，以及原有 `mmv_bullet3_host_tests.cpp` 改动保留在工作区，未混入本轮提交。没有删除临时工作树、日志或旧运行文件。

构建日志、原始回读与测试记录分别保存在 `.local/artifacts/global-processor/`、`.local/artifacts/no-mmv-stutter-20260927/` 及各功能文档标明的位置。
