Plug guns in. Yank them out. The plugin keeps up.

## New in 1.0.2

- **Hot-plug support** — an always-on USB topology watcher rescans (debounced) whenever Windows announces device changes. Plug a gun in mid-session and it appears in the picker in place, with your existing picks preserved; players are re-seated on their same physical guns after every rescan (which also hardened the manual Rescan button); a gun replugged on the same port gets its serial link reopened and game control re-entered automatically. If no gun was selected at all, the first arrival re-opens the picker on its own. A freshly plugged Sinden still pops the tracking border.
- **Disconnect and connect broadcasts** — new BlueprintAssignable events on the subsystem for host games:
  - **`OnGunDisconnected(Gun, PlayerIndex)`** — fired after the seat is freed and its backend torn down (`PlayerIndex` = -1 if the gun wasn't assigned). Bind it to pause the game and re-raise your gun configuration.
  - **`OnGunConnected(Gun)`** on arrivals, and **`OnDetectedGunsChanged`** for any UI that lists guns.
- Hot-plug is code-complete and smoke-tested; yank-and-replug reports from real rigs are welcome — the log tells the story (`Gun connected:` / `gun disconnected:` lines).

## Try it without Unreal

**`LightgunLab-Demo-1.0.2-Win64.zip`** is the aim test range as a standalone Windows app: unzip, run `LightgunLab.exe` (borderless fullscreen recommended; required for Sinden). Logs land at `%LOCALAPPDATA%\LightgunLab\Saved\Logs\LightgunLab.log` — attach that file to any hardware test report.

## Install (the plugin)

Drop the `LightgunLab` folder from the plugin zip into your project's `Plugins/`, enable, build (prebuilt Win64 editor binaries for UE 5.8.1 included). Hooker configs in `HookerConfigs/`.

MIT licensed.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
