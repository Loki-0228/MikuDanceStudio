# 正式 D3D9/MME 蒙皮运算器

本次把已有 D3D12 计算蒙皮接入正式 D3D9/MME 渲染流程，在「显示 → 蒙皮运算器」中选择设备。没有启用功能不完整的 D3D12 预览窗口。

## 设备与回退

- 2026-09-26 起，默认项「自动（优先高性能）」按每个模型的实测耗时选择 CPU 或 GPU，见[蒙皮运算器自动选择](SKINNING_AUTOSELECT_2026-09-26.md)。
- 菜单列出支持 D3D12 FL 11_0 的硬件适配器及 CPU。同名适配器加序号区分。
- 按 D3D12 的 UMA 属性区分独显与集显，组内保留 DXGI 高性能顺序。软件、远程及不能确认架构的适配器不列入。
- 手动选卡先尝试指定设备，失败后继续尝试其余硬件，最后回退 CPU。不会每帧重复尝试故障设备；重新选择运算器可再次尝试。
- QDEF、未知权重类型及不满足现有 GPU 输入条件的模型继续走原有 CPU 分支。其他模型仍可用 GPU。
- 「最近运算」显示最后一次模型更新的设备或 CPU 回退原因，不表示多模型场景的所有模型都使用同一设备。

全局设置保存在 EXE 同级的 `Data/skinning.ini`，不改位置式 `mmconfig.ini`。启动读取设置，菜单选择即时生效并保存。缺失、损坏或无法匹配的全局设置使用自动模式。并行加入的 PMM v2 工程设置可覆盖全局偏好，按显卡类型恢复；具体格式及保留规则见 [PMM v2 扩展说明](PMM_V2_EXTENSIONS.md)。

系统重启后 GPU 的 LUID 可能改变，此时仅在名称唯一时恢复原设备；同名设备无法确认身份时回到自动模式。本机 Windows 返回一个 NVIDIA RTX 4070 Laptop GPU 适配器和四个不同 LUID 的 Intel UHD 适配器。这是逻辑适配器清单，不能解释成四块物理集显。

设备判定依据：[D3D12 架构属性](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_architecture)、[DXGI 高性能枚举](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_6/nf-dxgi1_6-idxgifactory6-enumadapterbygpupreference)、[设备 LUID 有效期](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-getadapterluid)。

## 正式渲染接入

`UpdateModelVertexBuffers` 按当前运算器选择调用 CPU 或 GPU。D3D12 在一次命令提交中计算蒙皮并复制到回读缓冲，等待完成后写入原有 D3D9 主体和描边顶点缓冲。

保留缓冲对象身份、32～96 字节主体步长、16 字节描边步长和 PMD 已有描边颜色。MME、阴影、透明排序、录制和附件继续使用原有 D3D9 渲染接口。只有计算和两个缓冲锁定都成功后才写入结果；失败由原 CPU 分支重新计算。

每次更新重新打包顶点及 UV morph 输入，避免缓存遗漏编辑或插件修改。骨骼求解、物理和 morph 准备仍在 CPU。该方案需要同步回读及重新上传，不是 GPU 常驻、零拷贝渲染，也不代表所有工作都移到了显卡。

## 验证记录

源码基线为 `8bb3a123dc01d2c48df0497bad4f6750338ea6cf`。为排除并行进行的透明排序、美影及 EMM 保存修改，在 `.local/worktrees/gpu-skinning-verify/` 单独应用 GPU 计算、正式缓冲桥接及菜单改动。隔离构建不含并行 PMM 类型保存的接入，该接入随后在 main 整合验证。构建为 VS 2022 x64 Release，`/O2 /Ob2 /DNDEBUG`，`MIKUDANCESTUDIO_PERF=OFF`、`MIKUDANCESTUDIO_DIAG=OFF`。

所有运行采用隐藏窗口，不采集桌面截图。图像来自程序的 D3D9 渲染目标。构建日志位于 `.local/build-gpu-skinning-*.log`；原始日志、JSON、图像、EXE 和哈希清单位于 `.local/artifacts/gpu-skinning/`。

| 验证 | 结果 |
| --- | --- |
| `skinning_backend_tests` | 60 组 CPU/GPU 比较通过；覆盖 PMD、BDEF1/2/4、SDEF、PMX 0～4 组附加 UV、两种描边透明状态、全部枚举设备、切回 CPU、QDEF 回退、保存设置与三语言菜单。 |
| `d3d12_skinning_tests`：洛希极限角色 | 183698 顶点、969 骨骼、三组姿态通过。NVIDIA 计算内核对 CPU 最大绝对误差为 0；各设备的正式桥接比较均在容差内，缓冲对象未更换。 |
| `d3d12_skinning_tests`：PMD 讲座箱子 | 基线和修改版均通过三组姿态比较。 |
| 正式 MME | 确认 `MMEffect.dll` 已加载；CPU/GPU 角色及 HGrass 完整渲染通过，GPU 样本没有 CPU 回退。 |

浮点容差为 `2e-5 * max(1, abs(reference))`，描边颜色必须完全相等。基准新增 `--processor auto|cpu|gpu`；`gpu` 模式在未发生 GPU 运算或发生 CPU 回退时返回失败。JSON 记录计算适配器、GPU 更新数和 CPU 回退数。

### 正式目录整合验证

main 同时包含并行的透明排序、EMM 和 PMM 设置改动。正式 Release 构建通过，60 组设备/布局比较、角色三组姿态逐卡比较及 MME CPU/GPU 渲染全部通过。PMM 扩展的 CRC 修正落盘后再次增量构建，并补跑 MME GPU 渲染，退出码为 0，渲染目标图像正常写出。此处只确认集成正确性，不用混合改动的样本替代下表的隔离性能对照。

交付程序为 `build/Release/MikuMikuDance.exe`，该次 SHA-256 为 `6CE061F2F7B1FADA58F5F001D437780917D27733D8F6AFD878547F9BE2C4AAA7`。原始构建日志为 `.local/build-gpu-skinning-integrated-4.log` 和 `.local/build-gpu-skinning-integrated-final.log`；源码、未提交状态、二进制哈希及运行命令均保存在 `integrated*-manifest.json`、`integrated-runs.json` 和 `integrated-final-mme-gpu-run.json`。后续其他任务重新构建时，日常 EXE 哈希可能改变。

### 性能限制

下表为包含 GPU 排空的批次毫秒/帧，不是窗口播放 FPS 或纯 GPU 时间。每次预热 30 帧、采样 120 帧，1280×720，自阴影模式 1。基线与 GPU 按 ABBA 顺序交替运行，随后补一次修改版 CPU。保留物理调用、描边、透明排序和阴影。

| 场景 | 基线 CPU 两次 | GPU 两次 | 修改版 CPU 一次 |
| --- | --- | --- | --- |
| 角色近景，183698 顶点、701 刚体、1098 关节 | 16.0582 / 14.8625 ms | 31.7777 / 34.4704 ms | 12.2424 ms |
| 两份 HGrass，spread 布局 | 30.7174 / 30.4320 ms | 139.5830 / 129.3390 ms | 30.3078 ms |

MME 角色样本另测 CPU 12.8150 ms、GPU 36.6539 ms，各一次。样本较短，不能用修改版 CPU 与基线的波动宣称额外加速。

**当前兼容回读方案在这些样本上慢于 CPU，性能收益未通过验收。** 该轮交付时默认 GPU 优先；2026-09-26 起改为实测自动选择，在这两组场景自动选用 CPU。本次交付的是可选择的 GPU 蒙皮能力，不能称为已经完成的蒙皮加速。

### 图像差异与覆盖边界

未宣称逐像素相同。1280×720 的前后图像比较中，角色有 884 个像素不同，最大通道差 18，平均通道差 0.000607；草地有 453 个像素不同，最大通道差 46，平均通道差 0.000660。MME CPU/GPU 有 415 个像素不同，平均通道差 0.000621。

同一基线重复运行也有差异：角色 523 个像素、最大通道差 24；草地 377 个像素、最大通道差 52。同一 GPU 构建重复运行的角色差异为 950 个像素。尚未定位重复渲染差异来源，不能据此宣称严格像素一致。

已测 GPU 计算、正式缓冲写入和 MME 默认渲染。未穷举第三方 MME 效果、录制、设备热拔插、TDR 或没有可用 GPU 的实机。保留接口不等于逐项实机验收。

## 本地产物与协作

- `before/`：未修改基线 EXE、Data、构建参数和 SHA-256。
- `after/`、`final/`：中间验证构建，对应原始正确性日志。
- `release/`、`mme-release/`：ABBA 及 MME 性能样本对应构建。
- `verified/`、`mme-verified/`：复核后加入输入指针和显式缓冲长度检查的最终隔离构建。
- `integrated/`、`mme-integrated/`：首次正式整合构建及回归产物；`integrated-final/`、`mme-integrated-final/` 保留 PMM CRC 修正后的交付构建。
- `release-manifest.json`、`correctness-runs.json`、`compare-runs.json`、`mme-runs.json`、`summary.json`：版本、命令、退出码和结果。
- `run-verification.py`：隐藏窗口串行运行、超时及日志保留脚本。异常样本原样保留。
- `.local/worktrees/gpu-skinning-verify/` 保留供复现，不自动删除或归档。

DSH 实际工作目录均为 `D:/MMD-Next/MikuDanceStudio-MME`。只读审查 `5585511f-b707-4e96-b3d0-8a88a2b68202` 完成。设备模块任务 `c4b2cef1-3374-4367-b7ca-ee48dd30b232` 写入目标文件后由主 agent 接管，其终态为 `aborted`，不把它当作验收成功。主 agent 修正 DXGI 旧接口参数、架构查询失败处理及重复 LUID 过滤，完成构建和运行验证。
