# MME 兼容与分配记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [MME 对象登记与效果分配验证（2026-09-17）](#record-mme-compatibility)
- [原生 MME 重新分配后模型消失的补充诊断](#record-mme-global-assignment)
- [GoldBoom 原生 MME 同名效果重分配诊断](#record-goldboom-mme-reassign)
- [EMM 自动保存修复（2026-09-25）](#record-emm-autosave)
- [MMV 网格登记时的 MME 后处理崩溃修复](#record-mme-registration-crash)

<a id="record-mme-compatibility"></a>
## MME 对象登记与效果分配验证（2026-09-17）

原始记录：`MME_COMPATIBILITY.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-mme-compatibility-材质名双向回退与时间轴编码2026-09-20"></a>
### 材质名双向回退与时间轴编码（2026-09-20）

名称语言仍由原有优先设置决定。首选名称为空时使用另一种已有名称：只有
日文名时保留日文，只有英文名时保留英文；两种都有则使用首选，两种都没有
才沿用 MME 的默认 `subsetN`。中文界面不再因为 MME 读取英文栏而丢失材质名。

MME 自己调用 `MMHack.dll!GetMaterialName` 并缓存两套 UTF-16 名称，宿主的
37 个导出没有材质名接口。因此兼容层在首次 `BeginScene` 登记对象前，按
已加载 PE 的导入表与公开导出名称定位这一条调用，为空名称补一次反向查询。
使用 `IMAGE_IMPORT_DESCRIPTOR` / `IMAGE_THUNK_DATA` 等具名 PE 结构，不使用
版本专用 RVA、私有对象偏移或指令补丁；不修改 DLL 文件、PMX 文件或名称数据。
返回值仍由插件持有，不复制到临时 ANSI 缓冲区。任一 DLL 未加载、导出/导入
缺失或导入已被其他组件替换时直接跳过，不强制加载插件。

本机 x64 二进制依据：MMEffect 在 `+0x58FC9`、`+0x5903B` 分别读取两套名称；
`+0x41127` 判断首选名称为空后直接生成默认标签，没有尝试另一语言。
MMHack `GetMaterialName` (`+0x1130`) 接收对象 ID、材质索引、语言标志，返回
插件持有的宽字符串或空指针。对象 ID 来自 `ExpGetPmdID` 的 32 位值，不能
把宿主模型完整指针或索引缓冲指针直接当作此 ID。以上地址仅记录分析证据，
不进入产品实现。

时间轴根骨骼、普通骨骼和表情改为使用模型原始 Unicode 名称，避免读取
`FrameGroup` 中截断的窄字节镜像。旧式分组名及固定日文标签按 CP932 解码；
配件名称继续沿用其 ACP/带标记 UTF-8 的既有格式。界面语言不再改变数据的
解码方式，英文模式下回退到日文仍正常显示。

验证覆盖：

- `effect_api_regressions`：日文独有、英文独有、双语、空指针/空字符串、
  Unicode、合法的 `subset0` 自定义名；不覆盖非空首选，未加载 DLL 时不强制加载。
- `ui_language_tests`：实际 PMX 导入后调用时间轴绘制，分别将中/英/日及再次
  切回英文的根骨骼、骨骼、表情和分组像素与原始 Unicode 名称的 GDI 结果对比。
- Release 正式程序重建成功，完整 CTest 回归 20/20 通过；无 MME 的普通测试
  宿主 `--menus-only` 初始化、语言切换和名称兼容安全跳过验证通过。
- 真实 MME 测试宿主运行 `Stage.pmm 3 --material-names`：每轮 11 个模型、
  15 个配件，渲染及物理更新后核对 MME 实际导入调用；每轮 76 个有名称材质，
  其中 72 个仅有日文名，三轮均通过并正常清理退出。英文独有情况由单元测试
  覆盖，当前舞台没有该样本。`StageNoRay.pmm` 未完成模型加载，不计入通过结果。

实测 DLL SHA-256：MMEffect `74b3f5882a3bad28131cbc57994e2f960938e6ca850fc7139018ce4822bd87dc`；
MMHack `e450b25e5cd2bcf125ca4b8eb9e57f858ec3ca379633c7d25f9dd40e864b390d`。
当前证据覆盖这组 x64 DLL；不代表其他版本插件或 Win32 插件已经实测。

<a id="record-mme-compatibility-mme-主菜单汉化2026-09-20"></a>
### MME 主菜单汉化（2026-09-20）

宿主为 MME 主菜单的 7 个命令提供中文标题，在首次创建菜单、切换语言和
展开菜单时同步。按命令 ID 更新 `MIIM_STRING`，保留菜单项的勾选、禁用状态、
快捷键及未知扩展项；原有英文 Enable Effect 命令 ID 修正仍然有效。

翻译入口先用 `GetModuleHandleW` 确认 `MMEffect.dll` 已加载，再查找当前窗口的
有效菜单；DLL 未加载或菜单尚未创建时直接返回。不会为汉化加载 DLL，也不读取
DLL 内部地址或修改 DLL 文件。语言切换后重新查找菜单，不缓存已销毁的菜单句柄。

验证：`effect_api_regressions` 覆盖无 DLL 跳过、原生命令状态和三语标题；
`scene_lifecycle_tests --menus-only` 与带本地 MME DLL 的
`scene_lifecycle_mme_tests` 目标（输出名 `MikuMikuDance.exe --menus-only`）
分别完成无 MME / 有 MME 的实际初始化、六次语言切换和缺失菜单检查。
后两项需要可用的 Direct3D 设备，不需要用户模型或工程文件。

<a id="record-mme-compatibility-问题与修复"></a>
### 问题与修复

MME 已能初始化，但效果分配窗口只有 `(default)`。原因不是缺少
MSVCR90：MMHack 的 x64 对象 ID 仍为 32 位，它通过创建资源时传入的
**输出槽地址**建立 ID 到完整对象地址的映射。

- PMX 原先将 `CreateIndexBuffer` 的输出写入栈上变量，再复制到模型。
  现在直接使用 `ModelRecord::indexBuffer` 的引用，保持输出地址稳定。
- 配件的 `D3DXLoadMeshFromXW` 同样直接写入 `AccessoryRecord::mesh`。
- `ExpGetPmdID` / `ExpGetAcsID` 明确返回地址低 32 位，与参考程序一致。
- 每帧进入 `BeginScene` 前清空登记标记；只放行本帧 MME 查询过绘制顺序的
  对象，避免删除、跳过或复用模型序号后使用失效的登记状态。
- 现代 CRT 打开文件不会经过宿主的 `CreateFileW` IAT。PMM 和模型加载前，
  通过宿主公开文件接口做一次只读打开/关闭，使 MME 得到工程路径和模型宽路径。
  不载入旧 CRT，不修改第三方 DLL，不依赖其内部 RVA。
- 文件名导出恢复参考程序的 CP932 → renderer 显式 locale 回退，修复
  `星穹铁道—遐蝶.pmx`、`镰刀mic.pmx` 在对象列表中名称为空的问题。

验证正常退出时另外发现 `DisposePhysicsWorld` 仍用 x86 Bullet 私有
数组偏移和虚表槽位。现通过 Bullet 2.75 的类型化接口清除约束、刚体与
世界组件，遵循已有 `ModelDispose` 的所有权约定。原来的模型初始化
修复与未登记对象绘制保护均保留。

<a id="record-mme-compatibility-二进制依据"></a>
### 二进制依据

所有地址都是相应映像的 RVA，映像基址均为 PE 文件中的首选基址；
运行时 ASLR 地址不参与产品代码。

| 参考 | SHA-256 | ImageBase |
| --- | --- | --- |
| 本机 MMHack.dll | `e450b25e5cd2bcf125ca4b8eb9e57f858ec3ca379633c7d25f9dd40e864b390d` | `0x180000000` |
| 本机 9.26 CHS x64 MikuMikudance.exe | `72c34b7ab607aef7b18974742ae18ce62f115f6a614bbedc1a4310e4a5406f72` | `0x140000000` |

- 参考 exe `0xDB91C` / `0xDBACC`：`mov eax,dword ptr [...]`，确认 ID 返回宽度。
- MMHack `0x3459`：`mov r14d,eax`；`0x3488` 以该 ID 调 `0xF2B0`
  恢复地址，然后扫描前 8 个指针槽。
- MMHack `0x2D40` 为索引缓冲创建钩子；`0x2DA8` 将输出参数地址交给
  `0xF150`。后者登记输出槽及其之前 7 个指针槽地址的低 32 位。
  配件加载钩子在 `0x440D` 做相同登记。
- 参考 exe `0xA6FA0..0xA71D0`：文件名先用 CP932，再逐个调用
  `_wcstombs_s_l`，使用 renderer 的四个 locale；不依赖进程的日语 locale。
- 退出崩溃捕获于修复前宿主 `+0x42C0`：
  `mov rdi,qword ptr [r14+rbp*8]`，对应旧 `DisposePhysicsWorld`
  从 `world+8/+16` 读取错误的 x64 碰撞对象数组。

<a id="record-mme-compatibility-实测结果"></a>
### 实测结果

测试素材为本机 `xd.pmm` + `xd.emm`，未纳入版本管理，原文件未改动。

- PMM SHA-256：`352ab40a8fe702ecfe73ea00d5e1cf57e14fd510092f1dce5d355dfe45974e0f`
- EMM SHA-256：`fb650e577cd3811a5c2c5c1d0bc72976619c68ca03d78ec3de4e716b34af8136`

Fit 后 MMHack registry 元素数 **6**；读取效果窗口控件 `1003` 的结果：

| 对象 | 主效果 |
| --- | --- |
| 星穹铁道—遐蝶.pmx | Shader_Main.fx |
| 镰刀mic.pmx | (none)，与 EMM 一致 |
| PSController.pmx | (none)，与 EMM 一致 |
| ExcellentShadow.x | ExcellentShadow.fx |
| AutoLuminousP.x | AutoLuminousP.fx |
| DiffusionP.x | DiffusionP.fx |

实际着色验证使用工程副本及独立 EMM，将第一个模型改配无贴图依赖的
洋红色 pixel shader（`object` / `object_ss` techniques）。模型实际显示为
洋红色，视口检测到 45,447 个近纯洋红像素，证明分配后的 shader 参与绘制。
副本、测试效果、截图和 Win32 诊断脚本均留在被忽略的 `build/verify-crash/`。
初次验证时，直接发送 MME 菜单命令 `40020` 未改变该版本的勾选状态，
当时未将“开关截图”当作渲染证据。后续修复和实际开关验证见下节。

有/无 MME 的 Fit(687)、Skip(689)、Cancel(690) 均完成无致命错误检查，
WM_CLOSE 正常退出码为 0。Skip 另以 30 秒观察处理后续模型提示；Skip/Cancel
用于验证放弃模型或装载后的清理安全，不作为 6 对象完整场景的登记证据。

Release 构建及 `effect_api_regressions` 通过。回归覆盖 32 位 ID、模型和配件
绘制顺序、未登记保护、逐帧失效、序号复用、中日文文件名、含刚体/约束的
物理世界退出清理。测试目标 `EXCLUDE_FROM_ALL`，不随发布安装。

```powershell
cmake -S . -B build -DMIKUDANCESTUDIO_BUILD_EFFECT_TESTS=ON
python -c "import os,subprocess; raise SystemExit(subprocess.call(['cmake','--build','build','--config','Release','--target','effect_api_tests'],env=dict(os.environ)))"
ctest --test-dir build -C Release -R effect_api_regressions --output-on-failure
```

实测覆盖当前 x64 DLL 和上述素材；未声称所有第三方效果或 x86 运行时已验证。
实现保持命名字段访问和原有双 ABI 布局，不把 x86 裸偏移带入 x64。

<a id="record-mme-compatibility-ray-工程重载与语言切换2026-09-17"></a>
### Ray 工程重载与语言切换（2026-09-17）

<a id="record-mme-compatibility-修复"></a>
#### 修复

- MME 英文菜单资源 107 把 Enable Effect 编号写成 `40020`；日文资源 101、
  DLL 的窗口过程（`MMEffect+0x55BF2`）及 Ctrl+Shift+E 钩子
  （`+0x5585B`）实际使用 `40006`。宿主在 MME 初始化后修正运行时菜单项 ID，
  继续让 DLL 自己处理开关。通过标准窗口 subclass，在语言命令 260 前保存
  勾选状态，待 DLL 重建菜单后恢复；不改 DLL 文件或内部变量。
- `LocalizeUI` 原先仍写 `model+12740`、读 `model+8776/8826`。
  x64 上前者落在 `boneListRowType[104]`，后两者也不是模型名称。
  改用 `physicsFlags`（历史字段名，实际用于模型消息的语言选择）和
  `name/nameEn`。这修复的是确定的模型元数据/名称访问错误，不能单凭它
  认定第三方场景的发白原因。
- 加载对话框会运行嵌套消息循环；原有 `windowLayoutReady=0` 可被
  `WM_SIZE` 等消息重新设成 1，导致半加载的对象进入 FrameDriver/MME。
  PMM 加载和新建重置现在使用可嵌套的 `ScopedSceneMutation`；场景处于
  替换过程中时，帧驱动和窗口绘制都跳过场景访问，退出作用域恢复原先
  的布局门并请求重绘。计数放在运行时对象中，不改变 PMM/模型 ABI 布局。
- 将 x64 D3DX 导入库依赖随 core 传播，修复键盘回归目标缺少导入库的链接错误；
  两个回归程序都输出到独立 tests 目录，避免加载 Release 中的 MME 代理。

<a id="record-mme-compatibility-验证素材与边界"></a>
#### 验证素材与边界

使用用户以旧版 MMD 创建的 `lty.pmm` 和同名 `lty.emm`；实际重复测试使用
`build/verify-crash/lty-fixture/` 下的副本，原文件未保存或修改。

- PMM SHA-256：`bb0102f1f13404fcdd8ca2f72105c6758499ef41143119d234b758ffe3081766`
- EMM SHA-256：`127c2ba44206f78c1e84aecbddd49d4a6ebb4a5a180647b2b376c70f43123a8b`
- 场景包含 5 个模型、8 个配件，以及 Ray、WorkingFloor、AutoLuminous 等效果。
- 修复前发布版的隔离副本：在模型结构差异对话框暂停加载，发送
  `WM_SIZE`/`WM_PAINT`，MMHack 对象数从 0 提前变为 **1**；加载完成才变为 13。
  修复后相同操作保持 **0**，加载完成后一次登记 **13**。这直接验证了重入路径
  及保护效果；本次对照没有复现致命崩溃，不能把推测的崩溃栈写成已捕获证据。
- MME 开关实测为 `8 → 0 → 8`；关闭后的画面回到无 Ray/后处理的模型着色，
  重新启用后恢复。三次语言切换分别保持关闭、开启、关闭状态。
- 连续三轮“新建 → 菜单重载”，每轮加载中追加上述重绘压力；对象数均
  `13 → 0 → 13`，未出现 Fatal 对话框，最终正常退出码 0。
- 另测“先启动空程序 → 菜单读取 → 不新建直接覆盖读取”，包含加载中重绘；
  两次完整加载后均为 13 个对象，正常退出码 0。
- `effect_api_regressions` 覆盖菜单命令、重建菜单后的实际开关状态、语言切换
  不破坏模型行元数据/名称、嵌套加载保护、既有对象 API 和物理清理；
  `keyboard_input_regressions` 同时通过。

第三方 F 盘 `Stage.pmm` 不在本机，未对该工程复测。上述结果不等同于已排除
所有 Ray 素材组合的发白或重载故障；保留 MME 开关可供进一步隔离定位。

<a id="record-mme-global-assignment"></a>
## 原生 MME 重新分配后模型消失的补充诊断

原始记录：`MME_GLOBAL_ASSIGNMENT_20261011.md`。其中的版本、日期和验证范围沿用原记录。

用户报告，在 MMS 的 MME 效果分配窗口给一个 PMX 重新选择 FX 后，所有带 FX 的 PMX 消失。未分配 FX 的模型和坐标网格仍可见。其他 PMX 和 FX 也能触发。点击文件框的「打开」后立即消失，关闭分配窗口、切换相机模式均不恢复；「重载全部效果」可以恢复。

用户已在正式程序中重复复现，本次已保存异常和恢复状态的只读快照。独立诊断的正常分配操作尚未复现持续消失；新增故障注入和恢复对照确认了一个 MME 动画纹理重建缺陷。针对该缺陷的修复已通过本文记录的自动验证。2026-10-11，用户用 `ef7abe1` 候选版重复原先的分配操作后反馈「正常了」。此前的 [GoldBoom 诊断](compatibility.md#record-goldboom-mme-reassign) 仍只代表其已覆盖的条件。

<a id="record-mme-global-assignment-运行环境核对"></a>
### 运行环境核对

用户此前运行的程序为 `C:/Users/lq/Desktop/新建文件夹 (2)/MikuMikuDance.exe`。该文件与 `MMS/build/Release/MikuMikuDance.exe` 均为 4,748,288 字节，SHA256 为 `f1b2e4be4d20de3cbade3118511492a7f867af6e3e5a86b5c264436934b6bddb`。

现场模块检查确认加载了原生 MME 的 `d3d9.dll`、`MMEffect.dll` 和 `MMHack.dll`，没有加载 `MMV4MMS.dll`。这三个 DLL 与 `refs/MME` 中的文件一致。文件核对记录保存在本地 `installation-hashes.json`。

<a id="record-mme-global-assignment-现场对照"></a>
### 现场对照

进程 105924 保存了模型模式异常、切换相机仍异常和重载后恢复的快照。随后用户重新启动同一程序，进程 111268 再次出现相同故障。在第二个进程中，读取范围增加了 MME 自有的模型包装、材质绑定、FX 包装、technique 数组和原生日志。每次均先核对正式 EXE 与 MME 文件哈希，只以查询和读取权限打开进程，没有执行目标函数、写入内存或改变窗口。

第二个进程的完整对照为 `live-111268-1791689016099121900`（异常）与 `live-111268-1791689102280653000`（重载恢复）。目前证据如下。

- 三个模型仍已加载，顶点与索引缓冲区存在。对象注册持续正常。
- 两份非空 FX 路径仍有有效的效果对象，材质绑定未丢失，technique 数量及空值分布不变。FX 的执行错误标记为零，没有读取到编译错误文本。
- 宿主五个延迟创建的渲染资源在第二个进程的异常和恢复状态中都为空。这不能解释效果消失。
- 1×1 视口来自最小化后零客户区的布局计算。它在异常和正常快照中都出现，首次异常也曾保有正常尺寸，不能据此认定根因。
- 异常前记录了一次成功的设备重置。MME 日志含「Resetting MME...」「done.」，没有错误信息。两份 FX 的编译模板缓存引用在异常时为空，重载后重新建立；静态分析确认设备丢失处理会清除此引用，因此该差异本身还不能证明缺陷。

堆快照按已核实的 MME 版本布局读取，设有 2 MiB 请求总量及节点数上限。它是非原子快照；头部复采只能发现部分并发变化。地址、帧内当前材质和资源引用变化不能单独视为损坏。

<a id="record-mme-global-assignment-诊断覆盖"></a>
### 诊断覆盖

`tests/mme_reassign_tests.cpp` 仍链接生产核心，调用原生 MME 的分配、编译和绘制。新增选项如下。

| 选项 | 覆盖条件 |
| --- | --- |
| `MMS_MME_SECOND_MODEL` | 加载第二个文件名不同的 PMX，只重新分配第一个模型 |
| `MMS_MME_MODEL_EDIT=1` | 模型操作模式，默认值为相机模式 |
| `MMS_MME_SWAPCHAIN_BACKBUFFER=1` | 在真实 `Present` 前读取交换链画面 |
| `MMS_MME_REASSIGN_REPEATS=1..20` | 全部重载前连续重新分配，默认 1 次 |
| `MMS_MME_PICKER_FRAMES=0..8` | 注入文件选择尚未返回时绘制，默认 0 帧 |
| `MMS_MME_OFFSCREEN_REAL_PICKER=1` | 配合 `MMS_MME_HIDDEN_DEFAULT=1` 和 `MMS_MME_REAL_PICKER=1`，在 Default 桌面屏外驱动真实文件框 |
| `MMS_MME_RESET_PROBE=before/picker/after/drawn` | 在第一次注入分配的指定位置执行一次生产设备重置，`drawn` 在第一次分配后已绘制一帧时执行，仅适用于交换链捕获 |
| `MMS_MME_REENTRANT_PAINT=1` | 外层 BeginScene 成功后一次性发送真实 WM_PAINT，记录嵌套帧及场景调用结果 |
| `MMS_MME_DEFAULT_POSE=1` | 保留控制骨骼的默认姿态，不设置测试用位移 |
| `MMS_MME_APNG_TRACE=1` | 观察已验证 MME 动画纹理的 Update、OnLost、OnReset 及帧缓存字段 |
| `MMS_MME_APNG_ZERO_NEW_TEXTURE=1` | 仅在 drawn 重置中清零测试进程最多两个新建动态纹理，须同时启用 TRACE；排除真实文件框和重入模式 |
| `MMS_MME_APNG_INVALIDATE_AFTER_ZERO=1` | 清零成功后将旧帧缓存设为树哨兵，作为独立恢复对照；不主动调用 Update |
| `MMS_MME_APNG_CONTRACT_TEST=1` | 通过实际生产回调验证 10 项调用边界，须同时启用 TRACE 且生产修复已安装 |
| `MMS_MME_EXPECT_APNG_COMPATIBILITY=0/1` | 断言生产兼容修复是否安装，并检查重复安装的幂等性 |

命令行的自阴影参数扩展为 `0`、`1` 或 `2`，省略时仍为 `0`。运行日志记录每个模型的当前对象编号、显示状态、缓冲区、共享效果、重复绘制计数、渲染目标和深度表面。

原测试替换了渲染目标，新选项保留交换链、深度、视口及宿主缓存。实际交换链为 1463×914、8 倍多重采样。诊断仅把画面解析到未绑定的同尺寸表面，再回读像素。该读取会同步 GPU，不能用于性能测量。

本次各组均设置 `MMS_MME_REASSIGN_REPEATS=3`，每组保存 9 张画面，包含分配窗口内、窗口关闭后、重复分配、全部重载和再次分配。默认重复次数为 1，对应 7 张画面。分配后的空画面会保留，并继续执行重载。程序输出图像差异，但退出码 0 只说明诊断完成，不代表用户问题已经解决。

真实文件框已在 `real-camera-swap-05`、`real-model-swap-01` 和 `real-original-model-01` 中完成。前两组为两个不同颜色的顶点和像素着色器，后一组为原始 GoldBoom 动画纹理 FX。每组真实选择四次，均未出现持续消失。纯色组九张 RGB 画面完全相同；原始 FX 组仅个别像素相差一个色阶。

此前 `real-camera-swap-01` 至 `04` 是窗口保护失败，未完成真实文件框测试。失败记录保留。`05` 的保护自检覆盖创建、SetWindowPos、MoveWindow 和 NOSENDCHANGING；另处理 Windows 还原窗口位置及 GDI+ 隐藏辅助窗口，继续审计窗口是否位于屏外、是否可见或成为前台。

真实文件框日志曾出现 `visible=1`，当时窗口仍位于屏外且非前台，随后定时器隐藏了窗口。因此不能概括为「窗口始终隐藏」。

`reentrant-model-01` 确认生产 FrameDriver 实际重入，内层 BeginScene 返回 `0x8876086C`。首帧画面发生变化，后续帧自动恢复并与基线一致。该合成触发没有复现持续消失，也不能证明真实文件框曾发生同样的重入。

`reset-drawn-original-model-01` 与 `reset-drawn-default-pose-01` 在原始 FX 已绘制后执行健康设备重置，也未复现持续消失。实际控制骨骼查找结果为 `-1`，两组都保留了默认姿态，不能把它们描述为不同动画时间的对照。素材的 PMX、FX 与 PNG 已和用户目录逐字节核对一致。

<a id="record-mme-global-assignment-操作入口审查"></a>
### 操作入口审查

对本机 `MMEffect.dll` 的资源和静态分支检查确认，文件选择按钮 `1007` 与菜单命令 `40007` 均进入同一设置效果函数。该函数直接读取列表的选择项。测试设置列表选中状态，随后调用 `40007`，覆盖相同分配入口。

关闭命令 `40011` 与确认按钮进入同一提交并销毁窗口的入口。取消按钮另有回滚分支。本次检查的是确认分配，不覆盖取消后的行为。

<a id="record-mme-global-assignment-动画纹理生命周期对照"></a>
### 动画纹理生命周期对照

现场两份效果都使用 GoldBoom.png。该文件包含 101 帧 512×512 图像，解码帧总量超过 MME 的预加载阈值。恢复状态的只读堆快照确认它们使用动态纹理，当前时间为零，缓存指向第零帧。

对 SHA256 为 `74b3f5882a3bad28131cbc57994e2f960938e6ca850fc7139018ce4822bd87dc` 的 MME 0.37 x64 分析发现：APNG OnLost 释放动态纹理，OnReset 创建新纹理，却保留旧帧节点；随后同一时间的 Update 因节点未变而跳过上传。使缓存失效的正确值是树哨兵，不能写空指针。原函数、构造器及调用路径的静态证据保存在 `MMEffect-APNG-lifecycle-evidence.txt`。

三个独立运行都使用原始素材和 NVIDIA GeForce RTX 4070 Laptop GPU，结果如下。像素比较只比较 RGB，忽略捕获表面的 alpha。

| 运行 | 条件 | 结果 |
| --- | --- | --- |
| `apng-trace-reset-01` | 正常重置，仅观察 | 两次 Lost/Reset 成功；新纹理仍沿用旧帧节点。画面基本不变，不能据此认定内容已重新上传。 |
| `apng-zero-reset-01` | 新纹理清零，保留旧节点 | 两张纹理各清零 1,048,576 字节。后续绘制、关闭分配窗口后均有 618,306 像素变化；重新分配只恢复主模型，全部重载才完全恢复。 |
| `apng-zero-invalidate-01` | 相同清零，再设缓存为树哨兵 | 下一次正常 Update 恢复内容，窗口内稳定画面与关闭后画面均与基线 RGB 完全相同，无需重载。其余捕获最多 10 个像素相对基线相差一个色阶。 |

这组对照证明 MME 会在资源重建后漏传当前动画帧。正常重置不总会显示空白，不能把新纹理的可见内容当成已经上传的证据。清零是测试故障注入，不是对用户进程的操作，也不是对用户完整触发过程的自动复现。

候选修复只在成功的动画纹理重建后让当前帧缓存失效。它不清零纹理、不主动绘制、不自动重载全部 FX，也不增加逐帧纹理复制。

<a id="record-mme-global-assignment-候选修复验收"></a>
### 候选修复验收

`src/exports/effect_apng.cpp` 在初始 D3D 设备创建后安装一次。它核对 DLL 大小和完整 SHA256、PE 标识、188 字节 OnReset 代码及五个虚表入口，然后仅替换 OnReset 槽。模块引用保留至进程退出，不修改磁盘 DLL。文件或内存签名不同、已有其他钩子、页保护操作失败均不启用修正。

回调保留原函数的调用次数、HRESULT 与 LastError；仅安装线程、`S_OK`、动态模式、有效帧和纹理、合法树哨兵才使缓存失效。已有纹理地址可能重用，因此不以地址改变作为成功条件。恢复页保护失败时尝试仅撤回自己的槽位；残留回调也只转发原函数，不启用修正。

- `apng-production-zero-01` 关闭测试恢复开关，仅靠生产修复。两张新纹理清零后，稳定画面、窗口关闭后及后续画面恢复；相对基线最多两个像素相差一个色阶。
- 同一运行的 10 项真实回调测试全部通过：同地址非空纹理、Reset 失败、非零成功值、预加载模式、零帧、空纹理、错误哨兵、空哨兵、错误类、其他线程。每项检查原方法恰好调用一次、返回值、LastError、缓存及纹理保持约定。
- `apng-production-unknown-01` 仅在独立 DLL 副本的 DOS 保留区修改一个字节。生产哈希校验拒绝安装，五个虚表槽仍为原地址；相同清零后持续缺失，重载恢复。这是版本拒绝的负向对照，不是测试失败。
- `apng-production-real-original-01` 使用真实文件框和原始 GoldBoom 素材，四次文件选择完成，未出现持续消失；相对基线最多四个像素相差一个色阶。
- `apng-production-real-shader-01` 使用不含动画纹理的两个纯色着色器和真实文件框，九张 RGB 画面完全一致。
- 效果接口、窗口模式设备重置成功／失败、可选图形导入共四项回归通过。

静态交叉审查未发现阻断项。没有执行所有仓库测试；本次测试针对新增兼容层及其初始化、重置和分配路径。

<a id="record-mme-global-assignment-证据与限制"></a>
### 证据与限制

本地证据目录为 `MMS/.local/artifacts/mme-global-assignment-20261011/01`。`validation-manifest.json` 记录各次运行的参数、退出码、日志和每张画面的完整哈希及 RGB 哈希。原始日志和失败样本均保留。

测试仍使用静止帧和固定视角。屏外窗口、禁止激活及自动选择会改变前台交互时序。真实文件框模式不等于私有桌面隔离，也不能证明前台人工操作没有问题。CBT 保护限定诊断主线程，窗口审计不能覆盖两个采样之间的全部行为。

本次未进行性能基准。用户已确认修复后原先的重新分配操作恢复正常。这一结论适用于本次已验证的 MME 版本和素材，不能推及所有第三方效果。修复未改变同名 FX 的查找、读取或重载顺序；现场效果对象及材质绑定仍在，问题位于设备重置后的动画纹理内容恢复。

<a id="record-goldboom-mme-reassign"></a>
## GoldBoom 原生 MME 同名效果重分配诊断

原始记录：`GOLDBOOM_MME_REASSIGN_20261011.md`。其中的版本、日期和验证范围沿用原记录。

用户确认从 MMS 的 MME 效果分配窗口，给 PMX 重新指定同一个 FX。本次未复现画面异常，未修改生产渲染代码，也没有完成用户问题的修复验收。

<a id="record-goldboom-mme-reassign-覆盖的操作"></a>
### 覆盖的操作

诊断目标 `mme_reassign_tests` 链接 MMS 生产核心，并加载 `refs/MME` 的真实 `d3d9.dll`、`MMEffect.dll` 和 `MMHack.dll`。素材复制自 `D:/MMD-Next/tests/mme/GoldBoomEX`，避免更改原文件或分配记录。

程序依次加载 GoldBoom.pmx 的伴随效果，在 MME 原生列表中选择 PMX 根行，通过设置效果命令重新选择 GoldBoom.fx，关闭窗口，执行全部重载，再次分配。补充采样包括效果窗口仍打开时完成 1 次和 8 次显式渲染循环后的画面，以及关闭窗口后完成 1 次循环的画面。消息队列中的 WM_PAINT 也可能触发绘制，因此这些采样不能严格证明覆盖实际首帧。

每次回读实际 640×480 渲染目标，保存 BGRA8 文件。程序输出 RGB 差异但不把图像相同当作内置断言；外部清单同时比较完整 BGRA 文件哈希。真实 PMX 的已加载骨骼表没有 `アニメ制御`，日志为 `controller=-1`，所以这些是默认静止姿态的检查，不能证明所有动画帧正确。

<a id="record-goldboom-mme-reassign-当前结果"></a>
### 当前结果

- GoldBoom 的自阴影 0 和 2：最终隐藏窗口测试全部完成，同模式的 7 张 BGRA 图像哈希相同。
- GoldBoomDiffuse 的自阴影 2：最终隐藏窗口测试完成，7 张 BGRA 图像哈希相同。
- 纯绿色 FX 正对照：重新分配使 155,848 个像素改变，确认分配命令进入了实际绘制。
- 真实 Windows 文件框也曾返回同一个 FX，画面未出现差异，但该轮隔离声明无效，限制见下文。

源码分析确认 MME 在第一次 `BeginScene` 才注入菜单。因此诊断需要先渲染空场景，再关闭分配文件自动保存。原生设置效果命令经过外部 MMEffect.dll；本次没有发现宿主按手动和自动分配分别处理 GoldBoom 的确证。

MMV4MMS 先前提交 `4f21ff2` 修复的是 `ANIMATEDTEXTURE.SeekVariable` 读取 float3 的兼容错误，属于独立问题，不能作为这次原生 MME 操作异常的解决结论。

<a id="record-goldboom-mme-reassign-后台环境与证据限制"></a>
### 后台环境与证据限制

初版 Python 启动器给 `subprocess.STARTUPINFO` 动态设置 `lpDesktop`，但该字段未随 `STARTUPINFO.copy()` 传给系统。因此早期 manifest 中的 `private_desktop:true` 不成立。那些运行使用隐藏窗口钩子；它们不是已验证的私有桌面运行。早期原始日志和错误元数据保留，由追加清单标出更正。

改用 `win32process.CreateProcess` 后，被测程序确认实际桌面名称为 `MMS_REPAIR_...`，随后 D3D9 创建设备失败。此机的私有桌面 GPU 路径未通过，不能把该轮当作渲染验证。最终用例只允许显式选择隐藏窗口模式，并在此模式禁止真实文件框；真实文件框测试仍强制要求私有桌面。

最终隐藏窗口检查注入文件选择结果，模拟目录变化，真实执行 MME 列表选择、分配、编译与绘制。它不能完整替代用户现场的系统文件选择过程。全部重载使用 MME 命令 40004；用户现场恢复正常的具体重载操作仍未确认。实际运行 EXE 的路径和错误画面也未取得。

这次没有生产修复，不报告性能收益，也没有以单版本诊断替代修复前后性能验收。

<a id="record-goldboom-mme-reassign-本地记录"></a>
### 本地记录

源码基线为 `4425ec88f82c97a4ba1453a94663f3a92e0391db`。VS 2022、x64、Release，诊断和性能宏关闭，DX12 编译选项开启。具体构建命令、源码状态、EXE 与 DLL 哈希、素材哈希、完整原始日志和失败样本记录在 验收清单（`.local/artifacts/goldboom-mme-20261011/01/validation-manifest.json`，本地证据文件当前缺失）。

相关目录保留如下：

- `.local/worktrees/goldboom-mme-before`：基线生产源码，加同一诊断用例。
- `.local/worktrees/goldboom-mme-fix`：诊断分支，没有生产代码修复。
- `.local/builds/goldboom-mme-before`：独立生产 EXE 和诊断目标。
- `.local/artifacts/goldboom-mme-20261011/01`：命令脚本、运行环境副本、图像和日志。

没有替换正式 `build/Release` 的 EXE 或 DLL，没有推送远端。

<a id="record-emm-autosave"></a>
## EMM 自动保存修复（2026-09-25）

原始记录：`EMM_AUTOSAVE_2026-09-25.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-emm-autosave-根因与修改"></a>
### 根因与修改

用户报告：打开工程时关闭「自动保存效果分配文件」，之后开启再保存，只更新 PMM。真实 MME 后台测试通过 `WM_COMMAND 0xCF` 触发保存，在修复前重现了该行为。

`SaveSceneFile` 使用 UCRT 的 `_wsopen_s`，绕过了 MMHack 改写的宿主 `CreateFileW` 导入。PMM 加载已有 `NotifyEffectFileOpen`，保存没有对应通知。实测 MMHack 的 `+0x41E0` 文件钩子在目标扩展名为 `.pmm`、访问标志恰好为 `GENERIC_WRITE` 时记录保存路径；MME 消费队列时检查当前自动保存状态。地址仅作此版本的分析证据，产品代码不依赖私有地址。

现在 PMM 成功关闭后调用 `NotifyEffectFileSave`。它通过宿主导入以 `GENERIC_WRITE` 和 `OPEN_EXISTING` 打开后关闭文件，不截断或重写 PMM。MME 继续处理开关及 EMM 序列化，不改变效果分配、菜单状态或 DLL 文件。没有加载 MMHack、路径为空或扩展名不是 PMM 时直接返回。

<a id="record-emm-autosave-验证结果"></a>
### 验证结果

对照基线为 `f7c6cb0`。隔离工作树 `.local/worktrees/emm-autosave/` 的两侧使用相同的回归源码、MSVC v143 x64 Release（`/O2 /Ob2 /DNDEBUG`）、MME DLL、单模型 Grass 夹具、相机、分辨率和每步四帧的处理流程。隐藏窗口通过真实保存菜单执行，不操作用户窗口，不覆盖输入素材。

| 场景 | 修复前 | 修复后 |
| --- | --- | --- |
| 关闭自动保存时保存 PMM | PMM 写入，未生成 EMM | 相同 |
| 在关闭状态下打开，再开启并保存 | 两次均缺少 EMM，退出码 7 | 两次均通过，退出码 0 |
| 初始开启、首次保存到新路径 | 缺少 EMM，退出码 7 | 通过，退出码 0 |
| 已有 EMM 后关闭，再保存 | 前项失败，未继续 | 已有 EMM 字节不变 |
| 再次开启并保存 | 前项失败，未继续 | 重写 EMM，效果分配内容不变 |
| 中日文路径另存为 | 前项失败，未继续 | 生成同名 EMM，效果分配内容不变 |

关闭起始场景按修复前、修复后、修复后、修复前的顺序交替运行。末尾另测初始开启。初始开启场景测试的是新路径首次保存，不据此推断用户已有 EMM 的更新情况。空场景也完成了关闭、重开及另存为测试。

正式工程重新构建 `MikuDanceStudio`、`effect_api_tests`、`scene_lifecycle_mme_tests` 成功。合入后的实际 MME 单模型测试退出码 0；`effect_api_regressions` 通过。正式工作树另有正在进行的蒙皮和阴影改动，构建状态保存在原始记录中，本次提交仅包含 EMM 保存与回归相关文件。

本次没有测量播放 FPS、CPU 或 GPU 帧时，性能部分尚未验收。记录中的 `process_seconds` 只是测试进程总耗时，不能换算为播放性能。修复只在工程保存后增加一次不截断的文件打开，不向逐帧渲染实现添加工作。

<a id="record-emm-autosave-本地产物"></a>
### 本地产物

- 原始日志、生成的 PMM/EMM、命令、源码状态及哈希：`.local/artifacts/emm-autosave/manifest.json` 和同目录日志。
- 修复前后测试宿主：`.local/releases/emm-autosave/before/`、`after/`；正式工作树测试宿主副本：`integrated/`。这些是后台测试入口，不能作为日常编辑器启动。
- 正式程序：`build/Release/MikuMikuDance.exe`。未重启用户程序。
- 隔离工作树保留作对照，没有清理或删除旧产物。

对照测试 EXE SHA-256：修复前 `1e5acd352741772dad2e84bd531c1dfea900d08a4055170f4e7015623a1e30cd`；修复后 `e725f4534d2a1a1f44917a9990ef82713cd0d3bc51e4f592b5dc0cce6edb62f2`。正式集成 EXE 的哈希另记在清单中。

MMEffect SHA-256 为 `74b3f5882a3bad28131cbc57994e2f960938e6ca850fc7139018ce4822bd87dc`，MMHack 为 `e450b25e5cd2bcf125ca4b8eb9e57f858ec3ca379633c7d25f9dd40e864b390d`。其他版本 DLL 未实测。

<a id="record-mme-registration-crash"></a>
## MMV 网格登记时的 MME 后处理崩溃修复

原始记录：`MME_REGISTRATION_CRASH.md`。其中的版本、日期和验证范围沿用原记录。

2026-09-27。修复范围是 `MMEffect.dll+0x2655` 读取空深度表面。宿主在保存分配、替换网格和恢复分配时，原先插入四个空的 `BeginScene/EndScene`。MME 请求离屏绘制后，需要宿主递减重复次数并提交对应绘制，才能进入主目标并捕获深度状态。直接结束场景会跳过这一步。

`DrainEffectRegistration` 现在使用现有阴影和模型渲染器，完整消费 `ExpSetRenderRepeatCount` 请求。它保存和恢复渲染目标、深度表面、视口、设备状态与宿主当前 pass，不推进动画、物理或 MMV，也不额外 Present。三个分配转移入口共用此流程。第三方 DLL、FX 和 37 个导出保持原状。

<a id="record-mme-registration-crash-验收"></a>
### 验收

后台测试由生产核心链接的 `mme_postprocess_tests` 运行，加载原路径 AutoLuminous.x 及其同名 FX。旧构建在首次 MMV 分配时稳定复现同一 DLL 偏移和空指针。新版通过初次分配、重载、打开参数窗口、保存 EMM 和退出。

- 兼容版、纯 DX12 版均通过真实 Miku PMD、AutoLuminous 和 MMV 恒等效果。640×480 渲染目标包含 20585 种颜色，分配前、分配后和重载后的 BGRA 完全相同，两版也相同。SHA256 为 `53c5f63d1e9bbc3c5f0a43437e6ef553086a0200d81d8f32a5eb6d7e24b8d625`。
- 原故障中的 Bullet3 FX 分配组合通过。此项证明该配置不再触发登记崩溃，不替代 Bullet3 模型物理精度验收。
- `mmv_host_tests production-extra` 通过生成材质命名、效果分配保存与真实绿色像素着色断言。
- 保存后的 EMM 保留 `[Effect@AL_EmitterRT]` 离屏页。测试同时检查 MMS 附件源 mesh 未被替换。
- 共同可运行的 MME-only 场景采用 60 帧、8 轮 ABBA/BAAB 交替采样。修复前墙钟中位数 614.832 ms，范围 609.429–617.442 ms；修复后 614.615 ms，范围 602.068–617.436 ms。差异处于波动内。这是含消息泵和 Present 的帧循环墙钟时间，不是纯 GPU 时间或播放 FPS。
- 旧版分配场景崩溃，不能提供该场景成功运行时的性能对照。新增登记绘制仅在结构更换或分配恢复时发生，成本不按普通帧计入；未声称这段维护流程性能提升。

<a id="record-mme-registration-crash-证据与复现"></a>
### 证据与复现

证据目录：`.local/artifacts/mme-null-crash/`。有效画面在 `after-target3/`、`validated-dx12/`、`validated-bullet3/`，性能原始日志为 `perf-*.log`。`validated-readbacks.json` 保存图像哈希；`manifest.json` 保存构建和运行文件哈希。

`after-identity-model`、`after-bullet3`、`after-material`、`after-dx12` 的早期空目标回读不作为画面依据。`after-readback` 因目标为空主动失败。`build-target` 链接失败后误跑旧程序的样本也排除。全部保留用于溯源。

构建目标：`cmake --build build --config Release --target mme_postprocess_tests mmv_host_tests MikuDanceStudio -j 8`。测试 EXE 需与参考 MMEffect.dll、MMHack.dll、d3d9.dll、待测 MMV.dll、Data 和 MMV 资源同目录，文件名必须保持 `MikuMikuDance.exe`。测试参数依次为 AutoLuminous.x 绝对路径、输出目录、MMV FX 绝对路径、可选模型绝对路径。两个 DLL 版本顺序运行，不并发占用测试目录。

DSH 在正式工作区完成了空指针类型和重复绘制门控的只读分析。长时间后续扫描由主 agent 中止，子任务终态为 `missing-turn-end`；报告不是独立验收结论。主 agent 复核实际调用链并完成上述运行验收。
