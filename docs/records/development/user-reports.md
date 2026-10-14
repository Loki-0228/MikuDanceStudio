# 用户问题与回归记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [用户测试报告修复与回归记录（2026-09-19）](#record-user-test-fixes)

<a id="record-user-test-fixes"></a>
## 用户测试报告修复与回归记录（2026-09-19）

原始记录：`USER_TEST_FIXES_2026-09.md`。其中的版本、日期和验证范围沿用原记录。

本轮承接 `416b650`。该版本已有第 3–9 项的部分修复和单项回归，但真实大型 PMM 重载仍会崩溃。下表区分已有修复、本轮新增修复及实际验证，不把旧诊断提交当作崩溃已修复的证据。

本轮分组提交：`2cf199a` 修正 D3DX ABI；`641f2dd` 修正 Unicode 名称和骨骼选择；`2b0f449` 修正快捷键消费层；本文件所在提交修复骨骼表情工作数组，并加入真实 PMM / MME 生命周期测试及此验证记录。

<a id="record-user-test-fixes-九项问题"></a>
### 九项问题

| 编号 | 根因与处理 | 回归证据 |
| --- | --- | --- |
| 1 大型 PMM → 新建 → 重开崩溃 | PMX 骨骼表情工作数组按条目数量分配，`ModelApplyMorphs` 却按模型骨骼编号访问。稀疏的高编号骨骼会越界写入其他堆对象，随后在配件 `DrawSubset` 等位置崩溃。本轮改为按骨骼数量分配并初始化，每个骨骼只应用一次累计结果；校验骨骼、组表情及材质索引。 | `morph_runtime_tests`：128 个骨骼、少量重复引用第 127 个骨骼的表情，验证直接/组表情叠加、重复初始化、跨帧清零及非法索引；真实 PMM 生命周期测试。 |
| 2 切中文后加载 PMM 崩溃 | 地址检查器在切语言重载时捕获同一骨骼表情越界，语言切换改变分配时序，使破坏落到不同对象。另纠正先前配件诊断修复中的 D3DX 虚表槽号错误，并使用明确的骨骼编号恢复下拉框选择。 | 日语、英语、中文循环切换后加载同一舞台；ASan 检查；`pmm_reload_tests` 使用真实 D3DX 网格校验 VB/IB、设备及绘制。 |
| 3 骨骼/表情语言不一致 | 已有 `178f543` 将 `englishUI != 0` 改为仅英语值使用英文名称。本轮补齐 Unicode 骨骼下拉框、PMX 完整名称及 PMD CP932 解码；跟随/附着骨骼使用 `CB_SETITEMDATA` 保存编号，避免重名或编码转换改变选中对象。 | `ui_language_tests`：三种语言、真实 CP932 日文、PMX 中日文长名称、切换语言、重复名称及过滤后的骨骼编号。 |
| 4 日语骨骼操作按钮缺字 | 已有 `178f543` 补齐遗漏的日文按钮文本，并纠正错误读取的字节序列。 | `ui_language_tests` 断言 494「全て選択」、501「未登録選」、562「なし」及中英切换后的文本。 |
| 5 FOV 滑杆无实时效果 | 已有 `214901d` 修复投影只更新一次、随后被其他绘制阶段覆盖的问题，按实时相机状态逐帧重建透视投影。 | `panel_key_tests` 覆盖数值与投影计算；场景测试发送真实滑杆消息并读取设备投影矩阵。 |
| 6 初期化与 Del | 已有 `214901d` 修复 Del 仅允许主窗口焦点的限制，并删除所选骨骼在当前帧的键；初始化增加未就绪数组保护。全选/初始化的可编辑骨骼过滤与参考行为一致，末端及跟随类骨骼并非全选目标，未将这类排除改成强制复位。 | `panel_key_tests` 覆盖选中、未选中、父/无父骨骼与四元数；真实工程执行全选、改变所有选中且有父骨骼的位姿、初期化并逐个断言，再验证 Del 删除当前键。 |
| 7 I/K 空帧操作 | 已有 `214901d` 补时间轴刷新。本轮发现第二层分发仍要求主窗口焦点，即使采样层已允许面板按钮，实际命令仍被拦截；现统一焦点规则，保留前台归属及文本输入保护。 | `keyboard_input_tests` 验证实际消费层的按钮/清空/编辑框焦点；场景测试通过字母按键分发验证键从 100 → 101 → 100。 |
| 8 点击骨骼列表无法选中 | 已有 `747d74b` 修复按下事件使用过期鼠标坐标及绘制行编号映射。本轮保留编号选择，避免 Unicode/重名回退为字符串匹配。 | `window_interaction_tests`；真实工程把缓存鼠标坐标设为错误位置，再直接发送按下/抬起消息，检查显示行对应骨骼确实选中。 |
| 9 Ctrl+F 对话框乱码且确定无效 | 已有 `747d74b` 将 Shift-JIS 对话框文本显式按 CP932 解码，并对空剪贴板/无目标给出反馈。本轮统一的快捷键分发同时覆盖 Ctrl+F。 | `window_interaction_tests` 验证文本及前置条件；真实工程通过 Ctrl+F 分发，自动确认本测试线程的粘贴对话框，并验证目标骨骼确实产生关键帧。 |

<a id="record-user-test-fixes-2026-09-20时间轴框选与骨骼登记点击"></a>
### 时间轴框选与骨骼登记点击

- 框选命中表按每帧 200 行排列。旧扫描在内层已前进选区行数后又前进 200，导致后续帧逐列偏离选区；模型和配件扫描均修正步长，并限制窗口外拖动的索引范围。配件清选同时修正了行号到槽号的偏移。
- 折叠组的点击范围改用帧列、轨道行坐标，移除错误的指针截断。拖动期间保留选区，防止额外重绘清空折叠组的选择；鼠标松开时处理最终位置，捕获被其他控件接管或取消时结束拖动。
- 3D 骨骼点选改在鼠标按下消息中执行，避免按下、松开都发生于两次渲染之间时漏选。主窗口及独立视图均使用该处理；Shift 点选只切换一次。
- `bone_registration_tests` 通过真实窗口消息验证多行多列框选、反向与快速拖动、折叠组及中途重绘、配件选区缩小、窗口外边界、丢失鼠标捕获、列表/3D 快速点选后单击原生登记按钮，以及不向未选骨骼写帧。恢复旧 `ui_mousemove.cpp` 后，相同跨行框选测试稳定失败；恢复修复后通过。
- Release 构建成功；`bone_registration_tests`、`panel_key_tests`、`window_interaction_tests`、`keyboard_input_regressions` 共 4 项通过。本轮验证使用合成模型和隐藏原生窗口，不代表已对用户截图中的工程进行人工操作复测。

<a id="record-user-test-fixes-2026-09-20首帧固定与中文时间轴文字"></a>
### 首帧固定与中文时间轴文字

- 拖动时明确将各轨道的保留首记录固定为第 0 帧，保持其位姿和链表连接。正常轨道原有的首记录过滤继续生效；对已经偏离 0 的首记录，不能只跳过移动而保留异常帧号。
- `bone_registration_tests` 覆盖首记录和后续关键帧混选、向右拖动、拖到时间轴左边界外、返回原位置，以及“操作中心”不是骨骼 0、首记录已偏离 0 的情况；检查 0 帧命中表、姿势保留及链表顺序。
- 时间轴名称使用 Unicode 绘制，字体按中/英/日语明确选择，字号按包含内留白的单元格高度设置。骨骼、分组、表情、配件和固定轨道标题统一在 14 像素行内垂直居中，长名称截断于名称栏，避免压线或覆盖第 0 帧列。
- `ui_language_tests` 使用实际 GDI 位图检查三种语言的文字像素均位于所属行和名称栏内，且不受调用者文字对齐状态影响；同时检查中文预览位图。

<a id="record-user-test-fixes-2026-09-20选中骨骼后无法移动"></a>
### 选中骨骼后无法移动

- 骨骼选择位图与 `selectedBone` 可能不同步：3D 视图 Shift 取消当前骨骼会把编辑目标清成 -1，即使仍有其他骨骼选中；列表取消当前骨骼会保留旧目标，框选/组选也没有明确更新目标。现在在这些路径共用的面板刷新处校正目标，保留仍被选中的当前骨骼，否则从剩余选择中确定目标，无选择时才清空。
- 数值输入框的 `WM_KILLFOCUS` 原先错误地提交 `wParam` 指向的下一个焦点窗口，导致点击别处后刚修改的坐标没有保存。改为提交输入框自身；Enter 通过同一失焦路径提交，避免为一次输入创建两份撤销快照。
- `bone_registration_tests` 在修复前分别复现坐标失焦未写入、Shift 取消后编辑目标为空。新增隐藏 Win32 窗口测试验证列表/3D 取消选择后的目标一致性、屏幕平面移动、轴向移动、数值提交、Enter 单次撤销、替换/清空选择；合成模型测试不等于已复测用户截图中的模型与工程。
- Release 程序构建成功；`bone_registration_tests`、`ui_language_tests`、`panel_key_tests`、`window_interaction_tests`、`keyboard_input_regressions` 共 5 项通过。

<a id="record-user-test-fixes-2026-09-20切到非零帧后不能移动"></a>
### 切到非零帧后不能移动

- 在用户指定的根目录极星舞台上复现：`StepFrame` / `RefreshAfterFrameApply` 的共同准备步骤把 `boneSelection` 当成了未登记编辑标记清空，随后移动循环找不到任何选中骨骼；名称栏又没有同步重绘，仍可能显示红色选中。改为快照并清空 `bonePhysicsState`，与时间轴标题点击路径保持一致，保留骨骼选择与当前编辑目标。
- `bone_registration_tests` 增加第 0/5/10 帧的直接应用、前后步进和原生时间轴标题点击，验证未登记标记清除、选中状态保留，以及空白非零帧上能继续移动。
- `scene_lifecycle_tests <Stage.pmm> 3 --bone-movement` 使用实际舞台（11 个模型、15 个配件），三轮分别运行日/英/中文。选取右足可移动骨骼，在 0→5→10→0→5 帧循环中验证三种切帧入口、坐标输入框失焦提交、鼠标移动消息、六种移动计算模式、登记及完整渲染/物理更新后的位姿保留；同时检查骨骼计算后的世界坐标有实际变化及进程堆完整性。
- 测试在隐藏窗口内发送消息，移动模式由测试设置，不等同于通过物理鼠标完整拖动手柄。本轮未复现用户报告的“未响应”，未将其归为已修复；测试不保存工程文件。
- Release 构建成功，4 项回归（`bone_registration_tests`、`panel_key_tests`、`window_interaction_tests`、`keyboard_input_regressions`）通过。普通测试宿主及确认加载 MMEffect/MMHack 的 MME 测试宿主各完成三语言 3 轮舞台验证，共 6 轮通过。

<a id="record-user-test-fixes-2026-09-20帧范围物理菜单与遗漏汉化"></a>
### 帧范围、物理菜单与遗漏汉化

- 播放范围标记改为“开始 / 结束”，`v-sel` 改为“纵选择”，`expand` 改为“大小缩放”。范围下拉框补齐“全部帧、显示·IK·外亲、选中骨骼、选中表情、全部表情、全部骨骼”。“全部帧”仍包含骨骼、表情、显示轨道，不能误标为仅骨骼。
- 固定范围项使用 combo item data 保存语义标识，不再让中文显示文字改变命令范围；选中骨骼直接按骨骼索引选轨道，避免普通骨骼名误走显示组名称查找。
- “选择物理开启帧”原先从 `model + 0x26E0/0x26E4/0x26E8` 读取 x86 指针，并按 28 字节遍历在 x64 上实际为 40 字节的显示帧。现使用 `ModelRecord`、`BoneKey`、`MorphKey`、`DisplayKey` 字段访问，按真实帧链选出物理开启的关键帧。
- “切换物理开关帧”菜单和实际弹窗、重力设置窗口补齐中文标题、说明、按钮与模式项。
- PMM 打开失败、无效文件和不支持版本改用 Unicode 三语言提示，保留路径和错误码；场景测试先解析绝对路径，再初始化会改变当前目录的程序。
- `ui_language_tests`、`ui_message_tests`、`bone_registration_tests` 验证范围选择的实际结果、原生物理菜单、实际弹窗语言切换。极星舞台 11 模型 / 15 配件在日、英、中三轮中，对每个模型发送 3 次物理开启帧菜单消息，检查选择和堆完整性，共 99 次通过；测试不保存工程。

<a id="record-user-test-fixes-崩溃证据与-d3dx-abi"></a>
### 崩溃证据与 D3DX ABI

后续对 x86 运行时偏移的系统清理、实际发现的问题、保留的文件格式访问及最新 19 项回归 / ASan 验证，见 [运行时布局审计](audits.md#record-runtime-layout-audit)。

修复前，真实舞台在重载循环中重现崩溃。MSVC AddressSanitizer 捕获的第一处非法访问位于：

```text
heap-use-after-free, READ of size 4
QuatMul -> AccumBoneEntry -> ModelApplyMorphs -> PhysicsFrame -> FrameDriver
```

被访问地址恰好属于已释放的 D3DX 分配，但根因是 `work[entry.boneIndex]` 超过骨骼表情数组的分配范围。不能据最终 `d3d9.dll` 栈帧推定驱动或网格本身有错，也不需要把所有 `DrawSubset` 改写为手工 VB/IB 绘制。

另外，旧提交 `d3d5b92` 把 `ID3DXBaseMesh` 槽 7 当成 `GetVertexBuffer`。槽 7 实际是写入顶点声明数组的 `GetDeclaration`，把指针变量地址传给它会破坏栈。正确的槽号是 `GetOptions=9`、`GetDevice=10`、`CloneMeshFVF=11`、`GetVertexBuffer=13`、`GetIndexBuffer=14`，与位数无关。现在统一在 `d3dx_mesh.hpp` 声明调用类型，以真实设备和网格测试返回对象及绘制成功；不再把合法 COM 缓冲共享本身当成悬空指针。

ABI 核对来源：[Wine 的 d3dx9mesh.h 接口定义](https://github.com/wine-mirror/wine/blob/master/include/d3dx9mesh.h)。

<a id="record-user-test-fixes-运行方式"></a>
### 运行方式

沿用项目已配置的 Conan / Visual Studio x64 构建目录，开启：

```text
MIKUDANCESTUDIO_BUILD_INPUT_TESTS=ON
MIKUDANCESTUDIO_BUILD_EFFECT_TESTS=ON
MIKUDANCESTUDIO_BUILD_ENCODING_TESTS=ON
MIKUDANCESTUDIO_BUILD_UI_TESTS=ON
```

测试目标均为 `EXCLUDE_FROM_ALL`，需显式构建，不进入发布目录：

```powershell
cmake --build build --config Release --target keyboard_input_tests effect_api_tests bone_projection_tests model_alpha_tests device_reset_tests filename_encoding_tests morph_import_tests ui_language_tests panel_key_tests window_interaction_tests pmm_reload_tests morph_runtime_tests scene_lifecycle_tests scene_lifecycle_mme_tests
ctest --test-dir build -C Release --output-on-failure
$scene = (Resolve-Path '.\极星动力LIVE舞台 Ray用一键渲染\UserFile\Stage.pmm').Path
& '.\build\tests\Release\scene_lifecycle_tests.exe' $scene 10 editor
```

真实场景测试使用与正式程序相同的初始化、加载、清理、物理、渲染和编辑实现。每轮切语言、新建、加载同一 PMM、检查资源数量、渲染 12 帧并检查进程堆；加 `editor` 才执行编辑命令断言。测试只修改进程内数据，不保存 PMM。按键测试在分发边界提供焦点快照，避免抢占用户当前窗口；正式程序仍检查前台窗口。粘贴确认由测试线程自己的对话框回调完成。

MME 测试宿主为 `build/tests/mme/Release/MikuMikuDance.exe`，保留原始宿主名称与导出序号供 MMHack 导入。需自行提供本地 MME 的 `d3d9.dll`、`MMEffect.dll`、`MMHack.dll`、相应运行库以及 `Data` 目录；缺少 MME 时明确失败，不能把未加载 MME 算作通过。第三方 DLL 和测试舞台均不入库。

ASan 使用独立 `build-asan`，MSVC `/fsanitize=address`、`MIKUDANCESTUDIO_DIAG=ON`。本机 DirectSound 工作线程与 ASan 组合会产生额外故障，检查时设置 `MIKUDANCESTUDIO_SKIP_DSOUND=1`，仅跳过音频初始化，保留物理和渲染；不使用跳过物理、跳过配件或跳过绘制的诊断开关。运行时需将 MSVC ASan DLL 所在目录加入该进程的 `PATH`。

<a id="record-user-test-fixes-验证结果"></a>
### 验证结果

- Release 回归集：12/12 通过。
- 本地大型舞台：每轮载入 11 个模型、16 个配件；普通 Release 连续 10 轮重载通过。
- 本地 MME 已加载：连续 10 轮重载及编辑断言通过；补齐快捷键消费层后，三种语言的完整 Ctrl+F、I/K 分发再测通过。
- ASan：骨骼表情回归通过；真实舞台三种语言的重载及完整编辑命令检查全部通过，未报告非法内存访问。

这些是给定场景与有限轮次的回归结果，不等同于所有模型、效果插件及无限时长运行的保证。第 6 项的非可编辑骨骼过滤是保留的 MMD 行为。

<a id="record-user-test-fixes-2026-09-22相机物体视图跳变视图轴颜色与坐标系标识"></a>
### 相机/物体视图跳变、视图轴颜色与坐标系标识

承接 `8dd6851`（该提交引入局部/全局/view 三态与文本标识）。用户报告四项：相机视图与物体视图互切仍跳变；“视图轴无效”；`view` 单词与坐标标识字体；Z 轴、Y 轴颜色不正确（以 `refs` 内的 MMD 本体为准）。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| Z、Y 轴颜色错误 | `InitAxisMesh` 从 `ID3DXBuffer` 逐条拷贝材质时写死 72 字节步长，那是 x86 的 `D3DXMATERIAL` 尺寸；x64 因尾部 `LPSTR` 需要 8 字节对齐而补齐到 80，材质 1 起全部错位，只有材质 0（红 X）正确。 | 步长按目标架构取 72/80，并对 `sizeof(D3DMATERIAL9) == 68` 加静态断言。颜色本身取自内嵌 `axis.x`（`res/assets/axis.x`）：材质 0 红 `(1.0,0,0.035)` 覆盖 +X、材质 1 蓝 `(0,0.043,1.0)` 覆盖 -Z、材质 2 绿 `(0.039,1,0)` 覆盖 +Y、材质 3 黄为原点方块、4/5 为黑色描边。`coordinate_axis_tests` 直接读回材质断言。 |
| 坐标系标识未居中、三态位置不统一 | 旧的标识按“每字符 16 像素图集格”采样。图集里的 `global`/`local` 是 GDI 按比例字体一次绘制的整串，中文界面还会换成 `全局`/`局部`/`视图`，既非等宽也不是英文宽度，于是字形被切开、居中按英文字符数计算。 | 建图集时记录每个标识的 GDI 步进宽度，并从绘制后的图集像素里量出实际着墨列区间，绘制时整串作为一个四边形、按实测宽度在 120×30 选择区内居中；纵向仍按 32 像素行对齐，使三态共用一条基线（`global` 的下伸部不会把它顶高）。图集每次重建（切换语言）都会重置测量表，避免沿用上一语言的宽度。`coordinate_axis_tests` 对日/英/中三种语言、三个状态断言中心偏差 ≤3 像素且三态中心一致，并断言存在白色字面与黑色描边。 |
| 相机 ↔ 物体视图跳变 | 相机模式用相机关键帧位置（`cameraPos`）作旋转中心，自由视图用 `viewOffset`/`cameraDistance`；两者互不同步，切面板即跳到陈旧值。此外自由视图把 `cameraDistance` 同时当作中心 z 使用，等效相机距离翻倍，同一镜头在两种模式下的模型大小不同。 | 相机模式下把自由视图 x/y 锚定到相机目标（与已附着/追踪分支一致）；自由视图中心固定在 y=0 地面平面，深度只由 `cameraDistance` 提供。原版把 `viewOffset`/`cameraDistance` 作为 `D3DXMatrixLookAtLH` 的眼点（x64 `0x140027F65..0x140027FD6`），中心来自世界矩阵平移。 |

验证方式与范围：

- `coordinate_axis_tests`（新增，CTest 名 `coordinate_axis_regressions`）使用真实 D3D9 设备、内嵌 `axis.x` 资源与真实 HUD 字体图集，覆盖上述前两项与视图轴方向（相机偏航 90° 时屏幕右方向应为世界 +Z）。
- 跳变以本机 Release 程序实拍前后帧比对：加载模型后把主组合框切到“相机”，视口区域差异从数千像素降到仅剩相机目标标记（313 像素），地面网格与 Y 轴位置不再移动。
- 本机屏幕仅 1463×914，MMD 窗口布局被压缩，实拍图不能替代用户在 2560×1600 桌面上的观感复测；`view` 状态在相机/配件面板中的第三态语义（原版该面板第三态为配件）本轮按用户要求统一显示为 `view`。

<a id="record-user-test-fixes-2026-09-22续hud-文字的语言切换与坐标系标识位置"></a>
### （续）：HUD 文字的语言切换与坐标系标识位置

用户报告：「低于x帧」与坐标系类型文字在切到日文/英文后没有跟着变；坐标系标识需要按「xyz 移动」与「xyz 旋转」两行按钮的间距再上移一点；具体措辞以 MMD 原版为准。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 切语言后两条视口文字仍是旧语言 | 场景 HUD 字体图集在启动时用当时的语言画一次，`LocalizeUI`（菜单 260，0x441AD0）只刷新面板与菜单，不重画图集，于是图集里的 `全局/局部/视图`、`低于1帧/秒` 一直被沿用。 | `LocalizeUI` 末尾调用 `InitSceneFontTexture`（0x42AE80）重画图集；该函数在设备未就绪时直接返回，不会破坏启动阶段的调用。`coordinate_axis_tests` 断言：英文 → 中文各调一次 `LocalizeUI` 后，`global` 图集格的实测宽度必须改变（英文 74px、中文 58px）。 |
| 中文界面显示 `1fps 未満` | 该行图集有两个原始字符串：英文锚点 x=0 的 `1fps or less`、日文锚点 x=208 的 `1fps 未満`；`DrawUiGlyph` 只在**英文锚点**替换中文措辞，而代码用 `EnglishUI() != 0` 判断，把中文也当成日文取了第二格。 | 改为只对 `kUiJapanese` 取日文格，英文与中文共用英文锚点格（中文为 `低于1帧/秒`）。`coordinate_axis_tests` 直接读回文本批次顶点的 u 范围，按语言断言格位（日文 0.375..0.6855，英/中 0..0.373）。 |
| 坐标系标识离按钮行过近 | 原版精灵条位于 `bottom-115..bottom-85`，与「xyz 移动」按钮行（`bottom-80..bottom-50`）只隔 5 像素，而两行按钮之间的间距是 10 像素（`bottom-50..bottom-40`）。 | 标识整体上移 10 像素（`bottom-125..bottom-95`），使它与按钮行之间也留出一行间距。`coordinate_axis_tests` 断言标识中心落在 `view.bottom-110`。 |

措辞依据 MMD 原版：帧率提示沿用原版 HUD 字符串（英文 `1fps or less`、日文 `1fps 未満`；中文为移植新增的 `低于1帧/秒`）；坐标系标识沿用原版 HUD 的英文词 `local`/`global`（第三态当时按用户要求显示 `view`），中文界面经字典显示 `局部`/`全局`/`视图`——原版精灵图集本身即为英文词，日文界面同样使用这些英文词。第三态的措辞与字体已于 2026-09-23 按用户要求修订，见下一节。

本轮验证：`coordinate_axis_tests` 扩展后通过；`build`（由 `build-current` 改名并重新生成 Conan 生成器）下 `ctest` 22/22 通过。仍未由用户在其 2560×1600 桌面上复测观感，上移量（10 像素）如不合适可直接改为其它数值。

<a id="record-user-test-fixes-2026-09-22续视图坐标系旋转相机视角锁定条件pmm-文件对话框取消"></a>
### （续）：视图坐标系旋转、相机视角锁定条件、PMM 文件对话框取消

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 选「视图」坐标系没有效果 | 模型面板的旋转手柄（拖拽模式 4/5/6）无条件使用骨骼自身轴向，只有平移手柄（10/11/12）才读坐标系选择，因此 local/global/view 在旋转上完全一致。 | 旋转轴改为按 `coordinateSystem` 选择：0 用骨骼局部轴（原路径）、1 用世界轴、2 用 `ViewWorldAxis` 的屏幕轴，统一换算到骨骼父空间（`matWorld` 转置）。`coordinate_axis_tests` 断言三种模式的两两四元数不同，且相机偏航 90° 时「视图 X 旋转」必须等于「全局 Z 旋转」。 |
| 相机面板与模型面板互切时视口跳变 | 相机面板只要被选中就用相机关键帧的位置作旋转中心（并在上一轮把自由视图锚定到相机），于是切到相机就跳到相机视角、切回模型又丢掉原视角。 | 视口默认使用自由视角；只有「相机面板 + 正在播放」或「开启追踪」时才强制相机视角。`coordinate_axis_tests` 断言：停播时相机面板与模型面板的帧变换矩阵完全相同，播放时相机位置才会进入变换。 |
| PMM 里文件对话框取消会中止整个导入 | 模型/配件缺文件后弹出的文件对话框一旦被取消（`GetOpenFileNameW` 返回 0）就 `AbortV2Load`/`AbortV1Load` 中止装载。 | 取消改为与「跳过该文件」同路径：v2 模型缺文件按对话框的 skip 分支（`doSkip = true`）、结构不匹配重载按 r==4 跳过分支；v2/v1 配件则保留已初始化的空配件记录并继续消费其记录；v1 模型沿用失败重试路径（空槽继续状态装载）。改动点：`pmm_load_v2.cpp`（3 处）、`pmm_load_v1.cpp`（2 处）。 |

未覆盖：文件对话框取消这条路径没有自动化测试（需要真实对话框交互），仅做了静态核对与编译；`coordinate_axis_tests` 扩展覆盖前两项。

<a id="record-user-test-fixes-2026-09-23坐标系标识的措辞字体与相机第三态"></a>
### 坐标系标识的措辞、字体与相机第三态

用户报告三项：视图操作的英文显示改为首字母大写；所有语言的说明字体改为衬线字体；对摄像机而言还有一种以 Accessory 坐标系为准的第三态，没有 view 态——因为它自己就是 view。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 英文标识为小写 | 标识词直接沿用原版精灵图集里的 `local`/`global`（图集里第三格是 `accessory`），原样画进 GDI 图集。 | 标识词改为首字母大写：`Local` / `Global` / `View` / `Accessory`。图集键即绘制词，因此中文字典补入 `Local→局部`、`Global→全局`、`View→视图`、`Accessory→配件`；旧的小写条目保留给其它控件。`coordinate_axis_tests` 断言四个标识格都存在于图集，并从文本批次顶点的 u 范围核对实际取用的格位。 |
| 说明字体非衬线 | 图集文字统一走 `DrawGlyph`（Arial / MS PGothic）与 `DrawWideUiGlyph`（Chinese UI 的 Microsoft YaHei UI）；而原版精灵图集里的 local/global/accessory 是蓝色衬线字。 | 新增 `UiCaptionFaceName`（英文 Times New Roman、日文 MS PMincho、中文 SimSun）与 `UiGlyphFace::Caption`；`InitSceneFontTexture` 只对这四个标识行使用该字体，其余 HUD 文字（`1fps or less`、帧号与数字）仍保持原版的无衬线配置，避免改动原版行为。`coordinate_axis_tests` 断言三种语言的标识字体均为对应衬线字体。 |
| 相机面板第三态显示 `view` | 上一轮按用户要求把原版该面板的 `accessory` 词统一改成了 `view`（见上一节）。 | 第三态按面板取名：模型/骨骼面板为 `View`（屏幕轴，`ViewWorldAxis`），相机/照明/配件面板为 `Accessory`（编辑所选配件，见 `ModeCameraAdjust`/`ModeAngleAdjust` 的 `target == 2 && optflag[0] != 0`；相机自身即视图，故没有 view 态）。`coordinate_axis_tests` 断言模型面板第三态取 `View` 格、相机面板第三态取 `Accessory` 格。 |

验证与范围：

- `coordinate_axis_tests` 扩展为 3 种语言 × 2 个面板 × 3 个状态共 18 次真实渲染，断言标识落在 120×30 选择区内（白字面、黑描边、居中），并断言三种语言的标识字体映射与各面板第三态的格位。
- 垂直居中的判据由「墨量加权中心」改为「着墨框中心」：衬线字面沿基线更重，`Accessory` 又带下伸部，加权中心会比共用的字行低约 4 像素，会把字体差异误判为位置缺陷。
- 本地目视核对：图集行与选择区渲染（Times New Roman）为 `Local` / `Global` / `View`（模型面板）与 `Local` / `Global` / `Accessory`（相机面板），白字黑描边，居中。
- `Accessory` 实测着墨宽约 125 像素，比 120 像素选择区略宽（原版该词约 108 像素，两侧留白）。选择区只是点击热区、不绘制边框，故未遮挡任何控件；若观感需要，可单独下调标识字号。
- 仍未由用户在其 2560×1600 桌面上复测观感。

<a id="record-user-test-fixes-2026-09-23续坐标系标识的中日英字体一致性与描边清晰度"></a>
### （续）：坐标系标识的中日英字体一致性与描边清晰度

用户报告三项：坐标系模式切换处的标识在中日英三种语言下均绘制不清；日文与英文字体不一致；中文应换回无衬线字体。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 日文与英文的同一标识字形不同 | 上一节为日文选用了 MS PMincho。日文界面的标识是英文词，Mincho 的拉丁字形与英文界面所用的 Times New Roman 差异明显；MS PMincho 不提供粗体字面，GDI 只能合成伪粗体，笔画偏细且边缘发毛。 | 日文与英文统一使用 Times New Roman。二者显示同一批英文词，共用字面后渲染结果逐字节相同。`UiCaptionFaceName` 仅对 `kUiChinese` 返回标签字体，其余一律返回 Times New Roman。`coordinate_axis_tests` 断言日文与英文取用同一字体（不再分别断言各自的字体名）。 |
| 中文标识发虚 | 上一节中文使用 SimSun。宋体在 30 像素下笔画极细，且无粗体字面，GDI 合成的伪粗体将笔画错开成双线。 | 中文标识改回标签字体 Microsoft YaHei UI，与中文 HUD 其余文字保持一致，笔形粗实。`coordinate_axis_tests` 断言 `UiCaptionFaceName(kUiChinese)` 为 `Microsoft YaHei UI`。 |
| 三种语言的标识均发糊 | 标识的黑色描边采用 `1.25 * viewScale` 的四向对角偏移。在 2560 像素桌面上 HUD 缩放约为 2.0，描边偏移随之增至约 2.5 像素，30 像素字形的字怀（`o`、`a` 的内白）被填满，整词糊成白块。 | 描边改为固定 1 个设备像素、八个方向（补上四正方向，斜向偏移 0.71 像素），不再随 HUD 缩放变化。`coordinate_axis_tests` 新增：HUD 缩放 2.0 时，标识四边形的总高超出 32 像素图集行的部分不超过 2.5 像素（旧代码为 5 像素），并断言标识由八个描边四边形与一张白字面共 9 个四边形组成。 |

验证与范围：

- 选型依据：将六种候选字体（Times New Roman、MS PMincho、SimSun、Microsoft YaHei UI、MS PGothic、Arial）× 两档字重（700/400）在 30 像素下绘制 `Global` 与 `局部` 的样张逐张目视比较。结论为 YaHei UI 的粗体最为粗实，Times New Roman 粗体次之；SimSun 与 MS PMincho 无粗体字面，合成效果最为发虚。
- 改动后将三种语言 × 三个状态在 HUD 缩放 1.0 与 2.0 下渲染成图并逐张目视核对：日文与英文的两张图 SHA-256 相同（像素结果一致），中文为 YaHei UI 的局部/全局/视图。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 的 `--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection` 在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上 exit 0。
- 未覆盖：本轮仅在 512×384 的离屏设备上出图，仍未由用户在其 2560×1600 桌面上复测观感。标识图集仍按 30 像素烘焙，HUD 缩放大于 1 时整个 HUD（含标识）仍由 GPU 放大，字形边缘因此仍略软；本轮解决的是字体选型与描边权重，未改变图集的烘焙尺寸。
- 本轮构建时，`build\Release\MikuMikuDance.exe` 正被用户打开运行，应用可执行文件无法重新链接，因此仅重建了库与测试二进制；应用本体需在该进程退出后重新构建。

<a id="record-user-test-fixes-2026-09-23续二标识的放大模糊与无配件时的选择器范围"></a>
### （续二）：标识的放大模糊与无配件时的选择器范围

用户报告了两项问题：无配件时应仅在局部与全局之间切换，不允许切到配件；描边与白色字面的交接处仍明显模糊（较上一轮有所改善）。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 无配件时仍可切到配件态 | 选择器条的点击路径（`BeginOrEndViewportToolDrag` 的 operation 21）使用固定上限 3，与配件是否存在无关；标识也照常显示「配件」。 | 新增 `CoordinateStateLimit(app)`：模型/骨骼面板恒为 3（View 仅需视口）；相机/照明/配件面板仅在当前配件槽有记录时为 3，否则为 2。点击与 `L` 键均改用该函数，标识绘制时将越界的旧状态钳到 0（显示「局部」），因此即使状态字节残留 2，也不会显示为「配件」。`coordinate_axis_tests` 断言：无配件时上限为 2 且标识落到 `Local` 格，装回配件后恢复为 3。 |
| 描边与白字交接仍模糊 | 标识以 30 像素烘焙进图集，HUD 缩放为 2.0 时整块被 GPU 放大两倍；描边是同一抗锯齿字形的副本，交界处的渐变随之被放大至 2 像素以上。 | 图集中为四个标识各存一份 60 像素副本，绘制时按 `viewScale` 选取更接近目标尺寸的一份：缩放 ≤1.41 时使用 30 像素副本，否则使用 60 像素副本，2560 桌面上接近 1:1 采样。副本采样 64 行窗口（= 2×32 行），因此两档绘制出的尺寸一致（测试断言缩放 2.0 下两档宽度差 ≤4 像素）。窗口按实测墨迹居中（GDI 在 60 像素下比 30 像素下低数行，中文另有固定 3 像素上移），并避开上一行标识的墨迹，详见下条。 |
| 60 像素副本初版出现字上方碎片、下伸部被裁 | 直接将「绘制行 + 64」作为采样窗口：288 行的中文墨迹绘制至 320 行，窗口首行因而采到上一行标识的最后一行；而 60 像素下的中文墨迹比 2×30 像素高 2 行，窗口底部又裁掉 `Accessory` 的下伸部。 | 窗口改为依据实测墨迹推算：取墨迹上下界的中心，向下取 64 行窗口，并将窗口上界钳在 321 行（288 行的中文墨迹至 320 行为止）。`Accessory` 与 `配件` 在缩放 2.0 下出图核对，下伸部完整、字上方无碎片。 |

验证与范围：

- 逐张出图核对：三种语言 × 三个状态在 HUD 缩放 1.0 与 2.0 下的渲染（448×320 离屏设备），确认 60 像素副本下描边与白字交接清晰、`Accessory` 下伸部不被裁切、中文无碎片，且 30 像素副本（缩放 1.0）与改动前像素一致。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 的 `--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection` 在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上 exit 0。
- 未覆盖：仍需用户在其 2560×1600 桌面上复测观感。512×512 图集仅能容纳两档副本，60 像素副本的采样倍率等于 `viewScale / 2`，因此 HUD 缩放超过约 2.8 时它同样会被放大（放大倍率大于 1.4）；若要覆盖更大的窗口，需再添一档副本，而图集已无空间，必须改用第二个纹理或更大的图集。
- `Accessory` 在小字号下的实测着墨宽度约为 125 像素，略宽于 120 像素的选择区（选择区仅作点击热区，不绘制边框）；两档渲染下的宽度一致，本轮未改动该词的宽度。

<a id="record-user-test-fixes-2026-09-23续三一次点击把坐标系推进两步"></a>
### （续三）：一次点击把坐标系推进两步

用户报告：切换不够流畅，像是「误切到配件态后又切回原来的模式」，期间出现瞬时的字符闪动。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 一次点击推进两步 | `HandleViewportToolRelease`（`frame_modes.cpp`）不论按钮是否仍被按住，都会将 `leftMouseButtonState` 写为 0。消息级按下处理（`HandleViewportToolPress`）会先把该字节置 1 以触发一次推进，再置 3 表示「已消费本次按下」；而 `WM_CAPTURECHANGED` 与 `WM_CANCELMODE` 都会经由 `HandleLButtonUp` → `HandleViewportToolRelease`，在按钮仍被按住时将其清回 0。下一帧由帧驱动的 `PollKey`（沿判据为「idle → down」）便会把同一次物理按下识别为**新的按下**，`ViewportToolAtPoint` 仍在选择条上返回 21，选择器因而再推进一次。两次推进叠加：无配件时上限为 2，从「局部」出发即 0→1→0，表现为「全局」闪现后又回到「局部」；有配件、上限为 3 时，从「局部」出发为 0→1→2，即用户先前报告的「误切到配件」。 | 新增 `ReleaseLeftButtonState(MMDApp*, bool physicallyHeld)`：仍按 2（释放沿，供拖拽逻辑收尾）调用一次 `BeginOrEndViewportToolDrag(app, 0)`，但当物理左键仍处于按下状态时保持状态为 3，只有真正抬起后才写 0。`HandleViewportToolRelease` 改为调用 `ReleaseLeftButtonState(app, LeftButtonPhysicallyDown(app))`；`LeftButtonPhysicallyDown` 与 `PollKey` 采用同一套判据（先 `GetKeyState`，窗口处于前台时再回退到 `GetAsyncKeyState`）。`coordinate_axis_tests` 新增断言：调用 `HandleViewportToolPress` 后状态只推进 1 步且 `leftMouseButtonState == 3`；按住时调用 `ReleaseLeftButtonState(app, true)`，该字节仍为 3，坐标态不再推进；放开时调用 `ReleaseLeftButtonState(app, false)`，该字节归 0。 |

验证与范围：

- `coordinate_axis_tests` 走的是真实的按下入口 `HandleViewportToolPress`（含选择条命中判定与 `PostLanguageSweep`），并断言一次按下只推进一个状态。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 的 `--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection` 在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上均 exit 0；Release 重建 0 error（含应用本体）。
- 未覆盖：本机无法在用户环境中人工复现触发 `WM_CAPTURECHANGED`/`WM_CANCELMODE` 的具体来源（本机测试是通过合成 `physicallyHeld` 参数覆盖该分支的），因此「点击过程中是谁抢走了捕获」仍未定位。若用户复测后仍存在闪动，下一步将在推进处加入计数诊断（每次推进记录一帧日志）后再行定位。

<a id="record-user-test-fixes-2026-09-23续四连点第二下被吞掉"></a>
### （续四）：连点第二下被吞掉

用户报告：点击依旧不灵敏。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 连续两次点击只生效一次 | 主窗口的窗口类样式为 `mainClass.style = 11`（`window_init.cpp:322`，即 `CS_VREDRAW \| CS_HREDRAW \| CS_DBLCLKS`），因此双击时间（系统默认 500 毫秒）内的第二次点击会以 `WM_LBUTTONDBLCLK` 送达，而不会产生第二条 `WM_LBUTTONDOWN`。主窗口的 `WM_LBUTTONDBLCLK` 分支（`wndproc.cpp:300`）只调用 `HandleViewportBonePress` 与 `HandleLButtonDblClk`，从不调用 `HandleViewportToolPress`，于是这次点击既没有推进选择器，也没有启动其它视口工具。浮动窗口的窗口过程（`mic_window.cpp:119-127`）则把 `WM_LBUTTONDOWN` 与 `WM_LBUTTONDBLCLK` 放在同一个 case 中，与主窗口处理并不一致。用户在手感不确定时往往会连点，于是表现为「点了没反应」。 | 将 `HandleViewportToolPress` 的主体抽为文件内的 `PressViewportTool(MMDApp*, int requiredOperation)`，并新增导出函数 `HandleCoordinateStripPress(MMDApp*)`（等价于 `PressViewportTool(app, kCoordinateStripOperation)`，其中 `kCoordinateStripOperation = 21`）。主窗口的 `WM_LBUTTONDBLCLK` 分支在浮动窗口为空时先调用该函数：命中选择条则消费该消息（`return 0`），未命中则维持原有处理（骨骼拾取 + `HandleLButtonDblClk`）。`coordinate_axis_tests` 新增断言：光标位于选择条上时 `HandleCoordinateStripPress` 返回真且坐标态只前进一格；光标移开后返回假、坐标态不变。 |
| 选择条顶部有一行像素点不到 | 选择条的命中框顶边为 `view.bottom - 125 * scale`，而标识按 32 行的图集单元格绘制、居中在 30 像素高的选择框内，因此绘制区域比命中框高出一行（自 `view.bottom - 126 * scale` 起）。 | 将命中框顶边改为 `view.bottom - 127 * scale`，使可点击区域覆盖实际绘制出的标识，并保留一行余量。 |

验证与范围：

- `coordinate_axis_tests` 覆盖两条新断言（双击消息能切换选择条；离开选择条时同一调用返回假且不改状态），并保留此前的「一次按下只推进一格」「丢捕获不重新武装按下沿」两组断言。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 的 `--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection` 在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上均 exit 0；Release 重建 0 error（含应用本体）。
- 未覆盖：本条按「连点第二下被吞」的解释处理用户报告，尚未由用户复测确认。若仍不灵敏，需要用户提供具体表现（点了完全没反应／要连点才生效／有延迟／偶尔跳两格），再据此定位。

<a id="record-user-test-fixes-2026-09-23续五配件坐标系是否被完全屏蔽"></a>
### （续五）：配件坐标系是否被完全屏蔽

用户要求核查配件坐标系是否已被完全屏蔽，并报告闪动与切换不灵敏的问题仍然存在。

核查结论：**没有完全屏蔽**。全仓库涉及 `state.coordinateSystem` 的行共 45 行（含测试与文档），其中生产代码只有三处写入：`app_defaults.cpp:117` 启动时置 0、`key_ladder.cpp:260`（`L` 键自增）、`frame_modes.cpp:496`（选择条点击 operation 21 自增）。后两处自上一轮起均已用 `CoordinateStateLimit` 设限，因此**循环已经到不了配件态**；但读取该字节并据此动作的地方没有同步设限，于是一个**残留的 2**（出现在选中槽位失去配件或面板切换之后）仍在驱动它们：

| 读取点 | 作用 |
| --- | --- |
| `frame_modes.cpp` 按下分支（`RefreshRequest(target == 2 ? selectedObjectSlot : -1)`） | 按下相机／配件拖拽控件时按该状态刷新对象 |
| `ModeCameraAdjust` / `ModeAngleAdjust`（同文件） | `target == 2` 时改为编辑所选配件的位置与角度 |
| `frame_modes_bone.cpp`（移动与旋转手柄、三处读取） | 选择骨骼局部轴／世界轴／屏幕轴 |
| `pump_navigation.cpp:381` | 右键拖拽时的刷新门 |
| `debug_geometry.cpp:505/517` | 操作轴 gizmo 的朝向 |
| `overlay_producers.cpp:218` | 选择条标识（本轮之前由其自行钳制） |

处理：

- 新增 `EffectiveCoordinateState(MMDApp*)`：将存储字节钳制到 `[0, CoordinateStateLimit)`，语义为「当前真正生效的坐标态」。上述六个读取点全部改用它。
- 新增 `NormalizeCoordinateState(MMDApp*)`：每帧开头（`FrameDriver` 调用 `MouseInteractionBegin` 之前）将钳制结果写回状态字节，使任何读取者（包括尚未发现的）读到的都是合法值。两个写入点本身已受上限约束，不会再写出越界值。
- 新增现场轨迹：`FrameDriver` 每帧调用 `TraceCoordinateSelector`，当「状态字节／面板字节／状态上限／配件槽号／该槽是否有配件／HUD 缩放」任一发生变化时，向 `MIKUDANCESTUDIO_TRACE_FILE` 指定的文件追加一行 `coord state=… panel=… limit=… slot=… accessory=… scale=…`；未设置该环境变量时不产生任何文件（沿用 `runtime_log::Trace` 的约定）。该轨迹已在真实舞台的测试进程中验证可用（出现 `coord state=0 panel=1 limit=2 slot=0 accessory=0 scale=100`）。

验证与范围：

- `coordinate_axis_tests` 新增断言：无配件且状态字节为 2 时，`EffectiveCoordinateState` 读作 0，`NormalizeCoordinateState` 将字节清回 0；装回配件后同一状态读作 2；模型面板在无配件时仍保留其第三态。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 的 `--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection` 在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上均 exit 0；Release 重建 0 error（含应用本体）。
- 未覆盖：闪动与不灵敏的**具体触发**仍未定位。下一轮请用户设置 `MIKUDANCESTUDIO_TRACE_FILE`（例如 `D:\MMD-Next\coord_trace.txt`）后复现一次，并提供该文件；其中的 `coord` 行会指出具体是哪个输入在跳变。若连一行 `coord` 都没有（说明六个输入均无变化，闪动发生在渲染侧），则下一步改为记录图集重建次数与 `SceneFontLabelCell` 的取档结果。

<a id="record-user-test-fixes-2026-09-23续六点多少次都不切换同一次按下被推进两次"></a>
### （续六）：点多少次都不切换——同一次按下被推进两次

用户报告：在相机／照明面板下无论点多少次，坐标系都看不出变化。上一轮已从读取侧钳掉残留的第三态，但闪动与不灵敏仍未定位；本轮定位到问题出在推进次数本身。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 点多少次都不切换 | 一次物理按下被两条检测路径各消费一次，净推进 2。主循环（`winmain.cpp:107-162`）只在消息队列为空时调用 `FrameDriver`；若按下发生在 `FrameDriver` 执行期间，`PollKey` 的 `GetAsyncKeyState` 回退会在 `WM_LBUTTONDOWN` 仍排队时就报告按下，帧路径据此推进一次；该消息随后被派发，经 `HandleViewportToolPress` → `PressViewportTool` 又推进一次。上限为 2（相机／照明面板且第三态不可达）时为 0→1→0，净位移为零，表现为「模式完全不变」；上限为 3 时则表现为「跳过一格」。 | 新增文件内标记 `g_viewportPressHandled`，语义为「当前跟踪到的这次按下已由工具消费」：`BeginOrEndViewportToolDrag` 在 `leftState == 1` 分支置位；`PressViewportTool` 在按键状态不为 0 且标记已置位时直接返回真，即消费掉这条消息但不再执行动作；`ReleaseLeftButtonState` 与 `PollKey` 在状态回到 0 时清除标记，因此真正抬起后的下一次按下仍会被视为新按下。 |
| 面板里明明有配件，第三态却不可达 | `CoordinateStateLimit` 此前只检查**所选**槽位是否有配件。载入的配件位于共享对象列表，而选择字节在用户点选行之前指向空槽，于是满载配件的面板仍停在两态，恰好落进「净推进 2 = 看不出变化」的区间。 | 上限改为扫描整个对象列表：只要存在任一配件即返回 3，列表清空后回到 2。`coordinate_axis_tests` 断言配件位于其它槽位时上限仍为 3，清空列表后回到 2。 |
| 现场无法判断是哪个输入在跳变 | 推进处此前没有逐次记录，只有状态变化时才写 `coord` 行。 | operation 21 的每次推进追加一行 `strip advance 旧值->新值 limit=…`，沿用 `runtime_log::Trace` 的约定：仅在设置了 `MIKUDANCESTUDIO_TRACE_FILE` 时写文件，正常使用不会在磁盘上留下任何文件。 |

验证与范围：

- `coordinate_axis_tests` 断言：同一次按下第二次调用 `HandleViewportToolPress` 不再推进（坐标态停在 1）；`ReleaseLeftButtonState` 之后的下一次按下照常推进（0→1→2），即该标记不会把后续点击一并挡掉。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 的 `--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection`、`--material-names` 与默认的编辑器命令路径在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上均 exit 0；Release 重建 0 error（含应用本体）。
- 未覆盖：仍需用户复测「点一次切一格」。若仍无反应，请设置 `MIKUDANCESTUDIO_TRACE_FILE`（例如 `D:\MMD-Next\coord_trace.txt`），复现一次后回传该文件。正常的一次点击应只产生一行 `strip advance`；若同一时刻出现两行，则说明仍存在第三条消费路径。

<a id="record-user-test-fixes-2026-09-23续七初启动视角偏高切一帧才回到-mmd-的起始位置"></a>
### （续七）：初启动视角偏高，切一帧才回到 MMD 的起始位置

用户报告：刚启动时视角偏高；原地切一帧后，视角便恢复到与 MMD 一致的初始启动位置。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 初启动视角比 MMD 高一截 | `InitDefaults` 把 `viewOffsetY` 误写为 10.0f。原版在这一步是用**一条 8 字节零写**同时清掉两个视图偏移：x64 `0x14000C5A1 mov qword ptr [rbx + 0x340], rdi`，其中 `rdi` 在整个初始化器中恒为 0（`0x14000C561 xor edi, edi`）。默认视点的 10.0f 只属于 cameraPosY：`0x14000C5DF mov qword ptr [rbx + 0x370], 0x41200000` 一次写入 cameraPosY = 10.0f 与 cameraPosZ = 0.0f。启动面板是相机面板（`UiOptionFlag(0) = 1`，原版在 `+0x328` 写 `0x1010101`），因此世界枢轴取 cameraPos = (0, 10, 0)，而 eye 取 `(viewOffsetX, viewOffsetY, cameraDistance)`（视图安装段 x64 `0x140027F75`～`0x140027FDE`：从 `0xA1900`/`0x344`/`0x340` 取 eye，以 `eye + (0, 0, 80)` 为 at，随后调用 `D3DXMatrixLookAtLH`）。多出的这 10 个单位把 eye 抬到枢轴上方 10 个单位，地面原点因而落在视口中心下方 `atan(20/45)` 处，而原版实测为 `atan(10/45)`。 | `src/app/app_defaults.cpp` 将 `ViewOffsetY()` 由 10.0f 改为 0.0f，并在注释中写明上述两条反汇编依据。`src/app/reset_app_state.cpp` 存在同一处误读（其写入随后被函数尾部的 `ReloadModels` 覆盖，此前未产生可观测影响），一并改为 0.0f。 |
| 切一下帧就恢复 | 相机面板下的切帧走 `RefreshFrameContext` 的相机分支，先调用 `ReloadModels`（VA 0x0042E640）；该函数在 exact-hit 与 terminal-key 两条分支上都会把两个视图偏移清零（`src/app/timeline_advance.cpp:211-212`）。清零后 eye 回到 y = 0，画面即与 MMD 的起始位置一致——也就是说，「切帧后的画面」才是原版的启动画面，而移植侧启动时少了这一步。 | 修正后，启动状态与一次帧求值等价，不再出现该跳变。面板切换本就各自重算这一对值：`RebuildModelModePanel` 把 cameraPos 复制进偏移，`RebuildCameraModePanel` 反向复制后再重载；「视点复位」类命令（402～406，`ResetViewVariant`）仅在模型面板且参照模式不是选中骨骼时写入 10。 |

验证与范围：

- 新增手动模式 `--startup-view`（`tests/scene_lifecycle_tests.cpp`）：只运行 `InitDefaults` 加窗口/D3D 初始化，断言启动面板为相机面板、cameraPos = (0, 10, 0)、距离 -45、三个角度为 0、视角 30 度、两个视图偏移均为 0、启动枢轴是 cameraPos，并断言 `focus[1] + viewOffsetY == 10`（即地面原点落在 `atan(10/45)`）；随后读取一次世界与视图矩阵，再执行 `ResetAppState` + `ReloadModels`（等价于切一帧）后重读，要求两组矩阵逐元素一致。命令：`build\tests\Release\scene_lifecycle_tests.exe "D:\MMD-Next\tests\极星动力LIVE舞台 Ray用一键渲染\UserFile\Stage.pmm" 1 --startup-view`。
- 判别力已验证：临时还原两处源码后，该模式以 exit 7 失败于 `FAIL: startup view offsets are cleared`；修正后输出 `PASS startup viewpoint matches the reference` 与 `PASS a frame step leaves the startup view unchanged`。
- `ctest -C Release`：22/22 通过。在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上，`--startup-view`、`--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection` 及默认编辑器路径均以 exit 0 结束。
- 未覆盖：仍需用户在桌面上复测初启动视角。本轮构建时应用本体正在运行，`MikuDanceStudio` 目标因 `LNK1104` 未能重新链接，需先关闭该进程，再执行一次 `cmake --build build --config Release`。模型面板的自由视角枢轴仍采用移植侧的选择（取视图偏移），本次未作改动。

<a id="record-user-test-fixes-2026-09-23续八相机与物体面板互切跳变自由视角枢轴"></a>
### （续八）：相机与物体面板互切跳变、自由视角枢轴

用户报告：关闭追踪（默认）时，在模型之间切换，视图与光照均不变化，播放时亦然；但一切换到相机会立即跳到相机视角；开启追踪时，从任何物体的视图播放都会跳到相机视角；两种模式下均存在「相机切到物体」的视图跳变。上一节末尾所述「自由视角枢轴仍取视图偏移」在本轮已修改。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 相机与物体面板互切时画面跳变，纵深相差一个相机距离（等效缩放 2 倍） | 两个面板绕不同点旋转世界：相机面板使用 `cameraPos`，模型面板当时使用 `(viewOffsetX, viewOffsetY, 0)`。切换时的「交接」把 `cameraDistance` 直接写入对方的深度字段（`src/window/ui_model_reload.cpp` 中的两处），导致相机面板渲染 `-(cameraPos + eye)`、模型面板渲染 `-(2*viewOffset, cameraDistance)`，两者的 z 恰好相差一个 `cameraDistance`。同一段交接还会把相机关键帧的位置反向改写为自由视角的偏移，仅切换面板便会改动相机数据。 | 枢轴改用原版语义：参照模式为「无」时世界平移为 0（x64 `0x14002797A` 的 `MatrixTranslation(0, 0, 0)`），眼点仍取 `(viewOffsetX, viewOffsetY, cameraDistance)`（x64 `0x140027FB0..0x140027FDE`，与移植原本一致）。将 `ApplyCameraReferenceModeChange` 中的眼点补偿抽为共用函数 `CarryEyeForFocusChange`：眼点移动 `R(focus_old − focus_new)`，任何枢轴变化都不会移动画面。两处面板切换改调用该函数，只读 `cameraPos`、只写眼点，因此不再改写相机数据。 |
| 模型面板的平移量为拖拽量的两倍 | 眼点与枢轴读取同一对 `viewOffsetX/Y`：平移时世界移动一次、眼点又移动一次，画面位移因此翻倍。 | 枢轴归零后平移仅改动眼点，画面位移等于拖拽量，与原版一致。`coordinate_axis_tests` 断言该位移恰等于拖拽量。 |

验证与范围：

- `coordinate_axis_tests` 断言：模型面板的世界平移为 0；平移的画面位移等于拖拽量；在偏航 0、0.7、π 三种取向下，切换面板前后的「世界 × 视图」合成矩阵逐元素一致（容差 1e-3）；相机位置在切换前后不变；往返一次后眼点回到原值；关闭追踪时播放不改变自由视角，打开追踪时播放切到相机视角。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 的 `--startup-view`、`--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection` 与默认编辑器路径在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上均 exit 0；Release 全量重建 0 error（含应用本体与 22 个测试目标）。
- 未覆盖：切换手感与平移速度仍需用户复测（模型面板的平移本次已减半至与原版一致）。追踪模式下两个面板本就走同一条相机分支（枢轴均为 `cameraPos`），按代码不应出现面板切换跳变；若在该模式下仍能观察到跳变，请提供具体的动作序列。

<a id="record-user-test-fixes-2026-09-23续九追踪模式下切到物体面板跳变停止播放后视角回退"></a>
### （续九）：追踪模式下切到物体面板跳变、停止播放后视角回退

用户报告了两项问题：其一，追踪模式下从相机面板切到物体（模型）面板时画面向上跳变；其二，无论是否开启追踪，停止播放后视角都不应回到播放前的状态（以 MMD 的行为为准）。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 追踪模式下相机切到物体面板时画面向上跳变 | 整个切换过程中共有三处会改写眼点：`ReloadModels`（相机关键帧求值）将 `viewOffsetX/Y` 清零，`PostModelReload`（追踪重锚）按相机父骨骼重算眼点，以及面板切换自身的枢轴交接。上一轮只把枢轴交接做到了精确，前两处仍会把眼点移往别处，因此在追踪模式下表现为一次向上的位移。 | 两个面板切换改为「整体保持画面」：进入时记下眼点与当前枢轴，完成所有重载后先还原眼点、再进行枢轴交接（`CarryEyeForFocusChange`）。这样，中间的 `ReloadModels` 与 `PostModelReload` 无论怎样改写眼点都不会移动画面。 |
| 停止播放后视角回到播放前状态 | `StopPlayback` 的两条分支（相机面板、追踪）都会将 `viewOffsetX/Y` 清零；这两个字段正是渲染眼点的横纵坐标（眼点 = `(viewOffsetX, viewOffsetY, cameraDistance)`），PMM 文件也会保存它们。清零即把画面移回未偏移的位置，正是用户看到的「回退」。同样的清零还出现在 `ReloadModels` 的三个分支与 `PlaybackPoseAdvance` 的相机轨步骤（原版地址 `0x7FF7CB479CFC`、`0x7FF7CB479EF9`、`0x7FF7CB47A537`）。 | 六处清零全部不再写眼点：`ReloadModels` 的三个分支、`PlaybackPoseAdvance` 的相机轨步骤、`StopPlayback` 的两条分支。停止播放后因而停在最后一帧的视角上，切帧也不再导致模型面板的取景回跳。 |

验证与范围：

- `scene_lifecycle_tests` 的 `--startup-view` 新增断言：将两个偏移分别设为 3 与 12 后调用 `ReloadModels`，二者必须原样保留（输出 `PASS a camera-key evaluation keeps the view offsets`）。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 的 `--startup-view`、`--playback-view`、`--view-tools`、`--bone-movement`、`--physics-selection` 与默认编辑器路径在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上均 exit 0。
- 应用本体本轮未重新链接：构建时运行中的实例占用了 `build/Release/MikuMikuDance.exe`（`LNK1104`）；关闭该进程后执行一次 `cmake --build build --config Release` 即可。
- 未覆盖：这两项仍需用户复测。清零是原版的写法（地址已注明于源码）；移植侧保留眼点则属于**有意偏离**：眼点在移植中承担取景职责，且 PMM 会保存它。若日后需要逐字忠于原版，必须先为取景另找一处存储。

<a id="record-user-test-fixes-2026-09-23续十关闭追踪时视图跳变"></a>
### （续十）：关闭「追踪」时视图跳变

用户报告：由追踪模式切换到非追踪模式（即取消勾选「追踪」）时画面跳变；按 MMD 的行为，画面应保持不变。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 取消「追踪」时画面跳变 | 追踪开关决定帧绕哪个点旋转世界：开启时是 `cameraPos`，关闭后（模型面板）是世界原点——`ResolveFrameFocus` 的 `cameraView = tracking \|\| CameraMode != 0` 分支。该开关只改变了枢轴，却未相应搬动眼点，画面因此整体平移 `R(cameraPos)`；勾选方向的旧代码还会顺手将 `viewOffsetX/Y` 清零，同样会改变画面。 | `CmdControl500` 的 535 分支改为：进入时记录眼点与当前枢轴，照原样执行完刷新链，末尾先还原眼点，再调用 `CarryEyeForFocusChange` 交接枢轴，使两个方向均保持画面不动；勾选方向不再清零眼点（与眼点归属契约一致）。 |

验证与范围：

- `coordinate_axis_tests` 新增断言：在模型面板、追踪开启的状态下直接派发 535（测试窗口没有该复选框，`BM_GETCHECK` 读作 0，因此走「取消勾选」分支），断言开关确实关闭，且派发前后的「世界 × 视图」合成矩阵逐元素一致。
- `ctest -C Release` 22/22 通过；`scene_lifecycle_tests` 六条入口在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上均 exit 0。
- 未覆盖：开关两个方向的手感仍需用户复测。应用本体仍需在关闭运行实例后重新链接（`LNK1104`）。

<a id="record-user-test-fixes-2026-09-24续十一用移动与旋转工具时视图漂移"></a>
### （续十一）：用移动与旋转工具时视图漂移

用户报告：在「视图」坐标系下使用移动与旋转工具时，视图仍会漂移。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 拖动骨骼（移动/旋转工具）时画面持续漂移 | 渲染眼点为 `(viewOffsetX, viewOffsetY, cameraDistance)`，任何将 `viewOffsetX/Y` 清零的路径都会搬动画面。上一轮只处理了相机关键帧求值与停止播放两处，交互路径上仍有五处残留：`ui_mouse_misc.cpp` 的追踪分支在**每次鼠标移动**时清零（拖动时即表现为持续漂移）；`frame_driver.cpp` 的延迟追踪重载在清零之外，还把属于 cameraPosY 的 10 写入 `viewOffsetY`（与 `116e3aa` 修复的启动缺陷系同一处误读）；`ui_editor_click.cpp` 与 `ui_frame_step.cpp` 的追踪分支；以及 `ui_model_reload.cpp` 的「追踪中切换模型」分支。 | 五处均不再写眼点，只保留各自的重载链；`command_file_menu.cpp` 的追踪菜单项（247）是复选框 535 的孪生入口，除同样去掉清零之外，还补上了双向的枢轴交接（`CarryEyeForFocusChange`），使菜单与复选框行为一致。 |

验证与范围：

- 应用与全部 22 个测试目标重建 0 error；`ctest -C Release` 22/22；`scene_lifecycle_tests` 的六条入口在 `tests/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm` 上均 exit 0。
- 应用本体已由本机重新链接（12:20:27），无需用户再次构建。
- 未覆盖：仍需用户复测「视图」坐标系下的移动与旋转工具。若仍有漂移，请设置 `MIKUDANCESTUDIO_TRACE_FILE` 后复现一次，`view` 行会指出具体是哪一项在变化。

<a id="record-user-test-fixes-2026-09-26续十二视口操作按钮改浅色配件态改用-x-标记"></a>
### （续十二）：视口操作按钮改浅色，配件态改用 .x 标记

用户要求：把视口右下角六格操作按钮的深灰色填充改成白色，摄像机图标（按钮左下角那块浅灰方块）颜色不变；再把摄像机/配件面板「调整配件」模式里的那个图标改成 `.x`，沿用摄像机图标的绘制风格；`.x` 之外的底色不要挡住原始轴向标识。

| 项 | 做法 |
| --- | --- |
| 重上色 | `res/assets/hud_sprites.png` 的六行操作按钮（y 31/61/91/151/181/211）中，饱和度低且亮度落在 60..205 的灰色像素改为白色。相机/配件面板那两行按钮左下角的浅灰方块（其填充区向外膨胀 3 像素的范围）保持原样，彩色字母与黑色描边也不动。 |
| 新增素材 | 图集下方空白区新增两行按钮：y 341..370（旋转行）与 y 371..400（移动行），内容与摄像机行一致，但左下角的摄像机方块换成 `.x`——一个点加一个叉，按单元格像素对齐绘制。`.x` 的每个颜色都取自摄像机方块自身的像素：面 `180,180,180`、右下深灰边 `119,119,119`、描边 `0,0,0`（alpha 249）、落到透明像素上的投影 `0,0,0`（alpha 128）。字形是 11×11 的叉（3 像素笔画）加 3×3 的点，点与叉之间留 2 像素，整体占位与原来的摄像机方块相当。 |
| 遮挡范围 | 新行里删掉的只有摄像机方块自身（其填充区向外膨胀 2 像素的范围），方块以外的轴向箭头保持可见。 |
| 取样 | `src/render/sprite_overlay.cpp` 的摄像机/配件面板分支改为按 `EffectiveCoordinateState(app)` 取行：状态 0/1（局部/全局）仍取 v 0.412109..0.46875 与 0.177734..0.234375，状态 2（配件）取 v 0.666015625..0.72265625 与 0.724609375..0.78125。 |

有意偏离：原版在配件态同样画摄像机方块，原版图集里也没有配件标记可供取样，因此这条按「有意偏离」写进代码注释。

资源重生成：`res/gen_resources.res` 与 `res/gen_resources_x64.res` 只重写了 PNG 102 一条记录（125274 → 116195 字节），其余 75 条记录与提交版逐字节一致。本会话沙箱里 `rc.exe` 无法在 `tempfile.mkdtemp` 建立的目录里创建输出（RC1109），而且 `gen_resources.py gen` 会把 `axis.x` 与两个 `skin_effect_*.fx` 记录从入库的 CRLF 形式改写成工作树的 LF 形式，所以本次改为只替换这一条记录。

验证与范围：

- `coordinate_axis_tests`（CTest 名 `coordinate_axis_regressions`）新增断言：摄像机态取样 maxV = 0.46875、配件态 maxV = 0.78125，且两者不同。这段回读放在测试末尾、所有输入相关的用例之后——它会把工具状态摆到另一套值上，插在按键/选择条用例之前会让「the operation button recentres the drag origin」失败。本机输出 `PASS axis.x colours, caption centring and outline`，exit 0。
- `ctest --test-dir build -C Release`：29 项通过 28 项；唯一失败项 `model_alpha_occlusion_regressions` 是既有基线（dbf189f 夹具的 17 像素差，见 `docs/records/rendering/transparency.md`），与本次改动无关。
- `scene_lifecycle_tests.exe "<tests\极星动力LIVE舞台 Ray用一键渲染\UserFile\Stage.pmm>" 1 --view-tools` exit 0（隐藏窗口、11 模型 / 15 配件、12 帧物理）。
- 应用本体的重新链接：图集改动提交时 `build\Release\MikuMikuDance.exe` 正被运行中的实例占用（`LNK1104`），需要在关闭该实例后重新构建 `MikuDanceStudio` 目标。
- 未覆盖：`.x` 标记在实际界面里的观感需要用户复测。

还原方法：`git checkout -- res/assets/hud_sprites.png res/gen_resources.res res/gen_resources_x64.res src/render/sprite_overlay.cpp tests/coordinate_axis_tests.cpp docs/records/development/user-reports.md`。

<a id="record-user-test-fixes-2026-09-26续十三快速双击操作按钮导致视角跳变"></a>
### （续十三）：快速双击操作按钮导致视角跳变

用户报告：快速双击视口右下角六格操作按钮会让视角跳变。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 快速双击六格操作按钮后画面跳一下 | 原版只由鼠标移动处理器写存储位置（x64 `0x381A4..0x3821D`）：`WM_LBUTTONDOWN` 只做 `SetCapture`（`0xA4B38`），`WM_LBUTTONDBLCLK` 只调双击处理器（`0xA4A86`）；而且那次写入紧跟一次光标回绕时会把上一个位置一起搬（`0x381CF..0x3821D`，并置 `mouseJumped`），所以回绕永远不会变成拖拽增量。移植版为修「点骨骼列表选不中」在这两个消息里补写 lParam 坐标；而六格按钮的按下会把指针回绕到视口中心，并把当前与上一个位置都设成该中心，此时补写按钮坐标会让上一个位置仍停在中心，下一帧的相机平移就按「按钮到视口中心」的整段距离走一次。 | 两处补写合并成 `LatchPressPosition`（`src/window/wndproc.cpp`）：居中拖拽已接管本次按下时（`ViewToolDragOperation` 落在 9..20 且左键状态非 0）保持指针位置不动，其余情况照旧补写坐标（含 0xEA60 回绕）。这是按原版「存储位置只属于指针」的不变量收敛，不是对双击做特判。 |

验证与范围：

- `coordinate_axis_tests` 新增断言：按钮按下后拖拽原点落在视口中心；再派发 `WM_LBUTTONDOWN` 与 `WM_LBUTTONDBLCLK`（都带按钮坐标）后，当前与上一个鼠标位置仍是该中心。修复前后对照：撤掉修复时该测试在「a repeated press keeps the centred drag origin」失败，加回后 exit 0；另一段断言保证没有居中拖拽接管时按下消息仍会补写自己的坐标（保住「点骨骼列表选不中」的修复）。
- `ctest --test-dir build -C Release`：29 项通过 28 项；唯一失败项 `model_alpha_occlusion_regressions` 是既有基线（dbf189f 夹具的 17 像素差，见 `docs/records/rendering/transparency.md`），与本次改动无关。
- `scene_lifecycle_tests.exe "<tests\极星动力LIVE舞台 Ray用一键渲染\UserFile\Stage.pmm>" 1 --view-tools` exit 0。
- 未覆盖：真实鼠标快速双击的手感需要用户复测；原版在拖拽期间还会在指针离开视口时持续回绕（`0x323DA`、`0x326B4`、`0x32761`、`0x36BB6`，四处都以 `mouseJumped == 0` 为门槛），移植版只在按下时回绕一次，属既有差异，本次未改。

还原方法：`git checkout -- src/window/wndproc.cpp docs/records/development/user-reports.md`。

<a id="record-user-test-fixes-2026-09-26续十四拖拽操作按钮时行程不再受窗口边界限制"></a>
### （续十四）：拖拽操作按钮时行程不再受窗口边界限制

用户报告：原版 MMD 在调整坐标与方向时可以一直往上或往下拖，移植版把指针拖出视口后就到顶了。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 把指针拖出视口后拖不动了 | 原版在每个拖拽体里、每帧应用完本帧增量之后，都会检查存储的鼠标位置是否越出视口矩形（x64 `0x323DA`、`0x326B4`、`0x32761` 属于平移与旋转体，`0x36BB6` 的门槛写作「mode 4..6 或 mode >= 10」，正好等于移植版的操作 9..20），越界就把该轴的位置设成该轴两端的中点（`0xA1DEC`/`0xA1DF4` 是 X 的两端、`0xA1DF0`/`0xA1DF8` 是 Y 的两端，四者都由视口布局写入），再用 `ClientToScreen` + `SetCursorPos` 把光标挪回中间，因此行程不限；这四个点都以 `mouseJumped == 0` 为门槛。移植版只在按下时回绕一次，指针离开视口后虽然增量还在累积，光标却已经出不了窗口边界，用户因此感觉「到顶了」。 | 新增 `RecentreCenteredDragCursor`（`src/app/frame_modes.cpp`）：拖拽由六格按钮接管、左键仍按住且 `mouseJumped == 0` 时，把越界的那一轴设成该轴中点并同时写进当前与上一个位置，然后只做 `ClientToScreen` + `SetCursorPos`——不置 `separateWindowMouseSeen`，否则回绕之后的第一段移动会被当成跳变吞掉（原版这里标的是 `0xA16C0`/`0xA16C2`，不是移动处理器读的 `0xA0088`）。`src/app/frame_driver.cpp` 在每帧拖拽分发之后调用它，与原版「先应用增量、再回中」的顺序一致。 |

验证与范围：

- `coordinate_axis_tests` 新增断言：拖拽进行中把 `MouseY` 放到视口下方 40 像素后回中，Y 与上一个位置都变成视口纵向中点、X 保持不动；`mouseJumped = 1` 时不回中；拖拽结束后也不回中。
- `ctest --test-dir build -C Release`：29 项通过 28 项；唯一失败项 `model_alpha_occlusion_regressions` 是既有基线（dbf189f 夹具的 17 像素差），与本次改动无关。
- `scene_lifecycle_tests.exe "<tests\极星动力LIVE舞台 Ray用一键渲染\UserFile\Stage.pmm>" 1 --view-tools` exit 0。
- 全部测试目标与应用目标重建 0 error，应用已重新链接（`build\Release\MikuMikuDance.exe`）。
- 未覆盖：拖拽手感需要用户复测；移植版仍在按下时把指针回绕到视口中心并隐藏光标（原版是拖出边界后才回中），这条差异保留。

还原方法：`git checkout -- src/app/frame_modes.cpp src/app/frame_driver.cpp include/mikudancestudio/ported_funcs.hpp tests/coordinate_axis_tests.cpp docs/records/development/user-reports.md`。

<a id="record-user-test-fixes-2026-09-26续十五配件态的-x-标记去掉黑色描边与残留阴影"></a>
### （续十五）：配件态的 .x 标记去掉黑色描边与残留阴影

用户报告：配件坐标态左下角的 `.x` 标记黑色阴影描边过浓，标记左上角还有一块错误的阴影。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 标记发黑 | 图集 y 341..370 与 y 371..400 两行里，`.x` 用 `(0,0,0,249)` 勾了 2 像素黑描边并带 `(0,0,0,128)` 投影，两行各有 169 与 168 个纯黑像素；同一批按钮的相机行（摄像机方块）只有 56 与 45 个。按钮格子只有 40×30 像素，黑边占掉字形近一半宽度。 | `tools/make_accessory_hud_rows.py` 重新生成这两行。每行先从同类相机行整行复制（旋转行 y 211..240 → y 341..370，移动行 y 91..120 → y 371..400，与渲染端取样值 `v` 一致），再画新标记。标记只用摄像机方块自身的颜色：面 `(180,180,180)`、边 `(119,119,119)`，3×3 点加 11×11 叉（笔画 2 像素），没有黑描边、没有投影。生成后两行的纯黑像素为 0。 |
| 左上角有暗影 | 旧行只擦掉方块的不透明像素并向外扩 2 像素，方块自身的半透明软边落在擦除范围之外，正好留在标记左上角。 | 擦除范围改成方块的全部像素：从方块灰面泛洪扩散到描边、白色镜头细节和半透明软边，再按行在自身两端之间填满，不会越进右侧字母。该掩码即擦除集合。 |
| 重画会切坏按钮 | 直接擦一个矩形会切掉圆环与字母下缘；把标记画到方块范围之外也会盖住字母像素。 | 标记只写在「方块原本占的像素」或「原本透明」的位置。相机态与配件态在方块以外的像素因此逐像素一致（3 个格子 × 2 对行，0 处不同）。 |

验证与范围：

- 图集逐像素核对：配件行相对相机行，在「方块未覆盖且原本不透明」的像素上 0 处不同；配件行的纯黑像素从 169/168 降到 0。
- 渲染目标对照（新增的、默认关闭的捕获分支）：`MIKUDANCESTUDIO_HUD_PREVIEW_DIR=<目录> scene_lifecycle_tests.exe "<tests\极星动力LIVE舞台 Ray用一键渲染\UserFile\Stage.pmm>" 1 --view-tools` 在相机态与配件态各写一张 PNG。对照图 `.local/artifacts/hud-rows/accessory-compare2.png`：配件态前后有 1651 个像素不同，都在标记区域；相机态前后有 195 个像素不同、最大通道差 17，那是场景两次渲染自身的波动，不是图集改动。
- `coordinate_axis_tests` exit 0，它断言配件态取 `v` 0.666016..0.722656 的两行。
- `ctest --test-dir build -C Release`：29 项通过 28 项。唯一失败项 `model_alpha_occlusion_regressions` 是既有基线（dbf189f 夹具的 17 像素差），与本次改动无关。
- 应用与测试目标重建 0 error，`build\Release\MikuMikuDance.exe` 已重新链接。
- 图集嵌入：`res/gen_resources.res` 与 `res/gen_resources_x64.res` 的 PNG 102 记录替换成新图集（116398 字节），另外 76 条记录的载荷与全部记录头逐字节不变。`scripts/gen_resources.py` 在本机沙箱里让 rc.exe 写出 .res 会得到 RC1109，因此嵌入改由 `tools/make_accessory_hud_rows.py embed` 直接改写这一条记录；该读写路径经往返核对，与提交版逐字节相同。
- 未覆盖：标记在用户屏幕与 HUD 缩放下的观感需要复测。标记现在有 66 个面像素与 36 个边像素，比原来的黑描边轻很多；要更明显时调 `tools/make_accessory_hud_rows.py` 的 `CROSS_STROKE`（笔画宽度）、`EDGE`（边色），或放大 `mark_shape()` 里的 `CROSS`。

还原方法：`git checkout -- res/assets/hud_sprites.png res/gen_resources.res res/gen_resources_x64.res docs/records/development/user-reports.md`。`tools/make_accessory_hud_rows.py` 可以保留，它按同一份输入能重现这两行。

<a id="record-user-test-fixes-2026-09-30续十六表示半透明显示的模型出现穿透边缘"></a>
### （续十六）：「表示→半透明显示」的模型出现穿透边缘

用户报告：把模型切到「表示→半透明显示」（`WM_COMMAND` 0xD6）后，模型边缘会透出本该被挡住的像素。

| 现象 | 根因 | 处理与证据 |
| --- | --- | --- |
| 半透明显示的模型边缘透出后面的像素 | 原版 x64 的半透明显示（`displayState != 0`）走原始记录顺序的单趟材质遍历（`0x1400B6D70`）：不排序、不拆不透明与半透明两趟，可见性只由半透明混合与深度测试决定。移植版把 displayState 模型照常收进透明排序趟，拆趟与排序改变了材质的提交顺序，边缘处的半透明像素因此透出后面的像素。 | `RenderNativeOrderedAlpha` 与 `RenderSplitOrderedAlphaImpl`（`src/render/model_renderers.cpp`）遇到 `displayState != 0` 的模型不再收集其材质，避免同一模型被画第二次。 |
| 同一模型在两趟里各画一次 | `RenderModelsFixed` 与 `RenderModelsEffect` 原先在不透明与半透明两个迭代里都提交 displayState 模型，与原版的单趟语义不符。 | 这两个入口让 displayState 模型只在不透明迭代里以 `ModelAlphaPass::Original` 画一次、跳过半透明迭代，效果路径的 toon 与否分岔保持不变。 |

验证与范围：

- `model_alpha_occlusion_tests` 新增 displayState 夹具（提交 `a8ca6de`）：一个 PMX 模型放两个全屏四边形，近处黑色、远处贴纹理，原始 `Diffuse.a` 都是 1.0；按 0xD6 处理器的双写语义同时设 `displayState = 1` 与 `characterTransparentMode = 1`（后者喂效果路径着色器的 `transp`）。期望像素只来自近四边形对白底的 0.5 混合（黑色排布 `(128,128,128)`、纹理排布 `(144,224,240)`）；拆趟或排序会把远四边形也画进画面，采样颜色必然不同。5 档透明排序精度 × 2 种排布 × 9 像素 = 252 个采样。
- 修复前后同条件对照（`D:\MMD-Next\.tmp-displaystate\`，同一次 `model_alpha_occlusion_regressions` 运行）：`run-before.log` 329 个失败采样 → `run-after.log` 77 个，displayState 段 252 个采样全部通过。余下 77 个是既有失败（17 个文档基线 + 60 个 `9a2e28c` 默认关排序的遗留），本次未改其期望。
- `ctest --test-dir build -C Release`：69 项通过 68 项；唯一失败项 `model_alpha_occlusion_regressions` 即上述 77 个既有采样。
- 顺带修复 `frame_rate_correction_tests` 的构建（提交 `0454280`，`CMakeLists.txt`）：`mmv_host.hpp` 引入 `mmv/mmv.h` 之后该目标缺 MMV 头文件目录、ctest 报 Not Run；补上 `../MMV/include` 后该目标构建并通过。
- 应用重建 0 error，`build\Release\MikuMikuDance.exe` 的 SHA-256 为 `0AA4FE0187395724868A0438F50136ABF5282F61E5336F296E2600F533816AE6`。
- 未覆盖：穿透边缘的观感需要用户复测，建议用 `tests\洛希极限\洛天依洛希极限_横向关节改名.pmx` 在「表示→半透明显示」下检查边缘。

还原方法：`git checkout -- src/render/model_renderers.cpp tests/model_alpha_occlusion_tests.cpp CMakeLists.txt docs/records/development/user-reports.md`。
