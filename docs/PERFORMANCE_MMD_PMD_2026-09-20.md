# 原版 MMD 优化应用：PMD 并行蒙皮

基于 6ddfc1c，独立于前一轮排序修改。

## 参考证据与实现

926 原版更新函数 0x1400C1390..0x1400C2CE1 调用 VCOMP worker 0x14011F370，
worker 的 0x14011F536 分支读取 PMD 两骨骼顶点，证明 PMD 也经过并行蒙皮。
具体反汇编见 REVERSE_PERFORMANCE_EVIDENCE.txt，完整解释见
REVERSE_PERFORMANCE_2026-09-20.md 第1节。

当前 PMX 已使用 OpenMP，PMD 仍为串行。现在为 PMD 恢复静态分块并行，
保留逐顶点算术、权重、无效骨骼保护和描边公式。骨骼只读，各线程写入
不重叠的顶点。顶点少于16384时串行，以减少小模型线程启动开销；这一阈值
是本工程的保守策略，不是逆向获得的原版常量，尚未跨硬件全面调优。

原版材质路径中未发现当前新增的逐三角形排序，不能据此删掉本工程已修复
透明问题的排序；前一轮选择加速并保持排序结果。SDEF 的原版逐顶点四元数
转换也已确认，不能将未经验证的四元数缓存归功于原版。

## 测试方式

tools/make_pmd_perf_fixture.py 生成3000和60000顶点的无外部贴图PMD网格，
包含混合权重、描边开关及偶发无效骨骼索引；不是分发参考软件中的模型。

frame_benchmark 增加 `--skin-only [输出缓冲文件]`：装载后设置确定性的
非单位骨骼矩阵，预热30次、测量150次 UpdateModelVertexBuffers，不绘制，
不执行物理。结束后可输出两个完整 VB，便于串行/并行结果逐字节比较。
这里应读取逐次 `mean_ms`，而不是含末尾 event query 的批次耗时；
输出中的 equivalent_fps 只是倒数，**不是整帧渲染或播放 FPS**。

采用同一 Release /O2、PERF=OFF、DIAG=OFF，RTX4070 Laptop 的实际D3D9缓冲。
串行/并行/并行/串行交替，两版只差 OpenMP pragma，均使用最终无提示的
加载路径及同一测试程序。原始数据：PERFORMANCE_PMD_2026-09-20.json。

| 顶点数 | 串行更新 ms | 新版更新 ms | 结论 |
|---|---:|---:|---|
| 3000 | 0.0283 | 0.0280 | 基本持平，仍为串行执行 |
| 60000 | 0.5720 | 0.0761 | 更新耗时下降86.7%，此环节约7.5倍 |

每种规模的4次输出逐字节相同（主VB+描边VB分别合计144000/2880000字节），
SHA256记录在原始数据。没有进行真实大型PMD动画、多模型或不同CPU的整帧
跑分，不能将该环节提速倍数外推成全局FPS提升。

## 修复测试弹窗

最初生成的PMD未写英文信息，测试加载触发“no English data”警告，用户
指出后已停止该批次；未使用被中断批次作为最终数据。

测试PMD现已补齐英文段和内置toon名称；LoadModelFile 新增默认true的
showModelInfo参数，基准传false，直接使用解析器已有的说明框开关。
普通用户加载模型保持原默认行为。最终8次测试输出均无load dialog记录。

## 复现

```powershell
python tools/make_pmd_perf_fixture.py build-perf/pmd-large.pmd 60000
# 在测试exe所在目录执行，确保旁边有Data目录：
.\frame_benchmark.exe <pmd-large.pmd绝对路径> --skin-only <输出文件绝对路径>
```

比较两版输出文件必须逐字节相等，不能只比较渲染截图或平均时间。
普通吞吐测试和PMM基准入口仍保留。

合并两轮优化后的 Release 构建成功；d3dx_import_regressions、
effect_api_regressions、bone_projection_regressions、model_alpha_regressions、
pmm_reload_tests、morph_runtime_tests 共6/6通过。实际MME特效场景的完整
画面对照与Nex原生GPU后端仍不在本轮验证范围内。
