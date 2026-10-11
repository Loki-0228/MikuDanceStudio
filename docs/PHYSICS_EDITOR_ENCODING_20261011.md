# 物理模型编辑页编码修复验收

本次修复模型编辑菜单 262 的物理编辑窗口。日区下中文标题不再变成问号，刚体与关节的列表、名称编辑框和关联刚体列表使用完整的 PMX Unicode 名称。旧 PMD 名称按 CP932 解码，与进程 ACP 无关。

## 原因与修改

窗口此前仍通过 `CreateDialogParamA` 创建，编辑框的子类过程也使用 A 接口。即使调用 `SetWindowTextW`，文本仍可能在 ANSI 窗口过程中丢失。现在只有该窗口改为 W 创建与 W 子类链，新增的 `CreateUiDialogParamW` 仍执行现有中文本地化。

物理记录同时保存完整 PMX 宽名称和固定 20 字节 CP932 镜像。此前界面只读取镜像，并在切换选项或确认时重新写入名称。现在优先读取原始宽名称；文字未变时保留原字节和宽名称指针。读取编辑框不再使用固定 256 字符缓冲区。

实际改名仍遵守现有扩展模型的 PMD 保存格式：按完整 CP932 字符截取，允许完整 20 字节且无结尾 NUL。无法表示的新名称会恢复旧名称并提示，不再清空字段。取消只释放借用名称指针的临时数组；确认时释放被删除或改名记录的旧宽名称，保留记录转交原名称所有权。下游 MMV 读取和两条 PMD 保存路径均按 20 字节处理。

球、盒、胶囊切换后的尺寸标签改用三语宽字符串，日文宽度标签保留原全角空格。

“保留完整 PMX 名称”指载入后的内存和编辑界面。菜单 263 的扩展模型导出仍为 PMD，只保存 20 字节 CP932 镜像；本次未增加 Unicode 导出格式。

## 前后版本与构建

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

## 四种 ACP 的真实窗口回归

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

## 窗口操作耗时

日文 ACP 下按 ABBA、ABBA 顺序采集，A 为修复前，B 为修复后。每个进程先预热一次，再执行 10 次相同的英文 UI、ASCII 名称窗口打开、填充、读取和取消。每侧四个样本，全部退出 0，校验值均为 130。PMX 载入不计入计时。

| 每次操作指标 | 修复前中位数（范围） | 修复后中位数（范围） |
| --- | --- | --- |
| 墙钟耗时 | 28.202 ms（27.664–28.873） | 29.197 ms（27.153–30.785） |
| 进程 CPU 时间 | 26.563 ms（23.438–26.563） | 25.000 ms（23.438–29.688） |

墙钟中位数增加约 1.00 ms，样本范围重叠。进程 CPU 时间分辨率较粗，不能据此宣称提速。这些数据只衡量编辑窗口操作，没有测量播放 FPS 或 GPU 耗时，也不代表大型物理模型的窗口操作成本。原始八次样本与汇总见 `*-benchmark-ja-JP-abba*/` 和 `benchmark-summary.json`。

## 本地交付

通过验收的 EXE 更新至 `build/Release/MikuMikuDance.exe`，完整运行目录快照保留在 `.local/releases/physics-editor-encoding-20261011/Release/`。替换前的 EXE 备份位于 `.local/archives/physics-editor-encoding-20261011/`，其 SHA-256 为 `909aeaca86b71bcb16afdf7fa508be09267336273efb6838481c8850e2a6f8c7`。部署清单位于验收目录的 `deployment.json`。没有发布 GitHub Release 或推送代码。

两个 ChatGPT 子进程分别负责生产修改与测试，再交叉复核；主进程审查差异并执行全部构建和验收。该记录只验收物理编辑页编码修复，不改变 GoldBoomEX 同名 FX 重分配问题的未复现状态。
