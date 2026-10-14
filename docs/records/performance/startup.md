# 启动与资源初始化记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [MMS 启动耗时与运算器配置缓存](#record-startup-processor-config)
- [空场景显示后编译默认着色器](#record-startup-deferred-effect)
- [时间轴音频按需初始化](#record-startup-lazy-audio)
- [PMM 加载与启动测速](#record-pmm-startup-calibration)

<a id="record-startup-processor-config"></a>
## MMS 启动耗时与运算器配置缓存

原始记录：`STARTUP_PROCESSOR_CONFIG_20261010.md`。其中的版本、日期和验证范围沿用原记录。

本次空场景后台测量确认，原有测速结果已经写入用户目录，正常环境下可以复用。当时的构建在启动时创建内置 D3D9 特效、图形设备和音频设备。移动缓存文件不会消除这些初始化步骤。后续已将音频改为按需初始化，见[时间轴音频按需初始化](startup.md#record-startup-lazy-audio)。

<a id="record-startup-processor-config-保存位置"></a>
### 保存位置

运算器测速配置现在保存在主程序旁的 `Data/processor-config-MMS-v1.bin`。路径绑定 EXE 所在目录，不随打开模型、工程或文件对话框而改变，也不扩展 `mmconfig.ini` 的旧格式。

启动时先读取新文件。新文件不存在或校验失败时，尝试读取旧的 `%LOCALAPPDATA%/MikuDanceStudio/skinning-MMS-v1.bin`。旧文件有效则直接复用测量，并保存到新位置，保留旧文件。两个位置都没有有效结果时才重新测速。新目录不可写时，已读入的旧结果仍可用于本次启动，内存日志中的 `cache migration saved=0` 会记录迁移失败。

缓存保留版本、CPU、并行线程数、GPU 身份和驱动校验。机器或驱动发生变化时可能需要重新测速。日志使用 `processor configuration`，并记录实际读写路径、迁移结果和缓存命中。

<a id="record-startup-processor-config-启动耗时"></a>
### 启动耗时

对现有发布 EXE 的测量结果如下。每次运行均为空场景、主窗口隐藏且未最小化，保留原有绘制流程，只读取进程内存中的运行日志。未清理系统文件缓存。初始化耗时从创建进程计至 `INIT complete` 被观测到，20 ms 轮询存在采样误差；该指标不是播放帧率，也不等于可见窗口首次显示时间。

| 条件 | 初始化完成 | 测速或缓存核验 |
| --- | ---: | ---: |
| 现有缓存有效 | 3.53 秒 | 0.30 秒 |
| 无缓存 | 9.20 秒 | 5.77 秒 |
| 有缓存，移除可选 MMV4MMS 插件的隔离对照 | 3.47 秒 | 已命中缓存 |

隔离诊断构建在关键调用前后写入内存日志，得到下列单次分段数据。计时点仅存在于诊断副本，不进入交付构建。

| 阶段 | 耗时 |
| --- | ---: |
| D3D9 内置特效创建，`createEffectFromResA`，资源 118 | 1.581 秒 |
| 时间轴音频初始化，`InitTimelineAudio` | 0.613 秒 |
| `InitD3D` 开始至设备创建成功 | 0.414 秒 |
| 运算器缓存核验及迁移 | 0.322 秒 |
| 场景字体纹理初始化 | 0.015 秒 |

这些区间没有覆盖全部启动步骤。内置特效创建和音频初始化是缓存命中后主要的已测耗时，不能把剩余等待都归因于测速。

<a id="record-startup-processor-config-与原版-926-的差异"></a>
### 与原版 9.26 的差异

对照用户指定目录中的 `MikuMikudance.exe`，其 SHA-256 为 `72c34b7ab607aef7b18974742ae18ce62f115f6a614bbedc1a4310e4a5406f72`。两侧均为空场景、相同 `mmconfig.ini`、隐藏且未最小化的窗口，按原版、MMS、MMS、原版顺序运行。原版保留其附带的 MME DLL，MMS 保留 MMV4MMS，因此这组结果是用户运行包的比较，不用于单独归因插件成本。

本组就绪标准是原生帧率计数大于 1 且主窗口可响应消息，比前文的 `INIT complete` 更晚。原版两次为 **2.82、2.33 秒**，MMS 两次为 **4.23、4.07 秒**。不能混用两组判定标准计算加速比例。

进一步提取原版 RCDATA 118 的 HLSL，与 MMS 的 `res/assets/skin_effect_sm3.fx` 进行独立编译对照。两侧使用同一个 `d3dx9_43.dll`、`D3DXCreateEffectCompiler`、`CompileEffect` 和零编译标志，按原版、MMS、MMS、原版顺序调用。原版编译为 **0.187、0.200 秒**，MMS 为 **1.594、1.609 秒**。编译结果大小分别为 35,660 和 118,840 字节。

这项隔离对照排除了窗口、音频、运算器测速和插件加载的影响。当前更复杂的内置特效每次启动都重新编译，较原版增加约 **1.41 秒**，可解释实际启动差距的大部分。MMS 源码中的阴影接收过滤、双线性及三次重建等计算会进入多项着色技术；本次没有通过删减这些效果降低画质。

优先优化方向是保留相同着色器与编译选项，把编译移到构建阶段，或复用有效的编译结果。微软的 [D3DXCreateEffectFromResource 文档](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxcreateeffectfromresource) 确认该接口支持文本及二进制特效。该优化尚未实现，需要验证 MME 拦截和渲染结果兼容性。

原始记录在 `.local/artifacts/startup-cache-20261010/original-comparison/results.json` 及 `effect-compile/results.json`。

<a id="record-startup-processor-config-修改验收"></a>
### 修改验收

前后构建均基于 `fc015a882200ed1787b455271bd556a89c5dea7b`，VS 2022 x64 Release，DX12 开启，`MIKUDANCESTUDIO_PERF=OFF`、`MIKUDANCESTUDIO_DIAG=OFF`。后构建仅增加本次缓存与测试改动。工作区其他正在修改的功能没有进入这组对照。

前、后、后、前四次顺序运行共享相同的窗口配置、插件、硬件和采样方法，初始化分别为 **3.52、3.21、3.07、3.08 秒**。后版本第一次从旧文件迁移，第二次只保留新的 Data 缓存；均未重新测速。该组结果不用于声称缓存迁移提升了启动速度。

后版本无任何缓存时初始化为 **7.87 秒**，测速结果成功写入新文件。随后仅保留新文件再次启动为 **3.05 秒**，没有重新测速。迁移前后缓存的 SHA-256 相同，旧文件保持不变。回归检查覆盖 120 组 GPU/CPU 输出、缓存有效性、路径不随工作目录变化、设置及菜单。

- 前版本 EXE SHA-256：`66d766c6e53ffa68e62e399df4186d799e4532e9ad5e2ec405b4541e61f10136`。
- 后版本 EXE SHA-256：`2a6fb83aa21128bd4c67eed438ebf5c43f5585131301a25e4658fb639313a662`。
- 原发布 EXE SHA-256：`172b7ff3d24e99cd0750548d46edb55a2767c4ec3e458bf95e5daf8d6201f80e`。

<a id="record-startup-processor-config-本地记录与版本"></a>
### 本地记录与版本

原始日志、结果 JSON、源码清单、构建记录和诊断脚本位于 `.local/artifacts/startup-cache-20261010/`。最终读写验收为 `acceptance.json`，最终回归为 `regression-final.json` 和 `regression-final.log`，分段计时为 `diagnostic-warm-01/runtime.log`。

前版本运行副本在 `.local/releases/startup-cache-before-20261010/`。已验收的新版本在 `.local/releases/processor-config-20261010/`，这两个目录均为隔离验证副本。正式 `build/Release/` 未替换，工作区还有其他未提交的功能改动。

以下失败或异常记录保留但不纳入最终对照：首次沙箱探测缺少 `PROCESSOR_IDENTIFIER`，导致缓存校验不匹配；随后从本机硬件注册表恢复该环境值。首次构建遇到沙箱 MSPDB 错误，包含其他未完成改动的源码快照遇到阴影符号链接错误。早期 `compare-*` 中后版本因复制时保留旧时间戳而没有重新编译，EXE 哈希与前版本相同，验收脚本拒绝了该组结果。最终 `final-*` 使用校验过的不同 EXE，断言迁移、新文件写入及连续启动复用均通过。

<a id="record-startup-deferred-effect"></a>
## 空场景显示后编译默认着色器

原始记录：`STARTUP_DEFERRED_EFFECT_20261011.md`。其中的版本、日期和验证范围沿用原记录。

原生渲染在首个空场景成功显示后，启动后台线程编译默认着色器。窗口创建阶段只复制内置源码。Direct3D 效果对象仍在主线程创建，后台线程不访问窗口、场景或设备。

空场景在编译期间继续绘制网格并处理操作。若打开含模型或附件的工程，或者编译期间导入物体，首个物体画面会等待编译完成，避免暂时缺少材质或阴影。设备重置期间延迟接入效果对象；关闭窗口后取消接入，后台线程只保留自己的源码和结果。编译失败时回退到无自阴影的渲染路径，不逐帧重复编译。

MME 在首次 `BeginScene` 前要求通过原有资源入口建立默认效果。因此，检测到 `MMEffect.dll` 时保留同步初始化。该兼容模式不获得本次后台编译收益。Shader Model 3 使用资源 118，兼容配置使用资源 117；本次不修改着色器源码、采样数量或材质设置。

<a id="record-startup-deferred-effect-启动对照"></a>
### 启动对照

两侧以 `e2c36ffeb50685b38e0781ac3e4b48f8fd53f6ea` 为基础，修改后只加入本次默认着色器初始化改动。采用 VS 2022、x64 Release、DX12 开启、PERF 和 DIAG 关闭。运行文件、窗口配置、MMV4MMS 插件和有效运算器缓存一致。均为空场景，窗口隐藏但未最小化，正常执行绘制和 Present，未清理系统文件缓存。

计时从创建进程开始，通过进程内存观察运行日志，轮询间隔约 20 ms。按修改前、修改后、修改后、修改前交替运行：

| 版本 | 初始化完成，第一次 | 初始化完成，第二次 | 平均 |
| --- | ---: | ---: | ---: |
| 修改前 | 2.987 秒 | 3.044 秒 | 3.016 秒 |
| 修改后 | 1.348 秒 | 1.445 秒 | 1.397 秒 |

初始化平均缩短 1.619 秒。修改后的首个空场景 Present 分别在 1.557 秒和 1.634 秒完成，效果对象分别在 3.155 秒和 3.264 秒就绪。后台编译分别耗时 1594 ms 和 1609 ms，主线程接入分别耗时 15 ms 和 0 ms，后者受系统计时粒度影响。

收益是提前显示和响应操作，完整着色器就绪时间没有缩短。修改前未记录首帧标记，不能将其初始化完成时间当作首帧时间。以上为隐藏窗口测量，不是用户桌面上的视觉计时，也不是播放 FPS 测试。

编译期间共发送 103 次窗口响应探测，100 ms 超时阈值下无超时，单次最长约 25.8 ms。四次运行均命中运算器缓存并正常退出。

| 程序 | SHA-256 |
| --- | --- |
| 对照前 | `68778c5fe22791799ad56ee1b445dc6ed3a3c8bdfe422206e0f907e873c80086` |
| 对照后 | `b8838bc63e4ff340fc4927af68bae729e7d7e8ea2a77344b726be612a76e76cd` |

<a id="record-startup-deferred-effect-正确性验收"></a>
### 正确性验收

`tests/deferred_effect_tests.cpp` 以隐藏窗口调用生产初始化、绘制和附件导入入口，拦截编译器以暂停后台编译，并检查下列行为：

- 窗口初始化期间不编译默认效果，第一次成功 Present 后启动编译。
- 编译暂停时空场景继续绘制，设备重置成功。
- 导入实际附件后，首个物体画面等待效果就绪，只编译一次。
- 12 个技术分支的 24 段顶点及像素着色器字节码，与同步资源创建结果逐字节相同。
- 自阴影接收面的 GPU 回读结果逐像素相同，覆盖有光和阴影区域。
- 注入编译失败后禁用不可用的自阴影，不重复编译。
- 加载实际 MME DLL 时，通过原始同步入口创建效果，附件可正常绘制。

最初尝试对 MME 同样延后编译，触发其默认效果初始化错误。该失败保存在 `integration-mme/`，引入同步兼容路径后测试通过。

随后在 `5a8cdefe0317b712d50cf0854ebf5f9b261cc0bd` 上整合当前阴影、显示兼容性和资源更新，重新构建并通过上述原生及 MME 测试。最终程序还通过实际启动及编译期间关闭检查，正常退出。SM2 设备路径未作硬件验收。

<a id="record-startup-deferred-effect-版本与原始记录"></a>
### 版本与原始记录

原始日志、配置、源代码状态、补丁和运行结果位于 `.local/artifacts/deferred-effect-20261011/`。`source-performance/` 保留性能对照源码，`source/` 为最终整合源码。构建目录为 `.local/builds/deferred-effect-x64-20261011/`。早期编译失败和 MME 失败日志保留在同一归档中。

- `before-01/`、`after-01/`、`after-02/`、`before-02/`：交替启动记录。
- `integration-final-native/`、`integration-final-mme/`：性能对照源码的正确性测试。
- `integration-integrated-native/`、`integration-integrated-mme/`：最终整合源码的正确性测试。
- `integrated-startup/`、`integrated-close-during-compile/`：最终程序启动与关闭检查。
- `perf-summary.json`：性能汇总。

最终验证包为 `.local/releases/deferred-effect-20261011-integrated/`，EXE SHA-256 为 `2c9875660db64090a70494602e12fbfe889f47dad89ab7f9182fdb47da2807e4`。本次未替换正式 `build/Release/` 包，也未发布新的 GitHub Release。

<a id="record-startup-lazy-audio"></a>
## 时间轴音频按需初始化

原始记录：`STARTUP_LAZY_AUDIO_20261011.md`。其中的版本、日期和验证范围沿用原记录。

新工程保持空音轨，不创建 DirectSound 设备、音频缓冲区或播放线程。打开含音频的 PMM 工程、拖拽音频、从菜单选择音频时，先读取音频，再创建设备和实际播放缓冲区。取消导入、空工程播放和空 PMM 载入均不触发设备初始化。

`CreateUIControls` 只绑定时间轴绘图句柄、主窗口和语言信息。三个导入入口共同调用 `LoadWaveFile`，由 `WaveStartPlayback` 按需创建设备，后续导入复用设备。移除了启动时创建临时空缓冲区并短暂播放的步骤。

新建工程会停止音频线程、释放设备及缓冲区、清除波形和音频路径。初始化失败不会启用播放，之后可以重新导入。处理线程清理时避免重复释放线程已经释放的缓冲区；空音轨的定位调用直接返回。

<a id="record-startup-lazy-audio-功能验收"></a>
### 功能验收

`tests/wave_audio_tests.cpp` 使用隐藏的主窗口和无声 PCM 文件，调用生产代码的菜单导入、拖拽处理、PMM 保存及载入、新建工程和播放入口。拦截 `DirectSoundCreate` 统计实际调用，并注入一次设备创建失败。文件对话框由测试直接返回选择或取消，错误提示由测试截获，不弹出窗口。

检查通过：首次启动与取消导入的设备创建次数为零；菜单、拖拽和含音频 PMM 导入均建立设备、缓冲区及波形；空 PMM 保持空音轨；播放后新建工程释放资源；初始化失败后重新导入成功；连续导入复用设备。程序正常退出，主窗口始终隐藏。

原始结果位于 `.local/artifacts/lazy-audio-20261010/integration/test.log` 和 `result.json`。测试程序 SHA-256 为 `bd096f4883891725297192c87d7200109eef18c4d9b93d52cf57ba79dc3437cc`。

<a id="record-startup-lazy-audio-启动对照"></a>
### 启动对照

对照以 `d1e88fbeae393b599d8ba832c5431055e3d472a4` 的源码为基础。修改前复用此前已验收的运算器配置版本；修改后从该提交的独立源码快照构建，仅应用本次音频改动。并行编辑中的其他功能未纳入对照。编译配置为 VS 2022、x64 Release、DX12 开启、PERF 和 DIAG 关闭。

两侧使用相同运行文件、窗口配置和有效运算器缓存，均为空场景、隐藏且未最小化。计时从创建进程至观察到内存日志 `INIT complete`，轮询间隔为 20 ms。没有清理系统文件缓存。按修改前、修改后、修改后、修改前的顺序测量：

| 版本 | 第一次 | 第二次 |
| --- | ---: | ---: |
| 修改前 | 3.068 秒 | 3.268 秒 |
| 修改后 | 2.940 秒 | 3.184 秒 |

本组平均缩短 0.106 秒，单版本波动约 0.20 至 0.24 秒，因此不能据此声称稳定缩短 0.6 秒。此前单次音频阶段的 0.613 秒属于不同运行时的测量。功能检查确认本次已消除空工程的设备初始化；本组测试中内置着色器编译耗时仍存在。后续已将原生空场景的编译延后至首帧显示之后，见[默认着色器后台编译](startup.md#record-startup-deferred-effect)。

- 修改前 EXE SHA-256：`2a6fb83aa21128bd4c67eed438ebf5c43f5585131301a25e4658fb639313a662`。
- 修改后 EXE SHA-256：`68778c5fe22791799ad56ee1b445dc6ed3a3c8bdfe422206e0f907e873c80086`。
- 四次原始运行记录及配置位于 `.local/artifacts/lazy-audio-20261010/a1`、`b1`、`b2`、`a2`，汇总为 `comparison.json`。

<a id="record-startup-lazy-audio-本地版本"></a>
### 本地版本

验证版位于 `.local/releases/lazy-audio-20261010/MikuMikuDance.exe`。正式 `build/Release/` 未替换，工作区仍有其他功能正在编辑。独立源码、工作区改动快照、构建记录和测试脚本保存在 `.local/artifacts/lazy-audio-20261010/`，独立构建在 `.local/builds/lazy-audio-x64-20261010/`。

首次配置在沙箱编译器检查阶段卡住并中止；测试首次构建缺少 Windows 头文件和函数声明，随后遇到资源变量定义顺序问题。对应失败日志均保留。修复后 `build-tests.log` 构建成功，功能测试正常退出，四次启动对照均命中缓存且正常退出。

<a id="record-pmm-startup-calibration"></a>
## PMM 加载与启动测速

原始记录：`PMM_STARTUP_CALIBRATION_20261008.md`。其中的版本、日期和验证范围沿用原记录。

MMS 自动顶点计算原先在每个模型第一次更新时执行 CPU/GPU 对照。加载工程会销毁模型及其测速缓存，下次打开便再次执行。`Stage.pmm` 的这轮记录中，10 个有网格的模型分别测试了枚举到的 5 个 GPU 计算设备条目，共进行了 50 次设备初始化和候选测量；这些模型最后都选择了 CPU。这里的 5 个条目包含多个 Intel 适配器标识，不表示机器有 5 块物理显卡。

用户要求改成程序启动时测速，工程加载只复用结果。当前实现按这一生命周期工作。

<a id="record-pmm-startup-calibration-行为"></a>
### 行为

- 创建主窗口和 D3D9 设备后、分派命令行中的工程文件前，调用 `skinning::InitializeAutomatic`。只有自动模式执行测速，CPU 或手动设备模式保持原有行为。
- 启动时用独立的 PMD 和 PMX 数据各测试 1,024、32,768、262,144 个顶点，覆盖串行与并行区间。数据不进入工程、MME 对象列表或物理世界。
- 每个 GPU 候选在启动时初始化一次。各档测试复用设备和着色器管线，保留 CPU/GPU 结果校验、传输、回读以及 D3D9 缓冲写入的实际成本。
- 每档测试完成后等待 GPU 工作结束并释放大块临时缓冲，保留设备和管线。启动结束也释放 CPU 侧的临时顶点数组。
- 实际模型按格式和顶点规模选取最接近的启动测量。更换模型缓冲、删除模型、新建或重开工程只清理模型的选择记录，不清除启动测量。加载与逐帧更新均不再启动测速或创建自动模式的计算设备。
- 不支持的权重继续使用 CPU；设备失败时按已有测量选择可用候选，不重复测速。没有完成启动初始化的非 GUI 调用者安全回退到 CPU。
- 菜单改为「自动（启动时测速）」。关闭程序时释放结果，下次启动重新测量。

按规模选择是对真实模型成本的估计，不等同于对每个工程模型重新计时。它满足加载期间不测速的要求；手动 CPU/GPU 选择仍可用。

<a id="record-pmm-startup-calibration-测量"></a>
### 测量

素材为 `D:/MMD-Next/tests/ToonGlass_Ray/UserFile/Stage.pmm`，保持相同的 MME、MMHack、D3D9 代理、相机、视区及窗口配置，未加载 MMV4MMS。每次只运行一个被测程序，主窗口隐藏且未最小化。程序从命令行直接打开工程；耗时从创建进程计至 11 个模型存在、程序原生 FPS 计数开始更新且主窗口能响应消息。100 ms 轮询会引入约一个轮询间隔的误差；数据包含初始化和首次绘制，不是纯磁盘读取时间。未清空操作系统文件缓存。

| 程序 / 配置 | 启动并打开工程 |
| --- | ---: |
| MMD 9.26 CHS，两次 | 17.50 秒、17.33 秒 |
| 修改前 MMS，自动模式，两次 | 28.83 秒、27.59 秒 |
| 修改前 MMS，临时 CPU 模式，两次 | 13.31 秒、12.58 秒 |
| 最终 MMS，自动模式 | 17.07 秒 |

CPU 模式只用于隔离诊断，没有修改用户的全局配置。约 15 秒的差值与旧实现的逐模型校准相符。模型槽位在 MMS 启动约 3.7–3.9 秒时已齐全，随后仍等待校准和特效初始化；主要额外成本不是 PMM 文件本身的解析。

最终程序的启动测速耗时 **4.40 秒**，已计入上表 17.07 秒。同一进程随后用 `WM_DROPFILES` 连续重开同一工程两次，分别约 **8.22 秒、7.89 秒**。这两次使用已有文件及特效缓存，不能当作独立启动时间。观测到三次 `LOAD_SCENE`，仅一次启动测速开始/结束，六份规模报告全部生成，重开过程中没有新的模型测速。

前一候选运行曾测得启动 18.49 秒、测速 5.08 秒；另一次同进程检查为启动 17.24 秒、重开 8.09/8.30 秒。记录均保留，没有只保留较快结果。最终结果与原版本体接近，不能据这几次运行承诺所有工程均快于原版。

<a id="record-pmm-startup-calibration-验证"></a>
### 验证

- `skinning_backend_tests` 通过 120 组 GPU/CPU 对照，覆盖 PMD/PMX、额外 UV、兼容回退、设备选择和菜单。新增检查验证六份启动测量、重复初始化无操作、模型缓冲替换、删除再加载以及偏好切换均不增加测速次数。
- `d3d12_skinning_tests` 在 `SourRIN/White.pmx` 的五组姿态上通过，含表情、骨骼缩放和剪切。最大绝对误差为 `1.90734863e-06`，原有 D3D9 缓冲身份保持。
- 新增缓冲释放检查确认旧输出凭据失效、计算设备身份保持，并能重新提交得到逐字节相同的结果。
- 先前报告中的菜单文案断言已按新的启动测速文案更新，本轮完整 `skinning_backend_tests` 通过。
- 最终 EXE 另做真实 PMM 连续重开验证。测试后所有自建进程退出，未改动原始 PMM、EMM 或用户配置。

<a id="record-pmm-startup-calibration-构建与记录"></a>
### 构建与记录

基于 `243a154`，正式 `build/`，VS 2022 x64 Release，`MIKUDANCESTUDIO_PERF=OFF`、`MIKUDANCESTUDIO_DIAG=OFF`。

- 修改前 EXE SHA-256：`9c11bb0bc9c87441f68a413336e8f771c2c1c19c5926b7879b2d81a3edb26b4f`。
- 最终 EXE SHA-256：`a8ac6e94aae8731d08e71985ccf3cf2f4af07620e8e83e85a75b35f82abc13eb`。
- 正式程序：`D:/MMD-Next/MMS/build/Release/MikuMikuDance.exe`。
- 修改前 EXE、MAP、PDB：`.local/archives/pmm-startup-calibration/`。
- 原始记录、内存日志、私有测试副本及校验清单：`.local/artifacts/pmm-load-926/`。
- 最终三次加载记录：`mms-auto-final-01/result.json`；功能检查：`regression/manifest.json`；总清单：`final-manifest.json`。
- 独立启动脚本：`.local/probe_pmm_load.py`、`.local/probe_pmm_load_after.py`；连续重开脚本：`.local/probe_pmm_reload.py`。
