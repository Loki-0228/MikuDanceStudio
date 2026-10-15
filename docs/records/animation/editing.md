# 骨骼、曲线与镜头编辑记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [播放范围留空时无法启动](#record-empty-playback-range)
- [曲线同步开销](#record-curve-sync-performance)
- [MMS 原生骨骼工具与曲线编辑验收](#record-mms-native-editors-validation)
- [普通点击骨骼选择修复](#record-bone-click-selection)
- [Overlapping bone selection](#record-bone-pick-cycle-fix)
- [骨骼竖向旋转柄与原版方向一致性](#record-rotation-axis-parity)
- [旋转拖拽与视频导出修复](#record-rotation-export-fixes)
- [自动联动相邻插值控制点](#record-auto-interpolation-link)
- [自定义曲线保存修复](#record-curve-save-recovery)
- [相机追踪视角修复](#record-camera-tracking)
- [时间轴隐藏表情双击崩溃](#record-hidden-morph-panel-safety)
- [曲线、表情烘焙与局部控制柄修复验收](#record-editor-input-fixes)

<a id="record-empty-playback-range"></a>
## 播放范围留空时无法启动

2026-10-15 核对。手动切帧正常，但播放按钮和 P 键均无法启动。现场只读检查确认桌面 EXE 与正式构建 SHA-256 相同，MME 已加载，起止帧输入为空，播放状态仍为停止。填写起止帧 `0` 后，用户确认播放恢复。

原因是 `3fffbf5` 将播放范围从 `atol` 改为严格帧号解析，遗漏了默认空输入的含义。原界面创建空起止帧框；空起点表示第 0 帧，空终点表示最后登记帧。严格解析拒绝空字符串，在设置播放状态之前返回。此前播放测试预先填写有效起止帧，因此未覆盖这个入口。

修复仅为播放范围增加空值解析，并用于循环播放重新读取起点的路径。空值和纯空白按 `0` 处理；非空的负数、非数字及溢出帧号仍被拒绝，其他帧号输入保持严格校验。

`media_render_tests --video-playback` 在修复前明确报出 `FAIL blank default playback range starts playback`。修复后的真实 MME 测试覆盖按钮、P 键、空值、显式范围、循环和非法输入；另用 YYB NT、动作 VMD 及桌面「指路.mp4」验证无视频和拖入视频后的播放游标、骨骼矩阵、GPU 顶点及物理推进。记录位于 `.local/artifacts/playback-regression-20261015/`。此问题的复现不依赖视频或 MME。

<a id="record-curve-sync-performance"></a>
## 曲线同步开销

2026-10-15 核对。表情编辑器原先逐条读取表情，每次读取都会检查全部已编辑表情。`ReadMany` 将同一次界面读取合并为一次同步，仍逐条检查原生帧号和值；未变化的有序链表直接比较已有记录，省去临时数组和排序。原生修改、插件直接写入、撤销恢复和非标准链表仍走原来的同步规则，没有延迟到停止播放后才检查。

编辑器选区和原生时间轴选区改为按帧号有序匹配。选区不变时直接比较已有选择，不重新构造整套索引。来源回调返回乱序或重复帧时先归一化匹配，源数据不变。通知仍按 UI 消息合并，不降低同步频率。

下表来自同机 Release 构建，按旧、新、新、旧顺序运行，各取两轮均值。每轮整组读取测量 8 次，表情求值测量 80 次；不变选区测量 60 次，变化选区测量 30 次。窗口保持隐藏，通过样式标志测试可见窗口的定时分支，不采集桌面，不创建渲染设备。

| 场景 | 修改前 | 修改后 |
| --- | ---: | ---: |
| 64 条表情、每条 501 帧，整组读取 | 22.361 ms | 0.803 ms |
| 同一表情夹具的分数帧求值及同步 | 0.339 ms | 0.080 ms |
| 8000 个关键帧，选区不变的定时同步 | 1.934 ms | 0.030 ms |
| 8000 个关键帧，交替改变末尾选择 | 2.883 ms | 1.410 ms |

新增回归检查曲线切线和分数帧采样、原生改值与恢复、乱序和重复帧、异常链表、批量读取顺序与失败输出、选区回写及撤销、空选区，以及 100 次通知只读取一次。`morph_curve_tests`、`bone_curve_tests`、`bone_scale_curve_tests` 和 `animation_curve_sidebar_tests` 通过。

完整 UI 测试仍有既存失败：场景宿主的缩放文字重绘、表情宿主的时间轴拖动和箭头位置、共享编辑器的范围条向左移动。优化前源文件重编译后也出现这些失败。另修正共享编辑器测试：激活窗口后须处理已投递的同步消息，再检查标签和撤销历史。

本次没有消除骨骼自由曲线的逐帧摘要检查，也没有改为全局编辑版本号缓存；骨骼来源重读仍会重建通道。上述数据只衡量同步及求值，不是整帧或 MME 性能结论。原始日志、修改前源文件和回归记录保存在本机 `.local/artifacts/curve-sync-20261015/`，不随发布包分发。

<a id="record-mms-native-editors-validation"></a>
## MMS 原生骨骼工具与曲线编辑验收

原始记录：`MMS_NATIVE_EDITORS_VALIDATION_2026-09-27.md`。其中的版本、日期和验证范围沿用原记录。

本轮修改属于 MMS 宿主。骨骼工具、表情曲线、附件与场景曲线均不要求安装 MMV。共享曲线窗口也会随两种 MMV DLL 一起构建。

<a id="record-mms-native-editors-validation-保留的前版"></a>
### 保留的前版

- 宿主快照：`.local/releases/native-bone-before/`，来源 `80bc305`。EXE SHA256：`5C5B54DC8C14219F3CAD787F26743337DF2B32A2D0FD725394D1F9BC2CDEA857`。
- 同条件 MME 测试宿主：`.local/releases/native-editors-before-readback/`，从上一轮已验收的 `mmv-bake-after/postprocess` 保留。EXE SHA256：`115571C413A0B2A6C1A10DF459EAEB6FC6E5A11F788E53F1AF146FF2D8203857`。
- 哈希记录：`.local/artifacts/native-bone-tools/before-binaries.json`。源模型、效果文件未修改。

<a id="record-mms-native-editors-validation-阶段验收记录"></a>
### 阶段验收记录

所有运行均在隐藏窗口中进行。界面图使用被测窗口的 `WM_PRINT`，三维图使用生产 DX9 渲染目标回读；没有读取桌面像素或激活用户窗口。

| 项目 | 结果与证据 |
| --- | --- |
| 骨骼缩放已有行为 | `bone-scale-cpu-1.log` 通过：矩阵、层级、关键帧、撤销、剪贴板、拖动、PMM/VMD 往返。 |
| 新缩放手柄与按钮 | `gizmo-cpu-3.log` 通过：轴投影方向、禁用条件、原移动旋转操作编号、三态选择、布局与三语文字宽度。 |
| 真实宿主手柄与布局 | `run-ui-2.log` 退出 0；真实 Rou PMX、三语按钮位置与内边距、三态切换、水平 X/垂直 Y 拖动、白框双轴缩放、转动镜头后的 Z 手柄。`ui-2/` 保留窗口图和前视/斜视 GPU 画面。 |
| 表情曲线后端初验 | `morph-cpu-1.log` 127 项通过；后续范围导出与原生编辑恢复修正需要重新验收。 |
| 三段比较控件初验 | `curve-ui-1.log` 1358 项通过；后续自动模式显隐一致性修正需要重新验收。 |
| 非常值区间算法 | `comparison-cpu-1.log` 通过；包含端点相同而中间变化的曲线，不使用有限帧采样代替判定。 |
| 前版 MME 图像基线 | `before-render-1.log` 通过；Rou + SvSSAO + MMV 恒等效果，保留 baseline/assigned/reloaded BGRA 和原始耗时。 |

本表是阶段记录，不能替代最终整包验收。后续需补齐缩放插值保存、表情实际入口与 VMD 范围往返、两种 DLL 构建、最终同条件前后对照。

<a id="record-mms-native-editors-validation-原生功能后续验证与-mmv-交接"></a>
### 原生功能后续验证与 MMV 交接

- `run-ui-5.log` 退出 0：实际宿主的四组表情入口、三语比较模式、VMD 曲线写入与读回，以及 7.5 帧采样一致性通过。产物在 `ui-5/`。此前 `ui-3`、`ui-4` 的失败记录保留，不计为通过。
- `build-native-handoff-2.log` 构建通过：原生帧率后端、帧率窗口、总事务控制器及缩放 IO 测试。第一次真实核心链接暴露 `CaptureCurveTable`、`SwapCurveTable` 被放进 `gizmo` 命名空间，修正后重新构建。
- `handoff-verified-tests.log`：`animation_retime_tests`、`frame_rate_dialog_tests`、`frame_rate_correction_tests`、`bone_scale_io_tests`、`mesh_bake_cache` 五个目标全部通过。缩放 IO 包括恒等矩阵端点的非线性曲线保存。
- 帧率窗口在隐藏窗口中验证了三语控件边界、输入框内边距、预览、应用和完整报告。此环境的隐藏原生 `EDIT` 控件不会把文字绘入 `WM_PRINT` 图像；图像本身不能证明输入文字可见性，控件文字、矩形和样式另行断言。没有补画文字冒充窗口回读。

`native-final-tests.log` 中 13 个相关目标通过，包括真实原生帧率后端、作者曲线、缩放矩阵与 IO、曲线窗口、网格缓存。`frame_rate_correction_tests` 的依赖使用测试替身，通过不能替代实际 MMV 的事务集成。

按用户要求，MMV 新分层架构、存档和分配界面交由「设置 MMV Bullet3 物理解算」会话主导。本会话停止重叠的 MMV 子任务，交接了 retime ABI 和全局“应用／关闭回退”要求；原生 MMS 文件独立继续。MMV 运行时事务、顶部层页签和全局应用尚不能据此标为完成。交接正式结果保存在同目录 `handoff-*.json`。

DSH 原生子任务曾因 `QUOTA: Insufficient Balance` 中断；用户充值后，新任务 `3111e75e-1383-435e-82cf-e4b77d5c084b` 在正式工作目录继续，最终限定只修改原生帧率测试。正式结果为 completed，主 agent 随后编译复核并修正生产代码及用例。未重启 DSH 宿主。

<a id="record-mms-native-editors-validation-最终原生验收"></a>
### 最终原生验收

- `native-handoff-7.log` 与 `native-final-tests.log`：无帧合并时骨骼 16 字节及相机 24 字节补间保持；合法交叉手柄、相同端点均可合并。缩放的九个矩阵分量按原曲线采样，合并槽的矩阵和曲线清除。失败回滚恢复矩阵、曲线和 revision，可重试。
- 原生 `.x` 作者曲线在不安装 MMV 时可修正。验证了非线性采样、回滚、编辑器外登记后的只读同步，以及合并后的标准记录值与曲线扩展一致。预览后增删对象、修改显示/IK 数据、新建缩放或作者曲线均使旧预览失效。
- `ui-final/run.log` 退出 0：真实 Rou PMX、三语布局、原生缩放手柄、四组表情入口和 VMD 往返通过。实际菜单执行帧率修正，自动打开一次另存为；取消保存后动画和完整逐帧报告保留。界面来自 `WM_PRINT`，画面来自 DX9 回读。
- 「基准」已改为「缩放中心」。`wording-final-tests.log` 的帧率窗口测试通过。曾尝试把“单一维度”解释为时间轴，已根据用户澄清撤回；该词指输出标量的维度。用户最终选择多次普通一维补间组合向量，不加入空间轨迹联合拟合。
- `after-render-1/run.log`：使用与前版相同的 Rou、SvSSAO、MMV 恒等效果及相同 DLL，真实 MME 的加载、分配、重载和保存通过。`mme-image-comparison.json` 中 baseline、assigned、reloaded 三张 640×480 RGB 回读均逐像素一致。
- 渲染原始阶段耗时保存在前后日志。assigned 的 60 帧阶段总墙钟时间为前版 24112.415 ms、后版 23435.630 ms；重载后的 30 帧为前版 10613.645 ms、后版 10796.154 ms。这只是各一次样本，未分离 CPU/GPU/等待时间，也未完成多轮波动采样，不据此宣称性能提升或播放 FPS。
- 已按用户要求覆盖 `build/Release/MikuMikuDance.exe`。运行中的旧 EXE 移至 `.local/archives/native-editors/overwrite-20260927-152953/`，没有结束用户进程；需要重启应用才使用新版。安装哈希与备份路径见 `overwrite-production.json`。

本次没有覆盖 MMV DLL。新版 MMV 的分层架构和运行时事务仍由协作会话整合，本报告仅验收 MMS 原生部分及与既有 MMV/MME 的兼容回读。

<a id="record-mms-native-editors-validation-排除的初次结果"></a>
### 排除的初次结果

- `build-gizmo-1.log`：新测试将三个宿主函数误声明为全局函数，产生链接错误；生产函数在 `mikudancestudio` 命名空间。修复测试声明后重新构建。
- `gizmo-cpu-1/2.log`：新测试错误要求第四行物理按钮与前几行有完全相同的尺寸/行距；保留原第四行布局，修正断言。
- `build-gizmo-2.log`：新集成测试链接生成资源时用了相对路径，改为仓库绝对路径。
- `run-ui-1.log`：合成 `WM_COMMAND` 没有触发自动选择按钮的按下状态，且未解析多重采样目标便回读。改为真实 `BM_CLICK` 和 `StretchRect` 解析后得到 `run-ui-2`。没有把失败画面计为有效渲染结果。
- `build-gizmo-3.log`：表情测试两处将 `animation_curve::NamedTrack` 写成错误命名空间；修正后 127 项通过。
- `build-gizmo-4.log`：共享编辑器正在由 DSH 改写三段控件，读到了未完成的源文件。随后单独用已构建的生产库验收手柄；最终构建须等各模块停止写入。

<a id="record-mms-native-editors-validation-当前边界"></a>
### 当前边界

原生骨骼移动和旋转保持原有输入路径。视口缩放轴沿实际投影方向拖动；白框沿视图平面对角方向取一个倍率，同时作用于视图平面的两轴。右下角缩放 HUD 排仍沿用原有纵向拖动规则。

表情曲线浮窗具有自己的撤销和重做。宿主原有表情键操作没有骨骼键那样的原生撤销环；本轮不能将两者混为一谈。新增的曲线状态恢复和保存只应记录实际已验证的行为。

<a id="record-bone-click-selection"></a>
## 普通点击骨骼选择修复

原始记录：`BONE_CLICK_SELECTION_20260927.md`。其中的版本、日期和验证范围沿用原记录。

物理计算开启后，无刚体的普通骨骼曾被点击选择逻辑误判为物理锁定。框选没有这项错误限制，因此出现框选正常、普通点击无效。

`BoneCanBePicked` 现在按 `hasRigidBody` 判断物理锁定，与骨骼标记的显示规则一致。无刚体的可见普通骨骼在各物理模式下均可点击。有刚体的骨骼继续遵循对应模式及 `physicsDisabled` 标志，不解除物理接管限制。

同一隐藏窗口测试在修复前失败于「模式 1 下点击没有刚体的普通骨骼」，修复后整套注册、点击和框选回归通过。框选测试经测试入口驱动原有按下、移动和释放逻辑，不伪造桌面按键或前台窗口。

两侧均为 VS 2022 x64 Release。基线采用 `.local/worktrees/mms-stutter-profile/` 中修复前的 `frame_modes.cpp`，仅同步不改变生产行为的框选测试入口；候选采用正式 `main` 工作区。记录位于 `.local/artifacts/global-processor/bone-registration-before.log`、`bone-registration-integration.log`，构建记录为 `menu-help/baseline-tests-build-seam.log` 和 `integration-build-20260927.log`。该对照验证输入行为，不代表播放性能测量。

<a id="record-bone-pick-cycle-fix"></a>
## Overlapping bone selection

原始记录：`BONE_PICK_CYCLE_FIX_20261012.md`。其中的版本、日期和验证范围沿用原记录。

Repeated clicks on overlapping markers now cycle through the eligible bones in model order. This applies to the full and minimal builds and to the main and floating viewports.

<a id="record-bone-pick-cycle-fix-cause-and-reference-behavior"></a>
### Cause and reference behavior

`PickBoneAtCursor` in `src/app/frame_modes.cpp` previously kept the highest bone index under the cursor. Every click selected that same bone, making an overlapping rotation-only bone unreachable when a translation-enabled bone had a higher index.

The reference is `refs/MikuMikudance 932 fix/MikuMikuDance.exe` (SHA-256 `3ebe6ead8076478c06539b14a0e32e70f30b0edc9c8f045819d04ad8d62bee49`). Its x64 picker saves the first eligible hit at `0x140031C81`, selects the first hit whose index exceeds the active bone index at `0x140031C89`, and wraps to the saved first hit at `0x140031D83`.

The corrected picker follows that order. It preserves the existing visibility and physics filters, the exclusive eight-pixel hit radius, and Shift's toggle behavior. It allocates no memory and can stop scanning as soon as it finds the next hit. It changes no model or motion file format.

<a id="record-bone-pick-cycle-fix-verification"></a>
### Verification

`tests/bone_registration_tests.cpp` sends mouse messages to the actual main and floating window handlers through hidden test windows. The fixtures cover PMD and PMX model modes, overlapping rotation-only and translation-enabled bones, a third overlapping bone, wraparound, rapid clicks delivered as `WM_LBUTTONDBLCLK`, hidden markers, physics locks, hit-radius boundaries, an invalid active index, empty clicks, Shift, and playback.

The original picker fails the new first-click assertion. After the fix, the complete `bone_registration_tests` executable passes in both build profiles, including selection, registration, movement, undo, frame changes, physics locks, and box selection.

The broader baseline run also exposed two incomplete synthetic gestures in older tests: a direct Shift press lacked its release, and a Shift-click helper omitted Shift from its release message. The tests now complete those gestures. No production behavior was changed for these fixture corrections.

Run the focused checks with:

```powershell
& '<build>/tests/Release/bone_registration_tests.exe' --pick-cycle
```

Omit `--pick-cycle` to run the complete suite. Local logs and video frames are under `.local/artifacts/bone-pick-cycle-20261012`. The tests use synthetic bone fixtures; the exact character from the supplied video was not loaded for an interactive retest.

<a id="record-bone-pick-cycle-fix-candidate-builds"></a>
### Candidate builds

The executables under `build/Release/bone-pick-fix-20261012/full` and `minimal` include this fix and the preceding PMD shared-toon fix. Copy the matching executable into an existing installation to retain its data and plugins. These are local candidates, not a published release.

<a id="record-rotation-axis-parity"></a>
## 骨骼竖向旋转柄与原版方向一致性

原始记录：`ROTATION_AXIS_PARITY_20261012.md`。其中的版本、日期和验证范围沿用原记录。

用户提供的 30 秒对比视频中，从模型侧面拖动骨骼的竖向旋转柄，原版主要改变 Z 轴角度，MMS 却混入 X、Y 轴旋转。切换 Local、Global、View 后仍有差异。该操作进入 `BoneEditModes` 的 mode 2；坐标选择器不改变这条原版操作路径。

<a id="record-rotation-axis-parity-原因与修复"></a>
### 原因与修复

旧实现将 `cos(-pitch)` 与 `sin(-yaw)` 组合成轴的 X、Z 分量。俯仰角为零、水平角为 ±90° 时，轴成为斜向，而非原版的 ±Z；Z 分量的方向也相反。上一轮的单位化处理解决了连续旋转带来的放大，但不能修正错误的轴方向。

本次改用 `(cos(yaw), 0, sin(yaw))`，保留转换到父骨骼坐标的矩阵运算、拖动角度、速度修饰键、固定轴覆盖和单位化处理。水平旋转柄、环形旋转柄、X/Y/Z 操作入口未改动。完整包与最小包共用此实现。

原版依据为 `refs/MikuMikudance 932 fix/MikuMikuDance.exe`，SHA-256：

```text
3ebe6ead8076478c06539b14a0e32e70f30b0edc9c8f045819d04ad8d62bee49
```

该 x64 映像的 `0x14003319A` 读取正的相机水平角（app + 0x34C），`0x1400331A7` 和 `0x1400331B7` 分别调用 `cosf` 和 `sinf`，随后至 `0x1400332BD` 将轴乘入父骨骼矩阵。导入跳板分别为 `0x14011A702` → IAT `0x140128390` 和 `0x14011A708` → IAT `0x140128388`。这条路径不读取 pitch 或 roll；不能用包含 roll 的完整相机逆矩阵替换它。

<a id="record-rotation-axis-parity-验证"></a>
### 验证

`bone_rotation_drag_tests` 通过真实骨骼编辑入口新增方向检查，使用显式的参考世界轴及独立的父矩阵求逆；比较带符号的单步姿态和逆向拖动后的姿态，避免仅检查单位长度而漏掉方向错误。

- 每档覆盖 PMD、PMX，8 组视角，3 种坐标选择，单位／旋转父矩阵，原始／已有旋转姿态，普通／粗调／细调，以及正反拖动，共 576 组单步与回程操作。
- 相同测试先运行于修复前代码，两档各有 14 项方向检查失败。仅替换生产实现后，完整档 188 项、最小档 184 项全部通过。新增方向检查的最大四元数分量误差约 `1.79e-7`。
- 原有 30 组、每组 10,000 次快速往返测试继续通过，包含多选根骨、固定轴、异常输入和已有漂移恢复。完整档另验证有意设置的非均匀缩放，实际骨矩阵基长度误差低于 `1.4e-7`。
- 两档正式 x64 Release 主程序及测试目标均重新构建，独立代码复核确认相邻两种旋转柄的公式与参考映像一致。

这些验证为参考映像静态核对及生产入口的数值回归，测试不创建窗口或图形设备。未载入视频中的原始模型工程进行桌面画面对照，也未以测试耗时判断播放性能。

<a id="record-rotation-export-fixes"></a>
## 旋转拖拽与视频导出修复

原始记录：`ROTATION_EXPORT_FIXES_20261011.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-rotation-export-fixes-骨骼反复旋转后放大"></a>
### 骨骼反复旋转后放大

快速正反拖动时，部分视角轴、直接操作的矩阵轴和模型固定轴不是单位向量。旧代码用它们生成旋转四元数，再持续相乘且不归一化。正反操作因此不能抵消，旋转数据的长度不断增长；骨骼矩阵将这类数据转成了变形。多选根骨的轨道计算也把非单位四元数的共轭当成逆，导致位置偏移带上缩放。

修复在 `BoneEditModes` 的公共旋转路径归一化操作轴、旋转增量和最终姿态，并在乘法前处理已有的长度漂移。零操作与非法轴不改动骨骼。向量乘法、模型缩放数据和历史视角轴方向保持原有行为；不能据此宣称纠正了全部视角方向问题。

同一份最小复现测试先在旧实现上运行：PMD 与 PMX 均在 64 次正反拖动后出现四元数长度约 53.329、最大矩阵基长度误差约 3169.110。完整档与最小档各 9 项检查中 6 项失败。仅替换生产实现后，同一测试两档均 9/9 通过，四元数长度误差低于 2.6e-8，回程为单位旋转。

扩展测试通过真实 `BoneEditModes` 和 `BoneFrameTransform` 验证 30 组旋转模式，每组 10,000 次不同幅度的往返操作，并覆盖多选根骨、未选子骨、超大或非法旧姿态、固定轴和合法缩放。完整档 156 项、最小档 152 项全部通过；完整档合法非均匀缩放的实际骨矩阵基长度误差低于 1e-7。另有完整档骨骼缩放、层级、撤销、剪贴板和 PMM/VMD 集成回归，以及最小档原插值与物理状态 32 项通过。

这是操作与骨骼变换入口的数值回归，未复现用户原始工程的完整桌面操作，不以测试耗时声称播放性能提升。两个构建档使用同一处修复。

<a id="record-rotation-export-fixes-视频导出数值多出数字"></a>
### 视频导出数值多出数字

导出窗口先把主界面结束帧写入控件，又通过 `EM_REPLACESEL` 插入计算默认值。该消息替换当前选区，不会替换整个字段。主界面为 `1100`、默认值为 `1` 时，旧实现实际显示 `11001`；确认后该值会进入导出范围，延长视频。

同一初始化中的宽高也使用插入消息。填写宽度会触发比例联动，先写入高度；随后填写高度又在已有文本中插入数字。通过实际本地化对话框包装运行旧代码，两档均复现结束帧 `11001`，以及 1920×1080 被填成宽 `19698`、高 `11080`，随后触发尺寸校验。

修复先确定结束帧，再将起止帧、帧率、宽高按完整数值赋值。主界面有效的非零结束帧优先；为零或未填时，默认值取已有动作末帧与音频长度的较大值。用户明确设置的较短范围不被长动作或音频覆盖。共享编辑框工具和确认阶段的尺寸校验未改动。

两档各通过 565 项检查、51 次真实对话框初始化与确认／取消，覆盖中文、英文、日文，8 组范围，重复打开，1920×1080 与 1918×1080。报告案例显示并确认的末帧均为 `1100`；取消保留原有帧范围、帧率和路径。

测试运行在未切换到前台的私有桌面，仅用固定编码器选项隔离本机枚举差异；帧范围计算、编辑框、比例联动和确认读回均走生产代码。未创建图形设备、录制图或视频文件，不宣称本次完成了实际编码或任意视频素材的端到端验收。

<a id="record-rotation-export-fixes-构建与分发"></a>
### 构建与分发

两档正式 x64 Release 目标已重新构建。公共缺陷继续在同一分支维护；最小包不因此加入骨骼缩放、新透明排序、阴影重构或现代媒体模块。发布 ZIP 仅含 `MikuMikuDance.exe` 与 `licenses/` 中的许可文件，修复记录和使用说明保留在仓库与发布页面中。

<a id="record-auto-interpolation-link"></a>
## 自动联动相邻插值控制点

原始记录：`AUTO_INTERPOLATION_LINK_2026-10-01.md`。其中的版本、日期和验证范围沿用原记录。

主界面的插值「自动」勾选框（530）现在控制相邻段联动。修改本段末尾控制点时，联动下一关键帧所保存曲线的开头控制点；修改本段开头时，联动上一关键帧所保存曲线的末尾控制点。相邻按同一轨道的关键帧查找，不是时间轴帧号加减一。

原生面板覆盖骨骼位置、旋转、缩放、相机六个插值通道和「全部」。独立编辑器的骨骼、表情及场景数据源读取同一个勾选框，只对可表示为插值控制点的连续通道生效；共享旋转、缩放曲线的轴一起更新。取消勾选后仍可单独编辑每段。

联动在 0..127 插值面板内镜像控制柄，保留相邻段的另一个控制点。镜像会越过另一个控制点时缩短控制柄并保持其方向。此规则联动归一化曲线的控制点，不承诺不同时间长度和数值跨度的两段具有相同的实际运动速度。

联动不增加关键帧，不改变帧号或关键帧值，不继续传递到整条轨道。首段开头及末段末尾没有可联动的相邻段。独立编辑器将本段和相邻段的修改放入同一次事务，宿主拒绝写回时整次修改失败；原生骨骼面板在拖动前的撤销快照中保存相邻记录，缩放仍由已有的缩放撤销表恢复。

<a id="record-auto-interpolation-link-验收"></a>
### 验收

证据保留在 `.local/artifacts/auto-interpolation-link-20261001/`。修复前提交为 `1884d0e`。目录包含前后源码、工作树清单、构建日志、EXE SHA256 和原始测试输出，不自动清理。修复前回归构建使用旧的生产实现，并保留追加的可选 Source 回调字段以编译同一份回归测试；旧实现不读取该字段。

两侧使用 Release、相同 CMake 构建目录、同一隐藏窗口测试和相同关键帧数据。测试不显示窗口，不截取用户桌面，不重启用户程序。

| 检查 | 修复前 | 修复后 |
| --- | --- | --- |
| `animation_curve_editor_tests auto-link` | 下一段开头未联动，退出 1 | 双向联动、实时勾选、共享旋转轴、事务失败及撤销重做通过，退出 0 |
| `ui_message_tests auto-curve` | 原生相邻控制点未联动，退出 1 | 骨骼/相机各通道、缩放、全部、取消勾选、首尾边界和骨骼撤销重做通过，退出 0 |
| `animation_curve_editor_tests` 完整入口 | `the new channel label is adopted` 失败，退出 1 | 同一名称刷新断言失败，退出 1；不算本次联动验收通过 |
| `bone_registration_tests` | 未作为本次前后联动对照运行 | 完整入口通过，退出 0 |
| `animation_curve_host_tests` | 未作为本次前后联动对照运行 | 完整入口通过，退出 0 |
| `morph_curve_host_ui_tests` | 未作为本次前后联动对照运行 | 完整入口通过，退出 0 |

最终生产 EXE：`build/Release/MikuMikuDance.exe`。SHA256：`A4EC79AA4EFD69B530FA70398CA0ECDC4A95F12E4A105601EC838254C953E18C`。本轮是编辑行为修复，未采集播放 FPS 或 GPU 性能指标。

新增回归保留在现有编辑器与实际 UI 消息测试文件内，用于防止丢失邻段写回或撤销快照；保留至相关编辑入口及自动联动功能被替换，并有等效行为覆盖。

<a id="record-curve-save-recovery"></a>
## 自定义曲线保存修复

原始记录：`CURVE_SAVE_RECOVERY.md`。其中的版本、日期和验证范围沿用原记录。

2026-10-02。

PMM 曲线扩展保存曾调用编辑器的原生轨道读取接口。原生相机插值无法解析时，即使已经存在有效的自定义曲线，保存仍报告 `Cannot capture an edited animation curve` 并中止。

现将有效的自定义曲线保留为扩展数据。原生读取成功时继续合并登记、删除等外部修改；读取失败时保留已编辑数据并等待后续同步。加载 PMM 时也允许有效扩展独立恢复。保存扩展失败仍保留原文件，并显示对应语言的提示。

<a id="record-curve-save-recovery-验证记录"></a>
### 验证记录

输出位于 `.local/artifacts/curve-save/20261002/`，不纳入版本控制。

- 修复前生产版本：`76b53fb` 加当时工作树改动，详见 `before-status.txt`。
- 同条件后台回归：先写入自定义相机曲线，再设置一个原生插值字节为 255，执行扩展保存、加载和切线数据比对。修复前失败，修复后通过。
- 原有全局和配件轨道同步、登记、删除、加载用例通过。测试控件窗口保持隐藏。
- Release 宿主和回归程序构建成功。构建命令为 `cmake --build build --config Release --target MikuDanceStudio animation_curve_host_tests --parallel 4`。
- EXE 哈希、两侧构建和回归日志均保留在上述目录。已有 MSB8028 中间目录警告仍存在。

此用例复现了保存错误的代码路径，未读取用户当前进程中尚未保存的自定义 VMD。未进行渲染性能测量。

<a id="record-camera-tracking"></a>
## 相机追踪视角修复

原始记录：`CAMERA_TRACKING_2026-10-01.md`。其中的版本、日期和验证范围沿用原记录。

在模型编辑界面勾选模型或骨骼、改变视角后，重新开启追踪应恢复当前帧的相机视角。原追踪开关在加载相机后写回旧视角，并跨参考点补偿位置，导致画面保持编辑视角。现在按钮与菜单均加载当前帧相机，再使用与相机面板相同的视角初始化。关闭追踪时仍保留当前画面。

追踪开启时，方向按钮及小键盘视角预设均使用相机分支。模型/骨骼参考不再决定这些预设的偏移，相机父骨骼也不会阻止“正面”按钮生效。

<a id="record-camera-tracking-验证"></a>
### 验证

`coordinate_axis_tests` 使用隐藏 D3D9 窗口，将实际世界与观察矩阵组合后比较。覆盖模型和骨骼参考、追踪按钮和菜单、无父骨骼和带父骨骼共八种组合；每种组合比较六个方向按钮和七个小键盘预设。原有关闭追踪保持画面及面板切换测试一并保留。

同一测试在修复前源码下复现“开启追踪未恢复相机”，在修复后源码下全部通过。完整骨骼登记回归也通过。测试不显示窗口、不移动桌面光标，未测量播放 FPS 或 GPU 性能。

前后均构建 `Release`，使用 `cmake --build build --config Release --target coordinate_axis_tests bone_registration_tests MikuDanceStudio -- /m /v:minimal`。正式入口为 `build/Release/MikuMikuDance.exe`。

基线为 `9df6499`；源码、工作树状态、构建日志、EXE 哈希和原始测试结果保留在 `.local/artifacts/camera-tracking-20261001/`。该目录继续保留，不自动清理；其他会话的未提交改动未纳入本次提交。

<a id="record-hidden-morph-panel-safety"></a>
## 时间轴隐藏表情双击崩溃

原始记录：`HIDDEN_MORPH_PANEL_SAFETY.md`。其中的版本、日期和验证范围沿用原记录。

本案例按用户报告登记为原版 MMD 缺陷，其修复属于 MMS 相对原版的新增特性。2026-09-27 已用用户提供的 `☆Rou.pmx` 完成 MMS 后台运行验收。`main` 中已有的范围保护满足下述目标行为；本次补充真实窗口消息回归测试、构建入口与验收记录，未改变生产逻辑。

<a id="record-hidden-morph-panel-safety-用户报告"></a>
### 用户报告

原版 MMD 有眉、目、口、其他四个表情操作栏。用户通过 PmxEditor（PE）的特殊设置将某些表情隐藏后，这些表情不出现在四个操作栏，只显示在时间轴左侧的「表情」表示枠列表。双击该列表中的隐藏表情会使原版 MMD 崩溃闪退。

用户提供的复现场景如下。

1. 加载 `D:/MMD-Next/tests/MMD ☆★Rou/☆Rou.pmx`。
2. 在时间轴左侧的表示枠列表中展开「表情」。
3. 双击截图标出的 `潰れ1`。

用户报告的结果为崩溃闪退。本轮未在原版 MMD 中重放该操作，报告中的原版版本号尚未记录。

用户将隐藏位置描述为「第 5/0 个」。本例文件中 `潰れ1` 的实际面板值为 `0`，不能据此认定文件值 `5` 是标准面板类别。

<a id="record-hidden-morph-panel-safety-样本核对"></a>
### 样本核对

本轮用仓库内的 [pmxlib.py](../../../tools/pmxlib.py) 只读解析模型，未修改原文件。

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

<a id="record-hidden-morph-panel-safety-面板分类与表示枠"></a>
### 面板分类与表示枠

本地参考 [PMX 文件格式解析](../../../../refs/PMX文件格式解析.md) 的「变形数据」节列出四个操作面板的位置 `1..4`，「表示枠」节则描述独立的骨骼与表情索引列表。该参考文件位于仓库外。

MMS 的 [MorphPanel](../../../include/mikudancestudio/subrecord_layout.hpp) 枚举定义 `system=0`、`eyebrow=1`、`eye=2`、`mouth=3`、`other=4`。[RefreshMorphGroup](../../../src/model/post_load_init.cpp) 按面板值筛选下拉框条目。因此，`panel=0` 不出现在眉、目、口、其他四个操作栏，本例仍显示在时间轴左侧的「表情」表示枠。该面板值也不影响表示枠引用与表情求值数据。

<a id="record-hidden-morph-panel-safety-已有保护与证据范围"></a>
### 已有保护与证据范围

本次运行验收的生产源码基线为 `4e735db85f2e507c7953bddb9ff4c97ba289faf7`（本地 `main`）。

[ui_editor_click.cpp](../../../src/window/ui_editor_click.cpp) 中，`HandleLButtonDown_NameColumnHit` 的表情行分支先按面板选择控件，再计算 `panelIndex = static_cast<unsigned>(panel) - 1`。当 `panelIndex >= 4` 时，程序刷新后返回，跳过 `selectedMorphs[panelIndex]` 访问与后续控件消息。`panel=0` 会命中这个范围保护。该分支处理时间轴名称列的选中，范围检查是选中条目后同步四个操作栏控件时的边界保护，不表示隐藏表情被归入或显示在某个操作栏。

`git blame` 显示该保护已存在于建库快照 `ba6b269`，不是本轮新增的代码。源码中关于原版沿用旧栈句柄的注释属于历史分析，本轮没有独立反汇编原版来确认崩溃根因。

双击消息由 [wndproc.cpp](../../../src/window/wndproc.cpp) 的 `WM_LBUTTONDBLCLK` 分支处理，再进入 [ui_mouse_misc.cpp](../../../src/window/ui_mouse_misc.cpp) 的 `HandleLButtonDblClk`。本次测试发送真实的 `WM_LBUTTONDOWN → WM_LBUTTONUP → WM_LBUTTONDBLCLK → WM_LBUTTONUP`，分别检查第一次单击和完整双击后的状态，覆盖这条完整消息路径。

<a id="record-hidden-morph-panel-safety-mms-目标行为"></a>
### MMS 目标行为

- 隐藏表情出现在时间轴左侧「表情」表示枠中时，单击、双击均安全处理，不越界访问面板状态，也不使程序闪退。
- 保留隐藏表情数据及组合、动作对它的驱动。隐藏属性不改变表情求值。
- 保持四个普通面板的选择、权重显示、滑块和关键帧操作正常。
- 不改写用户模型，不把隐藏表情强行改归四个普通面板。隐藏表情只显示在时间轴左侧「表情」表示枠，不出现在眉、目、口、其他四个操作栏。

<a id="record-hidden-morph-panel-safety-运行验收2026-09-27"></a>
### 运行验收（2026-09-27）

新增 [hidden_morph_tests.cpp](../../../tests/hidden_morph_tests.cpp)，通过真实 `InitMainWindowAndD3D`、PMX 加载器和主窗口消息入口验收。窗口使用 `SW_HIDE`，钩子禁止激活并取消意外模态框；初始化或加载出现模态框视为失败。测试不占用桌面，不保存模型。

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

<a id="record-hidden-morph-panel-safety-复跑专项测试"></a>
### 复跑专项测试

该测试需要单独提供上述 Rou 样本，不包含模型资源，不加入默认 CTest 集。启用 `MIKUDANCESTUDIO_BUILD_UI_TESTS` 后显式构建，再从有正式 `Data` 文件夹、无外部 MME DLL 的运行目录执行：

```powershell
cmake --build build --config Release --target hidden_morph_tests
# 把生成的 build/tests/Release/hidden_morph_tests.exe 及正式 Data 复制到隔离运行目录后：
.\hidden_morph_tests.exe --model 'D:/MMD-Next/tests/MMD ☆★Rou/☆Rou.pmx' --mode contract
.\hidden_morph_tests.exe --model 'D:/MMD-Next/tests/MMD ☆★Rou/☆Rou.pmx' --mode bounds
```

退出码 0 表示该模式全部断言通过；缺失素材、初始化失败、断言失败均返回非 0。测试需要可用的本机 D3D9 环境。

<a id="record-hidden-morph-panel-safety-尚未核实的原版证据"></a>
### 尚未核实的原版证据

尚未固定原版 MMD 的版本和 EXE 哈希，也未独立重现原版崩溃或确认其具体根因。原版缺陷的来源仍是用户报告；这与 MMS 已通过的运行验收分开记录。

<a id="record-hidden-morph-panel-safety-复核入口"></a>
### 复核入口

分类规则见[近期新增特性](../development/features.md#record-additional-features)。在仓库根目录可核对保护的历史来源：

```powershell
git blame -L 757,770 --date=short -- src/window/ui_editor_click.cpp
```

<a id="record-editor-input-fixes"></a>
## 曲线、表情烘焙与局部控制柄修复验收

原始记录：`EDITOR_INPUT_FIXES_20261007.md`。其中的版本、日期和验证范围沿用原记录。

日期：2026-10-07。修复前提交：`556c0b40d0efdbcba5ffc600d331cd132aea0d3d`。本记录随修复一起提交；最终构建提交和文件哈希保存在 `.local/artifacts/editor-input-fixes/`。

<a id="record-editor-input-fixes-行为"></a>
### 行为

- 完整曲线视图的“自由”切线允许左右独立拖动和数值编辑。“对齐”仍联动两侧斜率，保留各自长度。旧 0–127 MMD 插值面板仍遵循自动相邻控制点联动；共用旋转插值通道、撤销和重做保持原有规则。
- 曲线关键帧选择只更新选中状态，不再误设主时间轴的拖拽标志。之前该标志会使后续鼠标移动重新读取骨骼关键帧，覆盖刚编辑的旋转和位移。主时间轴重排现在同时要求真实拖拽处于活动状态。
- 局部 XYZ 工具在处理每帧拖拽时读取当前鼠标位置。主视口和分离视口的旧移动消息不能覆盖居中后的坐标；第一段实际移动仍被计入，同一位置不会重复累加。

<a id="record-editor-input-fixes-同条件回归"></a>
### 同条件回归

使用 VS 2022 x64 Release，后台隐藏窗口，不读取桌面画面。专项命令：

```text
animation_curve_editor_tests.exe --editor-input
morph_curve_host_ui_tests.exe --editor-input
coordinate_axis_tests.exe
window_interaction_tests.exe
animation_curve_tests.exe
channel_bake_tests.exe --morph-capacity
```

| 检查 | 修复前 | 修复后 |
| --- | --- | --- |
| 自动联动开启时拖动完整曲线手柄 | 对侧被改写 | 91 项专项检查通过，包含两侧拖动、数值编辑、对齐、旧 MMD 联动与撤销重做 |
| 表情曲线烘焙及仅表情通道烘焙后编辑骨骼 | 遗留拖拽标志；旋转、位移被鼠标移动覆盖 | 22 项通过，实际写入 21 个烘焙采样；真实时间轴拖拽仍生效 |
| 居中后投递旧鼠标移动消息 | 主视口坐标被旧消息改写 | 主视口、分离视口、首段真实移动、重复采样及既有居中检查通过 |

原始结果在 `before/`、`after/`；修复前测试 EXE 独立保存。不同缺陷分步隔离，不将某一修复中途的构建冒充三项均未修改的统一基线。MMS 表情 A/B 两侧使用相同的完整模型池夹具和无渲染器刷新保护；早期夹具缺少必要池/绘制前置条件导致的异常记录保留，未计入验收。

宽范围 `channel_bake_tests` 在两仓原始提交与修复后均有相同 6 项失败：相机 X 重采样断言、网格菜单位置/三语言顺序、场景说明文字。本次不修改这些行为。原始提交对照的构建命令、源码哈希和 EXE 哈希见 `before/channel-bake.json`，没有把这组测试标记为全通过。

完整 MMS 曲线窗口旧测试仍以 ListBox 消息读取现有树控件，在“读取后保留选择/历史”的标签断言停止；完整表情宿主旧测试另有 4 项箭头布局断言失败。专项测试采用真实编辑与烘焙事件路径且已通过，不以这些旧断言报告为全量通过。MMS 同步 MMN 已有的无渲染器刷新保护，使后台宿主也能安全执行该流程。

<a id="record-editor-input-fixes-运行包"></a>
### 运行包

从本次提交构建 `MikuDanceStudio`，正式运行入口为 `D:/MMD-Next/MMS/build/Release/MikuMikuDance.exe`。本轮不增加运行库或编解码模块。

<a id="record-editor-input-fixes-未验收项目"></a>
### 未验收项目

没有使用用户具体工程执行人工拖拽验收。没有采集播放 FPS、CPU 提交时间或 GPU 性能对照，不声称性能提升；未调整画质、物理或透明排序。
