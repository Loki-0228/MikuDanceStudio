# 时间轴隐藏表情双击崩溃

本案例按用户报告登记为原版 MMD 缺陷，其修复属于 MMS 相对原版的新增特性。2026-09-27 已用用户提供的 `☆Rou.pmx` 完成 MMS 后台运行验收。`main` 中已有的范围保护满足下述目标行为；本次补充真实窗口消息回归测试、构建入口与验收记录，未改变生产逻辑。

## 用户报告

原版 MMD 有眉、目、口、其他四个表情操作栏。用户通过 PmxEditor（PE）的特殊设置将某些表情隐藏后，这些表情不出现在四个操作栏，只显示在时间轴左侧的「表情」表示枠列表。双击该列表中的隐藏表情会使原版 MMD 崩溃闪退。

用户提供的复现场景如下。

1. 加载 `D:/MMD-Next/tests/MMD ☆★Rou/☆Rou.pmx`。
2. 在时间轴左侧的表示枠列表中展开「表情」。
3. 双击截图标出的 `潰れ1`。

用户报告的结果为崩溃闪退。本轮未在原版 MMD 中重放该操作，报告中的原版版本号尚未记录。

用户将隐藏位置描述为「第 5/0 个」。本例文件中 `潰れ1` 的实际面板值为 `0`，不能据此认定文件值 `5` 是标准面板类别。

## 样本核对

本轮用仓库内的 [pmxlib.py](../tools/pmxlib.py) 只读解析模型，未修改原文件。

- 文件大小为 14210002 字节。
- SHA256 为 `392f7a5697bc7c7f511ed256bf15d3a138d6da6d251e4fa666f95300ae16883e`。
- 表情总数为 70。`panel=0` 有 5 个，`panel=1` 有 9 个，`panel=2` 有 22 个，`panel=3` 有 26 个，`panel=4` 有 8 个。
- 下表五个隐藏表情的 `panel` 均为 `0`，均被时间轴左侧索引为 1、名称为「表情」、`special=1` 的表示枠引用。

表中索引均为零基，引用方均为组合表情。

| 表情索引 | 名称 | 变形类型 | 偏移数 | 组合表情引用方 |
| --- | --- | --- | --- | --- |
| 17 | 潰れ1 | 顶点（1） | 578 | 16：潰れ |
| 18 | 潰れ2 | 材质（8） | 2 | 16：潰れ |
| 20 | 暗目2 | UV（3） | 158 | 19：暗目 |
| 24 | HL1_R | UV（3） | 158 | 22：HL_R |
| 25 | HL2_R | 材质（8） | 4 | 22：HL_R |

样本没有 `panel=5` 的表情。隐藏表情仍被组合引用，因此不能通过删除隐藏表情来规避崩溃。

## 面板分类与表示枠

本地参考 [PMX 文件格式解析](../../refs/PMX文件格式解析.md) 的「变形数据」节列出四个操作面板的位置 `1..4`，「表示枠」节则描述独立的骨骼与表情索引列表。该参考文件位于仓库外。

MMS 的 [MorphPanel](../include/mikudancestudio/subrecord_layout.hpp) 枚举定义 `system=0`、`eyebrow=1`、`eye=2`、`mouth=3`、`other=4`。[RefreshMorphGroup](../src/model/post_load_init.cpp) 按面板值筛选下拉框条目。因此，`panel=0` 不出现在眉、目、口、其他四个操作栏，本例仍显示在时间轴左侧的「表情」表示枠。该面板值也不影响表示枠引用与表情求值数据。

## 已有保护与证据范围

本次运行验收的生产源码基线为 `4e735db85f2e507c7953bddb9ff4c97ba289faf7`（本地 `main`）。

[ui_editor_click.cpp](../src/window/ui_editor_click.cpp) 中，`HandleLButtonDown_NameColumnHit` 的表情行分支先按面板选择控件，再计算 `panelIndex = static_cast<unsigned>(panel) - 1`。当 `panelIndex >= 4` 时，程序刷新后返回，跳过 `selectedMorphs[panelIndex]` 访问与后续控件消息。`panel=0` 会命中这个范围保护。该分支处理时间轴名称列的选中，范围检查是选中条目后同步四个操作栏控件时的边界保护，不表示隐藏表情被归入或显示在某个操作栏。

`git blame` 显示该保护已存在于建库快照 `ba6b269`，不是本轮新增的代码。源码中关于原版沿用旧栈句柄的注释属于历史分析，本轮没有独立反汇编原版来确认崩溃根因。

双击消息由 [wndproc.cpp](../src/window/wndproc.cpp) 的 `WM_LBUTTONDBLCLK` 分支处理，再进入 [ui_mouse_misc.cpp](../src/window/ui_mouse_misc.cpp) 的 `HandleLButtonDblClk`。本次测试发送真实的 `WM_LBUTTONDOWN → WM_LBUTTONUP → WM_LBUTTONDBLCLK → WM_LBUTTONUP`，分别检查第一次单击和完整双击后的状态，覆盖这条完整消息路径。

## MMS 目标行为

- 隐藏表情出现在时间轴左侧「表情」表示枠中时，单击、双击均安全处理，不越界访问面板状态，也不使程序闪退。
- 保留隐藏表情数据及组合、动作对它的驱动。隐藏属性不改变表情求值。
- 保持四个普通面板的选择、权重显示、滑块和关键帧操作正常。
- 不改写用户模型，不把隐藏表情强行改归四个普通面板。隐藏表情只显示在时间轴左侧「表情」表示枠，不出现在眉、目、口、其他四个操作栏。

## 运行验收（2026-09-27）

新增 [hidden_morph_tests.cpp](../tests/hidden_morph_tests.cpp)，通过真实 `InitMainWindowAndD3D`、PMX 加载器和主窗口消息入口验收。窗口使用 `SW_HIDE`，钩子禁止激活并取消意外模态框；初始化或加载出现模态框视为失败。测试不占用桌面，不保存模型。

| 范围 | 实际检查 | 结果 |
| --- | --- | --- |
| 五个 `panel=0` 表情 | 展开时间轴「表情」表示枠，检查行表保留；分别单击、双击、Shift 切换，另重复点击 `潰れ1` | 通过；时间轴选择标记按预期变化，四栏条目、选中项、文本、滑块及全部表情权重保持不变 |
| 四个普通栏 | 各选一个普通表情，检查名称、数值与滑块同步；将滑块改为 0.65，按登记按钮后跳帧回读 | 通过；四栏登记均可回读，普通操作保留隐藏权重 |
| 组合求值 | `潰れ`、`暗目`、`HL_R` 驱动五个隐藏表情，逐通道对比模型偏移表计算的预期顶点、UV、材质结果 | 通过；结果确实发生变化，且与预期一致 |
| 动画求值 | 为组合登记第 40、60 帧关键帧，用生产回放游标推进至第 50 帧，再核对插值权重和顶点、UV、材质结果 | 通过；隐藏表情自身权重为零时，组合动画仍正常驱动它 |
| 未知面板边界 | 将一个普通表情在内存中依次改为 `0`、`5`、`255`，重建四栏后重复点击 | 通过；不归入四栏，不破坏面板状态；未生成或改写 PMX |

`--mode contract` 共 **358 项断言、0 失败**；另起进程运行 `--mode bounds`，共 **88 项断言、0 失败**。`panel=5/255` 的结果只说明这些未知值的边界安全，不赋予它们新的标准分类语义。

相关已有回归 `window_interaction_tests`、`morph_import_regressions`、`morph_runtime_tests`、`panel_key_tests` 在集成前、后均 **4/4 通过**。正式 Release 程序构建成功。原始日志、运行文件、源码快照和清单保留于 `.local/artifacts/hidden-morph-20260927/`，目录不入库：

- `before/manifest.json`、`baseline-CMakeCache.txt`：起始版本、干净工作树、配置与哈希；`before/` 保留运行文件和生产库。
- `baseline-contract-03.log`、`baseline-bounds-03.log`：最终专项结果；`before/test-runtime/` 为只含测试程序及 Data 的隔离运行目录，不加载外部 MME 插件。
- `baseline-regressions.log`、`after-regressions.log`、`after-build.log`：前后相关回归与最终构建结果。
- `after/manifest.json`：集成后的配置、程序与测试哈希；同目录保留最终测试源码、程序和生产库。

构建配置为 VS 2022 / v143 / x64 / Release，`/O2 /Ob2 /DNDEBUG`，`DIAG=OFF`、`PERF=OFF`。生产 EXE 与核心库在本次测试集成前后完全相同：

| 文件 | SHA256 |
| --- | --- |
| `MikuMikuDance.exe` | `ac6d29c3177537ac9e708bfa8384e866cb794cae3e1af014011e2da5b401dc1f` |
| `mikudancestudio_core.lib` | `a5a16e002dc30d4b72c5f09a415352d93e047809370dcd38065e20e57c106c41` |
| `hidden_morph_tests.exe` | `60b70f2dacba9c0c837d5e7ea5cf5c7576ec084c5b2754de8bd13758820986b5` |

原始 PMX 在测试后仍符合上文 SHA256。本次没有新的生产修复或性能改动，因此没有“修复前失败、修复后通过”的两版差异，也未进行渲染性能采样或作 FPS 声明。动画结果以生产回放求值、GPU 输入数据和材质数据验证，未做外部 MME 效果或画面验收。

早期失败日志一并保留：`baseline-build.log` 使用了错误的构建目标名；`baseline-contract-01.log` 是测试滚动定位错误；`baseline-contract-02.log` 是测试将帧数传入以秒为单位的回放游标。修正测试后通过，不能将这些失败记作生产缺陷。

## 复跑专项测试

该测试需要单独提供上述 Rou 样本，不包含模型资源，不加入默认 CTest 集。启用 `MIKUDANCESTUDIO_BUILD_UI_TESTS` 后显式构建，再从有正式 `Data` 文件夹、无外部 MME DLL 的运行目录执行：

```powershell
cmake --build build --config Release --target hidden_morph_tests
# 把生成的 build/tests/Release/hidden_morph_tests.exe 及正式 Data 复制到隔离运行目录后：
.\hidden_morph_tests.exe --model 'D:/MMD-Next/tests/MMD ☆★Rou/☆Rou.pmx' --mode contract
.\hidden_morph_tests.exe --model 'D:/MMD-Next/tests/MMD ☆★Rou/☆Rou.pmx' --mode bounds
```

退出码 0 表示该模式全部断言通过；缺失素材、初始化失败、断言失败均返回非 0。测试需要可用的本机 D3D9 环境。

## 尚未核实的原版证据

尚未固定原版 MMD 的版本和 EXE 哈希，也未独立重现原版崩溃或确认其具体根因。原版缺陷的来源仍是用户报告；这与 MMS 已通过的运行验收分开记录。

## 复核入口

分类规则见[近期新增特性](ADDITIONAL_FEATURES.md)。在仓库根目录可核对保护的历史来源：

```powershell
git blame -L 757,770 --date=short -- src/window/ui_editor_click.cpp
```
