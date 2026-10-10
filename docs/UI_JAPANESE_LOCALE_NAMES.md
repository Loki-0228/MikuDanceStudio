# Unicode model names in the Japanese locale

The Chinese UI could lose imported names when Windows used the Japanese code
page. Some main-panel selectors were ANSI windows. The frame-registration list
read truncated legacy bone and morph names even though PMX retained the original
Unicode strings. PMX display-group names were converted to legacy strings and
their Unicode originals were discarded. Failed conversions left empty entries.

Main-panel combo boxes now use Unicode windows. Registration rows read the
original bone and morph names, with the existing English-name fallback. Imported
display groups retain their Unicode names in an owned side table without changing
the model ABI. Changes from the English-name editor still take precedence. Model
initialization, disposal, and failed imports clear the side table. PMX row counts
now exclude uninitialized records, including the special Root entry that is not
copied into the bone-group array.

## Verification on 2026-10-10

The baseline is `a475523b2aab82e72839f5c5b574d7733f014273` plus the regression
fixture and a behavior-preserving extraction of panel control creation. The
instrumentation patch, build log, CMake cache, worktree status, and executable
hashes are in `.local/artifacts/ui-japanese-names/01/`. The baseline binaries are
in `.local/releases/ui-japanese-names-before/`.

Both sides use Release x64, Visual Studio 17 2022, the Conan toolchain, and the
same generated PMX fixture. The fixture contains Chinese and Japanese names,
long names, supplementary Unicode characters, and missing English translations.
Tests run in hidden windows and compare timeline pixels from GDI memory surfaces.
They do not capture the desktop or open a scene on the user's desktop.

| Check | Before | After |
| --- | --- | --- |
| PMX registration names with Japanese CRT locale | Fails on morph name | Passes |
| Production panel combo boxes preserve Unicode | Fails | Passes |
| Same checks with process ACP 932 | Fail | Pass |
| Complete UI language suite with process ACP 932 | Fails | Passes |
| UI language and menu regressions with system ACP 936 | Not claimed | Both pass |

The Japanese test copies use a `ja-JP` process manifest, supported by
[Windows application manifests](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests#activeCodePage).
The test log confirms `ACP=932` and `CRT=Japanese_Japan.932`. Windows system
settings are unchanged. The helper and raw logs are under
`.local/artifacts/ui-japanese-names/01/`, including `japanese-before/` and
`japanese-after/`.

The suites also verify three UI languages, language switching, readable timeline
glyphs, complete display-group names, legacy English-name edits, and name-cache
cleanup. No rendered-scene FPS or GPU performance comparison was performed;
this record accepts UI correctness only.

## Local outputs

- `.local/worktrees/ui-japanese-names/` isolates this repair from unrelated live
  edits. It is also reused for the subsequent
  [model name translation build](MODEL_NAME_TRANSLATION.md), with the same
  Release options and its final source revision recorded in the artifacts.
- `.local/releases/ui-japanese-names-before/` preserves the failing baseline.
- `.local/releases/ui-japanese-names-after/` preserves the verified repair.
- `.local/archives/ui-japanese-names/` preserves the previously installed EXE.
- `build/Release/MikuMikuDance.exe` is the installed repair.
- `.local/artifacts/ui-japanese-names/01/` retains build and test evidence,
  source revisions, patches, settings, and executable hashes.

The snapshots and worktree are retained for comparison. They are not release
tags and have not been pushed or published.
