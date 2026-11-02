# Changelog

Each release on GitHub starts with its section from this file.

## v0.1.8-alpha

- **Fixed:** crashes when zooming out, and 2K/4K failing to start the game. With a large Game memory setting, Java took the memory the GPU needs for the screen. The Java heap is now capped at 5120 MB at 1080p, 4096 MB at 1440p and 3072 MB at 4K, and the launch message says when it was capped. 1080p is recommended.
- **Fixed:** the controller no longer stays disconnected after opening the PS button menu.

## v0.1.7-alpha

- **Changed:** the touchpad press is now the Xbox Back button (health/character panel). The PS5 keeps the Create button for its own menu, so games rarely receive it.
- **Docs:** controller confirmed working; a PS5 Phat on firmware 13.60 added to the tested consoles; where to find the logs for bug reports.

## v0.1.6-alpha

- **Fixed:** the controller works in game. In Native gamepad mode it is enabled in Project Zomboid automatically, with no need to tick Enable in Options → Controller.
- **Docs:** tested on a PS5 Phat on firmware 11.60; the README link in release notes works.

## v0.1.5-alpha

- **Added:** Java mods through [ZombieBuddy](https://github.com/zed-0xff/ZombieBuddy), such as Viewpoint. Put `ZombieBuddy.jar` in the game folder and enable the mods in game.

## v0.1.4-alpha

- **Fixed:** Build 42 black screen that returned to the launcher. The game's media file watcher ran the PS5 out of file handles; it is now disabled when the game loads.

## v0.1.3-alpha

- **Changed:** the game is no longer started with `-debug`.

## v0.1.2-alpha

- **Fixed:** importing Build 42 failed with "Unsupported classpath entry: .".

## v0.1.1-alpha

- **Fixed:** "errno 2" when opening the app. The release now includes a `/data` access helper built for this app's title id (PPSA99777).

## v0.1.0-alpha

- First release: Project Zomboid launcher for PS5 forked from ProsperoMC, with game import, profiles, mods, world backups, dedicated server profiles and controller support.
