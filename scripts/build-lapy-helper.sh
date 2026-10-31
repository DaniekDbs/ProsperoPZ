#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Build the one-shot /data elevation helper (lapy.elf) for ProsperoPZ's title.
#
# ProsperoPZ needs the same cooperative helper ProsperoMC uses to reach /data on
# a jailbroken PS5: a small ELF streamed to the local Lapy service, which grants
# this title's process access to /data. The helper is title-specific, so it must
# be compiled with ProsperoPZ's own id (PPSA99777); ProsperoMC's helper refuses
# a different title. This builds it from the public PS5-Lapy-JB-Daemon source
# (MIT) at a pinned commit, using the PS5 payload SDK and prospero-win's own
# ps5log client. Only the logging client header differs from an upstream build;
# the kernel operations are the pinned source, unchanged.
#
# Output: $OUT/lapy.elf and $OUT/lapy-manifest.json (default build/lapy).
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)

# Pinned Lapy source. Bump deliberately; a new commit is a new helper to test.
lapy_repo=${LAPY_REPO:-https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon.git}
lapy_commit=${LAPY_COMMIT:-b48b7d7236eca25c7b9dfd6c040c763ae05dde80}
title=${TARGET_TITLE:-PPSA99777}

# The payload SDK the native title build already prepared.
foundation_pin=9c0b994a048521af6fb84c73ded364504fe250e9
sdk=${PS5_PAYLOAD_SDK:-$root/.deps/source/ps5-native-app-boilerplate-$foundation_pin/.deps/native/ps5-payload-sdk}
out=${1:-$root/build/lapy}
work=$root/.deps/source/lapy-$lapy_commit

[[ $title =~ ^PPSA[0-9]{5}$ ]] || { echo "TARGET_TITLE must be a PPSA id" >&2; exit 2; }
[[ -x $sdk/bin/prospero-clang ]] || { echo "PS5 payload SDK missing: $sdk/bin/prospero-clang" >&2; exit 2; }

if [[ ! -d $work/.git ]]; then
    rm -rf "$work"
    git clone --quiet "$lapy_repo" "$work"
fi
git -C "$work" fetch --quiet origin "$lapy_commit" 2>/dev/null || git -C "$work" fetch --quiet --all
git -C "$work" checkout --quiet "$lapy_commit"

# The wire protocol the helper speaks must be exactly the one the title was
# built against, or the staged package will reject the helper.
protocol=$root/vendor/prospero-win/native/lapy_elevation_protocol.h
if ! cmp -s "$work/source/lapy_elevation_protocol.h" "$protocol"; then
    echo "Lapy protocol header does not match prospero-win; refusing to build an incompatible helper" >&2
    exit 2
fi

# A ps5log/1 logging client: prospero-win's own header provides every symbol the
# helper uses (ps5log_init_default/printf/close, PS5LOG_*). Logging is diagnostic
# only; it does not touch the elevation path.
logging=$(mktemp -d)
trap 'rm -rf "$logging"' EXIT
cp "$root/vendor/prospero-win/native/ps5log/ps5log.h" "$logging/ps5log.h"

echo "Building $title helper from Lapy $lapy_commit"
( cd "$work" && make --quiet owned-helper \
    PS5_PAYLOAD_SDK="$sdk" LOGGING_CLIENT="$logging" TARGET_TITLE="$title" )

built=$work/build/owned_root_helper-$title
[[ -f $built/lapy.elf && -f $built/lapy-manifest.json ]] || {
    echo "Helper build did not produce lapy.elf and lapy-manifest.json" >&2; exit 2; }
mkdir -p "$out"
cp "$built/lapy.elf" "$built/lapy-manifest.json" "$out/"

python3 - "$out/lapy.elf" "$out/lapy-manifest.json" "$protocol" "$title" <<'PY'
import hashlib, json, sys
elf, manifest_path, protocol, title = sys.argv[1:5]
def digest(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()
manifest = json.load(open(manifest_path))
problems = []
if open(elf, 'rb').read(4) != b'\x7fELF': problems.append('output is not an ELF')
if manifest.get('target_title') != title: problems.append('manifest target_title != ' + title)
if manifest.get('mode') != 'elf-helper': problems.append('manifest mode != elf-helper')
if manifest.get('elf_sha256') != digest(elf): problems.append('manifest elf_sha256 mismatch')
if manifest.get('protocol_sha256') != digest(protocol): problems.append('protocol_sha256 != prospero-win header')
if 'root_layout_probe_retry' not in (manifest.get('features') or []): problems.append('missing root_layout_probe_retry feature')
if problems:
    print('Helper manifest invalid:\n  ' + '\n  '.join(problems), file=sys.stderr); sys.exit(1)
print('OK ' + title + ' helper sha256=' + manifest['elf_sha256'])
PY
echo "Helper ready: $out/lapy.elf"
