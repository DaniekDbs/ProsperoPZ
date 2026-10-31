# ProsperoPZ artwork

`prosperopz-icon.png` and `prosperopz-world.png` are drawn procedurally by
`scripts/generate-artwork.py` (a house with a lit window, a fire axe and a
zombie hand inside a lime ring; a Kentucky town under a blood moon). They use
no Project Zomboid, Minecraft or PlayStation artwork.

`scripts/prepare-artwork.py` encodes them into the native UI's RGB565 header
and the loader's icon and backgrounds. `launcher-home.png` and
`launcher-games.png` are renders of the native UI from `tests/frontend/run.sh`.
