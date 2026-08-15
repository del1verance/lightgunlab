# Pre-configured hooker configs (Outputs mode)

These files are for players who drive their gun with an existing rig — MAMEHooker, QMamehook, or Hook of the Reaper — instead of letting the game talk to the gun directly.

**In-game setup:** open **Lightgun Options** and set recoil mode to **"Outputs"**, then tick the output channel your app uses:

| Your app | Tick in game | Where the ini goes | `cmo`/`cmw` port number means |
|---|---|---|---|
| **QMamehook** | MAME network outputs (TCP :8000) | `%LOCALAPPDATA%\QMamehook\ini\lightgunlab.ini` | detection order — first gun = `1`, leave as shipped |
| **Hook of the Reaper** | MAME network outputs (TCP :8000) | its game-files folder | pick the COM port in HOTR's own UI |
| **MAMEHooker 5.1** | MAME window-message outputs | MAMEHooker's `ini` folder | the **actual COM number** — edit every `1` in the file to your gun's COM port (e.g. `cmo 5`, `cmw 5` for COM5) |
| **Sinden software** (≥ V2.08a) | MAME network outputs (TCP :8000) | no ini — see `Sinden/` below | n/a |

Pick the folder matching **your gun**, copy its `lightgunlab.ini` to the location above, and start your hooker app before (or after — network clients reconnect) launching the game.

The filename must match the game's **Outputs game name** setting (default `lightgunlab`). If you change that setting, rename the ini to match.

## What the game emits

- `P1_CtmRecoil` — pulses `1` then `0` on every **live** shot. Never emitted on an empty magazine — that's the whole point.
- `P1_Ammo` — the live round count, whenever it changes.
- `P1_Damaged` — pulses when the player takes a hit.
- `P1_Life` — current health, whenever it changes.
- `mame_start = lightgunlab` on connect, `mame_stop = 1` on quit (standard MAME network protocol, TCP :8000, also mirrored as MAME window messages when enabled).

Player 2 machines emit `P2_*` when the in-game Player Slot setting is 2.

## Sinden

The Sinden software consumes the game's network outputs itself — no hooker app in the middle. Append the single line in `Sinden/RecoilOutputsGamesList_entry.txt` to the `RecoilOutputsGamesList.ini` in your Sinden software folder, enable the outputs client on its **recoil outputs** tab, and tick MAME network outputs in the game.

(Direct mode is still the simpler Sinden path — the game then talks to Sinden's TCP recoil server itself and none of this is needed.)
