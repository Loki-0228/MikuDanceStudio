# MMV 加载 Stage 的 EMM 修复

宿主现在向 MMV 提供已加载但隐藏的模型，并通过独立的可见性字段控制绘制。此前 `ray_controller.pmx` 被过滤，MMV 导入 `Stage.emm` 时找不到该控制器。修复只涉及 `src/mmv4mms/renderer.inl`，不更改宿主骨骼、morph 或物理数据。

配套的 MMV4MMS 与 MMV4MMN 修复了 EMM 解析、效果发现、映射容量、权重副本、Ray 脚本状态及多后处理串联。详细记录见 [插件修复记录](../../MMV4MMS/docs/EMM_IMPORT_REPAIR_20261009.md)。

## 验证记录

用户指定输入为 `D:/MMD-Next/tests/ToonGlass_Ray/UserFile/Stage.pmm`。工程副本与原生 MME 在后台对照，结果保存在 `.local/artifacts/emm-import-20261009/`：

| 目录 | 结果 |
| --- | --- |
| `before` | 原插件复现权重和 `1.001076` 导致整帧被拒绝。 |
| `after` | 初步导入修复复现隐藏 `ray_controller` 缺失。 |
| `native` | 原生 MME 完成加载及截图。 |
| `after-2` | 19 个对象、328 条映射导入成功，但画面为黑色轮廓。 |
| `after-3` | Ray 着色恢复，后续效果仍覆盖为黑色。 |
| `after-4` | 完整后处理链恢复人物、场景颜色和景深。含临时逐阶段贴图诊断。 |
| `after-final` | 当前生产核心库与无诊断代码的正式 DLL 再次完成导入、VMM 保存和绘制。 |

`comparison-manifest.json` 记录源工程、比较程序、DLL、日志与截图的 SHA-256。诊断宿主基于已构建的生产核心库，宿主修复通过独立编译桥接对象接入；不混入当时其他会话的未提交源码。正式宿主随后由当前生产源码独立构建。

原始文件 SHA-256：

- `Stage.pmm`：`44041f9a1b88d2d2f10202e089a3b036a83cd0872e0cf1c83221d112de2986ba`
- `Stage.emm`：`a8231a3651aa6f0ac592ce1ae7a157bc32a120c239a1671ce55fc7d061ee0f67`

与原生 MME 的云朵边缘采样仍有差异。本轮没有宣称像素一致，也没有测播放 FPS 或 GPU 性能。所有窗口保持隐藏，未重启用户程序。原始工程及素材保持不变。

旧 EXE 与 DLL 保存在 `.local/archives/emm-import-20261009/`，MMN 的旧插件保存在 MMN 同名归档目录。不清理这些归档。
