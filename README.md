# Lightgun Lab

Native lightgun support for Unreal Engine 5.8 on Windows: auto-detection, **ammo-gated recoil** (the gun kicks on live rounds and stays silent on an empty magazine, like a real arcade board), Sinden border rendering, and MAME-compatible output emission for existing cabinet rigs.

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

Guns are auto-detected by USB VID/PID (Gun4IR `2341:804x`, OpenFIRE `F143`, Blamcon `3673`, RS3 `0483:5740`, Sinden HID `16C0:0F0x` + software process check), with a user-extensible ID override list in settings for unknown firmware. A startup panel lists what was found, lets the player confirm/switch/test-fire, and remembers the choice. Confirming drops into an **aim test range with a live 6-round weapon** (`ULightgunWeapon`, reusable from any actor): a custom crosshair tracks the gun (OS cursor fully hidden), on-screen shots spend ammo and fire recoil with hit markers and HUD pips, an empty magazine dry-fires with **no recoil** (Sinden gets its soft empty-chamber clunk, RS3 its `Z0` state), and reload comes from an offscreen shot, any non-trigger gun button, or any keyboard key — with Sinden's `T2170` double-pulse rack on reload. Rapid fire is lossless (double-click events handled; Sinden fire runs collapse into gun-paced `T` bursts).

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

## Status — v0.3

Compiles clean; protocols implemented verbatim from vendor documentation. **Sinden and GUN4IR: fully validated end-to-end in engine on real hardware** (2026-08-15), including the complete weapon loop — six live rounds with recoil, dry-fire on empty, all three reload paths, and lossless rapid fire. Both guns: USB detection → startup picker → game control seizure (gun's own trigger recoil correctly silenced) → game-commanded recoil on the first shot → clean handback. Sinden additionally: paced TCP, empty-chamber soft fire, and the V2.08b pitfalls documented under Known issues. GUN4IR note: bench gun enumerated as PID `8046` (community docs only list `8042`/`8043`), so detection accepts the whole `804x` block and settings expose user ID overrides. RS3/OpenFIRE/Blamcon backends built to spec and awaiting community confirmation. Known open items: RS3 VID/PID confirmation, GUN4IR firmware↔PID mapping, OpenFIRE `FDA` value formatting, live MAMEHooker 5.1 window-message validation.

Licensed under the [MIT License](LICENSE).

## Known issues (Sinden, all bench-confirmed on V2.08b)

- **Don't start the recoil server with the `tcpserver` launch argument.** It races the gun connection at app boot: the server binds and accepts TCP but its dispatcher holds a dead gun reference, so every command is silently ignored (in-app trigger recoil and the tab's test box still work, which makes it maddening to diagnose). Use the **autostart checkbox** on the recoil outputs tab, or click **Start Recoil Server** manually after the software is up. If recoil over TCP is silent while trigger-pull recoil works: stop/start the recoil server on that tab.
- **Fresh software instances boot with recoil disabled** — every `A` fire command is ignored until a `J1` arrives. This plugin sends `J1` automatically in `EnterGameControl`.
- **Keep commands ≥150ms apart on the recoil server.** Faster spacing coalesces messages, which the server rejects and which appeared to kill its reader mid-session on the bench. The plugin paces at 200ms (`SindenCommandGapMs`).

## Protocol credits

Sinden Lightgun software package (RecoilTcpServerReadme.txt, ExternalRecoilOutputs.txt) · GUN4IR official User Guide v1.2 by JB · Retro Shooter RS3 Reaper manual · [OpenFIRE firmware](https://github.com/TeamOpenFIRE/OpenFIRE-Firmware) · [Blamcon serial commands](https://blamcon.com/get-started-with-blamcon/serial-commands/) · [MAME](https://github.com/mamedev/mame) network/win32 output modules · MAME Interop SDK
