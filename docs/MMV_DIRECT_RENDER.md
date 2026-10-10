# 原版 MMV 直接渲染接入

新插件源码在同级 `MMV` 独立仓库，公共头为 `MMV/include/mmv/render.h`。MMS 只编译宿主桥，不编译插件；可选安装的产物名为 `MMV.dll`。

`frame_scene.cpp` 在宿主默认绘制前调用 `RenderTakeover`。返回 1 后跳过默认模型顶点上传、模型/阴影绘制和场景合成；随后 `PresentRenderOutput` 将插件颜色纹理提交到宿主目标，继续原有的录制捕获与编辑控件路径。返回 0 表示未安装或已禁用插件，继续原生渲染；返回 -1 表示已接管的渲染失败，本帧停止，不再执行第二套场景绘制。

桥位于 `src/mmv_host/renderer_bridge.inl`。模型顶点在 CPU 副本上采样当前形变和蒙皮，不更新宿主顶点缓冲；配件读取现有网格。相机、材质、共享 toon 与投影阴影配置只读传入。插件使用宿主 D3D9 设备，桥保存并恢复状态、渲染目标、深度表面和 viewport。颜色纹理由宿主 AddRef 保留到提交结束。

打开 PMM 时保存路径，插件自动读取同名 EMM。新入口不执行旧 `Evaluate/Upload` 输出，也不进行 MME 分配快照同步。旧桥辅助代码暂留供分阶段迁移，但没有直接渲染入口的旧 DLL 已被拒绝。

## 验证记录

- Release x64 插件与生产 MMS 均编译通过。
- 独立隐藏窗口测试覆盖默认颜色、透明混合、自阴影执行、EMM 隐藏、后处理最终像素、输入不变和设备资源重置。
- `AlternativeFull/00_test_material.pmx` 的 PMM 在 640×480 后台验证中，显示与隐藏分别接受 3/3 帧，失败为 0，默认蒙皮更新计数为 0。
- `scene_lifecycle_tests --record-frames` 连续五帧录制计数和相机关键帧采样通过；这项测试没有写 AVI 文件，不能代替完整编码验收。
- `[MMV] Enabled=0` 启动时未加载插件，默认蒙皮更新恢复。

结果存于 `.local/artifacts/mmv-direct-render/verified/`。`initial/`、`final/` 留有中间结果，其中黑色默认材质和共享 toon 格式拒绝属于已修复的失败；不得用于性能结论。新仓库 README 记录未迁移的显式离屏 EMM、Compute FX、QDEF、宿主深度交接与 DX12 等范围。

未完成对应两侧生产源码的前后画质与性能验收；以上是首阶段的功能验证，不宣称完整替代所有 MME 效果。
