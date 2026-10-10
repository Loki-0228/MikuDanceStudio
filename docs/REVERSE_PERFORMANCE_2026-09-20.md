# MMD / NexGiMa 性能逆向笔记（2026-09-20）

本轮只增加分析工具与报告，不修改渲染代码。结论来自本地二进制、NexGiMa
随包 HLSL/CSO 和本工程源码，不代表已经复原两款程序的完整源码或测得优化收益。

用户补充：NexGiMa 不支持 MME。因此分别研究 MMD 兼容路径与原生渲染路径，
不能把 NexGiMa 的快速原生渲染等同于已解决 MME 兼容问题。

## 样本与方法

| 样本 | SHA-256 |
|---|---|
| ref/MikuMikudance 926 CHS/MikuMikudance.exe | 72c34b7ab607aef7b18974742ae18ce62f115f6a614bbedc1a4310e4a5406f72 |
| ref/NexGiMa_ver_2022-12-18_373/NexGiMa.exe | dcea56dd56df4b6062dc440da55008e3a9c09e6798ac66fddd83a29d8e1c5e22 |

两者均为 x64，首选 ImageBase=0x140000000。下文地址为首选 VA，实际进程需考虑
ASLR。926 的地址不能直接套用到源码注释中的 932 地址。

工具 `tools/inspect_reference_pe.py` 使用 pefile、Capstone、系统 D3DDisassemble，
读取导入表、交叉引用、MSVC RTTI 与 .pdata。未执行、注入或修改参考 exe。
关键指令节选保存于 `REVERSE_PERFORMANCE_EVIDENCE.txt`。
完整临时反汇编在被 Git 忽略的 `build-reverse/`。

注意：.pdata 条目可能只是同一函数的 chained-unwind 片段；工具输出的 range
不能自动当成完整源函数。虚表偏移也必须结合对象来源和参数确认，单凭 call +0x70
不能区分 Dispatch、Queue::Signal 或其他对象方法。

## 1. MMD：PMD 也经过并行蒙皮，而当前实现遗漏了这一点

**确认的证据：**

- 926 模型更新函数 `0x1400C1390..0x1400C2CE1` 同时处理两种模型格式。
- `0x1400C29FC` 读取附加 UV 数，选择五个 stride worker。
- 零附加 UV 分支 `0x1400C2A2F` 锁主 VB 和描边 VB；
  `0x1400C2AA0` 调 `_vcomp_fork` 的 thunk `0x1401277DC`，worker 地址为
  `0x14011F370`。最终统一在 `0x1400C2CB2 / 0x1400C2CBC` Unlock。
- worker 在 `0x14011F3E3` 调静态分块初始化 thunk；循环内 `0x14011F4B0`
  读取格式，`0x14011F536` 的非 PMX 分支读取 40 字节 PMD 顶点、两个
  16 位骨骼索引与百分比权重。PMD 并非绕过这个并行 worker。
- 当前 `src/model/model_skinning.cpp::SkinPmd` 是普通串行 for，只有 SkinPmx
  使用 `#pragma omp parallel for`。

**可借鉴：**恢复 PMD 的分块并行，分别测小模型、大模型、多模型的线程开销。
这项改动不要求升级 D3D，也不要求取消 MME；但不能解释上次 PMX 基准的退化。
PMX 已经并行，不应重复声称“开启 OpenMP”能解决其性能问题。

原版 worker 的大量计算也是 mulss/addss 标量 SSE，且 SDEF 每顶点调用两次
D3DXQuaternionRotationMatrix 和一次 D3DXMatrixRotationQuaternion
（`0x140120718 / 0x14012072C / 0x14012079F`）。不能把它描述成神秘的
全顶点 AVX 批处理。当前 SDEF 也保留了这些调用。

## 2. MMD：观察到的材质绘制路径不含当前新增的三角形重排

检查了模型材质函数 `0x1400B6D40..0x1400B8221` 的主体：设置材质、纹理、
渲染状态，绑定 VB/IB，并按材质绘制。观察到的绘制点包括
`0x1400B7DB0 / 0x1400B7E8A / 0x1400B801C / 0x1400B8107`。
此主体未发现重新 Lock VB/IB、逐三角形计算深度或排序的路径。
这是针对已检查函数的结论，不是对整个程序所有功能的全局否定。

当前 `SortTransparentModelMaterials` 在半透明阶段重新读取 VB、写 IB，并对
材质执行 stable_sort。第一轮优化只让不需要排序的模型跳过 Lock；透明模型
仍可能在前面的不透明绘制之后重新访问同一缓冲。

**推断：**原版的 MANAGED 参数与当前新增的绘制后锁缓冲组合，可能造成不同的
上传/同步代价。这是与上轮负面基准相容的解释，尚未通过驱动采样证实。
原版用 MANAGED 不等于当前代码只换 MANAGED 就会快。

**可借鉴：**将透明排序所需的位置保留在 CPU 暂存区，在首次绘制前完成必要
索引更新；避免为了排序从已提交的 VB 再取数据。保留透明效果正确性，不能仅为
性能删除排序。先测 skin、sort、Lock/Unlock、draw、Present 的分段时间。

## 3. NexGiMa：可核实的 GPU 蒙皮数据流

`Shaders/skining.cso` 可由 D3DDisassemble 还原反射信息与 shader 汇编：

| 绑定 | 内容 | 每元素字节 |
|---|---|---:|
| t0 | 原始位置、法线、切线、4 组 float2 UV、edgeBias | 72 |
| t1 | 骨骼初始偏移逆矩阵 | 64 |
| t2 | 骨骼世界变换矩阵 | 64 |
| t3 | 4 个骨骼索引、权重、SDEF 标记与 C/R0/R1 | 72 |
| t4 | 每顶点位置与 UV morph 偏移 | 44 |
| u0 | 输出顶点 | 72 |

- Shader Model `cs_5_1`，线程组 `64,1,1`，按顶点编号独立计算。
- 有顶点数量越界 early return；有 SDEF 分支和最多四权重的普通分支。
- 普通分支跳过不大于零的权重；处理后的顶点通过 store_structured 写入 u0。
- morphCount 在这里用来控制是否应用偏移；输入已经是每顶点的 morphOffset。
  **不能据此声称所有 morph 混合、骨骼求解和物理都在 GPU 上。**

主程序存在 `NndSkiningComputer` RTTI，vtable=0x1403E61C8；
GPU 提交函数识别为 `0x14027C470..0x14027D5B3`：

- `0x14027CCCA` 绑定计算 root signature（槽偏移 0xE8）；
  `0x14027CD28` 设置 compute PSO，`0x14027D24B` 绑定 descriptor table。
- `0x14027D305` 对已识别的 graphics command list 调 Dispatch。
- `0x14027D384` 把输出资源转为 `GENERIC_READ (0xAC3)`；状态追踪和 barrier
  表明结果进入 GPU 后续读取路径，而非这里立即取回 CPU 再蒙皮。
- Dispatch 的 X 参数来自输入计数乘 1/64 后的转换，并再次按 64 对齐；
  不将这个额外对齐直接当成最佳实践。此处并未完整确认被调用数学 thunk 的
  舍入语义；新实现应独立核对组数、边界与额外线程成本。

**可借鉴：**稳定几何、骨骼参数、变形输出分开存储，逐顶点工作放 GPU，结果留给
后续绘制消费。它不是“选择独显”一个开关的收益。不过输出 stride 为 72，不能
据此宣称总显存流量一定小于本工程；关键是计算归属和 CPU/GPU 数据往返。

## 4. NexGiMa：描边在顶点着色器扩张

随包 `Shaders/mmd.hlsl:281` 的 VSEdgeMain 根据相机距离、FOV、材质 edgeSize、
morph 系数及顶点 edgeBias 计算描边，然后执行 position + normal * edgeWeight。
它使用同样的输入顶点布局。当前工程 SkinPmx/SkinPmd 会在 CPU 生成单独的
16 字节 EdgeVertex 并更新描边 VB。

**可借鉴：**原生后端可在 shader 里扩张描边，避免 CPU 为相机相关描边反复改写
几何。不要将该公式直接替换 MMD 公式：两者画面语义需要验证，MME 描边钩子
和预期的顶点布局也需要兼容设计。

## 5. NexGiMa：提交与等待分开，并支持批量 command list

RTTI 中存在 `NndD3d12CommandListPool`、`NndD3d12CommandQueue`。
不只根据类名推断，已经检查到以下实现：

- `0x1401224C0` 接收一组 command list，收集原生指针。
- `0x1401225E9` 批量 ExecuteCommandLists；`0x1401225FB` Signal fence，
  增加 fence 编号，返回旧编号。本函数中没有紧接着 CPU 等待 GPU 完成。
- `0x140122770` 先比较缓存的完成值，必要时 GetCompletedValue。
- `0x140122680` 在尚未完成时调用 SetEventOnCompletion，再 WaitForSingleObject。
- 另有 `0x140122710` 的 Signal 后主动等待路径。因此不能声称 NexGiMa
  从不阻塞、所有帧都完全异步，或已确定其最大在途帧数。

**可借鉴：**复用命令与资源时依据完成编号判断生命周期；批量提交，只有需要
CPU 消费结果或复用仍在使用的资源时才等待。D3D12 barrier 是 GPU 资源状态
同步，不能直接等同于 CPU 等待。

上次 frame_benchmark 每帧显式等待 GPU 完成，是延迟测试，抑制了流水并行。
以后应同时保留同步延迟指标与不逐帧 flush 的稳态吞吐测试，不能只用它评价
NexGiMa 这种架构的端到端表现。

## 应用顺序与 MME 边界

1. 先在 D3D9 路径建立分段计时、draw/lock 次数与同步/吞吐两种指标。
   不把此前 MANAGED 修改视作已经完成的性能修复。
2. 局部验证透明排序移到首次绘制前、CPU 暂存位置与索引缓冲更新方式。
3. 单独恢复 PMD 并行。SDEF 骨骼四元数按骨骼每帧缓存也是可试验方案，
   但这是本工程的推导，不是声称两份参考都已这样实现。
4. 静态场景的 dirty 判断需覆盖骨骼、morph、物理、材质、相机相关描边及效果
   回调；不能只检查时间轴帧号就停止更新。
5. 若引入 NexGiMa 式 GPU 蒙皮/描边，作为原生后端设计评估。
   当前 MME 对象登记依赖 D3D9 创建资源的输出槽地址、绘制调用及宿主导出接口
   （见 MME_COMPATIBILITY.md）。换 D3D12 或改变 VB/IB 身份不是透明替换，
   也不能因为用户未加载 .fx 就认为 MME 的 D3D9 代理不存在。

本轮未实现上述候选，也没有新增“比原版快多少”的结论。

## API 核对与复现

虚表签名核对使用本机 Windows SDK 10.0.26100.0 的 d3d12.h/d3d9.h。
SDK 的 C 接口可能因预处理分支重复列出按值返回的方法，不能简单数文本里的
所有函数指针。已识别的 Dispatch/ResourceBarrier/Queue::Signal 位于这些重复
项之前。

- [Dispatch 参数含义](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-dispatch)
- [D3D12 resource barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)

示例：`python tools/inspect_reference_pe.py <exe> function 0x1401224c0`；
`python tools/inspect_reference_pe.py <exe> rtti NndSkiningComputer`；
`python tools/inspect_reference_pe.py <skining.cso> shader`。
工具需 Python、pefile、capstone；shader 模式另需 Windows d3dcompiler_47.dll。
