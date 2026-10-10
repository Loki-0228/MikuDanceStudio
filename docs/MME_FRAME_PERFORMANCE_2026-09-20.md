# Stage.pmm: MME buffer access and repeated transparent sorting

The user now measures approximately 18 FPS in the port and 35 FPS in original
MMD on `Stage.pmm`, with little remaining difference without MME. This round
targets MME. It follows the separate dialog-language fix `3e8a064`.

## Controlled comparison

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

## What the original MMD path shows

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

## Changes and why they help

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

## Validation and limits

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
