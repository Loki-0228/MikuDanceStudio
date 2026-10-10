# MMV 旧版 FX 全局变量兼容修复

PowerShader v3.2 二影対応版的 `Shader/Shader_Main.fx` 在 MMV 中分配失败，
诊断指向 `Common_Shader.fxsub:292`。该行在像素着色器中改写 `EdgeColor.rgb`，
D3DX 编译器返回 `X3025: global variables are implicitly constant`。

共用的 DX9 FX 编译入口现增加 `D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY`，
保留原有行主序矩阵选项。兼容版和 CPU＋DX9 版均包含此修复。
此选项对应 `/Gec`；微软的 [HLSL 变量说明](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-variable-syntax)
明确说明它允许改写默认视为常量的全局变量。

## 验收范围

使用本机原始 PowerShader 文件及其 include，未修改第三方 FX。
通过独立隐藏测试进程调用生产 `LoadEffect`、`Legacy::ValidateEffect` 和实际
DLL 的 `setBinding`。此次验证覆盖编译、分配及合成场景的像素结果；
未验收 PowerShader 整套场景与原 MME 的画面一致性，也不代表所有 MME FX 均已兼容。

| 检查 | 修复前 | 修复后 |
| --- | --- | --- |
| 原始 PowerShader，兼容版 DLL 分配 | `X3025`，`setBinding=2` | `setBinding=0`，`Ready` |
| 原始 PowerShader，CPU＋DX9 DLL 分配 | `X3025`，`setBinding=2` | `setBinding=0`，`Ready` |
| DX9 后端回归，含原始 PowerShader | 166 项，2 项失败 | 171 项，0 项失败 |
| 兼容版实际 DLL 回归 | 本轮未重复运行完整旧版插件套件 | 89 项通过 |
| CPU＋DX9 实际 DLL 回归 | 本轮未重复运行完整旧版插件套件 | 93 项通过 |

新增合成 FX 同时改写顶点和像素着色器的全局变量。红、绿材质交替绘制三轮，
回读颜色分别为 `(0.5, 0, 0, 1)` 和 `(0, 0.75, 0, 1)`，验证每次调用的计算
及宿主参数更新，防止全局变量改写影响后续绘制。原有编译失败后保留旧程序、
材质语义、矩阵、HDR、脚本、设备重置和处理器选择测试继续通过。
CPU＋DX9 DLL 的 PowerShader 分配探针确认 `d3d9=1, d3d12=0`。

## 构建来源与限制

基线 Git 提交为 `ee631bcff3241fff2d8a8808e1ba676c1dc1fe35`。
工作区原有未提交的 CPU＋DX9 等改动保持原样，未纳入本次提交。
两侧测试使用相同驱动与相同已编译依赖，生产兼容层唯一差异为编译选项。
测试按 Visual Studio 2022、x64、Release 构建。

原构建缓存引用 `D:/MMD-Next/MMV-Bullet3`，但该目录的源码和头文件缺失。
因此本轮完整依赖构建失败，不能声称通过干净全量构建。后续单独编译
`mmv_legacy` 与 `mmv_compat_tests`，保留已有其他库和插件目标文件，
用 MSBuild `LibLinkOnly` 重新链接两个 DLL。现有 Bullet3 功能未通过关闭
构建选项绕过。正式运行目录 `build/Release/MMV.dll` 更新为本次兼容版，
CPU＋DX9 版输出在 `build/mmv/dx9/Release/MMV.dll`。

## 同条件计时

相同驱动以 ABBA、ABBA 顺序运行。计时区间为已有的 64×64 双材质夹具，
连续 120 次绘制与回读，包含 CPU 工作和等待回读的时间；不含效果编译。
各样本的像素稳定性检查均通过。没有模型动作或物理计算。

| 顺序 | 版本 | 120 次绘制与回读耗时，毫秒 |
| --- | --- | --- |
| 0 | 前 | 40.96 |
| 1 | 后 | 40.72 |
| 2 | 后 | 37.88 |
| 3 | 前 | 91.96 |
| 4 | 前 | 37.25 |
| 5 | 后 | 39.09 |
| 6 | 后 | 40.60 |
| 7 | 前 | 43.07 |

前版均值 53.31 毫秒，范围 37.25～91.96；后版均值 39.57 毫秒，
范围 37.88～40.72。前版较慢样本原因未确定，保留全部样本，未排除。
这组小场景数据未见后版耗时增加，但波动不足以支持性能提升结论。
未测完整播放 FPS、独立 CPU 提交时间或 GPU 时间。

## 证据与复跑

全部原始日志在 `.local/artifacts/mmv-fx-globals-20260928/`。
`before/` 和 `after/` 保存测试 EXE、兼容层源码、像素回读与运行日志；
`abba.json` 保存原始计时。`manifest.json` 记录最终提交、EXE/DLL 哈希及构建方式；
原有工作树差异、构建配置、依赖和目标文件哈希另行保留。
初次依赖构建失败、未实际链接的 `Link` 调用，以及缺少 ABI 版本字段的初版
探针日志均保留。有效 DLL 分配证据使用 `*-assignment-fixed.log`。

DX9 后端默认回归不依赖 PowerShader。指定真实文件时可以设置：

```powershell
$env:MMV_COMPAT_EFFECT_PATH = 'D:\BaiduNetdiskDownload\MMD\0-MME\PowerShader v3.2_二影対応版\Shader\Shader_Main.fx'
$env:MMV_COMPAT_READBACK = '.local\artifacts\mmv-fx-globals-20260928\rerun-readback.txt'
& '.\build\tests\Release\mmv_compat_tests.exe' '.local\artifacts\mmv-fx-globals-20260928\fixtures'
```

DSH 宿主状态检查返回 `fetch failed`，本轮停止委派，由 Codex 本地完成修复与验收。
