GunCon 3 joins the supported list, plus a host-game integration setting.

## New in 1.0.1

- **GunCon 3 (aim-only)** — the Namco GunCon 3 (`0B9A:0800`) is auto-detected and seatable in single player or either two-player slot. It requires the community Windows driver running: **[gameotaku79/Guncon3Windows](https://github.com/gameotaku79/Guncon3Windows)** (WinUSB driver + calibration console; aim is delivered through a TetherScript virtual mouse — install per its README, run `Guncon3Console.exe`, and map the trigger to `MOUSE.Left`). Aim, trigger, and button reloads route per-device: the plugin adopts the driver's virtual mouse for the GunCon player at runtime, visible in the range footer. No recoil or rumble hardware exists on this gun, so the Test recoil / Test vibration buttons grey out and the ammo gate runs its silent paths. Built from the driver project's documentation — hardware test reports welcome. (The in-progress [sonik-br/GunconUSB](https://github.com/sonik-br/GunconUSB) should route the same way once usable.)
- **`bShowRangeOnConfirm`** — host games set this `false` so confirming the gun picker hands straight back to the game instead of opening the aim test range. Game-control seizure and raw aim routing still run on confirm either way, and the range stays reachable via `ShowCalibrationScreen()`. The default (`true`) keeps the range as the home screen, which is what the plugin's own test bed wants.

## Try it without Unreal

**`LightgunLab-Demo-1.0.1-Win64.zip`** is the aim test range as a standalone Windows app: unzip, run `LightgunLab.exe`, plug in your gun(s), and you're in the picker → range flow (borderless fullscreen recommended; required for Sinden). Logging is enabled, so a full detection/routing log lands at `%LOCALAPPDATA%\LightgunLab\Saved\Logs\LightgunLab.log` — **attach that file to any hardware test report**, especially for the guns awaiting community validation (RS3 Reaper, OpenFIRE, Blamcon, GunCon 3).

## Install (the plugin)

Drop the `LightgunLab` folder from the plugin zip into your project's `Plugins/`, enable, build (prebuilt Win64 editor binaries for UE 5.8.1 included). Hooker configs in `HookerConfigs/`.

MIT licensed.
