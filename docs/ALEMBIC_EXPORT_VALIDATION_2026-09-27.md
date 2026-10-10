# ABC 导出与网格烧录验收

## 构建与证据

基线为 `4b107c2`，工作树位于 `.local/worktrees/alembic-baseline`。功能提交为 `54bfe14`，直接在正式主仓库构建。两侧均为 VS 2022 x64 Release，MSVC 14.44、Windows SDK 10.0.26100，启用 `MIKUDANCESTUDIO_BUILD_UI_TESTS` 和 `MIKUDANCESTUDIO_BUILD_RENDER_TESTS`。Conan 工具链位于主仓库 `build/generators/conan_toolchain.cmake`。完整 CMake 参数、Git 状态与 EXE 哈希随性能清单保留。

| 生产文件 | SHA256 |
| --- | --- |
| 基线 `MikuMikuDance.exe` | `83B23DDE3804A5DD61B2F870AC3713A8C5D2AB76A60859A2F42B0039D0D5661B` |
| 功能版 `MikuMikuDance.exe` | `80AE6C44DE90B39AEA2768A20C3E08E5DA30473B163FC34D1D7162FD93F7522F` |
| 正式目录兼容版 `MMV.dll` | `DDFC377BE373F9745F5226E6BF04B2663157ADF4D063C1AE7BEFE59511618598` |

原始结果位于 `.local/artifacts/alembic-export/`，包含成功与失败的开发日志，不把失败样本当作验收结果。`release-manifest.json` 记录发布文件、功能日志和 DSH 会话；DSH 实际工作目录为 `D:/MMD-Next/MikuDanceStudio-MME`。网格缓存、菜单、顶点捕获与 MMV 事务模块经过主 agent 审查及统一构建；MMV 事务独立验证另存于 `.local/artifacts/mmv-mesh-bake/`。

## 功能结果

| 验证 | 结果与范围 |
| --- | --- |
| `vertex_capture_tests` | 95 项实际 D3D9 HAL 检查通过，含 VS 1.1/2.0/3.0、状态恢复、拒绝不支持路径，以及 300000 顶点的两块回读。 |
| `mesh_bake_cache_tests` | 真实 Ogawa 文件采样、PMM 配套文件往返、替换、取消提交、清除与多实例临时文件隔离通过。 |
| `mmv_runtime_tests` | 208 项检查通过，使用 GPU 替身，验证 BEGIN/暂存/提交/取消，以及 root、材质、骨骼、owner 和 VEM 自动发现绑定的停用；不是 GPU 性能测试。 |
| ABC 原生路径 | 生成三角形 PMX，3 个样本的骨骼位移正确；Unicode 文件名、30/60 fps 采样、取消保留旧文件、模型及时间轴恢复通过。 |
| MME 顶点变形 | VS 将 X 增加 3，ABC 回读 3 个样本各顶点均增加约 3.00001。测试效果覆盖 `object` 和 `object_ss` 通道。 |
| MMV + MME | MMV DX12 先将 X 增加 2，再由 MME 增加 3，ABC 各样本回读约 5.00002。纯 DX12 版与正式目录所用兼容版 DLL 均通过完整导出、烧录和清除流程，日志为 `combined.log`、`combined-compat.log`。 |
| 新增与变化的拓扑 | 测试插件追加材质和三角形；变化模式在三次采样中依次产生 3/6/3 个顶点和 1/2/1 个三角形，空样本正确。 |
| 烧录和恢复 | 只烧录、同时输出、取消、PMM 保存重开、旧 EMM 再停用通过；烧录后把骨骼末帧 X 改为 12，缓存不变，清除后 X=12。 |
| 清除菜单 | 从「烘焙」命令路由打开对话框并实际清除。清除当前模型保留附件缓存，清除全部移除剩余缓存。 |
| 附件 | 生成 X 网格，改变附件关键帧的平移和缩放后，已烧录的最终坐标保持不变。 |
| 条件警告 | 三种语言分别检查默认 ABC 勾选、ABC 勾选不弹警告、拒绝烧录警告撤回勾选、确认勾选、取消勾选不再提示。 |
| 原有物理烧录 | 前后两侧隐藏运行同一 `tests/UserFile/测试刚体.pmm`：140 骨骼、45 刚体、27 joint；均烧录 23 骨骼、298 项修改、22 步，PMM/VMD 往返通过，`failures=0`。 |

`ctest -C Release -R "^(vertex_capture|mesh_bake_cache|mmv_runtime)$"` 的 3 项测试全部通过。所有集成测试使用隐藏窗口；未读取桌面截图。完整入口和运行目录要求见 [tests/README.md](../tests/README.md)。

## 性能对照

前后生产源码构建的 `frame_benchmark` 在相同素材、相机、1280×720 D3D9 视口下按 ABBA 顺序采样。指标为等待 GPU 完成后的批次毫秒/帧，不是播放 FPS、独立 GPU 时间或 CPU 提交时间。此对照检查未启用新功能时的渲染开销，不能代表导出吞吐率或缓存回放速度。

设备为 NVIDIA GeForce RTX 4070 Laptop GPU。每次预热 15 帧、记录 60 帧，每场景两轮 ABBA，两侧各 4 次，全部 16 次退出码为 0，采样与视口检查通过。两侧构建输入均已提交且无未提交源码。工作区另有其他任务的文档变动，原样记录在清单中，不属于本功能。

| 场景 | 基线中位 ms/帧（范围） | 功能版中位 ms/帧（范围） | 中位变化 |
| --- | --- | --- | --- |
| 单份 HGrass，471122 顶点 | 201.358（194.912–212.046） | 201.289（197.063–205.634） | -0.03% |
| Rou，移动相机 | 6.968（6.673–8.586） | 6.679（6.527–7.728） | -4.15% |

本轮未见中位耗时增加，两侧数值范围重叠；人物场景的差异不作为稳定性能提升结论。原始 JSON、日志、素材哈希、构建参数、提交与汇总保留在 `.local/artifacts/alembic-export/performance/`。

```powershell
python tools/run_alpha_background_compare.py `
  --before-tree .local/worktrees/alembic-baseline --after-tree . `
  --output .local/artifacts/alembic-export/performance `
  --grass D:/MMD-Next/tests/Grass/HGrass.pmx `
  --character 'D:/MMD-Next/tests/MMD ☆★Rou/☆Rou.pmx' `
  --cases grass-1-spread,face-moving --warmup 15 --samples 60 --cycles 2
```

复跑时选择新的输出目录，保留已有结果。

## 边界

验证使用合成 MME/MMV 变形与实际本地物理素材，没有覆盖所有第三方效果。屏幕后期、自定义投影、依赖历史渲染目标的效果以及非标准绘制有明确限制，见[功能说明](ALEMBIC_EXPORT.md#捕获范围与限制)。旧插件缺少烧录事务通知时会拒绝工程烧录；只导出 ABC 的能力不受该接口限制。

开发中修正了 Windows INI 刷新返回值的误判：`WritePrivateProfileStringW(NULL, NULL, NULL, file)` 刷新缓存时返回 0，该值不能按普通写入失败处理。依据 [Microsoft API 文档](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringw)。
