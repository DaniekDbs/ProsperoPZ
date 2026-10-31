# Native frontend preview

The PNG files in `artifacts/frontend/` come from `pm_frontend_render()` writing
an upstream `PwPresentTarget`, followed by PPM-to-PNG conversion. The console
frontend uses that exact renderer; these are CPU framebuffer renders, not a
browser mockup.

Run `bash tests/frontend/run.sh` from a host with a C compiler, then
`python app/preview/convert.py` with Pillow installed (WSL works on Windows).
The tests compile as C99 with `-Wall -Wextra -Werror`.

`render output.ppm SCREEN [launcher-view.properties|--empty|--OPTION]` renders
real backend state or the built-in fixture. Screens 0-5 are Home, Profiles,
Games, Mods & Saves, Settings and Server. Options: `--import`, `--saves`,
`--rename`, `--close`, `--resolution`, `--memory`, `--input`, `--theme`,
`--motion [ms]`, `--palette=NAME`. The fixture (two game builds, eight
profiles, five mods) is illustrative and does not show that a game ran.

Montserrat Medium is rasterized into `pm_font.h`; its SIL OFL license is stored
beside it. Rebuild with `generate_font.py FONT.ttf app/native/pm_font.h`.

## Navigation

L1/R1 changes tabs. The d-pad selects rows; Right moves from a list to its
actions and Left returns. Cross selects, Circle goes back (Home from any tab),
Triangle refreshes (games, the current mods/saves list, or Java runtimes) and
Square runs the screen's main shortcut (new profile, import game, import
mods/worlds, new server). A running backend command blocks duplicate actions
but keeps navigation responsive. A USB mouse moves the pointer (left click
selects, right click goes back, wheel scrolls); a USB keyboard types in the
rename dialog.
