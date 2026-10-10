# PMX loading bottleneck in StageNoRay LowEnd assets

## Follow-up: Skip hang fixed

After the user explicitly requested skipping missing PMX files, the actual
Skip path reproduced a separate parser bug. It manually consumed the dense
display key's header, IK flags and parent states, then called
`DiscardPmmDisplayKey`, which consumed those same fields again. The resulting
offset error turned the following existing stage model into an apparent missing
file and let later fields become enormous loop counts. This was a parsing hang,
not simply slow disk I/O.

The duplicate consumption is removed. `frame_benchmark --load-skip-missing`
now completes this unchanged project in 1,120.735 ms, skipping only Null_00 and
restoring 3 models (168,596 vertices) and 10 accessories. Other dialogs still
fail the benchmark. This is an explicitly skipped-resource load, not a claim
that the missing model was recovered.

An end-to-end regression runs:

`scene_lifecycle_tests.exe <StageNoRay LowEnd.pmm> 3 --skip-missing 3 10`

It asserts the expected model/accessory counts, loads in three UI languages,
renders 12 physics frames after each load, and resets/reopens the project.
All three cycles passed: 1,062.127 / 592.514 / 633.837 ms without MME.
The same test also passed with the release directory's d3d9/MMEffect/MMHack
proxy DLLs and exported application entry points: 1,086.955 / 598.893 /
636.891 ms, again restoring 3 models and 10 accessories and rendering all
three cycles. Seven selected existing regressions pass as well.
These timings are for this port and fixture; they are not an original MMD
comparison or a claim about arbitrary third-party effects.

The sections below record the earlier diagnosis before Skip was authorized.

The user selected `极星动力LIVE舞台 Ray用一键渲染/UserFile/StageNoRay LowEnd.pmm`.
The file declares four models. An actual load aborts on the missing `Null_00`
model, whose stored source is
`F:\926MMD_ZN\MME\mme\1Shaders pack\G_ToonShaderCC\GTSC_001.pmx`.
The test reports a dialog and returns failure; its 163 ms aborted load is NOT
a successful PMM loading result. No PMM was changed, saved or substituted.
The missing model's current path is required for a full comparison with MMD 926.

## Change

The PMX parser issued one CRT `_read` for each small scalar, vertex component
and index. The 129,307-vertex stage requires 3,157,219 such calls to consume
15,667,857 bytes. A bounded 64 KiB sequential reader now supplies these exact
logical reads using 240 underlying reads. Parser field order and arithmetic
are unchanged. Logical offsets remain correct for diagnostics despite read-ahead;
the reader owns and closes its descriptor on every exit.

This optimization is based on measurements of this port. It does not claim
that original MMD uses this particular buffering implementation.

## Measurements

Release x64, RTX 4070 Laptop GPU, no MME. Same executable and parser, switching
only buffered versus raw reads. Each model is loaded into a fresh process;
timing covers `LoadModelFile`, including its existing texture/initialization
work, but excludes application startup and digest calculation. Two repetitions,
reversing the order in the second repetition. These are individual existing
assets from the selected project, NOT a complete PMM benchmark or cold-disk test.

| Asset | Raw reads, mean | Buffered, mean | Ratio | Underlying reads |
|---|---:|---:|---:|---:|
| PolarisLiveStage.pmx | 2,983 ms | 521 ms | 5.73x | 3,157,219 → 240 |
| SourRIN/White.pmx | 566 ms | 143 ms | 3.94x | 509,451 → 47 |
| Sky with box.pmx | 67 ms | 66 ms | ~1.02x | 16,169 → 2 |

Small skybox loads are dominated by other work; no material speedup is claimed.
Raw outputs for all 12 runs are in `PMX_LOADING_2026-09-20.json`.

## Correctness checks

- All runs load without dialogs and have the same model/vertex/bone/morph counts.
- Digests of initialized GPU input geometry, bone transforms, edge materials
  and triangle indices match between modes and repeated processes. These
  digests are a focused parser regression check, not a hash of every runtime field.
- This exposed inactive PMX weight/SDEF fields that the legacy loader does not
  initialize. GPU input packing now copies only fields used by the vertex's
  weight type, leaving inactive GPU lanes zero; it does not change active math.
- Reader tests cover existing descriptor offsets, small fields, 64 KiB boundary
  crossings, larger reads, short reads without overwriting the destination tail,
  EOF, logical positions and repeated close.
- The real character GPU/CPU skinning test includes BDEF1/2/4, SDEF, nonzero
  vertex morphs, outlines and cached submission reuse.
- All seven selected regressions pass: sequential reader, D3DX import hooks,
  effect API, bone projection, alpha drawing, PMM reload and morph runtime.

Reproduce with `frame_benchmark.exe <model.pmx> --load-unbuffered` and
`frame_benchmark.exe <model.pmx> --load-only`. Both reject any load dialog;
missing assets cannot silently become a successful fast-load result.
