# D3DX 模块查询优化实测

## 改动与原因

在 3532a36 基础上细分物理计时，人物样例的 PhysicsFrame 约 20 ms，其中
姿态更新约 11 ms、刚体回读约 6 ms，主 stepSimulation 仅约 2 ms。
这些路径频繁调用 D3DX 数学函数；每次 Api::Load 都重新调用
GetModuleHandleA("d3dx9_43.dll")，造成大量重复的模块查询。

静态导入已保证 D3DX DLL 在进程运行期间存在，因此只需首次获取模块句柄。
本轮仅缓存这一句柄，仍然在每次 Load 重新读取全部 IAT 函数入口，保留
MME 在设备创建之后安装挂钩的行为。动态加载分支保持原样。
没有修改数学函数、姿态计算顺序、物理步数、透明排序、资源格式或渲染质量。

新增默认关闭的 pose / physics_sync / physics_readback / physics_step /
bone_morph 计时。physics_step 仅包围主步进，不含初始化/settle/追帧步进；
这些基准的采样期主步进每帧一次。父子阶段包含关系与上一轮相同。

## 前后对照

原始数据见 PERFORMANCE_D3DX_2026-09-20.json，包含每个测试二进制的 SHA256。
baseline 是 3532a36 加相同物理细分计时，candidate 唯一行为差异是缓存句柄。
两者同一隔离构建目录、MSVC 19.44、Release `/O2 /Ob2 /DNDEBUG`、PERF=ON、
DIAG=OFF；RTX 4070 Laptop，HAL adapter 0，859×482，未加载 MMEffect.dll。
单模型、编辑态、默认物理开启，30 帧预热 + 150 帧采样。

throughput 每个模型按 baseline / candidate / candidate / baseline 顺序测量，
取各两次均值；serial 各一次交叉检查。全帧时间包含末尾 GPU 排空。

| 样例 | 模式 | 修改前 ms/帧 | 修改后 ms/帧 | 耗时变化 |
|---|---|---:|---:|---:|
| SourRIN White，37922 顶点 | throughput | 24.0557 | 5.5247 | -77.0%（吞吐约 4.35 倍） |
| SourRIN White | serial | 27.4303 | 5.8540 | -78.7% |
| PolarisLiveStage，129307 顶点 | throughput | 12.7950 | 10.9162 | -14.7% |
| PolarisLiveStage | serial | 14.6283 | 14.8020 | +1.2% |

人物 PhysicsFrame 从 20.67 降到 2.25 ms/帧；姿态、同步、回读耗时显著下降，
主步进仍约 2 ms。这支持重复模块查询是此人物样例主要浪费的判断。
舞台没有同等数量的骨骼/刚体数学运算，收益较小，且串行测试没有提升。
舞台透明排序仍约 5 ms 以上 CPU 时间，应另开一轮优化。

这不是 MMD / Nex 的同条件跑分，也不是播放窗口实测 FPS；不能外推到所有
模型、播放状态、MME 特效或显卡。测试期间有其他桌面任务，舞台数据波动
尤其明显；人物的大幅提升在前后交替测量中可重复。

## 正确性验证

- 六项回归通过：d3dx_import_regressions、effect_api_regressions、
  bone_projection_regressions、model_alpha_regressions、pmm_reload_tests、
  morph_runtime_tests。
- 新增 d3dx_import_tests 在测试进程自身的 IAT 安装临时函数，验证 Api 已
  初始化后仍能读取新挂钩、调用挂钩，并在恢复后重新读取原入口。
  这验证导入刷新契约，不等于实际 MMEffect 全场景测试。
- 普通构建的 PERF 默认仍为 OFF。最终隔离 Release 也显式关闭 PERF。
  关闭计时后人物 throughput 复测为 5.4822 ms/帧，原始数据末项单独记录。
- 主 build/Release/MikuMikuDance.exe 被现有进程占用，链接失败；未关闭
  该程序，使用 build-perf/profile-opt/Release 输出新版，避免覆盖正在使用的文件。
  该目录已补齐原构建中的 Data 运行资源，未复制 MME 代理。

## 复现

先按 PERFORMANCE_PHASES_2026-09-20.md 配置优化 Release 和性能计时。
保存未修改 d3dx_dyn.hpp 的 benchmark exe，再仅应用本轮模块句柄缓存并
重建。将两个 exe 放在同一测试目录，旁边放置 Data，分别执行：

```powershell
.\baseline.exe <模型绝对路径> --throughput
.\candidate.exe <模型绝对路径> --throughput
```

通过 `ctest --test-dir build-perf/profile-opt -C Release --output-on-failure`
运行已构建的回归目标；如仅构建本报告六项，应使用 `-R` 筛选这些目标。
交付构建关闭计时后不输出阶段数据。
