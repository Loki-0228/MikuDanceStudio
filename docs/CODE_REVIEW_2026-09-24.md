# 渲染修复后的代码审查

## 范围与方法

本次审查覆盖 `src` 与 `include/mikudancestudio` 的 210 个自有源码文件，共约 9.3 万行。其中有 146 个 C++ 源文件、53 个头文件、5 个 `.inc`、3 个 HLSL、2 个着色器模板和 1 个导出定义。第三方依赖、原版二进制、模型素材及构建产物不计入自有代码范围。

独立 `build-analyze` 以 VS 2022 x64 Release、`/analyze /analyze:quiet` 构建全部生产目标。146 个 C++ 源文件均生成分析报告，没有遗漏源文件；头文件随翻译单元分析。手工审查重点为本次生产改动、上一轮审查后的改动、渲染与蒙皮热路径、透明排序线程隔离、设备重置、文件读取和资源释放。三份 HLSL 另检查索引范围、分派范围及 CPU 侧输入约束。

这是全项目静态分析与重点手工审查，不声称逐行重新审读了全部 9.3 万行。上一轮 [审查报告](CODE_REVIEW_2026-09.md) 和 [缺陷清单](CODE_REVIEW_FINDINGS.md) 保留为历史依据，不将其工作量重新计入本次。

首次分析输出 316 条告警记录，按文件、行号及规则去重后为 266 处，另有 16 处普通编译警告。主要类别如下：

| 规则 | 去重数量 | 核验方向 |
| --- | ---: | --- |
| C6031 | 167 | 忽略文件读取、转换、COM 初始化等返回值 |
| C6387 | 25 | 空指针与接口参数契约 |
| C6246 | 16 | 局部变量遮蔽 |
| C6386 / C6385 | 10 | 数组容量、跨成员访问及索引范围 |
| C6011 | 7 | 分配失败或 API 输出可能为空 |
| C6340 | 6 | 格式化参数有符号性不一致 |
| C6384 | 2 | PMM v1 将指针大小当作数组容量，已修复 |
| C6993 | 2 | 分析器忽略 OpenMP，只分析串行语义 |

告警数量不是确认缺陷数量。分析器没有验证并行数据竞争，也没有证明第三方 MME 回调安全。本次排序实验另运行了多线程隔离测试；生产渲染仍以实际像素和场景回归验收。

本机完整清单、每个文件的哈希与行数、XML 分析结果汇总位于 `D:\MMD-Next\.alpha-perf-artifacts`：`review-source-inventory.json`、`static-analysis-findings.json`、`static-analysis-unique.json`、`static-analysis-0.log`、`static-analysis-1.log`。原始 XML 在 `build-analyze/*/Release/*.nativecodeanalysis.xml`。

在仓库根目录，`git ls-files src include/mikudancestudio` 可重新列出这 210 个文件。分析构建的关键 CMake 参数为 `-DCMAKE_CXX_FLAGS="/MP4 /DWIN32 /D_WINDOWS /EHsc /analyze /analyze:quiet"`，同时关闭 `MIKUDANCESTUDIO_PERF` 与 `MIKUDANCESTUDIO_DIAG`。`/analyze:quiet` 将分析结果写入 XML，控制台没有分析告警不等于检查未发现问题。

## 本次修复

| 问题 | 实现与验证 |
| --- | --- |
| 半透明边缘先写深度，后方 alpha 材质中的实心像素未绘制 | 原生不透明阶段增加实心覆盖绘制，固定管线和内置自阴影路径均恢复 `ALPHAREF`。54 个真实 GPU 像素和 6 次状态恢复检查通过，基线可复现失败。见 [透明遮挡说明](TRANSPARENCY_OCCLUSION_2026-09-24.md)。 |
| 录制缩放的水平采样相位跨行残留 | 每行重置相位，分配前拒绝无效或放大尺寸。独立面积重叠算法核验整数、非整数及同尺寸缩放；基线第二行失败，修复后通过。 |
| PMX 文本前缀溢出、截断和缺少显式终止符 | 检查偶数字节长度、剩余文件长度、`INT_MAX` 和实际读取数量；初始化完整纹理指针表后才能清理失败路径。缓冲及直接读取共 98 个样本通过。此修改只加固文本字段。 |
| PMM v1 丢失素材的重新定位路径容量错误 | `pmm_load_v1.cpp` 两处 `ofnTitle` 是指针别名，原容量在 x64 上为 4 个宽字符。改为 `ctx.ofnTitle` 的真实 256 字符容量，避免普通文件路径触发 `wcscpy_s` 参数错误。DSH 只修改此模块，主任务核对差异并集成。 |

## 告警核验与保留问题

- `pmm_load_v1.cpp` 的两处 C6384 是明确缺陷，已修复。加载器选择替代素材后会把完整文件路径交给 `ExtractDirFromPath`，4 字符容量无法容纳该路径。
- `app_defaults.cpp` 的 48/64 字节告警对应 `customColorTable` 后紧邻的 16 字节保留区，布局注释明确该区合计保存 16 个 `COLORREF`。`command_control_400.cpp` 的插值与灯光清零跨越相邻数组。`pmd_load.cpp` 的名称字段后有 30 字节保留区。它们需要逐项理解二进制布局，不能仅凭告警判定写出了所属对象；类型边界的表达仍可改进。
- `dialog_select_ops.cpp` 先用同一谓词统计可选骨骼，再写入对应数量的元素。`morph_apply.cpp` 的材质暂存区按材质数量分配。`ui_selection_reeval.cpp` 已排除负选择值并将“全部通道”映射为 0。对应索引告警未确认正常输入下的越界。
- `capture_downsample.cpp` 与 `scene_font.cpp` 的像素指针告警位于成功的 D3D 锁定之后；本次真实设备回归通过。仍依赖成功锁定返回有效内存的接口约定。
- `mmdxshow/push_pin.cpp` 初始化跨过显示区末尾 4 字节，后续构造代码立即初始化相邻成员。它是二进制布局恢复写法，未把该告警记为本次新发现的运行时越界。
- `model_renderers.cpp` 与 `model_skinning.cpp` 的 OpenMP 告警说明工具覆盖限制。排序工作项先建立独立缓存项，工作线程不扩容共享容器；本次没有改动该所有权规则。

以下问题保留为后续工作，不能因本轮回归通过而视为已解决：

| 优先级 | 位置 | 触发条件与影响 |
| --- | --- | --- |
| P2 | `src/app/frame_driver.cpp` 的 `D3DERR_DEVICELOST` 等待循环 | 仅等待 `DEVICENOTRESET`，且窗口模式不处理消息；异常设备状态可能持续等待。尚未通过故障注入复现，不能把此前 MME 重置错误框的堆栈归因于此循环。 |
| P2 | `src/model/pmx_load.cpp`、PMD/VMD/PMM 读取路径 | 多处计数与非文本字段仍未统一检查读取数量和分配乘法。畸形文件仍可能导致超量分配或读取未完整初始化的数据。此次文本修复不是完整格式校验器。 |
| P2 | `src/render/draw_glyph.cpp`、`src/window/frame_line_edit.cpp`、`src/window/command_control_400.cpp`、`src/media/wave_audio.cpp` | 部分 `GlobalAlloc/GlobalLock/malloc` 结果在检查前使用。低内存或失败分配下存在空指针风险，尚未做分配失败注入。 |
| P2 | `undo.auxiliaryPose` 的多处分配与释放 | `malloc/free` 和标量 `operator new/delete` 仍有混用，上一轮清单也记录了此遗留问题。需要按完整所有权路径统一处理。 |
| P3 | `src/main/winmain.cpp`、PMX 读取及排序局部缓冲 | 分析器报告较大的栈使用，包括状态清零临时量和 64 KiB 顺序读取缓冲。正常场景已运行，深层调用及小线程栈边界仍需专门验证。 |

没有为消除告警而重写恢复出的结构布局、关闭透明排序或改动物理行为。新性能方案是否接受由真实对照决定，见 [Grass 帧率记录](GRASS_PERFORMANCE_2026-09-24.md)。

## 验证状态

包括 PMM v1 最后两处容量修改的最终 Release 构建成功，25 项已注册 CTest 全部通过，耗时 10.83 秒。增量分析仍覆盖 146 个翻译单元，报告变为 314 条、去重后 264 处；两处 C6384 已消除。PMM v1 的两条原生文件选择对话框恢复流程未逐一做 UI 自动化，本次依据调用点容量核验、增量静态分析和既有回归确认该修改。

最终记录为 `final-verification.json`、`final-build.log`、`final-analysis.log`、`final-ctest.log`、`ctest-25-final.log` 和 `static-analysis-final.json`。可执行文件为 `build/Release/MikuMikuDance.exe`，SHA-256 为 `001a2e318396eab44a88344279a623aaeba8c3a5aaee7abaffb709f657f73295`。

## 透明层排序复核（追加）

本节记录主任务对透明层排序改动的复核，追加在原有静态审查与 25 项 CTest 结果之后。上述 146 个翻译单元的分析结论对应改动前的源码，不能声称为对最新缓存重跑。

复核覆盖以下环节：候选门控、PMD/PMX 索引边界、双面剔除、近远裁剪回退、跨材质模型次序、draw 上限、scratch 解绑与释放、缓存失效。DSH 独立模块的改动经主任务复核，其中的错误候选已拒绝，未合入。

透明几何的新头文件算法与新的 GPU 回归记录见 [透明层排序说明](TRANSPARENT_LAYER_ORDERING_2026-09-24.md)。

缓存最终候选 `41a84b0` 的 6 项相关 CTest 全部通过，耗时 2.13 秒。这 6 项覆盖 320 帧累计漂移、深度翻转、数量变化以及预算失败检查。全量静态分析没有为本次缓存改动重跑，前面的 146 翻译单元结论仍对应改动前的源码。

`41a84b0` 的 7 场景 56 次后台性能对照已全部完成，字段、构建、Data 与测试源核对一致。实测结果不是性能无损通过：面部固定场景成本增加 12.67%，移动场景成本增加 22.20%。

其后的 `372c4bc` 只对静止几何做了一次撤 padding 精简，该提交的 6 项相关 CTest 再次全部通过，耗时 2.42 秒。`372c4bc` 的面部 16 次对照已完成，场景、构建、Data 与测试源核对一致。实测同样不是性能无损达标：面部固定场景额外成本 6.95%，移动场景额外成本 28.63%。Grass 一项保留 `41a84b0` 的 56 次矩阵中的样本，本节不声称 `372c4bc` 的所有场景都已测。

逐场景数据表与最终明细见 [透明层排序说明](TRANSPARENT_LAYER_ORDERING_2026-09-24.md)，本节不复制该表。

前述全量静态审查仍属历史结论，没有为本次缓存改动重跑。原截图中的症状未在同条件下复现，本报告不声称该症状已完全修复。本报告也不据此声称全套回归通过或已无剩余缺陷；前一节列为保留的 P2/P3 问题仍然有效。
