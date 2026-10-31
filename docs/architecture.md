# ProsperoPZ architecture

ProsperoPZ is a Project Zomboid launcher for homebrew PS5. Its native C
frontend draws directly into prospero-win's framebuffer. Wine supplies the
Windows API for the Windows x64 Java runtime, LWJGL, FMOD and RakNet native
libraries. OpenGL goes through Zink to the PS5's Vulkan driver.

## Processes

The title keeps the native launcher on screen while Wine runs a small Windows
Java 8 runtime with the launcher JAR in `--serve` mode. The frontend writes
bounded, atomic `request.properties` files; the JVM handles one at a time and
writes `progress.properties`, `response.properties` and
`launcher-view.properties`. Settings saves and import-folder scans are handled
directly in the native bridge, so they keep working if Java is unavailable.

`prepare-launch` writes two files:

- `profiles/launch.profile` for prospero-win: the game's Java executable, the
  JVM arguments, the game folder as working directory, the `zomboid` Wine
  prefix, Zink graphics, display mode and controller mapping.
- `profiles/launch-plan.json` (private): the main class and game arguments,
  including a server's admin password.

The title then restarts into the game JVM. Its entry point is
`org.prosperopz.Main --execute-plan`, which installs the PS5 networking
compatibility layer and calls `zombie.gameStates.MainScreenState.main` (or
`zombie.network.GameServer.main` for a server) in the same JVM, because the PS5
port cannot create child processes. When the game exits, the title returns to
a fresh launcher.

The native bridge only starts a launch profile whose id and prefix are
`zomboid` and whose executable is `games\ID\jre64\bin\java.exe` or
`runtimes\javaN\bin\java.exe` under the data root.

## Data layout (`/data/ProsperoPZ`)

| Path | Contents |
| --- | --- |
| `games/ID/` | Imported Windows game folders, unchanged (`ProjectZomboid64.json`, `media/`, `java/` or `projectzomboid.jar`, DLLs, `jre64/`). |
| `instances/ID/instance.properties` | A profile: name, game id, mode (`client`/`server`), server name, admin password. |
| `instances/ID/Zomboid/` | The profile's game folder, passed as `-cachedir`: `Saves/`, `mods/`, `Server/`, `options.ini`, `console.txt`. |
| `imports/` | Game folders, `imports/mods`, `imports/saves` waiting to be imported. |
| `backups/` | World `.zip` backups, deleted worlds and deleted profiles. |
| `runtimes/javaN/` | Windows Java runtimes: Java 8 runs the launcher; others are fallbacks when a game has no usable `jre64`. |
| `prefixes/zomboid/` | The Wine prefix. |
| `launcher/` | The launcher JAR, refreshed from the app package. |

`/app0` is the immutable app package.

## Backend classes

| Class | Role |
| --- | --- |
| `Main` | Request loop, commands, settings and view output. |
| `Games` | Game install detection, descriptor parsing, import from `imports/` or USB, deletion. |
| `ZomboidLaunch` | JVM option rewriting, classpath, `options.ini`, launch profile and plan, in-JVM execution. |
| `Mods` | `mod.info` parsing (B41/B42), `mods/default.txt`, dependencies, imports, server `.ini` sync. |
| `Saves` | World listing, backup, removal and restore. |
| `ZomboidText` | Lenient reader/merger for the game's own text formats. |
| `JavaRuntime` | Windows JVM validation and verified Temurin downloads. |
| `Ps5Dns`, `Ps5DirectoryDns`, `Ps5Compat` | Networking compatibility for Java on PS5 Wine. |
| `StartupDiagnostics`, `HeapDiagnostics`, `FileDiagnostics`, `GameplayProfile` | Bounded diagnostics for console investigation. |

## Evidence

Host tests prove packaging, request handling, launch planning and UI
behavior. They do not prove that Project Zomboid, FMOD or RakNet run on a PS5;
see [compatibility](compatibility.md).
