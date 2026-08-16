# Wiki build-out — full project wiki with step-by-step Blueprint guides

**Prompt for the authoring session.** Read this whole file, then build it. The repo is
`del1verance/lightgunlab`, local at `C:\UE581\LightgunLab` (UE 5.8.1 source build at `C:\UE581`).
The deliverable is the project's **GitHub wiki**, written as complete, self-sufficient
documentation — a gun owner or UE developer should never need to read the source to use the
plugin, and every Blueprint walkthrough must be exact enough to follow node by node.

## Ground rules

1. **The code is the only truth.** Before writing any page, re-verify every function name,
   signature, event, setting, default, and behavior against the source — do not trust this
   prompt's lists, the README, or memory of the project; all can drift. Authoritative files:
   - `Plugins/LightgunLab/Source/LightgunLab/Public/LightgunSubsystem.h` — the whole game-facing API + events
   - `Public/LightgunWeapon.h` — the weapon core + its delegates
   - `Public/LightgunSettings.h` — every config key, default, clamp, and category
   - `Public/LightgunTypes.h` — models, recoil modes, `FDetectedLightgun` fields
   - `Private/RecoilBackends.cpp` — per-gun protocol dialects + capability tables
   - `Private/LightgunRawInput.h` — how per-device aim routing and adoption work
   - `Private/LightgunDetection.cpp` — USB IDs and detection notes
   - `README.md`, `docs/release-notes-v1.0.*.md`, `HookerConfigs/README.md` — voice and framing
2. **Never invent a Blueprint node.** A node exists only if the UFUNCTION/UPROPERTY exists and
   is `BlueprintCallable`/`BlueprintPure`/`BlueprintAssignable`. Name nodes exactly as the BP
   palette shows them (e.g. the subsystem getter appears as **Get LightgunSubsystem** under
   Game Instance Subsystems; dynamic delegates bind via **Bind Event to On Gun Disconnected**).
   If unsure how a node is titled, derive it from UHT rules (DisplayName metadata, spacing of
   CamelCase) and say so in a wiki footnote rather than guessing silently.
3. **Publishing mechanics.** The wiki is its own git repo:
   `https://github.com/del1verance/lightgunlab.wiki.git`. It only exists after the wiki is
   initialized once through the GitHub web UI (repo Settings → Features → Wikis on, then create
   any first page). Pre-flight: try cloning; if the clone fails, STOP and ask the user to
   initialize the wiki, then continue. Pages are `.md` files at the repo root; `Home.md` is the
   landing page; `_Sidebar.md` is the navigation; `_Footer.md` optional. Page links use
   `[[Page-Name]]`. GitHub wiki renders Mermaid fences — use them for flow diagrams.
4. **Voice**: the project's existing register — direct, precise, lightly arcade-flavored,
   allergic to filler. Facts carry the personality ("recoil only ever means a live round").
5. Commit the finished wiki (all pages, one coherent push) and also commit a one-line pointer
   to the wiki in the repo README ("Full documentation lives in the wiki"), pushed to `main`.

## Page map

**Home** — what Lightgun Lab is in five sentences, a capability matrix table (gun × aim /
recoil / vibration / notes), and three doors: "I own a gun" → Try-the-Demo, "I'm building a
game" → Developer-Quickstart, "Something's wrong" → Troubleshooting.

**Try-the-Demo** — download the demo zip from the latest release, unzip, run, what you'll see
(picker → range), borderless fullscreen note, where the log lives for Shipping builds
(`%LOCALAPPDATA%\LightgunLab\Saved\Logs\LightgunLab.log`), and how to file a hardware report
(attach the log; what a good report contains).

**Developer-Quickstart** — copy `Plugins/LightgunLab` into `<Project>/Plugins/`, enable, build;
first-run flow; the two integration postures (test-bed style with the range vs host-game style
with `bShowRangeOnConfirm=false`); minimum viable integration in ~10 minutes.

**Per-gun pages** (one each: Sinden, GUN4IR, OpenFIRE, Blamcon, RS3-Reaper, GunCon-3,
Desktop-Mouse) — detection IDs, physical setup, software prerequisites (Sinden software ≥2.08a
with the recoil server started — NEVER the `tcpserver` launch arg; GunCon 3 needs the community
driver, link it), capabilities, quirks verbatim from the Known-issues research, and what its
line in the range footer / log looks like when healthy.

**Core-Concepts** — the ammo gate (why the plugin never fires recoil on empty/reload), game
control lifecycle (seize at selection, release on exit), recoil modes (Direct / Outputs /
Disabled), one Sinden TCP connection law, per-device raw routing + adoption (when "adopted
unmatched aim source" is normal), hot-plug behavior and the disconnect contract.

**Blueprint guides** — the heart of the wiki. Each is a numbered, node-by-node walkthrough
(node names bold, pin names in backticks, every wire stated), with a Mermaid sketch of the
graph where it helps. Required guides:
1. *Getting the subsystem* — Get Game Instance → **Get LightgunSubsystem**; promote to variable.
2. *Fire your first recoil* — button/trigger input → **Fire Recoil**; the ammo gate contract
   (your weapon decides live vs empty and calls **Fire Recoil** vs **Notify Empty**).
3. *A complete gun with `ULightgunWeapon`* — **Construct Object from Class** (LightgunWeapon),
   **Initialize** (`In Subsystem`, `In Player Index`), trigger → **Try Fire** (Branch on return),
   reload paths → **Reload**, bind **On Ammo Changed** / **On Dry Fire** / **On Reloaded** to
   drive a HUD widget.
4. *Pause on gun disconnect* — the user-requested flagship: **Bind Event to On Gun Disconnected**
   (payload `Gun`, `Player Index`) → **Set Game Paused** → **Show Startup Panel** (or your own
   config menu); companion **On Gun Connected** to unpause/reseat; note the slot is already
   freed when the event fires.
5. *Host-game confirm flow* — `bShowRangeOnConfirm=false` in `DefaultGame.ini`
   (`[/Script/LightgunLab.LightgunSettings]`), what confirm still does (StartRangeSession =
   control + routing), reopening the picker from a pause menu via **Show Startup Panel**.
6. *Two players* — reading **Get Detected Guns**, **Select Gun For Player**, the `...ForPlayer`
   event family, **Swap Players**, per-seat outputs.
7. *Feedback beyond recoil* — **Notify Damaged**, **Set Life**, **Rumble Pulse**,
   **Play Gun Effect** (Sinden `T2200` example), **Notify Reloaded** semantics per gun.
8. *Reskinning the panels* — Blueprint-subclassing the C++ widgets (startup panel, options,
   range, border), what NativeOnInitialized builds and what's safe to override.
9. *Settings from Blueprint and ini* — reading `ULightgunSettings` values (BlueprintReadOnly),
   which knobs exist (crosshair colors, strengths, pacing, border percents), full ini reference
   with defaults, and **Save Settings**.

**Outputs-Mode** — MAMEHooker/QMamehook/HOTR/Sinden-outputs integration, lifted and expanded
from `HookerConfigs/README.md`; every emitted output name (`P{n}_CtmRecoil` et al.) and when
it fires; per-app ini placement table.

**Troubleshooting** — symptom-first table (crosshair doesn't track / wrong gun kicks / recoil
silent / border missing / aim freezes in PIE / rumble not felt), each mapped to cause + fix +
the log line that confirms it. Include every bench-won Sinden pitfall and the GUN4IR
one-pulse-is-imperceptible fact. Log locations for editor, packaged Development, and Shipping.

**API-Reference** — every BlueprintCallable/Pure function and event on the subsystem and
weapon, grouped as in the header, one line each: signature, what it does, 1P/2P notes.
Generated by reading the headers, not from memory.

**FAQ** — engine version support, Fab plans, adding unknown guns via `IdOverrides`, multi-PC
vs one-PC two-player, license.

## Done =

Wiki repo pushed with every page above, `_Sidebar.md` mirroring the page map, Home's matrix
consistent with the per-gun pages, all cross-links resolving, every BP walkthrough verified
against the current headers (and flagged inline if anything could not be verified), README
pointer committed to `main`. No page may contradict the source at the commit it was written
against — state that commit hash in `_Footer.md`.
