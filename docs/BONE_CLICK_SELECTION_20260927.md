# 普通点击骨骼选择修复

物理计算开启后，无刚体的普通骨骼曾被点击选择逻辑误判为物理锁定。框选没有这项错误限制，因此出现框选正常、普通点击无效。

`BoneCanBePicked` 现在按 `hasRigidBody` 判断物理锁定，与骨骼标记的显示规则一致。无刚体的可见普通骨骼在各物理模式下均可点击。有刚体的骨骼继续遵循对应模式及 `physicsDisabled` 标志，不解除物理接管限制。

同一隐藏窗口测试在修复前失败于「模式 1 下点击没有刚体的普通骨骼」，修复后整套注册、点击和框选回归通过。框选测试经测试入口驱动原有按下、移动和释放逻辑，不伪造桌面按键或前台窗口。

两侧均为 VS 2022 x64 Release。基线采用 `.local/worktrees/mms-stutter-profile/` 中修复前的 `frame_modes.cpp`，仅同步不改变生产行为的框选测试入口；候选采用正式 `main` 工作区。记录位于 `.local/artifacts/global-processor/bone-registration-before.log`、`bone-registration-integration.log`，构建记录为 `menu-help/baseline-tests-build-seam.log` 和 `integration-build-20260927.log`。该对照验证输入行为，不代表播放性能测量。
