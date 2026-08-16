# Lightgun Lab

Native lightgun support for Unreal Engine 5.8 on Windows: auto-detection, **ammo-gated recoil** (the gun kicks on live rounds and stays silent on an empty magazine, like a real arcade board), **two guns on one PC** via Windows Raw Input, Sinden border rendering, and MAME-compatible output emission for existing cabinet rigs.

`Plugins/LightgunLab` is the deliverable — drop it into any UE 5.8 project. The surrounding `LightgunLab` project is a minimal test bed.

## Supported guns

| Gun | Aim | Recoil control | Protocol source |
|---|---|---|---|
| **GUN4IR** | mouse cursor | COM 9600: `S6` → `F0.2.1` → `E` | official User Guide v1.2 |
| **Sinden** (software ≥ V2.08a) | mouse cursor + in-game border | TCP `localhost:13000`: `1K0` → `1A` → `1K1`, soft empty-chamber `1U4` | official `RecoilTcpServerReadme.txt` |
| **Retro Shooter RS3 Reaper** | mouse cursor | COM 115200: `ZS` → `Z1`–`Z5` → `ZX`, empty state `Z0` | official manual |
| **OpenFIRE** | mouse cursor | COM 9600: `S6` → `F0x2x1` → `E`, OLED ammo `FDA<n>` | firmware source |
| **Blamcon** | mouse cursor | COM 9600: `SM.6.1` → `FB.0.1` → `ES` | vendor serial-command docs |
| Anything MAMEHooker knows (AimTrak, custom cabs) | mouse cursor | **Outputs mode** — see below | MAME network/window-message output protocols |

Guns are auto-detected by USB VID/PID (Gun4IR `2341:804x`, OpenFIRE `F143`, Blamcon `3673`, RS3 `0483:5740`, Sinden HID `16C0:0F0x` + software process check — every physical Sinden enumerated separately), with a user-extensible ID override list in settings for unknown firmware. A startup panel lists what was found, lets the player confirm/switch and test recoil or vibration per gun, and remembers the choice. Confirming drops into an **aim test range with a live 6-round weapon** (`ULightgunWeapon`, reusable from any actor): a custom crosshair tracks the gun (OS cursor fully hidden), on-screen shots spend ammo and fire recoil with hit markers and HUD pips, an empty magazine dry-fires with **no recoil** (Sinden gets its soft empty-chamber clunk, RS3 its `Z0` state), and reload comes from an offscreen shot, any non-trigger gun button, or any keyboard key. Reload never fires a real kick — recoil only ever means a live round; rumble-motor guns buzz on reload, and the Sinden marks it with its lightest solenoid tap. Rapid fire is lossless (double-click events handled; Sinden fire runs collapse into gun-paced `T` bursts).

## Two players, one PC

Windows merges every pointing device into one cursor; Lightgun Lab un-merges them. A **"Two players" toggle on the startup panel** gives each player their own device dropdown — any mix works: Sinden + GUN4IR, two Sindens, or a gun plus the **desktop mouse as an aim-only P2**. The same physical device can never serve both players. Detecting two guns opens the panel in two-player mode automatically, and guns that declare their seat (GUN4IR P1/P2 PIDs, Sinden A/B models) are pre-seated accordingly.

- **Per-device aim** through a Windows Raw Input router (`WM_INPUT` with `RIDEV_INPUTSINK`): each gun's HID mouse is correlated to its player by exact device path, shared USB composite parent (which tells two *identical* guns apart by tying each COM port to its sibling HID interface), or VID/PID — with a **Swap P1↔P2** button on the range for the cases hardware can't disambiguate.
- **Two crosshairs** (P1 blue, P2 red — both configurable via `CrosshairColorP1/P2`), two ammo pip rows, per-player hit rings, status lines, and RELOAD indicators.
- **Two independent `ULightgunWeapon` magazines** — each `Initialize(Subsystem, PlayerIndex)`; the whole 1P API (`FireRecoil()` etc.) now has `...ForPlayer(Index)` twins, and outputs emit `P1_*`/`P2_*` per player.
- **Per-gun reload isolation**: a gun's own non-trigger buttons and its offscreen corner shot reload only that gun — including buttons that present as *keyboard* keys (GUN4IR/Sinden), which are routed per-device through raw keyboard input. The desk keyboard (uncorrelated) reloads P1.
- **One Sinden TCP connection, ever**: all Sinden recoil rides a single shared, paced socket with per-player `1`/`2` command prefixes matching the software's Lightgun A/B assignment. Two Sindens = two prefixes, one socket — reselection, swaps, and mode switches never reconnect it.
- Whenever a gun is in play — 1P included — the range takes gameplay input **exclusively** from the raw router, so **only the selected gun steers its crosshair** no matter how many mice/guns are attached, and the merged cursor can't double-fire. Mouse-only 1P keeps the classic merged-cursor path.
- **Shoot the buttons**: the range's Back / Crosshair / Swap buttons are pressed by shooting them — each gun's own aim point decides (the mouse player's click works the same way), costs no ammo, and rings in the shooter's color.
- The Sinden border shows when **either** selected gun is a Sinden.

## The ammo gate

Your weapon code reports what happened; the plugin routes it:

```cpp
ULightgunSubsystem* LG = GetGameInstance()->GetSubsystem<ULightgunSubsystem>();

LG->FireRecoil();     // live round -> solenoid kick
LG->NotifyEmpty();    // dry fire   -> silence (RS3: Z0 reload state, Sinden: soft clunk)
LG->SetAmmo(Count);   // feeds P1_Ammo output + OpenFIRE's OLED counter
LG->NotifyDamaged();  // rumble pulse + P1_Damaged output
```

`BeginGameControl()` seizes the gun's feedback channel (suppressing its built-in trigger recoil); `EndGameControl()` hands it back — both automatic around session lifetime, both crash-guarded.

## Outputs mode (MAMEHooker ecosystem)

Instead of (or alongside) direct control, the game can impersonate MAME's output system:

- **TCP server on :8000** speaking MAME's exact network output protocol (`mame_start = <game>`, `P1_CtmRecoil = 1`, CR-terminated) — consumed by QMamehook, Hook of the Reaper, OutputHooker, and Sinden software ≥ 2.08a.
- **Window-message broadcast** (`MAMEOutput` window + `WM_COPYDATA` interop) for classic MAMEHooker 5.1.

Players with an existing rig point their `lightgunlab.ini` at the game and every device they've ever wired up just works.

## Requirements

- Unreal Engine **5.8** (built against 5.8.1), Windows 64-bit
- For Sinden: the Sinden software **V2.08a or newer** running with its Recoil Server started (`Lightgun.exe tcpserver`, or the checkbox on the recoil outputs tab)
- For serial guns: nothing — plug in and go (RS3 recoil additionally needs its 24 V PSU)
- Game should run **borderless fullscreen** (required for the Sinden border overlay; correct for cursor aim everywhere)

## Install

**Into your own project:** copy `Plugins/LightgunLab` into `<YourProject>/Plugins/`, enable it, build. All UI panels are C++-built with zero content dependencies and can be subclassed in Blueprint for reskinning.

**This test bed:** clone into a UE 5.8 source-build root (or fix up `EngineAssociation`), then open `LightgunLab.uproject`.

## Status — v1.0

Compiles clean; protocols implemented verbatim from vendor documentation. **Single-player AND two-player fully validated end-to-end on real Sinden and GUN4IR hardware (2026-08-15)**: independent per-device aim in every mix and both seat orders (Swap included), simultaneous rapid fire with no lost shots or crosstalk, per-gun reload isolation, the complete ammo-gated weapon loop with reload rumble/tap feedback, shoot-to-press range UI, gun + desktop mouse, and the live Sinden border with its UI safe zone. Sinden aim confirmed to arrive as per-device raw input on the bench (the injected-stream fallback stayed idle). Sinden pitfalls (V2.08b) documented under Known issues. GUN4IR notes: bench gun enumerated as PID `8046` (community docs only list `8042`/`8043`), so detection accepts the whole `804x` block and settings expose user ID overrides; one rumble pulse is physically imperceptible, so vibration feedback pulses in threes. RS3/OpenFIRE/Blamcon backends built to spec and awaiting community confirmation. Known open items: RS3 VID/PID confirmation, GUN4IR firmware↔PID mapping, OpenFIRE `FDA` value formatting, live MAMEHooker 5.1 window-message validation.

Licensed under the [MIT License](LICENSE).

## Known issues (Sinden, all bench-confirmed on V2.08b)

- **Don't start the recoil server with the `tcpserver` launch argument.** It races the gun connection at app boot: the server binds and accepts TCP but its dispatcher holds a dead gun reference, so every command is silently ignored (in-app trigger recoil and the tab's test box still work, which makes it maddening to diagnose). Use the **autostart checkbox** on the recoil outputs tab, or click **Start Recoil Server** manually after the software is up. If recoil over TCP is silent while trigger-pull recoil works: stop/start the recoil server on that tab.
- **Fresh software instances boot with recoil disabled** — every `A` fire command is ignored until a `J1` arrives. This plugin sends `J1` automatically in `EnterGameControl`.
- **Keep commands ≥150ms apart on the recoil server.** Faster spacing coalesces messages, which the server rejects and which appeared to kill its reader mid-session on the bench. The plugin paces at 200ms (`SindenCommandGapMs`).

## Protocol credits

Sinden Lightgun software package (RecoilTcpServerReadme.txt, ExternalRecoilOutputs.txt) · GUN4IR official User Guide v1.2 by JB · Retro Shooter RS3 Reaper manual · [OpenFIRE firmware](https://github.com/TeamOpenFIRE/OpenFIRE-Firmware) · [Blamcon serial commands](https://blamcon.com/get-started-with-blamcon/serial-commands/) · [MAME](https://github.com/mamedev/mame) network/win32 output modules · MAME Interop SDK
