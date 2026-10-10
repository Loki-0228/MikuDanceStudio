# MMV 实现与验证记录

## 2026-09-27 分层循环接口

当前集成位置为正式 `main` 工作目录。总接口说明已更新到
[MMV_FX.md](MMV_FX.md#原生结构与状态循环)，公共头为 `include/mmv/mmv_deformer.h`。
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

## 历史集成记录

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

## 宿主交接

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

## 已运行的验证

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

## 公开接口依据

ID3DXBaseMesh / ID3DXMesh 的方法声明及 D3DXMESH_MANAGED 常量可在
[公开头文件](https://raw.githubusercontent.com/wine-mirror/wine/master/include/d3dx9mesh.h)
中核对。属性缓冲 Lock/Unlock 对应接口槽 24/25，托管资源选项为 0x220。
产品调用统一使用这些公开 COM 接口的类型签名。
