# Stage.pmm frame performance

The user confirmed that `Stage.pmm` is the scene used for the reported original
MMD 35 FPS / port 9 FPS comparison. This round uses that complete project and
measures an improvement over port commit `a36953d`. Original MMD has not been
measured under the harness conditions; the numbers below are not GUI FPS and
do not establish parity with original MMD.

## Workload and method

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

## Change

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

## Validation

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

## Remaining measured work

The final instrumented run measured 57.67 ms/frame. Inclusive phase timings
include approximately 17.33 ms of transparent-sort CPU work, 5.33 ms of sort
buffer locking, and 13.94 ms inside MME-hooked `BeginScene`. These scopes can
include nested work and must not all be summed. Scope reference release costs
approximately 0.0024 ms/frame. The `BeginScene` measurement locates time at the
hook boundary; it does not yet establish which internal MME operation or GPU
wait is responsible.

An intermediate CPU index-staging experiment failed to improve the separate
LowEnd workload and was reverted before this Stage comparison.

## Reproduction

Build `frame_benchmark_mme` with `MIKUDANCESTUDIO_PERF=ON` and place the release
MME DLLs and runtime Data next to its executable. From that output directory:

```text
MikuMikuDance.exe "<absolute path>/极星动力LIVE舞台 Ray用一键渲染/UserFile/Stage.pmm" --throughput-skip-missing
```

This is the benchmark executable under `tests/mme-benchmark/Release`, not the
GUI executable. The flag allows missing-resource skipping, but the recorded
Stage runs report zero dialogs and include the complete scene.
