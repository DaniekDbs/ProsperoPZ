#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
UPSTREAM=vendor/prospero-win/src
if [ ! -f "$UPSTREAM/pw_present.h" ]; then
  UPSTREAM=.deps/source/prospero-win-b510eb3db8709bb251d3da3c56850e96e057f489/src
fi
mkdir -p artifacts/frontend
mkdir -p artifacts/frontend/icons
python3 scripts/build-webp-host.py
cp assets/generated/prosperopz-icon.png artifacts/frontend/icons/custom.png
cc -std=c99 -Wall -Wextra -Werror -O2 -I app/native -I "$UPSTREAM" app/native/pm_frontend.c tests/frontend/navigation.c -o artifacts/frontend/navigation
artifacts/frontend/navigation
cc -std=c99 -Wall -Wextra -Werror -O2 -I app/native -I "$UPSTREAM" tests/frontend/rounded-spans.c -o artifacts/frontend/rounded-spans
artifacts/frontend/rounded-spans
cc -std=c99 -Wall -Wextra -Werror -O2 -I app/native -I "$UPSTREAM" -I .deps/native/libwebp/libwebp-1.6.0 app/native/pm_frontend.c app/native/pm_icons.c app/preview/render.c build/tests/webp/libwebpdecoder.a -lm -o artifacts/frontend/render
for screen in 0 1 2 3 4 5; do
  artifacts/frontend/render "artifacts/frontend/screen-$screen.ppm" "$screen"
done
artifacts/frontend/render artifacts/frontend/empty.ppm 0 --empty
artifacts/frontend/render artifacts/frontend/games-empty.ppm 2 --empty
artifacts/frontend/render artifacts/frontend/server-empty.ppm 5 --empty
artifacts/frontend/render artifacts/frontend/import.ppm 2 --import
artifacts/frontend/render artifacts/frontend/saves.ppm 3 --saves
artifacts/frontend/render artifacts/frontend/close.ppm 0 --close
artifacts/frontend/render artifacts/frontend/resolution.ppm 4 --resolution
artifacts/frontend/render artifacts/frontend/memory.ppm 4 --memory
artifacts/frontend/render artifacts/frontend/controller-input.ppm 4 --input
artifacts/frontend/render artifacts/frontend/theme.ppm 4 --theme
artifacts/frontend/render artifacts/frontend/motion.ppm 0 --motion
artifacts/frontend/render artifacts/frontend/rename.ppm 1 --rename
for palette in knox blood rosewood louisville muldraugh westpoint; do
  artifacts/frontend/render "artifacts/frontend/settings-$palette.ppm" 4 "--palette=$palette"
done
