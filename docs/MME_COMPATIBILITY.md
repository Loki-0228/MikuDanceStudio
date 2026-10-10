# MME 对象登记与效果分配验证（2026-09-17）

## 材质名双向回退与时间轴编码（2026-09-20）

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

## MME 主菜单汉化（2026-09-20）

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

## 问题与修复

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

## 二进制依据

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

## 实测结果

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

## Ray 工程重载与语言切换（2026-09-17）

### 修复

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

### 验证素材与边界

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
