# 测试保留与清理说明

核对日期：2026-09-20。所有测试源文件均在首行用 `MAINTENANCE:` 标记。测试源码不进入发布目录，目标使用 `EXCLUDE_FROM_ALL`；编译后的测试程序位于 `build/tests/`。不要因为测试不随正式程序发布就删除源码。

## 长期回归：`regression-keep`

这些文件覆盖真实行为或已发生的缺陷，应保留；仅在对应功能删除，或验证范围完整迁移到其他测试后再移除。

| 文件 | 用途 |
| --- | --- |
| `optional_graphics_imports.py` | 检查正式 EXE 的普通与延迟导入表，防止可选 DX12、DXGI、编译器 DLL 再次成为启动依赖。只用 Python 标准库，注册 CTest。 |
| `graphics_runtime_tests.cpp` | 隐藏 DX9 设备上注入可选 DLL 或入口缺失，验证 CPU 蒙皮回退、描边、Present 和像素回读。标记为 hardware-manual，需 DX9、DX12 硬件，逐场景注册 CTest。 |
| `native_shadow_tests.cpp` | 隐藏 D3D9 设备上的原生 SM3 阴影回读，覆盖两种模式、接收面修正、斜边平滑、贴图边界与缩小采样。 |
| `keyboard_input_tests.cpp` | 快捷键与输入分发。 |
| `mesh_bake_cache_tests.cpp` | Ogawa 采样、区间钳制、替换、清除、PMM 配套文件往返、畸形记录、提交前回调取消、临时文件跨进程归属。独立 CPU/文件测试，注册 CTest。 |
| `vertex_capture_tests.cpp` | 隐藏 D3D9 HAL 设备上重放 VS 1.1/2.0/3.0，检查 GPU 位置回读、分块、状态恢复与不支持路径拒绝。注册 CTest。 |
| `alembic_export_tests.cpp` | 隐藏生产窗口内生成 PMX，核对骨骼动画、MME/MMV 叠加、生成与消失的材质/拓扑、Unicode ABC、取消、三语条件警告、烧录回工程、PMM 保存重开和清除后恢复。需要按模式准备 Data/插件，显式运行。 |
| `sequential_file_reader_tests.cpp` | 顺序文件读取、边界和回退。 |
| `d3dx_import_tests.cpp` | D3DX 导入与 ABI；需要本机 DirectX 运行库。 |
| `effect_api_tests.cpp` | 效果接口、对象导出及材质名称双向回退；无 MME 时安全跳过。 |
| `bone_projection_tests.cpp` | 骨骼投影与命中坐标。 |
| `bone_overlay_tests.cpp` | 骨骼显示/覆盖绘制。 |
| `coordinate_axis_tests.cpp` | 视口操作轴与坐标系标识：读取内嵌 `axis.x`，校验 X 轴为红色、Z 轴为蓝色、Y 轴为绿色、原点为黄色；针对三种语言 × 模型/相机两个面板 × 三种坐标系状态，渲染共 18 组标识，并校验标识居中、白字黑描边、措辞首字母大写，以及各面板第三状态的格位；校验中英日字体映射（英文与日文共用 Times New Roman，中文使用 Microsoft YaHei UI）；校验标识描边在 HUD 缩放为 2.0 时仍为 1 设备像素（八个描边四边形加一张白字面，共 9 个）；校验标识按 HUD 缩放取用 30 或 60 像素两档图集副本，且两档绘制的宽度一致；校验在没有配件时选择器上限为 2、标识回落为 Local；并校验视图轴随相机偏航变化。 |
| `background_overlay_tests.cpp` | 实际载入图片及 AVI 后的 GPU 像素回读，覆盖完整背景、纹理坐标、移动缩放及视频换帧；素材临时生成并自动清理。 |
| `model_alpha_tests.cpp` | 材质透明度与绘制行为。 |
| `model_alpha_occlusion_tests.cpp` | GPU 像素回读的透明叠层回归，覆盖固定管线与内置效果资源两条路径（效果路径含两个着色器变体）。场景含半透明双层叠加、两个模型与单模型内两种材质提交顺序互换、PMD 与 PMX 两种 ABI、PMX 双面材质、材质 Diffuse.a 由 1.0 变 0.5 的对照，以及几何交错模型的相机变换采样；并断言绘制后恢复调用方的 `D3DRS_ALPHAREF`。 |
| `transparent_material_order_tests.cpp` | 纯 CPU 的透明材质依赖排序与三角形分批，不建 D3D 设备、不启窗口。覆盖深度依赖排序与冲突循环的判定、退化与非法输入、三角形预算、以及用独立采样合成器对随机几何逐像素验证分批结果。 |
| `transparent_geometry_tests.cpp` | 纯 CPU 的透明几何排序，不建 D3D 设备、不启窗口。标准档按三角形中心深度稳定排序、以 ordinal 破同深度；低档用 64 个远到近计数桶，同桶保持输入顺序；两档都只重排，每个有效输入面恰好返回一次、顶点分量不变。分割回归在最高档运行，核对分片保留来源 draw/ordinal、分割顶点插值，以及同一屏幕点沿结果的深度不递增。 |
| `capture_downsample_tests.cpp` | 用独立面积重叠算法核验真实 D3D9 缩放像素；检查逐行水平相位、非整数比例及无效目标尺寸。 |
| `pmx_text_tests.cpp` | 缓冲与直接读取两种模式下的 98 个异常 PMX 文本样本；检查截断、奇数字节长度、过大前缀、贴图表清理、单字符及空名称回退。 |
| `model_edge_tests.cpp` | 轮廓宽度与镜头坐标空间、PMD/PMX 膨胀及 GPU 输入。 |
| `device_reset_tests.cpp` | 设备重置和资源恢复。 |
| `filename_encoding_tests.cpp` | 中文/日文路径与编码转换。 |
| `morph_import_tests.cpp` | 表情记录步长、Unicode 控件、VSQ 歌词/时间隔离；可另传 VMD 验证实际素材。 |
| `ui_language_tests.cpp` | 多语言字符串、控件语义、时间轴固定标题翻译、实际 PMX 导入后的 Unicode 绘制与中日文字号。 |
| `ui_message_tests.cpp` | 实际对话框资源和消息。 |
| `panel_key_tests.cpp` | 时间轴登记、编辑与帧链。 |
| `bone_scale_tests.cpp` | 原生缩放的坐标变换、斜切保留、层级继承、全骨骼类型、关键帧插值、撤销重做与剪贴板。 |
| `bone_scale_hud_tests.cpp` | 隐藏窗口的真实 GPU 回读：四个缩放按钮、相机和配件禁用蒙版、透明边缘及三档 DPI；不读取桌面。 |
| `bone_scale_io_tests.cpp` | PMM 缩放记录和单文件 VMD 扩展的编解码、校验、未知版本、帧偏移及骨骼匹配。 |
| `affine_skinning_tests.cpp` | 非均匀缩放和斜切的独立数值参考；BDEF、PMD、SDEF 的位置和法线、极分解及单位缩放旧路径一致性。 |
| `physics_bake_tests.cpp` | 烧录事务、撤销重做与三语隐藏对话框。另传 PMM 和输出目录时验证离线模拟、取消隔离、参数生效、范围、选择、回放与 PMM/VMD 往返；不修改输入素材。 |
| `physics_bake_settings_tests.cpp` | Bullet 默认值、严格 UTF-8 JSON、数值边界、Unicode、格式错误、原子保存及失败保护。 |
| `bone_registration_tests.cpp` | 骨骼选择、登记、首帧保护、移动、撤销/重做和跳帧。 |
| `window_interaction_tests.cpp` | 窗口消息、鼠标/选择行为。 |
| `pmm_reload_tests.cpp` | PMM 重载与状态恢复。 |
| `pmm_extension_tests.cpp` | PMM v2 尾部扩展的独立字节布局、CRC32、长度边界、重复键、未知记录与文件位置恢复；纯文件测试。 |
| `pmm_project_settings_tests.cpp` | 隐藏生产窗口中的实际 PMM 保存、加载、另存和新建，核对运算器类型、缺失类型回退自动、五档排序上限的菜单状态与单一档位字段、未知档位字节原样保留与显式替换、旧开关记录不参与档位判断及未知数据保留。显式传入测试产物目录，可附加 PMX 模型路径；需 D3D9 设备。 |
| `morph_runtime_tests.cpp` | 表情运行时计算和边界。 |
| `runtime_layout_tests.cpp` | 结构体布局相关行为、标准姿态骨骼遍历、稀疏轨道清理、配件、物理及全局帧。 |
| `mmdxshow_interface_tests.cpp` | 加载实际 DLL 验证 COM 子接口、推帧状态与析构。 |
| `physics_pose_tests.cpp` | 骨骼姿态与物理登记中的非有限数值防护：PMM v1/v2 读取与 `RegisterBonePoseAtFrame` 登记时把 NaN/Inf 位置与旋转修复为可用值且保留正常的非单位四元数，物理回写后异常刚体按跟随矩阵复位，超大关节距离用双精度算回并夹到半径界。含样例 PMM 的 23 条无效关键帧回归与修复前后对照入口。 |
| `pmx_joint_tests.cpp` | PMX joint 端点校验：刚体 A 缺失、刚体 B 缺失、双方缺失、索引越界、刚体表为空与刚体未创建时，该 joint 以约束号 `-1` 无效化并保留原始编号与名称；核对中日英三语警告标题与正文，以及关闭模型时只回收自己的约束、不误删其他模型的 0 号约束。 |

部分测试需要 Win32 窗口、D3D 设备或 DLL，不能将沙箱/硬件初始化失败视为功能通过。测试目标统一注册在仓库根目录的 `CMakeLists.txt`（`tests/` 下没有 CMakeLists.txt），部分目标受 `MIKUDANCESTUDIO_BUILD_*` 选项控制。`ctest --test-dir build -C Release -N` 列出当前构建配置实际注册的项目，README 不代替该清单：注册数量随 CMake 选项与源码变化，以该命令的输出为准。

## 手动验证与性能：按用途保留

| 标记 | 文件 | 依赖与清理条件 |
| --- | --- | --- |
| `integration-manual` | `scene_lifecycle_tests.cpp` | 需要本地 PMM 与素材；普通目标与 MME 目标共用同一份源码，MME 目标还需外部插件。`--startup-view` 在仅执行过 `InitDefaults`、窗口初始化与 D3D 初始化的状态下核对启动视点（相机面板、cameraPos = (0, 10, 0)、距离 -45、两个视图偏移清零、地面原点落在 `atan(10/45)`），并断言一次帧求值（`ResetAppState` + `ReloadModels`）不会改变世界矩阵与视图矩阵。`--playback-view` 验证选择模型后按 P 播放与停止时均保持自由视角，播放过程中的旋转、平移、缩放有效，并隔离残留的镜头骨骼目标与延迟刷新。`--view-tools` 验证以模型首骨骼或选中骨骼为旋转中心、绑定相机的平移与缩放及登记回读、骨骼批量变换与撤销重做。设置环境变量 `MIKUDANCESTUDIO_HUD_PREVIEW_DIR` 时，该模式另外把相机态与配件态的整帧写进该目录（`hud-camera-rows.png`、`hud-accessory-rows.png`），用于核对两块操作按钮行；不设置时不写文件。MME 目标的 `--material-names` 模式核对由插件实际导入的材质名查询结果。不得以“与单元测试重复”为由删除本测试；只有在替代测试覆盖真实加载、渲染、物理、编辑与重载之后，才可考虑合并。 |
| `benchmark` | `frame_benchmark.cpp` | 普通/MME 性能目标共用源码，结果依赖设备、模型、效果、配置。保留工具，过期结果可按清理清单 C07 归档。 |
| `hardware-manual` | `d3d12_interop_tests.cpp` | D3D9/D3D12 互操作验证，显式构建运行；当前不在 CTest 默认回归集。 |
| `hardware-manual` | `d3d12_skinning_tests.cpp` | D3D12 骨骼计算及正式 D3D9 缓冲桥接验证，需本地 PMD/PMX 素材及硬件设备；当前不在 CTest 默认回归集。 |
| `hardware-manual` | `skinning_backend_tests.cpp` | 隐藏窗口验证各硬件适配器、PMD/PMX 权重与附加 UV、缓冲对象身份、CPU 回退、运算器设置及三语言菜单。另核对自动测速排名、CPU 胜出、无效候选排除、校准复用，以及模型缓冲替换和删除后的失效。需 D3D9/D3D12 设备；显式构建运行，不在 CTest 默认回归集。证据见 [GPU 蒙皮记录](../docs/GPU_SKINNING_2026-09-25.md) 与 [自动选择记录](../docs/SKINNING_AUTOSELECT_2026-09-26.md)。 |
| `integration-manual` | `hidden_morph_tests.cpp` | 显式 `--model` 指定 Rou PMX 样本；隐藏生产窗口中验证时间轴隐藏表情的单击/双击/Shift、四栏隔离与普通权重/关键帧登记、组合动画的顶点/UV/材质结果。`--mode bounds` 独立验证内存面板值 0/5/255，不保存模型。需本机 D3D9 与 Data，不加入默认 CTest；仅在等效实际交互回归替代后移除。命令与证据见 [隐藏表情验收](../docs/HIDDEN_MORPH_PANEL_SAFETY.md)。 |
| `integration-manual` | `physics_scene_tests.cpp` | 传入现有 PMM 或 PMD 路径推进真实物理场景，逐帧断言全部骨骼姿态与刚体变换为有限值，覆盖跳帧与反复跳转；用于修复前后同条件对照，不注册进 CTest 默认回归集。 |

## 本地临时验证产物

### ABC 与网格烧录回归

构建 `alembic_export_tests`、`vertex_capture_tests`、`mesh_bake_cache_tests`、`mmv_fixture`、`mmv_dx12`、`mmv_compat` 和 `mmv_runtime_tests`。ABC 测试程序名保留为 `MikuMikuDance.exe`，位于 `build/tests/alembic/Release`；从各自独立运行目录执行：

```text
MikuMikuDance.exe <测试输出绝对路径> native
MikuMikuDance.exe <测试输出绝对路径> mme
MikuMikuDance.exe <测试输出绝对路径> combined
MikuMikuDance.exe <测试输出绝对路径> extra
MikuMikuDance.exe <测试输出绝对路径> varying
```

每个运行目录需要 `Data`。`native` 不放插件；`mme` 放 MME 的 `d3d9.dll`、`MMEffect.dll`、`MMHack.dll`；`combined` 再放正式 `MMV.dll` 与 `MMV` 资源目录。`extra`、`varying` 使用 `build/tests/mmv-fixture/Release/MMV.dll`，分别验证追加几何与逐帧拓扑出现/消失。夹具 DLL 只用于测试，不放入正式运行目录。测试自行隐藏窗口，不读取桌面画面。

2026-09-20 已在 Win32/x64 布局编译通过后删除临时 `build/layout-probe/`，同时清理 5 份旧 followup 日志，具体文件见清理清单 C08。正式布局约束保留在头文件断言中，测试源文件及现有测试构建目标全部保留（数量以实际文件与 CTest 清单为准，见上节）。之后产生的本地探针和迭代日志仍不应补加入 Git；清理前确认无需符号、失败日志和探针证据。

2026-09 全量审查复核（命令与结果见 [代码审查报告](../docs/CODE_REVIEW_2026-09.md)）：`ctest --test-dir build -C Release -N` 报 22 项注册测试，`ctest -C Release` 22/22 通过。`tests/*.cpp` 实际有 26 个文件，其中 22 个对应 CTest 注册目标，另 4 个是 `d3d12_interop_tests`、`d3d12_skinning_tests`、`frame_benchmark`、`scene_lifecycle_tests`（需设备或本地素材，不在默认回归集）。该轮同时发现 CTest 覆盖不到的两条相机/视图缺陷——`--playback-view` 与 `--view-tools` 在真实工程上失败，逐条见 [缺陷清单](../docs/CODE_REVIEW_FINDINGS.md)。

全工程候选与条件见 [清理清单](../docs/CLEANUP_INVENTORY.md)。新增测试时更新本表；不要把一次性调试输出留在测试断言之外。


## EMM 自动保存回归

`scene_lifecycle_mme_tests` 的输出仍名为 `MikuMikuDance.exe`。在独立运行目录放入同版本 `MMEffect.dll`、`MMHack.dll`、`d3d9.dll` 和 `Data` 后，执行：

```text
MikuMikuDance.exe <尚不存在的输出绝对路径前缀> 1 --emm-save-off [素材PMM绝对路径]
MikuMikuDance.exe <另一个新输出前缀> 1 --emm-save-on [素材PMM绝对路径]
```

两种模式在隐藏窗口内用真实保存菜单测试初始开关、关闭状态下打开工程后再开启、关闭时保留旧 EMM、再次开启时更新，以及中日文路径另存为。提供素材时须包含模型及可加载的效果；测试核对效果分配内容在重复保存与另存为后保持一致。测试只向新输出前缀写文件，不保存输入素材。每次运行须更换输出前缀。详细证据见 [EMM 自动保存修复记录](../docs/EMM_AUTOSAVE_2026-09-25.md)。

## 骨骼缩放物理回归

`affine_collision_shape_tests.cpp` 验证球、盒、胶囊在非均匀缩放和斜切下的解析支撑点、包围盒与源形状隔离。`physics_scale_tests.cpp` 验证三种刚体模式、继承偏移、joint 两端连接点和平移限位、复位、实际 GJK 碰撞及物理回写不重复缩放。`physics_bake_tests.cpp` 包含当前场景已缩放时的离线隔离和 PMM/VMD 往返。测试隐藏运行；仅在相关功能移除或完整迁移覆盖范围后删除。

## 密集透明累积专项

`model_alpha_occlusion_tests --low-alpha` 使用内置 SM3 资源 118，比较 256 层 alpha=1/255 和 512 层 alpha=2/255 的黑色面在白底上的结果与双精度解析式，容差 2，并核对最终 A=255。该模式和 `--peel-budget` 都自己选择 1024 层剥离档（五档格式下为「高」），不依赖进程默认档位。`--peel-budget` 使用该档位与 1152 层，检查预算耗尽后最近的绿色层仍显示。两项已注册 CTest；以 `MIKUDANCESTUDIO_TRACE_FILE` 日志核对实际走过分层路径，避免把能力不足时的直接回退误报为预算覆盖。背景窗口隐藏，像素来自目标读回。前后 build 证据见 [浮点合成修复](../docs/CLOUD_FLOAT_COMPOSITING_2026-09-26.md)。

三个专项均支持附加 `--msaa`，使用实际 4x 目标和同尺寸 resolve 后读回；硬件不支持时明确失败。MSAA 三项也注册 CTest。`--intersections` 同时覆盖不透明表面夹在两个透明表面之间，保证后方颜色不会混进前方透明层。


## 五档透明排序上限与 PMM v2

`pmm_project_settings_tests <新输出目录> [PMX绝对路径]` 使用隐藏的生产窗口，核对五档（低、标准、高、最高、关闭）、真实保存/重开/另存、单一档位字段、关闭覆盖旧档、菜单三语言、未知档位字节保留与显式替换、旧开关记录原样写回且不参与档位判断，以及 CRC 异常隔离。调用真实菜单、写入器和加载器；夹具直接通过扩展 codec 构造，不经新写入器预先迁移。

`model_alpha_occlusion_tests --maximum-limit` 复用黑色低 alpha 面，1152 层和 4608 层均以 alpha=1/255 混合，对照双精度解析式及目标 A=255。支持 `--msaa`，两种模式均注册 CTest。该最高模式使用 16×16 目标和与生产云 build 一致的硬件顶点处理，并断言设备实际标志；层数、几何和九个解析式采样点保留。`--maximum-full-size` 可执行原 96×96 压力用例，不用于默认 CTest。其他旧模式保持原尺寸及软件顶点处理。它检查最终像素；诊断日志另核对是否完成全部层，不能仅把预算数值相等当作正确性证据。

`frame_benchmark_mme` 的 `--sort-precision` 接受存档值 0..4，与档位编号一致：0 低、1 标准、2 高、3 最高、4 关闭。JSON 同时记录请求值与实际生效值，旧 build 对未知命令的忽略不能被写成已选择最高。`--verify-view-toggles` 兼容旧开关和新的单一档位；私有排序几何不要求原始索引缓冲发生改变，排序正确性由像素用例验证。背景采样方法和原始结果见[五档上限与 PMM 保存](../docs/TRANSPARENT_SORT_FIVE_TIERS_2026-09-27.md)。

## 烘焙通道与动态骨骼池

`bone_key_pool_tests` 和 `bone_registration_tests` 验证超过旧池边界的增长与登记。`physics_bake_tests --capacity` 验证大槽位、撤销和 VMD；`physics_bake_tests --capacity-scene <PMM或PMX> <新输出目录>` 运行真实场景和 PMM/VMD 往返，输出目录须为新的测试位置。

`channel_bake_tests` 不依赖 MMV，覆盖场景/模型通道选择、附件子参数、范围外曲线和中途提交失败恢复。`mmv_binding_animation_tests` 覆盖独立原生效果启停轨道；`mmv_ui_integration <PMX> <FX> <新输出目录>` 通过实际 DLL 和隐藏窗口核对三语、缩放、拖动和连续帧下拉框。`alembic_export_tests <新输出目录> native|mme|combined` 验证对应运行目录下的源网格与渲染缓存。实际执行结果及失败记录见 [验收说明](../docs/BAKING_CHANNELS_VALIDATION_2026-09-27.md)。

## MMDModern 查找优化验证

`lookup_cache_tests.cpp` 是保留的回归测试，覆盖纹理重载、资源销毁后地址复用、同名纹理、大小写、满池和稀疏绘制顺序。`effect_api_tests.cpp` 同时检查当前对象缓存的注册、重排和重复渲染失效。

`playback_benchmark.hpp` 由 `frame_benchmark.cpp` 引用，仅用于后台测评。`--playback-seconds 15 --json <路径>` 启动正常播放并记录逐帧 Present、绘制提交和播放游标。此计数方式只在 MME 设备代理上验证通过；系统原生 D3D9 会切换调用表，该路径返回无效结果，不进入 FPS 汇总。删除后台播放模式时一并删除该头文件。

`tools/run_lookup_compare.py` 对封存的前后 EXE 交替采样。所有性能结果保留原始日志、JSON、CSV、资源哈希及失败样本。具体测评范围与结果见 `docs/MMDMODERN_PORT_20261010.md`。

低 CPU 预算对照同时传入 `--cpu-percent 2 --logical-cpus 4,6`。百分比按全机 CPU 预算计算；逻辑核心编号须先核对当前机器。`tools/limited_process.py` 挂起创建测试进程，设置并回读 Job Object 硬上限和亲和性后恢复执行。它只管理新建的测试子进程，记录进程及全系统 CPU 时间；显卡共享负载另存 `*-gpu.csv`。限制不预留核心，不限制 GPU，也不改变正式程序的运行设置。该辅助模块随资源限制测试入口保留或一并删除。

本轮后续按用户要求固定 `--cpu-percent 2 --seconds 45 --logical-cpus 4,6`。长采样统一排除前 5 秒，并要求余下首末帧区间达到目标区间的 90%，即至少 36 秒。原始 15 秒短采样保留，不与长采样混算。资源限制工具在退出或超时时清理自身作业中的子进程，不留下后台构建进程。

用户要求同时比较两个 MMS 版本时，使用 `tools/run_lookup_simultaneous.py`。此脚本仅用于该并行测评：前后版本分别限为 1% CPU，合计预算为 2%；分别使用逻辑处理器 4、6，并在四轮中交换核心。两进程都完成限制回读后由同一启动屏障放行。每轮采样 90 秒，排除前 5 秒后逐版本计算帧率；保留各自绘制计数、CPU 记录和全显卡共享负载。采样重叠区间根据结果文件完成时间估算，不能视为逐帧同步。删除并行测评入口时一并删除此脚本。

用户要求直接用正式程序载入工程时，使用 `tools/run_actual_project_compare.py`。传入 `--before`、`--after` 两个已校验的正式 EXE，`--runtime` 正式运行文件目录，`--mme-runtime` 相同 MME 文件目录，及 `--scene`、`--output`。默认两侧各限 1% CPU；用户明确取消上限后传入 `--uncapped`，同时取消固定核心。默认四轮，每轮在两侧开始正常播放并预热 10 秒后，共同采样 90 秒。

该入口限定本报告的两份 EXE 哈希，只读状态偏移也与这两份程序绑定。它通过程序原有的工程命令行与播放处理函数运行隐藏窗口，不修改 EXE、不注入 DLL、不写进程内存。正常帧循环日志提供完成 `FrameDriver` 的次数，约每 100 毫秒读取一次，按首末完整日志批次的观察时间计算频率。该指标不能声称是成功 Present 数、桌面显示 FPS 或逐帧耗时分位数。需要这些指标时应另行验证可用的 Present 采集器。

每轮保留相同运行资源哈希、加载模块、播放游标、视口、场景设置、CPU 记录和 GPU 共享负载。测试结束仅关闭自身作业中的进程，不保存输入 PMM。`limited_process.run` 的可选 `stop_event` 用于这种外部观测结束后的清理；传入 `None, None` 作为 CPU 和核心参数时，回读确认作业没有 CPU 速率控制，并保留继承的处理器亲和性。删除正式程序对照入口时一并删除该脚本。
