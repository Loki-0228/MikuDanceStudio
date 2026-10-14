# 材质、光照与描边记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [PMD 共享 toon 映射修复记录](#record-pmd-toon-selection-fix)
- [HGrass.pmx 在物体面板渲染为黑色（2026-09-23）](#record-mme-toon-black)
- [云内部暗块的来源](#record-cloud-dark-shading)
- [云模型 Toon 调整](#record-cloud-toon-adjustment)
- [模型模式光照与场景保持一致](#record-model-view-lighting)
- [轮廓宽度与原版 MMD 的镜头距离计算](#record-outline-width)

<a id="record-pmd-toon-selection-fix"></a>
## PMD 共享 toon 映射修复记录

原始记录：`PMD_TOON_SELECTION_FIX_20261012.md`。其中的版本、日期和验证范围沿用原记录。

PMD 的 toon 文件表不必按 `toon01.bmp` 到 `toon10.bmp` 排列。旧渲染代码把文件表索引当作共享贴图索引，因此第一项引用 `toon03.bmp` 时会查询不存在的模型目录贴图。固定管线第 0 级纹理为空后，后续主贴图无法参与绘制，模型呈灰白色。关闭骨骼操作「选择」或切换到相机模式后，内置效果仍能绘制主贴图，因此出现模式相关的显示差异。

`SharedToonSlot` 按文件名解析共享槽位。固定管线、内置效果的 toon 颜色和 MMS 提交给 MMV 的 toon 数据使用同一规则。匹配仍区分大小写。PMX 自定义 toon、无 toon、普通自定义贴图路径保持原来的处理方式。完整包和最小包共用此修复；文件格式不变。

原版依据为 `refs/MikuMikudance 932 fix/MikuMikuDance.exe`，SHA-256 为 `3ebe6ead8076478c06539b14a0e32e70f30b0edc9c8f045819d04ad8d62bee49`。x64 地址 `0x1400B79BC` 起按文件名选择共享贴图，`0x1400B8B61` 起按同一规则选择 toon 颜色。

<a id="record-pmd-toon-selection-fix-验收结果"></a>
### 验收结果

本地记录位于 `.local/artifacts/pmd-selection-20261012`。`matrix-results.json` 包含各次运行结果，各运行子目录保留 `console.log` 和进程记录。

| 检查 | 结果 |
| --- | --- |
| 修复前，文件表槽 0 引用 `toon03.bmp` | 「选择」开启时双色贴图变为灰白，关闭或切换到相机模式后恢复 |
| 完整包和最小包各 11 个原创 PMD 样例 | 全部通过，每个样例检查 8 个真实界面切换状态、每个状态 4 个内部像素 |
| MME 与 MMV 各 4 个共享 toon 样例 | 全部通过，日志确认实际插件已加载 |
| 14 组语义等价文件表比较 | 每组 8 个状态的采样颜色完全相同，包括背光面 |
| 完整包已有材质、遮挡与关闭透明排序回归 | 3/3 通过 |
| 最小包已有关闭透明排序回归 | 2/2 通过 |

样例覆盖有序表、槽 0 引用 toon03、槽 9 引用 toon01、槽 0 引用 toon10、缺失自定义 toon 后回退、有效自定义 toon、无 toon，以及背光面颜色。`tests/pmd_selection_fixtures.py` 生成原创模型和贴图，不依赖第三方资产。

`pmd_selection_render_tests` 通过实际模型加载器和界面命令切换「选择」及模型、相机面板，从 Direct3D 目标读回像素。默认要求测试私有桌面；当前设备在私有桌面无法创建 D3D9，因此验收显式使用 `--hidden-default` 和隐藏、不激活窗口守卫。测试不操作其他进程。

<a id="record-pmd-toon-selection-fix-原报告仍待现场确认"></a>
### 原报告仍待现场确认

POLLYPOLARIS、初音 PMD、两个盒子及贴图城市场景在修复前的本地测试中没有出现相同的全白问题。POLLYPOLARIS 也完成了原生、MME、MMV 下的模型与相机模式切换。用户尚未在当前 Release 中手测该模型。

此次修复确认消除了 toon 文件表重排和回退造成的灰白问题，尚不能证明最初报告的所有 PMD 场景都是同一原因。后续验收应使用实际出现异常的 PMD 与原操作顺序。

<a id="record-mme-toon-black"></a>
## HGrass.pmx 在物体面板渲染为黑色（2026-09-23）

原始记录：`MME_TOON_BLACK_2026-09-23.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-mme-toon-black-结论"></a>
### 结论

模型面板（物体模式）下 `HGrass.pmx` 渲染成黑色剪影，相机面板下同一模型、同一机位则正常。
触发条件需**同时满足两点**：宿主目录装有 MME（`MMEffect.dll` + `MMHack.dll` + MME 的 `d3d9.dll` 代理），
且模型目录存在 `HGrass.emd`，将 MME 的 `SoftParticleModel.fx` 分配给该模型。

直接原因是**该 effect 的 toon 纹理采样在移植下返回纯黑，而原版返回白色**。
`Basic_PS` 中的
`Color *= tex2D(ObjToonSampler, float2(0, 0.5 - LightNormal * 0.5));`
会把整片草地乘成 0。将该行替换为 `Color *= 1.0;`（其余字节不变）后，模型立即恢复正常。

<a id="record-mme-toon-black-复现与判据"></a>
### 复现与判据

- 命令行直接传入模型路径即可加载：`build\Release\MikuMikuDance.exe "<pmx 绝对路径>"`（`src/window/window_init.cpp` 按扩展名分派）。
- PMX 加载器会弹出英文注释确认框，标题为 `Model Infomation`，模型英文注释为空时正文为 `NoInfo`
  （`src/model/pmx_load.cpp`）。无人值守时必须发送 `WM_COMMAND IDOK`，否则主窗口不会显示。
- 面板切换通过控件 536（`panel::kModelEditToggle`，中文标题「到相机」）跨进程 `PostMessage(BM_CLICK)` 完成。
  `PollKey` 走 `GetAsyncKeyState`，在无人值守会话中 `SendInput` 返回 0，不生效。
- 判据取视口像素：物体面板有 27.7% 像素为纯黑 `(0,0,0)`，草地只剩 `(35,65,35)`；
  相机面板同机位的纯黑比例降至 7.2%，草地为 `(116,208,101)`。

<a id="record-mme-toon-black-消元实验"></a>
### 消元实验

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

<a id="record-mme-toon-black-渲染路径"></a>
### 渲染路径

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

<a id="record-mme-toon-black-已排除"></a>
### 已排除

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

<a id="record-mme-toon-black-插件侧定位2026-09-24-续"></a>
### 插件侧定位（2026-09-24 续）

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

<a id="record-mme-toon-black-与透明材质排序的关系2026-09-24-续二"></a>
### 与透明材质排序的关系（2026-09-24 续二）

`SortTransparentModelMaterials` 会在绘制前按相机深度重排透明材质的三角形索引（通过 `indices->Lock(..., 0)` 获取可写锁并原地回写）；装有 MME 时，排序时机还会从 Opaque 趟推迟到 Translucent 趟。

在整体跳过该调用后重新构建（临时改动，未提交，验证后已用 `git checkout` 还原），结果如下：

| 构建 | 物体面板纯黑占比 | `[0x6E798]` 取 1 的采样数 |
| --- | --- | --- |
| 排序开启（现状） | 27.7%，草地为纯黑剪影 | 102/200 |
| 跳过排序 | 15.1%，草地变成黑白纹理，石头仍为黑 | 6/200 |
| 跳过排序加探针 effect 直接输出 toon 采样 | 211897 个纯黑像素，与排序开启时逐位相同 | — |

排序确实同时改变了画面表现与 MMHack 的绘制期占比，但最后一行表明，「toon 纹理本身是黑色的」这一结论与排序无关：排序影响的是 MME 是否选用带 toon 采样的 technique（`IsToonUsed` 以 `draw-type ∈ {1,2}` 为前提），而非令纹理变黑。排序本身是保证透明混合正确所必需的，无法通过禁用它来修复该问题。

<a id="record-mme-toon-black-插件侧定位2026-09-24-续三toon-纹理取自哪张表"></a>
### 插件侧定位（2026-09-24 续三）：toon 纹理取自哪张表

<a id="record-mme-toon-black-gettoontexture-的两层数据源"></a>
#### GetToonTexture 的两层数据源

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

<a id="record-mme-toon-black-谁填充它取宿主哪个字段"></a>
#### 谁填充它、取宿主哪个字段

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

<a id="record-mme-toon-black-运行期对照移植侧少了两类键"></a>
#### 运行期对照：移植侧少了两类键

用 `probe_toonmap.py` 遍历两侧注册表的桶链：

| 宿主 | 元素个数 | 键 |
| --- | --- | --- |
| 原版 | 16 | 5 个模型纹理的绝对路径 + `toon01.bmp`..`toon10.bmp` + 空串 |
| 移植 | 5 | 只有 5 个模型纹理的绝对路径 |

原版中这些 toon 键与空串键的槽地址连续排列、每 8 字节一个（空串对应 `0x2E8FD70`，`toon01.bmp` 对应 `0x2E8FD78`，
依次到 `toon10.bmp` 对应 `0x2E8FDC0`），恰好就是宿主 `toonTextures[11]` 的槽位：空串键对应槽 0（内嵌 PNG 默认纹理），
`toonNN.bmp` 对应槽 NN。

<a id="record-mme-toon-black-根因"></a>
#### 根因

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

<a id="record-mme-toon-black-修正此前对-gate-补丁的读法"></a>
#### 修正此前对 gate 补丁的读法

本文档此前把「把 `MMHack+0x6E870` 写 0 能恢复画面」解释为「让 `GetToonTexture` 回到 `nullptr` 分支」。
按现在的证据，这一解释并不成立：移植侧本来就走 `nullptr` 分支。真正的原因是 `0x6E870` 同时驱动材质 toon 标志——
绘制钩子中 RVA 0x2B7E..0x2B95 的代码会根据 `[0x6E870] == 1` 与否写 `[0x6E78B]`，等价于
`[0x6E78B] = ([0x6E870] != 1)`。把 gate 强写为 0，会让钩子把 `[0x6E78B]` 写成 1，`IsToonUsed` 随即返回假，
这与把 `0x6E78B` 写 1 是同一件事。三条补丁实验中只有这两条能恢复画面，正好互相印证。

<a id="record-mme-toon-black-附带发现expgetcurrenttechnic-的字段位置两侧不一致"></a>
#### 附带发现：ExpGetCurrentTechnic 的字段位置两侧不一致

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

<a id="record-mme-toon-black-修复与验证2026-09-24-续四"></a>
### 修复与验证（2026-09-24 续四）

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

<a id="record-mme-toon-black-诊断脚本"></a>
### 诊断脚本

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

<a id="record-cloud-dark-shading"></a>
## 云内部暗块的来源

原始记录：`CLOUD_DARK_SHADING_2026-09-26.md`。其中的版本、日期和验证范围沿用原记录。

本次定位只针对逐像素透明排序实验中云内部的灰暗三角形块，不覆盖云边界漏出背景或漏层。结论：在当前 `cloud.pmm` 固定视角中，主要来自云粒子三角面的原始法线参与普通方向光 Toon 着色；关闭投射或接收自阴影不改变画面。改浅 Toon 只减少变暗幅度，不能解决受光方向造成的分片。

此结论是当前场景的单变量实验，不表示任意 MME 效果或其他视角都没有阴影问题，也不证明整个透明排序特性已验收。

<a id="record-cloud-dark-shading-素材与-shader-证据"></a>
### 素材与 shader 证据

- cloud.tga 的全部 RGB 均为 `(255,255,255)`。134,629 个 alpha 非零像素中，93,418 个是部分透明，41,211 个完全不透明。暗色不是烘在该贴图中的。
- 模型有 95,400 个顶点、31,800 个三角形。法线长度约为 1，方向覆盖正负 XYZ；三角形顶点多使用近似一致的面法线。
- 15 个材质的 diffuse 为 `(1,1,1,1)`、ambient 为 `(.5,.5,.5)`、specular 为零。原 Toon 阴影色为 `RGB(182,196,212)`。
- `res/assets/skin_effect_sm3.fx` 的 `BShadowTexture_PS`（约 245–260 行）把方向光项 `saturate(dot(normalize(N), -LightDir) * 3)` 与阴影可见度取较小值，再以 `comp + ToonColor * (1-comp)` 乘材质颜色。背光面即使没有别的物体投影，也会被压到 Toon 的阴影色。
- 原型继续调用该原始像素着色函数，随后按 alpha 混合。混合式为 `源颜色 × alpha + 背景颜色 × (1-alpha)`，没有每层额外将整幅结果重复乘 Toon 的步骤。暗片被纳入正确顺序后，仍保留其受光颜色。

<a id="record-cloud-dark-shading-同条件后台对照"></a>
### 同条件后台对照

使用同一候选 EXE、相机、灯光、原 Toon、贴图 alpha 和三角形位置/顺序。各项仅改独立模型副本；原模型未改。所有候选运行均记录 `input=21412 layers=668 complete=1`，因此统一法线没有通过减少透明层或跳过几何来变亮。

| 变量 | 结果 |
| --- | --- |
| 原模型 | 云内部存在灰暗面片 |
| 仅关闭投射自阴影（材质 bit 2） | 云区域与原模型逐像素相同 |
| 仅关闭接收自阴影（材质 bit 3） | 云区域与原模型逐像素相同；复测一致 |
| 仅统一法线朝上 `(0,1,0)` | 内部暗块消失，云趋于均匀白色 |
| 统一法线并关闭接收自阴影 | 与仅统一法线的云区域相同 |

比较区域为渲染目标 `(323,25)` 到 `(1399,540)`，避开底部 HUD。原模型与投射/接收开关两组的最大通道差均为 0。统一法线改变 131,320 个像素。以统一法线图中 RGB 全通道至少 250 的云内部像素为掩模，原模型平均 RGB 约 `(230.2,234.9,240.3)`；统一法线后约 `(254.7,254.7,254.8)`。其中 R 小于 220 的像素从 38,280 个降至 0。此掩模用于描述亮度差，不能作为独立排序正确性指标。

修改前 build 也分别渲染原法线与统一法线：统一法线会减少内部压暗，但旧排序的蓝色重叠边界仍存在。因此需要区分普通受光造成的暗片和透明顺序/深度造成的背景透出，二者不能靠同一种调色处理。

<a id="record-cloud-dark-shading-修复方向与边界"></a>
### 修复方向与边界

统一朝上的法线只是定位用例，会把体积明暗也抹平，未作为正式修复，也未写回原 PMX。云粒子更适合使用云团的平滑/球形法线或专用粒子受光方式；需要按材质显式选择，并保留普通模型现有光照。不能仅因为检测到半透明，就自动关闭所有半透明材质的光照或自阴影。

本次仍保持 Cloud.fx 禁用，不引入其透明裁切来掩盖排序。Toon 浅色版本保留为对照，不作为发黑问题的修复结论。

<a id="record-cloud-dark-shading-记录与版本"></a>
### 记录与版本

结果目录：`.local/artifacts/cloud-darkness-20260926/`。`diagnose.py` 保存副本生成与后台运行步骤，`manifest.json` 保存参数、构建来源和委派状态，`pixel-analysis.json` 保存像素统计。`reference.png`、`no-cast.png`、`no-receive-repeat.png`、`normals-up.png` 为关键图像；`before-reference.png` 和 `before-normals-up.png` 为旧 build 对照。

基线生产源码为 `baseline/cloud-precision` 的 `3d13fbc`，候选渲染代码固定于 `099caf8`。此后只有 Toon 工具及文档提交，EXE 未变化。两侧后台驱动源码哈希相同，详见 manifest。无生产渲染代码因本次诊断改变，主分支 Release 未替换。

DSH shader 复核任务 `02ae4cd7-82b0-4cc2-8124-3f2bf85967e0` 在候选工作树启动，立即以 `402 Insufficient account balance` 失败，未执行审查。正式错误结果已保存；上面的代码检查和实际对照由主 agent 完成，没有把失败委派记作通过。

<a id="record-cloud-dark-shading-后续覆盖率缺陷"></a>
### 后续覆盖率缺陷

用户指出修改 Toon 只影响卡通阴影后，进一步构造低 alpha 多层回归。逐层写回 8 位颜色目标时，舍入误差确实会保留过多背景，并破坏目标 alpha。因此上面的法线结论不能解释全部发暗和透色。云内部的受光颜色、透明层排序、合成精度是三个独立问题。参见 [浮点合成修复](transparency.md#record-cloud-float-compositing)。

<a id="record-cloud-toon-adjustment"></a>
## 云模型 Toon 调整

原始记录：`CLOUD_TOON_ADJUSTMENT_2026-09-26.md`。其中的版本、日期和验证范围沿用原记录。

用户要求修改云模型 Toon。本次提供独立浅阴影版本，保留原模型、原 `CToon.png` 和原 `cloud.pmm`。它用于分离材质明暗与透明排序的影响，不代表云排序整体已经验收。

<a id="record-cloud-toon-adjustment-修改内容"></a>
### 修改内容

`ToonCloud.pmx` 的 15 个材质都使用外部 Toon，索引为 1。原 `CToon.png` 为 64×64，上 32 行白色，下 32 行为蓝灰色 `RGB(182,196,212)`。

新 `CToon-soft.bmp` 是 64×64、24 位无压缩数值光照查找表：上 28 行白色，中间 8 行线性过渡，下 28 行改为 `RGB(235,240,246)`。阴影更浅，保留少量冷色层次。内置自阴影会提取 Toon 的阴影色；过渡带本身不等于改善透明遮挡顺序。

模型副本仅改动 PMX 的 textures 段。顶点、索引、材质参数、骨骼、表情、表示枠及物理数据保持原字节。恢复原贴图表后，整个 PMX 可逐字节还原为输入文件。

<a id="record-cloud-toon-adjustment-文件与复现"></a>
### 文件与复现

最终对照目录为 `.local/artifacts/cloud-toon-20260926/validated/`：

- `cloud-soft.pmm` / `cloud-soft.emm`：浅色 Toon 工程。
- `cloud-original.pmm` / `cloud-original.emm`：原 Toon 对照工程。
- `model/ToonCloud-soft.pmx` / `model/CToon-soft.bmp`：新模型和 Toon。
- `model/textures/tex-0.tga`：原 cloud.tga 的逐字节副本，保留透明度。
- `asset-validation.json`：源文件哈希、PMX 字节校验和贴图哈希。
- `comparison-manifest.json`：两侧 Git 状态、构建选项、EXE 哈希、命令和计时。
- `run-comparison.py`：隐藏窗口交替对照驱动；PNG 为渲染目标回读。

PMM 仅替换偏移 68 处、长度 256 字节的模型路径字段，其余字节保持。相机、动作及光照相同。两个 EMM 均明确 `Pmd1 = none`，Cloud.fx 未启用；ikParticle 配件效果保留。PMM 的模型路径是本机绝对路径，移动整套工程时需重新指定模型。

重建模型工具：`tools/make_cloud_toon_variant.py input.pmx output_dir`。它拒绝覆盖已有输出，依赖贴图按原字节复制并用相对路径引用。

<a id="record-cloud-toon-adjustment-加载问题与无效样本"></a>
### 加载问题与无效样本

最初的工具把纹理表改为绝对路径。实际加载器 `src/model/pmx_load.cpp` 直接拼接模型目录与纹理条目，因此副本未加载 cloud.tga，表现为不透明白色轮廓。该组耗时和图像无效，不能作为性能收益。根目录下的首组 `01` 至 `08` 结果全部保留；manifest 已记录排除理由。

最终副本将贴图随模型存放并改用相对路径。原始 cloud.tga 与复制件 SHA-256 相等，未转换图像或改变 alpha。

<a id="record-cloud-toon-adjustment-委派与验收"></a>
### 委派与验收

DSH 子任务 `dd340559-d576-4e2b-859b-97c33333f263` 在 `D:/MMD-Next/MikuDanceStudio-MME/.local/worktrees/cloud-alpha-fix` 修改单个 Toon 生成工具。主 agent 负责真实素材校验、PMM/EMM 集成、前后 build 对照和文档。原输入 PMM 哈希为 `80018d90968d69c98554cc2672cbce7a6b2bf73ab8264af3f1b283e8f9025811`。

本次使用旧排序 build 与逐像素分层实验 build，各自渲染原 Toon、新 Toon，执行两轮反向交替采样。每次预热 3 帧、采样 5 帧。结果口径为 GPU 完成后的批次平均毫秒，不能换称桌面播放 FPS。未启用额外像素层调试读回。

排序原型仍有成本及异常回退限制，见 [透明排序精度实验](transparency.md#record-transparent-sort-precision)。本次不替换正式 Release。

<a id="record-cloud-toon-adjustment-有效对照结果"></a>
### 有效对照结果

基线为 `3d13fbc`，候选为 `1d5e757`（渲染代码固定于 `099caf8`）。两侧使用同一后台驱动源码，其 SHA-256 均为 `6e0b7ee299ae6368495cf32fd4ff44fd34642536eb081cff635da09dd74b73f4`。启动时工作树均干净；后续仅修改 Toon 工具和文档。

| build / Toon | 第一轮 ms/帧 | 第二轮 ms/帧 | 中位 ms/帧 |
| --- | ---: | ---: | ---: |
| 旧排序 / 原 Toon | 4.902 | 2.742 | 3.822 |
| 旧排序 / 浅 Toon | 4.809 | 5.923 | 5.366 |
| 逐像素实验 / 原 Toon | 654.924 | 653.167 | 654.045 |
| 逐像素实验 / 浅 Toon | 661.474 | 626.693 | 644.083 |

仅两轮短采样，基线波动明显；不据此声称 Toon 带来性能提升。逐像素排序原型仍有数百毫秒成本，性能不合格。

画面检查：浅 Toon 保留 cloud.tga 的透明边界，并减弱云内部蓝灰暗块。旧排序 build 即使换浅 Toon，仍可见分片重叠轮廓；这说明 Toon 调色不能替代透明排序。原 Toon 与浅 Toon 在候选 build 中使用同一套排序。

基线与候选部分回读图均未显示 HUD，操作叠层的一致性不在本组验收结论内。只把本组用作 Toon 变化的对照，不把它写成完整渲染回归通过。

候选原 Toon 见 `02-candidate-original.png`，浅 Toon 见 `03-candidate-soft.png`；旧排序浅 Toon 见 `04-before-soft.png`。全部图片、日志和失败样本保留。

<a id="record-cloud-toon-adjustment-后续发黑定位"></a>
### 后续发黑定位

用户确认调 Toon 未解决问题后，继续通过原 Toon 下的法线和阴影控制定位。暗片主要来自粒子面法线的方向光响应，浅 Toon 仅降低对比。此工具及素材保留为诊断对照，详见 [云内部暗块来源](materials.md#record-cloud-dark-shading)。

直接修改 Toon 贴图颜色或 Toon 着色参数主要改变暗部 RGB；当前内置 SM3 shader 在着色之后重新设置输出 alpha，Toon 色不会替代贴图/材质的不透明度。低 alpha 密集叠层另有逐层量化缺陷，详见 [浮点合成修复](transparency.md#record-cloud-float-compositing)。

<a id="record-model-view-lighting"></a>
## 模型模式光照与场景保持一致

原始记录：`MODEL_VIEW_LIGHTING_20261007.md`。其中的版本、日期和验证范围沿用原记录。

进入模型模式或关闭镜头追踪后，继续使用镜头编辑中的光照方向、颜色和亮度。时间轴切帧、播放与停止时同步更新光照和自阴影；自由视角仍由用户控制。

修复移除了两处默认光照回退：`RebuildModelModePanel` 的颜色和方向重置，以及 `SetupFrameWorldTransform` 的固定斜光。光照、自阴影轨道的刷新与相机追踪开关分离。切换模式时保留尚未注册为关键帧的当前光照值。

这是按用户要求对原版模型编辑默认光照行为的调整，不改变 PMM 的光照数据格式。

验证使用同一台机器、同一隐藏 D3D9 设备和同一组非默认方向、颜色及光影关键帧：

- `camera_follow_tests.exe --lighting-only`：修复前 4 项失败；修复后 12 项全部通过。覆盖反复开关追踪、设备光照方向与颜色、自由模型视角下的光照插值和自阴影关键帧，以及相机值保持不变。
- `scene_lifecycle_tests.exe <triangle.pmx> 1 --model-light`：实际载入模型后，模式切换、追踪复选框及追踪菜单均通过；主窗口全程隐藏。
- 正式 Release 主程序编译通过。未进行播放 FPS、CPU 或 GPU 性能对照，不作性能收益声明。

原始记录保存在 `.local/artifacts/model-view-light/20261007/`，含修复前提交、工作树状态和补丁、前后构建及测试日志、EXE 哈希。本次基线带有尚在验收的原生媒体/MOV 工作树改动，两侧保持这些改动一致；光照提交单独记录。

<a id="record-outline-width"></a>
## 轮廓宽度与原版 MMD 的镜头距离计算

原始记录：`OUTLINE_WIDTH_2026-09-20.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-outline-width-原因与修复"></a>
### 原因与修复

`PmdEdgeDistance` 是 PMD、PMX CPU 蒙皮和 D3D12 输入共用的轮廓膨胀计算。
模型参考骨骼的位置已经经过 `frameWorld` 的镜头目标平移及旋转，旧代码却减去
`CameraPosition()`（变换前的镜头目标），混用了两个坐标空间。近景可因此明显
变粗，其他镜头下也可能变细甚至得到零宽度。

改为减去 `SetupFrameWorldTransform` 用于构建视图矩阵的实际视点：
`{ViewOffsetX(), ViewOffsetY(), CameraDistance()}`。原有计算系数保持：

```
distance * 0.000045 * (cameraFov * 0.6 + 1) * model.edgeScale
```

PMX 顶点倍率、材质宽度及禁用描边的收缩规则保持。无需重设项目中的轮廓粗细；
修复不改变骨骼位置或模型主体顶点。

<a id="record-outline-width-原版证据"></a>
### 原版证据

只读分析 `ref/MikuMikudance 926 CHS/MikuMikudance.exe`，SHA256：
`72C34B7AB607AEF7B18974742AE18CE62F115F6A614BBEDC1A4310E4A5406F72`。
下列地址使用映像首选基址 `0x140000000`：

- `0x14002A213..0x14002A247`：调用者从 app 的 `+0x340`、`+0x344`、
  `+0xA18F8` 组装实际视点，通过第三参数传给 `0x1400C1390`。
- `0x1400C13B0`：保存第三参数至 R15。
- `0x1400C2940..0x1400C29EE`：参考骨骼经 frameWorld 变换后，减去 R15 的
  三个视点分量，再计算距离。
- `0x1400C29F3..0x1400C2A26`：应用 FOV、`0.000045` 和模型宽度；常量
  `0.6f`、`0.000045f` 分别位于 `0x140132C38`、`0x140132C34`。

<a id="record-outline-width-验证"></a>
### 验证

- 新增 `model_edge_regressions`：旧代码失败（距离为 2 的样例输出 0，
  预期膨胀量 0.00171）；修复后通过。覆盖 PMD/PMX 的实际 CPU 顶点缓冲、
  D3D12 全局及材质输入、场景/目标共同平移、单骨骼/骨骼 1 参考点、旋转、
  视点平移、近远距离、宽度倍率及禁用描边。
- `d3d12_skinning_tests`：PolarisLiveStage PMX（129307 顶点）及 PMD
  测试模型（60000 顶点），各三个姿态均通过；CPU/GPU 最大绝对误差为 0。
- `StageNoRay LowEnd.pmm`：修复前后均成功加载 MME、3 模型、168596 顶点、
  10 配件；自动跳过 1 个缺失模型提示，完成渲染并保存画面。
- Release 构建通过，诊断及性能采集编译选项均为 OFF。

场景渲染使用 PMM 保存的镜头，未复现用户截图中手动调整的近景，不能据此宣称
与原版截图逐像素一致。近景行为由独立回归用例验证，现场画面对照仍需用户复核。
