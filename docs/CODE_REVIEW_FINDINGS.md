# MikuDanceStudio-MME 缺陷清单（2026-09）

本清单是 [代码审查报告](CODE_REVIEW_2026-09.md) 的配套文件，逐条列出本轮全量审查
发现的缺陷。编号沿用审查批次的编号（`B1-`…`B12-`），批内按严重度排列。

合计 **146** 条：S1 **36**、S2 **33**、S3 **41**、S4 **36**。

其中 `MB-08` 与 `CB-05` 已核验并撤回，S2 实际为 **31** 条。`B3-01` 一度被判为非缺陷，经两轮重核后确认为**真缺陷**（原版无界），计数不变。编号均予保留，以便与批次报告对照。

## 落地状态（2026-09-23）

修复分为「常态可达」与「有意加固」两类分别提交，均为本地提交，尚未推送。

| 类别 | 提交 | 内容 |
| --- | --- | --- |
| 常态可达 | `7f42910` | `RB-03`（未载模型时骨骼轴叠加层解引用空指针）、`AB-01`（压缩器类别为空时对空枚举器调 `Reset`，注释记为有意加固）、`AB-02`（`G` 键三分支恢复） |
| 常态可达 | `e3f291e` | `B1-01`（灯光编辑框只写 `LightColor()`/`LightDirection()`，未同步 `SceneLight()`） |
| 有意加固 | `b0645b8` | 对核验结论为「原版无界」的条目中的 47 条进行加固，覆盖 16 个文件；注释注明原版地址，其中 `B3-07`、`B3-11` 标注为「原版位置未定位」，每条以 `Defect <编号>.` 结尾 |
| 有意加固 | `2986d17` | 同类的最后 4 条：`CB-07`、`CB-08`（原版同样不关闭句柄、不释放流，注释记为有意偏离）、`CB-11`（仅添加第二下标）、`RB-06`（配件父骨骼与 PMM 骨骼名称重映射） |
| 按原版改 | `90f6b67` | `MB-09`（将顶点布局改为显式 0..4 分支）、`MB-12`（移除 facial 填充中 `f < 2` 与 `frames[0].special` 的前提） |
| 文档 | `54b5696` | 修正三处过期结论：`B3-01` 曾被写作「非缺陷」、`MB-08` 仍列在待修清单、`CR-002` 写作「符号已修」 |
| 文档 | `9c13204` | 新增本节，并修正 `VERIFY_METHOD.md` 中同处 `B3-01` 的旧判据 |
| 常态可达 | `cbe5f76` | `CR-002`（参照模式补偿误读旧枢轴；将四路枢轴选择抽为 `ResolveFrameFocus`，供渲染与补偿共用） |
| 常态可达 | `6b1dd24` | S2 余项 11 条：`B2-02`、`B2-03`、`B2-04`、`B2-05`、`CB-06`、`FB-01`、`FB-02`、`HB-01`、`RB-04`、`RB-05`、`AB-03` |
| 有意加固 | `69d4301` | S2 中需畸形输入的三条：`B3-22`、`B3-23`、`B3-24` |
| 常态可达 | `0a14f5e` | S3 余项 34 条，覆盖 36 个文件：`B1-02`~`B1-06`、`B2-06`、`B2-07`、`B2-09`、`B3-25`、`B3-26`、`B3-28`、`CB-10`、`FB-03`~`FB-06`、`RB-07`~`RB-12`、`AB-04`~`AB-06`、`AB-08`~`AB-10`、`MB-25`、`HB-02`~`HB-04` |
| 有意加固 | `2283f0f` | S3 中由畸形或超量输入触发的两条：`B3-27`（丢弃循环改为逐字节读取）、`MB-26`（修复纹理池写满时的越界写） |
| 代码 | `f6c16b2` | S4 中需要改动代码的 10 个文件：`FB-13`（追踪缓冲与 undo 姿态缓冲统一使用标量 `::operator new`/`::operator delete`）、`FB-14`（在两条失败路径前将 `*ppv` 置空）、`MB-28`（`RdPtr` 按指针宽度读取）、`MB-29`（改为释放 `morphTableGap`，即 x64 中的 `model+0x2270`）、`MB-30`（刚体与关节的英文名改为走 jpText 回退）、`B1-07`（FOV 转弧度改为单精度，与同文件另两处保持一致）、`B1-08`（`HandleBoneSlider` 更名为 `HandleMorphSlider`，因为它写入的是 `MorphRecord::value`） |
| 文档 | `1f30b15` | S4 余下 20 个文件的注释、命名与台账修正：`B2-10`~`B2-14`、`B3-29`、`B3-30`、`FB-07`、`FB-08`、`FB-12`、`FB-15`、`RB-13`、`HB-05`~`HB-15`、`AB-07`、`CB-12`、`FB-09`~`FB-11` |

至此，「核验总表」中的 51 条「原版无界」与 2 条「无法定位」已全部落地，2 条「按原版改」同样已落地，2 条「不要改」则依结论维持原样。S2 表内除 `CR-002`（已修）与 `CB-05`（复核后撤回，见下表专条）外，其余条目均已落地。S3 表内除 `MB-27`（核验结论为「原版同为死判断，保持」）外，其余条目均已落地；S4 表内 36 条亦全部落地，其中需修改代码的 10 个文件见上表 `f6c16b2`，其余为注释、命名与台账修正（`1f30b15`）。

更早前修复的还有 `CR-001`、`B2-01`、`RB-01`，以及附着门控对齐（`2284dc5`、`6c7883e`、`34ece55`、`57f68ff`）。

各批验证方式一致：Release 重建 0 error，`ctest` 22/22；另运行 `scene_lifecycle_tests --playback-view / --view-tools / --bone-movement / --physics-selection / editor`（`tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm`，11 个模型）与 `--physics-selection`（`tests/MMD Dream Grassland/UserFile/梦中草原 2晚霞 Sunset.pmm`，5 个模型）。在该 Dream Grassland 工程中，`--bone-movement` 受阻于素材前置条件 `stage has a movable right-foot bone`。`--view-tools` 曾因 `CR-002` 而阻塞；该项修复后，在 `Stage.pmm` 上已转为 exit 0，五种手动模式与 `ctest` 均告通过。

S4 的逐条判定记录如下。经复核，`FB-12`、`FB-15`、`AB-07` 三条均属「代码正确、注释过期」：`FB-15` 的末尾守卫在 x64 原版中同样恒真（`0x14008960B` 清零、`0x14008969C` 再测同一字节，两条路径留下的值均为 0），因此两个 `ShowWindow` 调用本就是无条件的；`FB-12` 的注释上界 `332` 应为 `336`（`model_layout.hpp` 的 `localTransforms[68]` 即 17 个标准姿态四元数，64 + 17×16 = 336）；`AB-07` 的 `0.0f` 初值在每帧被 `playback_catchup.cpp` 覆盖之后才会被读取，注释中「保持整帧真实 dt」的说法与 `0.0f` 相矛盾。`HB-14` 的三处 `(stub)` 标记经复核确认，对应函数已分别在 `bone_sort.cpp`、`post_load_init.cpp`、`fill_panel.cpp` 中移植。`FB-13` 仅统一了追踪缓冲与两个 undo 姿态缓冲的分配与释放形态；`undo.auxiliaryPose` 这一槽位在仓内仍被多处以 malloc 与 `::operator new` 混用，属于 AB-05 家族的剩余站点，不在 S4 范围之内。

至此 146 条已全部处理完毕：`MB-08` 与 `CB-05` 经复核撤回，`MB-27` 按核验结论保持不动，其余分别以「常态可达」「有意加固」「按原版改」或「注释与台账修正」落地。

## 如何读这份清单

**严重级别**

| 级别 | 含义 |
| --- | --- |
| S1 | 崩溃、内存损坏或数据丢失 |
| S2 | 用户可见的功能错误 |
| S3 | 资源泄漏、性能退化或维护隐患 |
| S4 | 一致性、命名与文档缺陷 |

**触发条件分类** —— 这一栏决定处理方式，务必先看：

- **常态可达**：用正常、结构良好的文件与常规操作即可触发。这类是确定的缺陷，
  无论原版行为如何都应当修。
- **需畸形输入**：只有文件被手工编辑、截断或构造时才触发。**已定口径：补边界检查，
  并按有意加固记录**，见下节。
- **环境相关**：依赖设备能力、外部 DLL 或系统状态。

### 畸形输入类的处理口径：有意加固

**已决定：检查并加固，同时记录为有意偏离原版行为。** 该项由仓库维护者于 2026-09 决定。

理由与代价：

- 「对齐原版可观测行为」是项目目标，但**崩溃现场无法保真**。原版是 VC9 加 32 位指针，
  本工程是 v143 加 x64，堆布局与分配器都不同，同一个越界写不会落在同一位置、也不必然
  产生同样的致命错误框。要求崩溃一致没有可验收的判据。
- 能验收的只有正常输入下的行为，而那部分正落在「常态可达」一类，本来就该修。
- 代价是这份输入在原版会崩、在本工程不会崩，两者行为分叉。因此每处加固都要在注释里
  写明这是有意偏离，否则后续有人对照原版反汇编会把它当作多余判断删掉。

加固的做法统一为：读入索引或计数后夹取到该数组或池的真实容量，越界时只消费字节并丢弃
记录，流的位置不变。

**动手前必须先核验原版有没有同类界。** 只凭本清单的条目改可能改错，实例见下节
「已核验条目」。原版的界有时是「用另一个语义的量做界」，例如拿骨骼数而不是键池容量
当界，那种情况移植已经忠实，不该再加一道。

核验会得出三种结论，处理方式不同：

| 结论 | 含义 | 处理 |
| --- | --- | --- |
| 原版无界 | 原版读到值后直接使用，中间无判断 | **加固**，并注明原版无此判断 |
| 原版有界，移植忠实 | 原版有判断，移植也有等价判断 | **不要改** |
| 原版有界，移植不忠实 | 原版有判断，移植没有或写法不同 | **按原版改**，这不是加固，是修行为去对齐 |

### 已核验条目（反汇编核对结果）

用 9.32 x64 原版反汇编核对了两条。方法：按 `"0001"`/`"0002"`/`"Polygon Movie maker"`
字符串定位 PMM 载入器（`0x140082D10` 起，版本分派在 `0x140082D8A`/`0x140082D9E`），
再在 `0x140082D00` 至 `0x140085000` 区间内查证。

| 条目 | 核验结论 | 处理 |
| --- | --- | --- |
| `B3-05` 模型槽字节 | 原版**无任何校验**。`0x1400835E1` 读字节，`0x1400835E8` 直接写 `slots[slotByte]`，两条指令，中间无比较；同一个字节在 `0x1400835F0`、`0x140083608` 又被复用了两次 | 按加固实现 |
| `B3-01` 稀疏关键帧帧号 | **原版无界**。v2 里没有 v1 那道 `cmp dword ptr [rcx+0x3110], esi`（该地址 `0x140083B54` 属 v1）。稀疏 display/morph/physics frame 直接 `imul 0x3c` 或 `lea *4` 后加表基址：`0x14007C237`、`0x14007C781`、`0x14007CAE4`；链修正里 `keys[m].next`、`keys[j].next` 同样无界（`0x14007C430`、`0x14007C486`） | 加固；要覆盖 8 个取用点。**本条曾两度误判**，经过见下文专条 |
| `RB-02` 骨骼标记缓冲 | **原版同样无顶点上限**。原版创建与锁定同为 `0x445C0`（`0x14001274B`、`0x140028054`），计数器 `[r12+0x9fd50]` 每四边形加 2（17 处 `add ..., 2`），全部访问里没有与容量比较的，唯一 `cmp` 在 `0x14002AFBA` 是和 0 比。另：批次报告给的「10000 顶点」有误，按 28 字节顶点算容量是 2,500 个四边形 | 加固候选，非移植缺陷；触发条件待实测 |

`B3-01` 的详细依据：移植在 `pmm_load_v2.cpp:682` 用 `i < workspace.displayCount` 遍历
稠密键、在 `:717` 用文件帧号访问稀疏键。原版对应的界是 `boneCount`，而移植在
`pmm_load_v2.cpp:718-719` 已用 `workspace.displayCount` 做同类判断。差别在于零点两侧
各有一个键（原版 0..boneCount-1，移植 0..displayCount-1），两者都是「用骨骼数量当界」，
语义一致。

键池容量作为界在原版载入器里一次都不出现：

```text
== any capacity-comparison address inside the loader 0x140082D00..0x140085000? ==
  (none)
```

原版里 `cmp rcx, 0x927c0`（600000）一类比较确实存在，但都在别处，例如
`0x14003A740` 是按池容量遍历所有槽的**清零循环**：

```text
0x14003A740  mov rax, qword ptr [rdx]
0x14003A743  add rcx, 0x78
0x14003A747  mov byte ptr [rcx + rax - 0x60], sil
0x14003A74C  mov rax, qword ptr [rdx]
0x14003A74F  mov byte ptr [rcx + rax - 0x24], sil
0x14003A754  cmp rcx, 0x927c0
0x14003A75B  jl 0x14003a740
```

那是「按容量遍历」，不是「验一个值合不合法」，不能当成参考界照搬到读取路径上。

**其余「需畸形输入」条目尚未逐条核验。** 它们同源（同一套把文件数据当索引的模式），
但已证实的反例说明不能整批照搬。建议按 `B3-05` 的做法逐条核验后再改，
不要因为条目分类相同就一起加判定。

### 核验结果：PMM v2 · 10 条全部「原版无界」，全部加固

**v2 loader 的实际范围**：`0x140078E30` 起，经
`0x14007EF23`（这是同一 loader 的**中段**，不是另一个函数），到 `0x140082BDB`。
判据：`0x14007D272` 顺序落入 `0x14007D27D`，`0x14007EF1D` 的 `jbe` 之后顺序落入
`0x14007EF23`，且各片段都从序言保存的 `[rsp+0x70]`（`0x140078E6F`）重新取回 app
指针，分别放进 `r14`（`0x140079BA6`）、`r15`（`0x14007E94E`）、`r12`（`0x140082070`）。
只搜第一个 `.pdata` 片段会漏掉 `B3-02`、`B3-08`、`B3-09`、`B3-10`、`B3-13`。

| 条目 | 原版证据要点 | 要覆盖的取用点 |
| --- | --- | --- |
| `B3-01` | 稀疏 display/morph/physics frame 直接 `imul 0x3c` 或 `lea *4` 后加表基址：`0x14007C237`、`0x14007C781`、`0x14007CAE4`；链修正里 `keys[m].next`、`keys[j].next` 同样无界（`0x14007C430`、`0x14007C486`）。**v2 里没有 v1 那道 `cmp dword ptr [rcx+0x3110], esi`** | 8 个取用点全要加固 |
| `B3-02` | 相机 `0x14007E261`、光照 `0x14007E760`、配件 `0x14007F65F`、重力 `0x140081350`、自阴影 `0x14008163E`，读入 frame 到用作下标之间无任何比较。`0x2710`（10000）只以遍历计数出现在 `0x1400820F2`/`0x14008216A`/`0x14008245D`/`0x140082514`；容量常量 `0x4e20`（20000）在整个 loader 不出现 | `1746/1800/1966/2401/2423` |
| `B3-06` | `0x14007BC43` 只有 `js`，紧接 `0x14007BC50` 的 `imul rcx, rcx, 0x108` 索引 264 字节映射表 | `640-642` |
| `B3-07` | 两处（`0x14007A994`、`0x14007B21D`）对 `sourceIndex` 只有与 `-1` 的相等判断（`0x14007A947`、`0x14007B1D8`），没有上界 | `1288`、`1505` |
| `B3-08` | `0x14008186F` 只判 `js`，`0x140081871` 的 `mov rcx, [r12+r11*8+0xbe8]` 直接解引用 `modelSlots` | `2451` 与 `2454` |
| `B3-09` | 槽字节 `0x140081CB4`，`0x140081CC4` 的 `add rdx, 0x96480`，无空判断无上界；循环里的 `cmp bl, [rsp+0x43]` 是模型计数器 | `2495` |
| `B3-10` | `0x14007EFF2`（`accs[accSlot]`）、`0x14007F663`（`accTracks[accSlot]`）、`0x14007FB6A`（`selectedObjectSlot`，来源 `0x14007EE88`）三处都无下标上界；`2310` 那处只有元素空判断 | `1877`、`1966`、`2310` |
| `B3-11` | 四个计数只做溢出检测乘法（`mul` + `cmovo`）后交给 `operator new[]`（`0x14007A323`、`0x14007A7FC`、`0x14007A8F1`，morph 在 `0x14007A5BF`），无范围判断 | `1159/1206/1251/1273`。**附带偏离**：原版靠 `new[]` 抛 `bad_alloc`，移植用 `malloc` 不检查会 `memset(NULL)`，加固要连同 `memset` 与逐项写入一起覆盖 |
| `B3-12` | 只判 `mappedIndex` 的符号（`0x14007CA04`、`0x14007CBC4`），`displayKeys[+0x20]` 表指针无空判断；循环上界取**文件里的** `rigidBodyCount`（`0x14007C9BD`），与模型自身刚体数无关 | 见下条说明 |
| `B3-13` | 链遍历里 `cur` 只与 `prev` 相等比较、与 0 非零比较（`0x1400825DA`、`0x1400826D7`），无上界；`0x14008277E` 的 `cmp esi, [rax+0x3110]` 是骨骼计数器遍历 | `2710-2729`、`2740-2753` |

`B3-12` 的处理需要一个判断：原版同样会在模型刚体数为 0 时把 `displayKeys[+0x20]`
（即 `SelectorStates`）当非空指针写。所以**端口这一处是忠实的**，不是移植偏差。
按「原版无界」处理就是偏离原版加一道判空；不加固则保留原版行为。建议加固并注明，
因为写空指针在正常工程里也会经「刚体数为 0 的模型 + 含刚体状态的项目」触发。

### 核验结果：媒体与导出 · 7 条全部「原版无界」

| 条目 | 原版定位 | 要点 |
| --- | --- | --- |
| `CB-03` | `WaveLoadFile` = `0x1400DA380` | `colBytes` 在 `0x1400DA61A..0x1400DA64A` 算出后，`0x1400DA671` 的 `idiv r12d` 直接用，中间无 `test`/`cmp`。字段：`0x3C` = `nBlockAlign`(2)、`0x38` = `nAvgBytesPerSec`(4)、`0x260` = `dataSize`(4)。加固须在 `310-313/315-316/319/355-360/366` 全部取用点前拦 `colBytes == 0` |
| `CB-04` | 同上 | `0x1400DA66A` 的 `idiv` → `inc` → `0x1400DA676` 的 `movsxd rbx, eax` → `malloc(columns)` → `memset(ptr, 0, columns)`，Max/Min 各一次。`0x1400DA6E0` 的 `cmp [rsi+0x280], 0` 加 `jle` 位于两次 malloc+memset **之后**，是循环入口判断，拦不住。加固：分配前判 `columns <= 0` |
| `CB-07` | app 级 `LoadWaveFile` = `0x14006B400` | 原版同样不关。`0x14006B484..0x14006B4A4` 的内联 reset 只清零 `+0x48`/`+0x258`/`+0x28`/`+0x270`，无 `fclose`/`CloseHandle`/`Release`；真正关句柄的 `CloseDataFile` = `0x1400DA250`（`fclose` 在 `0x1400DA2AB`、`CloseHandle` 在 `0x1400DA2BD`）只在 `parseStream != 0` 时进入，reset 之后进不去。端口 `139-145` 与 `238-239` 逐行忠实，补关闭属有意偏离 |
| `CB-08` | `0x140067D3F` | 原版失败分支同样不释放 `best`。两条失败路径汇到 `0x140067D3F`：`MessageBoxA` → `AVIFileRelease(file)` 在 `0x140067D4C` → `file = 0`；`best` 在 `app+0x9F2E0`，只在 `0x140067C68` 写入，失败路径不碰 |
| `CB-09` | `0x1400DA595`、`0x1400DAAE3` | 原版同样误判。两处 `CreateFileW` 后都是 `cmp qword ptr [ctx+0x270], 0`，而失败返回 `-1` 非 0；原版同样先调 `SetFilePointer`。加固要改用 `INVALID_HANDLE_VALUE` 并跳过 `SetFilePointer`，`288-298` 与 `443-453` 两处一起改 |
| `AB-01` | `FillCodecCombo` = `0x14000A110` | 原版同样只判 `< 0`：`0x14000A1AE` 的 `test eax, eax` 加 `0x14000A1B0` 的 `jns`；`0x14000A1DD` 起对可能为 NULL 的 `enumMon` 执行 `mov rax, [rcx]` 与 `call [rax+0x28]`（即 `Reset`）。原版的空判断在遍历**之后**（`0x14000A3C9`），挡不住 |
| `CB-11` | `ExpGetPmdBoneName` = 序数 22 = `0x1400DB710` | **原版只界模型下标，不界第二个下标。** 模型走 255 槽计数扫描，未命中 `xor eax, eax` 返回 NULL；骨骼下标 `movsxd rax, edx; imul rax, rax, 0x270; add rax, [rcx+0x2748]` 无比较。`ExpGetPmdMorphName`(序数 30) 与 `MorphValue`(序数 32) 步长 `0xC0`、`value` 在 `+0x38`；`ExpGetPmdBoneWorldMat`(序数 24) 在 `0x1400A7370`，`0x1400A745B` 同样乘 `0x270` 无比较。对照序数 29 `ExpGetPmdMaterial` 有 `jbe`，说明原版并非一律不判 |

三条附带发现：

- `CB-08`：原版 `0x140067C51`/`0x140067C56` 在优先级不低于最优时直接丢弃 stream 而不 Release，**端口 `258-260` 已经补了释放**，即端口已有一处自行加固。
- `AB-01`：原版 `BindSelectedCompressor`（独立函数，`ret` 在 `0x14000AA06`）连 HRESULT 都不测（`0x14000A92D` 直接到 `0x14000A930` 的 `Reset`）。所以端口 `:212-217` 的判空与 `:269` 的 `devEnum->Release()` **本身就是端口新增的加固**，不是原版行为。整理注释时要把这两处标出来，免得后人对照原版以为是多余判断。
- `CB-11` 的处理要点：端口 `ModelByIndex`（`103-111`）是**忠实的有界实现，不要动**；加固只加四处第二下标，界用 `boneCount`(`0x3110`) 与 `morphCount`(`0x310C`)。

导出表已从原版 PE 直接解析（RVA `0x140660`，37 项，序数基准 1），与
`exports/MikuMikuDance.def` 完全对应，`CB-11` 的定位可信。

### 核验结果：PMD 载入器与物理回写 · 6 条无界，1 条误报

x64 PMD 载入器定位为 `0x1400B2670..0x1400B5972`，依据是
`0x1400B27F7..0x1400B282F` 逐字节比较 `'P','m','d'`，失配后比较 `'P','M','X'`
并 `call 0x1400A9AC0` 进入 PMX 载入器；`0x1400B28BA` 读 4 字节版本。

| 条目 | 结论 | 原版证据要点 |
| --- | --- | --- |
| `MB-07` | 原版无界 | `0x1400B3B1A` 读 `vertexIndex`，`0x1400B3B1E`/`0x1400B3B21` 直接 `morph0Table + 16*idx` 取值并写回 `vertices[]`，读前无比较 |
| `MB-08` | **误报，撤回** | 原版就是 `strcpy_s(&bone->nameEn, 0x14, &bone->name)`（`0x1400B3F76`/`0x1400B3F7A`），英文分支在 `0x1400B40FF` 自行补 `nameEn[19] = 0`。原版无 `_set_invalid_parameter_handler`/`_invoke_watson` 引用，溢出同样终止 |
| `MB-14` | 原版无界 | `0x1400B376B` 的 `movsxd tailIdx` → `0x1400B3776` 的 `imul` → `0x1400B377D` 读 `bones[tailIdx].type`，无 `test`/`js`。同段的 `parent` 在 `0x1400B371B`/`0x1400B371D` 反而有 `test`+`js` |
| `MB-15` | 原版无界 | `0x1400B3BF1` 的 `movzx targetIndex` → `0x1400B3BF6/3BFA/3BFE` 的 `morphs + 192*idx` → `0x1400B3C05` 的 `strcpy_s`；`morphCount == 0` 时源为 0。加固要覆盖 `529` 与 `638` 两处 |
| `MB-16` | 原版无界 | `0x1400B3C32` 的 `add byte ptr [rsi], 2`、`0x1400B3C3F`/`0x1400B3C41` 的 `inc`，全程 `u8`；`0x1400B3C4B` 用回绕值分配 `101*count`，`0x1400B3C86`/`0x1400B3CCA` 仍写 `groups[0]/[1]/[2]` |
| `MB-24` | 原版无界 | `0x1400B2CBE`/`0x1400B2CC3` 是 32 位 `mov eax, 4` 加 `mul rcx`（`rcx = [rbx+0x28]` 即 `indexCount`），紧接 `operator new`；`0x1400B2CDF` 的 `shl r8, 2` 同样 32 位。加固：`271/284/285` 共用一个已校验长度 |
| `CB-01` | 原版无界 | `0x1400C39D2` 的 `movsxd rec.boneIndex` → `0x1400C39D7` 的 `test` / `0x1400C39D9` 的 `js` 只挡负数 → `0x1400C39E9` 的 `imul 0x270` → `0x1400C3A2F`/`0x1400C3A39` 写 `bones[bi].matInit`。`tailIdx` 五组（`0x1400C3B6D`、`3C41`、`3DD6/3E14/3E52`、`40FD`、`41D1`、`432E/436C/43AA`）全部 `movsxd` 后直接 `imul`；`centerBone` 兜底路径 `0x1400C4745`/`0x1400C474C` 同样。载入侧 `pmd_load.cpp:726/742` 对应 `0x1400B4A2C`/`0x1400B4A3F` 与 `0x1400B49F3..0x1400B4A1D`，也只有 `>= 0` |

两条可复用的定位事实（记下来给后续条目）：导入槽位 `0x140128458` = `_read`、
`0x1401284F0` = `strcpy_s`、`0x1401284E0` = `wcscpy_s`、`0x140128420` = `_write`，
判断读循环里的 `call` 是不是 `_read` 必须看槽位地址。

**`physics_create.cpp:670` 的那道 `record.boneIndex >= boneCount` 是端口自加的加固**，
原版对应位置本轮未展开。后续不要把它当成「原版有界」的先例。

### 已核验条目的误报统计

截至 PMD 组，共核验 **15 条**：`B3-01`、`B3-05`、`B3-02`…（PMM v2 组待回）、`RB-02`、
`CB-01`、媒体与导出 7 条（`CB-03`、`CB-04`、`CB-07`、`CB-08`、`CB-09`、`AB-01`、`CB-11`）、
PMD 组 7 条（`MB-07`、`MB-08`、`MB-14`、`MB-15`、`MB-16`、`MB-24`、`CB-01`，其中 `CB-01`
与上表重复，只计一次）。

**其中 2 条整条误报**（`B3-01` 的误判已因区域判错而作废、重核中，不计入）：

| 撤回或修正的条目 | 误报原因 |
| --- | --- |
| `MB-08` 整条撤回 | 原版用的就是同一个 `strcpy_s`，不是移植引入 |
| `RB-02` 定性修正 | 原版同样没有顶点上限；报告给的容量数字也算错了 |

另有两条是**方向对但细节要修**，不算整条误报：

- `CB-08`：原版失败分支同样不释放 `best`（结论成立），但端口 `258-260` **已经**自行补了释放，所以改动范围比报告说的小。
- `CB-11`：结论成立，但要限定为「第二个下标」；模型下标原版是有界的，端口 `ModelByIndex` 已忠实实现。

误报率约两成。静态审查只读了移植代码、没有对照原版，「移植少了一道界」这类判断
是推断出来的，方向对但结论不一定成立。这是核验优先于动手的直接理由。

### 核验结果：模型运行时 · 9 条加固，1 条按原版改，1 条保持

| 条目 | 结论 | 原版证据要点 | 处理 |
| --- | --- | --- | --- |
| `MB-05` | 原版无界 | `0x1400C204A` 取 `entry.vertexIndex` 后 `0x1400C205D` 的 `imul rdx, rdx, 0xbc` 直接写；组变形路径 `0x1400C18C9`/`0x1400C18E1` 同形 | 加固 `462/476/505/519/544/563/584`，界取 `vertexCount` |
| `MB-06` | 原版无界 | `0x1400C187C` 的 `movsxd rcx, [rdi+rax]` → `0x1400C1884` 的 `shl r10, 6` → `0x1400C1888` 解引用，中间无 `cmp`，**连负数判断都没有**（移植注释所说的「只判负」在 544 行并不存在） | 加固 |
| `MB-17` | 原版无界 | `0x1400BB3D9` 取 `boneRecords[targetBone].parent` → `0x1400BB3FD` 的 `imul rax, rax, 0x270` → `0x1400BB404` 解引用，无判断；`parent = -1` 即读 `boneRecords[-1]` | 加固 `824/829/830/872/1150/1155/1166`。**世界传递相位不要动**：那里原版确有界（`0x1400BB01F` 的 `cmp dword ptr [rdx+0x38], 0` 加 `0x1400BB023` 的 `jl`），对应移植 `764/771`，已忠实 |
| `MB-18` | 原版无界 | `SelectDisplayBone` 在 `0x140071230`；`0x1400712A2` 的 `movsxd rcx, edx` → `0x1400712B4` 的 `mov byte ptr [rcx+rax], 1`，无比较 | 加固 |
| `MB-19` | 原版无界 | `0x1400D000B` 取 `FrameGroup.targetIndex`（`word ptr [rax+r9+0x2a]`）→ `0x1400D0050` 的 `mov byte ptr [rdi+rcx+0x10], 1`，无比较；骨骼组分支 `0x1400D02A5`/`0x1400D02E0` 同形 | 加固 `939`（`morphKeys` 只有 20000 项而 `targetIndex` 上限 65535，属真越界）与 `974` |
| `MB-20` | 原版无界 | 四处空闲槽扫描（`0x1400C9958`、`0x1400C9B3D`、`0x1400CA46F`、`0x1400CA6B9`）都是先 `cmp dword ptr [rcx+rdx], 0`（读 `keys[cursor].frame`）再 `cmp rcx, 0x2255100`（查容量），顺序反了 | 加固 `439/470/574/602` |
| `MB-26` | 原版无界 | `0x140008C45` 的 `cmp r8, 0x2710` 是池遍历计数器判断，退出后**没有**对 `used` 的任何判断；`0x140008C55` 起 `used*24` 直接用于写 `heapBuffer`(+8)/`comObject`(+0x10)/`tag`(+0x18..1a) | 加固 `211/224/226/228`。x64 槽步长是 24，`pool[10000]` 落在 `+240008` = `localeTable`，所以「两种 ABI 都落在 localeTable」成立；移植注释里的「12 字节步长」是 x86 值 |
| `MB-25` | **无下标问题** | 原版 `0x1400CCA1E..0x1400CCA7B` 是七条带位移的平铺存储（`bone+0x148..0x15C` 六个 0、`bone+0x160 = 1.0f`），不按数组寻址；写入的字节集合与移植一致，仅顺序不同 | 按 `model_keyframe_advance.cpp:507-510` 的写法改成两段三元素循环，字节不变。属规范性修法 |
| `MB-29` | **不是界的问题，是位移** | x64 原版析构 `0x1400A9311`/`0x1400A9323` 释放的是 `model+0x2270`（第三个变形表，即 x86 8732 的 x64 对应），不是字面量 8732（`0x221C`），后者在 x64 落在 `gap4` 填充区 | 按原版修正位移（x86 8732 / x64 `0x2270`），并把 `morphTableGap` 从 `RawPad` 提升为指针字段。附注：全 `.text` 对 `[+0x2270]` 只有析构的读与清零，**没有写入点**，所以原版这次 free 实际也是空操作 |
| `MB-27` | 原版同为死判断 | 后缀指针 `0x140009D82` 的 `lea r8, [rbx+rcx*2-8]` 即 `&path[len-3]`；`pmd`/`wav`/`vac` 段用 `repe cmpsw` count 4（三字符加 NUL，能命中），`x`/`X` 段用 count 2（`0x140009F32`/`0x140009F4A`），对三字符后缀恒假 | 移植 `112-113` **忠实，保持**。若要修须记为有意偏离。附注：移植 `107` 行的 `L"P"` 与 `115` 行的 `L"V"` 在原版没有对应物，x64 原版是 `L"PMD"`（`0x14012A3F0`）与 `L"VAC"`（`0x14012A498`），都是 count 4 |
| `RB-06` | 原版无界 | 配件摆放唯一的判断是 `0x1400A19F3` 的 `cmp dword ptr [r10+0x240], -1`（`parentModel == -1`）；`0x1400A1B10` 的 `movsxd r8, [r10+0x244]`（`parentBone`）→ `0x1400A1B1F` 的 `imul r8, r8, 0x270` → `0x1400A1B2D` 解引用，**无上界，连 `max(...,0)` 的下界钳制也没有**。PMM 重映射 `0x140082215`/`0x14008221D`/`0x14008249F` 同样只判 `mapped < 0`，源下标也未校验 | 加固 `accessory.cpp:247-248`、`effect_api.cpp:722`、`pmm_load_v2.cpp:2576-2579` |

本组另一条可复用发现已并入方法文件：端口注释里的 x64 运行时地址按
`0x140000000 + (地址 - 0x7FF7CB420000)` 可换算成文件 VA，三个锚点验证通过。

## 核验总表

已核验 **59 条**。按结论分五类：

| 结论 | 条数 | 处理 |
| --- | --- | --- |
| 原版无界 | 51 | 加固，注释注明原版无此判断 |
| 原版有界，移植不忠实 | 2（`MB-09`、`MB-12`） | 按原版改，不是加固 |
| 原版有界，移植忠实 | 2（`MB-08`、`MB-27`） | 不要改 |
| 无法定位原版位置 | 2（`B3-07`、`B3-11`） | 仍需加固（空指针与负长度是确定的崩溃），但注释只能写「原版位置未定位」 |
| 无下标问题，按规范改 | 2（`MB-25`、`MB-29`） | `MB-25` 改写法字节不变；`MB-29` 修位移 |

`B3-12` 计在「原版无界」内，但有附注：原版同样会把空表指针当非空写，端口此处忠实，
所以加判空属有意偏离。

`MB-08` 是唯一整条撤回的条目，`B3-01` 经两轮重核后确认为真缺陷。

### 逐条结论

| 条目 | 结论 | 条目 | 结论 |
| --- | --- | --- | --- |
| `B3-01` | 原版无界 | `MB-11` | 原版无界 |
| `B3-02` | 原版无界 | `MB-12` | 原版有界，移植不忠实 |
| `B3-03` | 原版无界 | `MB-13` | 原版无界 |
| `B3-04` | 原版无界 | `MB-14` | 原版无界 |
| `B3-05` | 原版无界 | `MB-15` | 原版无界 |
| `B3-06` | 原版无界 | `MB-16` | 原版无界 |
| `B3-07` | 无法定位 | `MB-17` | 原版无界（世界传递相位已忠实） |
| `B3-08` | 原版无界 | `MB-18` | 原版无界 |
| `B3-09` | 原版无界 | `MB-19` | 原版无界 |
| `B3-10` | 原版无界 | `MB-20` | 原版无界 |
| `B3-11` | 无法定位 | `MB-21` | 原版无界 |
| `B3-12` | 原版无界（端口忠实，见说明） | `MB-22` | 原版无界 |
| `B3-13` | 原版无界 | `MB-23` | 原版无界 |
| `B3-14` | 原版无界 | `MB-24` | 原版无界 |
| `B3-15` | 原版无界 | `MB-25` | 无下标问题，改写法 |
| `B3-16` | 原版无界 | `MB-26` | 原版无界 |
| `B3-17` | 原版无界 | `MB-27` | 原版有界（同为死判断），保持 |
| `B3-18` | 原版无界 | `MB-29` | 位移错，按原版改 |
| `B3-19` | 原版无界 | `CB-01` | 原版无界 |
| `B3-20` | 原版无界 | `CB-03` | 原版无界 |
| `B3-21` | 原版无界 | `CB-04` | 原版无界 |
| `AB-01` | 原版无界 | `CB-07` | 原版无界 |
| `MB-01` | 原版无界 | `CB-08` | 原版无界（端口已自加释放） |
| `MB-02` | 原版无界 | `CB-09` | 原版无界 |
| `MB-03` | 原版无界 | `CB-11` | 原版无界（仅第二下标） |
| `MB-04` | 原版无界 | `RB-02` | 原版无界（原版也无顶点上限） |
| `MB-05` | 原版无界 | `RB-06` | 原版无界 |
| `MB-06` | 原版无界 | `MB-07` | 原版无界 |
| `MB-08` | 原版有界，移植忠实 | `MB-09` | 原版有界，移植不忠实 |
| `MB-10` | 原版无界 | | |

### 误报与偏差统计

59 条已核验里：

- **整条误报 1 条**：`MB-08`。原版用的就是同一个 `strcpy_s`，不是移植引入。
- **结论作废重做 1 条**：`B3-01`。最终确认为真缺陷，但两次判错本身消耗了一轮核验。
- **需要收窄范围 3 条**：`CB-08`（端口已自加释放，改动比报告小）、`CB-11`（只有第二个下标要加）、`RB-02`（原版同样无上限，且报告给的容量数字算错）。
- **原版比移植更宽 1 条**：`RB-06`。原版连 `max(..., 0)` 的下界钳制都没有，移植现有那半个钳制本身就是加固。

结论：静态审查「移植少了一道界」这个方向判断成立率很高（51/59 属原版无界），
但具体范围、容量数字、以及是否存在原版界，靠推断不可靠。**动手前逐条核验是必要的**，
且核验时必须自行复核锚点，不要直接采信任务给的区间。

### 尚未核验的条目

其余 99 条分两类：

- **常态可达或环境相关，不需要原版核验**（约 11 条）：`B2-01`、`RB-01`、`RB-03`、`RB-04`、`RB-05`、`B1-01`、`CR-001`、`CR-002` 等。这些在正常输入下就会触发，原版在正常输入上不崩，所以直接修，不存在口径问题。
- **需畸形输入但尚未核验**：多为 S2/S3 级，例如 `B3-22` 到 `B3-30`、`MB-28`、`MB-30`、`B2-*` 各条。开工前按同样的方法逐条核验。

`B3-07` 与 `B3-11` 属「无法定位」：两条都必须加固（空指针解引用与负长度分配是确定的
崩溃，与范围界无关），但原版对应位置未找到，所以注释只能写「原版位置未定位」，
不能写「原版无此判断」。

### 核验结果：PMX 载入器 · 10 条加固，2 条按原版改

x64 PMX 载入器定位为 `0x1400A9AC0..0x1400B246B`：`.pdata` 切成 14 个顺序贯穿的分块，
唯一外部调用点是 `0x1400B286B`（分派器 `0x1400B277D` 在 `0x1400B27E5` 先读 3 字节）。
方法文件早先把 `0x1400B2409` 当作「格式分派」不准确，那是版本不符的错误出口，
`0x1400B241E` 引用的是 `"PMX files version is different from 2.0."`。

**加固类（原版无界）**

| 条目 | 原版证据要点 |
| --- | --- |
| `MB-01` | 附加 UV 循环的界取自 `model+0x21F1` 的有符号字节；`0x1400AA33E` 只判 `> 0`，`0x1400AA3D2` 每轮重读，全程无 `cmp …, 4` |
| `MB-02` | `0x1400AC07C` 取 `indices[faceCursor]`、`0x1400AC07F`/`0x1400AC093` 用 `vi`，既不比 `indexCount` 也不比 `vertexCount`；`0x1400AC0A3`/`0x1400AC0B5` 是按材料遍历，不算界 |
| `MB-03` | `0x1400ACF56` → `0x1400ACF65` 写 `bones[ikTarget].type = 6`；`0x1400AD0A9` → `0x1400AD0B4` 写 `bones[link.boneIndex].type = 4`，都没判范围 |
| `MB-04` | `groupCount` 存成 `u8`，`0x1400B03E5` 按 `101 字节 × u8` 分配，随后无条件写 `[0]`/`[1]`，facial 时写 `[2]` |
| `MB-10` | `parent` 只判 `js`（`0x1400AD209`）；`tailIdx` 连负值都不判（`0x1400AD25D` → `0x1400AD26C`） |
| `MB-11` | `0x1400B02FF` → `0x1400B0317` 直接 `morphs + index*192`；`morphCount == 0` 时 `0x1400AD3B0` 跳过分配，指针保持原值仍被相加 |
| `MB-13` | `0x1400B0790` 取 `u16`，`0x1400B07A7` 读回后乘 `0x270` 索引 `bones[]`，无比较 |
| `MB-21` | `0x1400B1112` 的 `setg` 加 `0x1400B1117` 的 `setns`，只要求 `mode > 0` 且 `bi >= 0`；逆变换同索引只在 `0x1400B114C` 判 `js` 后直接用 |
| `MB-22` | `0x1400B159E`/`0x1400B15C2`/`0x1400B1652`/`0x1400B167B` 直接存；`0x1400B191A` → `0x1400B192E` 直接乘 `0xC0` 取 `invTransform`，`rigidB` 同路 |
| `MB-23` | 池定长 `0x2255100`（600000 × 60）；`0x1400B22F5` 只判 `boneCount > 0`，循环界就是 `boneCount`，无容量比较 |

**按原版改（原版有界，移植不忠实）** —— 这类不是加固，是修行为去对齐：

| 条目 | 原版证据要点 | 修法 |
| --- | --- | --- |
| `MB-09` | `0x1400AA137` 起是对 `0..4` 的**显式比较链**，全不等则走 `0x1400AA1F6` 的错误路径；原版从不拿该字节做表下标 | 移植的 `kSpec[count & 7]`（表长 5）不忠实。按原版改成显式分支，`≥ 5` 走报错返回。注意 `≥ 5` 分支读的是 `[rbp+0x80]`，该栈槽此前无写入，所以原版对 `≥ 5` 不是可复现的拒绝；落点选「报错返回 false、不建主 VB」这条可复现路径 |
| `MB-12` | 原版计数遍历全部帧（`0x1400B0224`），填充也遍历全部帧（`0x1400B0392`），两者覆盖同一组帧 | 移植把填充限成 `f < 2` 导致计数与填充不一致。按原版修填充循环去掉 `f < 2`，并去掉原版没有的 `frames[0].special` 前提（该字节在 `0x1400B0034` 读进栈槽后再未被使用） |

**置信度**为 `低` 的条目在动手前应先按条目内注明的步骤复核。

**本清单未做**：未反汇编原版二进制；未做模糊测试或构造畸形样本实测；
未做性能测试；未修改任何生产代码。

---

## S1 —— 崩溃、内存损坏或数据丢失（36 条）

保留 `B3-01` 的编号，以便与批次报告对照。该项在核验中曾被判为非缺陷，经第三轮重核后确认为**真缺陷**（原版 v2 载入器无此界），仍计入 36 条，处理方式为加固。两度误判的经过见下文 `B3-01` 专条。


### B3-05 模型槽字节 · 已核验原版无校验 · 按加固实现

`src/io/pmm_load_v2.cpp:1025`（v1 同形：`src/io/pmm_load_v1.cpp:483`）

模型记录前的槽字节直接当 `modelSlots[]`（255 项）的下标，无上界校验。
`modelSlots[255]` 的地址正好是 `slotIdx`，一次 8 字节写覆盖 `slotIdx`+`editMode`，
随后 `slots[SelectedModelSlot()]` 用垃圾下标记解引用。合法域只有 0..254：
写侧 `pmm_save.cpp:704` 是 `slot != 0xFF` 的循环，释放侧 `scene_ownership.cpp:89`
是 `< 255`。

原版核验（9.32 x64）：

```text
0x1400835E1  0f b6 4c 24 40        movzx ecx, byte ptr [rsp + 0x40]
0x1400835E6  33 d2                 xor edx, edx
0x1400835E8  49 89 84 cf e8 0b 00 00  mov qword ptr [r15 + rcx*8 + 0xbe8], rax
0x1400835F0  0f b6 4c 24 40        movzx ecx, byte ptr [rsp + 0x40]
0x1400835FB  49 8b 8c cf e8 0b 00 00  mov rcx, qword ptr [r15 + rcx*8 + 0xbe8]
```

`0xbe8` 即 3048，与 `include/mikudancestudio/app_layout.hpp` 的 `modelSlots`
偏移一致。读字节与写槽之间没有任何比较；同一个字节在 `0x1400835F0`、`0x140083608`
又被复用了两次。

加固要覆盖**全部取用点**，不能只改第一次出现的位置：

- v2：`:1025` 写入、`:1026` memset、`:1027` 初始化默认值、`:1050` 再取一次，
  另有 `:1135` 在跳过路径写 `slots[slotByte]`。
- v1：`:483`、`:484`、`:485`，以及对应的跳过路径。

做法：槽字节越界时按「跳过本模型」处理，仍消费该记录的字节以保证流位置不变，
并把模型指针留在局部变量里供 `ModelDispose` 使用。加注释写明这是**有意加固**、
原版无此判断。

### B3-01 稀疏关键帧帧号 · 已核验：原版无界，加固

`src/io/pmm_load_v2.cpp:717`（另 738/747/750/788/804/812/862）

原版 v2 里，稀疏 display/morph/physics 记录的帧号直接用来算记录地址，中间没有任何
比较：

```text
0x14007C237   imul ... , 0x3c      ; display 稀疏记录，60 字节步长
0x14007C781   lea  ..., *4         ; morph 稀疏记录
0x14007CAE4   ...                  ; physics 稀疏记录
0x14007C430 / 0x14007C486          ; 链修正里的 keys[m].next / keys[j].next，同样无界
```

**v2 里没有 v1 的那道界。** v1 在 `0x140083B54` 有
`cmp dword ptr [rcx + 0x3110], esi` 加 `jle`（`0x3110` 是 `BoneRecord` 的父级
`boneCount`），而 v2 的对应位置没有任何等价判断。

处理：加固，要覆盖 8 个取用点（`717`、`738`、`747`、`750`、`788`、`804`、`812`、
`862`）。界取对应池的容量（`BoneKey` 600000、`MorphKey` 20000、`DisplayKey` 1000）。

#### 本条曾两度误判，过程记录

第一次判成「原版有界、移植已忠实」，依据 `0x140083B54` 那道 `cmp`。但那个地址属
**v1** 主体：`0x140082D9C` 的 `je 0x140082e1c` 是 `"0001"` 的入口。第二次改对区间后
又把 v2 切窄成第一个 `.pdata` 片段 `0x140078E30..0x14007D27D`，而 v2 实际延续到
`0x140082BDB`，中间 `0x14007EF23` 那一段不是独立函数。

两次判错的教训记在 `docs/VERIFY_METHOD.md`：分派处的跳转方向要按被比较的串读，
`.pdata` 的边界不等于控制流边界。

### B2-01 常态可达 · 置信度高 · MEM

`src/window/command_view_menu.cpp:526`

模型计算顺序对话框的 OK 处理用 `ModelSlot(order[i])` 寻址，但该数组存的是
**槽号 + 1**。写入方在 `src/window/misc_dialogs.cpp:567-577`（`order[ord] = slot + 1`）。

触发：载入 3 个模型 → 删除中间那个（槽 1 变空、槽 0/2 仍有模型）→ 背景菜单
「model calculate order」（id 289）→ 确定。此时 `order[1] == 1`，
`ModelSlot(1) == nullptr`，`mdl::Mdl(nullptr)->comboSelIndex = i` 向空指针附近
（`0x2D7C`）写 1 字节 → 访问违例。槽 254 有模型时还会取 `ModelSlot(255)`，
越过 255 槽数组读到 `slotIdx`/`editMode` 拼成的伪指针并写入。

同族 `ApplyModelDisplayOrderDialog` 已正确使用 `order[i] - 1`，两处应统一。

### RB-03 常态可达 · 置信度高 · MEM

`src/render/debug_geometry.cpp:448`

`DrawBoneOperationAxis` 直接解引用 `Mdl(SelectedModel())->selectedBone`。
空场景（未载入任何模型）时 `src/render/frame_scene.cpp:492-497` 每帧调用它，
空指针崩溃。同一访问在 `src/render/sprite_overlay.cpp:285-287` 与
`src/app/frame_driver.cpp:111-116` 都判了空，此处漏。

### RB-02 骨骼标记叠加缓冲 · 已核验原版同样无顶点上限 · 边界条件待确认

`src/render/sprite_overlay.cpp:473`（另一处标记循环在 `:452`）

批次报告称「循环无容量上限，锁定区 10000 顶点 / 1,666 个四边形」。**该数字有误**，
按源码重算：

- `SpriteOverlayVertices` 由 `CreateVertexBuffer(0x445C0, USAGE_WRITEONLY, 0x144, POOL_MANAGED, ...)`
  创建（`src/window/ui_init.cpp:397-399`），锁定长度也是 `0x445C0`（`sprite_overlay.cpp:261`）。
- `SpriteVertex` 是 28 字节（`sprite_overlay.cpp:21-31`，带 `static_assert(sizeof == 28)`），
  一个四边形 4 个顶点 = 112 字节。
- 所以容量是 280,000 / 112 = **2,500 个四边形**，不是 1,666。

原版核验：

```text
0x14001274B  mov edx, 0x445c0          ; CreateVertexBuffer size in [r12+0x9fd48]
0x140028054  mov r8d, 0x445c0          ; Lock length
0x140028062  mov dword ptr [r12 + 0x9fd50], r15d   ; counter = 0
0x140028604  add dword ptr [r12 + 0x9fd50], 2      ; one quad (17 such sites)
0x14002AFBA  cmp dword ptr [r12 + 0x9fd50], r15d   ; compares against 0, not a cap
```

原版创建与锁定的长度、以及「每四边形给计数器加 2」都与移植一致，**原版也没有
顶点数上限**：`[r12+0x9fd50]` 的全部访问里没有与容量比较的，唯一的 `cmp` 是
和 0 比。因此这不是移植漏掉的界，而是两边都留的缺口。

结论：属于加固候选，但不是移植缺陷。触发需要**场景内可 emit 标记的骨骼总数**
超过容量。单个模型达不到：一只 344 骨骼的模型约 0.9 MB 至 1.2 MB，远小于 280 KB
缓冲乘以 4 的空间……实际阈值取决于四边形平均 28 字节乘以每个标记占 4 个顶点，
即约 2,500 个标记。多模型叠加的大场景可能达到，但本轮未测出具体工程的实际骨骼
总数，所以**触发条件未经实测确认**。

移植侧另有一处不对称：同一函数只给线段批次加了 5,000 上限
（`sprite_overlay.cpp:341-343/355/435`），标记批次没有。这暗示原作者对线段批次有
预算意识，标记批次则没有。

处理：按加固给标记循环加顶点预算判断；动手前先测一个已知大场景的骨骼总数，
确认阈值是否真的可达。

### RB-01 边界输入 · 置信度高 · LIFE

`src/render/accessory.cpp:771`

`NormalizeAccessoryMeshFvf` 失败时 `ReleaseCom(meshSlot)` 不回写 `nullptr`，
调用方 `LoadAccessoryFile`（`accessory.cpp:864`）随后 `DisposeAccessory`，
同一 `ID3DXMesh` 被释放两次。PMD/PMM 载入路径（`pmm_load_v1.cpp:1035`、
`pmm_load_v2.cpp:1885`）不 Dispose，于是留下「已释放但非空」的 `record.mesh`，
后续每帧 `DrawSubset` 对已释放网格调虚函数。

修法：`Release` 之后把槽位置空。

### CB-03 需畸形输入 · 置信度高 · MEM

`src/media/wave_audio.cpp:319`

`colBytes` 为 0 时（`nBlockAlign == 0`，或 `nAvgBytesPerSec < 390 * nBlockAlign`）
`audio->dataSize / colBytes` 触发整数除零（0xC0000094）。

### CB-04 需畸形输入 · 置信度高 · MEM

`src/media/wave_audio.cpp:321-325`

波形数组按文件里 data 块的长度分配。`dataSize` 最高位为 1 时 `columns` 为负，
`malloc` 返回空，随后 `memset(nullptr, 巨量)` 崩溃。分配返回值未检查。

### CB-02 常态可达（来自 MME 插件）· 置信度高 · MEM

`src/exports/effect_api.cpp:719-726`

`ExpGetAcsWorldMat` 对 `acc->parentModel` 既不查范围也不查父槽是否为空，
直接 `ModelSlot(parentSlot)` 再取 `Bones(model)`。`parentModel` 为 -2 等越界值时
会把 `accKeyTracks` 尾部当指针解引用。

同字段的其它五个消费点都有守卫（例如 `src/render/accessory.cpp:241-245`）。

### AB-01 环境相关 · 置信度高 · MEM

`src/app/dshow_record_graph.cpp:272`

`FillCodecCombo` 只判 `CreateClassEnumerator` 的返回值 `< 0`，未判
`enumMon == nullptr`（类别为空时 DirectShow 返回 S_FALSE + NULL），
随后 `enumMon->Reset()` 空指针调用。打开 AVI 出力对话框即触发
（`src/window/command_file_menu.cpp:585` 的 `WM_INITDIALOG`）。
同文件 `BindSelectedCompressor`（`:212-217`）有该判空，属漏写。

### MB-08 PMD 20 字节骨骼名 · 已核验为误报 · 撤回

`src/model/pmd_load.cpp:588`（另 617）

**核验结论：不是缺陷。** 原版做的是同一件事，用的是同一个函数：

```text
0x1400B3F76  strcpy_s(&bone->nameEn, 0x14, &bone->name)
0x1400B3F7A  (同一表达式继续)
0x1400B40FF  英文分支自行补 nameEn[19] = 0
```

`_read` 与 `strcpy_s` 的导入槽位经解析导入名表确认为 `0x140128458` 与
`0x1401284F0`，所以判定依据是可靠的。原版对自己的溢出同样终止：二进制里没有
`_set_invalid_parameter_handler` 或 `_invoke_watson` 的引用。

处理：**不要改，也不要另加截断。** 移植在这处逐行忠实，加判断反而偏离原版。

原先的严重度 S1 一并撤回，总计数相应调整（见文首）。

### MB-01 需畸形输入 · 置信度高 · MEM

`src/model/pmx_load.cpp:395`

PMX 头里的 additional-UV 计数未夹取到 0..4，却按该字节逐顶点循环写
`additionalUvByComponent[4][4]`。计数为 5 起覆盖 `weightType`；≥16 时每个顶点
越界写约 900 字节。同一来源在 `pmx_load.cpp:344` 的 `kSpec[count & 7]`（表长 5）
也有同类问题。

### MB-04 需畸形输入 · 置信度高 · MEM

`src/model/pmx_load.cpp:1337`

`displayGroups` 按截断到 `u8` 的 `groupCount` 分配，却固定写 `[0]/[1]/[2]`。
帧数为 0/1 或 `groupCount ≥ 256` 时堆越界写 101~202 字节。

### MB-02 需畸形输入 · 置信度高 · MEM

`src/model/pmx_load.cpp:679`

逐顶点材质归属表用「材质面数累加」和文件里的顶点索引直接索引
`edgeKey`/`vertMat`/`PmxIndices`，三处都无边界。Σ材质面数 > `indexCount`、
索引 ≥ `vertCount`，或 `indexCount == 0` 时 `PmxIndices` 为空指针直接访问违例。

### MB-03 需畸形输入 · 置信度高 · MEM

`src/model/pmx_load.cpp:851`

IK 转换段用文件里的 `ikTarget` 与每个 link 的 `boneIndex` 直接写 `bones[...]`
（-1 会写 `bones[-1]`，落在堆头）。

### MB-05 每帧路径 · 需畸形输入 · 置信度高 · MEM

`src/model/model_skinning.cpp:462`

PMX 顶点 morph 累加用未校验的 `entry.vertexIndex` 索引顶点数组。载入器只给
vmap 加了门，没给条目本身加。每帧蒙皮读写 12 字节越界。

### MB-06 每帧路径 · 需畸形输入 · 置信度高 · MEM

`src/model/model_skinning.cpp:544`

组 morph 引用的 `morphIndex` 只挡了负数，上界未校验就索引 `morphs[]`。
同语义在 `src/model/morph_apply.cpp:239/280` 有完整门，属漏写。

### MB-07 需畸形输入 · 置信度高 · MEM

`src/model/pmd_load.cpp:510`

PMD 后续 morph 的顶点索引经 `BaseVertexMorphTable` 重映射时用文件值直接索引该表，
越界读出垃圾索引后写入 `vertices[]`（写点在 `model_skinning.cpp:584`）。

### AB-02 常态可达 · 置信度高 · PORT

`src/app/key_ladder.cpp:223`

`G` 键块丢掉了原版的三分支（跳帧 / 自阴影 / 细影提示），改为循环坐标轴，
与本文件里的 `L` 键重复。`FineShadowModeNotice`（定义在
`src/window/command_view_menu.cpp:1651`、声明在 `key_ladder.cpp:64`）因此成为
无调用者的死函数；Shift+G 应切换的 `app+0xA1DC8` 字节没有任何写入者。
该块是全梯唯一没有 VA 锚点的块。

### B3-01 … B3-21 —— PMM/VPD/VSQ/VMD 读取路径 · 需畸形输入 · 置信度高

共同根因：**把文件里读来的帧号、槽字节、计数或链指针直接当数组下标或分配长度**。
越界位置多处恰好落在相邻状态字段上，一次写即破坏应用状态。

本组 21 条已在「核验总表」中逐条给出结论：`B3-01` 核验为原版 v2 无界（此前曾两度误判为非缺陷，经重核推翻，详见上文专条），`B3-05` 核验为原版无校验，其余各条亦以该表为准。本表中的行号，以及「原版是否同样无界未核验」等旧措辞均已过期，应以「核验总表」为准。

| 编号 | 位置 | 内容 |
| --- | --- | --- |
| B3-01 | `pmm_load_v2.cpp` 中的 8 个取用点 | **真缺陷，按加固实现**。原版 v2 未设此边界；此前两度判定为非缺陷的记录均已作废，详见上文专条 |
| B3-02 | `pmm_load_v2.cpp:1746`（另 1800/1966/2401/2423） | 相机/照明/配件/重力/自阴影五张全局轨道的帧号当下标，每表 10000。原版是否同样无界未核验 |
| B3-03 | `pmm_load_v1.cpp:629`（另 644/675） | v1 的骨骼/形态/显示池同样无门；675 命中后还会用越界记录里的 `ikStates` 指针再写一次。v1 是否与 v2 同样有 `boneCount` 界未核验 |
| B3-04 | `pmm_load_v1.cpp:886`（另 947/1116/1595/1655） | v1 的全局轨道同样无门 |
| B3-05 | `pmm_load_v2.cpp:1025`；v1 见 `pmm_load_v1.cpp:483` | 模型槽字节未校验。**已核验原版同样无校验**，按加固实现。详见上文专条 |
| B3-06 | `pmm_load_v2.cpp:640` | `boneMappings[selectedBone]` 只判 `>= 0`，越界读 264 字节步长的映射表 |
| B3-07 | `pmm_load_v2.cpp:1288`（另 1505） | 刚体重映射用文件里的 `sourceIndex` 直接索引骨骼名字表 |
| B3-08 | `pmm_load_v2.cpp:2451`；v1 见 `:1670` | `cameraParentModel` 无上界即索引 `slots[]` 并解引用。v1 的姊妹代码对 `accessory.parentModel` 做了该判断，属遗漏 |
| B3-09 | `pmm_load_v2.cpp:2494` | 逐模型注册帧块用文件槽字节索引 `slots[]`；空槽时写地址 `0x4CCF0`（`offsetof(frameRegistrationSelection)`） |
| B3-10 | `pmm_load_v2.cpp:1877`（另 1958/2310）；v1 见 `:1027/1108/1148` | 配件槽字节与当前选中字节未校验。合法域只有 0..254——写侧 `pmm_save.cpp:704` 是 `slot != 0xFF` 的循环，释放侧 `scene_ownership.cpp:89` 是 `< 255`。`accs[255]` 正好是 `lastRegisteredFrame` |
| B3-11 | `pmm_load_v2.cpp:1159`（另 1206/1251/1273） | 四处映射表按文件计数 `malloc`，返回值不检查，极大或负计数触发 `memset(NULL)` |
| B3-12 | `pmm_load_v2.cpp:846`（另 884） | 模型刚体数为 0 时 `SelectorStates` 为空，仍按 `mappedIndex = 0` 写入 |
| B3-13 | `pmm_load_v2.cpp:2714`（另 2746）；v1 见 `:1838/1873` | 链完整性遍历把文件里的 `next` 当数组下标 |
| B3-14 | `vpd_file.cpp:147`（另 173） | 骨骼计数不校验即用于 `operator new`，负值/巨值时未捕获的 `bad_alloc` 终止进程 |
| B3-15 | `vpd_file.cpp:224` | 尾骨骼索引未校验即用于 `bones[bone->tailBone]` |
| B3-16 | `vsq_load.cpp:279` | `wcsstr` 返回 NULL 时 `+1` 得到地址 2 并写入 |
| B3-17 | `vsq_load.cpp:431`（另 448/469/484/489/496） | `fgets` 扫描不判 EOF，文件缺标记时死循环 |
| B3-18 | `vsq_load.cpp:436`（另 478/494/524） | `strstr` 结果不判空直接解引用 |
| B3-19 | `vsq_load.cpp:285`（另 428/468/692） | `_wfopen_s` 失败后继续对 NULL 调 `fprintf`/`fgets` |
| B3-20 | `vmd_save.cpp:189`（另 224） | 沿 `previous` 回溯求所属骨骼，索引来自键记录且无上界 |
| B3-21 | `vmd_save.cpp:305` | IK 链的 `boneIndex` 未校验即用于 `bones[boneIdx].name` |

### CB-01 需畸形输入 · 置信度高 · MEM

`src/physics/physics_frame.cpp:699`（另 360/712/757）

物理回写用模型文件里的 `rec.boneIndex` / `bone.tailIdx` 索引 `Bones(m)[]`，
只判了 `>= 0`，越界时向骨骼表外写 64 字节。加载器同样不夹取
（`pmd_load.cpp:726/742`、`pmx_load.cpp:1459`），而 `physics_create.cpp:670` 有正确写法。

### 其余 S1（`model/` 加载路径）· 需畸形输入 · 置信度高

| 编号 | 位置 | 内容 |
| --- | --- | --- |
| MB-09…MB-23 | 见各条 | `pmx_load.cpp` 与 `pmd_load.cpp` 的 parent/tailIdx/刚体/关节/facial 索引、`displayGroups` 的 u8 回绕、`bone_transform.cpp:830` 的 IK 链端点（`boneRecords[-1]`）、`model_query_gaps.cpp:291` 的骨骼写下标、`key_registrars.cpp:439/939` 的空闲槽与显示表下标 |
| B2-08 | `physics_model_dialog.cpp:1128` | 提交物理编辑时用文件来的 `boneIndex` 无上界索引骨骼表并回写 `hasRigidBody` |

---

## S2 —— 用户可见的功能错误（33 条）

`MB-08` 已核验撤回，编号保留以便与批次报告对照，不计入 33 条。

### CR-001 已实测复现 · v2 工程不恢复相机参照模式

`src/io/pmm_load_v2.cpp:2088-2100`

v2 载入器读出「相机参照模式」字节后只写给对话框复选框，**没有写回**
`s->state.cameraReferenceMode`。同字段在 v1 载入器是写回的
（`pmm_load_v1.cpp:1217`）。`BM_SETCHECK` 不触发命令 412，
而命令 412（`src/window/command_control_400.cpp:2913-2932`）才是唯一把该字段写进
状态的地方。

影响面很窄但确定：v2 载入器里 `Rd` 出的其它三个配置字节
（`playbackLoopEnabled`、`playbackReturnsToStartFrame`、`playbackStartsAtCurrentFrame`）
都写回了状态（`pmm_load_v2.cpp:2102/2105/2108`），只有这一个漏掉。
`pmm_load_shell.cpp:128-137` 按版本分派，0002 只走 v2，所以 v2 工程永远拿不回该字段。

后果：该字段为 `None`(0) 时 `SetupFrameWorldTransform` 走 `cameraView` 分支
（`src/render/debug_geometry.cpp:594-597`），取景中心用 `CameraPositionX/Y/Z`；
工程记录的 `ModelRoot`(1) 本应走 `target && !modelReference` 分支（同文件 586-593），
取被参照骨骼的位置。两者观景中心不同，载入工程后视口偏离工程记录的取景。

实测：`scene_lifecycle_tests.exe <Stage.pmm> 3 --playback-view` → exit 7。
旁证：该工程为 0002 版，记录流开头 `0x28: 01 01 01 01 01` 按
`pmm_save.cpp:758-775` 的字段顺序覆盖 `editMode` 与 `cameraReferenceMode` 的位置。

修法：在 `pmm_load_v2.cpp:2090` 读入后补 `s->state.cameraReferenceMode = b;`。

### CR-002 已修 · 参照模式补偿读错了旧枢轴

`src/app/frame_modes.cpp:864-896`、`src/render/debug_geometry.cpp:566-617`（提交 `cbe5f76`）

**状态：已修并实测通过。** 根因既不是符号问题，也不是「以哪个字段为权威」的问题，而是补偿逻辑在解析旧枢轴时用错了函数。原有实测数据仍列于下方，与下述推导一致。

**实测环境**：`scene_lifecycle_tests.exe <Stage.pmm> 1 --view-tools`，即参照模式由 `None` 切换至 `ModelRoot` 的那一步（测试在 `:220` 点复选框）。补偿函数内部（临时打点）：

```text
oldMode=0  newMode=1
tOld = (0, 0, 0)        tNew = (-40, -6, 8)      // 各乘 rot 之后
CameraPosition = (-1.8, 0.9, -36)
ViewOffset 前后 = (3, 12, -70)  ->  (43, 18, -78)     // 当时为 +=
```

渲染侧（在 `SetupFrameWorldTransform` 末尾打点）与测试侧读到的矩阵：

```text
eye = (3, 12, -70)   -> 视图矩阵平移读出 (-3, -12, 70)
eye = (43, 18, -78)  -> 视图矩阵平移读出 (-43, -18, 78)
before: W = (-3, -12, 0)    V = (-3, -12, 70)
after:  W = (-40, -6, 8)    V = (-43, -18, 78)
```

**合成关系与所需值**：世界矩阵为 `world = T(-focus) * R`，视图矩阵为固定的 `+Z` look-at `V = T(-eye)`，故画面平移量为 `-R(focus) - eye`。切换参照时要保持画面不动，就须保持 `R(focus) + eye` 不变，即

```text
eye_new = eye_old + R(focus_old - focus_new)
```

代入实测数据（`eye_old = (3, 12, -70)`、`focus_old = (3, 12, 0)`、`focus_new = (40, 6, -8)`，相机角度为零故 `R = I`）：

```text
eye_new = (3, 12, -70) + ((3, 12, 0) - (40, 6, -8)) = (-34, 18, -62)
```

代回校验：`W_after = (-40, -6, 8)`、`V_after = (34, -18, 62)`，和为 `(-6, -24, 70)`，与切换前的 `(-3, -12, 0) + (-3, -12, 70)` 一致。

**误差的精确解释**：原 `-=` 实现（`tOld` 取零）实测 `eye = (-37, 6, -62)`，与本条所需值之差恰为 `(-3, -12, 0)`，即被丢掉的 `focus_old`；HEAD 的 `+=` 实测 `eye = (43, 18, -78)`，等于 `eye_old - (focus_old - focus_new)`，方向相反。两个错误相互独立，因此此前「四种写法逐一实测」每次只改变一个变量，均告失败，并由此得出「问题不在符号」的结论。

**修正**：新增 `ResolveFrameFocus(app, mode, focus)`，将原先内联在 `SetupFrameWorldTransform` 中的四路枢轴选择移入该函数，渲染与补偿共用；补偿逻辑保留原 `-=`（对旋转后的 `tOld`/`tNew` 求差），仅更换枢轴来源。当 `oldMode` 为 `None` 时，该函数给出自由视图的 `(ViewOffsetX, ViewOffsetY, 0)`，即渲染实际使用的那个点。

**实测**：`--view-tools` 由 exit 7 转为 exit 0，`:227-230` 的 `changing orbit reference preserves the current view` 通过；`ctest` 22/22；`--playback-view`、`--bone-movement`、`--physics-selection`、`editor` 均为 exit 0。

**遗留一处**：切回自由视图（将参照改为 `None`）时，新枢轴即为本次写回的 `ViewOffsetX/Y`，方程自耦合，精确解需满足 `eye = eye_old + R(focus_old - (eye.x, eye.y, 0))`。该方向不在测试断言覆盖范围内，目前按预修正值计算，画面仍会发生位移；已在源码注释中记录。

**此前记录中需修正的数字**：本条旧稿列出了三处互相矛盾的「所需值」，分别写作 `(43, 6, -78)`、`(37, 18, 78)` 与 `(40, 6, -78)`（前两处混用了「视图矩阵平移 = `-eye`」与「`eye`」两种符号约定）。现按本条所需值 `(-34, 18, -62)` 重新比对旧稿记录的四次实验：

| 写法 | 实测 `eye` | 与所需值比较 |
| --- | --- | --- |
| `+= (tOld - tNew)`（旧稿 HEAD 现状） | (43, 18, -78) | 方向相反，仅 y 巧合一致 |
| `-= (tOld - tNew)`，`tOld` 取零 | (-37, 6, -62) | 相差 `(3, 12, 0)`，即丢失的 `focus_old` |
| `.x -=`、`.y +=`、`.z -=` | (-37, 18, -62) | 仅 x 相差 `3` |
| 改补偿 `CameraPosition -= (tOld - tNew)` | (3, 12, -70)，未变 | 完全不匹配，自由视图渲染读取的是 `ViewOffset` |
| 本次修复：`-=` 加解析器给出的旧枢轴 | (-34, 18, -62) | 与所需值一致，测试通过 |

前四次实验的共同点在于均未更换旧枢轴的来源，只是在同一个错误之上调整写法，因此无论如何修改都无法成立。

### 相关的两处结构性问题（同一区域，尚未处理）

1. `attachedView` 原为 `tracking`，导致「相机面板下已设父级」无法解析附着。
   **已修**（提交 `57f68ff`），实测使 `--playback-view` 由失败转为通过；原版对照见
   该提交说明（`0x14002DAF0` 判 `cameraParentModel >= 0`，不要求开启跟随）。
2. 枢轴的四路选择将相机位置排在显式参照之前（`ResolveFrameFocus` 保留了这一顺序），
   而相机位置分支的条件是 `tracking || CameraMode() != 0`，故跟随模式下显式参照分支不可达，
   参照被静默忽略。`--view-tools` 的枢轴断言不经过该分支，因此不受本条影响；该点仍未处理。
   本轮早先记录称「已把 `modelReference` 提前但尚未提交」，与工作树不符：当前源码中
   相机位置始终排在前面。

### 当前测试状态

| 项 | 状态 |
| --- | --- |
| `ctest` 22 项 | 全部通过 |
| `scene_lifecycle_tests --playback-view` | 通过 |
| `scene_lifecycle_tests --bone-movement` | 通过 |
| `scene_lifecycle_tests --physics-selection` | 通过 |
| `scene_lifecycle_tests editor` | 通过 |
| `scene_lifecycle_tests --view-tools` | 通过（`CR-002` 修复后，退出码由 exit 7 变为 exit 0；`changing orbit reference preserves the current view` 用例通过） |

### 其余 S2

| 编号 | 位置 | 内容 |
| --- | --- | --- |
| B1-01 | `ui_edit_commit.cpp:311-338` | 灯光 RGB（461/462/463）与方向（464/465/466）编辑框只写 `LightColor()`/`LightDirection()`，不写 `SceneLight()` 也不 `SetLight`。x86 里 `lightColor` 与 `D3DLIGHT9.Ambient` 同块内存，x64 拆成独立镜像（`mmd_app.hpp:1452-1458`），滑块路径补了同步（`ui_hscroll.cpp:148-150/183-184/225-230`），编辑框路径漏了。在编辑框改灯光颜色画面不变，直到下一次灯光轨道求值 |
| B2-02 | `command_control_400.cpp:3073` | IK 单选 444/445 用未校验的 `CB_GETCURSEL` 索引 `ikChains[]`，无选中时为 -1 |
| B2-03 | `command_control_400.cpp:1823` | 命令 420 帧复制无条件写 `rec.selectorStates[0]`，而该子缓冲只在 `boneOrderCount > 0` 时分配 |
| B2-04 | `command_control_450.cpp:736` | 命令 495 的保护区漏掉 `boneCount == 0`，随后解引用可能为空的 `boneSelection` |
| B2-05 | `enhance_model_io.cpp:185` | `SaveEnhancedModel` 用 `_S_IREAD` 作 `_wsopen_s` 的 pmode，新建的 `.pmd` 是只读文件，第二次保存同路径必失败（EACCES 13）。全仓其它写文件调用点都传 `_S_IWRITE` |
| ~~CB-05~~ | `physics_frame.cpp:760-766` | **误报，已撤回**。三处实现并不矛盾：原版在两处 `RotateGrant` 现场（`0x1400C3B5F`、`0x1400C40EF`）均先以 `xorps xmmN, xmm8` 翻转尾骨四元数的 x/y/z，再参与乘法；而 `bone_transform.cpp` 的两处原版现场（`0x1400BA915`、`0x1400BD1F3`）则不作翻转。三处移植均各自忠实于对应的原版现场，详见下文专条 |
| CB-06 | `effect_api.cpp:573` | `ExpGetPmdMaterial` 上界写成闭区间，`mat == count` 时越界读 68 字节；配件侧孪生函数用严格 `<` |
| FB-01 | `dialog_gaps.cpp:531-534` | `kTwistL/kTwistR` 把 `.rdata` `0x52B80C`/`0x52B7F4` 的「左手首/右手首」错认成「左捩/右捩」。7 字节 `memcmp` 因末字节 `0x8D` 永不匹配，追踪记录的手首通道从不写入，而缓冲区是**未清零**的 `::operator new(0x3B3760)`；`oni_skeleton_pump.cpp:450-456` 与 `query_audio_gaps.cpp:528-536` 的 slot 61/65 读到堆垃圾并登记成手首骨关键帧 |
| FB-02 | `oni_skeleton_pump.cpp:186-195` | 同一 308 字节记录的两个读取端分量次序不一致：mode 1 把 `[W,X,Y,Z]` 直接拷进 `rotation[0..3]`（`BoneKey` 的 W 在 `[3]`），mode 2 从组基址 +16 取旋转（应为 +12）。同记录的读取端 `query_audio_gaps.cpp:182-190` 与写入端 `dialog_gaps.cpp:562-565` 一致，可判定 oni 侧错 |
| HB-01 | `accessory_layout.hpp:38` + `ui_refresh.cpp:42` | x64 字段别名冲突。`rowSelected` 在 x64 是 `1212`(0x4BC)、x86 才是 `1196`(0x4AC)；x64 的 `1196` 是 `shadowEnabled`。`ui_refresh.cpp:42` 的 `kObjFlag4AC = 0x4AC` 在 `:53/:118/:124` 无架构保护地读写该字节，`app_gap_bodies.cpp:459/705` 同样用 `acc[1196]`。后果：每次面板刷新把所有配件 `shadowEnabled` 清零、选中项置 1；渲染侧 `accessory.cpp:1064` 直接跳过，存档侧 `pmm_save.cpp:746` 把 0 落盘；`rowSelected` 反而从未被写，`ui_panel_sweep.cpp:505` 的高亮失效。同语义的其它站点已改用 `->rowSelected`，`ui_panel_sweep.cpp:104-105` 已写明 0x4AC 在 x64 错位 16 字节 |
| MB-09 | `pmx_load.cpp:344` | 顶点缓冲规格表按 `pmxAdditionalUvCount & 7` 取值，而表只有 5 项 |
| MB-10 | `pmx_load.cpp:896` | 继承/分层扫描用未校验的 parent 与 tailIdx 索引 `bones[]`（尾部只判了 `>= 0`） |
| MB-11 | `pmx_load.cpp:1310` | facial 显示帧表的 morph 索引未校验就索引 `Morphs(m)`，且 `morphCount` 为 0 时直接解引用空表 |
| MB-12 | `pmx_load.cpp:1292` | `facialFrameCount` 统计**所有**显示帧的 morph 条目，但记录只填前两帧，facial 面板与保存出的 PMD 多出空行 |
| MB-13 | `pmx_load.cpp:1392` | 骨骼显示帧记录用截断到 u16 的索引索引 `Bones(m)`，未校验 |
| MB-14 | `pmd_load.cpp:441` | PMD 物理扫描对每个骨骼都先读 `bones[tailIdx]`，tailIdx 未做任何范围校验（-1 是常见值） |
| MB-15 | `pmd_load.cpp:529` | facial 表用文件里的 `targetIndex` 索引 `Morphs(m)`，`morphCount` 为 0 时是空指针解引用 |
| MB-16 | `pmd_load.cpp:536` | `groupCount` 是 u8 且加 1 后回绕，表按回绕值分配却仍写 `[0]/[1]/[2]` |
| MB-17 | `bone_transform.cpp:830` | IK 求解用未校验的链端点索引索引 `boneRecords[]`，IK 根无父骨骼时读 `boneRecords[-1]` |
| MB-18 | `model_query_gaps.cpp:291` | `SelectDisplayBone` 把未经范围校验的骨骼索引直接当写下标，写坏 `boneSelection` 前后各 1 字节 |
| MB-19 | `key_registrars.cpp:939` | 按名标记关键帧时用显示表的 `targetIndex` 直接做 key 记录根下标，`keys[cur].allocated = 1` 越界写 |
| MB-20 | `key_registrars.cpp:439` | 空闲槽扫描先读 `keys[searchCursor]` 再查边界，池满（`cursor == kBoneKeyCapacity`）后读到池外并可能写到池外 |
| MB-21 | `pmx_load.cpp:1459` | 刚体段用只判了下界的 `bi` 读 `bones[bi].position` 并写 `hasRigidBody`，逆变换也按同一索引取骨骼 |
| MB-22 | `pmx_load.cpp:1542` | 关节段用未校验的 `rigidA`/`rigidB` 索引取刚体记录（含 `rigidB.body` 传给物理），并已解引用 `invTransform` |
| MB-23 | `pmx_load.cpp:1640` | 初始化插值默认值时按 `boneCount` 遍历 `model.boneKeys`，`boneCount` 超过池容量时越界写 |
| B3-22 | `pmm_load_v2.cpp:1887` | 定长名字字段读满后未终止即当 C 字符串用（`%s` 越界读，长名字时 `sprintf_s` 越界） |
| B3-23 | `vsq_load.cpp:797` | `division` 与 tick 未校验，除零得 inf 后转 int 属 UB，垃圾帧号被写成关键帧 |
| B3-24 | `pmm_save.cpp:494` | 16 位循环计数器在 `dispCount > 65535` 时回绕，保存永不结束 |
| RB-04 | `device_reset.cpp:55` | `PostDeviceReset` 释放 `captureSurface` 后全仓没有任何重建路径；录制中设备丢失后 `frame_scene.cpp:458-464`、`capture_downsample.cpp:53`、`model_renderers.cpp:1941-1946` 继续使用它 |
| RB-05 | `capture_downsample.cpp:50-57` | `CreateOffscreenPlainSurface` 与 `LockRect` 返回值都未检查，随后无条件解引用 |
| RB-06 | `accessory.cpp:248` | `AccessoryPlacement` 只用 `max(parentBone, 0)` 夹下界，缺上界（同函数对模型槽却加了上界）。PMM 骨名重映射只夹 `mapped < 0`，换用骨数更少的模型再载入旧 PMM 即得越界索引 |
| AB-02 | `key_ladder.cpp:223` | `G` 键三分支丢失（详见上文 S1 条目，此处按 PORT 归类） |
| AB-03 | `key_ladder.cpp:236` | `L` 块恒定 mod 3，与本文件注释声明的「model 模式 mod 2」相矛盾（置信度低） |

### CB-05 撤回依据（2026-09-23 复核）

原判定仅依据「三处实现互相矛盾」及 `bone_layout.hpp:67` 的注记，未与原版比对。复核时
对基线 `refs/MikuMikuDanceE_v932x64/MikuMikuDance.exe` 进行反汇编，得出相反结论：

| 现场 | 原版行为 | 移植 |
| --- | --- | --- |
| 物理回写 `RotateGrant`，父级 ≥ 0（`0x1400C3B5F`） | 取尾骨四元数（`+0x180/184/188/18C`）后 `xorps xmm0, xmm8`、`xorps xmm1, xmm8` 翻转分量，再 `D3DXQuaternionMultiply` | 一致 |
| 物理回写 `RotateGrant`，父级 < 0（`0x1400C40EF`） | 同上 | 一致 |
| `bone_transform.cpp` 两处 | 传源四元数不翻转（`0x1400BA915`、`0x1400BD1F3`） | 一致 |

`xmm8` 的值是 `-0.0f`（`.rdata` 中 `0x140132B80` 处的 `00 00 00 80`，在 `0x1400C398D` 处载入），
因此上述两条 `xorps` 确为取反操作。也就是说，**原版本身就存在这种不一致**：物理回写这一路取共轭，
是因为刚体世界矩阵已带有一次旋转，回写时需将其去掉。无论删去共轭，还是让 `bone_transform.cpp`
同样取共轭，都会破坏与原版的对应关系，故本条目不改动代码，编号予以保留，以便与批次报告对照。

原判遗漏的原因与 `MB-08` 同型：只考察移植内部是否自洽，未先行核验原版。判定口径由此补充一句：
**「几处实现互相矛盾」只能说明至少有一处可疑，必须逐处对照原版现场，才能确定究竟哪一处有误。**

---

## S3 —— 资源泄漏、性能退化与维护隐患（41 条）

| 编号 | 位置 | 内容 |
| --- | --- | --- |
| B1-02 | `ui_ctlcolor.cpp:152-159`（另 `shutdown_cleanup.cpp:538-544`） | 释放列表 7 条缺 Gravity，而 `ui_editor_click.cpp:275` 会为它分配记录缓冲，每次框选泄漏（键数 × 16 字节）。这是原版 7 个地址的照搬，补齐等于修掉原版自带的泄漏 |
| B1-03 | `ui_ctlcolor.cpp:54-60` | 记录由 `::operator new` 分配（`ui_editor_click.cpp` 7 处），却用 `std::free` 释放；MSVC 下能跑，标准上是 UB |
| B1-04 | `ui_selection_reeval.cpp:140-146` | 白色画笔/画刷未还原即 `DeleteObject`，且遗弃 DC 原对象；曲线拖动期间每次 `WM_MOUSEMOVE` 调一次 → GDI 句柄持续泄漏。同族画法都先还原再删（置信度中） |
| B1-05 | `ui_view_refresh.cpp:215-218` | `d3dx::D3DXMATRIXF m;` 未初始化；d3dx9_32.dll 缺失时 `matrixRotationQuaternion` 被跳过，随后按垃圾值算欧拉角并写进状态与三个读数框 |
| B1-06 | `ui_edit_commit.cpp:346-356` | 448 FOV 提交自建投影矩阵，绕过 `ApplyFrameCameraProjection`/`BuildCameraPerspectiveProjection`，缺 aspect 校验、device 判空、d3dx 缺失回退，也忽略正交帧模式。构建器被复制两份 |
| B2-06 | `physics_model_dialog.cpp:484/491` | 两个 10 万槽暂存数组（合计约 30 MB）只在对话框 OK/Cancel 里 free；打开无模式物理模型对话框后再执行「文件 > 新建」或打开 PMM（`reset_app_state.cpp:106-112` 只 `DestroyWindow`）即永久泄漏，可重复累积 |
| B2-07 | `model_edge_dialog.cpp:133` | `strcpy_s(model->nameEn, 0x32, text)` 的声明长度是字段实长的 2.5 倍（当前靠手工截断掩盖） |
| B2-09 | `command_file_menu.cpp:1261` | 四处按模型表遍历的计数器用 `std::uint16_t`，模型计数是 32 位，超过 65535 死循环 |
| B3-25 | `vmd_save.cpp:152` | 把未初始化的栈字节写进 VMD 文件头与模型名 |
| B3-26 | `pmm_load_v2.cpp:1937` | 配件装配失败的中止路径不释放映射工作区与名字/索引映射数组（对比 `AbortV2Load` 会做） |
| B3-27 | `pmm_io_common.hpp:195` | 丢弃循环按文件给出的计数逐字节读，巨大计数等于长时挂起（21 亿次 `_read`） |
| B3-28 | `pmm_load_v2.cpp:2647` | `previousDisplayOrder` 全程从未赋值，比较恒等价于 `> 0`，第二个比较对任何 ≥1 的值都成立 |
| CB-07 | `wave_audio.cpp:513` | 换 WAV 时 `WaveCtxReset` 直接丢弃上一个 WAV 的文件句柄与 DirectSound 流缓冲（未 `CloseHandle`/`Release`） |
| CB-08 | `media_load.cpp:234-238/247-250` | AVI 流扫描失败分支漏 `AVIStreamRelease(best)`，AVI 文件保持占用 |
| CB-09 | `wave_audio.cpp:297`（另 452） | `CreateFileW` 的失败判断写成 `== nullptr`（实际返回 `INVALID_HANDLE_VALUE`），检查恒不成立，失败被当成功 |
| CB-10 | `physics_frame.cpp:798`（缺陷体在 170-175） | 四元数乘法 fallback 不支持输出与输入别名，而调用点正是 `D3dxQuatMul(q, q, rot)`；无 D3DX 时结果错 |
| CB-11 | `effect_api.cpp:597`（另 609/638/647） | `ExpGetPmdBoneName` 一族的骨骼/表情下标完全无界，越界返回野指针（置信度低） |
| FB-03 | `push_source.cpp:155-159` | `SetBitmapInfo` 覆盖已有 push-cfg 块且不释放，每次调用泄漏 0x2C 字节 |
| FB-04 | `source_base.cpp:792-802` | `JoinFilterGraph` 对事件汇的 QI 结果 AddRef 后立刻 Release、从不写入 `+0x44`，使 EC 通知路径恒为 `E_POINTER`（PORT，置信度低） |
| FB-05 | `source_base.cpp:1741-1746` | `QueryId` 固定分配 8 字节，而所用宽字符转换函数自身上限是 32 wchar |
| FB-06 | `source_base.cpp:1414-1462`（与 `push_pin.cpp:675-712` 重复） | 同一 VA `0x10001310` 有两份实现且取值来源不同，基类那份是死代码并做跨类下转型 |
| RB-07 | `scene_font.cpp:253` | 对 `IDirect3DSurface9::GetDC` 取得的 HDC 调用 `DeleteDC`（`bg_overlay.cpp:320` 是反向约定） |
| RB-08 | `debug_geometry.cpp:186` | `DrawSelectionBox` 的 `NumVertices` 传 72，而顶点缓冲只有 16 个顶点（256 字节）；72 是索引数 |
| RB-09 | `d3d_init.cpp:308` | `CreateDevice` 全阶梯失败时 Release 了 `IDirect3D9`，却把悬垂指针留在寄存器/成员里（当前被 `wndproc` 的 `exit(1)` 挡住二次 Release） |
| RB-10 | `model_renderers.cpp:507`（另 971） | 用 256 字符栈缓冲拼接 256 字符目录 + 100 字符文件名，靠 `swprintf_s` 失败兜底（置信度低） |
| RB-11 | `debug_geometry.cpp:82` | `SetDebugColor` 未检查 `Lock` 结果，随后按 16 字节步长写入可能为空的数据指针 |
| RB-12 | `debug_geometry.cpp:452`（另 234） | 选中骨索引只判下界，`bones[selected]` 无上界 |
| AB-04 | `playback_catchup.cpp:463` | 播放时钟用 `timeGetTime` 32 位对拼 64 位（`winmain.cpp:116/140-148` 高字只装 Sleep 补偿、加法不进位）；跨 49.7 天回绕后 `elapsed ≈ −4.29e9 ms`，游标被写成负数且 `t0` 不重锚 → 播放时间轴永久卡死，需停止重播才恢复 |
| AB-05 | `record_readback.cpp:171` | 帧回读像素缓冲用 `::operator new` 分配，录制收尾（`stubs.cpp:120`）用 `free()`，而 `shutdown_cleanup.cpp:433` 用 `::operator delete`（同字段两种释放） |
| AB-06 | `playback_state.cpp:286` | undo 环的 `bonePose` 一处 `new[]` 分配、一处 `free()` 释放；同类第三处（轨道键数组）`ui_init.cpp:63`/`reset_app_state.cpp:318` 用 `operator new`，`shutdown_cleanup.cpp:470`/`scene_ownership.cpp:71` 用 `free` |
| AB-08 | `scene_ownership.cpp:93` | 配件记录块由 `::operator new(sizeof(mdl::AccessoryRecord))` 分配（`accessory.cpp:849-850`），场景拆除路径却用 `std::free`，而同类型的 `DeleteAccessory` 走 `::operator delete`（`accessory.cpp:816-820`）。打开 PMM、新建场景或退出时释放已加载配件即经过该路径。MSVC 下当前静默，标准上是 UB。与 AB-05、AB-06 及轨道键数组属同一族，建议四处一并规整 |
| AB-09 | `scene_ownership.cpp:24` | 仅 `MIKUDANCESTUDIO_DIAG` 构建（默认 OFF，不在发布二进制）。`char path[MAX_PATH]; sprintf_s(path, "%s\\pmm_model_load.log", directory);`，`directory` 来自环境变量且无长度校验；超过 245 字符时 `sprintf_s` 的数组重载触发 CRT invalid-parameter handler，默认直接终止进程。同批 `frame_state_dump.hpp:41` 已改用 `std::snprintf`，照抄即可 |
| AB-10 | `frame_state_dump.hpp:330` | 仅 DIAG 构建。morph 链根节点上界用 `state.morphCount`（来自文件，可大于 20000）索引固定 20000 项的 `morphKeys` 表，而同链的 `next` 用的是字面量 20000。应改用 `mdl::kMorphKeyCapacity`/`mdl::kDisplayKeyCapacity`。置信度低，需畸形 PMX 才能触发；载入器按 `morphCount` 单独分配其余 morph 数组（`pmx_load.cpp:958-971`），故载入阶段不越界，问题只在此 dump 的界上 |
| MB-24 | `pmd_load.cpp:284` | `4 * idxCount` 是 32 位乘法，索引表分配量可回绕 |
| MB-25 | `model_frame_seek.cpp:226` | 终止键的零填充写成 `bone->trans[0..5]`（`float[3]` 越界下标），语义正确但形式是 UB，姊妹文件已改掉 |
| MB-26 | `path_resolve.cpp:224` | 纹理缓存满（10000 条）后扫描以 `used == 10000` 退出，紧接着写的 `resourcePool[10000]` 在两种 ABI 下正好落在 `localeTable` 上 |
| MB-27 | `path_resolve.cpp:112` | 单字符扩展名比较恒为假，`.x` 文件的 `UserFile\Accessory` 回退永不生效 |
| HB-02 | `path_workspace.hpp:13-14` | `ExtractDirFromPath` 按「调用方持有 1000 wchar 缓冲」拷贝，而 `path_utils.cpp:26` 传入的是 256 wchar 字段（当前来源 ≤255 字符，属潜伏） |
| HB-03 | `ported_funcs.hpp:189-191` | `FillPanelBottom` 声明的参数名 `x/y/w/h` 与定义的 `left/top/right/bottom` 语义相反 |
| HB-04 | `model.hpp:230-234` | 五个 x86 专用偏移常量放在无架构切分的命名空间里，且全仓零引用 |

---

## S4 —— 一致性、命名与文档（36 条）

这一组多是注释与代码不符。它们的价值在于：按注释「修正」代码会把本来正确的
行为改坏。

| 编号 | 位置 | 内容 |
| --- | --- | --- |
| B1-07 | `ui_viewport_refresh.cpp:59-64` | 注释称原版用 `mulss` 单精度，代码按双精度算（同文件另两处是单精度） |
| B1-08 | `ui_hscroll.cpp:94-105` | 注释把 morph 滑块写成 bone 记录数组，代码实际写 `MorphRecord::value` |
| B2-10 | `command_file_menu.cpp:2284` | 237-240 的注释把四张时间线表标成 bone/morph/camera-light/accessory，代码用的是 camera/light/self-shadow/gravity |
| B2-11 | `command_control_400.cpp:93` | 文件头注释把帧编辑对话框模板号写反（0x25E EN / 0x28C JP），代码与同族全部用例相反 |
| B2-12 | `mic_window.cpp:148` | `case 792 /*WM_PALETTECHANGED*/` 常量名错：792 是 `WM_PRINTCLIENT`（`WM_PALETTECHANGED` 是 785） |
| B2-13 | `ui_dropfiles.cpp:150` | 注释称 `DragQueryFileW` 的 `cch=0x200`「replicated verbatim」，代码实际传 `_countof(szFile)`（256） |
| B2-14 | `misc_dialogs.cpp:444` | `ApplyCameraFrameScaleAdd` 全仓无调用者，注释暗示仍在使用 |
| B3-29 | `pmm_load_v1.cpp:387` | 文件头注释写「7 option bytes」，代码只读 6（v2 才是 7）。按注释改回 7 会让整个 v1 流偏移 1 字节 |
| B3-30 | `pmm_load_shell.cpp:121` | 注释称原版用 `strstr` 匹配魔数，代码改成偏移 0 的 `memcmp`（低置信度） |
| CB-12 | `kernel_gaps.cpp:32`（共 7 处） | 台账里的「Ported call site」行号全部过期，建议改成符号名 |
| FB-07 | `source_base.cpp:1537-1548` | 枚举次序注释、头文件成员名与代码三者互相矛盾 |
| FB-08 | `query_audio_gaps.cpp:313-315` | 注释给出的窗口下标算式 `i = 4*srcIdx+8` 与代码 `+ srcIdx` 不一致，代码才是与写入端一致的那个 |
| FB-09 | `crt_ledger.cpp:11-14` | 台账开头的「边界认定」与文件自身 [C] 段矛盾：`0x10006130` 既被说成 `__CxxFrameHandler3` 又被说成 `GetBitmapSize` |
| FB-10 | `crt_ledger.cpp:118/135/141` | 台账把 CAMThread/CSourceStream 根虚表的 `+0x00` 与 `+0x04` 槽标反 |
| FB-11 | `subsystem_gaps.cpp:66-107` | 台账引用的 `effect_api.cpp` 行号整体过期约 300 行 |
| FB-12 | `dialog_gaps.cpp:491-494` | 标准姿态四元数区间的上界写错（332 应为 336） |
| FB-13 | `query_audio_gaps.cpp:454-457` | 追踪缓冲与 undo 姿态缓冲在 features 侧用标量 `::operator new` 分配，在 app 侧用 `delete[]` 释放 |
| FB-14 | `dll_main.cpp:315-333` | `DllGetClassObject` 的两条失败路径没有把 `*ppv` 置空 |
| FB-15 | `stubs.cpp:123-128` | `FinishAviRecord` 末尾的 `ShowWindow` 守卫恒为真（同一字节在 `:92` 刚被清零），且注释写的偏移 `0xA0B41` 与代码读的 `0xA0D61` 不是同一个 |
| RB-13 | `stubs.cpp:255/264` | 注释指向的 `src/render/background_plane.cpp` 不存在，实现实际在 `bg_overlay.cpp` |
| HB-05 | `app_layout.hpp:849` | `englishUI` 的 x64 注释写成 `0xA1B98`，与同行十进制 662416（= 0xA1B90）及断言墙矛盾 |
| HB-06 | `app_layout.hpp:950` | `physicsInterval` 初始值注释写 `0.0125f`，源码全站实际是 `0.01125f` |
| HB-07 | `app_layout.hpp:991` | `statusText` 的 x86 注释写 `+0xA4535`，与断言墙的 672813（= 0xA442D）冲突，且该值超出 x86 结构体总长 |
| HB-08 | `mmd_app.hpp:1435`（字段在 `app_layout.hpp:950`） | `PhysicsInterval()` 与 `ShadowDistance()` 是同一存储的两个无关名字，头文件无别名说明 |
| HB-09 | `model.hpp:37-38` | 「Five-int entries … 20-byte stride」挂在 8 字节的 `BoneReference` 上，描述的是 `BoneOrderEntry` |
| HB-10 | `globals.hpp:56-59`（定义在 `frame_modes.cpp:57`） | `g_MouseScaleA` 注释写「port keeps 0.01f」，实际定义是 `0.5` |
| HB-11 | `globals.hpp:15/39/58`、`ported_funcs.hpp:6` | 引用的 `docs/PORTING_STATUS.md` 与 `docs/ARCHITECTURE.md` 在本仓库不存在 |
| HB-12 | `bone_layout.hpp:1-4`、`model.hpp:3-4` | `bone_layout.hpp` 仍自称 `GENERATED` 并指向不存在的 `scripts/gen_bone_layout.py`；`model.hpp` 又称 `model_layout.hpp` 为 generated |
| HB-13 | `app_layout.hpp:68-70` | `dialogFlags` 的字母表只列 17 个字母，漏掉 `D`，与 18 槽的表及 `keyboard_input.hpp` 都不符 |
| HB-14 | `ported_funcs.hpp:94/95/191` | 三个已移植函数仍被标成 `(stub)` |
| HB-15 | `mmd_app.hpp:1152-1159` | 描述 `state.messageSeen` 的注释块挂在了 `SidebarWidth()` 上 |
| MB-28 | `keyframe_common.hpp:38` | `RdPtr` 只读 4 字节却写进 8 字节指针，高 4 字节未初始化（当前无调用者） |
| MB-29 | `model_dispose.cpp:217` | 用 x86 字面量 8732 释放「未知 morph 表」，两种 ABI 下该偏移都落在填充区，释放是空操作 |
| MB-30 | `pmx_load.cpp:1437` | 刚体/关节的英文名未走 `ReadTextBuf` 的 jpText 回退，空英文名会被造成 `Null_XX` |
| AB-07 | `globals.cpp:27` | `g_CatchupDtBudget` 初值与注释相反：`0.0f` 会让 settle 主步按 dt=0 步进，注释却说保持整帧真实 dt（当前时序不可达） |
| B2-01 附注 | — | 见 S1 首条 |

---

## 审查覆盖

199 个文件全部过审，无未读完文件。逐文件的审读深度见各批次报告（按批次保存在
本机临时目录，不随仓库发布）。

已明确记录「无 S3 以上问题」的文件：

- `io/`：`io_gaps.cpp`
- `model/`：`morph_apply.cpp`、`model_keyframe_edit.cpp`、`vmd_load.cpp`、
  `bone_sort.cpp`、`post_load_init.cpp`、`model_init.cpp`、`add_model.cpp`、
  `bone_edit_undo.cpp`、`model_labels.inc`、`track_apply.cpp`
- `render/`：D3D12 预览两文件（生命周期、三槽 fence 复用、上传缓冲复用、
  SRV 堆容量、回读顺序、常量缓冲步长与 HLSL cbuffer 一致）按声明只审工程风险，
  未发现越界或竞态
- `window/`：`command_control_500.cpp`、`dialog_select_ops.cpp`、
  `command_impl.cpp`、`command_dispatch.cpp`、`dialog_procs.cpp`
- `mmdxshow/`：`guids.cpp`、`MMDxShow.def`、`enumerators.cpp`、`mmdxshow.hpp`
- `math/`、`main/`：`text_encoding.cpp`、`charset_conv.cpp`、`quat_to_mat3x4.cpp`、
  `color_lerp.cpp`、`winmain.cpp`、`runtime_log.cpp`、`effect_names.cpp`
- `include/`：断言墙算术逐字段重算，x86 与 x64 两侧全部自洽
  （`MMDAppState` x86 673072 = 0xA4530 / x64 677344 = 0xA55E0；`ModelRecord`
  x86 314612 = 0x4CCF4；`AccessoryRecord` 0x4B0/0x4C0；`BoneRecord` 604/624；
  `MorphRecord` 136/192；`RigidRecord` 172/192；`JointRecord` 140/152；
  `UndoRecord` 28/40；`D3DRenderer` 0x1D574/0x3AA88；`PhysicsScene` 0x48/0x90）

独立核对通过、确认无缺陷的项：

- `exports/MikuMikuDance.def` 的 37 个名字与 `effect_api.cpp` 的 37 个
  `__declspec(dllexport)` 定义一一对应，无缺、无重复。
- 各 `ExpGet*` 写入调用方缓冲的字节数（0x40 / 0x44 / 256）符合 MME 契约。
- `scene_gizmo_data.inc` 十张表的长度与 `scene_create.cpp` 传入值全部吻合。
- `d3dx_effect.hpp` / `d3dx_mesh.hpp` 的 vtable 槽号按 wine-mirror 的
  `d3dx9effect.h` / `d3dx9mesh.h` 逐槽数过，全部正确。
- `d3dx_dyn.hpp` 两个分支的 25 个入口签名一致；静态导入分支的 `static_assert`
  自洽。
- `panel_controls.hpp` 的 400..567 段与 `src/window/ui_controls.inc` 的 168 条
  创建表逐条一致。
- `physics_create.cpp` 的 Bullet 对象在 `model_dispose.cpp:116-125` 与
  `shutdown_cleanup.cpp:154-186` 成对释放，无泄漏或二次释放。
