# 蒙皮运算器自动选择

显示菜单的「运算器（蒙皮）」改名为「蒙皮运算器」，默认项由「自动（独显优先，其次集显，最后 CPU）」改为「自动（优先高性能）」。自动模式不再按设备类别排序，而是逐个模型实测 CPU 和全部可用 GPU 的完整蒙皮耗时，选择结果正确、耗时最低的设备。设备类别只影响菜单文字和 PMM 保存的类型意图，不决定自动模式的胜者。

## 自动项显示当前设备

自动菜单项追加冒号与最近一次模型运算的实际设备，例如「自动（优先高性能）：CPU」或「自动（优先高性能）：NVIDIA GeForce RTX 4070 Laptop GPU」。设备名称和下方「最近运算」来自同一运行状态；多模型场景显示最后更新的模型所用设备。兼容回退保留 CPU 原因说明，尚未运算时显示「等待模型运算」。打开菜单时更新文字，原有勾选、运算器选择及设置保存规则不变。

本次修改前后的 Release 构建均通过 60 组硬件回归。修改版另核对未运算、逐卡切换、CPU 兼容回退与中文自动项文字。仅调整菜单格式，不改蒙皮或测速算法，不据测试运行时长声称性能变化。构建日志、前后 EXE、哈希清单、命令与退出码保存在 `.local/artifacts/skinning-device-label/` 和 `.local/build-skinning-device-label-*.log`。

DSH 委派 `1e74a4db-a8de-40dc-9cf5-05d8bb0af6e7` 的实际 cwd 为 `D:/MMD-Next/MikuDanceStudio-MME`，因供应商返回 `429 quota exhausted` 失败且未写入文件。菜单代码、现有回归用例和整合均由主 agent 完成。

## 测速规则

首次处理某个模型时，CPU 和每个候选 GPU 各预热 3 次，随后交替执行 5 轮采样，每轮同时取一个 CPU 样本和一个 GPU 样本，先后顺序逐轮交换。候选的耗时是自身样本排序后的中位数；CPU 的中位数取自全部轮次的 CPU 样本，不是首轮那 5 个。

GPU 计时覆盖输入打包、上传、计算、同步回读，以及写入原有 D3D9 主体和描边顶点缓冲。设备初始化、着色器编译和正确性读回不计入排名。

这是蒙皮阶段的墙钟耗时，不是 GPU 时间、播放帧率或整个渲染器的耗时，后续绘制中的驱动工作不属于这次排名。首次校准会增加加载等待，结果只对校准时的模型、设备负载和供电状态成立。供电或负载变化后，重新选择「自动（优先高性能）」即可重新评估。

正确性判据是 GPU 预热结果与 CPU 结果相符：主体顶点缓冲的全部浮点分量和描边位置按 `2e-5 * max(1, abs(cpu))` 比较，描边颜色必须完全相等。初始化、运算、缓冲读写或比较失败的候选不参与排名，耗时非有限或非正的候选同样被排除。

CPU 胜出属于正常选择，次数记入 `cpu_updates`，不计入 `cpu_fallbacks`。校准次数记入 `automatic_calibrations`。运行日志按候选打印 `Automatic skinning candidate id=... valid=... median_ms=...`，并按模型打印 `Automatic skinning vertices=... cpu_ms=... selected=...`。

## 缓存与失效

校准结果按模型缓存，键是模型地址，附带一份签名。签名包含两个顶点缓冲对象、骨骼表、材质表、蒙皮源数组指针、顶点数、骨骼数、材质数、物理模式与附加 UV 组数。这些字段任一变化即重新校准，覆盖模型替换和权重布局改动两种情况。

删除模型时 `ModelDispose` 先调用 `ForgetModel`，释放该模型的校准记录和计算设备。更换运算器、新建或加载工程走 `SetPreference`，它清空全部缓存。缓存条目达到 256 个时整体清空，避免反复单独替换模型时旧指针长期堆积。

GPU 在运行中失败时，把该设备的测速结果标为无效，按剩余有效结果重新排名并重建设备，CPU 也参与这次排名。返回 `Unsupported` 的模型直接走 CPU，不再尝试其他 GPU。不满足 GPU 输入条件的模型保留原有 CPU 路径。

全局 `Data/skinning.ini` 和 PMM 继续保存「自动」意图，不把本机测得的 CPU 或 GPU 写成手动选择。手动 CPU、指定显卡和 PMM 指定显卡类型的行为不变，格式见 [PMM v2 项目扩展格式](PMM_V2_EXTENSIONS.md)。

## 中文文案

| 原文 | 新文案 | 位置 |
| --- | --- | --- |
| 运算器（蒙皮） | 蒙皮运算器 | 显示菜单 |
| 自动（独显优先，其次集显，最后 CPU） | 自动（优先高性能） | 显示菜单 |
| 节省CPU功耗 | 节省内存模式 | 显示菜单 |
| 省电 | 省性能 | 主界面复选框 |

前两项的英文同时改为 `Automatic (best performance)`、日文改为 `自動（性能優先）`。后两项只改中文名称，仍共用原有的空闲暂停刷新开关，没有增加内存压缩、内存释放或降低画质的行为。

## 验证

基线为 `09483c643c7b0fff757cc31fbb0809cfe9087332`。旧侧可执行文件由该提交加上本地化子任务已改动的三个菜单源码构建，清单如实记录这个状态，蒙皮与渲染算法未改。两侧都是同一 main 工作目录的 VS 2022 x64 Release 构建，参数 `/O2 /Ob2 /DNDEBUG`，`MIKUDANCESTUDIO_PERF=OFF`、`MIKUDANCESTUDIO_DIAG=OFF`。所有运行使用隐藏窗口，图像取自渲染目标，不采集桌面截图。

| 验证 | 命令与结果 |
| --- | --- |
| `skinning_backend_tests` | 退出码 0，`PASS 60 GPU/CPU cases, PMD/PMX UV strides, fallback, settings and menus`。 |
| `d3d12_skinning_tests` | 退出码 0，洛希极限角色 183698 顶点、969 骨骼、三组姿态的最大绝对误差为 0，五个适配器的桥接比较都保持缓冲对象身份。该目标不在 CTest 默认集，显式构建运行。 |
| `pmm_project_settings_tests` | 退出码 0，6 项 PASS。 |
| `ctest --test-dir build -C Release` | 注册 29 项，通过 28 项。`model_alpha_occlusion_regressions` 失败 17 像素，与[全量透明排序实验记录](TRANSPARENCY_GLOBAL_SORT_2026-09-25.md)所记基线 `dbf189f` 的既有失败一致，与本次改动无关。 |
| `frame_benchmark` MME 自动模式 | 退出码 0，`cpu_updates=41`、`automatic_calibrations=1`、`gpu_updates=0`、`cpu_fallbacks=0`，自动模式选中 CPU。 |

`skinning_backend_tests` 新增的判定：低耗时的后序候选可以胜过前序候选；CPU 可以胜出；失败或耗时非法的候选不能胜出；实机自动决策等于全部有效候选的最小值；同一模型第二次更新复用校准；替换顶点缓冲后重新校准；删除模型释放记录；中文菜单标题为「蒙皮运算器」和「自动（优先高性能）」。

## 性能对照

按旧版、新版、新版、旧版的顺序串行运行，每次预热 30 帧、采样 120 帧，1280×720，自阴影模式 1，保留物理、描边、透明排序和 MME 接口。下表是等待 GPU 排空后的批次毫秒/帧，数值越低越好，不换算成窗口播放 FPS。

| 场景 | 旧自动模式两次 | 新自动模式两次 | 新自动模式选择 |
| --- | --- | --- | --- |
| 角色，183698 顶点、701 刚体、1098 关节 | 31.9292 / 38.6524 ms | 12.3192 / 11.9932 ms | CPU |
| 两份 HGrass，共 942244 顶点，spread 布局 | 136.3660 / 117.6650 ms | 24.9445 / 23.9371 ms | 两个模型均为 CPU |

旧自动模式在两组场景里都选了 NVIDIA GeForce RTX 4070 Laptop GPU，两轮差距来自运算器的选择，不是蒙皮算法的改动。MME 角色另测手动 CPU 11.6626 ms、自动 11.3136 ms，各一次。这两个短样本不足以声称自动模式比同一个 CPU 算法更快。

图像未宣称逐像素相同。详情见[正式 GPU 蒙皮记录](GPU_SKINNING_2026-09-25.md)。同一构建重复运行的差异与新旧差异同量级，因此不能据此判断算法差异。

### 本轮复核

2026-09-26 00:38 在同一工作树重新构建并复跑：`skinning_backend_tests` 退出码 0，PMM 测试 6 项 PASS，`ctest -C Release` 仍是 28/29 且失败项相同，MME 自动模式三次运行退出码 0 且都选择 CPU。宿主当时同时运行沙箱虚拟机 `qemu-system-x86_64-headless`，角色场景三次样本为 37.317 / 37.045 / 36.988 毫秒/帧，约为上表 10.7235 毫秒/帧的三倍。两次采样条件不同，这些数值只作为负载记录，不用于性能结论。

## 复现产物

产物位于 `.local/artifacts/skinning-autoselect/`，不进入 Git：

- `before/`、`before-mme/` 保存旧自动模式的程序，`after/`、`after-mme/` 保存交替采样的版本，`final/`、`final-mme/` 保存补齐生命周期与故障回退后的版本。
- `before-manifest.json`、`after-manifest.json` 记录提交、工作树状态、构建参数和源码或二进制哈希。
- `runs.json`、`final-runs.json`、`summary.json` 及同名日志、JSON、PNG 保留原始命令、退出码、耗时和图像比较。
- `run.py`、`run-final.py` 为隐藏窗口执行脚本；构建日志位于 `.local/build-skinning-autoselect-*.log`。
- `rerun/`、`rerun-mme/` 保存本轮复核的构建副本与输出，`mme-auto.json`、`mme-auto-2.json`、`mme-auto-3.json` 对应三次自动模式运行。

菜单本地化由 DSH 子任务 `7c2c3c37-2428-4241-b424-81490d559f42`（标签「蒙皮 菜单文案」）完成，实际工作目录为 `D:/MMD-Next/MikuDanceStudio-MME`。主 agent 复核三个文件的差异，并负责选择算法、测试、整合与提交。
