The whole machine is now Blueprint-callable: build your own picker, read every gun's aim, no C++ required.

## New in 1.0.3

- **Start Range Session from Blueprint** — the picker-confirm finale (game control for every active seat, plus per-device raw aim routing started or stopped to match the session) is now a BlueprintCallable node. A custom gun picker built on the public API can finish its own confirm exactly like the stock panel — keeping the stock picker in the loop is no longer required for aim routing.
- **Per-player input events** — while a gun session runs, the raw router's per-device streams mirror onto BlueprintAssignable subsystem events: **On Gun Aim** (`PlayerIndex`, `DesktopPx`), **On Gun Trigger Pulled** (position = the aim at press time), and **On Gun Reload Requested** (any non-trigger gun button or correlated keyboard key; the desk keyboard reloads P1). Positions are desktop pixels — convert with **Absolute to Local** against your widget's geometry. Mouse-only 1P streams nothing; normal input remains the path there.
- **Get Aim For Player** — the polled alternative for Tick-driven crosshairs: BlueprintPure, returns the seat's latest cached aim and whether its device has aimed this session. The cache clears when the session stops.
- Together these close the last two "C++ only" gaps: a Blueprint-only project can now run the complete two-player loop — custom picker, per-gun crosshairs, ammo-gated weapons, per-gun reloads — end to end. Node-by-node walkthroughs: [wiki guide 6](https://github.com/del1verance/lightgunlab/wiki/Blueprint-06-Two-Players) for the input events, [guide 8](https://github.com/del1verance/lightgunlab/wiki/Blueprint-08-Reskinning-Panels) for the custom-picker recipe.
- The events ride the same per-device router the aim test range has always consumed (bench-validated on Sinden + GUN4IR); the Blueprint mirroring is the new part — reports from real rigs welcome.
- The plugin's DocsURL now opens the [project wiki](https://github.com/del1verance/lightgunlab/wiki) — per-gun setup, Blueprint guides, troubleshooting with the log line that confirms each diagnosis, full API reference.

## Try it without Unreal

**`LightgunLab-Demo-1.0.3-Win64.zip`** is the aim test range as a standalone Windows app: unzip, run `LightgunLab.exe` (borderless fullscreen recommended; required for Sinden). Logs land at `%LOCALAPPDATA%\LightgunLab\Saved\Logs\LightgunLab.log` — attach that file to any hardware test report.

## Install (the plugin)

Drop the `LightgunLab` folder from the plugin zip into your project's `Plugins/`, enable, build (prebuilt Win64 editor binaries for UE 5.8.1 included). Hooker configs in `HookerConfigs/`.

MIT licensed.
