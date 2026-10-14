# 可选网格插件宿主与渲染记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [MMV 实现与验证记录](#record-mmv-implementation)
- [原版 MMV 直接渲染接入](#record-mmv-direct-render)
- [无 MME 时的 MMV 启动与最终输出](#record-mmv-without-mme)
- [MMV 独立项目与构建目录](#record-mmv-project-split)
- [MMV 加载 Stage 的 EMM 修复](#record-mmv4mms-emm-fix)
- [MMV 骨骼形变接入记录](#record-mmv-deformation-v3)
- [MMV 分层与骨骼物理验收记录](#record-mmv-layer-validation)
- [Bullet3 主分支集成验收](#record-mmv-bullet3-main-integration)

<a id="record-mmv-implementation"></a>
## MMV 实现与验证记录

原始记录：`MMV_IMPLEMENTATION.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-mmv-implementation-2026-09-27-分层循环接口"></a>
### 分层循环接口

当前集成位置为正式 `main` 工作目录。总接口说明已更新到
[MMV_FX.md](effects.md#record-mmv-fx-原生结构与状态循环)，公共头为 `include/mmv/mmv_deformer.h`。
第 0 层接收完整上游结构，对 MME -1 的 `MmvObjectOutput` 布局保持不变。
下面的早期运行目录与提交号为历史证据，不表示当前生产二进制已被更新。

| 模块 | 已执行的验证 | 证据位置 |
| --- | --- | --- |
| 通用求解实例 | 58 项检查通过：私有候选、历史、固定步、时间轴手动推进、失败丢弃、重置、跨帧版本、独立结构写入合并，以及错误诊断优先于信息提示 | `.local/artifacts/mmv-bone-physics/deformer-diagnostic-tests.log` |
| 分层运行时 | 652 项检查通过：完整结构向下传递、共享域 NaN 回滚、同层写入竞争、截层采样、缓存边界、共享参数重置、编辑 checkpoint，以及运算器策略创建时读取和重置保留 | `.local/artifacts/mmv-bone-physics/global-runtime-01.log` |
| 真实 Bullet3 与运行时 | CPU 直接输出 866 项、DX12 分层路径 173 项检查通过；骨骼赋予、不同 FX 独立重力与共享碰撞、主实例更换、移动物理层、下层读取已解算骨骼、原生物理切换、宿主姿态不回写，以及 DLL 仅位于 FX 文件夹 | `.local/artifacts/mmv-bone-physics/fx-relative-cpu-01.log`、`fx-relative-gpu-01.log` |
| MMV 处理设备 | DX12 161 项、DX9 兼容后端 164 项检查通过，覆盖精确显卡身份、WARP/REF、错误选择拒绝和资源恢复；既有 DX12 回归 370 项通过 | `.local/artifacts/mmv-global-processor/*tests.fix3.log` |
| 生产 DLL 的 CPU 设置 | 经公开宿主回调选择 CPU，纯 DX12 版 91 项、兼容版 89 项检查通过；包含计算、拓扑、DX9 浮点纹理到 DX12 网格、设备恢复和回调只查询一次 | `.local/artifacts/mmv-bone-physics/global-plugin-dx12-cpu.log`、`global-plugin-compat-cpu.log` |
| 结构快照 | 21 个场景、123 项检查通过：稳定身份、引用、形状、权重、惯量、joint 锚点和拓扑版本 | `.local/artifacts/mmv-bone-physics/structure-topology-snapshot.log` |
| D3D12 结构输入 | 真机 370 项检查通过；t32..t40 数据、空输入、缺失蒙皮前数据、旧前缀、非法数据及图形 pass 回读；此前 368 项版本另通过调试层检查 | `.local/artifacts/mmv-bone-physics/optional-deformed-gpu.log`；调试层记录见 `.local/artifacts/mmv-deformer-gpu/run-004-final/stdout.log` |
| 正式 DLL 接口 | DX12 版 89 项、兼容版 87 项检查通过；新增导出、快照租约、截层采样、0 层与最终网格 ABI 一致、旧宿主前缀和 DX9 纹理交接 | `.local/artifacts/mmv-bone-physics/plugin-abi-dx12-04.log`、`plugin-abi-compat-01.log` |
| 分层界面与结构查看 | 隐藏窗口 26 个场景、4790 项检查通过；包含只读快照、稳定键、三语、草稿守卫、应用／关闭回退，以及层号旁的描绘顺序入口；参数窗口另通过 9228 项检查 | `.local/artifacts/mmv-deformer-editor/mmv-editor-tests-stdout.txt`、`mmv-parameter-editor-tests-stdout.txt`；同目录 report.md |
| 分层 VEM 集合 | 27 项通过、1 项平台条件跳过；包含提交失败恢复 | `.local/worktrees/mmv-bone-physics/.local/artifacts/vem-layer-storage/` |
| 宿主分层烘焙 | 隐藏宿主回归通过：非法层表拒绝、OUTPUT 与 SOURCE 区分、采样租约、移除事务、插件重载、网格导出安装回放及失败回滚；物理与网格缓存单元回归通过 | `.local/artifacts/mmv-layer-bake/run-mmv_host_tests-layers.log`、`run-physics_bake_tests.log`、`run-mesh_bake_cache_tests.log` |

通用接口使用确定性测试 DLL，不以此声称 AVBD、DDM、SDS、VBD 算法通过验证。
真实 Bullet3 已通过上述实际 DLL 与运行时组合验证，候选生产版本性能对照仍未完成；
尚未取得的生产 CPU/GPU/FPS 指标不从独立测试推算。

宿主 PMX 快照按权重类型导出有效槽。BDEF2 与 SDEF 的第二权重取 `1-w`，避免将文件中未存储的槽按零导出，导致 Rou 顶点 408 的严格权重校验失败；不修改原始模型权重或放宽校验。

截层烘焙的当前限制会明确报错：删除整项 FX 前要求覆盖完整动画范围；网格缓存不支持部分对象顶点烘焙，也不能丢弃其格式无法保存的材质、附加 UV、描边和可见性变化。正层缓存还要求其结构与姿态可由后续层继续读取。骨骼关键帧烘焙拒绝无法映射到原模型的新增骨骼、绑定改变和额外网格变形。隐藏回归使用确定性测试插件，不能代替真实效果在完整宿主中的验收。

<a id="record-mmv-implementation-历史集成记录"></a>
### 历史集成记录

当前开发工作树为 `.local/worktrees/mmv-native`，分支 `feat/mmv-native`。
正式主工作区截至 `d1353ec` 的五档排序等改动已合入本分支。插件尚在实现，测试用的
`mmv_fixture` 是独立 CPU 夹具，不是最终的 DX12 发布 DLL。

菜单与分配窗口这批代码已快进合入 `main`。正式运行文件从 `eb10743` 构建：
`build/Release/MikuMikuDance.exe`，该目录已安装兼容版 `MMV.dll` 和 `MMV/`
资源目录。纯版单独保存在 `build/mmv/dx12/Release`。源码状态、编译日志、
导入导出表与 SHA256 在 `.local/artifacts/mmv-native/main-integration/manifest.json`
及同目录文件中；完整运行快照在 `.local/releases/mmv-native/menu-ui-eb10743/`。
正式宿主仍为 37 个导出；纯版没有 DX9/D3DX9 导入，延迟导入目录为空。
这是菜单及当前模块的可用集成构建，不代表下文未完成事项已经验收。

<a id="record-mmv-implementation-宿主交接"></a>
### 宿主交接

`include/mmv/mmv.h` 定义不含 DirectX、Bullet 和 STL 类型的 C ABI。
`src/mmv_host/host.cpp` 在蒙皮之后、主设备 BeginScene 之前获取快照，
将插件结果复制到独立渲染记录。模型和附件的阴影、描边、普通绘制、
透明排序及 MME 查询都经过该视图。宿主交接代码以 `MMV_HOST_MME_BRIDGE`
标识，不能当作插件的旧 FX 兼容模块移除。

模型通过原有 CreateIndexBuffer 的稳定输出槽登记；附件通过原有
D3DXLoadMeshFromXW 输出槽登记。实测 MMHack 在后续 BeginScene 仍检查
加载时的附件网格身份，因此宿主保留登记资源，实际绘制统一从
AccessoryMesh 获取本帧生成网格。登记资源由对象代理持有，随对象删除释放。
不读取或修改 MMHack/MMEffect 的私有数据，也不调用未知签名的导出。

新增材质名使用仓库已有的 GetMaterialName 导入适配，原始材质仍走原查询。
37 个宿主导出的定义文件、签名和序号未改动。PMD 的 CPU 索引表是
每项占两个 WORD 的布局，不能按紧密 WORD 数组读取。

<a id="record-mmv-implementation-已运行的验证"></a>
### 已运行的验证

MSVC v143、Win64 Release，Windows SDK 10.0.26100.0。

- `mmv_storage_tests`：UTF-8、中文路径、重复实例、相对路径、显式移除与禁用、
  未知字段、事务失败和扩展名限制均通过。
- `effect_api_tests`：已有菜单、语言、对象 ID、登记顺序和退出测试通过。
- `mmv_host_tests` 配合独立 `mmv_fixture`：隐藏窗口运行四种 MMV/MME 安装组合，
  验证 PMD 与附件登记、源资源及 CPU 拓扑未改写，渲染目标回读成功。
- 追加材质夹具：PMD、PMX 和附件均通过 MME 材质数量、Unicode 名称、
  效果分配保存和绿色 shader 实际绘制验证。

原始日志和 BGRA 回读位于 `.local/artifacts/mmv-native/bridge-01/`。
保留了失败样本：属性缓冲接口误用导致的崩溃、PMD 索引步长错误、
附件网格替换造成登记失败，以及测试夹具未排空 MME 保存队列造成的竞争。
这些问题已在后续运行中修复。后续验证见下文；设备恢复、GPU 性能与双版
发布的整体验收尚未完成。

后续 `bridge-02` 验证了恒等输出的主顶点和描边缓冲逐字节一致；两种 MME
安装状态下的画面仍各有 566 个通道不同，最大差值 128，尚未作为像素恒等通过。
`grow` 与 `grow-02` 暴露了 MME 材质缓存不随原记录扩容更新的问题。
`grow-03` 通过模型和附件的结构重新登记恢复了新材质分配，但 `grow-04`
发现旧材质的分配会丢失。`grow-05` 使用已有 PMM 文件通知将 MME 分配写入
新建的临时 EMM，在重新登记后恢复，旧分配与新增材质的实际着色、保存均通过。
这一处理只在材质结构变化时运行，不改写用户现有的 EMM/EMD。

当前还完成了以下独立模块验证：

- FX 前端通过 599 项检查，包含条件编译、同名 include 的独立路径和准确行号。
- 运行时通过 74 项配置测试，覆盖事务失败、循环依赖、重复实例、工程目录移动、
  `.vem` 覆盖与纯版保留传统 FX。该测试明确使用 GPU 测试替身，不代表 GPU 执行。
- 8 个随插件发布的原生示例通过真实 HLSL 编译。
- DSH 实现的独立 DX12 后端经主 agent 用 CMake 重新构建，通过 231 项真实 GPU
  检查，包括源顶点数量与输出拓扑分离、骨骼权重、刚体/joint、材质、纹理、
  输出校验、设备重建及 GPU 时间戳。调试层测试无错误。

DX12 初轮测试失败样本保留在开发工作树
`.local/artifacts/mmv-native/dx12/run-1`。上传暂存资源曾在命令提交前释放，
调试层报告 `OBJECT_DELETED_WHILE_STILL_IN_USE`；修复为 fence 完成后释放。
主 agent 的独立 GPU 运行目录为主仓库 `.local/artifacts/mmv-native/dx12-main`。
`bridge-03` 定位并修复了附件材质转换丢失自发光的问题。四种组合均通过，
`plain/frame.bgra` 与 `mmv/frame.bgra`、`mme/frame.bgra` 与 `both/frame.bgra`
分别逐字节相同。`grow-06` 验证了重新登记并恢复 MME 分配后释放旧登记资源。
新增纹理的透明分类、toon 颜色查询、替换后的资源回收也已通过实际宿主测试。
PMX 新增材质现在有独立的材质表情数组，避免按新增编号访问源模型数组之外。

顶部 MMV 菜单位于 MME 左侧，同属右侧菜单组。宿主测试通过实际菜单
矩形验证位置，并逐次运行宿主日文、英文、中文切换，检查标签和重复注册。
分配窗口已链接进两版正式 DLL；主构建独立运行 1557 项隐藏窗口检查，
包括三语即时切换、最小尺寸、按钮互不重叠、控制器范围和关闭后不重开。
窗口自身的 WM_PRINT 确认图在 `editor-main-03`，不使用桌面截图。

DX9 兼容模块已通过独立系统设备上的 53 项回读检查。主 agent 用 CMake
重新构建并复跑通过，目录为 `.local/artifacts/mmv-native/compat-main-01`。
基础变换、材质、时间、控制物、纹理、命名目标和脚本已实现；鼠标语义、
部分材质表情语义、准确光矩阵及动画纹理尚需补齐，不代表完整 MME 兼容。
当前实现中这些缺口的默认值和近似值仍需修正后才能通过整体验收。

透视模块已合入。主 agent 复跑了 97 项配置测试及 21195 项真实 GPU 检查，
原生纹理预览有 59511 个像素变化，记录位于 `runtime-05` 与
`perspective-main-01`。这些结果尚未代替宿主 DX9/MME 的两版正式 DLL 验证。

宿主计时分别记录快照 CPU、完整求值 CPU、DX12 计算 GPU、回读 CPU、上传 CPU
和 DX9 绘制 GPU 区间。DX9 使用时间戳、频率和不连续标志查询，依据
[Direct3D 9 查询文档](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries)。
绘制区间可能包含提交间隙，不能解释成 GPU 忙碌时间；无效查询单独标记。
计时默认关闭，测试按需开启。`bridge-03/both/pmx-extra.timing.csv` 保留了冷启动
与随后三帧的原始结果，尚未用于性能结论或播放 FPS 推算。

两版已完成正式 DLL 构建。`plugin-main-01/dx12` 的 60 项检查通过；
兼容版初次集成发现普通 FX 缺省 Script 未执行 pass，以及未设置选中技术。
修复后独立 DX9 检查增加至 67 项，`plugin-main-03/compat` 的 58 项检查通过，
包含传统纹理驱动同帧 DX12 网格。纯版导入与项目链接审计没有 DX9/D3DX9，
隔离进程中加载旧 FX 配置、设备恢复和重开工程也没有加载旧后端。

`host-production-02` 使用正式纯版 DLL 通过 PMD 恒等输出、PMX 与附件新增材质、
MME 查询与实际着色、分配保存；`host-production-03` 验证合入当前主分支后的
兼容版宿主。两者均真实点击 MMV 菜单命令并验证已打开窗口随宿主即时切换三语。

可见性更正：此前宿主测试虽传入 SW_HIDE，但初始化中的 SetWindowPlacement
仍强制最大化，不能把那些运行标为全程隐藏。`af267c2` 修复宿主忽略 SW_HIDE
的问题，并增加初始化后窗口不可见的断言。`host-production-01` 保留发现此问题
的失败结果；`host-production-02` 及以后的宿主运行通过该断言。隐藏启动没有
首个窗口绘制，测试因此显式绘制空场景，等待 MME 首次 BeginScene 注册菜单。
既有原始像素和计时文件保留，不改写历史结果，也不将旧计时混入隐藏运行对照。

完整兼容语义、所有失败恢复场景和完整性能对照仍未验收，不代表整个首版计划完成。

`compat-main-03` 通过 84 项真实 DX9 检查，包括跨节点负数、HDR、小数和
previous 累加的 RGBA32F 回读。旧 FX 内部 RENDERCOLORTARGET 仍保留手册的
8 位默认值，显式 Format 才改用浮点；MMV 节点边界保持浮点。
`host-production-03/compat/legacy-04` 验证主设备有 MME 时，独立 DX9
纹理仍可驱动当帧 DX12 网格，模型主顶点缓冲的预期位移为 0.025。
该目录 `legacy` 至 `legacy-03` 保留测试 FX 将注解结束与赋值写成 `>=`
导致的编译失败；`legacy-04` 修正测试语法后通过，不计入产品失败。
宿主交接错误现在也进入现有可选追踪日志。

DSH 浮点纹理子任务在明确禁止写记忆的任务文本之外仍调用了其自身记忆工具。
代码交付已独立复核与复跑；未将其记忆作为工程事实，也未修改或清理这些记录。

修复前宿主和测试宿主保存在 `.local/releases/mmv-native/before/`，
哈希、提交和构建配置在其 `manifest.json` 中。

<a id="record-mmv-implementation-公开接口依据"></a>
### 公开接口依据

ID3DXBaseMesh / ID3DXMesh 的方法声明及 D3DXMESH_MANAGED 常量可在
[公开头文件](https://raw.githubusercontent.com/wine-mirror/wine/master/include/d3dx9mesh.h)
中核对。属性缓冲 Lock/Unlock 对应接口槽 24/25，托管资源选项为 0x220。
产品调用统一使用这些公开 COM 接口的类型签名。

<a id="record-mmv-direct-render"></a>
## 原版 MMV 直接渲染接入

原始记录：`MMV_DIRECT_RENDER.md`。其中的版本、日期和验证范围沿用原记录。

新插件源码在同级 `MMV` 独立仓库，公共头为 `MMV/include/mmv/render.h`。MMS 只编译宿主桥，不编译插件；可选安装的产物名为 `MMV.dll`。

`frame_scene.cpp` 在宿主默认绘制前调用 `RenderTakeover`。返回 1 后跳过默认模型顶点上传、模型/阴影绘制和场景合成；随后 `PresentRenderOutput` 将插件颜色纹理提交到宿主目标，继续原有的录制捕获与编辑控件路径。返回 0 表示未安装或已禁用插件，继续原生渲染；返回 -1 表示已接管的渲染失败，本帧停止，不再执行第二套场景绘制。

桥位于 `src/mmv_host/renderer_bridge.inl`。模型顶点在 CPU 副本上采样当前形变和蒙皮，不更新宿主顶点缓冲；配件读取现有网格。相机、材质、共享 toon 与投影阴影配置只读传入。插件使用宿主 D3D9 设备，桥保存并恢复状态、渲染目标、深度表面和 viewport。颜色纹理由宿主 AddRef 保留到提交结束。

打开 PMM 时保存路径，插件自动读取同名 EMM。新入口不执行旧 `Evaluate/Upload` 输出，也不进行 MME 分配快照同步。旧桥辅助代码暂留供分阶段迁移，但没有直接渲染入口的旧 DLL 已被拒绝。

<a id="record-mmv-direct-render-验证记录"></a>
### 验证记录

- Release x64 插件与生产 MMS 均编译通过。
- 独立隐藏窗口测试覆盖默认颜色、透明混合、自阴影执行、EMM 隐藏、后处理最终像素、输入不变和设备资源重置。
- `AlternativeFull/00_test_material.pmx` 的 PMM 在 640×480 后台验证中，显示与隐藏分别接受 3/3 帧，失败为 0，默认蒙皮更新计数为 0。
- `scene_lifecycle_tests --record-frames` 连续五帧录制计数和相机关键帧采样通过；这项测试没有写 AVI 文件，不能代替完整编码验收。
- `[MMV] Enabled=0` 启动时未加载插件，默认蒙皮更新恢复。

结果存于 `.local/artifacts/mmv-direct-render/verified/`。`initial/`、`final/` 留有中间结果，其中黑色默认材质和共享 toon 格式拒绝属于已修复的失败；不得用于性能结论。新仓库 README 记录未迁移的显式离屏 EMM、Compute FX、QDEF、宿主深度交接与 DX12 等范围。

未完成对应两侧生产源码的前后画质与性能验收；以上是首阶段的功能验证，不宣称完整替代所有 MME 效果。

<a id="record-mmv-without-mme"></a>
## 无 MME 时的 MMV 启动与最终输出

原始记录：`MMV_WITHOUT_MME.md`。其中的版本、日期和验证范围沿用原记录。

未加载 `MMEffect.dll` 时，MMV 仍正常启动。正层逐层传到 0 层，0 层输出网格交给 MMS 原生渲染；有 MME 时，0 层到 -1 层的既有交接保持不变。无需额外开关。

<a id="record-mmv-without-mme-代码核对与修改"></a>
### 代码核对与修改

宿主初始化只要求可选的 `MMV.dll`。`mmv_host::Evaluate` 在模型和附件绘制之前发布输出，固定渲染、内置效果、自阴影及附件路径通过 `RenderModel` / `RenderAccessory` 取得代理网格。

本次生产修改仅在烘焙缓存恢复分支为 `DrainEffectRegistration` 加上 MME 存在检查。无 MME 时没有待登记的 MME 效果，不再回放 MME 专用登记帧。有 MME 时仍执行原调用。

MMV 菜单的中英日切换已通过测试。虽然 `RefreshEffectMenuLanguage` 在无 MME 时早退，外层 `RefreshMenuLanguage` 仍会独立刷新 MMV；这不是缺陷。

<a id="record-mmv-without-mme-隐藏窗口验收"></a>
### 隐藏窗口验收

Release `mmv_host_tests` 的 `final` 模式使用真实宿主帧路径和夹具插件，运行目录为 `.local/releases/mmv-no-mme-validation/`，其中不含 MME 及其 D3D9 代理。

以下检查全部通过：

- `MME=0 MMV=1`；三种语言各只有一个 MMV 菜单。
- 固定、自阴影和设备重建后三组恒等输出，各自连续两次回读逐像素一致。
- 模型、附件均发布代理；模型代理与着色前捕获均包含新增几何。
- 固定渲染与内置自阴影渲染中，0 层新增几何均改变超过 100 个最终画面像素。
- 设备重建后，新增几何相对同一次重建后的恒等基准仍改变超过 100 个像素；不使用重建前画面作基准。
- 每次回读先断言实际帧目标与读取目标相同。

最终日志：`.local/artifacts/global-processor/mmv-final-integration-stable-reset.log`。同目录的 `mmv-final.*.bgra` 保存各阶段回读；`mmv-final-validation.json` 保存像素差计数与二进制哈希。

<a id="record-mmv-without-mme-失败记录与验证边界"></a>
### 失败记录与验证边界

早期失败日志保留。固定路径的标记三角形被 PMD 背面剔除，夹具现提供正反绕序；PMX 双面标志不能控制 PMD 固定路径。模型编辑视图使用 `ViewOffsetX/Y`，相机面板使用另一组取景状态，测试分别设置。

自阴影绘制结束会恢复渲染器保存的场景表面，初版测试仅替换设备当前表面，因而读回了错误目标。现在测试临时接入正确场景目标并在结束后还原，未改生产渲染目标逻辑。原始阴影贴图预览开关保持关闭，正常自阴影着色仍开启。

本轮验收覆盖宿主与夹具插件的 0 层交接，不替代真实多层 FX 的整套场景验收。新增 MME 登记门禁尚未单独完成烘焙缓存回放的前后 build 对照，该分支保留为待验收项。有 MME 的交接未在本轮重跑。

<a id="record-mmv-project-split"></a>
## MMV 独立项目与构建目录

原始记录：`MMV_PROJECT_SPLIT_2026-09-28.md`。其中的版本、日期和验证范围沿用原记录。

用户已确认拆出 MMV 本体源码，并要求每个项目在自身 `build/` 下构建。

| 项目 | 源码目录 | 构建目录 |
| --- | --- | --- |
| MMV | `D:/MMD-Next/MMV` | `D:/MMD-Next/MMV/build` |
| MMV-Bullet3 | `D:/MMD-Next/MMV-Bullet3` | `D:/MMD-Next/MMV-Bullet3/build` |
| MMS-MME | `D:/MMD-Next/MikuDanceStudio-MME` | `D:/MMD-Next/MikuDanceStudio-MME/build` |

拆分时，三个项目的 `.gitignore` 均忽略 `build/`。2026-10-11 清理后，MMS 本机改用
`.git/info/exclude` 保留忽略规则。MMV 和 Bullet3 各有独立
`scripts/build.ps1` 与 CMake preset；本宿主不再提供 `mmv_all` 等本体构建目标。
宿主默认从 `../MMV/include` 读取公开头，可通过 `MMV_INCLUDE_DIR` 指定 SDK 位置。
生成运行包时，选择独立 MMV 构建的一版 DLL 及资源复制到宿主 EXE 旁。

本体源码、资源及独立测试已迁出宿主；CMake 宿主接入在 `cmake/mmv_host.cmake`。
现有并行任务的未提交 MMV 实现已保存在新仓库及原始来源快照中。
宿主导出修复及其他宿主修改仍由对应会话维护。

前后 FX 和三版生产 DLL 回归通过。编辑器套件前后均有相同的三个既有失败，
已保留日志并交回页签任务。详细范围、来源和原始证据见
[MMV 迁移记录](../../../../MMV/docs/PROJECT_SPLIT_2026-09-28.md)。

旧 `build/mmv/` 只保留为历史基线，不再作为 MMV 编译入口。
用户随后清理了 `.local`，原始归档与前后日志已不可作为现存文件引用。
源码迁移保存在三个项目的本地提交中；最终清单位于
`../MMV/build/verification/project-split-20260928/manifest.json`，
保存最终版本、构建路径和二进制哈希。

<a id="record-mmv4mms-emm-fix"></a>
## MMV 加载 Stage 的 EMM 修复

原始记录：`MMV4MMS_EMM_FIX_20261009.md`。其中的版本、日期和验证范围沿用原记录。

宿主现在向 MMV 提供已加载但隐藏的模型，并通过独立的可见性字段控制绘制。此前 `ray_controller.pmx` 被过滤，MMV 导入 `Stage.emm` 时找不到该控制器。修复只涉及 `src/mmv4mms/renderer.inl`，不更改宿主骨骼、morph 或物理数据。

配套的 MMV4MMS 与 MMV4MMN 修复了 EMM 解析、效果发现、映射容量、权重副本、Ray 脚本状态及多后处理串联。详细记录见 [插件修复记录](../../../../MMV4MMS/docs/EMM_IMPORT_REPAIR_20261009.md)。

<a id="record-mmv4mms-emm-fix-验证记录"></a>
### 验证记录

用户指定输入为 `D:/MMD-Next/tests/ToonGlass_Ray/UserFile/Stage.pmm`。工程副本与原生 MME 在后台对照，结果保存在 `.local/artifacts/emm-import-20261009/`：

| 目录 | 结果 |
| --- | --- |
| `before` | 原插件复现权重和 `1.001076` 导致整帧被拒绝。 |
| `after` | 初步导入修复复现隐藏 `ray_controller` 缺失。 |
| `native` | 原生 MME 完成加载及截图。 |
| `after-2` | 19 个对象、328 条映射导入成功，但画面为黑色轮廓。 |
| `after-3` | Ray 着色恢复，后续效果仍覆盖为黑色。 |
| `after-4` | 完整后处理链恢复人物、场景颜色和景深。含临时逐阶段贴图诊断。 |
| `after-final` | 当前生产核心库与无诊断代码的正式 DLL 再次完成导入、VMM 保存和绘制。 |

`comparison-manifest.json` 记录源工程、比较程序、DLL、日志与截图的 SHA-256。诊断宿主基于已构建的生产核心库，宿主修复通过独立编译桥接对象接入；不混入当时其他会话的未提交源码。正式宿主随后由当前生产源码独立构建。

原始文件 SHA-256：

- `Stage.pmm`：`44041f9a1b88d2d2f10202e089a3b036a83cd0872e0cf1c83221d112de2986ba`
- `Stage.emm`：`a8231a3651aa6f0ac592ce1ae7a157bc32a120c239a1671ce55fc7d061ee0f67`

与原生 MME 的云朵边缘采样仍有差异。本轮没有宣称像素一致，也没有测播放 FPS 或 GPU 性能。所有窗口保持隐藏，未重启用户程序。原始工程及素材保持不变。

旧 EXE 与 DLL 保存在 `.local/archives/emm-import-20261009/`，MMN 的旧插件保存在 MMN 同名归档目录。不清理这些归档。

<a id="record-mmv-deformation-v3"></a>
## MMV 骨骼形变接入记录

原始记录：`MMV_DEFORMATION_V3.md`。其中的版本、日期和验证范围沿用原记录。

原版 MMS 向 MMV 提供只读场景快照和形变输入服务。MMV 执行 Compute 网格效果或原生非物理形变，按骨骼顶点组权重混合私有网格，再完成阴影、模型、透明和后处理绘制。MMS 接收最终渲染目标，保留编辑器、操作控件及显示和录制入口。

<a id="record-mmv-deformation-v3-接口与资源"></a>
### 接口与资源

`MMV_RENDER_ABI` 为 `3`。EXE 和 `MMV.dll` 必须配套更新。插件仍可选，不带 Modular 后缀，不与 MME 共存。

宿主增加原始顶点、刚体和 Joint 的快照需求，并提供只读 `physicsInput`。MMV 不写回输入网格、骨骼或约束。当前 DX12 网格输出回读到 MMV 私有内存，经权重混合后交给 D3D9 绘制；该路径尚未消除 GPU 回读。

骨骼分配采用 `[MMVBoneEffects]`，骨骼名称采用 `[MMVBones]`，参数采用 `[MMVBoneParameters@...]`。骨骼索引与名称共同校验身份。刚体按骨骼读取，Joint 保留两端和约束依赖。执行器当前接收完整场景数据，是依赖集合的保守超集。

运行包位于 `.local/releases/mmv-deformation-v3/`，包含配套 EXE、MMV DLL、FX 头文件、权重位移示例和原生 DDM。来源、工作树状态、构建参数和 SHA256 记录于包内 `source.json`。测试夹具 DLL 不随包发布。[分配步骤](../../../../MMV/docs/DEFORMATION_V3_USAGE.md)说明参数编辑和 EMM 保存方法。

<a id="record-mmv-deformation-v3-已验证的行为"></a>
### 已验证的行为

最终后台 CTest 的 3 项测试全部通过：渲染、连续顶点组权重、物理依赖读取。渲染测试包含原生 DDM、跨骨骼 Joint 两端读取、参数保存重载、材质形变继承对象绘制参数、失败候选回滚，以及输入快照保持不变。

真实 PMM 为 1 个模型、134 个顶点。Compute 和 DDM 两组分别接受 3/3 个采样帧，无失败，MMS 默认模型 CPU 与 GPU 上传计数均为 0。该 PMM 不含刚体或 Joint，约束读取由独立 CPU 和 GPU 用例验证。

原始记录保留于 `.local/artifacts/mmv-deformation-v3/`。`final/` 使用最终 DLL，其中 `compute.json`、`ddm.json` 和对应日志保留真实宿主结果。`final-ctest.log` 保留回归输出。先前记录与运行文件保留，不覆盖。

<a id="record-mmv-deformation-v3-未完成的验收与能力"></a>
### 未完成的验收与能力

当前仅完成上述正确性检查。生产源码的修复前后配对构建、交替采样和性能对照尚未完成，不能据此认定性能提升。MMS 构建包含其他会话尚未提交的改动，具体列表保留于 `source.json`。

原生物理求解器的动画前接管尚未接入。声明 `MMV_DEFORMER_NATIVE_PHYSICS` 的插件被拒绝，宿主物理继续运行。Bullet3、AVBD、SDS、VBD 的接管不属于本次完成范围。

纹理与跨层 Compute 图、原生 `.x` 全局控制器分配、复杂多组重叠场景和完整 MME 兼容仍待接入或验收。本轮未重新验证 AVI 录制。

<a id="record-mmv-layer-validation"></a>
## MMV 分层与骨骼物理验收记录

原始记录：`MMV_LAYER_VALIDATION_2026-09-27.md`。其中的版本、日期和验证范围沿用原记录。

本记录尚未完成最终验收。目标为内部 `main`；当前实现位于 `.local/worktrees/mmv-bone-physics`，不得把工作树中的编译结果当作已部署的 MMV DLL。

<a id="record-mmv-layer-validation-功能合同"></a>
### 功能合同

- 内置 MMV 层为 0，单个内置分配页可选择骨骼或材质；最终 MME 层为 -1。
- 正层从大到小执行，同层附件读取同一上游快照，靠后效果覆盖靠前效果，倒序跳过已覆盖范围；层完成后向下一层传递合并输出。
- Bullet3 骨骼分组共用碰撞世界，分别设置重力、地面碰撞和编辑模拟；共享频率和迭代次数取最大值。
- `.x` 通过显式模板声明默认 FX 和控制附件状态，不声明层号。
- `.vem0` 是整套对象预设的入口，`.vemN` 保存第 N 层；VMM 保存全部层及展开设置。
- 展开许可、开合状态、展开方式按层持久化。缺失附件、目标、骨骼或材质时拒绝预设，不重映射。

设计和操作说明分别见 [分配范围](interface.md#record-mmv-assignment-scope-design)、页面与附件（`docs/MMV_PAGES_AND_ACCESSORIES.md`，历史引用文件不在当前仓库中）、[存档](usage.md#record-mmv-storage)。

<a id="record-mmv-layer-validation-已取得的证据"></a>
### 已取得的证据

| 范围 | 实际验证 | 限制 |
| --- | --- | --- |
| `.x` 声明解析 | 系统 D3DXFile 的文本、二进制、压缩和错误输入，共 278 项检查通过 | 不能替代原附件加载器的几何加载验证 |
| Bullet3 适配器 | 992 项检查通过，包括分组规则、碰撞与求解后姿态读取 | 不是用户模型的最终宿主验收 |
| 单文件存储 | 15 项测试通过，官方 CMake 构建及独立编译均执行 | 当时尚未实现整套 VEM 提交 |
| 分层运行时 | 编译检查通过 | 编译不能替代链接及运行测试；后续改动待联调 |

解析、适配器与运行时日志在正式目录的 `.local/artifacts/mmv-bone-physics/`。存储模块原始日志在工作树 `.local/artifacts/vem-layer-storage/`。最初独立存储编译漏掉 `/utf-8`，失败日志保留；补齐与正式构建一致的参数后通过。

<a id="record-mmv-layer-validation-前后版本对照"></a>
### 前后版本对照

基线为 `7f28800a275037da7436dcd60554d94405c44611` 的生产宿主和 MMV DLL。基线工作树只替换了验证驱动，使两侧使用同一测试逻辑。版本、工作树状态、构建参数及二进制哈希记录在 `.local/artifacts/mmv-bone-physics/comparison-manifest.json`。

统一条件为 Rou PMX、640×480 渲染目标、60 个固定时间步、关闭编辑时模拟、相同物理参数。宿主保持隐藏，画面从渲染目标回读。记录 CPU 帧耗时和有效的绘制 GPU 时间，不推算播放 FPS。

`comparison/before-01` 已通过基线正确性检查：暂停位置差为 0；CPU 帧耗时中位数 166.773 ms，P95 为 182.093 ms。候选版本及交替样本尚未完成，因此这些数字不构成性能结论。

探索样本 `host-before-01` 混用了候选宿主和基线 DLL，已排除，不参与最终比较；原始文件保留。

<a id="record-mmv-layer-validation-待完成"></a>
### 待完成

- 整套 VEM 的往返、损坏文件、过期层清理和提交失败回滚。
- 单页签、层切换、展开门槛及保存入口的隐藏窗口测试。
- 实际 DX12 的上层物理与下层网格、骨骼快照读取。
- 带 MMV 声明的 `.x` 在生产宿主中加载几何并自动分配效果。
- 候选 DLL、真实 Rou 模型、交替前后采样及合入 `main` 后的验证。

<a id="record-mmv-bullet3-main-integration"></a>
## Bullet3 主分支集成验收

原始记录：`MMV_BULLET3_MAIN_INTEGRATION.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-mmv-bullet3-main-integration-合并内容"></a>
### 合并内容

集成提交 `b2b5e79` 合并 `main` 的 `f41a4d0` 与 `feat/mmv-bullet3` 的 `f3330a5`。共同基线为 `8dfa7103`，合并前两侧分别有 12 个和 3 个独有提交，可用 `git rev-list --left-right --count f41a4d0...f3330a5` 复核。

三处冲突分别保留两侧功能：

- `src/mmv/runtime.cpp` 的场景复位先取消网格烧录事务，再恢复原生物理模式并复位 Bullet3。
- `src/mmv_host/host.cpp` 保留没有插件时播放网格缓存的入口，同时清理每帧物理快照并记录原生物理状态。
- `docs/records/development/workspace.md` 保留两侧已有归档记录。

计算 DLL、适配器、FX 和 UIFX 仍属于独立仓库 `D:/MMD-Next/MMV-Bullet3`，本轮构建使用该仓库的 `711a293`。上游 Bullet 源码仍只读引用 `D:/MMD-Next/refs/bullet3-master`。

<a id="record-mmv-bullet3-main-integration-构建与正确性"></a>
### 构建与正确性

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

<a id="record-mmv-bullet3-main-integration-同条件对照"></a>
### 同条件对照

原 Bullet3 分支 `f3330a5` 与集成版本使用同一 Rou 模型、FX、测试驱动、640×480 渲染目标及 60 帧固定时间步，按前后交替顺序运行两轮。关闭编辑时模拟以保持重复帧冻结，双方均开启 Bullet3。四次运行的最大顶点变化均为 1.015368，初始状态和原生物理恢复检查均通过。

四张第 60 帧 PNG 的 SHA256 相同：`DB6B8610BCB5E39FA9B542EDFBDB8C512B480D7609FC99CF6DDE616A69EEEC31`。图像来自实际渲染目标回读。

| 轮次 | 原分支 CPU 中位耗时 | 集成版 CPU 中位耗时 |
| --- | --- | --- |
| 1 | 136.673 ms | 139.760 ms |
| 2 | 137.915 ms | 147.106 ms |

这些值是隐藏窗口固定帧测试的 CPU 墙钟时间，包含同步等待，不能换算成已测播放 FPS。集成版两轮有波动，不据此宣称性能提升。原始样本全部保留。

<a id="record-mmv-bullet3-main-integration-证据与边界"></a>
### 证据与边界

证据已迁移至主目录 `.local/artifacts/mmv-bullet3-retired/mmv-bullet3-main/`：`comparison-manifest.json` 记录两侧 EXE/DLL、素材、测试驱动哈希和构建参数；`summary.json` 汇总对照；`ab/`、`slowplay/`、`editing/` 保留 CSV、PNG 和日志；`runtime-review.md` 与 `dsh-result.json` 保存独立复核。`feature-before/` 保留原分支的测试运行文件。

用户提供的 `MMEffect.dll+0x2655` 空指针截图与主目录 `.local/artifacts/mmv-curve/ui-probe/acc-open7/run.log` 完全匹配，线程号为 15904。该测试使用 `.local/releases/mmv-curve-before-2e684bf/host/MikuMikuDance.exe`，发生在打开附件参数窗口时，尚未接入 Bullet3。该崩溃已交由曲线编辑任务继续处理，不计为本轮 Bullet3 回归通过的范围。

本轮没有验证 Bullet3 视频导出、原生 VMD 物理烧录或所有第三方 MME 效果组合。Bullet3 仍只改变 MMV 输出网格，接口和使用边界见 [MMV Bullet3](physics.md#record-mmv-bullet3)。

<a id="record-mmv-bullet3-main-integration-同步最新-main"></a>
### 同步最新 main

`7d783ca` 继续合并 `a017903`，保留曲线编辑、VEM 静态预设、三语菜单以及参数窗口修复。`main` 随后快进至该集成提交，`git merge-base --is-ancestor feat/mmv-bullet3 main` 返回成功。并行任务未提交的文档保持原样，没有暂存、回退或提交这些修改。

同步后再次构建宿主与两版 MMV，运行时回归增加至 240 项并全部通过。Bullet3 直接 CPU 与真实 DX12 图路径仍各通过 72 项。Rou 的 60 帧测试继续得到最大顶点变化 1.015368，物理切换恢复及暂停冻结通过。

使用 `a017903` 的 `mmv_ui_integration` 驱动，在独立目录加载正式目录原有的 `d3d9.dll`、`MMEffect.dll`、`MMHack.dll`，以及集成版 `MMV.dll` 和 `MMVBullet3.dll`。日志确认 `MME loaded=1`，真实宿主保持隐藏；引擎下拉框在连续 60 帧后仍保持展开，三语参数浮窗、MMV 与宿主曲线窗口检查全部通过，`UI integration failures=0`。该测试覆盖 Rou 的模型参数窗口，不等同于旧基线附件崩溃已修复。

上述复测已迁移到该归档证据目录的 `current-main-*.log`、`current-main-rou/` 和 `current-main-ui-results/`。正式目录构建日志及最终运行文件哈希记录于主目录 `.local/artifacts/mmv-bullet3-main/`。合并前补丁、未跟踪源码和 EXE/DLL 快照保留在 `.local/archives/mmv-bullet3-main/20260927-102250/`。
