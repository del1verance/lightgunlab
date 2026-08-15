Two lightguns on the same PC at the same time — ANY mix: Sinden + GUN4IR, two Sindens, or a gun plus the desktop mouse as an aim-only second player. Windows merges every pointing device into one cursor; v0.4 un-merges them with a Raw Input router and gives each player their own crosshair, magazine, recoil, and reload.

## New since v0.3.0
- **Two-player mode** — a "One player ‖ Two players" toggle on the startup panel; in 2P each player gets a device dropdown (the same physical device can never serve both) and confirm drops both guns into the aim test range: **P1 blue / P2 red crosshairs** (both configurable), two ammo pip rows, per-player status + hit rings + RELOAD indicators, and a live 6-round `ULightgunWeapon` per player.
- **Per-device input routing** (`WM_INPUT`, `RIDEV_INPUTSINK`): aim, triggers, and reloads are correlated to players by exact raw device path, shared USB composite parent (tells two *identical* guns apart by pairing each COM port with its sibling HID interface), or VID/PID — plus a **Swap P1↔P2** button for anything hardware can't disambiguate. Software-injected aim streams are detected and adopted (logged in the range footer).
- **Per-gun reload isolation** — a gun's own non-trigger buttons and its offscreen corner shot reload only that gun, including buttons that present as *keyboard* keys (routed per-device through raw keyboard input, auto-repeat filtered). The desk keyboard reloads P1.
- **One Sinden TCP connection, ever** — all Sinden recoil rides a single shared, paced socket with per-player `1`/`2` prefixes matching the software's Lightgun A/B assignment; per-player fire runs still collapse into gun-paced `T` bursts, and reselects/swaps/mode switches never reconnect (the connection-churn wedge can't happen).
- **Every physical Sinden detected separately** (PIDs 0F01/0F02/0F38/0F39 with A/B hints) so two-Sinden rigs pick per-gun.
- **Shootable UI** — in two-player mode the range's buttons (Back / Crosshair / Swap) are pressed by *shooting* them: each gun's own aim point decides (the mouse player's click works the same way), it costs no ammo, and the press rings in the shooter's color.
- **Silent reload** — the Sinden `T2170` double-pulse rack on reload is gone; reload makes no solenoid noise on any gun, so recoil only ever means a live round.
- **Per-player API** — `FireRecoilForPlayer()` & friends beside the unchanged 1P API; outputs emit `P1_*`/`P2_*` per seat in 2P for dual-gun hooker rigs.
- 1P keeps the bench-validated v0.3 path — merged-cursor aim, Slate input, same behavior (minus the reload pulse).

## Install
Drop the `LightgunLab` folder from the zip into your project's `Plugins/`, enable, build (prebuilt Win64 editor binaries for UE 5.8.1 included). MAMEHooker/QMamehook/Hook of the Reaper configs in `HookerConfigs/`.

## Status
Single-player: Sinden ✅ and GUN4IR ✅ validated end-to-end on real hardware (v0.3.0), untouched here. Two-player: validated on the reference bench — Sinden + GUN4IR in both seat orders, simultaneous rapid fire, per-gun reload isolation, and gun + desktop mouse. RS3 Reaper / OpenFIRE / Blamcon built verbatim from vendor docs — test reports welcome.

MIT licensed.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
