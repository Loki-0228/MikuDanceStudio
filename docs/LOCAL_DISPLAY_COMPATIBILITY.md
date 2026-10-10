# Local display changes integrated on 2026-10-10

The pending README, recording-preview, and startup-resolution changes are
integrated with the model-name translation and acknowledgement-link commits.
The README keeps all three language versions, the new feature descriptions,
StarNoodle495's name, and the already committed Salad Sauce homepage. Its
processor-setting description still distinguishes per-user preferences from
project data.

Fresh installations use 1920 x 1080. Valid saved dimensions remain unchanged,
including 640 x 360: the old configuration has no flag that distinguishes an
untouched default from an intentional user choice. Missing or nonpositive
dimensions fall back to the new defaults.

The recording preview fits the nearest monitor's work area and keeps the
export aspect ratio. The full capture source is presented into the preview's
client area. Passing a null destination to
[D3D9 Present](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-present)
scales it to that area instead of clipping an export-sized destination. Capture
dimensions remain at the requested export resolution, and recording restores
the previous render dimensions when it ends.

## Verification

The baseline is `b1d1b8f14470401f285c2a64976edea1d3d5143a` plus the pending
local changes and the new regression harness. The original local patch is
retained in `.local/archives/local-compatibility/original-uncommitted.patch`.
Both builds use Release x64, Visual Studio 17 2022, the Conan toolchain,
`MIKUDANCESTUDIO_PERF=ON`, and the same test inputs.

The harness starts the real application initialization code in hidden windows,
with a separate executable and configuration directory for each case. Recording
uses the production MOV entry point and frame loop. A synthetic monitor work
area at negative coordinates forces a smaller preview. A pass-through hook
observes the actual D3D9 presentation arguments; it does not replace rendering
or capture the desktop.

| Check | Pending local version | Integrated version |
| --- | --- | --- |
| New installation uses 1080p | Pass | Pass |
| Saved 640 x 360 survives startup | Fail: changed to 1080p | Pass |
| Saved portrait resolution survives startup | Pass | Pass |
| Missing dimensions use 1080p | Pass | Pass |
| Preview fits the monitor work area | Pass | Pass |
| Full capture fits the preview destination | Fail: destination clips | Pass |
| MOV contains three 1920 x 1080 PNG frames | Pass | Pass |
| Render resolution is restored after export | Pass | Pass |
| GPU capture pixels match the independent filter oracle | Pass | Pass |

All three exported PNG samples have identical hashes across the two builds.
The existing UI-language and menu suites also pass after integration.
README relative links and three-language acknowledgement entries were checked.
No scene FPS, CPU submission, or GPU-time comparison was performed; performance
remains unverified.

## Local artifacts

- `.local/artifacts/local-compatibility/01/` contains source revisions,
  worktree status, build settings, hashes, logs, exported test frames, and the
  final installation record.
- `.local/releases/local-compatibility-before/` retains the pending local build.
- `.local/releases/local-compatibility-after/` retains the integrated build.
- `.local/archives/local-compatibility/` contains the original local patch and
  the previous installed executable.
- `build/Release/MikuMikuDance.exe` receives the verified integrated executable.

These artifacts are retained for comparison and are not a published release.
