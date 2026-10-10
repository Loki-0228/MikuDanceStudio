# MikuDanceStudio-MME 全量代码审查报告（2026-09）

本报告记录一次覆盖全部 199 个源/头文件的静态审查，以及一次从零重建并跑完全部
回归的实测验证。缺陷逐条列在 [缺陷清单](CODE_REVIEW_FINDINGS.md)，本文件给出
审查范围、验证证据、子系统结论与未覆盖范围。

审查对象：`D:\MMD-Next\MikuDanceStudio-MME`，提交 `e3ad30d`（工作区干净）。
行为基准：`refs/MikuMikuDanceE_v932x64/MikuMikuDance.exe`，
1,722,368 字节，SHA-256 `07516fd3bf1e6b1339836b6773a156f61bdd6f848eeb621fdda012375df313a1`。

## 一句话结论

从零重建成功、22 项回归 100% 通过，但**现有回归的覆盖面不构成可发布性证据**：
本轮共记录 146 条缺陷，其中 **36 条 S1**（崩溃、内存损坏或数据丢失），另有
2 条由真实素材实测复现的缺陷不在 CTest 覆盖范围内。S1 高度集中在一个共同的
根因上——**把文件里读来的索引、计数或链指针直接当成数组下标或分配长度使用，
没有夹取**。

## 范围与方法

### 审查口径

- `docs/` 记录的是**重构前**的情况，重构版已新增功能。因此代码与文档不一致
  按文档缺陷记录，**不**当作代码缺陷，也不把文档当作代码判据。
- 只有从原版二进制反汇编核对过的「与原版不符」才作为行为偏差判定；未核对的
  一律降级为「待核实」并注明所需的核对步骤。
- 本工程的既有约定不重复报告：`RawPad`、`offsetof`、`sizeof`、ABI 静态断言、
  二进制字段大小、硬编码偏移本身都不是缺陷证据（`docs/CLEANUP_INVENTORY.md:40`
  已明确）。`src/unported/stubs.cpp` 含真实实现，不因路径名判为死代码。
- `src/render/d3d12_native.cpp`、`d3d12_skinning.cpp` 是显式声明的实验性预览视口，
  `docs/D3D12_MIGRATION_2026-09-20.md` 已列出其不实现的特性（自阴影、地面、
  选择覆盖层、完整材质着色器、mip 1 以上）。本轮只按工程风险审查这两个文件，
  不把缺失特性记为偏离原版。

### 覆盖

199 个文件全部过审，按子系统分 12 批，每批用同一份评级与字段规约，逐文件标注
审读深度。缺陷编号前缀：`MEM` 越界与缓冲区、`LIFE` 资源生命周期、`LOGIC` 逻辑与
边界、`CONC` 并发、`API` 平台接口误用、`ABI` 布局自洽性、`PORT` 与原版行为的
偏差或注释与代码不符。

## 实测验证

### 从零重建

删除 `build/`（203.8 MB）、`dist/`（1.5 MB）、`build.log` 与
`tools/.x64_scan_cache.pkl`（含 `.bak`）后重建。删除前把 `build/` 内的非构建产物
备份并核对 SHA-256，重建后原样放回。

```powershell
conan install . --profile:all profiles/x64 --build=missing -s build_type=Release
cmake --preset conan-default -DMIKUDANCESTUDIO_BUILD_INPUT_TESTS=ON `
      -DMIKUDANCESTUDIO_BUILD_EFFECT_TESTS=ON `
      -DMIKUDANCESTUDIO_BUILD_ENCODING_TESTS=ON `
      -DMIKUDANCESTUDIO_BUILD_UI_TESTS=ON
cmake --build build --config Release
```

结果：配置成功（MSVC 19.44.35228.0、v143、Windows SDK 10.0.26100.0），
构建退出码 0，**0 个错误、20 条警告**。25 个 `EXCLUDE_FROM_ALL` 测试目标逐个
显式构建，全部退出码 0。

`bullet275/2.75` 取自 Conan 本地缓存（输出 `Already installed!`），
本轮**没有**重新构建该依赖。

### 警告

| 代码 | 条数 | 站点 |
| --- | --- | --- |
| C4018 有符号/无符号不匹配 | 14 | `frame_modes.cpp:552`、`dialog_gaps.cpp:328/342/418/421/442/464`、`post_load_init.cpp:206`、`d3d12_native.cpp:254`、`command_control_400.cpp:3024`、`command_control_500.cpp:406/545`、`command_file_menu.cpp:1741/2012` |
| LNK4104 导出应标 PRIVATE | 4 | `src/mmdxshow/MMDxShow.def` 的四个 COM 入口 |
| C4200 零大小数组 | 1 | `subrecord_layout.hpp:220`（`SkinnedVertex<0>` 柔性数组成员） |
| C4996 `GetVersionExA` 已弃用 | 1 | `src/mmdxshow/dll_main.cpp:358` |

LNK4104 与 C4996 是照搬原版导出形状的结果，属预期。

### 回归

```powershell
ctest --test-dir build -C Release --output-on-failure --timeout 60
```

**22/22 通过**，用时 8.05 秒。测试覆盖窗口消息、D3D 设备、文件格式、布局断言、
PMX 导入、语言切换、PMM 重载与效果接口。

### 发布布局与导出

`cmake --install build --config Release --prefix dist` 后 `dist/` 只有
`MikuMikuDance.exe` 与 `Data/MMDxShow.dll`，与 README 描述一致。
导出表 37 个 `Exp*`、序数 1..37 连续，与 `exports/MikuMikuDance.def` 及
`docs/MME_COMPATIBILITY.md` 一致。

### 真实素材手动验证

测试素材位于 `D:\MMD-Next\tests`（父项目共享，本轮未改动）：

```powershell
scene_lifecycle_tests.exe "<Stage.pmm>" <轮数> [模式]
```

| 模式 | 结果 |
| --- | --- |
| 默认（日/英/中三轮） | 通过；每轮 11 个模型、15 个配件，渲染 12 帧物理 |
| `--bone-movement` | 通过 |
| `--physics-selection` | 通过；11 个模型 × 3 条原生物理菜单命令 |
| `editor` | 通过 |
| `--playback-view` | **失败**，`explicit camera/follow mode still applies the camera attachment` → CR-001 |
| `--view-tools` | **失败**，`changing orbit reference preserves the current view` → CR-002 |

表中的两处失败均已在后续提交中修复：`CR-001` 由一系列对齐附着门控的提交（含 `57f68ff`）修复，`CR-002` 则由 `cbe5f76` 修复。修复后的实测结果见 [缺陷清单](CODE_REVIEW_FINDINGS.md) 的「当前测试状态」表。

MME 宿主单独验证：把真实 x64 `d3d9.dll`、`MMEffect.dll`、`MMHack.dll`、
`msvcr90.dll` 放进 `build/tests/mme/Release/` 后运行该目录下的
`MikuMikuDance.exe --menus-only`，输出 `MME loaded` 并以 0 退出；
不放 DLL 时普通宿主的 `--menus-only` 同样通过。这两条覆盖了 MME 宿主的三项
硬性要求（37 个导出、可被改写的 D3D9 导入表、模块名）在真实插件下的连通性。

## 缺陷总览

完整条目见 [缺陷清单](CODE_REVIEW_FINDINGS.md)。

| 级别 | 条数 | 含义 |
| --- | --- | --- |
| S1 | 36 | 崩溃、内存损坏或数据丢失 |
| S2 | 33 | 用户可见的功能错误 |
| S3 | 41 | 资源泄漏、性能退化或维护隐患 |
| S4 | 36 | 一致性、命名与文档缺陷 |
| 合计 | 146 | |

（`B3-01` 与 `MB-08` 经反汇编核验后撤回，见缺陷清单的「已核验条目」。）

S1 按子系统分布：`io/` 20 条（PMM/VPD/VSQ/VMD 读取路径），`model/` 8 条
（PMX/PMD 加载与每帧蒙皮），`render/` 3 条，`media/` 2 条，
`window/`、`physics/`、`exports/`、`app/`、`features/` 各 1 条。

清单里每条都标了**触发条件分类**：用正常文件与常规操作即可触发的属确定缺陷；
仅在文件被手工编辑、截断或构造时触发的按「有意加固」处理，动手前先核验原版有无
同类界。详见清单开头的读法说明与「已核验条目」。

### 根因一：文件数据直接充当索引、计数与长度（S1 主体）

这是本轮最集中的一类，横跨 io、model、physics、window、media 五个子系统，
共同形态是：值来自用户磁盘上的 PMD/PMX/PMM/VPD/VSQ/WAV，随后直接用于

- 稀疏关键帧记录的帧号下标（骨骼池 600000、形态池 20000、显示池 1000、
  全局轨道各 10000）；
- 模型槽字节、配件槽字节、当前选中槽字节（合法域只有 0..254，255 越界）；
- 映射表下标（`boneMappings`、`rigidMappings`、`SelectorStates`）；
- `malloc`/`operator new` 的分配长度；
- `previous`/`next` 链指针的回溯；
- 物理刚体的 `boneIndex`/`tailIdx` 与 IK 的 `link.boneIndex`。

危害不是理论性的：多处越界恰好落在相邻状态字段上（例如 `modelSlots[255]`
正好覆盖 `slotIdx`、`editMode`，`accs[255]` 正好覆盖 `lastRegisteredFrame`），
一次写就破坏应用状态；值取 `0x7FFFFFFF` 时目标地址在池首之后数 GB，直接访问冲突。

修法方向统一：在**每个**读入索引/计数的位置夹取到该数组或池的真实容量，
越界时只消费字节并丢弃记录；链指针在使用前同样夹取；分配长度先做上限校验并
检查返回值。逐条位置见清单中的 `B3-*`、`MB-*`、`CB-*`、`B2-08`。

### 根因二：同类逻辑在多个实现点之间不一致

同一语义在多处实现，其中一处漏了守卫或算错，是本轮 S1/S2 的第二大来源：

- `physics_create.cpp:670` 有正确的骨骼号夹取，`physics_frame.cpp:699/360/712/757`
  与加载器 `pmd_load.cpp:726/742`、`pmx_load.cpp:1459` 没有；
- `render/accessory.cpp:241-245` 等五处对 `parentModel` 有范围与空槽守卫，
  `effect_api.cpp:719` 的 `ExpGetAcsWorldMat` 没有；
- `morph_apply.cpp:239/280` 对 morph 索引有完整门，`model_skinning.cpp:544` 只挡负数；
- `bone_transform.cpp:677/1225` 的 `RotateGrant` 不取共轭，`physics_frame.cpp:760-766`
  取了共轭，两处实现互相矛盾；
- `effect_api.cpp:573` 的材质表上界写成闭区间，配件侧孪生函数用严格小于；
- `pmm_load_v1.cpp:1737-1746` 的槽遍历带空指针判断，`pmm_load_v2.cpp:2494` 不带。

建议按「同一语义只保留一个带守卫的实现并让其余调用点复用」来收口，而不是逐个补判断。

### 与实测失败对应的两条

CR-001 与 CR-002 均由真实工程实测复现，细节见清单。二者的共同点是 **相机参照模式（camera reference mode）**：一条为 v2 工程载入时不恢复该状态（v1 载入器则会恢复），另一条为改变参照模式时补偿读取了错误的旧枢轴——本节原先记作「补偿被后续的 `SetupFrameWorldTransform` 覆盖」，该判断不成立。两条均位于近期改动过的相机/视图区域，且均不在 CTest 覆盖范围内，两条均已在后续提交中修复。

## 子系统结论

| 子系统 | 文件数 | 结论 |
| --- | --- | --- |
| `io/` | 9 | S1 最密集处。v1 与 v2 两条 PMM 载入路径都把文件帧号当池下标；VPD/VSQ 的计数与字符串判空缺失。`io_gaps.cpp` 无 S3 以上问题。 |
| `model/` | 21 | 加载期与每帧蒙皮期都存在未夹取的索引。`pmx_load.cpp`、`pmd_load.cpp`、`model_skinning.cpp` 是重点。`morph_apply.cpp`、`model_keyframe_edit.cpp`、`vmd_load.cpp`、`bone_sort.cpp`、`post_load_init.cpp`、`model_init.cpp`、`add_model.cpp`、`bone_edit_undo.cpp`、`track_apply.cpp` 无 S3 以上问题。 |
| `window/` | 54 | S1 少但用户可见缺陷多：模型计算顺序对话框的数组语义错位（正常操作可复现崩溃）、灯光编辑框不写回场景光、GDI 画笔/画刷泄漏、约 30 MB 暂存数组在「新建/打开工程」路径不释放。 |
| `physics/` | 4 | 刚体回写用文件里的骨骼号索引骨骼表；`RotateGrant` 三处实现互相矛盾。Bullet 对象的释放路径与方法体创建成对，未见泄漏或二次释放。 |
| `render/` | 25 | 主要为生命周期与状态恢复问题。D3D12 预览按声明只审工程风险：未发现越界或竞态。 |
| `app/` | 26 | 帧驱动、播放追赶与 AVI 录制路径的生命周期问题为主。 |
| `mmdxshow/` | 8 | 无 S1。DirectShow 契约基本正确；`MMDxShow.def` 的序号 @1..@5 与头文件一致。 |
| `features/` | 3 | 308 字节（77 浮点）追踪记录的读写端通道约定不一致，导致部分通道读到未初始化堆内存。 |
| `media/` | 2 | WAV 头的 `nBlockAlign` 可为 0 导致除零；data 块长度直供 `malloc`/`memset`。 |
| `exports/` | 2 | 37 个导出与 `.def` 一一对应、缓冲区写入量符合 MME 契约；但两个函数对入参索引缺守卫。 |
| `math/`、`main/` | 6 | 无 S1–S3 问题。 |
| `include/` | 47 | 布局断言与源码用法经逐条复核一致，未发现 ABI 冲突。 |

## 未覆盖与未验证范围

- **反汇编核验覆盖了 59 条「需畸形输入」条目**，结论分五类记录在缺陷清单的
  「核验总表」里：原版无界 51 条（加固）、原版有界但移植不忠实 2 条（按原版改）、
  原版有界且移植忠实 2 条（不要改）、原版位置无法定位 2 条、无下标问题 2 条。
  59 条里 1 条是整条误报，另有 3 条需收窄范围、1 条原版比移植更宽。
  结论是：静态审查对「移植少了一道界」这一方向的判断成立率高（51/59 属原版无界），
  但具体范围、容量数字与「原版到底有没有界」不能靠推断。
  核验方法与载入器定位表见 [核验方法](VERIFY_METHOD.md)。
- **未做性能测试**。`docs/` 下的性能报告与 JSON 快照按 `CLEANUP_INVENTORY.md`
  的 C07 保留，本轮未重跑、也未核对数字。
- **未做模糊测试或畸形文件生成**。S1 的触发条件由静态路径推出，并给到具体字节
  取值，但没有逐个构造样本实测。加固后就地构造样本验证，是实施阶段应做的事。
- **未做 MME 全流程验证**。本轮只验证宿主在真实插件下能初始化、菜单可用、
  三语言切换与清理；未验证效果分配、`.fx` 编译与渲染结果。
- **未覆盖 D3D12 预览的功能对齐**，按前文口径只审工程风险。
- **本轮未修改任何生产代码**，只做诊断与文档。截到本地提交 `049d7f5` 为止。
- Windows 消息、D3D 设备、堆完整性相关的结论都在本机环境下取得；沙箱或硬件
  初始化失败不视为功能通过。

## 畸形输入类缺陷的处理决定

**已决定：补边界检查，并记录为有意加固。** 该项由仓库维护者决定。

理由与代价：项目目标是对齐原版的可观测行为，但**崩溃现场无法保真**。原版是 VC9 加
32 位指针，本工程是 v143 加 x64，堆布局与分配器都不同，同一个越界写不会落在同一位置，
也不必然产生同样的致命错误框。要求崩溃一致没有可验收的判据。能验收的只有正常输入下的
行为，而那部分正落在「常态可达」一类，本来就该修。代价是这份输入在原版会崩、在本工程
不会崩，两者行为分叉，因此每处加固都要在注释里写明是有意偏离，否则后续有人对照原版
反汇编会把它当多余判断删掉。

加固做法统一为：读入索引或计数后夹取到该数组或池的真实容量，越界时只消费字节并丢弃
记录，流的位置不变。

**动手前必须先核验原版是否存在同类判断。** 这条要求源自 `B3-01`（稀疏关键帧帧号）：该条目曾被列为 S1，后被判为非缺陷，理由是原版在 `0x140083B54` 处有 `cmp [rcx+0x3110], esi` 加 `jle`。第三轮核验发现该地址属于 **v1** 载入器，v2 并无等价判断，于是结论改回 **真缺陷**，按加固处理。两次误判均源于区域定位有误，判据本身并无问题。这说明核验必须自行复核锚点，也不能按条目分类整批附加检查。核验方法与逐条结论见 [缺陷清单](CODE_REVIEW_FINDINGS.md) 的「已核验条目」。

## 建议的后续顺序

1. CR-001 与 CR-002（已完成）：两者均有实测失败，改动量小，且直接对应「重构后相机/视图行为不正确」这类用户可见问题。CR-001 由附着门控对齐的一系列提交修复，CR-002 由 `cbe5f76` 修复。
2. 再修「常态可达」的 S1：`B2-01`、`RB-01`、`RB-03`、`CB-02`、`AB-01`、`AB-02` 等。
   这些无需先做反汇编，原版在正常输入下不会崩溃。`RB-02` 属于双方均存在的缺口，`MB-08`
   已核验并撤回，两者均不在此列。
3. 需要加固的「需畸形输入」条目：先逐条反汇编核验原版有无同类界，再按核验结论改。
   建议从各家族各取一条做样板，确认做法后再批量推进，不要一次改完 20 多处。
4. 给现有 22 项回归补上真实素材路径：本轮两条缺陷都不在覆盖内，而它们恰好是
   用户能看到的。建议把 `scene_lifecycle_tests` 的 `--playback-view` 与
   `--view-tools` 纳入默认回归集（不需要 MME，只需可用 D3D 设备）。
5. 文档收口：见下节。

## 已解决：视点字段的归属（CR-002）

CR-002 已由提交 `cbe5f76` 修复，逐条细节见[缺陷清单](CODE_REVIEW_FINDINGS.md) 的 `CR-002` 专条。本节原先以「待决」列出，下文记录结论，并说明原先的两个前提为何不成立。

### 原来的错处

补偿函数读取旧枢轴时调用 `ModelViewReferencePoint(oldMode)`；参照模式为 `None` 时，该函数返回零（即世界原点），而渲染实际使用的是自由视图分支的 `(ViewOffsetX, ViewOffsetY, 0)`。因此补偿少算了一个完整的枢轴。测试台架上的实测结果恰好印证了这一点：原 `-=` 实现得到 `eye = (-37, 6, -62)`，与不变量要求的 `(-34, 18, -62)` 相差恰为 `(3, 12, 0)`。这也解释了为什么「四种写法逐一实测」全部失败——每次只改动一个变量，而当时存在两个相互独立的错误（符号与旧枢轴）。

### 原前提一：两条测试互斥

本节原先断言 `--playback-view` 与 `--view-tools` 不可能同时成立。实测表明该断言不成立：前者作用于附着分支（`src/render/debug_geometry.cpp` 中将 `cameraPos` 写入 `ViewOffset` 的那一段），后者作用于自由视图加参照的分支。两者分属不同分支，并无取舍关系。修复后两条测试同时通过。因此，无需再用字段归属（路线 A 或 B）来收口这一缺陷。

### 原前提二：这次复制会把补偿抹掉

同一处复制仅在附着分支执行，而该分支的枢轴来自被附着的骨骼，不随参照模式变化，补偿在此处理应为零。自由视图与参照分支中并不执行该复制，补偿得以正常保留。两条路径各自自洽，不存在「互相抹掉」的情形。

### 修法与遗留

四路枢轴选择被抽为 `ResolveFrameFocus(app, mode, focus)`，渲染与补偿共用；补偿按 `eye_new = eye_old + R(focus_old - focus_new)` 写回，实现上仍是原来的「两个旋转矩阵求差再减」。仍有一处未决：切回自由视图（参照改为 `None`）时，新枢轴正是补偿所要写入的 `ViewOffsetX/Y`，方程因此自耦合，精确解需满足 `eye = eye_old + R(focus_old - (eye.x, eye.y, 0))`；该方向不在断言范围内，画面仍会发生移动。本节列出的三个结构点中，`attachedView` 一条已修复；枢轴选择中「相机位置排在显式参照之前」一条仍未处理（跟随模式下显式参照被静默忽略）；实测数值条目已随修复一并消失。

## 文档审查

审查口径：`docs/` 记录的是**重构前**的情况，重构版已新增功能。因此代码与文档
不一致按文档缺陷记录，不当作代码缺陷，也不把文档当作代码判据。以下每条都实测过。

### 悬空引用

源码里有大量注释把推理依据指向本仓库不存在的文件。这直接影响开源后的可追溯性：
新贡献者读到「见 `docs/ARCHITECTURE.md` §8」时无据可查。

| 引用 | 处数 | 实测 |
| --- | --- | --- |
| `docs/PORTING_STATUS.md` | 12 处，如 `src/unported/stubs.cpp:14` 称其为 authoritative tracker | 本仓库与全部历史中都不存在 |
| `docs/ARCHITECTURE.md`（多指向 §8 deviations） | 15 处 | 同上 |
| `translated/` 或 `../translated/` 反编译语料 | 37 处，如 `src/mmdxshow/crt_ledger.cpp:21`、`src/window/command_control_400.cpp:169` | `D:\MMD-Next\translated` 不存在 |
| `src/render/background_plane.cpp` | `src/unported/stubs.cpp:255/264` | 文件不存在，背景函数在 `src/render/bg_overlay.cpp` |

`docs/CLEANUP_INVENTORY.md:16-18` 的 C02、C03、C04 称被删脚本可从提交 `99c3c19`
恢复，该提交同样不存在。

### 提交哈希

`docs/` 共引用 16 个哈希，只有 2 个存在（`ba6b269` 起始提交、`e3ad30d`）。
其余 14 个在本仓库 `git cat-file -e` 全部失败。原因是本仓库从当前工作树起始，
只保留 2 个提交，而文档沿用父工程的历史记录。

### 测试计数

当前实测：CTest 注册 **22** 项，`tests/*.cpp` 共 **26** 个。
`tests/README.md` 的「22 项通过 CTest 注册」与当前一致，其「23 个测试源文件」
应更新为 26。其余文档记录的 `19/19`、`20/20`、`12/12` 是各自轮次的口径，
不是错误，但读者容易误当作当前数字——建议在各自标题行注明测试集规模。

### 已实测完全一致的文档声明

这些声明经本轮核对成立，可以放心引用：

- `docs/MME_COMPATIBILITY.md:42-43` 的 DLL 哈希：MMEffect `74b3f588…`、
  MMHack `e450b25e…` 与 `refs/MikuMikudance 926 CHS/` 下的文件逐字节一致；
  `:93` 的 9.26 CHS x64 exe 哈希 `72c34b7a…` 一致。
- README 的发布布局（`dist/` 只有主程序与 `Data/MMDxShow.dll`）、
  四个测试选项名、`EXCLUDE_FROM_ALL`、PDB 与静态库的输出目录。
- 37 个 `Exp*` 导出与序数 1..37。
- `docs/CLEANUP_INVENTORY.md` 的 C06、C07 判断（`src/unported/stubs.cpp`
  含实现不能按名删除；`tools/strcheck2_result.json` 与性能 JSON 是历史快照）。

### 文档引用的构建目录

`build-asan`、`build-current`、`build/verify-crash` 在当前源码树均不存在。

## 附录：本轮验证的原始数据

### 清理与备份

删除 `build/`（203.8 MB）、`dist/`（1.5 MB）、`build.log`、
`tools/.x64_scan_cache.pkl` 及 `.bak`。删除前把非构建产物备份并核对 SHA-256，
重建后原样放回（`build/tests/mme/Release/` 与 `build/Release/Data/`）：

| 文件 | 字节 | 说明 |
| --- | --- | --- |
| `d3d9.dll` | 41,472 | x64 MME 代理 |
| `MMEffect.dll` | 917,504 | MME 主体 |
| `MMHack.dll` | 458,240 | MME 宿主挂钩 |
| `msvcr90.dll` | 644,496 | MMDxShow/MME 所需 VC9 运行库 |
| `mmconfig.ini` | 268 | 程序运行时状态（缺失时程序走默认值） |
| `MikuMikuDance.before-background-fixes.exe` | 1,636,352 | 自制对照产物 |
| `MikuMikuDance.before-view-fixes.exe` | 1,634,816 | 自制对照产物 |

`bullet275/2.75` 由 Conan 本地缓存提供，本轮未重建该依赖。

### 构建产物

重建后 `build/Release/MikuMikuDance.exe` 为 1,649,152 字节
（SHA-256 `0174e5a6…`），原版参考为 1,722,368 字节（`07516fd3…`）。
两者大小不同属预期：`/OPT:REF`、`/OPT:ICF` 与不同的 CRT 都会改变体积，
本项目的对齐目标是可观测行为而非字节级一致。

