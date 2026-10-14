# 跨模块集成与兼容记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [MMS 与 MMV4MMS 合并审查记录](#record-review-merged)
- [第一批修复验收记录](#record-repair-batch1)
- [第二批修复验收](#record-repair-batch2)
- [MMS 与 MMV4MMS 协同修复方案](#record-repair-coordination)
- [本轮集成 build](#record-integration-build)
- [Local display changes integrated on 2026-10-10](#record-local-display-compatibility)
- [MMDModern 汉化适配包与 MMS 崩溃风险核对](#record-mmdmodern-new-dll-audit)

<a id="record-review-merged"></a>
## MMS 与 MMV4MMS 合并审查记录

原始记录：`REVIEW_MERGED_20261011.md`。其中的版本、日期和验证范围沿用原记录。

实施更新：F01、F02、F04、F07 已完成[第一批专项验收](integration.md#record-repair-batch1)。GPU Mesh 内联纹理和跨层静态测试两项遗留已完成测试并提交，见[遗留项验收](D:/MMD-Next/MMV4MMS/docs/REPAIR_FOLLOWUP_20261011.md)。第二批 F03、F05、F06、F08 已通过专项及整合验收并提交，详见[第二批记录](integration.md#record-repair-batch2)。原问题描述和优先级保留用于溯源。

本记录合并 2026 年 10 月 11 日两轮审查，保留 10 项缺陷、4 项构建与交付问题，以及 9 项优化候选。严重度与实施批次分别记录。同一文件中的问题交给同一负责人，因此个别 P2 可以随 P1 一起处理。

执行分工见[协同修复方案](D:/MMD-Next/MMS/docs/REPAIR_COORDINATION_20261011.md)。完整任务依赖和文件清单见[合并数据](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/03/consolidated-review.json)。原始记录保留在[第一轮](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/01/review-evidence.json)和[第二轮](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/02/review-evidence.json)，不覆盖原证据。

审查源码基线为 MMS `145cbda5a89c22e0a41b1a122a867e3c333678ce` 和 MMV4MMS `74edd1d`。原始缺陷来自静态检查和调用链核对，当时尚未逐项运行复现。最新实施状态以上方更新及各批验收记录为准；下列优先级保留原始排序。

整理期间，另一会话已提交 OpenMP 修复 `6ca7c986f1ee62797b4fe242244b206b53fd538d`，当前源码包含该提交。本轮没有复跑其完整性能和画面对照，因此 O01 更新为已提交、待核对验收，不再重复派发。其后续文件所有权仍须交接。仓库中原技术路线文档的删除状态保留。

缺陷按影响排列。P1 表示应先修复的内存安全或正常功能失败；P2 表示有触发条件的行为错误、状态错误或兼容风险。本轮没有 P0。

| 顺序 | 编号 | 严重度 | 问题与位置 | 修复包 |
| --- | --- | --- | --- | --- |
| 1 | F01 | P1 | [VMD 记录数计算溢出与短读未检查](D:/MMD-Next/MMS/src/model/vmd_load.cpp:432) | A |
| 2 | F02 | P1 | [渲染入口覆盖已协商的输入需求](D:/MMD-Next/MMV4MMS/src/mms/runtime_scene.inl:31) | B |
| 3 | F03 | P2 | [设备丢失等待阻塞消息处理与退出](D:/MMD-Next/MMS/src/app/frame_driver.cpp:861) | D |
| 4 | F04 | P2 | [PhysicsInput 缓存不随动态与结构变化更新](D:/MMD-Next/MMS/src/mmv4mms/physics_input.inl:30) | C |
| 5 | F05 | P2 | [全模型烘焙丢失顶点附加属性](D:/MMD-Next/MMS/src/render/native_mesh.cpp:493) | E |
| 6 | F06 | P2 | [清空烘焙缓存未清除旧 v3 数据](D:/MMD-Next/MMS/src/io/mesh_bake_cache.cpp:1158) | E |
| 7 | F07 | P2 | [mesh 替换后的材质通道仍按原槽号应用](D:/MMD-Next/MMV4MMS/src/mms/runtime_scene.inl:57) | B |
| 8 | F08 | P2 | [场景重置后仍保留旧仿真输入快照](D:/MMD-Next/MMV4MMS/src/mmv/deformer_engine.cpp:47) | F |
| 9 | F09 | P2 | [处理器设置未传入且混用计算与显示设备策略](D:/MMD-Next/MMS/src/mmv4mms/menu.inl:24) | G |
| 10 | F10 | P2 | [正式清单仍声明 VC90 运行库依赖](D:/MMD-Next/MMS/res/mmd_manifest_x64.xml:1) | H |

F01，VMD 记录数计算溢出与短读未检查。未经校验的 32 位计数乘以记录大小可能溢出，分配不足后撤销快照写越界。

验收范围：构造 0x04000000 骨骼记录数、各段短读和长度矛盾，进入真实加载路径；拒绝后不留下部分关键帧或撤销记录。

F02，渲染入口覆盖已协商的输入需求。新进程自动加载透视修正绑定、含刚体、尚未打开编辑器时，宿主省略刚体表，插件改成 ALL 后又获取绑定，数量不符。

验收范围：覆盖新进程自动加载、效果重载改变需求、失败帧重试；不能通过永久请求 ALL 掩盖问题。

F03，设备丢失等待阻塞消息处理与退出。窗口模式等待不处理消息，全屏可能取走 WM_QUIT 却没有结束外层运行。

验收范围：窗口和全屏均可退出；LOST、NOTRESET、S_OK、Reset 失败、录制取消均能正确结束等待。

F04，PhysicsInput 缓存不随动态与结构变化更新。表情顶点、惯量、刚体绑定和关节帧仅首次采集。同一对象编辑后返回旧内容或数量不匹配。

验收范围：表情逐帧改变、刚体关节增删、同数量参数修改、重复请求与清场重建；校验指针有效期。

F05，全模型烘焙丢失顶点附加属性。回放清零附加 UV 和描边标志，并将描边倍率设为 1；导出时缺少所需来源映射。

验收范围：烘焙前后比较附加 UV、描边倍率和标志；覆盖重复、重排、生成顶点及 source mapping 缺失的明确处理。

F06，清空烘焙缓存未清除旧 v3 数据。旧分层项目的 v3 节保留于原项目文档，全部清除后保存重载可能恢复缓存。

验收范围：旧 layered/v3 项目导入、全部清除、保存、重载后不复活；不删除用户原始 ABC 文件。

F07，mesh 替换后的材质通道仍按原槽号应用。合法减少材质会越界报错；重排材质会施加错误的纹理、sphere、toon 表情系数。

验收范围：两材质变一材质、sourceMaterial=[1,0]、-1 新材质、一源多输出及非法来源；新材质采用中性系数。

F08，场景重置后仍保留旧仿真输入快照。Reset 清 previous/output，却保留 source/upstream，旧 domain 可继续返回过时快照并保留场景内存。

验收范围：清场、删对象后四个阶段查询不返回旧场景；已借出 lease 仍安全，释放后可回收；保留约定的 CPU device-loss 行为。

F09，处理器设置未传入且混用计算与显示设备策略。遗漏 processor 回调使插件停留 Automatic；只补回调会与借用的 DX9 HAL 或指定异卡冲突。

验收范围：CPU、Automatic、指定 GPU、跟随渲染器、多显卡不可用与设备恢复；显示设备保持宿主所有权。

F10，正式清单仍声明 VC90 运行库依赖。PE 普通导入面向 VC140，但激活清单仍含 VC90 CRT/OpenMP。干净系统启动影响尚未实测。

验收范围：检查最终 PE 的清单；原生和 MME 启动；保留权限、Common Controls 和现有 DPI 策略。

构建与交付问题影响复现、部署和优化测量。C01、C02 属于基线准备，不等于已经发生 ABI 损坏。Win7 是否作为正式支持目标尚未决定；目标未改变前，保留既有兼容边界。

| 编号 | 问题 | 已有证据 | 验收要求 |
| --- | --- | --- | --- |
| C01 | Conan 声明与实际编译器不一致且未锁定工具版本 | profile 继承用户 default，compiler.version=193；已有实际构建为19.44。SDK与小版本也未统一固定。 | 提交项目自有配置；记录实际编译器、SDK、CRT、架构、依赖包ID与锁定信息；不修改生成配置。 |
| C02 | 部署运行库版本未随构建基线固定 | 14.44构建实际加载VCOMP140 14.51.36247.0。固定编译器不等于固定运行库行为。 | 每次验收记录实际加载的CRT/OpenMP路径、版本、哈希和分发前提；不擅自更改用户系统运行库。 |
| C03 | 插件普通导入 D3DCompiler47 而安装规则未部署 | CPU与DX9选择发生在DLL加载之后，不能规避缺失D3DCompiler47的加载失败。 | 确定随包组件或受检系统前提；在目标干净镜像测试完整与缺失组件时的加载和诊断。 |
| C04 | Win7 安全 DLL 搜索标志要求额外补丁 | 宿主、deformer、Bullet3加载器采用的搜索标志要求KB2533623；Win7 SP1本身不是充分条件。 | 若保留Win7，确认补丁基线或安全兼容策略；不能直接删除安全搜索标志。 |

构建配置从[MMS profile](D:/MMD-Next/MMS/profiles/x64:3)与[MMV 构建脚本](D:/MMD-Next/MMV4MMS/scripts/build.ps1:16)核查。Conan 的 193 和 194 分别对应 19.3x 与 19.4x；修正配置后须核对实际编译记录和依赖包身份。[Conan 版本定义](https://docs.conan.io/2/reference/config_files/settings.html)。

若保留 Win7，不能直接换成 v145/14.50；该版本已停止支持 Win7、Win8 和 Win8.1。LTO 输入还要求精确匹配构建工具版本。[MSVC STL 变更记录](https://github.com/microsoft/STL/wiki/Changelog)、[MSVC 二进制兼容条件](https://learn.microsoft.com/en-us/cpp/porting/binary-compat-2015-2017?view=msvc-170)。

Win7 加载器的安全搜索标志要求 KB2533623；缺补丁应给出准确诊断，不以删除安全搜索标志作为默认修复。[LoadLibraryEx 文档](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexa)。

体积比较使用第一轮固定的二进制，不把正在构建的其他版本混入。

| 对象 | 字节数 | 说明 |
| --- | --- | --- |
| refs 中的 MMD 9.32 EXE | 1,723,392 | 约 1.64 MiB。 |
| MMS 正式 EXE | 4,728,320 | 约 4.51 MiB，来源提交 24691fd。 |
| EXE 差值 | 3,004,928 | MMS 约为参考程序的 2.74 倍。 |
| Imath half 查表 | 262,144 | 约占该 MMS EXE 的 5.54%；不等于已测得的可净减体积。 |
| 本机 Release 目录 | 9,785,283 | 其中六个可选插件 DLL 合计 4,697,088 字节；不能用目录大小代替 EXE 对比。 |
| 当时正式发布 ZIP | 2,122,597 | 不含可选插件、PDB 和测试文件。 |

EXE 增量约 67.15% 来自 `.text`，27.43% 来自 `.rdata`。资源增量只有 6,144 字节。已有 Release 启用 O2、Ob2、MD、OPT:REF 和 OPT:ICF；PDB 在外部。增量主要在功能代码和依赖数据，不能归结为误用了 Debug 或图标太大。链接 MAP 归属是近似估计，含填充及折叠代码，不代表删除某模块能获得等额净缩减。

字节数及段表见[PE 清单](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/01/pe-inventory.json)。half 表核验覆盖 63,490 个有限值及无穷编码，未发现不一致；NaN 规则需在后续替代实现验收中单列，见[half 表证据](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/01/half-table-validation.json)。

优化候选按已有证据与前置条件安排。只有 O01 已有受控实验支持主要归因，其修复已提交，完整验收仍待核对；其他项不预报收益。

| 编号 | 排序依据 | 候选 | 证据与限制 |
| --- | --- | --- | --- |
| O01 | 已有受控实验，优先接收既有修复 | OpenMP 主动等待 | 同EXE、8线程默认静止CPU23.12%～23.56%，PASSIVE为2.60%～2.88%；程序帧计数稍降，尚不能视为零代价。 |
| O02 | 正确性修复后优先 | 宿主快照、约束索引与 toon 转换 | 两次构建重复元数据，关节逐个扫描约束。第一份快照不读取完整几何，不能称为两次全网格回读。 |
| O03 | 与 O02 同包 | 隐藏编辑器仍触发完整输入 | 关闭窗口保留Runtime.editor，仍被视为快照消费者；需确认隐藏期间实际需求再减少工作。 |
| O04 | 按透明场景测量排序 | 透明 VB、IB 和剥离目标复用 | 排序缓存命中仍重建缓冲；最多7张RGBA32F全屏目标反复分配。必须覆盖Reset与尺寸变化。 |
| O05 | 求值顺序稳定后 | GPU 蒙皮静态输入与同帧批处理 | 逐模型提交、等待、回读、DX9上传；需要保留Auto校准，禁止交付上一帧结果。 |
| O06 | 架构阶段明确后 | 空闲时骨骼与物理缩放更新 | 部分缩放和变换在空闲门之前执行。脏标记必须覆盖表情、物理、通用变形及导出采样。 |
| O07 | 烘焙正确性合入后 | Alembic 法线与临时复制 | 已有法线时仍先计算后覆盖；条件表达式还可复制sourceVertices。先只优化局部处理，不跳求值或绘制。 |
| O08 | 固定依赖配置后实验 | Imath half 查表体积 | 已定位262144字节表；净体积与性能收益未测。全部依赖配置一致后比较。 |
| O09 | 最后独立实验 | LTO 与 Gw | 已有O2、Ob2、MD、REF、ICF；未开GL/LTCG/Gw。不同选项分别对照，不能承诺收益。 |

OpenMP 数字来自另一会话的[CPU 测量报告](D:/MMD-Next/MMS/docs/CPU_USAGE_MONING_20261011.md:39)，不是本轮新测量。报告记录的程序帧计数不是 ETW Present FPS 或 GPU 时间，也没有完成图像一致性验收。

通用 mesh 变换纳入接口目标。下列能力与缺口分别处理，避免将尚未接入的能力描述为已完成。

| 方面 | 已有能力 | 尚需完成 |
| --- | --- | --- |
| 数据表达 | deformer C ABI 支持 mesh、bones、binding、rigids 等通道，允许完整 mesh 替换。 | MMS 公共阶段消费输出，供原生渲染、MME、MMV 独立选择。 |
| 网格结构 | 允许改变顶点数、拓扑、法线、UV、附加 UV、描边和材质；已有来源标记和结构版本校验。 | 修 F05、F07；明确生成顶点、新材质与粗仿真网格到渲染网格的映射。 |
| 求值阶段 | 已有原始绑定几何、表情后未蒙皮几何和 posed mesh。 | 明确蒙皮前、蒙皮后执行位置；骨骼输出不会自动重新蒙皮，需规定输出组合和顺序。 |
| 状态与时间 | 已有 source、upstream、previousCommitted、output，固定步长及多种 Reset。 | 修 F04、F08；区分无状态形变和有状态仿真，明确 seek、重采样、输出缓存与求解器 checkpoint。 |
| 独立选择 | 渲染和 deformer 各有 DLL 接口。 | 当前 deformer 仍挂在 MMV RenderScene 内；宿主只收颜色纹理。独立仿真入口与 MME 消费路径尚未实现。 |

向前大跳目前最多推进 32 步并丢弃超额时间，不能视为任意跳帧后的确定性重建。需要相互作用的刚体、布料和软体还应明确共享求解域；只提供只读碰撞输入不等于可替换完整物理后端。具体求解器的实现不包含在接口接入任务内。

已有判断作如下限定：MMS 支持原 MME；当前是 MMV 渲染入口与 MMEffect.dll 互斥。真实 CPU 模式已有跳过无用 cs_5_1 编译的分支，当前缺陷是处理器选择没有正确到达该策略。公开头文件抽查内容一致，未发现应据此报告的 SDK 快照漂移。

<a id="record-repair-batch1"></a>
## 第一批修复验收记录

原始记录：`REPAIR_BATCH1_20261011.md`。其中的版本、日期和验证范围沿用原记录。

本批完成合并审查中的 F01、F02、F04、F07，分别涉及 VMD 读取、MMV 输入协商、PhysicsInput 刷新和替换网格的材质通道。三个 ChatGPT 子进程在独立工作树中实现，主进程整合并执行修复前后对照。用户明确改用 ChatGPT 子进程，本批未向 DSH 派发任务。

专项回归通过。扩展运行时套件仍有一个修复前后相同的失败，GPU 网格输出丢失内联纹理像素也仍存在，见下文。此次没有修改公开 SDK、ABI 或正式运行目录。

<a id="record-repair-batch1-改动"></a>
### 改动

| 问题 | 完成的行为 |
| --- | --- |
| F01 | 先读取稳定文件快照并校验全部标准段，再修改关键帧和撤销记录。按 `size_t` 检查分配尺寸，提前准备加载器自有缓冲和扩展数据。按已验证偏移跳过镜头、灯光、自阴影段，保留可选完整段边界 EOF 和扩展忽略策略。 |
| VMD 补充边界 | IK 名称比较限定为 20 字节，开关字节不再参与名称匹配。相机路径的附件扩展从完整标准体之后读取。 |
| F02 | 保留当前帧已协商的输入需求。效果重载扩大需求时拒绝旧快照；相同帧号重试也需重新查询。物理输入自动补齐刚体、关节和骨骼依赖，覆盖普通原生读取及首次接管物理。 |
| F04 | 每次查询读取当前未蒙皮顶点、法线、刚体绑定、惯量和关节帧。两组缓冲跨帧复用，同帧各次借用使用独立槽，成功发布后才切换。失败保留先前有效数组及调用方输出。 |
| F07 | 原始材质通道先按宿主原表校验，再按输出 `sourceMaterial` 应用。支持重排、删减、一源多输出及 `-1` 新材质的中性系数，拒绝非法来源。 |

VMD 的无状态污染保证覆盖文件验证和加载器自有预分配失败，不包括既有注册器、扩展 Apply 内部任意内存耗尽的全场景回滚。PhysicsInput 的借用数组保证到当前同步求值结束；同帧重复查询不会使先前借用失效。每对象保留两组历史最高容量，删除对象或清场时释放。

<a id="record-repair-batch1-构建与来源"></a>
### 构建与来源

| 项目 | 修复前源码 | 验收源码 | 主产物字节数 |
| --- | --- | --- | --- |
| MMS | `7a531c0d59128247653485eaa3ba0006c045e057` | `6d75f8e6ffff3c3f953cb3f27386a1c014d54dfd` | 4,731,392 → 4,741,120，增加 9,728 |
| MMV4MMS | `74edd1da128f232c4755eb9b3968eb34716a958f` | `f127e84809e408fbdc51a0bc2e46dcb0963d5395` | 3,166,720 → 3,167,744，增加 1,024 |

两侧都是 MSVC 19.44.35229.0、Windows SDK 10.0.26100.0、Visual Studio 2022 x64 Release。MMS 关闭诊断及阶段计时，启用现有 DX12 选项；使用同一 Conan 工具链与依赖。MMV 单独构建并向各自临时 Release 目录安装资源。没有更改画质或物理配置。

基线生产源码保持原提交，只加入相同回归测试及必要的测试注册。测试源码在统一换行后逐字相同。完整工作树状态、构建参数、测试源码校验和、EXE 和 DLL 的 SHA-256 均在构建清单（`.local/artifacts/repair-b1-20261011/01/build-manifest.json`，本地证据文件当前缺失）中。两个嵌入资源文件仍与既有产物逐字节一致。

<a id="record-repair-batch1-正确性对照"></a>
### 正确性对照

| 生产路径 | 修复前 | 修复后 |
| --- | --- | --- |
| VMD 七组用例 | 计数用例超时，其余可复现错误返回、段边界、扩展或 IK 断言失败 | 七组全部通过，覆盖截断、计数、旧新格式、可选尾、真实相机与附件、满长 IK 名称 |
| PhysicsInput 七组用例 | 七组均失败 | PMX 和 PMD 形变、同数量编辑、结构增删、失败恢复、旧 ABI、多对象及双缓冲复用全部通过 |
| MMV 输入协商 | 省略物理表后数量不匹配；普通原生读取未声明完整物理输入 | t5、t6、t14、t15，重载、同帧重试、重新查询及首次物理需求全部通过 |
| 材质重排 | 实際输出来源为 `[1,0]`，像素仍按旧槽得到左亮右黑 | 左黑右亮；共享来源双黑；`-1` 中性双亮；非法来源清空输出并失败 |
| 材质 2 减为 1 | 完成初始化透传帧后报 `Invalid material channel index` | 原生变形器输出一个来源为 1 的材质，两三角均按其通道绘制 |
| DLL 完整冒烟 | 新增目标用例复现失败 | ABI、真实隐藏 GPU 绘制、输入只读、保存加载、设备重建、原生光照及全部新增用例通过 |
| 共享目标和设备重建 | 通过 | 通过，包括共享颜色、体纹理、旧接口尾部、绘制顺序及 Reset |

后台绘制在不可见私有桌面执行，图像来自渲染目标回读。材质回归使用同一张白色 BMP；先在同一运行时确认中性场景两三角明亮、背景黑，再启用原生效果。测试不额外添加会改变网格求值的材质绑定。

最终专项结果位于 `mms-before-02-*`、`mms-after-03-*`、`mmv-before-02-*`、`mmv-after-06-*`。每次命令、超时、退出码和二进制哈希保存在对应目录的 `manifest.json`。汇总见测试结果（`.local/artifacts/repair-b1-20261011/01/test-results.json`，本地证据文件当前缺失）。

<a id="record-repair-batch1-性能与内存"></a>
### 性能与内存

PhysicsInput 使用 50,000 顶点、128 刚体、127 关节，预热 20 次、测量 300 次，逐帧修改输入，三轮交替采样。

| 实现 | 每次回调耗时范围 | 输出校验和 |
| --- | --- | --- |
| 原版，仅返回第一次缓存 | 0.023–0.047 微秒 | 300，数据已过时 |
| 首次正确刷新，每次分配 | 411.270–424.139 微秒 | 152,850 |
| 最终双缓冲刷新 | 161.219–169.785 微秒 | 152,850 |

双缓冲相对首次刷新实现减少约 60% 的回调成本。最终每次约 0.16–0.17 毫秒，若每帧查询一次，约占 60 FPS 单帧预算的 1%。多模型、多次查询会叠加；未请求 PhysicsInput 的路径没有这项复制成本。此数据不能换算成已测播放 FPS。两组 50,000 顶点的位置和法线约占 2.4 MB，另加刚体、关节和容器开销；同帧多次查询还会保留额外槽。

正常 VMD 基准包含 1,000 个骨骼关键帧、111,074 字节，文件生成及结果验证在计时外。预热 3 次，每轮载入 20 次。三轮平均耗时为修复前 35.508、34.436、32.621 毫秒，修复后 3.714、6.520、5.188 毫秒。六轮导入校验和均为 `02a3772ce88e0570`。快照解析减少了原逐字段文件读取的开销，峰值内存增加约文件大小。

MMV 使用 64×64、一个三角形、相同 DX9 效果，预热 20 帧后测量 200 帧。最终红通道回读均为 1，确认实际绘制。三轮 CPU API 累计耗时为修复前 150.462、158.065、141.123 毫秒，修复后 135.698、152.743、151.683 毫秒。整批完成及最终回读耗时为修复前 155.756、163.291、145.865 毫秒，修复后 141.806、158.103、157.520 毫秒。没有观察到稳定退化，也不能据此宣称大型场景提速。

MMS 生产核心对照使用同一莫宁模型、77,908 顶点、1,445 刚体、2,510 关节、CPU 蒙皮、800×450 回读、4096 阴影，预热 30 帧后采样 60 帧，不加载 MMV。三轮 GPU 完成后的每帧吞吐耗时为修复前 11.774、8.956、9.452 毫秒，修复后 8.879、9.871、8.908 毫秒。波动较大，不作为播放 FPS 收益。画面对照分别有 305、219、212 个像素不同，占 360,000 像素的不到 0.09%；同版本重复采样也有 109–277 个像素不同，最大通道差同为 28，不能宣称逐像素相同。

原始数据及汇总见性能结果（`.local/artifacts/repair-b1-20261011/01/performance-summary.json`，本地证据文件当前缺失）。

<a id="record-repair-batch1-遗留问题和排除记录"></a>
### 遗留问题和排除记录

- 扩展 `mmv_runtime_tests` 在两侧均报告 `714 runtime checks, 1 case(s) failed`，失败为 `native deformer layers: cross-layer complete output flows downstream`。该 case 抛出后没有执行后续断言，不能宣称套件全通过。静态复核确认该路径没有调用本批修改的需求查询，也不编入 MMS 专属检查；已登记为后续诊断项。
- GPU Mesh 输出丢失内联纹理像素是既有独立问题。`dx12.cpp` 的 `Sanitized` 清空像素，后续元数据恢复不保留内联 toon。文件纹理路径仍保留。本批没有改这条纹理保留策略；后续通用 mesh 接入前应优先修复并补真实内联纹理回归。
- 初期测试的资源缺失、shader 测试表达式编译失败、内联 toon 场景以及会改变求值的材质探针，均保留日志，不能作为通过证据。MMV 基准前三轮的蓝色像素断言不能排除黑背景，最终性能结论只采用第 4–6 轮。
- MMS 最初沙箱构建停滞，保留对应日志；完整 Release 构建随后在相同参数下完成。无进程重启或用户桌面截图。

<a id="record-repair-batch1-保存位置"></a>
### 保存位置

MMS 与 MMV4MMS 的前后构建均位于各自 `.local/builds/repair-b1-before/` 和 `.local/builds/repair-b1-after/`。本批全部原始证据位于 `D:/MMD-Next/MMS/.local/artifacts/repair-b1-20261011/01/`。

两仓 `repair-b1-before` 和 `repair-b1-integration` 工作树作为验收参考保留。实现工作树 `repair-b1-vmd`、`repair-b1-physics-input`、`repair-b1-render-contract` 完成合入后记为冗余保留，不删除。正式 `build/Release/` 未替换，既有技术路线文档删除状态保留。通用 mesh 变换结果接入仍按后续 N 批实施。

<a id="record-repair-batch2"></a>
## 第二批修复验收

原始记录：`REPAIR_BATCH2_20261011.md`。其中的版本、日期和验证范围沿用原记录。

第二批处理 D（F03 设备丢失等待）、E（F05 烘焙属性、F06 v3 清理）、F（F08 仿真状态清理）。第一批两项遗留已先行测试并提交，详见 [MMV4MMS 遗留项验收](D:/MMD-Next/MMV4MMS/docs/REPAIR_FOLLOWUP_20261011.md)。第二批已完成实现、独立复核、功能和性能对照，并测试后提交：MMS `497400b`、`f800538`、`1d5e126`，MMV4MMS `6154cdb`。

本轮按用户要求使用 ChatGPT 内部子进程。各模块在独立工作树修改，主进程维护公共 CMake、设备 Reset 和关闭入口，整合后由另一子进程只读复核。重型构建、GPU 验证和性能采样串行进行。

<a id="record-repair-batch2-修复内容"></a>
### 修复内容

<a id="record-repair-batch2-d设备丢失退出与录制"></a>
#### D：设备丢失、退出与录制

提交 `1d5e126` 让普通绘制和录制回读共用等待逻辑。窗口与全屏分支都处理消息，保留 WM_QUIT 的退出码；每轮最多处理 64 条消息，丢失状态下通过消息等待休眠，避免忙轮询。设备、窗口或回读资源发生变化时中止旧操作。录制取消和 Reset 后不再使用释放的表面，也不提前增加已录制帧数。

Reset 失败会保留恢复状态。下一帧先恢复资源，再进入绘制；设备仅返回 S_OK 不能代替此前失败的资源重建。直接从窗口消息触发的 Reset 同样受此规则约束。等待与 Reset 期间拒绝 FrameDriver 和回读重入，但仍允许结束录制、更新 CPU 相机状态和处理退出。

复核另覆盖帧内全屏退出、时间线操作、录制到达结束帧及 ESC 取消：等待消息处理或 Reset 失败后的业务刷新跳过投影、视口和背景视频纹理上传；成功 Reset 自身仍会重建视口和投影。背景视频跳转保留待刷新状态，恢复后的首帧补传；暂停后不重复上传未改变的帧。正常绘制会按当前相机重新建立投影，包含 S_OK 恢复而未执行 Reset 的情况。

<a id="record-repair-batch2-e烘焙属性与缓存持久化"></a>
#### E：烘焙属性与缓存持久化

提交 `497400b` 在完整模型烘焙中也保存已有的来源顶点映射。回放先校验映射长度和所有索引，再修改回放快照；按映射继承 4 组附加 UV（16 个分量）、描边倍率、标志及源顶点信息。重复来源、重排和分材质输出均保持对应关系。

完整旧缓存没有来源映射时，仍采用明确的旧版默认值，不能把空映射猜成恒等映射；局部回放需要映射。新生成顶点的 sentinel 保留，未知来源使用默认属性。没有升级文件格式或公开 ABI。将任意新生成属性持久化仍属于后续通用 mesh 契约 N。

提交 `f800538` 在清空全部烘焙缓存时同时删除项目中的旧 v3 节。保存并重新载入后不复活旧缓存，也不删除原始 ABC 文件、旁文件或无关扩展记录。

<a id="record-repair-batch2-f仿真快照生命周期"></a>
#### F：仿真快照生命周期

MMV4MMS 提交 `6154cdb` 在 Reset 和对象退役时清理 source、upstream、previous、output 四个阶段。非活动 domain 的查询拒绝返回旧快照，重复求值的快速路径也要求 domain 仍活动。

已经借出的 lease 保持有效，释放后内存可回收。退役保留求解器以便按既有规则重新启用；CPU domain 的设备丢失策略保持不变，GPU domain 按约定失效。此处清理发生在重置或退役事件，不增加每个稳定播放帧的输入深复制。

<a id="record-repair-batch2-正确性结果"></a>
### 正确性结果

| 项目 | 修复前 | 修复后 |
| --- | --- | --- |
| D 最终设备恢复矩阵 | 选取 33 个同条件组合：29 个复现缺陷（含 12 次超时），4 个正常对照通过 | 107/107 通过，无超时；三个入口、两种代码分支及全部补充路径 |
| E 原生属性 10 组 | 6 组失败 | 全部通过 |
| E 实际 Alembic 属性往返 | 完整模型缺少映射，属性校验和为 0 | 4 个源顶点按两个材质输出 6 行，映射、附加 UV、实际描边缓冲及校验和 358 全部通过 |
| E 旧 v3 清空后重载 | 旧缓存复活 | 无复活，源文件和无关扩展保留 |
| E 原生与 MME 导出 | 原有导出路径通过 | 最终整合构建再次通过 |
| 真实 D3DX 设备 Reset 与 CTest 注册 | 已保留基线与既有测试 | 重复实际 Reset 通过，直接从 CTest 注册入口运行正常用例通过 |
| F Reset、退役、设备失效及隐式 Reset | 四个专项均能复现旧快照残留 | 完整 490 条检查通过 |
| MMV 完整运行时 | 已修正静态用例时间条件后 846 条通过 | 846 条通过 |
| MMV 实际插件与共享目标 | 采用遗留修复后的基线 | 输入协商、材质重排/删减、内联像素、共享目标和 Reset 均通过 |

后台 GPU 测试使用不可见 Windows 桌面和真实设备，图像从渲染目标回读。D 的故障序列通过钩子控制 HRESULT，实际运行生产 FrameDriver、RecordingReadbackPass 和 Reset。所谓 fullscreen 用例覆盖全屏代码分支，底层设备仍保持隐藏窗口模式；没有验证真实独占全屏切换、拔插显卡或驱动重启。运行时单元测试中的 GPU test double 与实际插件 GPU 测试分别记录。

保留了全部失败及诊断记录，没有把以下中间结果算作最终通过：

- 原生属性测试最初缺少 D3DX 初始化，基线进程异常退出；补齐测试初始化后重新对照。
- 实际描边测试曾在第一帧过早读取未更新的源缓冲；改为第二次真实采集并先验证源姿态、法线和描边 2:1 条件，再做导出回放。
- D 测试最初把真实 Reset 内驱动自身的 266 次初始化调用计为应用违规。最终只在“原始 Reset 调用范围内且直接调用点属于设备 DLL”时单独分类；应用及窗口回调仍计为违规。分类后剩下的录制清理投影上传是真实遗漏，已加入恢复 guard 并重新测试。直接 Reset 用例还确认驱动内部会调用一次 TestCooperativeLevel，前后版本均如此；最终钩子原样转发该内部查询、单独计数，不向驱动注入应用故障状态，应用不能额外恢复的断言保留。

<a id="record-repair-batch2-性能与体积"></a>
### 性能与体积

两侧均由对应生产源码构建，使用相同素材、参数和测试入口。下面的三次对照均交替运行，无并行 GPU 采样。API 调用耗时、CPU 路径耗时、完整帧调用耗时、等待 GPU 完成后的吞吐耗时分别列出，不换算成播放 FPS。

| 项目 | 修复前 | 修复后 | 解读 |
| --- | --- | --- | --- |
| E：50,001 顶点，预热 10 次、Sample + Apply 100 次，单次 CPU 路径耗时均值 | 3.850 / 3.987 / 3.854 ms | 4.813 / 4.943 / 4.604 ms | 每次多约 0.75–0.96 ms，换取正确附加属性；源模型准备复制不计入此段 |
| E：真实导出 30 帧，预热 1 次、导出 3 次，单次均值 | 10.638 / 7.757 / 8.550 ms | 8.327 / 8.642 / 7.734 ms | 小样本波动，不能宣称加速 |
| E：相同小缓存 100 次完整 FrameDriver 调用墙钟耗时 | 46.386 / 67.870 / 34.628 ms | 43.204 / 51.041 / 46.826 ms | 含绘制和 Present；未测 GPU 完成时间，波动区间重叠 |
| F：重建、Reset、释放 lease 的每周期三段计时之和 | 0.300 / 0.339 / 0.286 ms | 0.361 / 0.365 / 0.387 ms | 50 周期；修复后真正释放旧输入 |
| F：重建、退役、释放 lease 的每周期三段计时之和 | 0.322 / 0.312 / 0.291 ms | 0.350 / 0.349 / 0.369 ms | 重置事件成本，不能外推为每帧损耗 |
| MMV 普通 64×64 三角形，200 帧 CPU API 总时间 | 135.333 / 138.832 / 151.752 ms | 137.364 / 135.461 / 138.844 ms | 未见稳定退化 |
| MMV 同一负载 GPU 完成吞吐总时间 | 140.094 / 144.082 / 156.647 ms | 142.280 / 140.359 / 143.524 ms | 不是播放帧率 |

E 的 Sample + Apply 和 F 的生命周期基准均以墙钟计时 CPU 代码路径，未取得该段线程 CPU 或 GPU 时间。

F 每周期包含 8 个 domain、16 份各 3 顶点的输入，预热 3 轮；统计仅相加求值/提交、清理及 3 个 lease 释放三段，排除输入快照构造、查询、校验和最终析构。清理加 lease 释放本身约 0.020–0.022 ms/整个 8-domain 事件。基线 16 份输入在清理和 lease 释放后仍存活；修复后清理时仅保留 2 份被借用输入，释放后为 0。全部 32 个失效阶段查询均被拒绝。

E 大缓存两侧同为 2,068,174 字节，但属性校验和由错误的 0 变为正确的 10,000,012.5。小型真实导出的 ABC 从 2,433 增至 2,721 字节（+288），因为保存了两份明确来源映射。Release MMS EXE 从 4,741,120 增至 4,745,728 字节（+4,608）；MMV4MMS DLL 从 3,168,768 增至 3,169,792 字节（+1,024）。这些是正确性修复带来的变化，不属于体积优化收益。

D 在受控丢失 200 ms 后进入 NOTRESET。三次等待总耗时（含真实 Reset）为 before 212.723 / 212.517 / 214.831 ms，after 225.122 / 215.956 / 208.909 ms；应用状态查询从 14 / 15 / 15 次降至 9 / 8 / 8 次。两侧驱动在真实 Reset 内另各查询一次，单独记录，不注入故障。线程 CPU 观测值只出现 0 或 15.625 ms，不足以量化 CPU 收益；不宣称等待更快。

MMS 正常模型使用同一份“莫宁-微调自用.pmx”：77,908 顶点、1,445 刚体、2,510 关节，CPU 蒙皮、原生渲染、阴影开启，800×450，相机固定，帧步长 1/30。设备为 NVIDIA GeForce RTX 4070 Laptop GPU。没有为性能关闭物理、描边或透明排序，MMV 未加载。每帧视口校验无错配，实际离屏目标为 1463×914，采集区域为 800×450。

| 采样条件及口径 | 修复前，三次 | 修复后，三次 |
| --- | --- | --- |
| 预热 30 帧、采样 60 帧：FrameDriver 调用均值 | 8.141 / 8.178 / 8.255 ms | 9.218 / 8.511 / 8.501 ms |
| 同上：等待 GPU 完成后的批次均摊 | 8.148 / 8.185 / 8.262 ms | 9.227 / 8.517 / 8.509 ms |
| 预热 120 帧、采样 600 帧：FrameDriver 调用均值 | 8.526 / 8.291 / 8.224 ms | 8.515 / 8.371 / 8.395 ms |
| 同上：等待 GPU 完成后的批次均摊 | 8.528 / 8.293 / 8.226 ms | 8.518 / 8.373 / 8.397 ms |

短测提示 after 偏慢，因此增加预热和采样帧数，反转首组顺序后再次交替测量。长测配对差为 −0.010 / +0.081 / +0.170 ms，波动区间重叠，未确认短测中的持续差距，也不能据此宣称零开销。Release 目标文件只读复核确认新增检查采用直接 TLS 读取，没有新增动态初始化、分配、锁或正常帧的绘制循环；未据短样本改动生产配置。所有短测和长测均保留，没有排除较慢样本。

短测前后图像差异为 149–608 像素；相同 before 自身重复的差异为 136–587 像素。长测配对差异为 214–265 像素，占 360,000 像素约 0.06%–0.07%，单通道平均绝对差为 0.00035–0.00051（0–255 标度）；同版本重复也有细微变化。画面来自实际渲染目标，未发现大范围画面差异，不宣称逐像素一致。

<a id="record-repair-batch2-构建及证据"></a>
### 构建及证据

- 测试时 MMS 最终 after 为 `f800538` 加 D 工作区补丁；源码哈希、工作树状态与二进制哈希共同标识受测版本，测试后已绑定提交 `1d5e126`，见 `accepted-commit-binding.json`。E 性能使用其专项验收构建，原始源码快照另存于 `tested-e-source-snapshot`；最终整合构建另做功能回归和正常模型采样。基线只补入一致的测试入口，生产源码未改。
- MMS 基线：`03f351eaf2a8cf1d35859376b5435ebcaadf1207`。MMV 基线：`0a502ed43ffd2677e5b4c5a129bc91bcbce83891`；其后的 `61491ab` 仅为遗留验收文档。
- 两侧固定 VS 2022 Professional、MSVC 19.44.35229、Windows SDK 10.0.26100.0、x64 Release。保留 `/Zc:alignedNew-`、CRT、浮点选项及物理依赖；MMS DIAG/PERF 关闭、DX12 开启，效果和 UI 测试开启。
- 工作树位于两个仓库各自的 `.local/worktrees/repair-b2-before` 与 `repair-b2-integration`；D/E/F 实现工作树分别为 MMS 的 `repair-b2-device-wait`、`repair-b2-bake` 和 MMV4MMS 的 `repair-b2-state`。全部保留；实现树含已整合补丁但未另提交，基线树仅增加同条件测试入口，均不能自动清理。
- 构建位于各仓库 `.local/builds/repair-b2-before` 与 `repair-b2-after`。正式 `build/Release` 未覆盖。
- [原始证据目录](D:/MMD-Next/MMS/.local/artifacts/repair-b2-20261011/01)：包括 `start-state.json`、构建脚本与日志、`build-conditions-final.json`、`final-source-and-binary-manifest.json`、`e-source-and-binary-manifest.json`、`mmv-source-and-binary-manifest.json`、`performance-summary-ef.json`、`final-acceptance-summary.json`、`render-long-summary.json` 及逐次运行的日志、二进制哈希、超时和时间记录。早期构建清单与诊断结果继续保留，以最终清单标识本次 D 验收源码。
- 每次 D 测试均独立进程、独立输出及隐藏桌面；基线超时 12 秒，修复后上限 45 秒。最终矩阵见 `d-matrix-final.json`；`d-matrix-both-verified.json`、`d-matrix-completion.json` 保留早期结果。
- MMS EXE SHA256：before `06d07241467aeb64edce169f75c4b6080e7bed0406538e856f2786e2074c07e2`；after `4d19b900128875defa739cef0bb93944b1e54c9299fb8d2396c20798132f0165`。
- MMV DLL SHA256：before `f0c33e75f990c933245e30f923045538b2bbd167cbd4a5f35d4fc8b3984a581c`；after `b1ff1feab4f48f6cc9f29d6f63140bbf780158952220a62101fc66fea182efdc`。

最终验收调度脚本曾因缺少 Path 导入，在完成设备矩阵和整合回归后退出；补齐导入的续跑脚本只执行当时尚未开始的 CTest 和渲染对照，没有重复或覆盖已完成结果。

后续按原依赖继续 G/H/K。通用 mesh 变换、拓扑变化、布料和软体的接口工作仍在 N，第二批没有提前改动 SDK 或求解器契约。

<a id="record-repair-coordination"></a>
## MMS 与 MMV4MMS 协同修复方案

原始记录：`REPAIR_COORDINATION_20261011.md`。其中的版本、日期和验证范围沿用原记录。

按本方案依次处理[合并审查记录](D:/MMD-Next/MMS/docs/REVIEW_MERGED_20261011.md)中的问题。先修复内存安全和输入输出错误，再接入通用 mesh 变换，最后比较性能及体积。第一批 A、B、C 已完成实现和专项验收，详见[第一批修复验收记录](integration.md#record-repair-batch1)。遗留项已验收提交，第二批 D/E/F 已完成验收并提交，详见[第二批记录](integration.md#record-repair-batch2)。第三批及以后尚未启动。

采用主进程加最多 3 个修复子进程。主进程固定契约、管理公共文件、串行合并与验收。子进程只修改任务允许的文件。并行开发并不代表可以并行修改同一文件、同时更新公共接口或同时测量性能。

本批用户后续明确要求直接使用 ChatGPT 子进程，因此第一批 A/B/C 和第二批 D/E/F 由内部子进程协同实现，主进程统一整合和验收。以下 DSH 条款保留为默认约定，不适用于这次明确覆盖。

当时的默认分工约定为：独立单模块修改交给 DSH，主进程负责跨模块整合。派发前读取已安装的 dsh-delegate 技能并核验宿主状态；每次显式传绝对 workspace，核对返回 cwd。宿主不可用时报告原因，不启动第二个宿主或用其他进程冒充。子进程的最终改动和交叉复核结果见对应批次验收记录。

先由主进程完成以下准备。

1. 记录两个仓库的提交与工作树状态。原审查基线为 MMS `145cbda`、MMV4MMS `74edd1d`。整理期间 MMS 新增 OpenMP 修复 `6ca7c98`，可在核对交接后作为开发基线。实施前若有新提交，重新检查对应问题，避免重复修复。
2. 从固定提交创建每包独立 Git 工作树，不复制当前未提交修改。预定路径分别为 `D:/MMD-Next/MMS/.local/worktrees/<包名>/` 和 `D:/MMD-Next/MMV4MMS/.local/worktrees/<包名>/`。第一批实际工作树位置见验收记录。
3. 每包使用所属仓库内独立的 `.local/builds/` 与 `.local/artifacts/` 目录。实验依赖使用独立缓存或唯一包身份，不改写正式依赖缓存。不要复用另一个工作树的 CMake 缓存。
4. 登记下列文件占用与公共文件所有权。任务需要越界修改时，将所需改动和理由交给主进程；主进程先调整分工，再继续相关修改。
5. 记录基础编译环境和实际加载的运行库。OpenMP 修复已提交为 `6ca7c98`，无需再创建同目标修复包；核对原会话的完整验收及文件交接。每项前后对照必须采用相同等待策略和构建条件，不能将 OpenMP 的收益算到其他任务上。

下列外部改动已核对：OpenMP 工作提交后，第一批基于 `7a531c0` 开发；主进程仅在 CMake 增加本批测试注册。OpenMP 的五个文件已在 `6ca7c98` 提交；原技术路线文档仍是既有删除状态，不能恢复或清理。

- [CMakeLists.txt](D:/MMD-Next/MMS/CMakeLists.txt)。
- [geometry_workers.hpp](D:/MMD-Next/MMS/include/mikudancestudio/geometry_workers.hpp)。
- [geometry_workers.cpp](D:/MMD-Next/MMS/src/model/geometry_workers.cpp)。
- [geometry_workers_tests.cpp](D:/MMD-Next/MMS/tests/geometry_workers_tests.cpp)。
- [window_init.cpp](D:/MMD-Next/MMS/src/window/window_init.cpp)。
- `D:/MMD-Next/MMS/MMS技术路线与插件接口讨论.md`（已有删除状态）。

根 CMake、测试注册、两侧公开 SDK 与 ABI 版本、公共模型结构、持久化格式、生成的 res、正式发布目录和跨仓库版本清单由主进程独占。`D:/MMD-Next/MMS/src/render/device_reset.cpp` 与 `D:/MMD-Next/MMS/src/app/shutdown_cleanup.cpp` 的接入也由主进程维护。OpenMP 会话尚未交接时，主进程同样不争改其 CMake 和启动文件。

按下表分批派发。同一批允许并行开发，合入与 GPU 验收始终串行。批次是保守安排；只有依赖已验收且文件所有权已交接的任务才可提前。

| 批次 | 子进程一 | 子进程二 | 子进程三 | 主进程与进入条件 |
| --- | --- | --- | --- | --- |
| 第一批 | A：VMD 读入安全 | B：MMV 输入与材质契约 | C：PhysicsInput 正确性 | 固定现有 ABI；B 中两项分提交、同 owner。 |
| 第二批 | D：设备丢失等待 | E：烘焙属性与 v3 清理 | F：仿真状态清理 | 接收第一批；逐包建立前后对照。 |
| 第三批 | G：计算与显示设备分离 | K：Alembic 局部优化 | 可安排只读验收，不新增交叉修改 | 主进程 H 固定构建、清单与运行库条件；G 接收 B、C，K 接收 E。 |
| 第四批 | J：加载与交付条件 | L：透明资源复用 | M：Imath 无查表实验 | J 接收 G、F、H；L 接收 D；M 接收 H、E、K。主进程可以设计 N，但此时不改公共接口。 |
| 第五批 | N1：MMS 通用结果接入 | N2：MMV 求值适配 | 独立只读评审 | 主进程先冻结 N 契约，再分别派发 N1、N2；两者不各自修改 SDK。 |
| 第六批 | O：快照与隐藏编辑器优化 | P：空闲求值抑制 | 独立只读评审 | N1、N2 集成验收后，先固定变更版本和时间语义再并行。 |
| 第七批 | Q：同帧 GPU 蒙皮批处理 | 独立只读评审 | 不启动竞争 GPU 的任务 | P、L 已验收，主进程统一 GPU ticket 和 Reset 接入。 |
| 第八批 | R：LTO 与 Gw 对照 | 独立只读评审 | 不启动竞争资源的采样 | 功能、运行库、OpenMP及依赖方案均稳定；每次只改变一组编译因素。 |

K、L、M 是优化任务，可以后移。通用 mesh 接入不以这些优化取得收益为前提；提前派发仍须检查实际文件所有权，并停止竞争资源的性能采样。

各包的允许修改清单如下。不存在的专属 helper、测试或实验 profile 是预定新文件，其名称不代表当前已有实现。清单外文件先交主进程处理，不因文件位于同目录就自动扩大权限。

A：VMD 读入安全。负责人：子进程。没有其他修复包的硬性前置。

- [vmd_load.cpp](D:/MMD-Next/MMS/src/model/vmd_load.cpp)。
- [vmd_read_guard.hpp](D:/MMD-Next/MMS/src/model/vmd_read_guard.hpp)（第一批新增私有读取器）。
- [key_registrars.cpp](D:/MMD-Next/MMS/src/model/key_registrars.cpp)（第一批交叉复核追加，仅限定 IK 名称比较宽度）。
- [vmd_load_safety_tests.cpp](D:/MMD-Next/MMS/tests/vmd_load_safety_tests.cpp)。

验收及边界：真实读入回归；容量、长度、短读、错误回滚与正常VMD均通过。新增测试文件由主进程注册。

B：MMV 输入与材质契约。负责人：子进程。没有其他修复包的硬性前置。

- [runtime_scene.inl](D:/MMD-Next/MMV4MMS/src/mms/runtime_scene.inl)。
- [scene_mesh_cache.hpp](D:/MMD-Next/MMV4MMS/src/mms/scene_mesh_cache.hpp)。
- [runtime.cpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.cpp)。
- [mms_plugin_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mms_plugin_tests.cpp)。
- [mms_scene_mesh_cache_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mms_scene_mesh_cache_tests.cpp)。

验收及边界：F02与F07分两个提交、同一所有者。覆盖需求随重载改变，使用已有sourceMaterial；不得擅改公开ABI。宿主适配若必需，由主进程先补充契约和改动范围。

C：PhysicsInput 正确性。负责人：子进程。没有其他修复包的硬性前置。

- [physics_input.inl](D:/MMD-Next/MMS/src/mmv4mms/physics_input.inl)。
- `D:/MMD-Next/MMS/tests/mmv4mms_physics_input_tests.cpp`（预定新增）。

验收及边界：最小修复在回调内更新动态输入和可变结构；先不向ModelRecord加epoch，也不广泛修改编辑器。后续性能包再引入有证据的缓存。

D：设备丢失等待。负责人：子进程。没有其他修复包的硬性前置。

- [frame_driver.cpp](D:/MMD-Next/MMS/src/app/frame_driver.cpp)。
- [record_readback.cpp](D:/MMD-Next/MMS/src/app/record_readback.cpp)。
- `D:/MMD-Next/MMS/src/app/device_lost_wait.hpp`（预定新增）。
- `D:/MMD-Next/MMS/tests/device_lost_wait_tests.cpp`（预定新增）。

验收及边界：消息、退出和失效表面覆盖。新私有helper可新增。device_reset与shutdown接线由主进程独占。

E：烘焙属性与 v3 清理。负责人：子进程。没有其他修复包的硬性前置。

- [native_mesh.cpp](D:/MMD-Next/MMS/src/render/native_mesh.cpp)。
- [alembic_export.cpp](D:/MMD-Next/MMS/src/io/alembic_export.cpp)。
- [mesh_bake_cache.cpp](D:/MMD-Next/MMS/src/io/mesh_bake_cache.cpp)。
- [alembic_export_tests.cpp](D:/MMD-Next/MMS/tests/alembic_export_tests.cpp)。
- [mesh_bake_cache_tests.cpp](D:/MMD-Next/MMS/tests/mesh_bake_cache_tests.cpp)。
- `D:/MMD-Next/MMS/tests/native_bake_attributes_tests.cpp`（预定新增）。

验收及边界：F05与F06分提交、同一所有者；优先既有sourceVertices映射。公共格式改变须先交主进程固定。

F：仿真状态清理。负责人：子进程。没有其他修复包的硬性前置。

- [deformer_engine.cpp](D:/MMD-Next/MMV4MMS/src/mmv/deformer_engine.cpp)。
- [mmv_deformer_engine_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_deformer_engine_tests.cpp)。

验收及边界：覆盖scene/object reset、借出lease、设备丢失与禁用/再启用；不改公开ABI。

G：计算与显示设备分离。负责人：主进程先定契约，子进程实现内部策略。前置：B：MMV 输入与材质契约；C：PhysicsInput 正确性。

- [menu.inl](D:/MMD-Next/MMS/src/mmv4mms/menu.inl)。
- [mmv4mms_menu_tests.cpp](D:/MMD-Next/MMS/tests/mmv4mms_menu_tests.cpp)。
- [runtime.cpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.cpp)。
- [runtime.hpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.hpp)。
- [compat.cpp](D:/MMD-Next/MMV4MMS/src/mmv/compat_dx9/compat.cpp)。
- [mmv_runtime_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_runtime_tests.cpp)。

验收及边界：跨仓库分别提交；冻结显示设备借用规则，不靠补一行callback完成；不能与Runtime或menu所有者并行。

H：构建与运行库基线及清单。负责人：主进程。没有其他修复包的硬性前置。公共构建接入仍须等 OpenMP 文件交接。

- [x64](D:/MMD-Next/MMS/profiles/x64)。
- [conanfile.py](D:/MMD-Next/MMS/conanfile.py)。
- `D:/MMD-Next/MMS/CMakePresets.json`（预定新增）。
- [mmd_manifest_x64.xml](D:/MMD-Next/MMS/res/mmd_manifest_x64.xml)。
- [build.ps1](D:/MMD-Next/MMV4MMS/scripts/build.ps1)。
- [mmv_distribution.cmake](D:/MMD-Next/MMV4MMS/cmake/mmv_distribution.cmake)。

验收及边界：保留当前兼容性；OpenMP交接前不修改公共CMake。分别固定开发构建条件和部署运行库；生成res与最终PE由主进程串行生成、检查。

J：加载与交付条件。负责人：主进程协调，单一加载器任务。前置：G：计算与显示设备分离；F：仿真状态清理；H：构建与运行库基线及清单。

- [menu.inl](D:/MMD-Next/MMS/src/mmv4mms/menu.inl)。
- [deformer_engine.cpp](D:/MMD-Next/MMV4MMS/src/mmv/deformer_engine.cpp)。
- [mmv_bullet3_effect.cpp](D:/MMD-Next/MMV4MMS/plugins/bullet3/integration/mmv_bullet3_effect.cpp)。
- [mmv_distribution.cmake](D:/MMD-Next/MMV4MMS/cmake/mmv_distribution.cmake)。

验收及边界：承接menu、engine、distribution文件所有权后实施。目标系统或分发策略未定时，仅完成依赖检测和方案，不擅自放弃Win7。

K：Alembic 局部处理。负责人：子进程。前置：E：烘焙属性与 v3 清理。

- [alembic_export.cpp](D:/MMD-Next/MMS/src/io/alembic_export.cpp)。
- [alembic_export_tests.cpp](D:/MMD-Next/MMS/tests/alembic_export_tests.cpp)。

验收及边界：保留所有输入采样和求值；逐字段比较输出与导出耗时；不同提交分别量化法线和复制优化。

L：透明资源复用。负责人：子进程。前置：D：设备丢失等待。

- [model_renderers.cpp](D:/MMD-Next/MMS/src/render/model_renderers.cpp)。
- [transparent_peeling.cpp](D:/MMD-Next/MMS/src/render/transparent_peeling.cpp)。
- [transparent_peeling.hpp](D:/MMD-Next/MMS/include/mikudancestudio/transparent_peeling.hpp)。
- `D:/MMD-Next/MMS/tests/transparent_resource_pool_tests.cpp`（预定新增）。

验收及边界：排序、剥离层数、画质不变；resize、Reset、场景切换、退出释放；主进程串行接入device_reset及shutdown。

M：Imath 无查表实验。负责人：子进程。前置：H：构建与运行库基线及清单；E：烘焙属性与 v3 清理；K：Alembic 局部处理。

- [conanfile.py](D:/MMD-Next/MMS/conanfile.py)。
- `D:/MMD-Next/MMS/profiles/imath-no-table`（预定新增）。
- `D:/MMD-Next/MMS/tests/imath_half_compatibility_tests.cpp`（预定新增）。

验收及边界：使用独立依赖缓存与包身份；检验表符号消失及净尺寸。65536种half分类核对，NaN规则单列；比较Alembic正确性与速度，不直接替换正式依赖。

N：通用变形与仿真契约。负责人：主进程。前置：B：MMV 输入与材质契约；C：PhysicsInput 正确性；E：烘焙属性与 v3 清理；F：仿真状态清理；G：计算与显示设备分离；J：加载与交付条件。

- 两侧 SDK、公共生命周期与构建接入由主进程统一形成改动；子进程不自行增加公开字段。

验收及边界：设计可先并行，生产接口改动须等前置合入。独占两侧SDK、ABI、菜单持久化、device_reset、shutdown、公共构建；确定原始、未蒙皮和posed阶段、mesh/bone顺序、时间及所有权。

N1：MMS 通用结果接入。负责人：子进程。前置：N：通用变形与仿真契约；D：设备丢失等待。

- [host.cpp](D:/MMD-Next/MMS/src/mmv4mms/host.cpp)。
- [menu.inl](D:/MMD-Next/MMS/src/mmv4mms/menu.inl)。
- [renderer.inl](D:/MMD-Next/MMS/src/mmv4mms/renderer.inl)。
- [physics_input.inl](D:/MMD-Next/MMS/src/mmv4mms/physics_input.inl)。
- [frame_scene.cpp](D:/MMD-Next/MMS/src/render/frame_scene.cpp)。
- [native_mesh.cpp](D:/MMD-Next/MMS/src/render/native_mesh.cpp)。
- [mmv4mms_menu_tests.cpp](D:/MMD-Next/MMS/tests/mmv4mms_menu_tests.cpp)。
- `D:/MMD-Next/MMS/tests/mmv4mms_mesh_pipeline_tests.cpp`（预定新增）。

验收及边界：同一变形模块结果能交原生、MME和MMV；每帧只求值一次；渲染切换不额外推进仿真，bone与mesh组合不重复蒙皮。只按冻结SDK实现。

N2：MMV 求值适配。负责人：子进程。前置：N：通用变形与仿真契约。

- [runtime.cpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.cpp)。
- [runtime.hpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.hpp)。
- [runtime_deformer.inl](D:/MMD-Next/MMV4MMS/src/mmv/runtime_deformer.inl)。
- [deformer_snapshot.cpp](D:/MMD-Next/MMV4MMS/src/mmv/deformer_snapshot.cpp)。
- [deformer_engine.cpp](D:/MMD-Next/MMV4MMS/src/mmv/deformer_engine.cpp)。
- [runtime_scene.inl](D:/MMD-Next/MMV4MMS/src/mms/runtime_scene.inl)。
- [mmv_runtime_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_runtime_tests.cpp)。
- [mmv_deformer_engine_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_deformer_engine_tests.cpp)。
- [mmv_deformer_snapshot_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mmv_deformer_snapshot_tests.cpp)。
- [mms_plugin_tests.cpp](D:/MMD-Next/MMV4MMS/tests/mms_plugin_tests.cpp)。

验收及边界：与N1按冻结SDK并行；内部快照、事务和状态不能绑到某个显示后端；接口修订先交主进程，相关双方暂停依赖修改。

O：快照与隐藏编辑器。负责人：子进程。前置：N1：MMS 通用结果接入；N2：MMV 求值适配。

- [host.cpp](D:/MMD-Next/MMS/src/mmv4mms/host.cpp)。
- [renderer.inl](D:/MMD-Next/MMS/src/mmv4mms/renderer.inl)。
- [physics_input.inl](D:/MMD-Next/MMS/src/mmv4mms/physics_input.inl)。
- [runtime.cpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.cpp)。
- [runtime.hpp](D:/MMD-Next/MMV4MMS/src/mmv/runtime.hpp)。
- [editor.cpp](D:/MMD-Next/MMV4MMS/src/mmv/editor.cpp)。
- [frame_input_gate.hpp](D:/MMD-Next/MMV4MMS/src/mmv/frame_input_gate.hpp)。

验收及边界：版本缓存不漏表情、同数量参数修改及隐藏消费者。计数元数据复制、约束查找、纹理转换，并比较内存与帧时间。

P：空闲求值抑制。负责人：子进程。前置：N1：MMS 通用结果接入；N2：MMV 求值适配；D：设备丢失等待。

- [frame_driver.cpp](D:/MMD-Next/MMS/src/app/frame_driver.cpp)。
- [frame_scene.cpp](D:/MMD-Next/MMS/src/render/frame_scene.cpp)。
- [physics_frame.cpp](D:/MMD-Next/MMS/src/physics/physics_frame.cpp)。
- [physics_scale.cpp](D:/MMD-Next/MMS/src/physics/physics_scale.cpp)。
- [bone_scale.cpp](D:/MMD-Next/MMS/src/model/bone_scale.cpp)。

验收及边界：不跳有状态仿真、程序化mesh、表情、导出预热或采样；与GPU批处理串行。

Q：同帧 GPU 蒙皮批处理。负责人：子进程。前置：P：空闲求值抑制；L：透明资源复用。

- [skinning_backend.cpp](D:/MMD-Next/MMS/src/model/skinning_backend.cpp)。
- [model_skinning.cpp](D:/MMD-Next/MMS/src/model/model_skinning.cpp)。
- [skinning_calibration_cache.hpp](D:/MMD-Next/MMS/src/model/skinning_calibration_cache.hpp)。
- [d3d12_skinning.cpp](D:/MMD-Next/MMS/src/render/d3d12_skinning.cpp)。
- [frame_scene.cpp](D:/MMD-Next/MMS/src/render/frame_scene.cpp)。
- [skinning_backend.hpp](D:/MMD-Next/MMS/include/mikudancestudio/skinning_backend.hpp)。
- [d3d12_skinning.hpp](D:/MMD-Next/MMS/include/mikudancestudio/d3d12_skinning.hpp)。
- [skinning_backend_tests.cpp](D:/MMD-Next/MMS/tests/skinning_backend_tests.cpp)。
- [d3d12_skinning_tests.cpp](D:/MMD-Next/MMS/tests/d3d12_skinning_tests.cpp)。

验收及边界：同帧结果、取消与Reset、Auto校准缓存版本均正确；分别测提交、GPU、等待、回读。

R：LTO 与 Gw 实验。负责人：子进程。前置：H：构建与运行库基线及清单；J：加载与交付条件；M：Imath 无查表实验；N1：MMS 通用结果接入；N2：MMV 求值适配；O：快照与隐藏编辑器；Q：同帧 GPU 蒙皮批处理。

- `D:/MMD-Next/MMS/profiles/lto-experiment`（预定新增）。
- `D:/MMD-Next/MMS/.local/artifacts/lto-experiment/README.md`（预定新增）。

验收及边界：固定同一源码与精确工具版本，分别改变GL/LTCG、Gw，保留MD、alignedNew-与物理数值基线；功能、MME导出符号、体积和性能全部验收。正式选项由主进程单独合入。

主进程按以下顺序处理公共语义和文件交接。

1. 让 B 独占 `runtime_scene.inl`，同时修 F02 与 F07。效果重载后需要更多数据时，重新获取对应快照，不能只改插件内的需求位。
2. C 先保证每次 PhysicsInput 返回当前数据。O 在此基础上做缓存，不让 C 为提前优化而侵入 ModelRecord、编辑器和 SDK。
3. F 完成后将 `deformer_engine.cpp` 交给 J，再交给 N2。G 完成后将 `menu.inl` 交给 J，再交给 N1。每次交接记录已验收提交。
4. E 完成后将 `alembic_export.cpp` 和对应测试交给 K。K 只减少重复计算与复制，不跳过求值、物理推进或 MME 捕获所需绘制。
5. H 完成后将 `conanfile.py` 交给 M。M 的实验配置先保留独立身份，主进程决定是否采纳正式依赖。
6. D 固定设备丢失和退出行为后再合入 L。主进程统一规定停止提交、结束 GPU 借用、释放默认池资源和 Reset 的顺序。后续 Q 必须遵守同一顺序。
7. N 先冻结数据与时间契约。N1 与 N2 各自在一份固定 SDK 上开发，需要修改契约时只暂停受影响部分，交主进程升级两侧快照后继续。
8. O 与 P 只在变更版本、输入可见阶段和有状态求值规则已固定后并行。若两者都需要新增公共 dirty/epoch 字段，主进程先串行完成该公共改动。
9. P 完成后把 `frame_scene.cpp` 交给 Q。Q 必须产出本帧结果；不能把旧帧结果交给 MME、录制或 Alembic 导出。

通用 mesh 接口由 N 定案，至少覆盖下列契约。

- 无状态整体变换、程序化形变与有状态刚体、布料、软体可以声明不同能力，不强制普通 mesh 变换创建物理世界。
- 输入分别说明原始绑定几何、表情后未蒙皮几何和 posed mesh。每个模块声明读取阶段与输出阶段，明确 mesh 与骨骼输出的先后及蒙皮责任。
- 拓扑变化维护稳定对象身份、来源顶点、绑定表和材质来源。粗仿真网格与渲染网格的映射独立表达，新顶点和新材质允许没有原始来源。
- CPU 输入借用期限、结果提交和失败回滚明确。异步持有输入必须有显式生命周期；GPU 交换需设备、资源状态、同步和回退契约，不能假设 DX9 任意零复制。
- 同一逻辑帧只推进一次仿真。暂停、编辑、回退、向前跳转、预热、导出采样和设备恢复分别定义行为。输出缓存不冒充完整求解器 checkpoint。
- 有双向相互作用的刚体、布料和软体明确共享求解域。渲染器切换不重新推进仿真，原生渲染与 MME 都是正常消费端。

维持现有 CMake、MikuMikuDance.exe 名称和 MME 导出约定，保留 `/Zc:alignedNew-`、Bullet 2.75 行为及浮点基线。不把升级 v145、改用 fast-math、替换物理求解器或统一静态 CRT 混入普通缺陷修复。Win7 支持目标、D3DCompiler47 分发方式及运行库部署方式尚未定案时，记录待决项，不把任意选择作为默认产品决定。

每个子任务派发说明采用下列内容。主进程填入真实工作树、提交与文件清单，不向独立会话只发送问题编号。

1. 写明任务目标、对应 F/C/O 编号、两个仓库中涉及的模块，以及用户要求保留的 MME、通用 mesh 与兼容性边界。
2. 写明已创建并核验的绝对 workspace、父提交和配对仓库版本。DSH 返回 cwd 不符时先处理目录问题，不继续项目修改。
3. 逐项复制本包允许修改文件，附主进程独占及外部占用文件清单。
4. 附该包触发条件、验收案例、允许的后台测试和不得重启或占用用户程序的约束。
5. 要求返回变更文件、独立提交、运行过的命令、原始结果路径及未完成项。子进程报告 completed 后，主进程仍须读取正式 result 并验收。
6. 限定不推送、不发布、不覆盖正式 EXE、不清理其他工作树或记录。交付仅包含任务范围内的本地提交。

所有性能采样都独占机器串行运行。独立构建目录只能避免产物覆盖，不能避免 CPU、GPU、磁盘与运行库竞争。因此重型构建、其他会话的 GPU 采样和正式性能测试不同时进行。

验收按仓库要求执行。

1. 每包修复完成后，以集成父提交构建 before，以合入该包后的提交构建 after。两侧保留相同测试入口与构建条件。记录提交、工作树、工具版本、依赖身份和 EXE/DLL 哈希。
2. 先执行真实生产路径的缺陷回归。VMD 用隔离进程处理恶意计数和短读；状态类问题验证错误回滚与借用对象寿命，不能只测试辅助算术或伪实现。
3. 进行后台前后生产 build 对照。保持素材、相机、分辨率、动作、物理、透明设置、MME 和采样帧序列一致。图像从渲染目标或回读接口取得，不靠桌面截图。
4. 验证后台窗口没有因最小化、遮挡、跳过绘制或 Present 而降低工作量。性能优化不得关闭物理、描边、透明层或降低画质。
5. 分别记录进程 CPU、CPU 求值与提交时间、GPU 时间、同步等待、峰值内存和导出耗时。取得 Present 数据才报告该口径 FPS；程序内部计数另列。
6. 交替采样前后版本并保留波动、失败和排除样本。每项优化事先规定判定指标；收益不明确或正确性变化时不采纳正式配置。
7. 主进程逐包合入并复核交叉场景：新进程加载透视效果、动态表情、mesh 拓扑与材质变化、设备丢失、场景清理、MME 绘制及导出取消。
8. 只有相关目标系统镜像实际启动、加载插件和变形器通过后，才宣称该系统兼容性已验收。静态导入检查不替代干净系统运行。

第一批、遗留项与第二批的实际工作树、before/after build、采样证据和本地提交见各批验收记录。基线与实现工作树均保留，不自动删除；后续批次仍须按前置条件重新核验文件所有权。

计数、来源覆盖、依赖顺序、同批文件所有权及本地链接可用以下命令重新核对：`D:\anaconda3\python.exe -X utf8 D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/03/validate_plan.py`。检查结果保存在 [plan-validation.json](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/03/plan-validation.json)。该检查只验证方案结构，不能代替修复代码的运行验收。

<a id="record-integration-build"></a>
## 本轮集成 build

原始记录：`INTEGRATION_BUILD_20260927.md`。其中的版本、日期和验证范围沿用原记录。

本轮代码提交到正式 `main`，未推送。主程序生产源码对应 `35d1e4b`；其后的 `ef7343f` 只加强测试基准和更新文档。

<a id="record-integration-build-运行文件与目录"></a>
### 运行文件与目录

- 新主程序：`.local/releases/mms-retier-20260927/MikuMikuDance.exe`。
- SHA-256：`25CC0F27D7A62E4E42EEF1B4B52DE074B991B6191DFEF2363EEA6160D657A4D3`。
- 使用现有 VS 2022 x64 Release 配置，以 MSBuild `OutDir` 指向上述目录。原 `build/Release/MikuMikuDance.exe` 被用户程序占用，未中止该程序，也未覆盖它。
- 同目录 DLL 来自原 `build/Release`，保留当前安装组合；它们不是本轮重新部署的 MMV 版本。各文件哈希见同目录 `build-manifest.json`。
- MMV 两种既有构建目标均已编译，分别位于 `build/mmv/dx12/Release/MMV.dll` 和 `build/mmv/compat/Release/MMV.dll`。
- 无 MME 回读验收目录 `.local/releases/mmv-no-mme-validation/` 使用测试宿主和夹具插件，不能当作发行程序使用。

<a id="record-integration-build-已整合与验证"></a>
### 已整合与验证

| 项目 | 结果与记录 |
| --- | --- |
| 透明合批、三档强度下调 | CPU 回归及最高档普通/4×MSAA 对照通过；最高精度与上限不变。见 [TRANSPARENT_SORT_STRENGTHS_20260927.md](../rendering/transparency.md#record-transparent-sort-strengths) |
| 显示菜单混合语言 | 旧版复现、新版两套菜单的三语检查通过。见 [MENU_LANGUAGE_FIX_20260927.md](../interface/languages.md#record-menu-language-fix) |
| 普通骨骼点击 | 前后测试复现并验证修复。见 [BONE_CLICK_SELECTION_20260927.md](../animation/editing.md#record-bone-click-selection) |
| VPD 缩放尾部 | 80 项格式检查与 160 项集成检查通过 |
| 操作柄四角停靠 | 绘制回读、命中和用户设置测试通过。见 [VIEWPORT_HANDLE_CORNERS_20260927.md](../interface/controls.md#record-viewport-handle-corners) |
| 顶部骨骼缩放读数 | 18 次 GPU 回读、数值轴向与刷新传输检查通过。见 [BONE_SCALE_READOUT_20260927.md](../interface/controls.md#record-bone-scale-readout) |
| 运算器设备列表 | 菜单测试通过；额外检测到但 D3D9 无法使用的设备显示为不可选信息 |
| MMV 双击/拖入赋予 | 编辑器测试 4,887 项通过 |
| 无 MME 的 0 层输出 | 固定、自阴影及设备重建后的回读通过。三组恒等重复均 0 像素差；输出差分别为 5,104、364、4,959 像素。见 [MMV_WITHOUT_MME.md](../extensions/host.md#record-mmv-without-mme) |
| FX 同目录 DLL 打包 | Bullet3 仓库提交 `15415f99`，4/4 检查通过，目录为 `D:/MMD-Next/MMV-Bullet3/build/fx-package/MMV/Bullet3/` |

DSH 各项结果由主端复核；无 MME 的末轮诊断由主端接手完成，原 DSH 会话已中止继续读取，不代表工作被删除。

<a id="record-integration-build-保留的限制"></a>
### 保留的限制

通用透明测试有 8 个既存阴影遮罩断言失败，前后版本相同，未删断言。新增 MME 登记门禁的缓存回放前后对照尚未单独完成；真实多层 FX 和可见菜单的人工操作不在本轮后台验收中。

CPU+DX9 MMV 的另一会话改动，以及原有 `mmv_bullet3_host_tests.cpp` 改动保留在工作区，未混入本轮提交。没有删除临时工作树、日志或旧运行文件。

构建日志、原始回读与测试记录分别保存在 `.local/artifacts/global-processor/`、`.local/artifacts/no-mmv-stutter-20260927/` 及各功能文档标明的位置。

<a id="record-local-display-compatibility"></a>
## Local display changes integrated on 2026-10-10

原始记录：`LOCAL_DISPLAY_COMPATIBILITY.md`。其中的版本、日期和验证范围沿用原记录。

The pending README, recording-preview, and startup-resolution changes are
integrated with the model-name translation and acknowledgement-link commits.
The README keeps all three language versions, the new feature descriptions,
StarNoodle495's name, and the already committed Salad Sauce homepage. Its
processor-setting description still distinguishes per-user preferences from
project data.

Fresh installations use 1920 x 1080. Valid saved dimensions remain unchanged,
including 640 x 360: the old configuration has no flag that distinguishes an
untouched default from an intentional user choice. Missing or nonpositive
dimensions fall back to the new defaults.

The recording preview fits the nearest monitor's work area and keeps the
export aspect ratio. The full capture source is presented into the preview's
client area. Passing a null destination to
[D3D9 Present](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-present)
scales it to that area instead of clipping an export-sized destination. Capture
dimensions remain at the requested export resolution, and recording restores
the previous render dimensions when it ends.

<a id="record-local-display-compatibility-verification"></a>
### Verification

The baseline is `b1d1b8f14470401f285c2a64976edea1d3d5143a` plus the pending
local changes and the new regression harness. The original local patch is
retained in `.local/archives/local-compatibility/original-uncommitted.patch`.
Both builds use Release x64, Visual Studio 17 2022, the Conan toolchain,
`MIKUDANCESTUDIO_PERF=ON`, and the same test inputs.

The harness starts the real application initialization code in hidden windows,
with a separate executable and configuration directory for each case. Recording
uses the production MOV entry point and frame loop. A synthetic monitor work
area at negative coordinates forces a smaller preview. A pass-through hook
observes the actual D3D9 presentation arguments; it does not replace rendering
or capture the desktop.

| Check | Pending local version | Integrated version |
| --- | --- | --- |
| New installation uses 1080p | Pass | Pass |
| Saved 640 x 360 survives startup | Fail: changed to 1080p | Pass |
| Saved portrait resolution survives startup | Pass | Pass |
| Missing dimensions use 1080p | Pass | Pass |
| Preview fits the monitor work area | Pass | Pass |
| Full capture fits the preview destination | Fail: destination clips | Pass |
| MOV contains three 1920 x 1080 PNG frames | Pass | Pass |
| Render resolution is restored after export | Pass | Pass |
| GPU capture pixels match the independent filter oracle | Pass | Pass |

All three exported PNG samples have identical hashes across the two builds.
The existing UI-language and menu suites also pass after integration.
README relative links and three-language acknowledgement entries were checked.
No scene FPS, CPU submission, or GPU-time comparison was performed; performance
remains unverified.

<a id="record-local-display-compatibility-local-artifacts"></a>
### Local artifacts

- `.local/artifacts/local-compatibility/01/` contains source revisions,
  worktree status, build settings, hashes, logs, exported test frames, and the
  final installation record.
- `.local/releases/local-compatibility-before/` retains the pending local build.
- `.local/releases/local-compatibility-after/` retains the integrated build.
- `.local/archives/local-compatibility/` contains the original local patch and
  the previous installed executable.
- `build/Release/MikuMikuDance.exe` receives the verified integrated executable.

These artifacts are retained for comparison and are not a published release.

<a id="record-mmdmodern-new-dll-audit"></a>
## MMDModern 汉化适配包与 MMS 崩溃风险核对

原始记录：`MMDMODERN_NEW_DLL_AUDIT_20261010.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-mmdmodern-new-dll-audit-结论"></a>
### 结论

**尚不能确认用户提到的旧版崩溃已在 MMS 中修复。** 用户未掌握触发操作，两份包也没有提供对应的崩溃修复记录。

新包唯一变化的配置值是 `DrawOrderTable=1` 改为 `0`。新旧 DLL 中已定位的顺序表、纹理索引和当前对象缓存核心代码相同。顺序表的宿主补丁地址经过适配，但默认不安装该功能。现有证据支持“默认禁用顺序表”，不能据此认定算法已修复，也不能确定禁用是为规避用户提到的崩溃。

MMS 参考旧包的机制编写源码，没有载入 `dsound.dll`。MMS 仍启用自己的顺序表实现，因此没有继承新包的禁用配置。MMS 的边界检查、缓存失效及对象注册检查已存在，但这些保护不能替代对未知崩溃的复现。

后续静态追踪确认新包另有一项新增缺陷：相机撤销步数被写入重做容器的起始索引。在补丁安装成功、使用默认步数且第一次撤销有历史记录时，代码会向复制函数传入非法源地址与下溢后的长度。该路径与 MMS 的移植无关，详见下文“相机撤销的进一步追踪”。这项结论来自指令和数据流分析，尚未实际运行复现。

本次保留现有生产实现，只提交调查报告。未重建程序，未执行运行测试，也没有新的帧率结果。

<a id="record-mmdmodern-new-dll-audit-文件身份"></a>
### 文件身份

“新包”和“旧包”表示用户提供的两份本地文件，不表示已经确认的上游版本先后。

| 项目 | 旧包 | 新包 |
| --- | --- | --- |
| 目录 | `D:/MMD-Next/refs/MMDModern` | `D:/MMD-Next/refs/MMDModern-适配MikuMikuDance932汉化版` |
| DLL | `dsound.dll` | `dsound.dll` |
| 大小 | 785920 字节 | 785920 字节 |
| PE 时间戳 | `0x6ac80290` | `0x6ac80290` |
| 映像大小 | `0xd2000` | `0xd2000` |
| SHA-256 | `b91d627d0d245d49a93d845278464c708ee7dd3e34686fa4cfcf58a6a4e1f9e1` | `fc07ea2a1cb039b57a54f766901cbeac49a932ec9a8557a1b528d3592758709f` |

两份 `README.md` 和 `LICENSE` 相同。新包 `README-cn.md` 增加 MikuMikuDancePlus 兼容说明，目标 EXE 时间戳为 `0x5A26BB1D`，原官方目标为 `0x5DEC0538`。相同 DLL 布局、核心代码及该说明支持“基于同一构建的适配包”这一判断，不能仅凭目录名称认定它包含新版算法修复。

<a id="record-mmdmodern-new-dll-audit-二进制差异"></a>
### 二进制差异

全文件共有 5112 字节不同：

| 区域 | 不同字节数 | 核对结果 |
| --- | ---: | --- |
| `.text` | 214 | 对应 204 条发生变化的指令，均保持原指令助记符及长度，操作数有变化 |
| `.rdata` | 4895 | 包含中文字符串替换、宿主补丁地址及预期机器码变化 |
| `.data` | 1 | RVA `0xb79e0` 的值从 `0x274f0` 改为 `0x274e0` |
| PE 头 | 2 | `MajorOperatingSystemVersion` 从 `0x6` 改为 `0x3d7c` |

`.pdata`、`.fptable`、`.rsrc` 和 `.reloc` 相同。逐指令比较覆盖 `.text` 的全部 214 个不同字节。助记符不变不等于行为不变，地址和常量也可以影响正确性。

<a id="record-mmdmodern-new-dll-audit-三项移植机制"></a>
#### 三项移植机制

下表地址均为 DLL RVA，范围右端不包含在内。比较范围包含当前对象入口的全部展开信息分段，避免只比较函数开头。

| 已定位代码 | 范围 | 新旧结果 |
| --- | --- | --- |
| 顺序表构建 | `0x6730..0x67c0` | 字节相同 |
| 顺序表查询 | `0x67c0..0x6993` | 字节相同 |
| 纹理索引辅助函数 | `0x73c0..0x7849`、`0x8080..0x80eb` | 字节相同 |
| 当前对象查询 | `0xda80..0xdc0a` | 字节相同 |
| 当前对象缓存入口 | `0xdc10..0xdcaf` | 字节相同 |
| 当前对象缓存重建 | `0xddd0..0xdef1` | 字节相同 |

当前对象缓存的安装代码把宿主函数地址 `0xa80a0` 改为 `0xa8090`，位于 DLL RVA `0xdcfe`、`0xdd0e`、`0xdd71`、`0xdd81`、`0xdd9e`。这是目标 EXE 的地址适配，MMS 的源码调用不使用这些补丁地址。

渲染补丁表位于 `0x80240..0x81370`，共 25 项。其中 15 项属于纹理查询，10 项属于顺序表。新包把 13 个纹理目标地址及全部 10 个顺序表目标地址减去 `0x10`，另两个纹理目标地址不变。顺序表并非完全未做地址适配；然而新包仍通过配置关闭它。该选择的原因没有说明。

两份 INI 按节与键解析后，只有 `[Render] DrawOrderTable` 的值不同。`TextureLookupHash` 和 `CurrentObjectCache` 均保持为 `1`。

<a id="record-mmdmodern-new-dll-audit-新包的相机撤销缺陷与-pe-头异常"></a>
#### 新包的相机撤销缺陷与 PE 头异常

这两处不属于 MMS 的三项移植，不能当作 MMS 需要照搬的修复。

- **相机撤销状态写入错误。** DLL RVA `0x126b1` 从 `mov [rip+0xa80a0], rdx` 改成 `mov [rip+0xa8090], rdx`，实际写入的 DLL 内部地址由 `0xba758` 变为 `0xba748`。这里的 `0xa80a0` 是 RIP 相对位移。后续追踪确认两个地址分别属于撤销步数上限和重做容器起始索引，详见下节。该缺陷是新包引入的，不能解释用户所述的旧包崩溃。
- **PE 操作系统版本字段异常变化。** 文件偏移 `0x168` 的 `MajorOperatingSystemVersion` 从 6 变为 `0x3d7c`，即十进制 15740。微软将该字段定义为所需操作系统的主版本号，`MajorSubsystemVersion` 和 `CheckSum` 是其他字段。两份文件的子系统版本均为 6.0，校验和字段均为 0，校验和偏移为 `0x180`。因此这不是正确位置上的校验和更新。未测试该异常版本值对本机加载器的影响，不能据此断言 DLL 一定无法加载。[微软字段定义](https://learn.microsoft.com/zh-cn/windows/win32/api/winnt/ns-winnt-image_optional_header64)。

<a id="record-mmdmodern-new-dll-audit-相机撤销的进一步追踪"></a>
#### 相机撤销的进一步追踪

<a id="record-mmdmodern-new-dll-audit-错误写入的两个目标"></a>
##### 错误写入的两个目标

配置解析代码 `0x39df..0x3a42` 读取 `CameraUndoSteps`，允许范围为 1 至 1000。安装代码 `0x1267c` 取得该配置值，`0x126b1` 保存它。两份 INI 的默认值均为 30。

| 字段 | DLL RVA | 初始化值 | 用途 |
| --- | --- | ---: | --- |
| 重做容器映射表指针 | `0xba738` | 0 | 指向存放记录块指针的数组 |
| 重做容器映射表容量 | `0xba740` | 0 | 分配的指针槽数 |
| 重做容器起始索引 | `0xba748` | 0 | 参与记录寻址，新包将此值错误覆盖为 30 |
| 重做容器记录数 | `0xba750` | 0 | 当前保存的记录数量 |
| 撤销步数上限 | `0xba758` | 30 | 旧包在此保存配置，新包遗漏该写入 |

容器字段用途由 DLL 的初始化、扩容、插入和弹出指令交叉确认。本机 MSVC `14.44.35207/include/deque` 的 `_Map`、`_Mapsize`、`_Myoff`、`_Mysize` 定义也与此布局相符。这只能辅助识别容器，不证明 DLL 使用了本机这一版编译器。

<a id="record-mmdmodern-new-dll-audit-为什么清理没有消除错误"></a>
##### 为什么清理没有消除错误

`0x13fc0` 是容器清理函数。只有记录数非零、实际弹出最后一项后，`0x1400f` 才把起始索引归零。对于 `map=0、size=0、offset=30` 的空容器，分支 `0x13fd6` 直接跳过该写入，随后因映射表为空返回。

相机历史初始化在 `0x13bd7` 清理重做容器。记录一次新的相机修改时，`0x120b9` 也清理重做容器。两条路径都调用上述函数，因此不会清掉这个空容器中被污染的索引。

<a id="record-mmdmodern-new-dll-audit-第一次撤销如何到达非法复制"></a>
##### 第一次撤销如何到达非法复制

条件是相机撤销补丁成功安装，`CameraUndo=1`，使用默认 `CameraUndoSteps=30`，且用户已经产生至少一条可撤销的相机历史。按这些条件追踪：

1. 撤销处理 `0x13390` 从撤销历史取出记录。
2. `0x13582` 取得重做容器地址，`0x13589` 调用 `0x14140`，将该记录存入重做历史。
3. 此时重做容器的容量与记录数均为 0。插入函数 `0x1416c` 先调用扩容函数 `0x13e00`，然后才执行 `0x14178` 的索引取模。错误值在取模前已进入扩容路径。
4. 扩容函数按 8 个指针分配新映射表。`0x13e58` 读到起始索引 30，随后计算旧表复制区间。

`0x13ecb` 调用内存复制函数 `0x79460` 时的参数如下：

| 参数 | 旧包正常初值 | 新包被污染的初值 |
| --- | --- | --- |
| 起始索引 | 0 | 30 |
| 复制源地址 | 0，复制长度为 0 | `0 + 30 × 8 = 0xf0` |
| 复制目的地址 | 新映射表起点 | 新映射表起点加 240 字节 |
| 复制字节数 | 0 | `(0 − 30) × 8`，按无符号 64 位为 `0xffffffffffffff10` |

新包请求从低地址 `0xf0` 复制极大长度，目的起点也已超过名义上 64 字节的新表。复制函数的 AVX 分支 `0x795dd`、SSE 分支 `0x798b6` 或字节复制分支 `0x7945b` 都会读取源内存。由此可静态确定该路径存在非法内存访问，不能将它解释为无影响的地址适配。

新包还使自定义撤销步数无法写入真正的上限。`0x13d96` 及 `0x13de0` 从历史对象偏移 `0x68` 读取上限，即 `0xba758`，因此仍使用初始值 30。安装日志 `0x127d0` 却继续读取配置值，可能显示与实际上限不同的数字。

<a id="record-mmdmodern-new-dll-audit-修正位置与结论边界"></a>
##### 修正位置与结论边界

相机缺陷对应的最小修正是将新 DLL 文件偏移 `0x11ab4` 的 `90` 恢复为 `a0`。完整指令从 `48 89 15 90 80 0a 00` 恢复为 `48 89 15 a0 80 0a 00`，使内部写入重新指向 `0xba758`。其他指令中用于汉化版 EXE 的 `0xa8090` 目标地址有独立用途。

PE 头异常的原始值为文件偏移 `0x168` 的 `06 00`，新包为 `7c 3d`。恢复该字段可单独处理，它与相机容器缺陷不是同一个问题。

上述字节仅作为修正定位，本次没有修改任一 DLL。没有执行目标程序或复制函数，也没有运行模拟器。相机路径结论来自反汇编和整数运算，尚未取得实际崩溃日志。MMS 没有移植这段相机撤销实现，因此无需将这两处二进制修正套用到 MMS。

<a id="record-mmdmodern-new-dll-audit-mms-源码核对"></a>
### MMS 源码核对

本次核对时最新提交为 `6368b3b`。移植原提交为 `f1aa12f2930489c18d6f583d6f1d4d9d7c931344`。

| 机制 | 现有保护 | 与 DLL 的区别 |
| --- | --- | --- |
| `BuildDrawOrder` | 固定容量数组初始化为空，跳过空对象、负数顺序及超界顺序，重复顺序保留首项 | 每次遍历重新构建，不使用 DLL 的跨调用顺序表缓存或机器码补丁 |
| `FindTextureResource` | 索引拥有路径字符串副本，按资源代次重建，返回资源项的实时字段 | 使用宿主资源生命周期失效，不扫描原版 EXE 的固定地址 |
| `ExpGetCurrentObject` | 缓存仅在渲染线程且场景未变更时命中，核对注册代次、通道、分割顺序、槽位指针及对象顺序 | 使用完整对象指针和宿主槽位访问，不安装 DLL 跳转补丁 |

源码位置：

- [顺序表实现](../../../include/mikudancestudio/draw_order.hpp)，调用位于 `src/render/model_renderers.cpp` 和 `src/render/accessory.cpp`。
- [纹理索引实现](../../../include/mikudancestudio/texture_lookup.hpp)，失效调用位于 `src/unported/stubs.cpp`、`src/model/path_resolve.cpp`、`src/window/enhance_model_io.cpp` 和 `src/app/shutdown_cleanup.cpp`。
- [当前对象接口](../../../src/exports/effect_api.cpp)，场景变更失效位于 `src/app/scene_ownership.cpp`。

`ExpGetCurrentObject` 已有“对象未被效果引擎注册则返回 0”的保护。源码说明该保护针对 MMHack 查询未注册对象状态时的空指针访问。没有证据表明这就是用户提到的 MMDModern 旧版崩溃，因此不能把这项已有保护列为本次新修复。

正式目录 `build/Release/` 中没有 `dsound.dll`。本次回读的正式 `MikuMikuDance.exe` SHA-256 为 `90cf474dcc0f004f757828369483979d9d00775e490fd5543c6e9a6dc7d500d6`，与前次实测版本一致。

<a id="record-mmdmodern-new-dll-audit-证据与验证范围"></a>
### 证据与验证范围

静态分析产物保存在 `.local/artifacts/mmdmodern/20261010/new-dll-audit/`：

- `compare.py`：读取 PE、反汇编差异、比较完整核心代码范围、解析 INI 与渲染补丁表。
- `comparison.json`：文件哈希、各区域统计、函数范围、逐指令差异、配置和 PE 头差异。
- `code-changes.txt`：逐指令变化。
- `render-patch-table.json`：25 项渲染补丁的目标地址与变化。
- `summary.txt`：比较脚本输出。
- `camera_deep.py`：提取相机数据流相关指令，记录指令文件偏移和静态计算条件。
- `camera-deep-old.txt`、`camera-deep-new.txt`：两份 DLL 的相关指令。
- `camera-deep-summary.json`：错误写入目标、PE 字段与复制参数的静态推导。
- `camera-global-xrefs.txt`：相机历史全局字段的 RIP 相对引用。

本机可在 `D:/MMD-Next/MMS` 下运行以下命令重新生成静态统计，依赖已安装的 `pefile` 和 `capstone`：

```powershell
python -X utf8 .local/artifacts/mmdmodern/20261010/new-dll-audit/compare.py
python -X utf8 .local/artifacts/mmdmodern/20261010/new-dll-audit/camera_deep.py
```

该脚本读取 DLL 字节，不载入 DLL。本次未启动 MMD 或 MMS，未运行回归、压力或性能测试。此前的 `Stage.pmm` 对照见[移植与测量报告](../performance/cpu.md#record-mmdmodern-port)，不能作为这次未知崩溃的复现或修复证据。
