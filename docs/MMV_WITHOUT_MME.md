# 无 MME 时的 MMV 启动与最终输出

未加载 `MMEffect.dll` 时，MMV 仍正常启动。正层逐层传到 0 层，0 层输出网格交给 MMS 原生渲染；有 MME 时，0 层到 -1 层的既有交接保持不变。无需额外开关。

## 代码核对与修改

宿主初始化只要求可选的 `MMV.dll`。`mmv_host::Evaluate` 在模型和附件绘制之前发布输出，固定渲染、内置效果、自阴影及附件路径通过 `RenderModel` / `RenderAccessory` 取得代理网格。

本次生产修改仅在烘焙缓存恢复分支为 `DrainEffectRegistration` 加上 MME 存在检查。无 MME 时没有待登记的 MME 效果，不再回放 MME 专用登记帧。有 MME 时仍执行原调用。

MMV 菜单的中英日切换已通过测试。虽然 `RefreshEffectMenuLanguage` 在无 MME 时早退，外层 `RefreshMenuLanguage` 仍会独立刷新 MMV；这不是缺陷。

## 隐藏窗口验收

Release `mmv_host_tests` 的 `final` 模式使用真实宿主帧路径和夹具插件，运行目录为 `.local/releases/mmv-no-mme-validation/`，其中不含 MME 及其 D3D9 代理。

以下检查全部通过：

- `MME=0 MMV=1`；三种语言各只有一个 MMV 菜单。
- 固定、自阴影和设备重建后三组恒等输出，各自连续两次回读逐像素一致。
- 模型、附件均发布代理；模型代理与着色前捕获均包含新增几何。
- 固定渲染与内置自阴影渲染中，0 层新增几何均改变超过 100 个最终画面像素。
- 设备重建后，新增几何相对同一次重建后的恒等基准仍改变超过 100 个像素；不使用重建前画面作基准。
- 每次回读先断言实际帧目标与读取目标相同。

最终日志：`.local/artifacts/global-processor/mmv-final-integration-stable-reset.log`。同目录的 `mmv-final.*.bgra` 保存各阶段回读；`mmv-final-validation.json` 保存像素差计数与二进制哈希。

## 失败记录与验证边界

早期失败日志保留。固定路径的标记三角形被 PMD 背面剔除，夹具现提供正反绕序；PMX 双面标志不能控制 PMD 固定路径。模型编辑视图使用 `ViewOffsetX/Y`，相机面板使用另一组取景状态，测试分别设置。

自阴影绘制结束会恢复渲染器保存的场景表面，初版测试仅替换设备当前表面，因而读回了错误目标。现在测试临时接入正确场景目标并在结束后还原，未改生产渲染目标逻辑。原始阴影贴图预览开关保持关闭，正常自阴影着色仍开启。

本轮验收覆盖宿主与夹具插件的 0 层交接，不替代真实多层 FX 的整套场景验收。新增 MME 登记门禁尚未单独完成烘焙缓存回放的前后 build 对照，该分支保留为待验收项。有 MME 的交接未在本轮重跑。
