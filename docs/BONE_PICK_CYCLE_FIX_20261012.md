# Overlapping bone selection

Repeated clicks on overlapping markers now cycle through the eligible bones in model order. This applies to the full and minimal builds and to the main and floating viewports.

## Cause and reference behavior

`PickBoneAtCursor` in `src/app/frame_modes.cpp` previously kept the highest bone index under the cursor. Every click selected that same bone, making an overlapping rotation-only bone unreachable when a translation-enabled bone had a higher index.

The reference is `refs/MikuMikudance 932 fix/MikuMikuDance.exe` (SHA-256 `3ebe6ead8076478c06539b14a0e32e70f30b0edc9c8f045819d04ad8d62bee49`). Its x64 picker saves the first eligible hit at `0x140031C81`, selects the first hit whose index exceeds the active bone index at `0x140031C89`, and wraps to the saved first hit at `0x140031D83`.

The corrected picker follows that order. It preserves the existing visibility and physics filters, the exclusive eight-pixel hit radius, and Shift's toggle behavior. It allocates no memory and can stop scanning as soon as it finds the next hit. It changes no model or motion file format.

## Verification

`tests/bone_registration_tests.cpp` sends mouse messages to the actual main and floating window handlers through hidden test windows. The fixtures cover PMD and PMX model modes, overlapping rotation-only and translation-enabled bones, a third overlapping bone, wraparound, rapid clicks delivered as `WM_LBUTTONDBLCLK`, hidden markers, physics locks, hit-radius boundaries, an invalid active index, empty clicks, Shift, and playback.

The original picker fails the new first-click assertion. After the fix, the complete `bone_registration_tests` executable passes in both build profiles, including selection, registration, movement, undo, frame changes, physics locks, and box selection.

The broader baseline run also exposed two incomplete synthetic gestures in older tests: a direct Shift press lacked its release, and a Shift-click helper omitted Shift from its release message. The tests now complete those gestures. No production behavior was changed for these fixture corrections.

Run the focused checks with:

```powershell
& '<build>/tests/Release/bone_registration_tests.exe' --pick-cycle
```

Omit `--pick-cycle` to run the complete suite. Local logs and video frames are under `.local/artifacts/bone-pick-cycle-20261012`. The tests use synthetic bone fixtures; the exact character from the supplied video was not loaded for an interactive retest.

## Candidate builds

The executables under `build/Release/bone-pick-fix-20261012/full` and `minimal` include this fix and the preceding PMD shared-toon fix. Copy the matching executable into an existing installation to retain its data and plugins. These are local candidates, not a published release.
