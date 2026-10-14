# 可选网格插件安装与存档参考

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [MMV4MMS 可选插件](#record-mmv4mms)
- [MMV 安装与分配窗口](#record-mmv-install)
- [MMV 工程与对象存档](#record-mmv-storage)

<a id="record-mmv4mms"></a>
## MMV4MMS 可选插件

原始记录：`MMV4MMS.md`。其中的版本、日期和验证范围沿用原记录。

2026-10-08 起，MMS 支持从主程序目录加载独立构建的 `MMV4MMS.dll`。插件通过通用接口注册菜单栏右侧的「MMV」，提供效果映射、启用开关、自动重载、自动保存映射文件、全部重载、日志窗口和 EMM 导入。插件缺失或加载失败时没有菜单占位，MMS 继续自身渲染。

宿主实现位于 `src/mmv4mms/`，接口快照位于 `sdk/mmv4mms/`。MMS 的 CMake 只构建宿主适配，不构建插件，不依赖相邻 MMV4MMN 或 MMV4MMS 目录。

预览和录制共用 `RenderFrameScene` 的接管分支。插件返回完成的 GPU 纹理后，宿主跳过原有场景绘制，继续输出及编辑控件合成。渲染异常不提交上一帧输出。项目切换、对象删除及设备丢失会通知插件释放对应状态。

宿主 Release 构建与后台菜单检查已通过。AllStar 原 PMM 的空网格辅助骨骼模型已通过加载和连续渲染回归，见[崩溃修复记录](../formats/models.md#record-allstar-empty-model)。真实 PMM 画面对照、完整 FX 回归、视频导出对照及性能对照尚未验收，不能据此声称画质或速度改进。

<a id="record-mmv4mms-菜单与文件格式"></a>
### 菜单与文件格式

菜单文字、命令和 `Ctrl+Shift+E` 快捷键由 DLL 实现，随 MMS 界面切换中文、英语和日语，不显示 About。宿主不定义 MMV 菜单项。

「加载 EMM 文件」只导入 `.emm`。效果编辑器保存工程为 `.vmm`，对象预设从 `.vem0` 进入，并按层生成 `.vem1` 等文件。自动保存映射文件在 PMM 保存成功后写同名 `.vmm`。

`menu.h` 是可选版本 1 状态接口快照。宿主只读取 `MMV_MENU_RENDERER_DISABLED` 判断是否使用自身渲染，不改变已有渲染 ABI。旧插件如果不主动注册菜单，宿主不会替它补建菜单。

MMHack、MME 和其他插件可以使用相同的 [MMS 菜单注册接口](../../development/plugin-menus.md)。没有注册者就不创建对应入口；旧 MME 通过 Win32 插入的菜单仍沿用原有路径。

<a id="record-mmv-install"></a>
## MMV 安装与分配窗口

原始记录：`MMV_INSTALL.md`。其中的版本、日期和验证范围沿用原记录。

2026-09-28 起，MMV 本体在独立项目 `D:/MMD-Next/MMV` 维护。
先在该项目运行 `./scripts/build.ps1`；本宿主的 `build/` 只编译 MMS 和宿主接入测试。
当前源码与构建入口见 [MMV 项目说明](../../../../MMV/README.md)，
拆分记录见 [项目迁移](host.md#record-mmv-project-split)。

MMV 是可选插件。宿主仍通过 DX9/MME 绘制；MMV 在绘制前生成独立网格。
当前菜单和分配窗口已经实现，完整兼容范围及未完成验收见
[实现记录](host.md#record-mmv-implementation)。

<a id="record-mmv-install-安装"></a>
### 安装

启用对应构建选项后，Win64 Release 生成以下版本，互不覆盖：

| 版本 | 构建输出 | 效果执行后端 |
| --- | --- | --- |
| 兼容版 | `../MMV/build/mmv/compat/Release/MMV.dll` | DX12，以及独立设备上的传统 DX9 FX |
| 纯 DX12 版 | `../MMV/build/mmv/dx12/Release/MMV.dll` | DX12 |
| CPU＋DX9 兼容版 | `../MMV/build/mmv/dx9/Release/MMV.dll` | CPU 原生求解器，以及传统 DX9 FX；无 DX12 运行时 |

表中路径相对于本宿主项目。CPU＋DX9 版的构建、效果要求和错误提示见
[CPU＋DX9 版本说明](../../../../MMV/docs/MMV_CPU_DX9.md)。

1. 关闭准备更换插件的宿主。
2. 从一个版本的目录复制 `MMV.dll` 和 `MMV/` 文件夹到新版
   `MikuMikuDance.exe` 所在目录。
3. 启动宿主。右上角的 `MMV` 菜单位于 MME 菜单左侧。

同一宿主目录只能放一个版本的 `MMV.dll`。纯版仍需要宿主提供 DX9/MME
输出适配；它的 DLL 内部不包含传统 DX9 FX 后端。未安装插件时不显示 MMV 菜单。
独立 DX12 后端不可用时保留分配，并让受影响对象使用 MMS 原始输出。

<a id="record-mmv-install-分配效果"></a>
### 分配效果

打开 `MMV` 菜单中的效果分配窗口。菜单、页签、按钮、列标题与状态说明
跟随宿主中文、日文、英文设置；已打开的窗口会立即切换语言。
FX 自己提供的名称和原始编译诊断保留原文。

选择对象后设置效果。带三角标记的对象可以双击展开材质行，材质使用稳定
标识保存分配。主输出代表最终网格；效果声明的网格或纹理目标自动成为
扩展页签，每页可继续分配其他 FX。窗口显示当前插件版本与每项分配状态。

窗口支持移除、启停、恢复默认、重新加载、打开存档、保存工程、另存为和
按对象保存。附件主输出行可设为控制器；控制器保留只读变换数据，隐藏辅助几何。
参数按钮在当前效果提供受支持参数面板时启用。

两个固定页签中的模型整行含义不同：「材质分配」的模型整行是模型主输出，「骨骼分配」的模型整行是先于主输出执行的整模型网格处理。展开后的子行才分别限定材质范围或骨骼权重范围。具体区别见模型整行与子行（`docs/MMV_PAGES_AND_ACCESSORIES.md`，历史引用文件不在当前仓库中）。

当前版本启用 Bullet3 时，在「材质分配」页选中模型整行，分配 `Bullet3_mmv.fx`。现有接入不接受骨骼页或单个材质。原生物理会自动切为「不计算」，完整步骤和分配错误处理见 [Bullet3 使用说明](physics.md#record-mmv-bullet3-按模型或骨骼启用-bullet3)。

`.x` 没有独立的 MMV 格式。MMV 只读取 `.vem`、`.vmm` 或界面中的显式分配，
不搜索附件同名 `.fx`。MME 继续使用原有自动关联规则。MMV 效果建议命名为
`名称_mmv.fx`。

<a id="record-mmv-install-存档与作者资料"></a>
### 存档与作者资料

`.vmm` 保存工程分配、参数和关键帧曲线，`.vem` 保存对象的静态效果预设；自动保存开关只影响 `.vmm`。
`.vem` 只在手动打开或按对象保存时读取或写入，不自动关联模型和附件。配置可以在各版本之间交换，但效果必须满足该版本的执行要求；纯 DX12 版保留
传统 DX9 FX 分配并标记不支持，依赖它的对象回退到 MMS 输出。

- [存档字段与覆盖顺序](usage.md#record-mmv-storage)
- [FX 目标、依赖与网格接口](effects.md#record-mmv-fx)
- [透视效果参数](effects.md#record-mmv-perspective)
- [关键帧与曲线编辑](../../guides/animation-curves.md)

新增界面功能继续沿用中、日、英三语，以及 MMV 位于 MME 左侧的菜单约定。
<a id="record-mmv-install-2026-09-27-界面与分配更新"></a>
## 界面与分配更新

正式运行目录 `build/Release/` 已安装本轮宿主与兼容版 MMV。独立快照位于
`.local/releases/mmv-native/ui-scope-e2e44eb/`，两版 DLL 分别在 `compat/` 和 `dx12/`。
它们都叫 `MMV.dll`，选择一版放在宿主 EXE 旁，配套 `MMV/` 资源目录一起使用。
本轮包含固定骨骼／材质页、名称语言回退、参数背景及 K 帧面板整合；存档写版本 2，仍可读版本 1。
哈希、验收与保留范围见 [本轮交付记录](interface.md#record-mmv-ui-scope-acceptance)。

<a id="record-mmv-storage"></a>
## MMV 工程与对象存档

原始记录：`MMV_STORAGE.md`。其中的版本、日期和验证范围沿用原记录。

`.vmm` 保存工程的 MMV 分层、分配、参数和关键帧曲线。`.vem0`、`.vem1` 等文件组成单个对象的静态效果预设，每个文件的数字尾缀表示所属层。
二者是带版本标记的 UTF-8 INI。读取允许 UTF-8 BOM，写入不加 BOM。
MMV 不读写 `.emm`、`.emd`，也不自动搜索附件同名 `.fx`。

`.x` 的声明只指定默认效果及控制附件状态，不规定层号。`.uifx` 描述效果控件，不能替代对象预设。

```ini
[Info]
Type=MMV
Version=3
Kind=Project
AutoLoad=true
AutoSave=true

[Binding.0]
ObjectKind=1
ObjectPath=accessory/cloth.x
Instance=0
Owner=
Target=Main
Material=
Bone=
Effect=effects/cloth_mmv.fx
Enabled=true
Controller=false
Layer=0
```

对象类别 0 为模型，1 为附件。对象由类别、规范路径和同路径实例序号识别；
显示名不参与匹配。`Owner` 为空表示固定分配页；非空值是声明目标的效果实例标识。
内置分配页为 `Target=Main`，可选择骨骼或材质展开。其所在的 MMV 自带层固定为 0。
`Material` 为材质稳定标识；新骨骼分配的 `Bone` 为 `@稳定骨骼键`，在所选层的上游表中解析。
原有 `索引:原始名称` 形式仍可读取，必须同时匹配索引与名称。生成骨骼不得用名称猜测身份。
两字段互斥，均为空时表示整个对象。
源材质标识依次为 `source:0`、`source:1` 等。

格式版本为 3，拒绝版本 1、2 和旧 `.vem` 文件，不进行迁移或猜测兼容。
MMV 层编号为 0～65535，从大到小执行。最终 MME 绘制固定为 -1，不进入 MMV 分配或层数。
VMM 的 `Layer` 字段记录分配所在层；单层 `.vemN` 由后缀决定层号，不另存 `Layer` 字段。
层号不属于分配身份，移动层不会改变参数和关键帧所属实例。

空 `Effect` 是显式移除，`Enabled=false` 是显式禁用，都参与覆盖。
`Controller=true` 可将附件用作 MMV 控制对象：保留变换等查询数据并隐藏辅助几何。
同一个 `.x` 可以先由 MMV 修改，再交给 MME 着色，不需要另造附件格式。

对象加载时不读取同名 VEM。手动打开 `名称.vem0` 时，按数字顺序读取相同主体的所有 `.vemN` 文件，作为一个事务应用。
只能从 MMV 的 0 层发起「保存当前物体」，选择 `.vem0` 后自动导出各层文件，包含持久化的空层。
数字尾缀不得包含前导零；`名称.vem01` 不是第 1 层。可编辑层之间允许空号。
保存对象预设时，将当前帧的参数求值结果保存为静态基础值，不保存关键帧和曲线。
含动画字段的 VEM 会被拒绝。缺失对象、所属附件、效果目标、骨骼或材质时，整套加载失败并报告不兼容，不改写其他范围。
工程默认加载同名 `.vmm`，PMM 保存成功后才自动写同名 `.vmm`。
自动加载与自动保存可以分别关闭，也可手动打开或另存。

<a id="record-mmv-storage-持久身份与相对路径"></a>
### 持久身份与相对路径

`ObjectIdentity.<id>` 段记录 `Path`、`Kind`、`Instance`。
`PageInstance.<id>` 段记录 `Object`、`Effect`、`Parent`、`Target`、`Material`、`Bone`，
将页签身份与当前文件的绝对位置分开。`MaterialSlots.<object-id>` 按零填充编号
保存新材质标识和保留编号。效果实例、对象身份段的标识是内部不透明字符串。
手工编写主输出分配不需要生成这些段，运行时会补齐。

路径能相对存档位置表达时优先写相对路径，读取时解析为绝对路径。
移动整个工程目录后，页签分配和材质编号保持原身份。
VEM 的 `[Object] Identity` 记录所属对象。手动打开时按存档的对象身份匹配，
不会自动给每个同名实例复制预设。

<a id="record-mmv-storage-每层的展开设置"></a>
### 每层的展开设置

VMM 与 VEM 都保存以下节。`N` 为层号，身份字符串不依赖界面语言。

| 节 | 字段 |
| --- | --- |
| `Layer.N` | `Present=true`，保留没有分配的空层 |
| `LayerView.N.Page.<hash>` | `Owner`、`Target`、`Expansion=Bone` 或 `Material` |
| `LayerView.N.Object.<hash>` | `Object`、`Owner`、`Target`、`AllowExpand`、`Expanded` |

默认展开方式为骨骼，默认展开许可关闭。`AllowExpand` 控制是否允许点击箭头；`Expanded` 保存实际开合状态。关闭许可时保留开合状态，再次允许后可以恢复。

<a id="record-mmv-storage-参数曲线"></a>
### 参数曲线

`Parameters.<效果实例>` 按变量稳定 ID 保存参数。未编辑曲线的旧关键帧保留原有
Step、Linear、Smooth 语义。曲线编辑后，`Var.<id>.Curve.Version=1` 和
`Var.<id>.Curve` 保存各分量的曲线。曲线数据包含帧、值、两侧斜率、手柄权重、插值及切线类型。
`.uifx` 只描述控件和多语言名称，不保存用户动画。原生宿主的相机、灯光、自阴影、重力和附件曲线
写入 PMM 扩展段，MMV 参数动画写入 `.vmm`。操作见 [曲线编辑器](../../guides/animation-curves.md)。

<a id="record-mmv-storage-编码保留与写盘"></a>
### 编码、保留与写盘

节名与字段名区分大小写。值不自动去除空白，`;` 和 `#` 是普通字符。
换行、回车、反斜杠、等号及方括号分别写作 `\n`、`\r`、`\\`、`\=`、`\[`、`\]`。
未知节和未知字段在往返时保留。缺失对象、缺失效果及纯 DX12 版不支持的传统 FX
仍保留分配，修复路径或换用兼容版后可恢复。

单文件上限 8 MiB，单行上限 64 KiB，分配上限 4096。
不支持的未来版本或语法错误使整个读取失败，当前配置不被部分替换。
写入先创建同目录唯一临时文件并刷盘，再替换目标文件。
替换失败时保留旧存档并报告错误。


<a id="record-mmv-storage-烘焙的原生启停轨道"></a>
### 烘焙的原生启停轨道

每个效果实例的 `Parameters.<实例身份>` 段可以保存：

```ini
NativeEnableVersion=1
NativeEnable.Base=1
NativeEnable.Key.10=Step;0
NativeEnable.Key.21=Step;1
```

示例使 10–20 帧停止运行该实例，21 帧恢复。只能使用 0/1 常值键，不插值。原生字段与作者 `Var.*` 分开保存，不占 HLSL 常量寄存器。VMM 保留轨道；手动 VEM 只保存当前静态值，不含关键帧。没有版本标记时按未创建轨道处理。

参数面板注入三语「启用」变量，作者原有 256 个变量的上限不变。若作者恰好声明了 `MMVEnabled`，该变量仍属于作者，烘焙会明确拒绝为该实例添加隐藏开关，不修改 FX 或 UIFX。
