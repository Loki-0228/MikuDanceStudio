# 透明排序主分支集成记录

本文保留六档测试版本的历史行为与证据；当前五档、默认值及存档规则见[五档排序记录](TRANSPARENT_SORT_FIVE_TIERS_2026-09-27.md)。

2026-09-27 将 `fix/cloud-precision`（`57c177c`）合入本地 `main`。合并前主分支为 `bcdfea9`，共同祖先为 `1e9c091`，双方分别有 7 和 28 个独有提交。合并保留主分支的骨骼缩放、刚体联动和物理烧录，新增云透明合成修复、六档排序上限及 PMM v2 单字段保存。未推送远端。

正式程序位于 `build/Release/MikuMikuDance.exe`，验收版本用 `accepted/cloud-sort-limit-20260927` 标签固定。默认上限仍为标准；关闭本身是档位，不保留先前精度。操作、算法边界与旧字段迁移见[透明排序上限](TRANSPARENT_SORT_LIMIT_PMM_2026-09-27.md)。

## 合并与复核

PMM 设置模块保留 `bone_scale_io::ResetPmm`、`LoadPmm`、`CapturePmm`，同时采用新的单字段排序读取、迁移与写入规则。实际保存测试同时验证缩放矩阵、缩放关键帧和排序档位。菜单语言刷新保留 `bake::RefreshMenu`，移除旧开关项并刷新六档上限。扩展保存的 16 MiB 上限保护仍在。

DSH 分别处理 PMM 设置模块、菜单刷新文件及功能导航文档，实际 cwd 均为 `D:/MMD-Next/MikuDanceStudio-MME`。任务 ID 为 `0753a0e7-3a7b-4478-a5ed-699c096f2bf6`、`0031024e-5bd0-4b3a-9442-60c362504265`、`2b0111de-9105-48dc-bf0e-c0c3647f3ee8`。主 agent 读取正式结果、复核 diff，并修订文档中的过期状态后完成验收。

合并后的 `model_renderers.cpp`、`transparent_peeling.cpp` 及其 shader 与候选分支一致。主分支独立的骨骼和物理实现保留。原 PMM、PMX、Toon 及 Cloud.fx 未改动。

## 构建与后台回归

合并前主分支和合并后程序均以 VS 2022、v143、x64 Release 构建，`MIKUDANCESTUDIO_DIAG=OFF`、`MIKUDANCESTUDIO_PERF=OFF`。正式程序、MME 基准及相关测试构建通过。所有运行测试均为隐藏窗口或无窗口进程，没有占用桌面。

16 次独立回归全部通过：扩展 codec、透明几何、骨骼缩放及读写、物理烧录设置、刚体缩放、仿射碰撞形状、物理烧录单元、真实 ToonCloud PMX 的项目设置往返、真实刚体工程的烧录往返，以及低 alpha、低 alpha + 4x MSAA、相交面 + 4x MSAA、预算回退、最高档和最高档 + 4x MSAA。

最高档两种采样模式均实际完成 1152／4608 层，日志为 `complete=1 committed=1`。对应 RGB 为 3／0，目标 A=255，符合解析式容差。实际刚体场景包含 140 根骨骼、45 个刚体和 27 个 joint，烧录并往返 253 个物理关键帧。

本轮没有重跑全量 CTest，也没有重新验收所有第三方 MME 效果。此前自阴影 alpha 的 8 个既存失败点保持原状态，不计作通过。96×96 深层压力样本的未完成记录仍保留，常规最高档回归使用既定 16×16 目标，真实云图的视口没有缩小。

## 候选版与合并版对照

本轮检查集成是否改变已验收的云图，以独立构建的候选版和合并版交替运行，顺序为候选、合并、合并、候选。两个生产核心各自链接同一份 `frame_benchmark.cpp`，使用相同 DLL、配置、素材和相机；均实际选择最高档。前轮修复前后对照继续保留在[六档上限验证](TRANSPARENT_SORT_LIMIT_PMM_2026-09-27.md)与[浮点合成验证](CLOUD_FLOAT_COMPOSITING_2026-09-26.md)。本次合并前旧 main 的源码和运行快照另行保存，不将其混入等画质的集成对照。

场景为原云 PMM 的未修改副本，MME 加载、Cloud.fx 关闭、ikParticle.fx 保留。视口 1076×606、8x MSAA，每轮预热 2 帧，测量 5 帧，图像从渲染目标回读。

| 轮次 | build | GPU 完成后的批次 ms/帧 |
| --- | --- | ---: |
| 1 | 候选 | 573.208 |
| 2 | 合并 | 571.172 |
| 3 | 合并 | 568.406 |
| 4 | 候选 | 568.405 |

两侧均值约为 570.807 和 569.789 ms/帧，差异小于轮次波动，不能声称提速。这是 GPU 完成后的批次吞吐耗时，不是桌面播放 FPS。

读回图像尺寸为 1463×914。全图有 10,275 个像素不同，全部位于右下角操作按钮区域（x=1233..1386，y=467..621），对应主分支新增的骨骼缩放按钮及上移的局部坐标标签；其余 RGBA 像素完全一致。云朵和天空未出现集成差异。同一 build 两轮图像也完全一致。

初次准备对照运行目录时发现 main 的基准输出缺少 MME DLL，在启动测试进程前停止。`comparison/` 保留该准备失败说明；有效对照位于 `comparison-02/`，使用与正式 Release 哈希一致的完整运行库。

## 版本与证据位置

- 合并前分支：`archive/main-before-cloud-sort-20260927`，固定 `bcdfea9`。
- 合并前运行快照：`.local/releases/main-before-cloud-sort-bcdfea9/`，正式 DLL 在其 `release/` 子目录。
- 合并后完整运行快照：`.local/releases/main-cloud-sort-20260927/`。
- 原始构建日志、DSH 结果、16 项回归、对照 JSON 与图片：`.local/artifacts/cloud-main-merge-20260927/`。
- `integration-manifest.json` 记录最终提交、源码状态、构建配置和二进制哈希。

原工作树和历史结果全部保留，已合入的候选工作树标为验收参考，不自动清理。分支关系可复核如下：

```powershell
	git rev-list --left-right --count bcdfea9...57c177c
	git merge-base bcdfea9 57c177c
	git merge-base --is-ancestor 57c177c main
```
