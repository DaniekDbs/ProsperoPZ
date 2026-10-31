# ProsperoPZ — Experimental Alpha

Project Zomboid (Windows build) on a homebrew-enabled PS5, with a native launcher built around your controller.

## Included

- One-step import of your own game folder from FTP or USB; Build 41 and Build 42 side by side.
- Profiles with their own worlds, mods and options.
- Mod import (Steam Workshop folders and `.zip`), toggling with automatic dependencies, posters.
- World backups to `.zip`, safe deletion to `backups/`, restore from a PC.
- Dedicated server profiles with admin password and mod/map sync.
- Native gamepad or mouse/keyboard emulation; USB keyboard and mouse.
- PS5-tuned JVM settings, resolution, theme and display-pacing settings.

## Install

Copy the app folder to `/data/homebrew/PPSA99777` and the `prosperopz` data folder to `/data/ProsperoPZ`. Refresh your loader, open ProsperoPZ, then follow the [README](../../README.md#getting-started) to import your game.

When updating, close Project Zomboid and ProsperoPZ first and keep your `/data/ProsperoPZ` folder.

## Still in progress

Tested on a PS5 Phat (original model) on firmware 11.60 with Project Zomboid Build 42 (42.21.0): single player runs well, including Java mods through ZombieBuddy. Other firmwares and models, Build 41, multiplayer and the dedicated server are not tested yet. Steam features are unavailable (`-nosteam`).

Project Zomboid files, worlds and credentials are not included. Third-party source and license notices must accompany the distributed runtime.
