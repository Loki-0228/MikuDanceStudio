# 相机视角与自阴影参数回显

换帧后，相机视角和自阴影参数从已经计算的实时值同步到输入框、滑块和阴影模式按钮。相机视角输入框保留过渡帧的小数值，滑块取整不会回写实时参数或关键帧。

`ReloadModels` 在原生相机插值与曲线覆盖之后刷新视角。`RefreshSelfShadowPanel` 在当前帧晚于末关键帧时保持末关键帧，不再退回其前一帧。原生阴影关键帧继续沿用阶梯变化；已编写的连续阴影曲线按实际插值回显。

播放的最终求值步骤直接刷新实时参数，不重新按整数 `CurrentFrame` 求值。几何导出的独立采样入口不新增 UI 操作。只有显示值改变时才写入控件。

## 后台对照

两侧使用 Release/x64、相同隐藏窗口、相同登记数据和同一测试源码。命令为 `runtime_layout_tests.exe --timeline-readouts`，没有显示窗口或执行 GPU 测试。

| 项目 | 修复前 | 修复后 |
| --- | --- | --- |
| 基础提交 | `f693a0f32529c5123abb3fbee86d9a390253ec12` | 同一提交加本项补丁 |
| 定向检查 | 43 项，18 项失败 | 43 项全部通过 |
| 测试 EXE SHA256 | `535f24e63f70a4540cd6a2a27b9a4c7b92a894f16afa859a1ae60c0dd028dc61` | `4671a2473d8b3bbc297422d35ace24964a46a5e732b93fbd98c0c925094b3e4e` |

检查涵盖真实相机和阴影登记、正反向跳转、插值小数视角、末帧以后、连续阴影曲线、播放游标与整数帧不同步时的回显，以及关键帧数据不被改写。修复后同一 EXE 的完整 `runtime_layout_tests` 也通过。

原始构建和测试日志、命令、Git 状态、源码与 EXE 哈希位于 `.local/artifacts/timeline-parameter-readouts/before-final/` 和 `after/`。`before/` 保留测试编译期间遇到 Windows `min` 宏冲突的失败记录，未计入行为对照。构建目录为 `D:\MMD-Next\MMS\build`，这些本地记录不入库。

未在用户工程中手动验收。播放 FPS、CPU 提交时间和 GPU 时间未测，性能对照未验收。

正式入口为 `D:/MMD-Next/MMS/build/Release/MikuMikuDance.exe`。提交后的宿主构建来源和 EXE 哈希写入 `.local/artifacts/timeline-parameter-readouts/release.json`。
