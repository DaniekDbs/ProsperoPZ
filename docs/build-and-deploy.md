# Build and deploy

## Quick update from Windows

From CMD in the project folder:

```bat
ProsperoPZ.cmd build-send
```

This builds the Java launcher and native PS5 app, stages their checksums, and
sends an app update to `192.168.1.111:2121`. Close Project Zomboid and
ProsperoPZ and start the PS5 FTP server first. Updates preserve games,
profiles, worlds, mods, settings, Java runtimes and the Wine prefix. To change
the address: `ProsperoPZ.cmd build-send 192.168.1.50 2121`. This updates an
existing installation; use the full steps below the first time.

## Building

| Part | Command | Output |
| --- | --- | --- |
| Java launcher backend | `bash scripts/build-launcher.sh` (or `scripts/build-launcher.ps1`) | `artifacts/launcher/prosperopz-launcher.jar` |
| Native PS5 title | `bash scripts/build-ps5.sh` (Linux/WSL) | `dist/PPSA99777` |
| Artwork (optional) | `python3 scripts/generate-artwork.py && python3 scripts/prepare-artwork.py` | icon, backgrounds, `app/native/pm_art.h` |

The native build downloads and verifies its pinned PS5 SDK and native
foundation through `scripts/setup-ps5-dependencies.sh`.

## Staging a full installation

Prepare an offline Wine prefix with `scripts/prepare-wine-prefix.sh`, then:

```bash
python3 scripts/stage-ps5.py --wine WINE_RUNTIME --java WINDOWS_JAVA8 \
  --launcher artifacts/launcher/prosperopz-launcher.jar --prefix PREFIX \
  [--zomboid "PATH/TO/ProjectZomboid"]
```

`--zomboid` copies **your own** Windows game folder into the staged
`imports/` so the console imports it on first start; leave it out for anything
you share. The stage script refuses missing modules, a GDI-only runtime and
credential files, and writes `STAGE-REPORT.json`.
`structurallyComplete=true` does not mean the game was tested on a console.

## Installing on the PS5

- Copy `dist/PPSA99777` to `/data/homebrew/PPSA99777`.
- Copy `dist/PPSA99777/prosperopz` to `/data/ProsperoPZ`.

`scripts/deploy-ps5.py --host IP --port 2121` does both over FTP, verifies
lengths, publishes executables last and skips unchanged files on later runs.

## Sending the game, mods and saves

```bash
python3 scripts/import-zomboid.py --ftp PS5_IP            # game only
python3 scripts/import-zomboid.py --ftp PS5_IP --mods --saves
python3 scripts/import-zomboid.py --dest E:/                # USB drive instead
```

The helper finds Steam libraries (and common GOG paths), skips the 32-bit
runtime and launchers the PS5 does not need, and resumes interrupted copies.
Workshop items go to `imports/mods/ITEM`, worlds to `imports/saves/MODE-NAME`.
On the console use **Games → Import Game** and **Mods & Saves → Import**.

## Measuring graphics

The Zink/Mesa build helpers (`scripts/build-graphics-batching.sh`,
`scripts/build-wine-*.sh`) are unchanged from ProsperoMC. Build with
`PROSPEROPZ_BATCH_TIMINGS=1` to log aggregate draw-batch timings every five
seconds; default builds omit this instrumentation.
