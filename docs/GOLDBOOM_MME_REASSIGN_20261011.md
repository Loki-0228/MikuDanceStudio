# GoldBoom 原生 MME 同名效果重分配诊断

用户确认从 MMS 的 MME 效果分配窗口，给 PMX 重新指定同一个 FX。本次未复现画面异常，未修改生产渲染代码，也没有完成用户问题的修复验收。

## 覆盖的操作

诊断目标 `mme_reassign_tests` 链接 MMS 生产核心，并加载 `refs/MME` 的真实 `d3d9.dll`、`MMEffect.dll` 和 `MMHack.dll`。素材复制自 `D:/MMD-Next/tests/mme/GoldBoomEX`，避免更改原文件或分配记录。

程序依次加载 GoldBoom.pmx 的伴随效果，在 MME 原生列表中选择 PMX 根行，通过设置效果命令重新选择 GoldBoom.fx，关闭窗口，执行全部重载，再次分配。补充采样包括效果窗口仍打开时完成 1 次和 8 次显式渲染循环后的画面，以及关闭窗口后完成 1 次循环的画面。消息队列中的 WM_PAINT 也可能触发绘制，因此这些采样不能严格证明覆盖实际首帧。

每次回读实际 640×480 渲染目标，保存 BGRA8 文件。程序输出 RGB 差异但不把图像相同当作内置断言；外部清单同时比较完整 BGRA 文件哈希。真实 PMX 的已加载骨骼表没有 `アニメ制御`，日志为 `controller=-1`，所以这些是默认静止姿态的检查，不能证明所有动画帧正确。

## 当前结果

- GoldBoom 的自阴影 0 和 2：最终隐藏窗口测试全部完成，同模式的 7 张 BGRA 图像哈希相同。
- GoldBoomDiffuse 的自阴影 2：最终隐藏窗口测试完成，7 张 BGRA 图像哈希相同。
- 纯绿色 FX 正对照：重新分配使 155,848 个像素改变，确认分配命令进入了实际绘制。
- 真实 Windows 文件框也曾返回同一个 FX，画面未出现差异，但该轮隔离声明无效，限制见下文。

源码分析确认 MME 在第一次 `BeginScene` 才注入菜单。因此诊断需要先渲染空场景，再关闭分配文件自动保存。原生设置效果命令经过外部 MMEffect.dll；本次没有发现宿主按手动和自动分配分别处理 GoldBoom 的确证。

MMV4MMS 先前提交 `4f21ff2` 修复的是 `ANIMATEDTEXTURE.SeekVariable` 读取 float3 的兼容错误，属于独立问题，不能作为这次原生 MME 操作异常的解决结论。

## 后台环境与证据限制

初版 Python 启动器给 `subprocess.STARTUPINFO` 动态设置 `lpDesktop`，但该字段未随 `STARTUPINFO.copy()` 传给系统。因此早期 manifest 中的 `private_desktop:true` 不成立。那些运行使用隐藏窗口钩子；它们不是已验证的私有桌面运行。早期原始日志和错误元数据保留，由追加清单标出更正。

改用 `win32process.CreateProcess` 后，被测程序确认实际桌面名称为 `MMS_REPAIR_...`，随后 D3D9 创建设备失败。此机的私有桌面 GPU 路径未通过，不能把该轮当作渲染验证。最终用例只允许显式选择隐藏窗口模式，并在此模式禁止真实文件框；真实文件框测试仍强制要求私有桌面。

最终隐藏窗口检查注入文件选择结果，模拟目录变化，真实执行 MME 列表选择、分配、编译与绘制。它不能完整替代用户现场的系统文件选择过程。全部重载使用 MME 命令 40004；用户现场恢复正常的具体重载操作仍未确认。实际运行 EXE 的路径和错误画面也未取得。

这次没有生产修复，不报告性能收益，也没有以单版本诊断替代修复前后性能验收。

## 本地记录

源码基线为 `4425ec88f82c97a4ba1453a94663f3a92e0391db`。VS 2022、x64、Release，诊断和性能宏关闭，DX12 编译选项开启。具体构建命令、源码状态、EXE 与 DLL 哈希、素材哈希、完整原始日志和失败样本记录在 [验收清单](../.local/artifacts/goldboom-mme-20261011/01/validation-manifest.json)。

相关目录保留如下：

- `.local/worktrees/goldboom-mme-before`：基线生产源码，加同一诊断用例。
- `.local/worktrees/goldboom-mme-fix`：诊断分支，没有生产代码修复。
- `.local/builds/goldboom-mme-before`：独立生产 EXE 和诊断目标。
- `.local/artifacts/goldboom-mme-20261011/01`：命令脚本、运行环境副本、图像和日志。

没有替换正式 `build/Release` 的 EXE 或 DLL，没有推送远端。
