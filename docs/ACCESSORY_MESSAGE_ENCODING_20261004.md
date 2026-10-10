# 附件错误提示编码修复

原版 MMS 与 MMS-Modular 的附件加载代码均有同一缺陷：CP932 日文常量直接传给 `MessageBoxA`，系统代码页为 936 时显示乱码。此提交只修复原版 MMS 的四个附件提示调用，改用已有的 `LegacyUiMessageBoxA`。该函数解码字符串后调用 `MessageBoxW`。附件加载、物理计算和字体设置没有改变。

## 验证结果

基线为 `3d396da2fcc1587e6004e0d6336d0ec8d6f97973` 的已提交源码，不包含原版工作区中其他未提交改动。两侧使用独立 Release 构建目录、相同 MSVC 工具链与依赖。新增测试实际调用 `LoadAccessoryFile`，通过隐藏的 CBT 探针读取系统提示文字并关闭提示，不创建渲染设备。

| 检查 | 修复前 | 修复后 |
| --- | --- | --- |
| 日文模式下缺失附件、无效附件、255 槽位上限提示 | 乱码，检查失败 | 完整 Unicode 文本，检查通过 |
| 英文与中文模式下上述提示 | 通过 | 通过 |
| 加载失败后附件槽位回滚 | 通过 | 通过 |
| 原有 `ui_message_tests` | 通过 | 通过 |

系统 ANSI 代码页实测为 936。中文模式下这些附件错误仍沿用原有英文回退文本；此次没有补充中文翻译。测试在每个语言模式读取三个实际提示，共九个提示。探针按窗口属性去重，避免同一系统提示重复触发激活回调而误报。

正式测试入口为 `accessory_message_regressions`，超时为 10 秒。原有 `ui_message_tests` 保持独立。

## 构建与证据

修复前 EXE 位于 `D:/MMD-Next/builds/ui-visible-acceptance-20261004/original-before/Release/MikuMikuDance.exe`，SHA-256 为 `de8fe4762dd254634449a5e6f2273f350db2967a457a9eeebb5b39ce0df23065`。

修复后 EXE 位于 `D:/MMD-Next/builds/ui-visible-acceptance-20261004/original-after/Release/MikuMikuDance.exe`，SHA-256 为 `bd98438508ce0eb817a3ba96327f8a496b3ab6ae3ff7f78169a8f8a4d0a2baad`。此构建没有部署到桌面 Release。

来源及二进制哈希见同目录 `accessory-source-manifest.json`。测试日志为 `original-before-final-tests.log` 与 `original-after-final-tests.log`。修复前 CTest 返回 8，符合新增编码回归失败的预期；修复后两个测试均通过，返回 0。

MMV 双臂蝴蝶抖动是独立的物理问题，本提交不声称修复该问题。
