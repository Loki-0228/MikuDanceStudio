# Bullet3 主分支集成验收

## 合并内容

集成提交 `b2b5e79` 合并 `main` 的 `f41a4d0` 与 `feat/mmv-bullet3` 的 `f3330a5`。共同基线为 `8dfa7103`，合并前两侧分别有 12 个和 3 个独有提交，可用 `git rev-list --left-right --count f41a4d0...f3330a5` 复核。

三处冲突分别保留两侧功能：

- `src/mmv/runtime.cpp` 的场景复位先取消网格烧录事务，再恢复原生物理模式并复位 Bullet3。
- `src/mmv_host/host.cpp` 保留没有插件时播放网格缓存的入口，同时清理每帧物理快照并记录原生物理状态。
- `docs/LOCAL_WORKSPACE_LAYOUT.md` 保留两侧已有归档记录。

计算 DLL、适配器、FX 和 UIFX 仍属于独立仓库 `D:/MMD-Next/MMV-Bullet3`，本轮构建使用该仓库的 `711a293`。上游 Bullet 源码仍只读引用 `D:/MMD-Next/refs/bullet3-master`。

## 构建与正确性

集成时使用 `.local/worktrees/mmv-bullet3` 工作树及 `integrate/mmv-bullet3-main` 分支；合入后已按用户要求移除该工作树。VS 2022 x64 Release 构建启用 `MMV_BULLET3_SOURCE_DIR=D:/MMD-Next/MMV-Bullet3`，宿主、兼容版 MMV 和纯 DX12 版 MMV 均构建通过。

| 验证 | 结果 |
| --- | --- |
| 适配器 | 606 项检查通过，包含慢帧播放与编辑模拟 |
| 生产运行时 | 直接 CPU 路径 72 项、真实 DX12 图路径 72 项通过 |
| 既有运行时 | 208 项通过，包含网格烧录事务、取消和场景复位；该组使用 GPU 替身 |
| FX | 599 项检查与 14 个随附示例编译通过 |
| Rou 隐藏宿主 | 96 个刚体、139 个关节；输出发生形变，暂停冻结、切回原生物理、移除效果恢复通过，原生刚体未被写入 |
| 慢帧播放 | 600 ms 时间增量，20 次播放调用后游标为 12 秒，最大顶点变化 1.031520 |
| 固定帧编辑 | 帧号保持 0，30 次编辑绘制后最大顶点变化 0.588609 |
| ABC 与网格缓存 | 原生路径导出、烧录、缓存导出、工程重开及清除后的坐标恢复通过 |

DSH 在同一集成工作树做运行时静态复核，没有发现合并引入的实质回归。审查没有执行测试；上述运行结果由主 agent 执行并核对。审查中的工具错误为技能入口不可调用及一次文件读取失败，正式结果正常完成；不将其 `completed` 状态替代运行验证。

## 同条件对照

原 Bullet3 分支 `f3330a5` 与集成版本使用同一 Rou 模型、FX、测试驱动、640×480 渲染目标及 60 帧固定时间步，按前后交替顺序运行两轮。关闭编辑时模拟以保持重复帧冻结，双方均开启 Bullet3。四次运行的最大顶点变化均为 1.015368，初始状态和原生物理恢复检查均通过。

四张第 60 帧 PNG 的 SHA256 相同：`DB6B8610BCB5E39FA9B542EDFBDB8C512B480D7609FC99CF6DDE616A69EEEC31`。图像来自实际渲染目标回读。

| 轮次 | 原分支 CPU 中位耗时 | 集成版 CPU 中位耗时 |
| --- | --- | --- |
| 1 | 136.673 ms | 139.760 ms |
| 2 | 137.915 ms | 147.106 ms |

这些值是隐藏窗口固定帧测试的 CPU 墙钟时间，包含同步等待，不能换算成已测播放 FPS。集成版两轮有波动，不据此宣称性能提升。原始样本全部保留。

## 证据与边界

证据已迁移至主目录 `.local/artifacts/mmv-bullet3-retired/mmv-bullet3-main/`：`comparison-manifest.json` 记录两侧 EXE/DLL、素材、测试驱动哈希和构建参数；`summary.json` 汇总对照；`ab/`、`slowplay/`、`editing/` 保留 CSV、PNG 和日志；`runtime-review.md` 与 `dsh-result.json` 保存独立复核。`feature-before/` 保留原分支的测试运行文件。

用户提供的 `MMEffect.dll+0x2655` 空指针截图与主目录 `.local/artifacts/mmv-curve/ui-probe/acc-open7/run.log` 完全匹配，线程号为 15904。该测试使用 `.local/releases/mmv-curve-before-2e684bf/host/MikuMikuDance.exe`，发生在打开附件参数窗口时，尚未接入 Bullet3。该崩溃已交由曲线编辑任务继续处理，不计为本轮 Bullet3 回归通过的范围。

本轮没有验证 Bullet3 视频导出、原生 VMD 物理烧录或所有第三方 MME 效果组合。Bullet3 仍只改变 MMV 输出网格，接口和使用边界见 [MMV Bullet3](MMV_BULLET3.md)。

## 同步最新 main

`7d783ca` 继续合并 `a017903`，保留曲线编辑、VEM 静态预设、三语菜单以及参数窗口修复。`main` 随后快进至该集成提交，`git merge-base --is-ancestor feat/mmv-bullet3 main` 返回成功。并行任务未提交的文档保持原样，没有暂存、回退或提交这些修改。

同步后再次构建宿主与两版 MMV，运行时回归增加至 240 项并全部通过。Bullet3 直接 CPU 与真实 DX12 图路径仍各通过 72 项。Rou 的 60 帧测试继续得到最大顶点变化 1.015368，物理切换恢复及暂停冻结通过。

使用 `a017903` 的 `mmv_ui_integration` 驱动，在独立目录加载正式目录原有的 `d3d9.dll`、`MMEffect.dll`、`MMHack.dll`，以及集成版 `MMV.dll` 和 `MMVBullet3.dll`。日志确认 `MME loaded=1`，真实宿主保持隐藏；引擎下拉框在连续 60 帧后仍保持展开，三语参数浮窗、MMV 与宿主曲线窗口检查全部通过，`UI integration failures=0`。该测试覆盖 Rou 的模型参数窗口，不等同于旧基线附件崩溃已修复。

上述复测已迁移到该归档证据目录的 `current-main-*.log`、`current-main-rou/` 和 `current-main-ui-results/`。正式目录构建日志及最终运行文件哈希记录于主目录 `.local/artifacts/mmv-bullet3-main/`。合并前补丁、未跟踪源码和 EXE/DLL 快照保留在 `.local/archives/mmv-bullet3-main/20260927-102250/`。
