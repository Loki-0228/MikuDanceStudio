# 表情曲线按钮滚轮切换验收

鼠标位于表情操作栏的曲线按钮时，向下滚动选择该栏的下一个表情，向上滚动选择上一个表情。四栏均使用原生下拉框处理及 `CBN_SELCHANGE` 通知，列表边界、其他栏的选择和键盘焦点保持原有行为。

父窗口根据滚轮消息的屏幕坐标判断命中的按钮，转发到同栏下拉框并消费消息。禁用按钮、禁用选择器、空列表和零增量不改变选择。Ctrl、Shift 滚轮不转发，因为原生下拉框会把这类消息退回父窗口，直接转发会递归。

## 后台对照

测试命令为 `morph_curve_host_ui_tests.exe --morph-wheel`。两侧使用相同测试源码、四个隐藏按钮、每栏三项表情及相同输入序列；没有显示窗口或移动系统鼠标。

| 项目 | 修复前 | 修复后 |
| --- | --- | --- |
| 基础提交 | `1c20f04008cf3ffff8451a5ace19965884dce139` | `1c20f04008cf3ffff8451a5ace19965884dce139` 加本项补丁 |
| 断言数 | 57 | 57 |
| 失败数 | 42 | 0 |
| 测试 EXE SHA256 | `e1f5e99b0eea6a892b0ee89d1066496fcdcdc91f04831b72f1ca45efee1c6058` | `b73889d93ef973f3360b13cc018c6023166c76c372ea063d4db79259611e66bc` |

失败断言包含前序选择失败产生的后续断言，不表示 42 个独立缺陷。验证覆盖按钮收到滚轮和父窗口收到滚轮两条路径、四栏正反方向、首尾边界、禁用控件、零增量、Ctrl/Shift、原生通知次数、其他表情栏、焦点与区域外原有行为。

构建目录为 `D:\MMD-Next\MMS\build`，配置为 Release/x64。`.local/artifacts/morph-launcher-wheel/before-final/` 和 `after-final/` 分别保留构建命令、Git 状态、生产源码与测试文件哈希、EXE 哈希、补丁和原始输出；较早的 `before/`、`after/` 为开发阶段记录。`.local/` 不入库。

## 验收范围

本次只验证控件输入，不运行 GPU、播放或用户工程。播放 FPS、CPU 提交时间和 GPU 时间未测，性能对照未验收。

MMS 正式入口为 `D:/MMD-Next/MMS/build/Release/MikuMikuDance.exe`。本项提交后构建宿主，最终来源与哈希写入 `.local/artifacts/morph-launcher-wheel/release.json`。
