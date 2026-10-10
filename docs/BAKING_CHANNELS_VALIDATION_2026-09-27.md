# 烘焙通道、容量与 MMV 启停验收

本次增加按需增长的骨骼键池、MMS 原生通道选择，以及 MMV 网格筛选和效果实例区间启停键。操作说明见 [物理烘焙](PHYSICS_BAKING_REQUIREMENTS.md)、[网格烘焙](ALEMBIC_EXPORT.md) 和 [存储](MMV_STORAGE.md)。

## 来源与证据

来源为 `e27972b` 加本次烘焙改动，保留之前的视口修改。后续原生骨骼操作面板单独提交。构建使用 Visual Studio x64 / Release，目标为 `MikuDanceStudio`、`mmv_all` 及对应测试。

前版为 `.local/releases/mme-null-after/`，来源 `541ab70`。后版位于 `.local/releases/mmv-bake-after/`：`native`、`mme`、`combined` 隔离无插件、仅 MME、MMV/MME 组合；`ui-compat`、`ui-dx12`、`postprocess` 使用实际宿主核心。

原始记录在 `.local/artifacts/mmv-bake-fixes/`。`build-hashes.json`、`readback-hashes.json`、`final-tests-3.json`、`final-readbacks.json` 保存产物和样本。DSH 的容量、通道、MMV 启停/布局三个子任务均在正式仓库运行，正式结果保留在同目录；主 agent 复核跨模块提交、MME 分配恢复与最终运行结果。

## 已执行验证

| 范围 | 结果 |
| --- | --- |
| 动态骨骼池 | `bone_key_pool_tests`、`bone_registration_tests`、`physics_bake_tests --capacity` 通过。验证超过旧边界的槽位、登记、撤销/重做和 VMD 记录。 |
| 真实 PMM | `测试刚体.pmm`：池增长至 983,040 槽，实际键写入槽 600,000。140 骨骼、45 刚体、27 joint；23 个物理骨骼烘焙，PMM/VMD 往返比较 299/299 已分配骨骼键通过。`run-capacity-scene-final.log`。这不是生成 983,040 个有效键的压力测试。 |
| 稀疏骨骼 | 2..8 帧、步长 3，仅写指定物理骨骼的 2/5/8 帧，每键禁用物理；非法骨骼索引被拒绝。 |
| 原生通道 | `channel_bake_tests` 16 阶段通过：主页/模型页隔离、附件子参数、范围外曲线、选择、容量失败与模型切换。中途失败恢复表情/显示池和帧范围。`channels/run-rollback-final.log`。 |
| MMV 启停 | 451 检查/8 用例、CPU runtime 427 检查/12 用例及两版生产 DLL ABI 测试通过。覆盖 Step 键、区间外恢复、VMM/VEM、作者变量重名和事务失败。 |
| 缓存 | 旧 ABC 和带区间/源顶点映射的新缓存通过；未选顶点保持源动画，退出区间不使用缓存。 |
| 实际渲染组合 | `run-native-integration-1.log`、`run-mme-integration-3.log`、`run-combined-integration-3.log` 通过。覆盖附件变换、局部顶点、PMM 重开、清除缓存和 MME 分配恢复。 |
| 实际参数界面 | 两版 `run-ui-*-final3.log` 为 0 失败：三语、800/560 宽度、参数窗移动/缩放、下拉框打开后 60 个生产帧保持展开、曲线入口。图像来自自身 `WM_PRINT`，未截取桌面。 |
| 分隔线 | `run-editor-sep-4.log`：17 用例，4,269 检查通过。配置和文件按钮下方绘制分隔线，上下各 12 逻辑像素；覆盖三语、最小窗口及实际像素。 |

## 同条件 DX9/MME 回读

使用 Rou PMX、SvSSAO.x 和 `identity_mmv.fx`，相同 640×480 目标、素材、相机、物理与采样方法。前后均为 `EMMAutoSave=true`。测试断言目标包含几何、MMV 发布独立网格、恒等效果保持画面，以及 MME 离屏目标分配可以保存。

采样顺序：前 `before-readback-2`、后 `after-readback-2-first`、前 `before-readback-3`、后 `after-readback-2`。两次后版原先共用标签，第一次在下一次运行前完整复制为 `-first`；原始日志和画面均保留。

| 批次墙钟毫秒 | 前版两次 | 后版两次 |
| --- | --- | --- |
| 分配后 60 帧 | 23,164.656 / 23,039.103 | 23,438.483 / 23,167.900 |
| 重载后 8 帧 | 2,799.823 / 2,779.432 | 2,769.395 / 2,784.950 |
| 编辑器刷新 30 帧 | 10,479.588 / 10,384.408 | 10,367.145 / 10,450.883 |
| 保存后 8 帧 | 2,867.709 / 2,726.286 | 2,859.397 / 2,790.640 |

每次内部 baseline/assigned/reloaded 逐字节相同。前三个样本跨版本也相同；最后一次后版仅 1 个像素的一个 RGB 通道相差 1，平均 RGB 绝对差 0.000001085。没有观察到明确性能变化。上述为包含同步等待的整批墙钟时间，不是 FPS 或 GPU 时间；没有独立测出计算、回读、上传和绘制耗时。

## 失败记录与边界

补充夹具验收：`.local/artifacts/mmv-bake-fixes/run-fixture-extra-persist.log` 与 `run-fixture-varying-persist.log` 均通过真实后台 DX9 的网格缓存、重开工程和 Alembic 往返。测试专用 MMV 夹具增加按对象身份保存的 `.fixture-mmvranges` 侧文件，修复夹具重开时丢失 FX 开关范围的问题；生产插件继续使用 `.vmm`。首次失败结果 `run-fixture-extra-final.log` 保留。

- `before-readback-1` 的 45 秒超时不足，排除；改用 120 秒上限后完成。
- `after-readback-1` 的目录关闭 EMM 自动保存，图像通过而存档检查失败；统一配置后重新交替采样。
- `ui-compat-final` 因旧 PowerShell 错读 UTF-8 脚本损坏素材路径，改用 PowerShell 7。`final2` 的测试错误区分 Win32 类名大小写，修正后得到有效的 `final3`。
- 初版分隔线子控件未在 `WM_PRINT` 绘制，最终由父窗口统一绘制屏幕和离屏图。失败日志保留。
- 增长的是 MMS 骨骼键池，不改变原版 MMD 的内存限制；表情、显示及部分场景轨道仍有既有容量，超限拒绝提交。
- 原生物理输出骨骼。当前 MMV Bullet3 输出变形网格，须用网格缓存，未实现任意网格反解骨骼动作。
- 通道支持准备、失败恢复和取消，尚未把新通道做成统一长期撤销历史。未做分配器故障注入；异常恢复经代码审查及中途容量失败用例验证。
