# 分阶段性能测量（2026-09-20）

本轮在 dd75137 基础上增加可选计时，不修改渲染、排序或物理算法。默认
`MIKUDANCESTUDIO_PERF=OFF`，普通构建不执行计时。原始输出见同名 JSON。

## 方法与构建核查

- MSVC 19.44，x64 Release，明确指定 `/O2 /Ob2 /DNDEBUG`；生成的 core
  vcxproj Release 配置为 `Optimization=MaxSpeed`。
- 新目录首次配置时 Conan 曾把 Release flags 留空，该批数据全部作废。
  正式 build 与上一轮 before/after 目录的缓存均已核实有 `/O2`；不能把
  此次新目录配置问题当作用户原发布版低性能的原因。
- NVIDIA GeForce RTX 4070 Laptop GPU，adapter 0，HAL，859×482，无 MMEffect.dll。
- 与上一轮相同的 PolarisLiveStage（129307 顶点）和 SourRIN White（37922
  顶点），各自单独加载，默认编辑态物理开启，无动画播放。
- 每轮预热 30 帧，采样 150 帧，每个场景/模式重复两次，共八次。
- serial 每帧使用 event query 等待 GPU 完成；throughput 仅在预热结束及
  采样结束排空 GPU，保留应用自身 Present/Lock 的等待。
- `drained_batch_ms_per_frame` 包含末尾 GPU 排空，适合读取吞吐模式结果。
  `mean_ms/median_ms/p95_ms/equivalent_fps` 来自逐帧调用耗时，throughput
  模式下不包含末尾排空，不能当成交互窗口 FPS。
- 阶段是 CPU 墙钟时间，包含驱动调用中的等待，不是 GPU 时间戳。
  父阶段包含子阶段，不能相加。`PhysicsFrame` 包含姿态、同步、求解和回读，
  不能直接将它的总时间归因于 Bullet 求解器。
- 本机有其他交互任务，测试过程也曾编译默认配置；舞台 serial 两次波动
  较大（15.79 / 21.24 ms）。数据用于定位热点，不用于宣称优化提升比例。

## 结果

下表为两次均值，单位 ms/帧。排序锁路径包括首次 GetTransform、矩阵组合、
VB/IB Lock；CPU 排序包括源索引复制与 SortTransparentTriangles。

| 场景 | 模式 | 全帧含排空 | 蒙皮 | 透明排序总计 | 排序锁路径 | CPU 排序 | PhysicsFrame |
|---|---|---:|---:|---:|---:|---:|---:|
| 舞台 | serial | 18.52 | 1.40 | 8.04 | 2.19 | 5.82 | 0.74 |
| 舞台 | throughput | 13.77 | 1.43 | 7.84 | 2.12 | 5.68 | 0.73 |
| 人物 | serial | 27.70 | 0.32 | 2.22 | 0.03 | 2.19 | 20.81 |
| 人物 | throughput | 24.72 | 0.30 | 2.22 | 0.07 | 2.14 | 21.17 |

每帧舞台执行 33 次材质 CPU 排序，人物执行 17 次；两者都是每帧一次
排序锁路径。舞台的主要渲染热点是透明排序；人物的主要热点在物理帧。
这解释了为什么之前仅改缓冲池、延迟不必要的 Lock 没有普遍改善性能。
不能据此宣称已解释与原版 MMD / Nex 的全部差距：尚无相同方法的原版测量。

## 后续修复顺序

1. 人物：继续拆分 PhysicsFrame 的姿态更新、刚体同步、stepSimulation、
   回读，确认 21 ms 的具体来源，并核查 Bullet 库的编译优化与步进次数。
   不以关闭物理或减少正确性所需的步进来制造提升。
2. 舞台：减少透明排序重复计算和临时分配，评估 CPU 顶点位置缓存及首个
   draw 前完成排序，分别衡量 CPU 排序和 Lock 等待；保持透明顺序和 MME
   D3D9 资源契约。只移动 Lock 不会消除约 5.7 ms 的 CPU 排序。
3. GPU 蒙皮属于后续架构工作；本次样例的 CPU 蒙皮并非首要瓶颈。

## 复现与验证

在已配置 Conan 工具链的环境中，新建隔离目录并指定：

```powershell
cmake -S . -B build-perf/profile-opt -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=D:/Download/MikuDanceStudio-main/build/generators/conan_toolchain.cmake `
  -DCMAKE_CXX_FLAGS_RELEASE="/O2 /Ob2 /DNDEBUG" `
  -DMIKUDANCESTUDIO_PERF=ON -DMIKUDANCESTUDIO_BUILD_UI_TESTS=ON `
  -DMIKUDANCESTUDIO_BUILD_EFFECT_TESTS=ON
cmake --build build-perf/profile-opt --config Release --target frame_benchmark model_alpha_tests pmm_reload_tests
```

将运行所需 Data 目录放到测试 exe 旁，在该目录运行：

```powershell
.\frame_benchmark.exe <模型绝对路径>
.\frame_benchmark.exe <模型绝对路径> --throughput
```

如 MSBuild 报 Path/PATH 重复，需先统一子进程环境中的变量名；本轮使用
Python 子进程规范化环境后构建。实际 D3D 测试在沙箱外运行。

- PERF=ON 的优化构建通过；PERF=OFF 的默认 Release frame_benchmark 构建通过。
- model_alpha_regressions、pmm_reload_tests：2/2 通过。
- event query 新增 HRESULT 检查和十秒等待上限；外部每次进程限制 55 秒。
- ref 参考二进制、素材、构建产物不纳入提交。
