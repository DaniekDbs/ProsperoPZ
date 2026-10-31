# ProsperoPZ launcher backend

A Java 8-compatible headless Project Zomboid launcher with no external
dependencies. The PS5 runs it under Wine with a small Windows Java 8 runtime;
it also runs on any desktop JDK for testing.

Build with `scripts/build-launcher.sh` (or `.ps1`). Run one request with
`java -jar prosperopz-launcher.jar --root ROOT --request REQUEST.properties`, or
keep a worker running with `--serve`. Requests, responses, progress and the
view are UTF-8 `key=value` files written atomically.

| Command | Request fields |
| --- | --- |
| `catalog` | Rescan `games/` and import sources (`imports/`, USB `/mnt/usbN`). |
| `import-game` | `source` (a listed folder), optional `game` (id) and `name`. Moves uploads from `imports/`, copies from USB. Creates a first profile if none exists. |
| `delete-game` | `game` |
| `create` | `game`, `mode=client|server`, optional `id`, `name` |
| `prepare-launch` | `id` |
| `clone`, `delete-instance` | `id` (deleted profiles move to `backups/`) |
| `rename-instance` | `id`, `name` |
| `server-password` | `id` of a server profile |
| `content-list` | `id`, `content_kind=mods|saves` |
| `content-import` | `id`, `content_kind`, `source` under `imports/` |
| `content-toggle` | `id`, `content_kind=mods`, `mod` (mod id), `enabled=true|false` |
| `content-backup` | `id`, `content_kind=saves`, `mod` (`MODE/NAME`) |
| `content-delete` | `id`, `content_kind`, `mod` |
| `settings` | `memoryMb` (1024-12288), `resolution`, `controllerMode=native|emulated`, `debugMode=on|off`, `themeColor`, `vrrMode`, `javaN`, `javaApi` |
| `install-java` | `major`, optional `repair=true` |
| `detect-java`, `view` | none |

`prepare-launch` reads the game's `ProjectZomboid64.json`, keeps its classpath
and compatible JVM options, replaces memory/GC/compressed-pointer/Steam options
for PS5, writes `profiles/launch.profile` and the private
`profiles/launch-plan.json`, and updates the profile's `options.ini` (client)
or `Server/NAME.ini` (server). The game JVM starts `org.prosperopz.Main
--execute-plan`, which calls the game's main class in the same JVM.

The game's own `jre64` is used when valid. Otherwise the required Java major
is taken from `jre64/release` (default 17) and a SHA-256-verified Adoptium
Windows x64 runtime is installed under `runtimes/javaN`.
