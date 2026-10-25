# Keeping up with HarbourMasters/PaperBoat

This repository keeps PaperBoat's full git history, with the PaperReality changes on top. Because the code and file names are unchanged, PaperBoat's updates merge in normally.

## One-time setup
```
git remote add upstream https://github.com/HarbourMasters/PaperBoat.git
```

## Merging an update
```
git fetch upstream
git checkout -b merge-upstream main
git merge upstream/develop
# resolve conflicts (see below), then push and open a pull request
```
Pushing the merge branch builds an IPA, so you can test before it goes into `main`.

## Where conflicts are likely
These are the files PaperReality changes (full list in [CHANGES.md](CHANGES.md)). Everything else merges cleanly.
* `src/port/Engine.cpp`, `src/port/ui/TouchControls.cpp`, `src/port/ui/Menu.cpp`, `src/port/ui/PaperboatMenuSettings.cpp`, `src/port/ui/PaperboatModMenuWindow.cpp`: UI and first-launch code.
* `CMakeLists.txt`, `.gitmodules`: iOS build lines and the libultraship fork URL.
* `ios/Icon.png`: if upstream changes it, keep ours. The workflow also overwrites the bundle icon from `ios/PaperReality-Icon.png`.
* `external/libultraship`: pinned to a fork (JeodC's `lus-converge` plus Metal present pacing). If upstream changes the pin, check it against the fork before taking it.

## Keep ours when merging
* `src/screen_overlays.c`: the `gsSPEndDisplayList()` at the end of `D_8014EA48`. Without it the display list runs past its end on iOS and crashes on the first frame. If upstream touches that list, resolve the conflict in our favour (or confirm upstream added the terminator).

## Merge log

* 2026-10-24: merged upstream `develop` at 781d164cf (#269, #270, #272) into `experimental/uikit-settings` with no conflicts. The new gameplay settings use the native UIKit mapping; the iOS libultraship fork and its AirPlay/frame-pacing changes are retained.
* 2026-10-18: merged upstream `develop` at d98b4b63 (#262, #263, #264, #266) with no conflicts. Upstream's `build.yml` (now with an iOS job) and `pages.yml` come with it; the PaperReality IPA workflow is `ios-unsigned.yml`.

## Rules that keep merges easy
* Don't rename source files, folders, functions or the `Paperboat` strings in code. Only the README, docs and workflow output names say PaperReality.
* Put new iOS-only code in new files (like `src/port/ios/` and `TouchDragScroll.*`) rather than rewriting upstream files.
* Never merge code from projects without a license. Check where a change comes from before it goes in.
