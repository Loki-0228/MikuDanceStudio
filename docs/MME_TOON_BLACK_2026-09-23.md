# HGrass.pmx 在物体面板渲染为黑色（2026-09-23）

## 结论

模型面板（物体模式）下 `HGrass.pmx` 渲染成黑色剪影，相机面板下同一模型、同一机位则正常。
触发条件需**同时满足两点**：宿主目录装有 MME（`MMEffect.dll` + `MMHack.dll` + MME 的 `d3d9.dll` 代理），
且模型目录存在 `HGrass.emd`，将 MME 的 `SoftParticleModel.fx` 分配给该模型。

直接原因是**该 effect 的 toon 纹理采样在移植下返回纯黑，而原版返回白色**。
`Basic_PS` 中的
`Color *= tex2D(ObjToonSampler, float2(0, 0.5 - LightNormal * 0.5));`
会把整片草地乘成 0。将该行替换为 `Color *= 1.0;`（其余字节不变）后，模型立即恢复正常。

## 复现与判据

- 命令行直接传入模型路径即可加载：`build\Release\MikuMikuDance.exe "<pmx 绝对路径>"`（`src/window/window_init.cpp` 按扩展名分派）。
- PMX 加载器会弹出英文注释确认框，标题为 `Model Infomation`，模型英文注释为空时正文为 `NoInfo`
  （`src/model/pmx_load.cpp`）。无人值守时必须发送 `WM_COMMAND IDOK`，否则主窗口不会显示。
- 面板切换通过控件 536（`panel::kModelEditToggle`，中文标题「到相机」）跨进程 `PostMessage(BM_CLICK)` 完成。
  `PollKey` 走 `GetAsyncKeyState`，在无人值守会话中 `SendInput` 返回 0，不生效。
- 判据取视口像素：物体面板有 27.7% 像素为纯黑 `(0,0,0)`，草地只剩 `(35,65,35)`；
  相机面板同机位的纯黑比例降至 7.2%，草地为 `(116,208,101)`。

## 消元实验

| 配置 | 物体面板结果 |
| --- | --- |
| 移植 + MME + `HGrass.emd` | **黑**（27.7% 纯黑） |
| 移植，移除 `MMEffect.dll`/`MMHack.dll` | 正常 |
| 移植 + MME，删除模型的 `.emd` | 正常 |
| 移植 + MME，effect 换成只输出纯红的最小 effect | 正常（整片 `(255,0,0)`） |
| 移植 + MME，effect 里禁用 toon 采样 | 正常 |
| 原版 `refs/MikuMikuDanceE_v932x64` + 同一套 MME | 正常 |

最小纯色 effect 渲染正确，说明 MME 的绘制链在移植下是通的（顶点、矩阵、绘制调用均无误），
缺陷只出现在依赖宿主语义参数的 effect 上。

## 渲染路径

`src/render/model_renderers.cpp` 中的 `EffectRenderEnabled` 决定走哪条路径：

```
cameraGate = PlaybackActive() != 0 || UsesViewportTool()
UsesViewportTool() = editMode >= ViewportEditMode::None(2)
```

- 物体面板：`editMode` 为 `Bone(0)`/`BoneBox(1)`，cameraGate 不成立，走 `RenderModelsFixed`
  （D3D 固定功能；装有 MME 时由 MMHack 接管对象绘制）。
- 相机面板：`editMode = Camera(3)`，cameraGate 成立且自阴影开启时走 `RenderModelsEffect`
  （exe 内置 skin effect，资源 118/117），MME 不接管该对象，因此照常显示。

两种面板渲染结果不同，正是这条分支造成的。

## 已排除

- **光照结构**：原版进程内实测 `D3DLIGHT9` 为 `Diffuse=(0,0,0,0)`、`Specular=Ambient=(0.602,0.602,0.602,0)`、
  `Direction=(-0.5,-1,0.5)`，与移植 `src/window/ui_init.cpp` 的初始化一致。
- **MME 读到的材质与光照语义**：用自建探针 effect 把语义值画成颜色，移植与原版逐位相同——
  `LightDiffuse.r=0`、`LightAmbient.r=0.6`、`MaterialDiffuse.r=0.498/1.0`、
  `MaterialAmbient.r=0.498`、`MaterialEmmisive.r=0.251`、`LightSpecular.r=0.6`。
- **材质记录布局**：按纹理路径定位记录，两个宿主的
  `diffuse/diffuseMirror/ambient/specularPower/texturePath/spherePath/faceVertexCount/toonReference`
  完全一致（`toonReference` 均为 `0xFF`，即 PMX 的 toon 贴图索引 -1）。
- **`Data\toonNN.bmp` 缺失**：移植目录原本没有这十个标准 toon 位图，补齐后现象不变，故不是直接原因。
- **effect 内部中间量**：`AmbientColor` 在移植下为 `(0.549,0.925,0.420)`，与原版相同。

## 插件侧定位（2026-09-24 续）

`MMHack.dll` 导出 `IsToonUsed` 与 `GetToonTexture` 供 `MMEffect.dll` 查询，两者均为十几条指令的薄封装：

- `IsToonUsed`（RVA 0x1110）：读取 `drawType = [0x6E798]`，仅当其取值为 1 或 2 时才继续判断，返回 `[0x6E78B] == 0`。
  也就是说，只要满足"处于绘制中且材质的 toon 标志为 0"，就视为使用 toon。
- `GetToonTexture`（RVA 0x10B0）：执行 `cmp dword ptr [0x6E870], 1`，若不相等则直接返回 `nullptr`；
  若相等，则以 `0x6E860` 为 this 跳转至 RVA 0xF810 获取缓存纹理。

运行时对两个宿主中的上述全局量进行采样（采样间隔 20 毫秒，各采样 200 次）：`[0x6E870]`（toon gate）与 `[0x6E798]`（draw type）在两侧均同步变化，取值为 1 的采样数分别为移植 102/200、原版 1/200。

这两个量同源，上述比例反映的是采样落在绘制期的概率（绘制越慢，占比越高），不能直接视为行为差异。原版绘制期较短，因此几乎采不到。

三个运行时补丁实验（在目标进程内持续写入相应地址并同时截图）：

| 补丁 | 结果 |
| --- | --- |
| `MMHack+0x6E78B` 写 1（使 `IsToonUsed` 返回假） | **恢复**：纯黑 27.7% → 8.8%，草地出现 `(113,205,100)` |
| 将 `MMHack+0x6E870` 写为 0（绘制钩子随后会把材质 toon 标志 `[0x6E78B]` 写成 1） | **恢复**：纯黑降至 7.2%，草地 `(113,205,100)` |
| `MMHack+0x6E798` 写 0（绘制类型） | 不变，仍为 27.7% |

结论：黑色源于 `GetToonTexture` 在绘制期返回的纹理——移植侧无法取得可用纹理，而原版侧返回宿主槽 0 的默认纹理。

「将 gate 写为 0 能够恢复画面，说明它让该函数回到了 `nullptr` 分支」这一解读是错误的，详见下文「修正此前对 gate 补丁的读法」一节。`0x6E870` 同时驱动材质 toon 标志；把 gate 强写为 0，会使绘制钩子将 `[0x6E78B]` 写成 1，从而使 `IsToonUsed` 返回假。

此外，将 PMX 材质段的 toon 字段由「贴图索引 -1」改写为「内置 toon01」（等长修改：`toonSharing` 0→1、toon 索引 -1→0）后，现象没有变化，说明 MMHack 并不从磁盘上的 PMX 文件读取 toon 信息。

## 与透明材质排序的关系（2026-09-24 续二）

`SortTransparentModelMaterials` 会在绘制前按相机深度重排透明材质的三角形索引（通过 `indices->Lock(..., 0)` 获取可写锁并原地回写）；装有 MME 时，排序时机还会从 Opaque 趟推迟到 Translucent 趟。

在整体跳过该调用后重新构建（临时改动，未提交，验证后已用 `git checkout` 还原），结果如下：

| 构建 | 物体面板纯黑占比 | `[0x6E798]` 取 1 的采样数 |
| --- | --- | --- |
| 排序开启（现状） | 27.7%，草地为纯黑剪影 | 102/200 |
| 跳过排序 | 15.1%，草地变成黑白纹理，石头仍为黑 | 6/200 |
| 跳过排序加探针 effect 直接输出 toon 采样 | 211897 个纯黑像素，与排序开启时逐位相同 | — |

排序确实同时改变了画面表现与 MMHack 的绘制期占比，但最后一行表明，「toon 纹理本身是黑色的」这一结论与排序无关：排序影响的是 MME 是否选用带 toon 采样的 technique（`IsToonUsed` 以 `draw-type ∈ {1,2}` 为前提），而非令纹理变黑。排序本身是保证透明混合正确所必需的，无法通过禁用它来修复该问题。

## 插件侧定位（2026-09-24 续三）：toon 纹理取自哪张表

### GetToonTexture 的两层数据源

`GetToonTexture`（RVA 0x10B0）在 `[0x6E870] == 1` 时以 `0x6E860` 为 `this`，尾调用到 RVA 0xF810。
该函数依赖两层数据源，两者都不是宿主结构体中的字段：

1. `0x6E860` 是一个「40 字节头部 + `std::vector`」的对象，向量位于 `+0x28`（即 `0x6E888`），元素大小 120 字节。
   函数以 `[0x6CCC0]`（宿主导出 `ExpGetCurrentMaterial()` 的返回值）作为下标，要求下标小于元素数，
   且 `元素+0x10` 非零。`元素+0x00` 是 MSVC `std::wstring`，`+0x10` 是其长度字段，
   因此「非零」即「名字非空」。
2. 名字非空时，用 RVA 0x10D90（`std::hash<std::wstring>`）计算哈希，再用 RVA 0x10C40（`std::unordered_map::find`）
   查询 **MMHack 自己的纹理注册表**。查不到则依次回退到 `L"toon10.bmp"`（字符串位于 RVA 0x5CD58）与空串 `L""`，
   三次均查不到才返回 `nullptr`。

注册表类型为 `std::unordered_map<std::wstring, TextureEntry>`，成员都是固定地址的全局量：
`MMHack+0x6CDD0` 是桶数量（两侧均为 16），`MMHack+0x6CDD8` 是元素个数，`MMHack+0x6CDF0` 是桶数组指针；
RVA 0x10F60..0x1105A 是其插入路径。每个 64 字节的值依次为哨兵 `-2`、键字符串的副本、一个常量、
缓存的 `IDirect3DTexture9*`，以及一个 `IDirect3DTexture9**`——缓存为空时，函数通过这个二级指针惰性重读纹理。

### 谁填充它、取宿主哪个字段

填充者是 MMHack 自己的绘制钩子与导入钩子。它们读取的不是宿主的数据结构字段，而是**宿主发给 D3DX 的调用参数**。
运行期实测（`probe_iat.py`）表明：MMHack 启动时会改写宿主 exe 的导入地址表，两侧宿主的六个 thunk 都指向 MMHack.dll。

| 导入 | 在 MMHack.dll 中的地址 |
| --- | --- |
| `Direct3DCreate9` | +0x14160 |
| `CreateFileW` | +0x141E0 |
| `D3DXCreateTextureFromFileExA` | +0x14540 |
| `D3DXCreateTextureFromFileExW` | +0x14670 |
| `D3DXCreateTextureFromFileInMemoryEx` | +0x147A0 |
| `D3DXCreateTexture` | +0x14850 |

因此注册表的键就是宿主调用这些 D3DX 函数时传入的文件名，值中记录的槽地址则是调用方传出的
`IDirect3DTexture9**`。带文件名的调用按文件名注册；不带文件名的 `D3DXCreateTextureFromFileInMemoryEx`
注册为空串键。

`0x6E860` 的向量由另一处填充：RVA 0x27F0..0x2D05 的绘制钩子每次绘制都会调用宿主导入的
`ExpGetCurrentObject()`，用返回的对象号查询 MMHack 自己的 `std::map<int, ObjectRecord>`（位于钩子对象的 `+0x60`）。
查不到就新建记录，随后把该记录的 40 字节头部拷贝到 `0x6E860..0x6E888`，把记录中的向量整体赋值给 `0x6E888`。
接着，它把 `ExpGetCurrentMaterial()` 存入 `0x6CCC0`，把 `ExpGetCurrentTechnic()` 存入 `0x6E798`
（后者即 `IsToonUsed` 的前置条件「绘制类型」）。也就是说，条目的 120 字节内容来自 MMHack 的对象缓存，
条目中的材质名并非每次绘制时现读宿主字段。

### 运行期对照：移植侧少了两类键

用 `probe_toonmap.py` 遍历两侧注册表的桶链：

| 宿主 | 元素个数 | 键 |
| --- | --- | --- |
| 原版 | 16 | 5 个模型纹理的绝对路径 + `toon01.bmp`..`toon10.bmp` + 空串 |
| 移植 | 5 | 只有 5 个模型纹理的绝对路径 |

原版中这些 toon 键与空串键的槽地址连续排列、每 8 字节一个（空串对应 `0x2E8FD70`，`toon01.bmp` 对应 `0x2E8FD78`，
依次到 `toon10.bmp` 对应 `0x2E8FDC0`），恰好就是宿主 `toonTextures[11]` 的槽位：空串键对应槽 0（内嵌 PNG 默认纹理），
`toonNN.bmp` 对应槽 NN。

### 根因

`tests/Grass/HGrass.pmx` 的五条材质在两侧都把条目首个字符串留空（同一模型在两侧逐字节相同），
因此 `GetToonTexture` 走的都是「查空串键」这条路径：

- 原版查得到：空串键存在，其值为宿主槽 0 的默认纹理，可采样出正常颜色。
- 移植查不到：注册表中没有空串键，函数返回 `nullptr`。MME 将这个空纹理绑定到 toon 采样器上，
  D3D9 对空纹理采样返回不透明黑 `(0,0,0,1)`，`SoftParticleModel.fx` 中的
  `Color *= tex2D(ObjToonSampler, float2(0, 0.5 - LightNormal * 0.5));` 便把整片草地乘成 0。

移植侧缺少这些键的原因在于 `src/render/toon_textures.cpp`：该文件自行用 `LoadLibraryA("d3dx9_43.dll")`
配合 `GetProcAddress` 解析 `D3DXCreateTextureFromFileExA` 与 `D3DXCreateTextureFromFileInMemoryEx`
（第 61 行到第 74 行的 `D3dxApi::Load`），绕开了被 MMHack 改写的导入表。同一工程的
`include/mikudancestudio/d3dx_dyn.hpp` 第 211 行到第 226 行已经写明这一约束（以静态导入方式载入这些入口点，
以便 MME 挂钩，且每次调用都要重读槽位）。模型纹理走的是那份加载器，因此被注册；toon 位图走私有加载器，因此未被注册。

### 修正此前对 gate 补丁的读法

本文档此前把「把 `MMHack+0x6E870` 写 0 能恢复画面」解释为「让 `GetToonTexture` 回到 `nullptr` 分支」。
按现在的证据，这一解释并不成立：移植侧本来就走 `nullptr` 分支。真正的原因是 `0x6E870` 同时驱动材质 toon 标志——
绘制钩子中 RVA 0x2B7E..0x2B95 的代码会根据 `[0x6E870] == 1` 与否写 `[0x6E78B]`，等价于
`[0x6E78B] = ([0x6E870] != 1)`。把 gate 强写为 0，会让钩子把 `[0x6E78B]` 写成 1，`IsToonUsed` 随即返回假，
这与把 `0x6E78B` 写 1 是同一件事。三条补丁实验中只有这两条能恢复画面，正好互相印证。

### 附带发现：ExpGetCurrentTechnic 的字段位置两侧不一致

宿主导出 `ExpGetCurrentTechnic` 是 MMHack 取「绘制类型」的来源（`IsToonUsed` 以该值属于 {1,2} 为前提）。
两侧实现读取的地址不同，运行期实测（`probe_technic.py`）的一致率如下：

| 宿主 | 导出体读的地址 | 与 `MMHack+0x6E798` 的一致率（物体面板 6000 次采样） |
| --- | --- | --- |
| 原版 | `app+0xA11DC` | 5989 / 6000 |
| 移植 | `app+0xA11E0` | 5970 / 6000 |

原版把 `app+0xA11D8` 当作一对 4 字节字段：低 4 字节是当前对象号（`ExpGetCurrentMaterial` 只比较低 32 位），
高 4 字节就是 `ExpGetCurrentTechnic` 读取的「当前技术」。移植则把 `app+0xA11D8` 做成 8 字节指针
`activeRenderObject`（`include/mikudancestudio/layout_pins.hpp` 第 549 行），于是 `activeRenderPass`
被排到 `0xA11E0`（第 550 行），比原版高出 4 字节。该偏差影响的是「MME 何时认为处在 toon 路径上」，
而不是黑纹理的来源，本轮不作改动，仅作记录。

## 修复与验证（2026-09-24 续四）

修复位于 `src/render/toon_textures.cpp`：删除了该文件私有的 `D3dxApi`（原第 56 行到第 77 行）及其自带的 `LoadLibraryA` 与 `GetProcAddress` 解析逻辑，改用 `include/mikudancestudio/d3dx_dyn.hpp` 的 `d3dx::Get()`，并在每次调用 D3DX 前先调用一次 `Load()`。该加载器按头文件第 211 行到第 226 行的约定，在每次调用时重读 exe 的导入槽。因此，槽 0 的 `D3DXCreateTextureFromFileInMemoryEx` 与槽 1..10 的 `D3DXCreateTextureFromFileExA` 均经宿主导入表解析。

| 项目 | 修复前 | 修复后 |
| --- | --- | --- |
| 物体面板纯黑像素（`px.py`，视口区域） | 266310（27.7%） | 68660（7.2%） |
| 物体面板草地色带主色 | `(0,0,0)` 183071 像素 | `(113,205,100)`、`(114,206,100)`、`(115,207,100)` 等亮绿 |
| 相机面板纯黑像素 | 68770（7.2%） | 68770（7.2%），未受影响 |
| MMHack 纹理注册表元素数 | 5 | 16（空串键与 `toon01.bmp`..`toon10.bmp` 齐备，与原版一致） |

命令与产物：`python D:\MMD-Next\.tmp-hgrass\probe_panel.py "D:\MMD-Next\MikuDanceStudio-MME\build\Release\MikuMikuDance.exe" "D:\MMD-Next\tests\Grass\HGrass.pmx" tcfix`
生成 `tcfix-1-model.png`（物体面板）、`tcfix-2-camera.png`（相机面板）、`tcfix-3-model-again.png`（切回物体面板）；
`python D:\MMD-Next\.tmp-hgrass\px.py tcfix-1-model.png` 打印像素统计；
`python D:\MMD-Next\.tmp-hgrass\probe_toonmap.py <exe> <pmx>` 打印注册表桶链。修复前的对照数据取自 `cap-1-model.png` 与 `cap-2-camera.png`。

回归：`ctest --test-dir build -C Release` 22/22 通过；`build\tests\Release\scene_lifecycle_tests.exe "tests/极星动力LIVE舞台 Ray用一键渲染\UserFile\Stage.pmm"`
的六条入口（`--startup-view` 1 轮、`--playback-view` 3 轮、`--view-tools` 1 轮、`--bone-movement` 1 轮、
`--physics-selection` 1 轮、`editor` 10 轮）均为 exit 0；另在无 MME 的干净目录（只放 exe 与 `Data`）复测同一模型，
物体面板纯黑仍为 7.2%、草地仍为 `(113,205,100)`，说明本次改动不影响无 MME 时的渲染。

该修复不依赖 `Data\toonNN.bmp`：空串键来自槽 0 的内嵌 PNG 资源（`D3DXCreateTextureFromFileInMemoryEx`），
而 `CMakeLists.txt` 第 586 行到第 587 行已说明 `Data\toon*.bmp` 取自参考版 MMD 安装、不属于本源码树。

`ExpGetCurrentTechnic` 的 4 字节偏差是否一并纠正，取决于移植侧 `activeRenderObject` 是否必须保持 8 字节指针。
若要纠正，改动将落在 `include/mikudancestudio/app_layout.hpp` 与 `layout_pins.hpp` 的布局上，影响面大于本条修复，另立一项。

## 诊断脚本

本节所有脚本与截图位于 `D:\MMD-Next\.tmp-hgrass\`，不属于任何仓库：
`probe_panel.py`（加载模型、绕过注释框、切面板、截图）、
`probe_light.py`（扫描宿主进程内的 `D3DLIGHT9`）、
`mkprobe.py` / `mkprobe2.py`（按字节从 `SoftParticleModel.fx` 派生探针 effect）、
`probe_scan.py` / `probe_fields.py`（按纹理路径定位材质记录并 dump 字段）、
`px.py`（截图色彩统计）、
`mm.py`（按 RVA 反汇编、列出导出与导入、按位移扫描指令）、
`disasm_any.py`（按 VA 反汇编任意 PE）、
`pe_export.py` / `exp_diff.py`（导出表列表与两侧对照）、
`xref_range.py` / `scan_xref.py` / `scan_disp.py` / `scan_str_xref.py`（引用与位移扫描）、
`dump_rdata_range.py` / `dump_strings.py`（字符串与名字表）、
`probe_tooncache.py`（dump `0x6E860` 头部与 `0x6E888` 向量条目）、
`probe_toonmap.py`（走 MMHack 纹理注册表的桶链）、
`probe_iat.py`（检查宿主导入表是否被 MMHack 改写）、
`probe_technic.py`（对照宿主字段与 `MMHack+0x6E798`）。