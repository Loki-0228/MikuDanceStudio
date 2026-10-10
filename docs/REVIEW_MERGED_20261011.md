# MMS 与 MMV4MMS 合并审查记录

本记录合并 2026 年 10 月 11 日两轮审查，保留 10 项缺陷、4 项构建与交付问题，以及 9 项优化候选。严重度与实施批次分别记录。同一文件中的问题交给同一负责人，因此个别 P2 可以随 P1 一起处理。

执行分工见[协同修复方案](D:/MMD-Next/MMS/docs/REPAIR_COORDINATION_20261011.md)。完整任务依赖和文件清单见[合并数据](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/03/consolidated-review.json)。原始记录保留在[第一轮](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/01/review-evidence.json)和[第二轮](D:/MMD-Next/MMS/.local/artifacts/size-code-review-20261011/02/review-evidence.json)，不覆盖原证据。

审查源码基线为 MMS `145cbda5a89c22e0a41b1a122a867e3c333678ce` 和 MMV4MMS `74edd1d`。缺陷来自静态检查和调用链核对，尚未逐项运行复现。下列优先级不表示修复已实施。

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
