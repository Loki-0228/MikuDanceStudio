# StageNoRay LowEnd PMM frame performance

The user later clarified that the reported original MMD 35 FPS / port 9 FPS
comparison uses `Stage.pmm`, not this LowEnd project. These LowEnd measurements
remain a separate regression workload; see `STAGE_FRAME_PERFORMANCE_2026-09-20.md`
for the full Stage comparison. Neither round establishes FPS parity with
original MMD, which has not been measured with this harness.

## Controlled comparison

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
  in `PMM_FRAME_PERFORMANCE_2026-09-20.json`.

| Path | Baseline ms/frame | New ms/frame | Throughput ratio |
|---|---:|---:|---:|
| D3D9 without MME | 9.96 | 6.33 | 1.57x |
| D3D9 with release MME | 44.86 | 23.84 | 1.88x |

MME new samples are 26.09 and 21.60 ms (baseline 44.41 and 45.32 ms).
These are short-run measurements, with variation in GPU waits and other frame
work; they are not a guaranteed gain on other machines or viewport sizes.

## Bottleneck and changes

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

## Validation and reproduction

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
