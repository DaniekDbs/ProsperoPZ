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

## Update from an earlier version

Your games, profiles, mods and worlds are kept.

1. Close Project Zomboid and ProsperoPZ on the PS5.
2. Download the **`-update.zip`** below and unzip it on your PC or phone.
3. Copy the two folders onto the PS5, merging into the existing folders and overwriting files when asked (do not delete the folders first):

   | From the zip | Copy its contents into |
   | --- | --- |
   | `PPSA99777/` | `/data/homebrew/PPSA99777/` |
   | `prosperopz/` | `/data/ProsperoPZ/` |

4. Restart the PS5 (recommended) and open ProsperoPZ.

## Still in progress

Tested on PS5 Phat consoles on firmware 11.60 and 13.60 with Project Zomboid Build 42 (42.21.0): single player runs well, including Java mods through ZombieBuddy. Other firmwares and models, Build 41, multiplayer and the dedicated server are not tested yet. Steam features are unavailable (`-nosteam`).

Project Zomboid files, worlds and credentials are not included. Third-party source and license notices must accompany the distributed runtime.
