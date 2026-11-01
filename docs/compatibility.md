# Compatibility notes

ProsperoPZ is experimental. This page separates what has been checked from what still needs a console.

## Verified on the host (automated tests)

| Area | What the tests cover |
| --- | --- |
| Game import | Detects Build 41 (`java/zombie`) and Build 42 (`projectzomboid.jar`) layouts, reads `ProjectZomboid64.json` (BOM and trailing commas tolerated), refuses incomplete folders, moves FTP uploads instantly. |
| Launch plan | Rewrites JVM options for PS5, keeps the game's own options, builds the classpath and native paths, and passes `-nosteam -cachedir=...`. The generated JVM flags are started on a real JDK, and the plan reaches a stub `MainScreenState.main` with the expected arguments. |
| Dedicated server | `zombie.network.GameServer`, `-servername`, generated admin password (kept out of the launch profile), server `.ini` defaults and mod/map sync. |
| Mods | Steam Workshop layout, `.zip` imports, Build 42 `42/` + `common/` folders, `default.txt` editing that preserves the maps block, dependency auto-enable, deletion. |
| Saves | Listing, `.zip` backup, move-to-backups delete, restore from `.zip`. |
| Native UI and bridge | Controller and pointer navigation for every screen, bounded view parsing, request serialization, executable allow-list (only the game's `jre64` or a managed runtime), framebuffer bounds (also under AddressSanitizer/UBSan). |

## Tested on a console

Console: **PS5 "Phat" (original model), firmware 11.60**, Relapse + kstuff lite, ShadowMountPlus, PS5 Web UI Explorer. A tester also runs Build 42 on a **PS5 Phat on firmware 13.60**.

| Area | Result |
| --- | --- |
| `/data` access, launcher, game import, profiles | Works. |
| Build 42 (42.21.0) client, bundled Java 25 `jre64` | Works and runs well: OpenGL 4.6 through Zink on the PS5 GPU, FMOD audio initializes. Build 42's media file watcher is disabled by the launcher (it ran the console out of file handles). |
| Mods, including Java mods via ZombieBuddy (Viewpoint) | Works. Each Build 42 mod needs `42/mod.info`. |
| Native gamepad | Works (enabled automatically since v0.1.6). The touchpad press is Xbox Back, as the system keeps Create. |
| Known issues (13.60 report) | The game can close while zooming far in/out; resolutions above 1080p from the launcher fail, and changing it in game gives graphical errors. Under investigation: logs welcome. |

## Needs console testing

| Area | Expectation |
| --- | --- |
| Other firmwares and models (Slim, Pro) | Expected to work where ProsperoMC works; untested. |
| Build 41 | Supported by the launcher; untested on a console. |
| Multiplayer (RakNet) | Native UDP sockets through Wine. LAN hosting/joining by IP is expected to work; untested. |
| Dedicated server | Starts the headless server; untested on a console. |

## Steam

The game runs with `-nosteam`. That means no Workshop downloads, Steam friends or invites, and Steam-only servers cannot be joined. Copy mods from your PC instead; for multiplayer, everyone uses `-nosteam`.

## Reporting a result

Include firmware, loader, game build (41 or 42), mods and the steps you tried. Attach `Zomboid/console.txt` from the profile folder. Both successful runs and failures help.
