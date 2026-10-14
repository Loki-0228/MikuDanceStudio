# 场景性能测量记录

本页按功能归并实现、诊断与验收资料。各记录中的「当前」「已完成」只适用于其注明的源码、构建和测试条件；当前使用入口见[文档导航](../../README.md)。

## 记录索引
- [MMS 与 MMD 9.26 的 MME 性能对照](#record-mme-926-performance)
- [Stage.pmm: MME buffer access and repeated transparent sorting](#record-mme-frame-performance)
- [Stage.pmm frame performance](#record-stage-frame-performance)
- [StageNoRay LowEnd PMM frame performance](#record-pmm-frame-performance)
- [Grass 帧率对照记录](#record-grass-performance)
- [分阶段性能测量（2026-09-20）](#record-performance-phases)
- [Performance comparison, 2026-09-20](#record-performance)
- [MMD / NexGiMa 性能逆向笔记（2026-09-20）](#record-reverse-performance)

<a id="record-mme-926-performance"></a>
## MMS 与 MMD 9.26 的 MME 性能对照

原始记录：`MME_926_PERFORMANCE_20261008.md`。其中的版本、日期和验证范围沿用原记录。

本轮修复减少了未使用网格缓存时的查询，以及 CPU 蒙皮中重复的描边计算，并将几何工作线程上限从 4 调整为 8。最终候选在同条件后台对照中为 **17.5 FPS**，参考目录中的 **MMD 9.26 CHS 为 24 FPS**；两侧 FPS 中位数之比为 **72.9%**。这不是已经追平原版的结论，也不能与用户截图中的 12/38 FPS 直接计算提升幅度。

用户在本轮明确允许以 9.26 本体的帧率比例验收，不再要求继续进行旧 MMS 与新 MMS 的性能基线对照。此前采集的基线记录仍保留在本地产物目录，不作为最终帧率比例的依据。

<a id="record-mme-926-performance-修改"></a>
### 修改

- `native_mesh::RenderModel`、`RenderAccessory` 在空槽位或替身表为空时直接返回。按显示顺序遍历模型和配件会反复访问空槽位；此前这些访问仍执行 `unordered_map::find`。有烘焙替身时保留原来的查找及启用状态判断。
- PMX 蒙皮将描边颜色和尺寸整理为每材质的只读表，每帧重新生成。颜色的浮点转换从每顶点一次减少到每材质一次；材质表情、强制不透明的阴影分支、描边乘法顺序不变。
- 几何计算使用最多 8 个 OpenMP 工作线程，仍服从 `omp_get_max_threads()`；没有 OpenMP 时仍为 1。小网格的原有串行门槛不变。
- 后台帧基准增加 `warmup_complete` 标记，使进程采样可以避开自动蒙皮校准和 MME 首次着色器编译。该标记不进入正式 GUI 程序。

<a id="record-mme-926-performance-定位证据"></a>
### 定位证据

稳定阶段的 256 次主线程堆栈采样中，61 次停在 `RenderModel`，17 次停在 `RenderAccessory`，合计约 30%。调用者包括模型材质、描边及配件顺序遍历。此前在“加载完成”后立即取得的采样仍含 MME 编译或自动校准，不能用来判断持续绘制瓶颈。

同时读取了用户运行的 9.26 堆栈，并核对其 EXE、MME 和 MMHack。用户的 MMS 当时未加载 MMV4MMS；本次性能测试也采用原生 DX9/MME 路径。WPR 因操作系统的性能采样策略拒绝启动，随后使用进程级只读堆栈采样。没有修改系统策略或用户运行的程序。

<a id="record-mme-926-performance-本体帧率对照"></a>
### 本体帧率对照

最终记录：`.local/artifacts/mme-926-performance/native-fps-06/result.json`。执行脚本：`.local/compare_926_fps06.py`。

| 项目 | 条件 |
| --- | --- |
| 场景 | `D:/MMD-Next/tests/ToonGlass_Ray/UserFile/Stage.pmm` |
| 场景 SHA-256 | `44041f9a1b88d2d2f10202e089a3b036a83cd0872e0cf1c83221d112de2986ba` |
| 模型与帧号 | 两侧均为 11 个模型，静止在第 0 帧 |
| 相机 | 两侧每个样本中的位置、距离及三轴角度完全相同 |
| 视区 | 两侧均为 `(259,38)-(1059,488)`，800×450 |
| 客户区 | 两侧读取结果均为 1853×1212；与视区坐标的差异来自 DPI 虚拟化 |
| 窗口 | 正常窗口以 `SW_HIDE` 启动，未最小化、未置顶 |
| 采样 | 正常 GUI 消息循环，预热后每秒读取程序自身 FPS 计数，共 30 个样本 |
| 帧率限制 | 所有计入样本的两侧设置均为 1000 FPS，无实际限帧等待 |
| GPU / CPU | RTX 4070 Laptop GPU / i9-13900HX，24 核 32 线程 |

两个测试副本同时运行，原版使用参考目录的 EXE 和 Data 资源。两侧使用一致的窗口配置以及相同的 MME、MMHack、D3D9 代理 DLL；未关闭特效、物理或描边。最终运行前已确认用户原先打开的 9.26 和 Blender 均已退出，仅两份测试程序同时运行。因此数据仍包含两者之间的资源竞争，不代表单独运行时的绝对帧率。

| 程序 | FPS 中位数 | 采样范围 |
| --- | ---: | ---: |
| MMD 9.26 CHS | 24 | 20–26 |
| MMS | 17.5 | 13–20 |

两侧中位数相除为 72.9%；逐时刻相除后取中位数为 72.4%。因此本轮可表述为“约为原版的 73%”，不把小数当作稳定精度。这是原生一秒 FPS 计数，未用 CPU 提交时间或 GPU 排空耗时的倒数冒充 FPS，也不是播放不同动画帧的测量。

早期 `native-fps`、`native-fps-02` 因视区未匹配而拒绝输出比例。`native-fps-03` 的侧栏配置不同，仅作诊断。`native-fps-04` 已统一配置，4 线程候选为 14/19 FPS；它的客户区高度与最终运行不同，不能据这些运行单独量化增加线程的收益。`native-fps-05` 的最终 8 线程候选曾测得 14.5/18 FPS（80.6%），但当时仍有用户原版和 Blender 运行；关闭后重测得到本报告采用的 `native-fps-06`。两次差异说明机器负载会影响帧率比例，不能只选比例较高的一轮。

<a id="record-mme-926-performance-正确性与限制"></a>
### 正确性与限制

- 角色 `SourRIN/White`、草地 `Grass/HGrass`、树木 `ToonTree/Tree` 的主体和描边缓冲，描边预计算修改前后逐字节相同，分别核对 1,820,256、30,151,808、3,038,784 字节。原始记录见 `buffer-comparison.json`。
- Alembic 集成检查覆盖局部顶点烘焙、完整网格烘焙、缓存回放、PMM 重新打开、清除缓存后恢复骨骼动画以及配件缓存。
- 角色的 5 组姿态覆盖 BDEF1/2/4、SDEF、表情及骨骼缩放；CPU/DX12 核心和 D3D9 回读桥接检查通过，描边颜色一致，缓冲对象身份保留。
- `skinning_backend_tests` 在菜单文案断言处失败：测试期望“自动（按模型选择）”，现有菜单为“自动（逐模型测速选优）”。这两个字符串在修改前的 `a61bfdc` 中已不一致。本轮未修改菜单或掩盖该失败，亦未声称完整测试集通过。
- 尚有约 27% 的相对帧率差距；未把本轮改动称为全面追平 9.26。MMV4MMS 渲染路径不在这组性能数据内。

<a id="record-mme-926-performance-构建与本地产物"></a>
### 构建与本地产物

源码基于 `a61bfdcfc67769fb9acc34fb3621dc6185807b58`，使用正式 `build/` 的 VS 2022 x64 Release 配置，`MIKUDANCESTUDIO_PERF=OFF`、`MIKUDANCESTUDIO_DIAG=OFF`，保留 `/O2` 和 OpenMP。独立的计时构建仅用于诊断，未替换正式配置。

最终比较中的二进制：

- 9.26：`72c34b7ab607aef7b18974742ae18ce62f115f6a614bbedc1a4310e4a5406f72`
- MMS：`9c11bb0bc9c87441f68a413336e8f771c2c1c19c5926b7879b2d81a3edb26b4f`

正式程序为 `D:/MMD-Next/MMS/build/Release/MikuMikuDance.exe`。被替换的程序保存在 `.local/archives/mme-926-performance/`。原始 JSON、日志、渲染目标回读图、堆栈以及独立测试程序均保存在 `.local/artifacts/mme-926-performance/`；最终功能检查位于 `final-native-regression/` 和 `final-skinning-regression/`。详细二进制与源码状态另见同目录的 `final-manifest.json`。

<a id="record-mme-frame-performance"></a>
## Stage.pmm: MME buffer access and repeated transparent sorting

原始记录：`MME_FRAME_PERFORMANCE_2026-09-20.md`。其中的版本、日期和验证范围沿用原记录。

The user now measures approximately 18 FPS in the port and 35 FPS in original
MMD on `Stage.pmm`, with little remaining difference without MME. This round
targets MME. It follows the separate dialog-language fix `3e8a064`.

<a id="record-mme-frame-performance-controlled-comparison"></a>
### Controlled comparison

Render baseline: `5a6485f`. Full project:
`极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm`.
All four runs load 11 models, 170,264 vertices and 15 accessories, with zero
missing dialogs and MME loaded. Hardware: RTX 4070 Laptop GPU, adapter 0,
hardware vertex processing, 859 x 482 viewport. The same release MME DLLs,
runtime Data and original project/effect files are used in separate directories.

Release x64 `/O2 /Ob2 /DNDEBUG`, 30 warmup frames and 150 measured frames,
fixed editor delta 1/30, GPU drained at batch boundaries. Two runs per build
in before/after/after/before order. PERF instrumentation is enabled for these
measurements and disabled for the delivered GUI release. Adjacent JSON retains
the raw outputs, individual experiments and reference/project hashes.

| Build | Individual ms/frame | Mean ms/frame | Throughput equivalent FPS |
|---|---|---:|---:|
| Before | 56.0551, 55.8502 | 55.9527 | 17.87 |
| After | 38.3227, 37.9829 | 38.1528 | 26.21 |

Mean frame time falls 31.8%; throughput increases **46.7%**. These are harness
throughput measurements, not GUI playback FPS. Original MMD has not been run
under this same harness, so this does not establish parity with its reported
35 FPS. No effects, models, accessories, physics updates or render rounds are
removed. Both versions execute 23,100 model sort visits / 150 frames (11 models
x 14 rounds), 4,200 buffer-lock/sort processing visits and 47,850 material-function calls.

| Inclusive phase | Before ms/frame | After ms/frame |
|---|---:|---:|
| Transparent sorting, including locks | 21.3511 | 10.5117 |
| Sort CPU section | 16.8618 | 9.4815 |
| Sort locks | 3.9721 | 0.5302 |
| MME-hooked BeginScene | 15.3223 | 9.8957 |

Nested times must not be summed. Timing all 37 host exports separately showed
only 0.5264 ms/frame across 1,389,450 calls / 150 frames before this change.
Optimizing slot lookups alone therefore would not explain the BeginScene cost.

<a id="record-mme-frame-performance-what-the-original-mmd-path-shows"></a>
### What the original MMD path shows

Read-only disassembly of the supplied 9.26 CHS x64 binary confirms:

- `0x14002A3F2` initializes the render-repeat count to one; `0x14002A40C`
  invokes the D3D9 device's BeginScene at vtable offset 0x148.
- `0x14002A4F0` decrements the repeat counter; `0x14002A570` calls the effect
  model renderer `0x1400A1E90`. `0x14002A5AD..0x14002A5B7` reloads the counter
  and loops, preserving repeats added by MME. These rounds are part of the
  plugin protocol, not something the port can simply remove.
- `0x1400A369D` calls the effect material routine `0x1400B8790`.
  In the inspected material body through `0x1400B9F0E`, vertex and index
  buffers are bound at `0x1400B9DEE` / `0x1400B9E02`, followed by indexed draw
  at `0x1400B9E43`. This body has no per-triangle depth-sort or buffer-lock
  path. This is a conclusion about the inspected path, not every MMD feature.

Reproduce with `tools/inspect_reference_pe.py` and the corresponding explicit
VA ranges. Temporary disassembly is under ignored `build-reverse/`.
The new exact-input sort cache below is the port's implementation, not a claim
that original MMD uses such a cache.

<a id="record-mme-frame-performance-changes-and-why-they-help"></a>
### Changes and why they help

The port adds transparent triangle sorting, which reads main-vertex positions
between MME passes. It had retained WRITEONLY main vertex buffers and used a
writable lock even though sorting changes only indices. The actual access and
the declared access therefore disagreed. A writable managed-buffer lock also
marks vertex data dirty and can provoke another upload on a later draw.

PMD and PMX main vertex buffers now remain managed but are readable, and sorting
uses `D3DLOCK_READONLY`. Outline buffers remain write-only. Resource output slots,
MME IDs and lifetimes are preserved; this does not introduce new default-pool
resources or new device-reset ownership. These flags follow Microsoft's
[vertex-buffer access rules](https://learn.microsoft.com/en-us/windows/win32/direct3d9/accessing-the-contents-of-a-vertex-buffer).

The read-only change alone measured 40.3021 ms/frame. An earlier cache-only
trial reduced sort CPU time to 11.76 ms but barely changed whole-frame time
(55.84 ms), as waiting moved elsewhere. Combining both changes measured 38.31 ms
in the exploratory run and 38.15 ms in the final alternating comparison.

The cache lives for one D3D9 render scope and is enabled only when MME is loaded.
It stores exact original indices, triangle depth keys and sorted output for
each material range. Every call still reads current vertex data, checks finite
depths, and compares the entire source-index sequence and exact depth-key
sequence. A change in any of those inputs causes a full stable sort. Equal-depth
order stays stable. No lossy hashes, approximate camera tests, assumed static
geometry or model pointers retained across frames are used.

Material workers own separate cache entries, allocated on the render thread
before dispatch. Cached array payload is limited to 32 MiB per frame; oversized
requests and changed material range lengths fall back to the complete sorter.
Nested render scopes restore the outer cache. With no MME module, the cache is
disabled. Index-buffer writes and MME callbacks remain on their original paths.

<a id="record-mme-frame-performance-validation-and-limits"></a>
### Validation and limits

- Final PERF=OFF release: eight regression suites pass, including language,
  actual modal dialogs, transparency, MME exports and resource lifetime.
- The final MME build completes three Stage reset/reopen cycles in Japanese,
  English and Chinese, each loading all 11 models and 15 accessories, rendering
  12 physics frames with heap validation, and cleaning up successfully. Total
  time: 51.459 seconds; individual loads: 1.791 / 1.244 / 1.209 seconds.
- A final no-MME Stage smoke test loads the complete scene with zero dialogs,
  renders 180 frames and exits normally. Its measured throughput is 6.9561 ms
  per frame; no before/after no-MME speedup is claimed in this round.
- Cached ordering matches the stable reference for 16/32-bit indices and size
  boundaries through 16,384 triangles. Additional cases cover repeated passes,
  camera changes, changed positions, source-order changes, nonfinite input,
  concurrent callers, nested scopes, changed ranges and cache-budget fallback.
- All four complete Stage runs exit normally and save a rendered image. Visual
  inspection retains the character, stage, outlines and main effect appearance.
  Images are not pixel-identical: repeat runs of the same executable also differ
  (viewport RGB mean absolute differences approximately 4-6 / 255; cross-version
  approximately 6-8 / 255). Thus these captures are a visual regression check,
  not proof of exact pixel parity for time-varying effects. The fixed 30 FPS HUD
  reflects the harness delta and must not be used as the measured FPS.

Reproduction uses the benchmark executable under `tests/mme-benchmark/Release`,
with the release MME DLLs and Data next to it:

```text
MikuMikuDance.exe "<absolute project path>/Stage.pmm" --throughput-skip-missing "<output path>/frame.bmp"
```

The flag permits missing-resource handling, but all recorded runs report zero
dialogs. The project and effect files are not modified.

<a id="record-stage-frame-performance"></a>
## Stage.pmm frame performance

原始记录：`STAGE_FRAME_PERFORMANCE_2026-09-20.md`。其中的版本、日期和验证范围沿用原记录。

The user confirmed that `Stage.pmm` is the scene used for the reported original
MMD 35 FPS / port 9 FPS comparison. This round uses that complete project and
measures an improvement over port commit `a36953d`. Original MMD has not been
measured under the harness conditions; the numbers below are not GUI FPS and
do not establish parity with original MMD.

<a id="record-stage-frame-performance-workload-and-method"></a>
### Workload and method

- Project: `极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm`.
- All runs load 11 models / 170,264 vertices / 15 accessories, with zero missing
  resource dialogs. No models or accessories are skipped.
- D3D9 with the release MME DLLs and Data files; MME load is checked by the harness.
- RTX 4070 Laptop GPU, adapter 0, hardware device, viewport 859 x 482.
- Release x64 `/O2 /Ob2 /DNDEBUG`; diagnostic logging disabled.
- 30 warmup / 150 measured frames, fixed editor delta 1/30, GPU drained at batch
  boundaries. No message-loop frame cap. PERF is enabled for measurements and
  disabled in the delivered release.
- Baseline/new/new/baseline order, two samples per build. The raw outputs, final
  phase profile and three-cycle PMM reload result are stored in the adjacent JSON.

| Build | Individual ms/frame | Mean ms/frame |
|---|---|---:|
| Baseline `a36953d` | 67.3302, 58.3883 | 62.8593 |
| Scoped object validation | 55.9963, 56.0382 | 56.0173 |

The ratio of mean times is **1.122x**, approximately **12% higher throughput**
(10.9% lower frame time). Baseline variation is substantial; this is a short-run
measurement, not a guaranteed gain across sessions, machines or viewport sizes.

<a id="record-stage-frame-performance-change"></a>
### Change

MME requests 14 model-render rounds per frame in this scene. Across those rounds,
the renderer performs 1,464 D3D resource validity checks per frame. Each original
check performs two `VirtualQuery` calls, even for an object already validated in
the same render scope.

Successful checks are now reused within a D3D9 render scope. Each cached COM
object is retained with `AddRef` until that scope ends, preventing its address
from being freed and reused during the cache lifetime. Repeated validation
checks its current vtable against the cached vtable. New pointers and changed
vtables receive the full original validation. Outside rendering, validation is
unchanged; a full 256-entry table also falls back to uncached checks. Nested
scopes own independent references and restore the previous scope on exit.

No model, accessory, effect pass, physics update or outline is removed. Resource
checks still execute 1,464 times per frame; measured time in that phase is now
0.23-0.66 ms/frame in the comparison runs. Some removed CPU overhead appears as
additional index-buffer lock waiting, so the full-frame result is used rather
than treating isolated CPU savings as an FPS gain.

<a id="record-stage-frame-performance-validation"></a>
### Validation

- COM lifetime regression: unscoped checks, repeated checks, owner release,
  nested scopes, table overflow with 300 objects, and final reference cleanup.
- `model_alpha_regressions`, `pmm_reload_tests`, `morph_runtime_tests`,
  `effect_api_regressions` and `d3dx_import_regressions` pass with PERF both on
  and off (5/5 for the final release build).
- Full Stage with release MME: three reset/reopen cycles in Japanese, English
  and Chinese; each loads all 11 models and 15 accessories, renders 12 physics
  frames, passes heap validation and exits cleanly. Final PERF=OFF load times
  were 1.77, 1.31 and 1.30 seconds; these are warm local measurements, not
  cold-start guarantees. An initial PERF=OFF run reached the harness's 55-second
  limit during the third render cycle. Repeating with a 150-second limit and
  live logging completed all checks and cleanup in 57.49 seconds (exit 0).

<a id="record-stage-frame-performance-remaining-measured-work"></a>
### Remaining measured work

The final instrumented run measured 57.67 ms/frame. Inclusive phase timings
include approximately 17.33 ms of transparent-sort CPU work, 5.33 ms of sort
buffer locking, and 13.94 ms inside MME-hooked `BeginScene`. These scopes can
include nested work and must not all be summed. Scope reference release costs
approximately 0.0024 ms/frame. The `BeginScene` measurement locates time at the
hook boundary; it does not yet establish which internal MME operation or GPU
wait is responsible.

An intermediate CPU index-staging experiment failed to improve the separate
LowEnd workload and was reverted before this Stage comparison.

<a id="record-stage-frame-performance-reproduction"></a>
### Reproduction

Build `frame_benchmark_mme` with `MIKUDANCESTUDIO_PERF=ON` and place the release
MME DLLs and runtime Data next to its executable. From that output directory:

```text
MikuMikuDance.exe "<absolute path>/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm" --throughput-skip-missing
```

This is the benchmark executable under `tests/mme-benchmark/Release`, not the
GUI executable. The flag allows missing-resource skipping, but the recorded
Stage runs report zero dialogs and include the complete scene.

<a id="record-pmm-frame-performance"></a>
## StageNoRay LowEnd PMM frame performance

原始记录：`PMM_FRAME_PERFORMANCE_2026-09-20.md`。其中的版本、日期和验证范围沿用原记录。

The user later clarified that the reported original MMD 35 FPS / port 9 FPS
comparison uses `Stage.pmm`, not this LowEnd project. These LowEnd measurements
remain a separate regression workload; see [STAGE_FRAME_PERFORMANCE_2026-09-20.md](benchmarks.md#record-stage-frame-performance)
for the full Stage comparison. Neither round establishes FPS parity with
original MMD, which has not been measured with this harness.

<a id="record-pmm-frame-performance-controlled-comparison"></a>
### Controlled comparison

- Unmodified `StageNoRay LowEnd.pmm`; explicitly skip missing `Null_00`.
- All runs load 3 models / 168,596 vertices / 10 accessories.
- Release x64 `/O2 /Ob2 /DNDEBUG`, RTX 4070 Laptop GPU, viewport 859 x 482.
- Baseline render code is commit `817659e`. The MME baseline was built from
  an isolated Git archive with only the current benchmark/CMake harness added.
- Same MME DLLs and Data files as the release installation, in isolated folders.
- 30 warmup / 150 measured frames, fixed editor delta 1/30, GPU drained at the
  measured batch boundaries. No message-loop cap; these are throughput timings,
  not playback FPS or the FPS displayed by either GUI.
- Two samples per build in baseline/new/new/baseline order. Raw outputs are
  in [PMM_FRAME_PERFORMANCE_2026-09-20.json](data/pmm-frame-performance.json).

| Path | Baseline ms/frame | New ms/frame | Throughput ratio |
|---|---:|---:|---:|
| D3D9 without MME | 9.96 | 6.33 | 1.57x |
| D3D9 with release MME | 44.86 | 23.84 | 1.88x |

MME new samples are 26.09 and 21.60 ms (baseline 44.41 and 45.32 ms).
These are short-run measurements, with variation in GPU waits and other frame
work; they are not a guaranteed gain on other machines or viewport sizes.

<a id="record-pmm-frame-performance-bottleneck-and-changes"></a>
### Bottleneck and changes

The MME scene executes seven model-render rounds per frame. The baseline
spends 28.09 ms/frame in transparent sorting versus 3.91 ms without MME.
New MME sorting averages 10.02 ms/frame. Effects and pass counts are preserved.

- Sort compact 8-byte depth-key/source-offset pairs instead of carrying all
  three indices and depth through each radix pass. Three stable radix passes
  replace four, preserving the exact float depth arithmetic and equal-depth order.
- Sort independent material index ranges in parallel for sufficiently large
  workloads. All D3D calls and MME callbacks remain on the render thread.
- Skinning and sorting share a maximum four-worker team, respecting a lower
  OpenMP thread limit. Small PMX meshes now use the same serial threshold as
  PMD. This avoids full-machine/small-team switching between short tasks.

The four-worker limit is a measured conservative choice for this workload,
not a claim that four threads is optimal for every machine or mesh.
Skinning alone may be slower with fewer workers; full-frame throughput is the
acceptance metric. No physics steps, models, effects or outline passes are removed.

An exact-input within-frame sort cache was tried and discarded: full buffer
validation/copying raised MME frame time to approximately 30 ms. No cache or
diagnostic matrix trace from that experiment remains in the implementation.

<a id="record-pmm-frame-performance-validation-and-reproduction"></a>
### Validation and reproduction

- Stable-sort parity with 16/32-bit indices, positive/negative/tied depths,
  size boundaries up to 16,384 triangles, nonfinite rejection, and four
  concurrent callers using independent thread-local scratch.
- Before/after skin-only main and outline buffers are byte-identical for
  `SourRIN/White.pmx` (1,820,256 bytes) and the 60,000-vertex PMD fixture
  (2,880,000 bytes), with nonidentity bone transforms.
- Final PERF=OFF Release: `model_alpha_regressions`, `pmm_reload_tests`,
  `morph_runtime_tests`, `effect_api_regressions` and `d3dx_import_regressions`
  all pass (5/5).
- With release MME DLLs, three real PMM reset/reopen cycles in Japanese,
  English and Chinese each load 3 models / 10 accessories and render 12
  physics frames successfully; process exits cleanly.
- The benchmark adds `--throughput-skip-missing`; unexpected dialogs still fail.
  `frame_benchmark_mme` is an opt-in executable named `MikuMikuDance.exe` with
  the required exports and a mandatory MME-loaded check.

Build with `MIKUDANCESTUDIO_PERF=ON` for phase timing. Place runtime Data next
to the benchmark; only the MME target receives the release MME DLLs. Run:

```text
frame_benchmark.exe "<absolute path>/StageNoRay LowEnd.pmm" --throughput-skip-missing
MikuMikuDance.exe "<absolute path>/StageNoRay LowEnd.pmm" --throughput-skip-missing
```

The second command is specifically the MME benchmark executable, not the GUI
release. The delivered GUI release is built with phase timing disabled.

<a id="record-grass-performance"></a>
## Grass 帧率对照记录

原始记录：`GRASS_PERFORMANCE_2026-09-24.md`。其中的版本、日期和验证范围沿用原记录。

<a id="record-grass-performance-修复前后-release-build-对照"></a>
### 修复前后 Release build 对照

两侧正式 `MikuDanceStudio` 目标均构建成功，采用相同的 VS 2022 x64 Release、`/O2 /Ob2 /DNDEBUG`，PERF/DIAG 均关闭。修复前生产代码为 `7d7aa4b`，基线工作树 `b9f8576` 只增加测试工具，`src` 未变；修复后构建对应 `bc75033`。后续提交只调整测量工具和文档，没有修改被测程序。

| Build | EXE 字节数 | SHA-256 |
| --- | ---: | --- |
| 修复前 | 1,659,392 | `94fe17f1c2abb56081915c6335d5e238a6dde5d4d73314fba3722c994ce03253` |
| 修复后 | 1,665,024 | `001a2e318396eab44a88344279a623aaeba8c3a5aaee7abaffb709f657f73295` |

两份程序放在独立运行目录，使用同一个 `MMDxShow.dll`，不加载 MME。复用原版对照的八个 Grass PMM，计划按 1/2/4/8 模型、平铺/重叠、每组一次 ABBA 运行，每次读取 12 秒原生整数 FPS。环境、源码版本、构建日志、EXE/依赖/场景/脚本哈希保存在 `D:\MMD-Next\.alpha-perf-artifacts\build-compare-final\build-provenance.json`。

首批 `native-abba` 完成 32 次运行，但截图核验发现部分窗口被聊天窗口遮挡，整批排除，不用于最终帧率结论。下表仅保留这一排除批次的原始计数，不能据此认定修复带来加速或回退。

| 布局 | 模型数 | 修复前 FPS（排除批次） | 修复后 FPS（排除批次） |
| --- | ---: | ---: | ---: |
| 平铺 | 1 | 109 | 108.5 |
| 平铺 | 2 | 60.25 | 60.5 |
| 平铺 | 4 | 31.5 | 32.5 |
| 平铺 | 8 | 14.25 | 15 |
| 重叠 | 1 | 107 | 106.5 |
| 重叠 | 2 | 58.5 | 58.5 |
| 重叠 | 4 | 30 | 30.25 |
| 重叠 | 8 | 14 | 14 |

采样脚本随后加入临时置顶及每次采样的中心、四角可见性检查；短运行通过。补测 `native-topmost-abba` 在第一轮第 11 次采样发现窗口最小化，立即中止，没有有效汇总值。两批原始记录均保留。用户明确要求停止测试，后续改用后台方法，并将修复前后 build 对照作为验收标准；窗口补测不再继续，后台播放 FPS 的完整对照尚未完成。

<a id="record-grass-performance-原版-mmd-932-对照"></a>
### 原版 MMD 9.32 对照

两侧通过正常窗口装载同一个 PMM 并播放。每份 `HGrass.pmx` 独立装载，视口为 1280×720，关闭帧率上限。脚本逐项核对相机、模型数量和全部骨骼矩阵，并确认播放过程中静态姿态没有变化。八组场景的首个原版与修复版视口截图逐像素一致。

原版可执行文件 SHA-256 为 `07516fd3bf1e6b1339836b6773a156f61bdd6f848eeb621fdda012375df313a1`。修复版来自 `codex/alpha-occlusion-perf`，生产修改截至 `7a53c87`，未包含排序缓存实验。各次运行另保存完整的 EXE、PMM 哈希。

每个配置按原版、修复版、修复版、原版的顺序运行一次。每次读取窗口内部的一秒 FPS 计数 12 次，下表为两次运行中位数的中位数。这个计数经过整数取整，不提供帧耗时 P95，也不能分辨 2% 的小差异。

| 布局 | 模型数 | 原版 MMD FPS | 修复版 FPS |
| --- | ---: | ---: | ---: |
| 平铺 | 1 | 101.5 | 88.5 |
| 平铺 | 2 | 47 | 47 |
| 平铺 | 4 | 24 | 22 |
| 平铺 | 8 | 12 | 12.5 |
| 重叠 | 1 | 89.25 | 83.5 |
| 重叠 | 2 | 42.5 | 42 |
| 重叠 | 4 | 21.5 | 21.5 |
| 重叠 | 8 | 11 | 11 |

原版单模型平铺的两次读数为 109、94 FPS，修复版为 91、86 FPS；四模型平铺的修复版读数为 24、20 FPS。负载波动仍存在。该对照说明多模型场景达到的实际帧率，不足以把版本差异归因于某一项修改。原版与本项目包含不同的历史实现，也不能把它们的差值当作白边修复成本。

本机原始记录位于 `D:\MMD-Next\.alpha-perf-artifacts\reference-compare\native-abba`，包含 32 次运行的计数、相机、骨骼、日志及截图。`image-comparison.json` 保存八组截图差异结果。PresentMon 在本机启动采集时退出且没有生成 CSV，因此本表使用原生 FPS 计数，不声称取得 ETW 数据。

<a id="record-grass-performance-修复前后与优化实验"></a>
### 修复前后与优化实验

独立 `frame_benchmark` 测量整批提交后等待 GPU 完成的平均耗时。这个值的倒数是吞吐率，与上表正常播放窗口的 FPS 不是同一种指标。方法、命令及错误处理见 [多模型帧基准](../../development/benchmarks.md)。

早期正式对照 `fix-abba-pumped` 中，一模型平铺的基线与修复版中位数为 11.6781、11.8089 ms，二模型为 23.66305、23.062 ms。四模型基线从约 44 ms 跃升至 72 ms，同组中位数会产生虚假的 16% 收益。异常样本全部保留，该批不用于证明性能达标。

四模型平铺的最终补测保存在 `native-four-fix-abba-final`。同样两轮 ABBA，基线中位数 55.142 ms/帧，修复版 55.1667 ms/帧，差异约 +0.04%；两轮分别为 -0.15%、+0.08%。同版本极差分别为 2.31%、1.20%，因此该差异应视为噪声范围内持平。这个场景没有显示明显修复成本，仍不能代表所有不透明或透明模型。

透明排序的按需顶点深度缓存实验保存在 `codex/alpha-lazy-depth-rejected`。一模型中位数从 19.38925 增至 20.2191 ms，四模型从 84.32435 增至 86.74465 ms；同版本样本波动大于差值，没有收益证据，未合入。

顺序预计算顶点深度的替代方案保存在 `codex/alpha-sort-depth-experiment`，提交 `9604a43`。对照使用修复分支，两轮 ABBA，每次预热 60 帧并采样 300 帧。原始数据在 `sort-eager-screen`。

| 平铺模型数 | 修复版 ms/帧 | 加入预计算后 ms/帧 | 中位数变化 | 两轮各自变化 |
| ---: | ---: | ---: | ---: | --- |
| 1 | 14.7743 | 15.68175 | +6.14% | +4.57%、+7.17% |
| 4 | 55.45305 | 59.5055 | +7.31% | +6.23%、+8.31% |

同版本极差仍达到约 5% 至 6.4%，不把上述比例当作精确回退率。但每轮方向一致，没有理由接受该优化。实验通过 16/32 位索引、共享顶点、稳定相同深度、顶点变形、异常索引、非有限数和多线程隔离的排序回归；正确性通过不等于性能达标。

阶段分析另用 PERF=ON 构建。四模型平铺样本平均整帧约 44.18 ms，其中蒙皮约 18.92 ms，透明排序约 21.85 ms，资源检查约 0.38 ms，物理约 0.059 ms。这些阶段存在嵌套，不能相加。排序内部更细阶段未测量。HGrass 没有刚体，不能用此结果推断角色物理性能。

不以降低画质、关闭描边、透明排序或物理来获得收益。尚未以专门的不透明压力场景验证 2% 回退目标，现有单组测量也不能保证所有透明场景都满足 5% 成本目标。

实际洛希极限模型另完成两轮 ABBA，仍预热 60 帧、采样 300 帧，未关闭物理。基线和修复版中位数分别为 10.33825、10.44345 ms/帧，相差约 +1.02%；两轮分别为 +0.95%、+1.05%。同版本极差分别为 1.79%、1.02%，原始数据在 `luoxi-fix-abba-final`。这是固定取景下的该模型结果，不是所有角色动作的性能保证。

<a id="record-grass-performance-mme-兼容性对照"></a>
### MME 兼容性对照

四份独立 Grass 模型分别测试平铺与重叠布局，每种配置两轮 ABBA，预热 60 帧、采样 300 帧，共 32 次成功运行。基线为 `b9f8576`，修复侧采样启动时为 `758b4b2`；每份可执行文件哈希保存在 `mme-fix-abba-final/manifest.json`。两侧均关闭 PERF 和 DIAG，没有设置额外的物理或 OpenMP 环境变量。

这里的 Git 提交是运行开始时的工作树 HEAD，不是嵌入二进制的构建标记。基准二进制在文档、工具及 PMM v1 容量修复等后续提交前构建；渲染实现未再改变。以保存的可执行文件 SHA-256 标识实测对象，最终完整重建另记录哈希，不把两者混称同一二进制。

| MME 配置 | 布局 | 基线 ms/帧 | 修复版 ms/帧 | 中位数变化 |
| --- | --- | ---: | ---: | ---: |
| 默认 | 平铺 | 58.7364 | 58.3043 | -0.74% |
| 默认 | 重叠 | 58.15115 | 58.635 | +0.83% |
| `SoftParticleModel.fx` | 平铺 | 58.69415 | 58.9631 | +0.46% |
| `SoftParticleModel.fx` | 重叠 | 58.55155 | 58.94205 | +0.67% |

这组样本没有显示明显回退；这些小差值不作为加速证据。相同版本的极差约为中位数的 0.24% 至 3.72%。原始记录位于 `D:\MMD-Next\.alpha-perf-artifacts\mme-fix-abba-final`。

正常窗口的 MME「效果文件分配」另核实了 `HGrass.pmx` 绑定 `D:\MMD-Next\tests\Grass\SoftParticleModel.fx`，截图保存在 `reference-compare/effect-mapping-wide`。基准使用同一素材路径及 MME DLL。没有随模型装载 `SoftParticleEngineNone.x`，因此不能声称验证了完整软粒子深度交互。

基线负载波动后，按用户追加的建议，将多份数的主对照改为原版正常窗口，完成 1/2/4/8 份的两种布局。独立基准用于核验修复成本及筛选优化，MME 正式对照采用四模型两种布局。最初设想的三模式、四份数、两布局完整独立基准矩阵未全部跑完；完整矩阵脚本予以保留，不把未采样的配置填入结果表。

<a id="record-performance-phases"></a>
## 分阶段性能测量（2026-09-20）

原始记录：`PERFORMANCE_PHASES_2026-09-20.md`。其中的版本、日期和验证范围沿用原记录。

本轮在 dd75137 基础上增加可选计时，不修改渲染、排序或物理算法。默认
`MIKUDANCESTUDIO_PERF=OFF`，普通构建不执行计时。原始输出见同名 JSON。

<a id="record-performance-phases-方法与构建核查"></a>
### 方法与构建核查

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

<a id="record-performance-phases-结果"></a>
### 结果

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

<a id="record-performance-phases-后续修复顺序"></a>
### 后续修复顺序

1. 人物：继续拆分 PhysicsFrame 的姿态更新、刚体同步、stepSimulation、
   回读，确认 21 ms 的具体来源，并核查 Bullet 库的编译优化与步进次数。
   不以关闭物理或减少正确性所需的步进来制造提升。
2. 舞台：减少透明排序重复计算和临时分配，评估 CPU 顶点位置缓存及首个
   draw 前完成排序，分别衡量 CPU 排序和 Lock 等待；保持透明顺序和 MME
   D3D9 资源契约。只移动 Lock 不会消除约 5.7 ms 的 CPU 排序。
3. GPU 蒙皮属于后续架构工作；本次样例的 CPU 蒙皮并非首要瓶颈。

<a id="record-performance-phases-复现与验证"></a>
### 复现与验证

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

<a id="record-performance"></a>
## Performance comparison, 2026-09-20

原始记录：`PERFORMANCE_2026-09-20.md`。其中的版本、日期和验证范围沿用原记录。

The two performance changes have NOT demonstrated a general speedup. The stage
regressed; the character was approximately unchanged. Do not describe the
reported tenfold slowdown against original MMD as fixed.

<a id="record-performance-method"></a>
### Method

- Release x64, same compiler/dependencies, isolated source/build directories.
- Before: 71ddb29. Pool only: 448fbab. Both changes: 91c17e9.
- Identical `tests/frame_benchmark.cpp` linked against each version's core.
- No MME loaded. Each process loads one PMX, runs 30 warmup frames and measures
  150 forced editor frames with DeltaTime=1/30. No animation playback.
- Each timed frame includes FrameDriver and a D3D event-query GPU completion
  wait. This serializes frames: results are latency measurements, NOT normal
  interactive FPS or a measurement of pipelined throughput.
- All runs reported NVIDIA GeForce RTX 4070 Laptop GPU, adapter 0, HAL,
  hardware vertex processing (flags 64), viewport 859x482, one model and no
  accessories. The hidden test window and default editor/physics configuration
  are identical across versions.
- Stage: PolarisLiveStage.pmx, 129307 vertices. Character: SourRIN/White.pmx,
  37922 vertices. Local assets are not distributed in Git.
- Original StageNoRay.pmm attempt was excluded: it references missing controller
  assets and blocked in a file-selection dialog. No partial scene results used.
- GPU power/thermal state and unrelated background activity were not controlled.

<a id="record-performance-results"></a>
### Results

Mean frame milliseconds, lower is better. Each row averages equal-sized runs.

| Experiment | Before | Pool only | Both changes |
|---|---:|---:|---:|
| Stage, 3 runs/version | 13.3018 | - | 15.8863 |
| Character, 3 runs/version | 25.4414 | - | 25.2142 |
| Character repeat, 3 runs/version | 25.8678 | - | 26.2446 |
| Stage ablation, 2 runs/version | 13.0175 | 15.5560 | 15.6372 |

Initial run order: before, after, after, before, before, after.
Ablation order: before, pool, after, after, pool, before.
Raw per-run output (including median and p95) is in PERFORMANCE_2026-09-20.json.

The initial stage frame time increased approximately 19.4%. The ablation also
shows a slower managed-buffer variant in both comparisons; the additional
deferred-lock change has no clear benefit in this model. This does not establish
the exact driver mechanism or measure the opaque-only fast path independently.
Character differences change sign across repeats and are not a stable speedup.

<a id="record-performance-limits-and-next-investigation"></a>
### Limits and next investigation

Original MMD and NexGiMa are not included in this same-driver test: their closed
executables cannot link this harness. No performance ratios against them are
claimed. A separate end-to-end capture with matched scene, playback, viewport,
GPU and frame limit is still needed to reproduce the reported tenfold gap.

Profile CPU stacks and GPU submission/wait time before further optimization.
In particular, investigate managed-buffer updates combined with the subsequent
transparent-pass re-lock; matching the original pool alone is insufficient.

<a id="record-performance-reproduction"></a>
### Reproduction

Configure with MIKUDANCESTUDIO_BUILD_UI_TESTS=ON, build the `frame_benchmark`
target in Release, and copy runtime Data assets beside the test executable.
Run `frame_benchmark.exe <absolute-path-to-model.pmx>` outside the filesystem
sandbox so D3D can access the GPU. Use separate processes and alternate versions.
The harness does not save models/scenes. Apply an external timeout for missing
asset dialogs. Keep diagnostics environment variables unset.

<a id="record-reverse-performance"></a>
## MMD / NexGiMa 性能逆向笔记（2026-09-20）

原始记录：`REVERSE_PERFORMANCE_2026-09-20.md`。其中的版本、日期和验证范围沿用原记录。

本轮只增加分析工具与报告，不修改渲染代码。结论来自本地二进制、NexGiMa
随包 HLSL/CSO 和本工程源码，不代表已经复原两款程序的完整源码或测得优化收益。

用户补充：NexGiMa 不支持 MME。因此分别研究 MMD 兼容路径与原生渲染路径，
不能把 NexGiMa 的快速原生渲染等同于已解决 MME 兼容问题。

<a id="record-reverse-performance-样本与方法"></a>
### 样本与方法

| 样本 | SHA-256 |
|---|---|
| ref/MikuMikudance 926 CHS/MikuMikudance.exe | 72c34b7ab607aef7b18974742ae18ce62f115f6a614bbedc1a4310e4a5406f72 |
| ref/NexGiMa_ver_2022-12-18_373/NexGiMa.exe | dcea56dd56df4b6062dc440da55008e3a9c09e6798ac66fddd83a29d8e1c5e22 |

两者均为 x64，首选 ImageBase=0x140000000。下文地址为首选 VA，实际进程需考虑
ASLR。926 的地址不能直接套用到源码注释中的 932 地址。

工具 `tools/inspect_reference_pe.py` 使用 pefile、Capstone、系统 D3DDisassemble，
读取导入表、交叉引用、MSVC RTTI 与 .pdata。未执行、注入或修改参考 exe。
关键指令节选保存于 [REVERSE_PERFORMANCE_EVIDENCE.txt](data/reverse-performance-evidence.txt)。
完整临时反汇编在被 Git 忽略的 `build-reverse/`。

注意：.pdata 条目可能只是同一函数的 chained-unwind 片段；工具输出的 range
不能自动当成完整源函数。虚表偏移也必须结合对象来源和参数确认，单凭 call +0x70
不能区分 Dispatch、Queue::Signal 或其他对象方法。

<a id="record-reverse-performance-1-mmdpmd-也经过并行蒙皮而当前实现遗漏了这一点"></a>
### 1. MMD：PMD 也经过并行蒙皮，而当前实现遗漏了这一点

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

<a id="record-reverse-performance-2-mmd观察到的材质绘制路径不含当前新增的三角形重排"></a>
### 2. MMD：观察到的材质绘制路径不含当前新增的三角形重排

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

<a id="record-reverse-performance-3-nexgima可核实的-gpu-蒙皮数据流"></a>
### 3. NexGiMa：可核实的 GPU 蒙皮数据流

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

<a id="record-reverse-performance-4-nexgima描边在顶点着色器扩张"></a>
### 4. NexGiMa：描边在顶点着色器扩张

随包 `Shaders/mmd.hlsl:281` 的 VSEdgeMain 根据相机距离、FOV、材质 edgeSize、
morph 系数及顶点 edgeBias 计算描边，然后执行 position + normal * edgeWeight。
它使用同样的输入顶点布局。当前工程 SkinPmx/SkinPmd 会在 CPU 生成单独的
16 字节 EdgeVertex 并更新描边 VB。

**可借鉴：**原生后端可在 shader 里扩张描边，避免 CPU 为相机相关描边反复改写
几何。不要将该公式直接替换 MMD 公式：两者画面语义需要验证，MME 描边钩子
和预期的顶点布局也需要兼容设计。

<a id="record-reverse-performance-5-nexgima提交与等待分开并支持批量-command-list"></a>
### 5. NexGiMa：提交与等待分开，并支持批量 command list

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

<a id="record-reverse-performance-应用顺序与-mme-边界"></a>
### 应用顺序与 MME 边界

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

<a id="record-reverse-performance-api-核对与复现"></a>
### API 核对与复现

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
