# MMS 原生骨骼工具与曲线编辑验收

本轮修改属于 MMS 宿主。骨骼工具、表情曲线、附件与场景曲线均不要求安装 MMV。共享曲线窗口也会随两种 MMV DLL 一起构建。

## 保留的前版

- 宿主快照：`.local/releases/native-bone-before/`，来源 `80bc305`。EXE SHA256：`5C5B54DC8C14219F3CAD787F26743337DF2B32A2D0FD725394D1F9BC2CDEA857`。
- 同条件 MME 测试宿主：`.local/releases/native-editors-before-readback/`，从上一轮已验收的 `mmv-bake-after/postprocess` 保留。EXE SHA256：`115571C413A0B2A6C1A10DF459EAEB6FC6E5A11F788E53F1AF146FF2D8203857`。
- 哈希记录：`.local/artifacts/native-bone-tools/before-binaries.json`。源模型、效果文件未修改。

## 阶段验收记录

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

## 原生功能后续验证与 MMV 交接

- `run-ui-5.log` 退出 0：实际宿主的四组表情入口、三语比较模式、VMD 曲线写入与读回，以及 7.5 帧采样一致性通过。产物在 `ui-5/`。此前 `ui-3`、`ui-4` 的失败记录保留，不计为通过。
- `build-native-handoff-2.log` 构建通过：原生帧率后端、帧率窗口、总事务控制器及缩放 IO 测试。第一次真实核心链接暴露 `CaptureCurveTable`、`SwapCurveTable` 被放进 `gizmo` 命名空间，修正后重新构建。
- `handoff-verified-tests.log`：`animation_retime_tests`、`frame_rate_dialog_tests`、`frame_rate_correction_tests`、`bone_scale_io_tests`、`mesh_bake_cache` 五个目标全部通过。缩放 IO 包括恒等矩阵端点的非线性曲线保存。
- 帧率窗口在隐藏窗口中验证了三语控件边界、输入框内边距、预览、应用和完整报告。此环境的隐藏原生 `EDIT` 控件不会把文字绘入 `WM_PRINT` 图像；图像本身不能证明输入文字可见性，控件文字、矩形和样式另行断言。没有补画文字冒充窗口回读。

`native-final-tests.log` 中 13 个相关目标通过，包括真实原生帧率后端、作者曲线、缩放矩阵与 IO、曲线窗口、网格缓存。`frame_rate_correction_tests` 的依赖使用测试替身，通过不能替代实际 MMV 的事务集成。

按用户要求，MMV 新分层架构、存档和分配界面交由「设置 MMV Bullet3 物理解算」会话主导。本会话停止重叠的 MMV 子任务，交接了 retime ABI 和全局“应用／关闭回退”要求；原生 MMS 文件独立继续。MMV 运行时事务、顶部层页签和全局应用尚不能据此标为完成。交接正式结果保存在同目录 `handoff-*.json`。

DSH 原生子任务曾因 `QUOTA: Insufficient Balance` 中断；用户充值后，新任务 `3111e75e-1383-435e-82cf-e4b77d5c084b` 在正式工作目录继续，最终限定只修改原生帧率测试。正式结果为 completed，主 agent 随后编译复核并修正生产代码及用例。未重启 DSH 宿主。

## 最终原生验收

- `native-handoff-7.log` 与 `native-final-tests.log`：无帧合并时骨骼 16 字节及相机 24 字节补间保持；合法交叉手柄、相同端点均可合并。缩放的九个矩阵分量按原曲线采样，合并槽的矩阵和曲线清除。失败回滚恢复矩阵、曲线和 revision，可重试。
- 原生 `.x` 作者曲线在不安装 MMV 时可修正。验证了非线性采样、回滚、编辑器外登记后的只读同步，以及合并后的标准记录值与曲线扩展一致。预览后增删对象、修改显示/IK 数据、新建缩放或作者曲线均使旧预览失效。
- `ui-final/run.log` 退出 0：真实 Rou PMX、三语布局、原生缩放手柄、四组表情入口和 VMD 往返通过。实际菜单执行帧率修正，自动打开一次另存为；取消保存后动画和完整逐帧报告保留。界面来自 `WM_PRINT`，画面来自 DX9 回读。
- 「基准」已改为「缩放中心」。`wording-final-tests.log` 的帧率窗口测试通过。曾尝试把“单一维度”解释为时间轴，已根据用户澄清撤回；该词指输出标量的维度。用户最终选择多次普通一维补间组合向量，不加入空间轨迹联合拟合。
- `after-render-1/run.log`：使用与前版相同的 Rou、SvSSAO、MMV 恒等效果及相同 DLL，真实 MME 的加载、分配、重载和保存通过。`mme-image-comparison.json` 中 baseline、assigned、reloaded 三张 640×480 RGB 回读均逐像素一致。
- 渲染原始阶段耗时保存在前后日志。assigned 的 60 帧阶段总墙钟时间为前版 24112.415 ms、后版 23435.630 ms；重载后的 30 帧为前版 10613.645 ms、后版 10796.154 ms。这只是各一次样本，未分离 CPU/GPU/等待时间，也未完成多轮波动采样，不据此宣称性能提升或播放 FPS。
- 已按用户要求覆盖 `build/Release/MikuMikuDance.exe`。运行中的旧 EXE 移至 `.local/archives/native-editors/overwrite-20260927-152953/`，没有结束用户进程；需要重启应用才使用新版。安装哈希与备份路径见 `overwrite-production.json`。

本次没有覆盖 MMV DLL。新版 MMV 的分层架构和运行时事务仍由协作会话整合，本报告仅验收 MMS 原生部分及与既有 MMV/MME 的兼容回读。

## 排除的初次结果

- `build-gizmo-1.log`：新测试将三个宿主函数误声明为全局函数，产生链接错误；生产函数在 `mikudancestudio` 命名空间。修复测试声明后重新构建。
- `gizmo-cpu-1/2.log`：新测试错误要求第四行物理按钮与前几行有完全相同的尺寸/行距；保留原第四行布局，修正断言。
- `build-gizmo-2.log`：新集成测试链接生成资源时用了相对路径，改为仓库绝对路径。
- `run-ui-1.log`：合成 `WM_COMMAND` 没有触发自动选择按钮的按下状态，且未解析多重采样目标便回读。改为真实 `BM_CLICK` 和 `StretchRect` 解析后得到 `run-ui-2`。没有把失败画面计为有效渲染结果。
- `build-gizmo-3.log`：表情测试两处将 `animation_curve::NamedTrack` 写成错误命名空间；修正后 127 项通过。
- `build-gizmo-4.log`：共享编辑器正在由 DSH 改写三段控件，读到了未完成的源文件。随后单独用已构建的生产库验收手柄；最终构建须等各模块停止写入。

## 当前边界

原生骨骼移动和旋转保持原有输入路径。视口缩放轴沿实际投影方向拖动；白框沿视图平面对角方向取一个倍率，同时作用于视图平面的两轴。右下角缩放 HUD 排仍沿用原有纵向拖动规则。

表情曲线浮窗具有自己的撤销和重做。宿主原有表情键操作没有骨骼键那样的原生撤销环；本轮不能将两者混为一谈。新增的曲线状态恢复和保存只应记录实际已验证的行为。
