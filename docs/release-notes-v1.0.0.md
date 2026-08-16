Native lightgun support for Unreal Engine 5.8 on Windows — detection to recoil to two players on one PC, in one plugin, validated on real hardware.

**Lightgun Lab 1.0** is the complete package: plug in any supported gun (or two, or a gun plus the desktop mouse), pick seats on the startup panel, and drop into an aim test range where every trigger pull, dry fire, reload, kick, and buzz behaves like a real arcade board.

## Everything in the box

- **Auto-detection by USB VID/PID** — GUN4IR (`2341:804x`), OpenFIRE (`F143`), Blamcon (`3673`), RS3 Reaper (`0483:5740`), Sinden (HID `16C0:0F0x` + software process check, every physical gun listed separately with A/B hints) — plus a user-extensible ID override table for unknown firmware, and a mouse-only fallback.
- **Ammo-gated recoil** — the point of the whole thing. `FireRecoil()` kicks on live rounds; `NotifyEmpty()` dry-fires with **no recoil** (the Sinden gets its soft empty-chamber clunk, the RS3 its `Z0` reload state); `SetAmmo()` feeds outputs and OpenFIRE's OLED counter. The gun's own trigger recoil is seized at selection (`S6` / `SM.6.1` / `ZS` / `K0`) and handed back on exit, crash-guarded.
- **Two players, one PC** — any mix: Sinden + GUN4IR, two Sindens, or a gun plus the **desktop mouse as an aim-only player**. A Windows Raw Input router un-merges the OS cursor: aim, triggers, and reloads are correlated per device by exact raw path, shared USB composite parent (which tells two *identical* guns apart), or VID/PID — with a **Swap P1↔P2** button for anything hardware can't disambiguate.
- **Single player, device-locked** — with a gun selected, only *that* gun steers the crosshair, fires, or reloads, no matter how many mice and guns are attached. Mouse-only play keeps classic cursor aim.
- **The aim test range** — a live 6-round `ULightgunWeapon` per player (reusable from any actor, Blueprint delegates included), blue/red configurable crosshairs, ammo pips, hit rings, per-player status and RELOAD indicators, per-gun offscreen-corner reload, and lossless rapid fire (Sinden fire runs collapse into gun-paced `T` bursts). The range's buttons are pressed by **shooting them** — no ammo spent, press rings in the shooter's color.
- **Reload feel** — reload never fires a real kick: rumble-motor guns (GUN4IR, OpenFIRE, Blamcon, RS3) buzz in three pulses (one is physically imperceptible — we checked), and the Sinden marks it with its lightest feelable solenoid tap (`SindenVibrationStrength`, default 3).
- **The startup panel** — a One player ‖ Two players toggle that opens straight into 2P when two guns are detected, seats pre-filled from each gun's own P1/P2 identity, per-seat **Test recoil** / **Test vibration** buttons that grey out where the hardware lacks the feature, rescan, and remembered choices.
- **Sinden done right** — exactly **one** paced TCP connection to its recoil server, ever (per-player `1`/`2` prefixes matching the software's Lightgun A/B; reselects and swaps never reconnect, so the connection-churn wedge can't happen), `J1` recoil-enable on takeover, 200 ms command pacing, and the tracking border: it pops the moment a Sinden is detected, follows your picks live, and the in-game UI insets into a **border safe zone** so nothing ever sits under the frame.
- **MAME-compatible outputs** — a network-protocol TCP server (`:8000`, `mame_start` handshake, CR-terminated `name = value` lines) and classic `MAMEOutput` window-message broadcast, emitting `P1_*`/`P2_*` per seat. Ready-made QMamehook / Hook of the Reaper / MAMEHooker 5.1 / Sinden configs ship in `HookerConfigs/`.
- **Zero content dependencies** — every panel is built in C++ and can be subclassed in Blueprint for reskinning; the plugin drops into any UE 5.8 project.

## Install

Drop the `LightgunLab` folder from the zip into your project's `Plugins/`, enable, build (prebuilt Win64 editor binaries for UE 5.8.1 included). Run the game borderless fullscreen (required for the Sinden border, correct for cursor aim everywhere). Hooker configs in `HookerConfigs/`.

## Status

**Sinden and GUN4IR validated end-to-end on real hardware** — single-player and two-player, both seat orders, simultaneous rapid fire with no lost shots or crosstalk, per-gun reload isolation, and every feedback path (recoil, dry fire, reload rumble/tap, border). RS3 Reaper / OpenFIRE / Blamcon are built verbatim from vendor documentation — test reports welcome, especially RS3 USB IDs and OpenFIRE `FDA` formatting.

MIT licensed.
