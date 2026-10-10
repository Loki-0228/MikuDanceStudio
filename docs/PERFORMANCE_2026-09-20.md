# Performance comparison, 2026-09-20

The two performance changes have NOT demonstrated a general speedup. The stage
regressed; the character was approximately unchanged. Do not describe the
reported tenfold slowdown against original MMD as fixed.

## Method

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

## Results

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

## Limits and next investigation

Original MMD and NexGiMa are not included in this same-driver test: their closed
executables cannot link this harness. No performance ratios against them are
claimed. A separate end-to-end capture with matched scene, playback, viewport,
GPU and frame limit is still needed to reproduce the reported tenfold gap.

Profile CPU stacks and GPU submission/wait time before further optimization.
In particular, investigate managed-buffer updates combined with the subsequent
transparent-pass re-lock; matching the original pool alone is insufficient.

## Reproduction

Configure with MIKUDANCESTUDIO_BUILD_UI_TESTS=ON, build the `frame_benchmark`
target in Release, and copy runtime Data assets beside the test executable.
Run `frame_benchmark.exe <absolute-path-to-model.pmx>` outside the filesystem
sandbox so D3D can access the GPU. Use separate processes and alternate versions.
The harness does not save models/scenes. Apply an external timeout for missing
asset dialogs. Keep diagnostics environment variables unset.
