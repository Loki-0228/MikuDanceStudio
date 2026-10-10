# PMX joint 校验与物理姿态安全（2026-09-25）

修复分支 `fix/physics-load-safety`，基线提交 `7523928`，对照工作树 `.local/worktrees/physics-load-before`（同一基线的未修改检出）。用户提出两项需求：原版 PMX 读取遇到未连接刚体 A 或刚体 B 的 joint 会闪退，改为提示用户后继续加载，并把错误 joint 无效化；手动把刚体注册到科学计数法显示范围的位置后，刚体状态算错，改为保持可计算。

## 最终行为

**joint 端点缺失。** joint 记录里的刚体 A 或刚体 B 不可用（索引越界、刚体表为空、刚体未创建或刚体对象为空）时，模型继续加载，只把该 joint 无效化：`JointRecord::constraint` 置为 `-1`，求解器的关节超伸钳制与物理编辑器都跳过它。无效化前该字段留 `0`，而 0 是合法约束号、可能属于其他已加载模型，关闭模型时会误删别人的首个约束，所以统一改用 `-1` 标记禁用。joint 槽位在文本读取失败前就全部初始化为禁用态，避免半张表留下可被误认的记录。PMX 在刚体表为空时仍会写 joint 计数，因此 joint 表在 `rigidCount == 0` 时也照读。

加载完成后弹一次警告框，按界面语言分中文、日文、英文三种文案，标题分别为「模型物理警告」「モデルの物理警告」「Model physics warning」，正文逐条列出 joint 原始编号、名称与缺失的刚体 A / 刚体 B。编号从 0 开始，取文件里的原始下标，不随重命名或排序变化；英文界面用英文名，其余界面用原名。无属主窗口的导入方不弹框，可调用同一函数取得文本。原始测试文件 `tests/UserFile/测试刚体.pmm` 未改动。

**超大坐标与无效姿态。** 骨骼位置或旋转出现 NaN / Inf 时，后续帧会把污染扩散到整副骨架与刚体。修复分三层：PMM v1 / v2 读取与 `RegisterBonePoseAtFrame` 登记骨骼姿态时，把非有限的位置与旋转修复为可用值（位置取当前骨骼值或零，旋转取当前旋转或单位四元数），正常的非单位四元数原样保留、不自动归一化；物理回写后若刚体的变换或速度出现非有限值，用跟随矩阵把该刚体复位并清零速度，只修受损的刚体，正常模拟的写入集合不变；关节超伸钳制里两刚体间距的平方在单精度下会溢出为无穷，改用双精度算回距离再夹到半径界，半径界本身非法时跳过该 joint。

## 改动清单

| 文件 | 改动 |
| --- | --- |
| `include/mikudancestudio/pmx_joint_validation.hpp` | 新增。端点缺失判定、三语警告文本与标题。 |
| `include/mikudancestudio/bone_pose_safety.hpp` | 新增。姿态修复入口，供 PMM 读取、登记与物理回写共用。 |
| `src/model/pmx_load.cpp` | joint 表预置禁用态；joint 创建条件改为端点完整；`rigidCount == 0` 时仍读 joint 表；加载完成后弹三语警告；刚体变换前检查物理场景与世界对象。 |
| `src/window/physics_model_dialog.cpp` | 物理编辑器跳过无效 joint，列表序号与原始下标分离，首项取第一个有效 joint。 |
| `src/physics/physics_frame.cpp` | 非有限刚体复位；同步与重置前检查跟随矩阵；关节距离溢出时按双精度钳制；回写姿态前修复；无效 joint 不进钳制。 |
| `src/model/model_keyframe_edit.cpp` | 登记骨骼姿态前修复非有限值，记录与骨骼当前姿态取同一份修复结果。 |
| `src/io/pmm_io_common.hpp`、`src/io/pmm_load_v1.cpp`、`src/io/pmm_load_v2.cpp` | 读取骨骼关键帧与模型状态时修复非有限姿态。 |
| `tests/pmx_joint_tests.cpp` | 新增回归：端点缺失、越界、零刚体、无效刚体、三语提示框、关闭模型保留其他模型约束。 |
| `tests/physics_pose_tests.cpp` | 新增回归：样例 PMM 的无效关键帧、非单位四元数保留、登记防护、超大关节距离、刚体复位。 |
| `tests/physics_scene_tests.cpp` | 新增手动对照：传入真实 PMM / PMD 推进物理并逐帧断言姿态有限。 |
| `tests/README.md` | 登记上面三个测试。 |
| `CMakeLists.txt` | 注册 `physics_pose_tests`、`pmx_joint_tests` 到 CTest，`physics_scene_tests` 作为手动目标。 |

## 修复前后对照

两侧同条件、同一场景 `tests/UserFile/测试刚体.pmm`（引用 `tests/UserFile/Model/Miku_Hatsune_Ver2.pmd`，140 骨骼、45 刚体、27 joint），原始输出在 `.local/artifacts/physics-load-safety/`。

| 检查 | 修复前（`physics-load-before`） | 修复后 |
| --- | --- | --- |
| 姿态回归（`before-pose.log` / `after-pose.log`） | 20 项检查失败；样例 184 条关键帧中 23 条含非有限数值 | 0 项失败；184 条关键帧全部有限 |
| 完整场景（`before-scene.log` / `after-scene.log`） | 第 10 帧 32 个骨骼、26 个刚体异常 | 第 0、10、14、15、16、24、25、26 帧与反复跳转全部 0 异常 |
| joint 回归（`before-joints.log` / `after-joints.log`） | `pmx_joint_failures=27` | `pmx_joint_failures=0`，`warning_dialogs=3`（中日英各一次） |

joint 回归的对照做法是把同一份测试源码编进修复前的工作树再跑一遍，失败项逐条对应约束号、关闭模型回收与零刚体读取。三语提示框回归核对了中文、日文、英文的实际标题与正文，并在显示前自动关闭。

## 性能对照

同一场景、CPU 蒙皮、1280×720、预热 30 帧、采样 120 帧，两侧交替采样（`perf-0-before.json`、`perf-1-after.json`、`perf-2-after.json`、`perf-3-before.json`，清单 `performance-manifest.json`）。GPU 排空批量耗时修复前 0.560 与 0.597 毫秒/帧，修复后 0.575 与 0.651 毫秒/帧；两侧区间重叠，波动幅度大于前后差，未观察到稳定回归。这些是排空批量吞吐率，不是窗口 FPS。两侧画面校验各 150 帧全部一致（`frames_mismatched=0`）。

## 回归命令

```text
cmake --build build --config Release --target pmx_joint_tests physics_pose_tests pmx_text_tests bone_registration_tests
ctest --test-dir build -C Release -R "physics_pose_tests|pmx_joint_tests|pmx_text_regressions|bone_registration_tests"
```

修复后的结果是 4 项全部通过。`physics_scene_tests` 不注册进 CTest，手动运行时传入 PMM 或 PMD 路径。

## 整合记录

基线提交 `7523928` 的 `CMakeLists.txt` 引用了当时未提交的 `src/io/pmm_extension.cpp` 与 `src/io/pmm_project_settings.cpp`，`src/window/skinning_menu.cpp` 调用了 `pmm_settings::ProcessorChanged`；这三个符号属于 PMM 便携设置功能，到 `58f4ea1` 才正式入库。独立检出的 `physics-load-safety` 工作树没有这些文件，本地用一个 `chore` 提交 `adaa949` 去掉了这两处引用才得以配置构建。

分支已合入 `main`，合并提交 `4409790`。`tests/README.md` 有一处合并冲突，保留 `main` 的 GPU 蒙皮两行并追加 `physics_scene_tests` 行。合并结果相对 `58f4ea1` 只含本次物理修复的 15 个文件（+730/−25），用 `git diff --stat 58f4ea1 main` 可复核：`src/window/skinning_menu.cpp` 回到 `main` 版本，`pmm_settings::ProcessorChanged(*app);` 保留；`mikudancestudio_core` 源清单里的 `src/io/pmm_extension.cpp` 与 `src/io/pmm_project_settings.cpp` 保留。合并时若不手工还原这两处，Git 会按分支侧自动删掉它们，属于必须逐项检查的静默回归。

合并后在 `main` 全量构建并运行 `ctest --test-dir build -C Release`：29 项注册测试通过 28 项，`model_alpha_occlusion_regressions` 失败。该失败是验收基线的既有状态，与本次改动无关：失败的 17 个像素全部来自 `dbf189f` 新增的大材质与阴影 alpha 遮罩夹具，[全局排序实验记录](TRANSPARENCY_GLOBAL_SORT_2026-09-25.md) 已写明基线提交同样失败 17 个像素，只有因性能被否决的全量排序实验 `7c92ca3` 通过这些样本。本次修复不触及渲染代码，改动集中在 PMX 读取、PMM 姿态读写、骨骼登记与物理回写，提交前的定向回归（`physics_pose_tests`、`pmx_joint_tests`、`pmx_text_regressions`、`bone_registration_tests`）全部通过。
