# 云模型 Toon 调整

用户要求修改云模型 Toon。本次提供独立浅阴影版本，保留原模型、原 `CToon.png` 和原 `cloud.pmm`。它用于分离材质明暗与透明排序的影响，不代表云排序整体已经验收。

## 修改内容

`ToonCloud.pmx` 的 15 个材质都使用外部 Toon，索引为 1。原 `CToon.png` 为 64×64，上 32 行白色，下 32 行为蓝灰色 `RGB(182,196,212)`。

新 `CToon-soft.bmp` 是 64×64、24 位无压缩数值光照查找表：上 28 行白色，中间 8 行线性过渡，下 28 行改为 `RGB(235,240,246)`。阴影更浅，保留少量冷色层次。内置自阴影会提取 Toon 的阴影色；过渡带本身不等于改善透明遮挡顺序。

模型副本仅改动 PMX 的 textures 段。顶点、索引、材质参数、骨骼、表情、表示枠及物理数据保持原字节。恢复原贴图表后，整个 PMX 可逐字节还原为输入文件。

## 文件与复现

最终对照目录为 `.local/artifacts/cloud-toon-20260926/validated/`：

- `cloud-soft.pmm` / `cloud-soft.emm`：浅色 Toon 工程。
- `cloud-original.pmm` / `cloud-original.emm`：原 Toon 对照工程。
- `model/ToonCloud-soft.pmx` / `model/CToon-soft.bmp`：新模型和 Toon。
- `model/textures/tex-0.tga`：原 cloud.tga 的逐字节副本，保留透明度。
- `asset-validation.json`：源文件哈希、PMX 字节校验和贴图哈希。
- `comparison-manifest.json`：两侧 Git 状态、构建选项、EXE 哈希、命令和计时。
- `run-comparison.py`：隐藏窗口交替对照驱动；PNG 为渲染目标回读。

PMM 仅替换偏移 68 处、长度 256 字节的模型路径字段，其余字节保持。相机、动作及光照相同。两个 EMM 均明确 `Pmd1 = none`，Cloud.fx 未启用；ikParticle 配件效果保留。PMM 的模型路径是本机绝对路径，移动整套工程时需重新指定模型。

重建模型工具：`tools/make_cloud_toon_variant.py input.pmx output_dir`。它拒绝覆盖已有输出，依赖贴图按原字节复制并用相对路径引用。

## 加载问题与无效样本

最初的工具把纹理表改为绝对路径。实际加载器 `src/model/pmx_load.cpp` 直接拼接模型目录与纹理条目，因此副本未加载 cloud.tga，表现为不透明白色轮廓。该组耗时和图像无效，不能作为性能收益。根目录下的首组 `01` 至 `08` 结果全部保留；manifest 已记录排除理由。

最终副本将贴图随模型存放并改用相对路径。原始 cloud.tga 与复制件 SHA-256 相等，未转换图像或改变 alpha。

## 委派与验收

DSH 子任务 `dd340559-d576-4e2b-859b-97c33333f263` 在 `D:/MMD-Next/MikuDanceStudio-MME/.local/worktrees/cloud-alpha-fix` 修改单个 Toon 生成工具。主 agent 负责真实素材校验、PMM/EMM 集成、前后 build 对照和文档。原输入 PMM 哈希为 `80018d90968d69c98554cc2672cbce7a6b2bf73ab8264af3f1b283e8f9025811`。

本次使用旧排序 build 与逐像素分层实验 build，各自渲染原 Toon、新 Toon，执行两轮反向交替采样。每次预热 3 帧、采样 5 帧。结果口径为 GPU 完成后的批次平均毫秒，不能换称桌面播放 FPS。未启用额外像素层调试读回。

排序原型仍有成本及异常回退限制，见 [透明排序精度实验](TRANSPARENT_SORT_PRECISION_2026-09-26.md)。本次不替换正式 Release。

## 有效对照结果

基线为 `3d13fbc`，候选为 `1d5e757`（渲染代码固定于 `099caf8`）。两侧使用同一后台驱动源码，其 SHA-256 均为 `6e0b7ee299ae6368495cf32fd4ff44fd34642536eb081cff635da09dd74b73f4`。启动时工作树均干净；后续仅修改 Toon 工具和文档。

| build / Toon | 第一轮 ms/帧 | 第二轮 ms/帧 | 中位 ms/帧 |
| --- | ---: | ---: | ---: |
| 旧排序 / 原 Toon | 4.902 | 2.742 | 3.822 |
| 旧排序 / 浅 Toon | 4.809 | 5.923 | 5.366 |
| 逐像素实验 / 原 Toon | 654.924 | 653.167 | 654.045 |
| 逐像素实验 / 浅 Toon | 661.474 | 626.693 | 644.083 |

仅两轮短采样，基线波动明显；不据此声称 Toon 带来性能提升。逐像素排序原型仍有数百毫秒成本，性能不合格。

画面检查：浅 Toon 保留 cloud.tga 的透明边界，并减弱云内部蓝灰暗块。旧排序 build 即使换浅 Toon，仍可见分片重叠轮廓；这说明 Toon 调色不能替代透明排序。原 Toon 与浅 Toon 在候选 build 中使用同一套排序。

基线与候选部分回读图均未显示 HUD，操作叠层的一致性不在本组验收结论内。只把本组用作 Toon 变化的对照，不把它写成完整渲染回归通过。

候选原 Toon 见 `02-candidate-original.png`，浅 Toon 见 `03-candidate-soft.png`；旧排序浅 Toon 见 `04-before-soft.png`。全部图片、日志和失败样本保留。

## 后续发黑定位

用户确认调 Toon 未解决问题后，继续通过原 Toon 下的法线和阴影控制定位。暗片主要来自粒子面法线的方向光响应，浅 Toon 仅降低对比。此工具及素材保留为诊断对照，详见 [云内部暗块来源](CLOUD_DARK_SHADING_2026-09-26.md)。

直接修改 Toon 贴图颜色或 Toon 着色参数主要改变暗部 RGB；当前内置 SM3 shader 在着色之后重新设置输出 alpha，Toon 色不会替代贴图/材质的不透明度。低 alpha 密集叠层另有逐层量化缺陷，详见 [浮点合成修复](CLOUD_FLOAT_COMPOSITING_2026-09-26.md)。
