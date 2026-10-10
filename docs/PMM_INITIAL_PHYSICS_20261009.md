# PMM 初始帧关闭物理的加载修复

打开 `D:/MMD-Next/tests/ToonGlass_Ray/UserFile/Stage.pmm` 时，MMS 已读到
160 根骨骼的关闭物理标记，但对应的 160 个刚体仍处于动态模式。
MMD 9.26 在同一工程、同一第 0 帧下的动态刚体数为 0。

## 原因和修改

PMM v2 加载器和物理菜单的四个入口，把原版 `0x4B2210` 的模式应用调用误接成了
`ModelKinematicSync`（`0x4B22F0`）。后者只同步已有运动学刚体的位置，不会切换
刚体模式或 Bullet 的 `CF_KINEMATIC_OBJECT` 标记。

恢复的 `bone.physicsDisabled` 已经等于初始关键帧值，后续跳帧也不会触发标记变化通知。
因此仅恢复骨骼标记不能弥补首次加载遗漏的刚体更新。

修复复用已有 `ModelBonePhysicsModeApply`，在读完工程全局物理模式后应用骨骼标记，
并接回物理菜单的四个入口。保留原有语义：全局关闭时切到运动学模式；始终计算时
启用动态模式；其余模式遵守骨骼关闭标记。帧循环中的位置同步保持原状。

## 验证

使用独立、隐藏的进程副本，原始 PMM 和 EMM 未改动。前后正式程序均为
VS 2022 x64 Release，`MIKUDANCESTUDIO_PERF=OFF`、`MIKUDANCESTUDIO_DIAG=OFF`。
相同 D3D9/MME 运行文件和配置，不加载可选 MMV4MMS；视口均为 800 × 450。

| 情况 | 修复前动态刚体数 | 修复后动态刚体数 |
| --- | ---: | ---: |
| 初次加载，第 0 帧 | 160 | 0 |
| 切到始终计算 | 160 | 160 |
| 切回遵守骨骼开关 | 160 | 0 |
| 切到播放模式 | 160 | 0 |
| 全局关闭 | 160 | 0 |
| 从始终计算状态重新加载 PMM | 160 | 0 |

`physics_scene_tests <Stage.pmm> --physics-modes` 同时检查刚体模式和 Bullet 标记。
修复前上述流程有 6 处失败，修复后为 0。补充的内存关键帧测试在一根骨骼的
第 15 帧开启、第 30 帧关闭物理，跳转 0 → 15 → 30 → 0 时动态刚体数为
0 → 1 → 0 → 0；测试不保存工程。

正式 EXE 的 CPU 和自动蒙皮配置均实测初始动态刚体数为 0，与 9.26 一致。
角色全部骨骼的初始位移、旋转及世界位置与修复前及 9.26 的差值均为 0。
`physics_pose_tests`、`physics_scale_tests` 均通过，`git diff --check` 通过。

另外保留每秒一次、共 6 次的静止预览原生 FPS 读数：修复前 CPU 为
33/33/31/31/32/34，修复后 CPU 为 31/38/33/33/35/39，修复后自动为
33/40/44/45/43/43，原版 9.26 为 39/38/39/37/39/39。
这是后台短时预览检查，不是动作播放、GPU 耗时或长期稳定性能验收，不能据此承诺性能提升。

## 版本与保留记录

- 修复前源码：`162250d4f23242bdc7a107a5661c67baab1609fa`；生产源码干净，回归测试单独加入。
- 修复后源码：本提交；测试时基于上述提交，修改声明、PMM 加载入口、物理菜单入口和回归测试。
- 修复前 EXE SHA-256：`a8ac6e94aae8731d08e71985ccf3cf2f4af07620e8e83e85a75b35f82abc13eb`。
- 修复后 EXE SHA-256：`79227e91cfabbc3fa6669d576869a0f41ba71822608bc80dbbd9d75ddeeaab1d`。
- 原版 9.26 EXE SHA-256：`72c34b7ab607aef7b18974742ae18ce62f115f6a614bbedc1a4310e4a5406f72`。
- PMM SHA-256：`44041f9a1b88d2d2f10202e089a3b036a83cd0872e0cf1c83221d112de2986ba`。
- EMM SHA-256：`a8231a3651aa6f0ac592ce1ae7a157bc32a120c239a1671ce55fc7d061ee0f67`。
- 发布程序：`D:/MMD-Next/MMS/build/Release/MikuMikuDance.exe`。
- 旧 EXE、MAP、PDB：`.local/archives/pmm-physics-initial/`，保留用于复查。
- 构建日志、原始内存状态、DLL 哈希、私有运行目录、前后测试结果：
  `.local/artifacts/pmm-physics-initial/`。
- 隐藏进程读取脚本：`.local/probe_pmm_physics_initial.py`；集成测试运行脚本：
  `.local/run_pmm_physics_test.py`。这些本地诊断文件不纳入发布包。

构建命令：`cmake --build build --config Release --target MikuDanceStudio physics_scene_tests physics_pose_tests physics_scale_tests --parallel 4 -- /nodeReuse:false`。
