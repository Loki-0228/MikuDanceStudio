# v1.0.1 验证记录

2026-10-18，在 Windows x64、175% 显示缩放下验证。

## 启动器与界面

- 启动器标题、路径标签、说明、按钮和错误提示固定为英文。
- 检查所有控件均在客户区内，「Start MMS」与「Close」之间有间距，说明文字与按钮不重叠。已核对窗口截图。
- 复现旧启动器在挂起进程中读取模块列表时返回 `ERROR_PARTIAL_COPY`，随后无法加载插件的问题。新启动器先恢复进程，再等待模块列表就绪，实际加载验证通过。

## MMS 内运行

使用已发布的 `v2026.10.17.1` 完整版 EXE 的隔离副本，未重新编译或替换宿主。测试前后 EXE 的 SHA-256 均为：

```text
d13bf334519007e7be4820af56b63a8b751b0468343f0dc5415efdbd3816f783
```

- MMS 中文、英文和日文模式均保持英文启动器。插件菜单、修复窗口和报告跟随 MMS 语言。
- 三种模式均核对了实际加载的 `MMSCurveRepair.dll` 路径、菜单文字和菜单命令。
- 三种模式均完成中文及日文路径 PMM 的扫描、保存副本、EMM 配套复制和正常退出。样本第 44 帧的 X 值从 `-5.58748070034e-08` 恢复为 `0`，原工程哈希保持不变。
- 英文模式另完成修复副本在全新 MMS 进程中的重开，正常退出。
- `repair_core` 回归通过，共 3,174 项检查。

运行记录位于本地 `.local/artifacts/curve-repair-ui-20261018/`：中文记录为 `popup-zh-r2/runtime-verification.json`，英文和日文记录分别为 `final-en/runtime-verification.json`、`final-ja/runtime-verification.json`。核心测试记录为 `.local/builds/curve-repair/Testing/Temporary/LastTest.log`。

本次未修改曲线恢复算法。恢复条件与不支持的格式见 `README.md`。
