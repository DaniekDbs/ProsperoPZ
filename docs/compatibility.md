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

## Needs console testing

| Area | Expectation |
| --- | --- |
| Client start under Wine + Zink | Same Java/LWJGL/OpenGL path as ProsperoMC, which runs Minecraft on this runtime. Project Zomboid's LWJGL build and GL usage are different and untested. |
| Audio (FMOD) | FMOD uses Windows audio APIs that Wine maps to the PS5 audio port. Untested. |
| Multiplayer (RakNet) | Native UDP sockets through Wine. LAN hosting/joining by IP is expected to work; untested. |
| Bundled `jre64` | Used as-is. If a build's runtime fails, delete its `jre64` folder on the PS5 and ProsperoPZ downloads Temurin of the same Java version. |
| Build 42 | Supported by the launcher; heavier on memory (use 4096 MB or more). |

## Steam

The game runs with `-nosteam`. That means no Workshop downloads, Steam friends or invites, and Steam-only servers cannot be joined. Copy mods from your PC instead; for multiplayer, everyone uses `-nosteam`.

## Reporting a result

Include firmware, loader, game build (41 or 42), mods and the steps you tried. Attach `Zomboid/console.txt` from the profile folder. Both successful runs and failures help.
