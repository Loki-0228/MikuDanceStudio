# 烘焙与导出验证记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [物理烧录验收：2026-09-26](#record-physics-baking-validation)
- [烘焙通道、容量与 MMV 启停验收](#record-baking-channels-validation)
- [ABC 导出与网格烧录验收](#record-alembic-export-validation)
- [物理烘焙撤销状态](#record-physics-bake-undo-state)
- [物理烘焙视窗预览](#record-physics-bake-preview)
- [烘焙预览与部分录入](#record-bake-preview-and-partial-recording)
- [烧录通道全选](#record-bake-channel-select-all)
- [烘焙警告逻辑修复](#record-bake-warning-logic)
- [帧操作与网格烧录修复](#record-frame-and-bake-fixes)

<a id="record-physics-baking-validation"></a>
## 物理烧录验收：2026-09-26

原始记录：`PHYSICS_BAKING_VALIDATION_2026-09-26.md`。其中的版本、日期和验证范围沿用原记录。

本次实现物理烧录、骨骼逐帧重采样、高级物理参数和非加密文本预设。使用方式、默认值和范围边界规则见[物理烧录说明](../../guides/physics-baking.md)。

<a id="record-physics-baking-validation-版本与构建"></a>
### 版本与构建

- 基线：`1e9c0910c0bb2415c4b5597008207facf6860c03`，工作树 `.local/worktrees/physics-baking-before`，构建时无修改。
- 候选：从同一基线建立 `feat/physics-baking`，工作树 `.local/worktrees/physics-baking`。测量时包含本次待提交实现；提交后的确切 SHA 与文件哈希记入 `.local/artifacts/physics-baking/manifest.json`。
- 两侧使用 Visual Studio 2022 / MSVC v143、x64、Release、C++17，以及同一份 `build/generators/conan_toolchain.cmake`。最终均启用 `MIKUDANCESTUDIO_BUILD_EFFECT_TESTS=ON`、`MIKUDANCESTUDIO_BUILD_UI_TESTS=ON`。
- 分别从两侧生产源码构建正式应用、`frame_benchmark` 和适用回归目标。原始配置、构建日志与测量数据保留在 `.local/artifacts/physics-baking`。

<a id="record-physics-baking-validation-功能验证"></a>
### 功能验证

基线的 `physics_pose_tests`、`pmx_joint_tests`、`bone_registration_tests` 三项全部通过。候选的这三项，加上 `physics_bake_settings_tests` 与 `physics_bake_tests`，五项全部通过。本轮执行的是相关回归集合，未宣称全仓测试通过。

真实场景使用用户提供的 `D:/MMD-Next/tests/UserFile/测试刚体.pmm`（140 根骨骼、45 个刚体、27 个关节），输入素材未改动。测试结果如下：

- 默认配置烧录第 0–10 帧，23 个目标物理骨骼、253 个范围内物理关键帧；含边界及链表修改共 298 个关键帧记录变更，执行 22 次 `1/60` 秒物理步进。
- 中途取消，以及完成但尚未提交的计算，都保持原模型记录、骨骼数组、关键帧池逐字节一致，原刚体变换不变。
- 相同输入重复计算，结果逐字节一致。倍率和子步数同时从 1 改为 2 后，步数变为四倍；分别将 ERP 改为 0.5、CFM 改为 0.1 后，结果各自变化，确认参数进入求解路径。CFM 作用于关节；当前 Bullet 2.75 的接触 CFM 固定为 0，界面和说明已明确该范围。
- 非零起点、单帧、只选中一根骨骼，以及独立普通关键帧重采样通过。范围外已有关键帧的姿态和物理标记保持，仅更新必要链表链接；边界新增帧按功能说明处理。
- 超容量请求拒绝写入。一次撤销恢复完整原关键帧池及末帧，重做恢复完整结果。没有变化的操作不占用撤销记录。
- 回放第 2、7、10 帧时，烧录骨骼处于禁用物理状态。PMM 保存重读后逐骨骼、逐帧的位置、旋转与物理标记保持一致；VMD 导出含 275 个原生禁用物理标记（包括源动作中已有的标记）。
- 测试发现源动作第 0 帧存在零四元数；离线求值与采样复用现有姿态修复规则后，保存重读一致。

中、英、日主窗口和高级设置窗口均通过隐藏原生对话框测试，检查标题、默认值、计算频率更新、科学计数法输入和按钮边界。通过窗口自身的 `WM_PRINT` 回读检查布局，没有使用桌面截图或弹出测试窗口。隐藏组合框的当前文字不由该回读路径完整绘出，回读图仅用于控件布局检查；未穷举其他 DPI、主题或屏幕阅读器。

预设测试覆盖 Unicode 名称与路径、JSON 转义、BOM、科学计数法、数值和大小边界、重复/未知/缺失字段、非法编码、坏版本、截断内容、失败不改变设置，以及只读文件保存失败后原文件保持完整。

<a id="record-physics-baking-validation-常规帧处理对照"></a>
### 常规帧处理对照

模式为后台 `frame_benchmark --throughput`，素材 `Miku_Hatsune_Ver2.pmd`，1280×720，预热 120 帧、扩展采样 6000 帧，CPU 蒙皮，相机 `0,12,0,-45,0,0,0`。物理、描边和透明排序保持相同设置；未通过关闭效果减少负载。两侧均为 12354 个顶点、45 个刚体、27 个关节，6120 次模型绘制视口检查均无不匹配。

在最终应用链接完成、构建和委派任务结束后，短采样出现波动，因此用每轮 6000 帧按 A–B–B–A 顺序复核：

| 样本 | 版本 | 平均 ms/帧 | 中位 ms/帧 | P95 ms/帧 |
| --- | --- | ---: | ---: | ---: |
| before-extended-1 | 基线 | 0.6430 | 0.6047 | 0.8557 |
| after-extended-1 | 候选 | 0.5999 | 0.5743 | 0.7755 |
| after-extended-2 | 候选 | 0.5944 | 0.5775 | 0.7140 |
| before-extended-2 | 基线 | 0.6722 | 0.6325 | 0.9343 |

最终 600 帧短样本（`*-final-*`）中，基线均值为 0.6450/0.6688 ms，候选为 0.6948/0.7686 ms，候选平均约慢 11.4%。扩大到 6000 帧后该差异未复现，候选反而更快；先前受控短样本（`*-controlled-*`）也未出现相同方向的差异。完整记录不支持稳定退化或稳定提速的结论。该小场景和有限重复次数不足以证明性能提升。数据为后台提交/等待路径的吞吐测量，不是窗口播放 FPS；未单独取得 CPU/GPU 阶段耗时，不作推算。

早期 `before-a.log` 因相对 JSON 输出路径失效而不用于正式对照；`before-b` 仅作试运行；首批 `*-abba-*` 与后续测试编译有时间重叠，也不用于结论。全部原始记录保留。`*-controlled-*` 是最后文案与频率显示边界调整前的受控记录；最终源码的性能记录为 `*-final-*` 与 `*-extended-*`。

<a id="record-physics-baking-validation-范围与限制"></a>
### 范围与限制

- 普通关键帧烧录是逐帧重采样，保留 IK 和表情轨道；没有加入 IK 展平、关键帧精简或多模型批量写入。
- 「MMD 60 Hz」仅为参考频率入口，不代表已完成原版 MMD 运动一致性验收。噪声使用确定性的本地序列。
- 本轮真实素材验证为上述 PMM；未穷举 PMX 骨骼表情、所有跨模型父子依赖、第三方 MME 或录制路径。
- 物理状态边界帧属于预期新增关键帧。原范围外姿态关键帧保留，但边界附近插值可能改变。

<a id="record-physics-baking-validation-dsh-委派验收"></a>
### DSH 委派验收

通过 [dsh-delegate](C:/Users/lq/.agents/skills/dsh-delegate/SKILL.md) 派发单模块任务 `46779248-18f0-44e4-9d8e-6e3c1e397fb5`，实际 cwd 为 `D:/MMD-Next/MikuDanceStudio-MME/.local/worktrees/physics-baking`。交付 `src/physics/physics_bake_settings.cpp` 和 `tests/physics_bake_settings_tests.cpp`，主 agent 复核源码并纳入正式 CMake 构建、运行测试。首次转义用例错误地允许名称内制表符，按规格修正测试后通过。最终正式结果为 `completed`，预设模块验收通过；未由子任务提交或推送代码。

<a id="record-baking-channels-validation"></a>
## 烘焙通道、容量与 MMV 启停验收

原始记录：`BAKING_CHANNELS_VALIDATION_2026-09-27.md`。其中的版本、日期和验证范围沿用原记录。

本次增加按需增长的骨骼键池、MMS 原生通道选择，以及 MMV 网格筛选和效果实例区间启停键。操作说明见 [物理烘焙](../../guides/physics-baking.md)、[网格烘焙](../../guides/mesh-export.md) 和 [存储](../extensions/usage.md#record-mmv-storage)。

<a id="record-baking-channels-validation-来源与证据"></a>
### 来源与证据

来源为 `e27972b` 加本次烘焙改动，保留之前的视口修改。后续原生骨骼操作面板单独提交。构建使用 Visual Studio x64 / Release，目标为 `MikuDanceStudio`、`mmv_all` 及对应测试。

前版为 `.local/releases/mme-null-after/`，来源 `541ab70`。后版位于 `.local/releases/mmv-bake-after/`：`native`、`mme`、`combined` 隔离无插件、仅 MME、MMV/MME 组合；`ui-compat`、`ui-dx12`、`postprocess` 使用实际宿主核心。

原始记录在 `.local/artifacts/mmv-bake-fixes/`。`build-hashes.json`、`readback-hashes.json`、`final-tests-3.json`、`final-readbacks.json` 保存产物和样本。DSH 的容量、通道、MMV 启停/布局三个子任务均在正式仓库运行，正式结果保留在同目录；主 agent 复核跨模块提交、MME 分配恢复与最终运行结果。

<a id="record-baking-channels-validation-已执行验证"></a>
### 已执行验证

| 范围 | 结果 |
| --- | --- |
| 动态骨骼池 | `bone_key_pool_tests`、`bone_registration_tests`、`physics_bake_tests --capacity` 通过。验证超过旧边界的槽位、登记、撤销/重做和 VMD 记录。 |
| 真实 PMM | `测试刚体.pmm`：池增长至 983,040 槽，实际键写入槽 600,000。140 骨骼、45 刚体、27 joint；23 个物理骨骼烘焙，PMM/VMD 往返比较 299/299 已分配骨骼键通过。`run-capacity-scene-final.log`。这不是生成 983,040 个有效键的压力测试。 |
| 稀疏骨骼 | 2..8 帧、步长 3，仅写指定物理骨骼的 2/5/8 帧，每键禁用物理；非法骨骼索引被拒绝。 |
| 原生通道 | `channel_bake_tests` 16 阶段通过：主页/模型页隔离、附件子参数、范围外曲线、选择、容量失败与模型切换。中途失败恢复表情/显示池和帧范围。`channels/run-rollback-final.log`。 |
| MMV 启停 | 451 检查/8 用例、CPU runtime 427 检查/12 用例及两版生产 DLL ABI 测试通过。覆盖 Step 键、区间外恢复、VMM/VEM、作者变量重名和事务失败。 |
| 缓存 | 旧 ABC 和带区间/源顶点映射的新缓存通过；未选顶点保持源动画，退出区间不使用缓存。 |
| 实际渲染组合 | `run-native-integration-1.log`、`run-mme-integration-3.log`、`run-combined-integration-3.log` 通过。覆盖附件变换、局部顶点、PMM 重开、清除缓存和 MME 分配恢复。 |
| 实际参数界面 | 两版 `run-ui-*-final3.log` 为 0 失败：三语、800/560 宽度、参数窗移动/缩放、下拉框打开后 60 个生产帧保持展开、曲线入口。图像来自自身 `WM_PRINT`，未截取桌面。 |
| 分隔线 | `run-editor-sep-4.log`：17 用例，4,269 检查通过。配置和文件按钮下方绘制分隔线，上下各 12 逻辑像素；覆盖三语、最小窗口及实际像素。 |

<a id="record-baking-channels-validation-同条件-dx9mme-回读"></a>
### 同条件 DX9/MME 回读

使用 Rou PMX、SvSSAO.x 和 `identity_mmv.fx`，相同 640×480 目标、素材、相机、物理与采样方法。前后均为 `EMMAutoSave=true`。测试断言目标包含几何、MMV 发布独立网格、恒等效果保持画面，以及 MME 离屏目标分配可以保存。

采样顺序：前 `before-readback-2`、后 `after-readback-2-first`、前 `before-readback-3`、后 `after-readback-2`。两次后版原先共用标签，第一次在下一次运行前完整复制为 `-first`；原始日志和画面均保留。

| 批次墙钟毫秒 | 前版两次 | 后版两次 |
| --- | --- | --- |
| 分配后 60 帧 | 23,164.656 / 23,039.103 | 23,438.483 / 23,167.900 |
| 重载后 8 帧 | 2,799.823 / 2,779.432 | 2,769.395 / 2,784.950 |
| 编辑器刷新 30 帧 | 10,479.588 / 10,384.408 | 10,367.145 / 10,450.883 |
| 保存后 8 帧 | 2,867.709 / 2,726.286 | 2,859.397 / 2,790.640 |

每次内部 baseline/assigned/reloaded 逐字节相同。前三个样本跨版本也相同；最后一次后版仅 1 个像素的一个 RGB 通道相差 1，平均 RGB 绝对差 0.000001085。没有观察到明确性能变化。上述为包含同步等待的整批墙钟时间，不是 FPS 或 GPU 时间；没有独立测出计算、回读、上传和绘制耗时。

<a id="record-baking-channels-validation-失败记录与边界"></a>
### 失败记录与边界

补充夹具验收：`.local/artifacts/mmv-bake-fixes/run-fixture-extra-persist.log` 与 `run-fixture-varying-persist.log` 均通过真实后台 DX9 的网格缓存、重开工程和 Alembic 往返。测试专用 MMV 夹具增加按对象身份保存的 `.fixture-mmvranges` 侧文件，修复夹具重开时丢失 FX 开关范围的问题；生产插件继续使用 `.vmm`。首次失败结果 `run-fixture-extra-final.log` 保留。

- `before-readback-1` 的 45 秒超时不足，排除；改用 120 秒上限后完成。
- `after-readback-1` 的目录关闭 EMM 自动保存，图像通过而存档检查失败；统一配置后重新交替采样。
- `ui-compat-final` 因旧 PowerShell 错读 UTF-8 脚本损坏素材路径，改用 PowerShell 7。`final2` 的测试错误区分 Win32 类名大小写，修正后得到有效的 `final3`。
- 初版分隔线子控件未在 `WM_PRINT` 绘制，最终由父窗口统一绘制屏幕和离屏图。失败日志保留。
- 增长的是 MMS 骨骼键池，不改变原版 MMD 的内存限制；表情、显示及部分场景轨道仍有既有容量，超限拒绝提交。
- 原生物理输出骨骼。当前 MMV Bullet3 输出变形网格，须用网格缓存，未实现任意网格反解骨骼动作。
- 通道支持准备、失败恢复和取消，尚未把新通道做成统一长期撤销历史。未做分配器故障注入；异常恢复经代码审查及中途容量失败用例验证。

<a id="record-alembic-export-validation"></a>
## ABC 导出与网格烧录验收

原始记录：`ALEMBIC_EXPORT_VALIDATION_2026-09-27.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-alembic-export-validation-构建与证据"></a>
### 构建与证据

基线为 `4b107c2`，工作树位于 `.local/worktrees/alembic-baseline`。功能提交为 `54bfe14`，直接在正式主仓库构建。两侧均为 VS 2022 x64 Release，MSVC 14.44、Windows SDK 10.0.26100，启用 `MIKUDANCESTUDIO_BUILD_UI_TESTS` 和 `MIKUDANCESTUDIO_BUILD_RENDER_TESTS`。Conan 工具链位于主仓库 `build/generators/conan_toolchain.cmake`。完整 CMake 参数、Git 状态与 EXE 哈希随性能清单保留。

| 生产文件 | SHA256 |
| --- | --- |
| 基线 `MikuMikuDance.exe` | `83B23DDE3804A5DD61B2F870AC3713A8C5D2AB76A60859A2F42B0039D0D5661B` |
| 功能版 `MikuMikuDance.exe` | `80AE6C44DE90B39AEA2768A20C3E08E5DA30473B163FC34D1D7162FD93F7522F` |
| 正式目录兼容版 `MMV.dll` | `DDFC377BE373F9745F5226E6BF04B2663157ADF4D063C1AE7BEFE59511618598` |

原始结果位于 `.local/artifacts/alembic-export/`，包含成功与失败的开发日志，不把失败样本当作验收结果。`release-manifest.json` 记录发布文件、功能日志和 DSH 会话；DSH 实际工作目录为 `D:/MMD-Next/MikuDanceStudio-MME`。网格缓存、菜单、顶点捕获与 MMV 事务模块经过主 agent 审查及统一构建；MMV 事务独立验证另存于 `.local/artifacts/mmv-mesh-bake/`。

<a id="record-alembic-export-validation-功能结果"></a>
### 功能结果

| 验证 | 结果与范围 |
| --- | --- |
| `vertex_capture_tests` | 95 项实际 D3D9 HAL 检查通过，含 VS 1.1/2.0/3.0、状态恢复、拒绝不支持路径，以及 300000 顶点的两块回读。 |
| `mesh_bake_cache_tests` | 真实 Ogawa 文件采样、PMM 配套文件往返、替换、取消提交、清除与多实例临时文件隔离通过。 |
| `mmv_runtime_tests` | 208 项检查通过，使用 GPU 替身，验证 BEGIN/暂存/提交/取消，以及 root、材质、骨骼、owner 和 VEM 自动发现绑定的停用；不是 GPU 性能测试。 |
| ABC 原生路径 | 生成三角形 PMX，3 个样本的骨骼位移正确；Unicode 文件名、30/60 fps 采样、取消保留旧文件、模型及时间轴恢复通过。 |
| MME 顶点变形 | VS 将 X 增加 3，ABC 回读 3 个样本各顶点均增加约 3.00001。测试效果覆盖 `object` 和 `object_ss` 通道。 |
| MMV + MME | MMV DX12 先将 X 增加 2，再由 MME 增加 3，ABC 各样本回读约 5.00002。纯 DX12 版与正式目录所用兼容版 DLL 均通过完整导出、烧录和清除流程，日志为 `combined.log`、`combined-compat.log`。 |
| 新增与变化的拓扑 | 测试插件追加材质和三角形；变化模式在三次采样中依次产生 3/6/3 个顶点和 1/2/1 个三角形，空样本正确。 |
| 烧录和恢复 | 只烧录、同时输出、取消、PMM 保存重开、旧 EMM 再停用通过；烧录后把骨骼末帧 X 改为 12，缓存不变，清除后 X=12。 |
| 清除菜单 | 从「烘焙」命令路由打开对话框并实际清除。清除当前模型保留附件缓存，清除全部移除剩余缓存。 |
| 附件 | 生成 X 网格，改变附件关键帧的平移和缩放后，已烧录的最终坐标保持不变。 |
| 条件警告 | 三种语言分别检查默认 ABC 勾选、ABC 勾选不弹警告、拒绝烧录警告撤回勾选、确认勾选、取消勾选不再提示。 |
| 原有物理烧录 | 前后两侧隐藏运行同一 `tests/UserFile/测试刚体.pmm`：140 骨骼、45 刚体、27 joint；均烧录 23 骨骼、298 项修改、22 步，PMM/VMD 往返通过，`failures=0`。 |

`ctest -C Release -R "^(vertex_capture|mesh_bake_cache|mmv_runtime)$"` 的 3 项测试全部通过。所有集成测试使用隐藏窗口；未读取桌面截图。完整入口和运行目录要求见 [tests/README.md](../../../tests/README.md)。

<a id="record-alembic-export-validation-性能对照"></a>
### 性能对照

前后生产源码构建的 `frame_benchmark` 在相同素材、相机、1280×720 D3D9 视口下按 ABBA 顺序采样。指标为等待 GPU 完成后的批次毫秒/帧，不是播放 FPS、独立 GPU 时间或 CPU 提交时间。此对照检查未启用新功能时的渲染开销，不能代表导出吞吐率或缓存回放速度。

设备为 NVIDIA GeForce RTX 4070 Laptop GPU。每次预热 15 帧、记录 60 帧，每场景两轮 ABBA，两侧各 4 次，全部 16 次退出码为 0，采样与视口检查通过。两侧构建输入均已提交且无未提交源码。工作区另有其他任务的文档变动，原样记录在清单中，不属于本功能。

| 场景 | 基线中位 ms/帧（范围） | 功能版中位 ms/帧（范围） | 中位变化 |
| --- | --- | --- | --- |
| 单份 HGrass，471122 顶点 | 201.358（194.912–212.046） | 201.289（197.063–205.634） | -0.03% |
| Rou，移动相机 | 6.968（6.673–8.586） | 6.679（6.527–7.728） | -4.15% |

本轮未见中位耗时增加，两侧数值范围重叠；人物场景的差异不作为稳定性能提升结论。原始 JSON、日志、素材哈希、构建参数、提交与汇总保留在 `.local/artifacts/alembic-export/performance/`。

```powershell
python tools/run_alpha_background_compare.py `
  --before-tree .local/worktrees/alembic-baseline --after-tree . `
  --output .local/artifacts/alembic-export/performance `
  --grass D:/MMD-Next/tests/Grass/HGrass.pmx `
  --character 'D:/MMD-Next/tests/MMD ☆★Rou/☆Rou.pmx' `
  --cases grass-1-spread,face-moving --warmup 15 --samples 60 --cycles 2
```

复跑时选择新的输出目录，保留已有结果。

<a id="record-alembic-export-validation-边界"></a>
### 边界

验证使用合成 MME/MMV 变形与实际本地物理素材，没有覆盖所有第三方效果。屏幕后期、自定义投影、依赖历史渲染目标的效果以及非标准绘制有明确限制，见[功能说明](../../guides/mesh-export.md#abc-文件)。旧插件缺少烧录事务通知时会拒绝工程烧录；只导出 ABC 的能力不受该接口限制。

开发中修正了 Windows INI 刷新返回值的误判：`WritePrivateProfileStringW(NULL, NULL, NULL, file)` 刷新缓存时返回 0，该值不能按普通写入失败处理。依据 [Microsoft API 文档](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringw)。

<a id="record-physics-bake-undo-state"></a>
## 物理烘焙撤销状态

原始记录：`PHYSICS_BAKE_UNDO_STATE_20261009.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-physics-bake-undo-state-原因"></a>
### 原因

物理开关属于骨骼编辑状态。烘焙产生的关键帧会关闭相应骨骼的实时物理，避免模拟结果被再次模拟。

原有 type-5 撤销记录保存了关键帧和位姿，但位姿快照的 `physicsDisabled` 字段实际复制的是 `ModelRecord::bonePhysicsState`，即骨骼的未注册编辑标记。当前开关值保存在 `BoneRecord::physicsDisabled`，两者都需要还原。

撤销还原关键帧后不会重新读取当前帧，所以骨骼和刚体仍会保留烘焙后的关闭状态。MMN 与 MMS 都存在这条路径。

<a id="record-physics-bake-undo-state-修改"></a>
### 修改

- type-5 提交在现有位姿快照的 `reserved[0]` 中保存实际物理开关，保留未注册编辑标记和原有结构大小。
- 撤销恢复烘焙前的开关，并为重做捕获当前开关；重做恢复捕获值。
- 按关键帧控制物理时，通过 `NotifyBonePhysicsMode` 同步原生刚体。全局关闭或始终开启模式保留自身的控制方式。
- 按钮和快捷键都传入当前全局物理模式。快捷键补齐场景修改标记和物理重置请求。
- 重做先读取当前帧，再刷新关键帧面板。

<a id="record-physics-bake-undo-state-构建与运行记录"></a>
### 构建与运行记录

本次不添加或运行测试。运行中的烘焙、撤销和重做效果尚未实测。

修改前的程序保存在 `.local/archives/physics-bake-undo/` 的时间戳子目录。编译日志及发布记录保存在 `.local/artifacts/physics-bake-undo/`；MMN 的正式发布流程另存 `.local/artifacts/release-build/`。

<a id="record-physics-bake-preview"></a>
## 物理烘焙视窗预览

原始记录：`PHYSICS_BAKE_PREVIEW_20261009.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-physics-bake-preview-操作"></a>
### 操作

在「关键帧烧录」窗口勾选「视窗滚动预览」，再开始物理烘焙。视窗随计算进度显示已经完成的采样帧。

取消勾选可关闭预览，减少绘制开销。开关默认开启，程序运行期间记住最近一次选择；烘焙过程中锁定该设置。

<a id="record-physics-bake-preview-实现"></a>
### 实现

原生物理烘焙显示离线模型副本。预览按约 66 毫秒节流，并显示最后一个采样帧，不改变物理步长和关键帧采样间隔。同步绘制后恢复模型槽、编辑帧和播放游标。

MMN 预览离线物理结果时直接绘制已计算的骨骼，避免再次执行实时节点和物理；分层烘焙则呈现已有的分层采样输出。MMS 使用已有的原生烘焙预览实现。

完成、取消或失败后，窗口解除锁定并请求刷新原来的编辑视图。骨骼和关键帧仅在烘焙成功后提交。

<a id="record-physics-bake-preview-验证状态"></a>
### 验证状态

本次不添加或运行测试，预览画面和取消流程尚未实测。编译日志保存在 `.local/artifacts/physics-bake-undo/`；MMN 正式发布日志另存 `.local/artifacts/release-build/`。

<a id="record-bake-preview-and-partial-recording"></a>
## 烘焙预览与部分录入

原始记录：`BAKE_PREVIEW_AND_PARTIAL_RECORDING.md`。其中的版本、日期和验证范围沿用原记录。

2026-10-02。

网格和分层关键帧烘焙在采样完成后呈现已有画面。原生骨骼烘焙绘制私有计算副本，并恢复骨骼、模型槽、帧游标和渲染对象状态；预览不调用播放驱动，也不重复计算 MMV。刷新最多约每秒 15 次，并显示末帧。

通道选择窗口宽度由 380 改为 440 个对话框单位，使右栏及按钮完整显示。

用户选择了部分录入时保留 FX。分层烘焙只要骨骼、表情或帧范围不是完整覆盖，就保留原 FX，写入所选通道。保留 FX 后仍参与运算。完整覆盖仍采用既有整层移除确认和表达能力校验。部分录入仍要求层输出能映射到原模型骨骼。

<a id="record-bake-preview-and-partial-recording-构建与验证"></a>
### 构建与验证

产物与日志位于 `.local/artifacts/bake-preview/20261002/`，不纳入版本控制。修复前版本、工作树和 EXE 哈希已保存。

- 三种语言的隐藏通道选择器检查通过，包括右栏和按钮边界。
- `physics_bake_tests --scene-physics` 通过。两个模型共 10 个模拟步，开启预览选项后关键帧数据和步数一致。测试环境无渲染设备，这项检查不验证 GPU 预览画面。
- 部分录入测试只生成一根骨骼的结果，未选骨骼的根关键帧不在写入集合，准备阶段不修改原关键帧池。
- 当前运行中的 EXE 阻止覆盖正式路径。Release 新版本输出到 `.local/releases/bake-preview/`，部署了 MMV.dll、Data 和许可证目录。
- 未在用户场景中执行完整的 MMV 层部分录入或 GPU 预览验收。未进行渲染性能对照测量。

独立目录用于本次可运行版本；原运行进程和原 EXE 保留。

<a id="record-bake-channel-select-all"></a>
## 烧录通道全选

原始记录：`BAKE_CHANNEL_SELECT_ALL.md`。其中的版本、日期和验证范围沿用原记录。

2026-10-02。

通道选择器新增「全选所有通道」，选择当前页面的全部轨道和全部子参数。模型页覆盖所有骨骼、表情及显示／IK 通道，并关闭「只烧录选中的骨骼」限制。原右栏按钮改为「当前全选」，保留只选择当前通道的操作。

重新选择右栏子参数时同步选中左栏通道。直接取消左栏通道仍有效。取消整个 FX 时的完整覆盖校验保留，提示现在说明如何使用全选按钮；帧范围和其他对象依赖的校验未更改。

<a id="record-bake-channel-select-all-验证"></a>
### 验证

后台产物在 `.local/artifacts/bake-select-all/20261002/`，不纳入版本控制。修复前提交、工作树、构建日志和两侧 EXE 哈希均已记录。

`physics_bake_tests --select-all` 在隐藏对话框中验证两根骨骼、两个表情及显示通道。日文、英文、中文均验证全选全部通道和子参数、重新勾选、直接取消通道、启用骨骼关键帧烧录及关闭所选骨骼限制。相同用例修复前失败 21 项，修复后失败 0 项。

Release 宿主及回归程序构建成功。已有 MSB8028 中间目录警告仍存在。此次未运行用户场景的完整 FX 烧录，也未测量渲染性能。

<a id="record-bake-warning-logic"></a>
## 烘焙警告逻辑修复

原始记录：`BAKE_WARNING_LOGIC_2026-10-01.md`。其中的版本、日期和验证范围沿用原记录。

物理烧录与网格烧录窗口原先只要取得 MMV 层列表，就默认选中第 0 层。层列表必定包含 0，它不代表模型已经赋予 FX。普通烘焙因此进入「永久删除整项 FX」路径，被完整通道、完整帧范围和其他对象检查拦截。加载缺少截层接口的 MMV DLL 也会在开始普通烘焙前被要求更新。

物理烧录现在默认选择「原生」，保存原生骨骼和物理关键帧，保留 MMV 赋予；网格烧录默认选择「范围缓存」，保持已有区间缓存与实例启停轨道行为。两者都必须由用户在「烘焙方式」中主动选择 MMV 层，才进入永久删除该层及更高层 FX 的路径。普通路径不要求截层接口，网格范围缓存仍需要其自身的区间烘焙接口。

显式截层继续检查输出可表示性、通道、全场景范围、其他对象和删除确认。表情覆盖改为检查模型实际存在的全部表情权重通道：无表情模型无需不存在的表情选项，只选第 0 个表情不能代表全部覆盖。逐骨骼列表选中全部骨骼时也可判定完整覆盖；独立的「仅选中骨骼」限制不会被误当成全部骨骼。

<a id="record-bake-warning-logic-前后对照"></a>
### 前后对照

修复前提交 `ef27fd0`，证据目录 `.local/artifacts/bake-warning-logic-20261001/`。目录保留两侧生产源码、EXE、原始构建日志、测试输出、Git 状态和 SHA256。两侧使用同一 Release 配置、同一个夹具 MMV DLL 和最终同一份 `physics_bake_tests --warnings`。夹具补齐宿主已经要求的 ABI 3 `queryInputDemand` 回调，两侧均使用该夹具修正，没有修改生产 MMV。

所有窗口隐藏，不截取用户桌面，不重启用户程序。警告夹具包含两个模型和一个未烘焙附件，选择帧范围 1..2，场景末帧 30；测试中文、英文、日文，以及有/无分层接口两种 MMV。物理在第一次预计算进度回调取消；网格使用无渲染设备夹具验证进入导出准备路径，随后保留引擎自身的设备不足报错。本项验收验证警告门禁，不替代完整 MMV 效果或网格采样验收。

| 验证 | 修复前 | 修复后 |
| --- | --- | --- |
| 同一 `--warnings` 回归 | 42 个断言失败，退出 1 | 全部通过，退出 0 |
| 空效果 MMV，普通物理与范围缓存 | 误进入删除 FX 检查，未进入采样准备 | 不出现删除 FX 或缺少分层接口警告，进入准备路径 |
| 主动选择 MMV 层，部分帧范围 | 仍有警告，但无表情模型误报表情不足 | 保留实际范围不足警告，不要求不存在的表情 |
| 两个表情，只选第 0 个 / 全选 | 第一条被当成全部表情 | 部分表情拒绝；全选后继续检查帧范围 |
| 现有 `physics_bake_tests` 完整入口 | 不作为本次门禁对照运行 | 退出 0，事务、层输出可表示性、动态键池、界面锁和三语对话框通过 |

构建了 `MikuDanceStudio`、`physics_bake_tests`、`alembic_export_tests`、`mmv_fixture`。未重跑真实 MMV 效果、GPU 渲染和完整 ABC 导出场景；不报告播放 FPS 或 GPU 性能数字。

最终程序为 `build/Release/MikuMikuDance.exe`，SHA256 为 `7D0525E79EF6C4F7390EA3281BBB3B0C03C91433C4BC7A9DD017E52C148EA89A`。新增警告回归保留至相关模式和删除检查被替换，且有等效行为覆盖；证据目录不自动清理。

<a id="record-frame-and-bake-fixes"></a>
## 帧操作与网格烧录修复

原始记录：`FRAME_AND_BAKE_FIXES_2026-10-01.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-frame-and-bake-fixes-修复范围"></a>
### 修复范围

- 空帧粘贴：显示帧的首项是模型级父关系，骨骼排序中的索引为 `-1`。原实现按普通骨骼读取名称，会访问骨骼表前的内存。现在直接保留首项父关系，其余项仍按骨骼名称匹配。修复提交为 `1377d1c`。
- 录制返回主界面：录制完成后恢复窗口时，旧输入框的失焦消息会提交帧号 `0`。现在录制保护持续到窗口恢复结束；录制期间不提交失焦输入，普通输入提交也不再制造新的 Enter 按键登记事件。修复提交为 `06e6e8c`。
- 网格烧录窗口：按所在显示器的工作区限制窗口大小，底部执行、关闭、清除按钮固定可见，内容区支持水平和垂直滚动。键盘焦点自动滚动到对应输入框。字号、导出选项和对象选择规则保持原行为。

<a id="record-frame-and-bake-fixes-对照验证"></a>
### 对照验证

使用 `Release` 配置。测试宿主链接同一生产源码，窗口通过进程内钩子保持隐藏。布局图像通过 `WM_PRINT` 绘制到内存位图，并保留子窗口裁剪区域；未使用桌面截图。

| 项目 | 修复前 | 修复后 |
| --- | --- | --- |
| 显示帧粘贴 | 保护页骨骼表夹具复现 `0xC0000005` | 新增、覆盖、插入以及空首项名称均通过；完整骨骼登记回归通过 |
| 录制恢复焦点 | 隐藏真实宿主复现旧输入框将当前帧改为 `0` | 两种结束位置选项均通过真实 `FinishAviRecord` 路径；没有新增骨骼关键帧 |
| 烧录窗口布局 | 821×1025 窗口超出两种模拟工作区 | 800×600 下为 784×584，640×480 下为 624×464；中英日、导出和烧录共 12 种组合均通过 |
| 网格导出 | 原生变形网格往返通过 | 原生变形网格、局部烧录、取消、工程重开往返通过 |

布局检查还覆盖按钮边界与互不重叠、滚动到底部状态区、按钮位置固定，以及键盘焦点显示首项和右下输入框。录制测试覆盖窗口恢复期间的失焦消息和结束状态；未执行实际视频编码。未测量播放 FPS、GPU 时间或编码吞吐率。

<a id="record-frame-and-bake-fixes-构建与证据"></a>
### 构建与证据

正式程序为 `build/Release/MikuMikuDance.exe`，本次生产 EXE 的 SHA256 为 `3B7857B4C03D4CE0A3742EF8DAE310F0AC7DE0AF926359F5C02FD58C4AD3DD60`。构建命令为 `cmake --build build --config Release --target alembic_export_tests bone_registration_tests MikuDanceStudio -- /m /v:minimal`。

原始日志、修复前源码、测试 EXE、哈希及工作树清单分别保留在 `.local/artifacts/empty-frame-paste-20261001/`、`.local/artifacts/recording-focus-20261001/` 和 `.local/artifacts/mesh-bake-layout-20261001/`。空帧与布局基线来源为 `9cb3cb7`，录制基线为 `1377d1c`；这些目录继续保留，不自动清理。其他会话的未提交改动未纳入本次提交。
