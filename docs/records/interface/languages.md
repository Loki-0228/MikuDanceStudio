# 语言、名称与界面编码记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [Model name translation](#record-model-name-translation)
- [Unicode model names in the Japanese locale](#record-ui-japanese-locale-names)
- [日区名称空白的边界修复](#record-japanese-name-gaps)
- [附件错误提示编码修复](#record-accessory-message-encoding)
- [物理模型编辑页编码修复验收](#record-physics-editor-encoding)
- [菜单语言混杂修复](#record-menu-language-fix)
- [新增界面日英审查（2026-10-14）](#record-ja-en-ui-review)

<a id="record-model-name-translation"></a>
## Model name translation

原始记录：`MODEL_NAME_TRANSLATION.md`。其中的版本、日期和验证范围沿用原记录。

The Help menu shows the model-language preference as
`Current model default language: Auto`. Clicking it cycles through Auto,
Japanese, and English. The next item, `Model language auto-translation`, toggles
dictionary translation for displayed bone, morph, and recognized display-group
names. It defaults to off.

| Preference | Translation target |
| --- | --- |
| Auto with English UI | English dictionary column |
| Auto with Japanese UI | Japanese dictionary column |
| Auto with any other UI language, including Chinese | Japanese dictionary column |
| Japanese | Japanese dictionary column |
| English | English dictionary column |

Changing either setting refreshes the main lists and timeline. Names in bone
selection, physics, curves, and channel selection use the same display helpers.
Both settings persist in `Data/model_info_language.ini`; existing files with
only a language value remain valid and start with translation off.

<a id="record-model-name-translation-dictionary-and-name-identity"></a>
### Dictionary and name identity

The built-in tables come from the supplied references in `D:/MMD-Next/refs/`:

- `骨骼转英文.txt`: 446 entries in `src/main/bone_name_dictionary.inc`.
- `表情命名 日转英参考字典.txt`: 410 entries in
  `src/main/morph_name_dictionary.inc`.

Each table retains source order, Japanese and English spellings, and optional
third-column aliases. The first entry wins when a source repeats a spelling.
Exact Japanese names take precedence over English aliases; English matching
also accepts ASCII case differences. Matches do not remove custom suffixes or
normalize punctuation. If the preferred model name has no match, the alternate
language field is checked. Unmapped names remain as displayed before translation.

Translation changes labels only. It does not overwrite model names, bone or
morph identities, saved files, motion matching, or plugin interfaces. Model
titles have no supplied dictionary and keep their existing language-field
selection and Unicode fallback.

<a id="record-model-name-translation-verification-on-2026-10-10"></a>
### Verification on 2026-10-10

Release x64 tests cover all three UI languages, all three model-language
preferences, translation on/off, English-to-Japanese lookup, supplied spelling
and left/right conventions, duplicate entries, unmapped Unicode names, and
unchanged raw identities. Hidden native combo boxes exercise production bone
and morph list population. Both shipped menu resources are checked for captions,
adjacent placement, enabled state, and check marks. Preference tests use an
isolated temporary directory and cover saving, reloading, old files, and absent
files.

`ui_language_tests` and `menu_language_regressions` pass. A test executable copy
with a Japanese process manifest also passes the complete UI suite and the
earlier PMX-import regressions with `ACP=932`. The host Windows locale is not
changed. See [the original Unicode repair](languages.md#record-ui-japanese-locale-names) for
before/after failure evidence. No rendered-scene FPS or GPU comparison was run;
performance remains unverified.

<a id="record-model-name-translation-retained-local-outputs"></a>
### Retained local outputs

- `.local/artifacts/model-name-translation/01/`: build and test logs,
  dictionary hashes, final source revision, build options, worktree status,
  and executable hashes.
- `.local/worktrees/ui-japanese-names/`: isolated worktree reused for the final
  Unicode repair and translation build.
- `.local/releases/model-name-translation-after/`: final binary snapshot.
- `.local/archives/ui-japanese-names/`: previously installed executable.
- `build/Release/MikuMikuDance.exe`: final installed executable.

Earlier baseline and Unicode-only snapshots remain in the locations recorded
by the original repair. These local outputs are retained, not published or
tagged as a user-accepted release.

<a id="record-ui-japanese-locale-names"></a>
## Unicode model names in the Japanese locale

原始记录：`UI_JAPANESE_LOCALE_NAMES.md`。其中的版本、日期和验证范围沿用原记录。

The Chinese UI could lose imported names when Windows used the Japanese code
page. Some main-panel selectors were ANSI windows. The frame-registration list
read truncated legacy bone and morph names even though PMX retained the original
Unicode strings. PMX display-group names were converted to legacy strings and
their Unicode originals were discarded. Failed conversions left empty entries.

Main-panel combo boxes now use Unicode windows. Registration rows read the
original bone and morph names, with the existing English-name fallback. Imported
display groups retain their Unicode names in an owned side table without changing
the model ABI. Changes from the English-name editor still take precedence. Model
initialization, disposal, and failed imports clear the side table. PMX row counts
now exclude uninitialized records, including the special Root entry that is not
copied into the bone-group array.

<a id="record-ui-japanese-locale-names-verification-on-2026-10-10"></a>
### Verification on 2026-10-10

The baseline is `a475523b2aab82e72839f5c5b574d7733f014273` plus the regression
fixture and a behavior-preserving extraction of panel control creation. The
instrumentation patch, build log, CMake cache, worktree status, and executable
hashes are in `.local/artifacts/ui-japanese-names/01/`. The baseline binaries are
in `.local/releases/ui-japanese-names-before/`.

Both sides use Release x64, Visual Studio 17 2022, the Conan toolchain, and the
same generated PMX fixture. The fixture contains Chinese and Japanese names,
long names, supplementary Unicode characters, and missing English translations.
Tests run in hidden windows and compare timeline pixels from GDI memory surfaces.
They do not capture the desktop or open a scene on the user's desktop.

| Check | Before | After |
| --- | --- | --- |
| PMX registration names with Japanese CRT locale | Fails on morph name | Passes |
| Production panel combo boxes preserve Unicode | Fails | Passes |
| Same checks with process ACP 932 | Fail | Pass |
| Complete UI language suite with process ACP 932 | Fails | Passes |
| UI language and menu regressions with system ACP 936 | Not claimed | Both pass |

The Japanese test copies use a `ja-JP` process manifest, supported by
[Windows application manifests](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests#activeCodePage).
The test log confirms `ACP=932` and `CRT=Japanese_Japan.932`. Windows system
settings are unchanged. The helper and raw logs are under
`.local/artifacts/ui-japanese-names/01/`, including `japanese-before/` and
`japanese-after/`.

The suites also verify three UI languages, language switching, readable timeline
glyphs, complete display-group names, legacy English-name edits, and name-cache
cleanup. No rendered-scene FPS or GPU performance comparison was performed;
this record accepts UI correctness only.

<a id="record-ui-japanese-locale-names-local-outputs"></a>
### Local outputs

- `.local/worktrees/ui-japanese-names/` isolates this repair from unrelated live
  edits. It is also reused for the subsequent
  [model name translation build](languages.md#record-model-name-translation), with the same
  Release options and its final source revision recorded in the artifacts.
- `.local/releases/ui-japanese-names-before/` preserves the failing baseline.
- `.local/releases/ui-japanese-names-after/` preserves the verified repair.
- `.local/archives/ui-japanese-names/` preserves the previously installed EXE.
- `build/Release/MikuMikuDance.exe` is the installed repair.
- `.local/artifacts/ui-japanese-names/01/` retains build and test evidence,
  source revisions, patches, settings, and executable hashes.

The snapshots and worktree are retained for comparison. They are not release
tags and have not been pushed or published.

<a id="record-japanese-name-gaps"></a>
## 日区名称空白的边界修复

原始记录：`JAPANESE_NAME_GAPS_20261011.md`。其中的版本、日期和验证范围沿用原记录。

反馈涉及日区环境、中文界面的表情和显示组，未提供原模型。本次没有复现对方的具体场景；先验证正常 PMX，再修复能独立复现的旧式名称解码空白。

<a id="record-japanese-name-gaps-修复"></a>
### 修复

旧式表情名称的首选字段只要非空，原读取逻辑就不会尝试另一语言。字段含无效 CP932 字节时，严格解码返回空字符串，列表随之显示空白。显示组虽然会尝试另一语言，但两侧均无法严格解码时也会丢失整个名称。

现在先尝试两种语言的有效名称，再处理损坏字段。如果固定长度字段截断了末尾的双字节字符，显示可读前缀和省略号；其他无效编码保留可解码的文字，并以替代字形标记损坏部分。带 UTF-8 标记的字段按标记读取，不依赖系统代码页，也不猜测未标记字段的其他编码。骨骼沿用同一套回退规则。

这些处理只生成显示文本，不改写模型名称字节、动作匹配或插件绑定。没有修改 PMX 原始 Unicode 文本的优先级，也不为原本未命名的条目编造名称。

<a id="record-japanese-name-gaps-验证"></a>
### 验证

模型选择框重建列表后，原来会把“未选中”状态原样恢复，收起时因此没有文字。现在在选中项无效时按当前模型恢复选择，相机和附件绑定列表也按当前绑定恢复；已有有效选择保持不变，并刷新收起状态的文字。列表补齐第 255 个模型的名称。这些修复不依赖区域设置。

模型默认语言和自动翻译是显示偏好。现在在打开菜单、载入模型和切换操作面板时恢复这两项的可用状态，保留原勾选值，避免沿用先前的灰色状态。模型编辑菜单仍按原规则限制。未取得反馈模型，不能确认对方最初触发禁用状态的路径。

修改前生产源码为 `b3cfb98`，位于 `.local/worktrees/japanese-name-before/`，加入同一份新增测试。为直接测试首次选择状态，把原有列表刷新代码原样提取为函数；旧版对照不包含任何修复逻辑。前后独立构建位于 `.local/builds/japanese-name-before/` 和 `.local/builds/japanese-name-after/`，均为 Windows x64 Release，关闭 DIAG 和 PERF。

测试副本通过 [Windows 应用清单的 activeCodePage](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests#activeCodePage) 设置进程编码，并断言 `GetACP()` 的实际值。没有修改系统区域设置，测试窗口保持隐藏。

- 932、936、1252、65001 四种编码：修改前均在无效首选表情名回退用例失败，修改后均通过。
- 用例覆盖固定字段无结束符、末尾半个双字节字符、可用备用名称、UTF-8 标记、翻译开关和原始字节不变。
- 常规 PMX 的原始 Unicode 名称、缺失英文名、表情下拉框和显示组像素检查通过，包括在日区进程中切换中文、英文和日文界面。
- 模型列表初始未选中、点击前的 Unicode 标题、相机绑定恢复、附件默认项和已有选择保留通过。修改前在四种代码页下均无法恢复初始模型选择。
- 实际中日英菜单资源中，模拟先前被禁用的两项显示偏好；打开菜单后恢复可用，翻译勾选状态保持不变。修改前四种代码页均失败，修改后均通过。
- 完整界面语言、菜单语言与名称编码三组回归通过。

首次旧版选择框测试的日区样本异常退出。检查发现新增测试未初始化应用状态，已补齐两侧测试夹具并重新执行；最终四种代码页均在预期断言失败。早期异常和日志保留，不计为有效复现。

在日区进程中，按修改前、修改后、修改后、修改前的顺序重复两轮，每轮查询 40 万次有效旧式表情和显示组名称。CPU 查询耗时为修改前 17.080–18.172 ms、修改后 19.429–20.394 ms；两侧查询结果校验值一致。这是名称转换开销，不是播放 FPS 或 GPU 耗时，后两项未测量。

源码补丁、状态、EXE 哈希、原始失败与通过日志、交替耗时记录位于 `.local/artifacts/japanese-name-gaps-20261011/01/`。因文本换行导致的资源重生成差异仅归档，恢复使用已提交资源，没有混入本次修改。

<a id="record-accessory-message-encoding"></a>
## 附件错误提示编码修复

原始记录：`ACCESSORY_MESSAGE_ENCODING_20261004.md`。其中的版本、日期和验证范围沿用原记录。

原版 MMS 与 MMS-Modular 的附件加载代码均有同一缺陷：CP932 日文常量直接传给 `MessageBoxA`，系统代码页为 936 时显示乱码。此提交只修复原版 MMS 的四个附件提示调用，改用已有的 `LegacyUiMessageBoxA`。该函数解码字符串后调用 `MessageBoxW`。附件加载、物理计算和字体设置没有改变。

<a id="record-accessory-message-encoding-验证结果"></a>
### 验证结果

基线为 `3d396da2fcc1587e6004e0d6336d0ec8d6f97973` 的已提交源码，不包含原版工作区中其他未提交改动。两侧使用独立 Release 构建目录、相同 MSVC 工具链与依赖。新增测试实际调用 `LoadAccessoryFile`，通过隐藏的 CBT 探针读取系统提示文字并关闭提示，不创建渲染设备。

| 检查 | 修复前 | 修复后 |
| --- | --- | --- |
| 日文模式下缺失附件、无效附件、255 槽位上限提示 | 乱码，检查失败 | 完整 Unicode 文本，检查通过 |
| 英文与中文模式下上述提示 | 通过 | 通过 |
| 加载失败后附件槽位回滚 | 通过 | 通过 |
| 原有 `ui_message_tests` | 通过 | 通过 |

系统 ANSI 代码页实测为 936。中文模式下这些附件错误仍沿用原有英文回退文本；此次没有补充中文翻译。测试在每个语言模式读取三个实际提示，共九个提示。探针按窗口属性去重，避免同一系统提示重复触发激活回调而误报。

正式测试入口为 `accessory_message_regressions`，超时为 10 秒。原有 `ui_message_tests` 保持独立。

<a id="record-accessory-message-encoding-构建与证据"></a>
### 构建与证据

修复前 EXE 位于 `D:/MMD-Next/builds/ui-visible-acceptance-20261004/original-before/Release/MikuMikuDance.exe`，SHA-256 为 `de8fe4762dd254634449a5e6f2273f350db2967a457a9eeebb5b39ce0df23065`。

修复后 EXE 位于 `D:/MMD-Next/builds/ui-visible-acceptance-20261004/original-after/Release/MikuMikuDance.exe`，SHA-256 为 `bd98438508ce0eb817a3ba96327f8a496b3ab6ae3ff7f78169a8f8a4d0a2baad`。此构建没有部署到桌面 Release。

来源及二进制哈希见同目录 `accessory-source-manifest.json`。测试日志为 `original-before-final-tests.log` 与 `original-after-final-tests.log`。修复前 CTest 返回 8，符合新增编码回归失败的预期；修复后两个测试均通过，返回 0。

MMV 双臂蝴蝶抖动是独立的物理问题，本提交不声称修复该问题。

<a id="record-physics-editor-encoding"></a>
## 物理模型编辑页编码修复验收

原始记录：`PHYSICS_EDITOR_ENCODING_20261011.md`。其中的版本、日期和验证范围沿用原记录。

本次修复模型编辑菜单 262 的物理编辑窗口。日区下中文标题不再变成问号，刚体与关节的列表、名称编辑框和关联刚体列表使用完整的 PMX Unicode 名称。旧 PMD 名称按 CP932 解码，与进程 ACP 无关。

<a id="record-physics-editor-encoding-原因与修改"></a>
### 原因与修改

窗口此前仍通过 `CreateDialogParamA` 创建，编辑框的子类过程也使用 A 接口。即使调用 `SetWindowTextW`，文本仍可能在 ANSI 窗口过程中丢失。现在只有该窗口改为 W 创建与 W 子类链，新增的 `CreateUiDialogParamW` 仍执行现有中文本地化。

物理记录同时保存完整 PMX 宽名称和固定 20 字节 CP932 镜像。此前界面只读取镜像，并在切换选项或确认时重新写入名称。现在优先读取原始宽名称；文字未变时保留原字节和宽名称指针。读取编辑框不再使用固定 256 字符缓冲区。

实际改名仍遵守现有扩展模型的 PMD 保存格式：按完整 CP932 字符截取，允许完整 20 字节且无结尾 NUL。无法表示的新名称会恢复旧名称并提示，不再清空字段。取消只释放借用名称指针的临时数组；确认时释放被删除或改名记录的旧宽名称，保留记录转交原名称所有权。下游 MMV 读取和两条 PMD 保存路径均按 20 字节处理。

球、盒、胶囊切换后的尺寸标签改用三语宽字符串，日文宽度标签保留原全角空格。

“保留完整 PMX 名称”指载入后的内存和编辑界面。菜单 263 的扩展模型导出仍为 PMD，只保存 20 字节 CP932 镜像；本次未增加 Unicode 导出格式。

<a id="record-physics-editor-encoding-前后版本与构建"></a>
### 前后版本与构建

| 项目 | 修复前 | 修复后 |
| --- | --- | --- |
| 源码 | `4425ec88f82c97a4ba1453a94663f3a92e0391db` | `8768d09faeb5828f1d5e454890b3d396f06abf33` 加本次补丁 |
| 工作树 | `.local/worktrees/goldboom-mme-before/` | `.local/worktrees/physics-editor-encoding/` |
| 构建 | `.local/builds/goldboom-mme-before/` | `.local/builds/physics-editor-encoding-after/` |
| EXE 字节数 | 4,745,728 | 4,748,288 |
| EXE SHA-256 | `cfd909e709a89609b6cd1257c67f88c6f16c5774b76b6194e5295f8efdc9c000` | `f1b2e4be4d20de3cbade3118511492a7f867af6e3e5a86b5c264436934b6bddb` |
| 测试 EXE SHA-256 | `ce4d75b6d7a712f5f96e6fb09d4ec19d8391e36e6300be50a6374cae8079f7c6` | `7cef6161152e20dcf8aee595259801c6714d19fd77bb0d97ddf5f06784ac9c41` |

`git diff 4425ec8 8768d09 -- src include res sdk` 为空。两侧使用相同测试源码与 CMake 注册，因此复用旧基线不会引入生产行为差异。新增测试源码 SHA-256 为 `176b5fabcbbb6a955e6d98e31db8a034b4833bf1453127754854795d6366a3b3`。

两侧均为 VS 2022 x64 Release，使用正式 Conan 工具链，关闭 `MIKUDANCESTUDIO_DIAG` 与 `MIKUDANCESTUDIO_PERF`，开启 DX12、效果测试、UI 测试和编码测试选项。先串行完成修复前构建，再完成修复后构建。本次 EXE 净增 2,560 字节。构建参数、源码状态、补丁、哈希及全部原始日志位于 `.local/artifacts/physics-editor-encoding-20261011/01/`，索引为 `build-manifest.json`。

<a id="record-physics-editor-encoding-四种-acp-的真实窗口回归"></a>
### 四种 ACP 的真实窗口回归

`physics_editor_encoding_tests` 链接生产 core 和实际窗口资源，载入零网格 PMX，创建真实 Bullet 刚体与关节，经 `CommandDispatch(262)` 打开实际窗口。测试不创建渲染器或 D3D 设备。

每个独立测试 EXE 副本嵌入对应 `activeCodePage` manifest，进程调用 `GetACP` 核对实际值。启动器通过 `win32process.CreateProcess` 的 `lpDesktop` 指定隔离桌面，测试再读取 `GetThreadDesktop` 核对完整桌面名称。所有八次运行均完成 ACP 和子进程桌面核对，未修改系统区域设置。

| 进程 ACP | 修复前失败数 / 816 | 修复后失败数 / 816 |
| --- | ---: | ---: |
| 日文 932 | 200 | 0 |
| 中文 936 | 178 | 0 |
| 西文 1252 | 182 | 0 |
| UTF-8 65001 | 205 | 0 |

每次运行打开 12 个窗口，覆盖三种 UI 语言。检查包含标题、动态形状标签、全部名称控件、超过 300 个 UTF-16 单元的名称、emoji、未改名确认、取消、名称编辑、选择往返、物理参数复制粘贴，以及 CP932 尾字节落在引导字节区间的合法字符。测试还检查拒绝无法表示的输入、按完整字符截断和确认后重新打开的名称。

PMD 部分使用真实 PMX loader 建立的完整记录，移除宽名称后填入 PMD 的 CP932 字段表示。它验证编辑器处理旧记录的路径，不代表执行了 PMD 文件加载器或文件导出往返。

修复后的 `ui_language_tests`、`filename_encoding_tests` 和 `pmx_joint_tests` 均退出 0，关节回归显示 `pmx_joint_failures=0 warning_dialogs=3`。这三项旧测试通过同一隔离桌面启动器运行，但未自行输出桌面名称；其记录不标为子进程桌面核验通过。

<a id="record-physics-editor-encoding-窗口操作耗时"></a>
### 窗口操作耗时

日文 ACP 下按 ABBA、ABBA 顺序采集，A 为修复前，B 为修复后。每个进程先预热一次，再执行 10 次相同的英文 UI、ASCII 名称窗口打开、填充、读取和取消。每侧四个样本，全部退出 0，校验值均为 130。PMX 载入不计入计时。

| 每次操作指标 | 修复前中位数（范围） | 修复后中位数（范围） |
| --- | --- | --- |
| 墙钟耗时 | 28.202 ms（27.664–28.873） | 29.197 ms（27.153–30.785） |
| 进程 CPU 时间 | 26.563 ms（23.438–26.563） | 25.000 ms（23.438–29.688） |

墙钟中位数增加约 1.00 ms，样本范围重叠。进程 CPU 时间分辨率较粗，不能据此宣称提速。这些数据只衡量编辑窗口操作，没有测量播放 FPS 或 GPU 耗时，也不代表大型物理模型的窗口操作成本。原始八次样本与汇总见 `*-benchmark-ja-JP-abba*/` 和 `benchmark-summary.json`。

<a id="record-physics-editor-encoding-本地交付"></a>
### 本地交付

通过验收的 EXE 更新至 `build/Release/MikuMikuDance.exe`，完整运行目录快照保留在 `.local/releases/physics-editor-encoding-20261011/Release/`。替换前的 EXE 备份位于 `.local/archives/physics-editor-encoding-20261011/`，其 SHA-256 为 `909aeaca86b71bcb16afdf7fa508be09267336273efb6838481c8850e2a6f8c7`。部署清单位于验收目录的 `deployment.json`。没有发布 GitHub Release 或推送代码。

两个 ChatGPT 子进程分别负责生产修改与测试，再交叉复核；主进程审查差异并执行全部构建和验收。该记录只验收物理编辑页编码修复，不改变 GoldBoomEX 同名 FX 重分配问题的未复现状态。

<a id="record-menu-language-fix"></a>
## 菜单语言混杂修复

原始记录：`MENU_LANGUAGE_FIX_20260927.md`。其中的版本、日期和验证范围沿用原记录。

动态插入运算器、透明排序等菜单项后，按位置更新文字会错改其他项目，并留下部分英文。现在普通项目按命令 ID 更新，子菜单通过其内部固定命令定位，缺失的项目直接跳过。全屏/立体显示名称也按命令 ID 更新。

「窗口置前」与「记住上次目录」移到帮助菜单；运算器与操作柄位置设置也位于帮助菜单，均属于用户偏好。原菜单资源保持 UTF-16 编码，并重新生成两份资源文件。

<a id="record-menu-language-fix-验证"></a>
### 验证

同一测试驱动、同一菜单资源分别链接修改前和修改后的生产菜单代码。旧版复现显示菜单英文残留、项目错位；新版通过两套原始菜单模板中 28 个项目的中英日文切换，并通过动态插入干扰项、菜单句柄稳定性和全屏命令检查。

证据保留于 `.local/artifacts/global-processor/menu-help/`：`menu-runtime-before.log`、`menu-runtime-final.log` 和 `before-after-test-hashes.json`。`gen_resources.py gen` 与 `dialog_rc.py verify` 均通过。未使用桌面截图作为验收。

<a id="record-ja-en-ui-review"></a>
## 新增界面日英审查（2026-10-14）

原始记录：`JA_EN_UI_REVIEW_20261014.md`。其中的版本、日期和验证范围沿用原记录。

本轮基于 `98c22fb` 的阴影、HUD 和最近邻放大，以及 `b9ee055` 的白线、TAA、SMAA 和 NVIDIA 修复界面。两个功能提交按顺序保留；本轮只调整三个对话框的文案，没有改变渲染或保存行为。

<a id="record-ja-en-ui-review-审查范围与修改"></a>
### 审查范围与修改

| 界面 | 结论与修改 |
| --- | --- |
| 图片序列输出 | 日文结束选项改为“出力完了後に MMS を終了”。格式、文件名前缀、帧范围、示例和按钮均能完整显示。英文保留明确的成功导出条件。 |
| 自动保存 | 日文文件夹按钮与导出界面统一为“参照…”。数量上限改为“バックアップの上限（件）”。说明明确使用原文件名和时间、手动文件不会被覆盖，以及只统计 PMM 备份。 |
| 视口分辨率 | 英文标题为“Viewport resolution scale”，日文为“描画解像度の倍率”，区分分辨率比例与相机缩放。关闭按钮改为“关闭／Close／閉じる”：滑块和“应用”会立即生效，关闭窗口不会撤销设置。菜单入口增加省略号。 |
| 最近工程 | 检查空列表、标题及文件浏览入口的日英显示。 |
| 抗锯齿菜单 | 检查原始、MSAA、FXAA、TAA 和四档 SMAA。日文“斜線・コーナー強化”由 AA 会话在 `b9ee055` 中完成。 |
| NVIDIA 安装说明和错误 | AA 会话在 `b9ee055` 补齐三语言授权说明、状态、失败提示及标题；保留原始技术诊断。审查没有运行安装或触发 UAC。 |
| 动态精度、滤波加强、强制粘贴 | 检查现有日英短标签的源码用词。本轮未修改这些按钮的布局，也未将源码核对计为实拍覆盖。 |

<a id="record-ja-en-ui-review-验证"></a>
### 验证

- 私有、未切换的 Win32 桌面：实际调用生产 `DialogProc` 和菜单构建逻辑，记录桌面名，退出所有自建测试窗口。
- 三语言共 15 个对话框状态：图片序列、自动保存、视口默认、超过 4K、输入无效。
- 三语言共 15 个弹出菜单：文件、最近工程、显示、抗锯齿、SMAA。
- 使用控件实际字体测量文字和边界，未发现截断或越界。日英文案和截图经人工式视觉检查。
- 本轮实拍为 96 DPI，未覆盖其他 Windows 缩放倍率或不同系统字体。无 GPU 的菜单审查中部分算法为灰色；算法可用性和动作路径另由 AA 会话的真实 GPU 测试覆盖。
- 白线／TAA 会话提供的最终图片序列日志为 749 项通过；三语言菜单回归和 NVIDIA 模拟错误测试通过。最终本地构建保留其提交，不重复执行已通过的渲染矩阵。

本地截图、控件测量、构建日志和可浏览审查页位于 `.local/artifacts/locale-review-20261014/`。既有用户 MMD 未被关闭，未上传或发布远端文件。
