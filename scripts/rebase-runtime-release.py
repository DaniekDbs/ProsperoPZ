#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a ProsperoPZ install tree on top of a published ProsperoMC runtime.

ProsperoMC's public install package carries a verified, game-independent
runtime: the PS5 Wine release (+ v0.2.0 core and Zink/RADV), Windows Java 8 for
the launcher backend and an offline-initialized Wine prefix, with their
licenses and source records. This script keeps exactly that runtime and
replaces everything title-specific with this repository's own build:

  - eboot.bin, sce_sys/, sce_module/ from dist/PPSA99777 (scripts/build-ps5.sh)
  - the launcher JAR (scripts/build-launcher.sh)
  - title id PPSA99888 -> PPSA99777, data root /data/ProsperoMC -> /data/ProsperoPZ,
    Wine prefix prefixes/minecraft -> prefixes/zomboid
  - ProsperoMC's title-specific elevation helper (lapy.elf) is dropped: it is
    only valid for PPSA99888 and must never run under another title. With
    --helper/--helper-manifest, a helper built for PPSA99777 is bundled instead
    (see scripts/build-lapy-helper.sh)
  - ProsperoMC's home music is dropped (licensed to that project only)

The result is written to dist/PPSA99777 (title) and dist/PPSA99777/prosperopz
(data), the layout scripts/package-release.py packages.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OLD_TITLE, NEW_TITLE = 'PPSA99888', 'PPSA99777'
DRIVE_TABLE = b'c:\t../drive_c\np:\t/data/ProsperoPZ\nz:\t/\n'
TEXT = {'.json', '.txt', '.md', '.reg'}
RENAMES = [(b'prefixes/minecraft', b'prefixes/zomboid'), (b'prefixes\\\\minecraft', b'prefixes\\\\zomboid'),
           (b'ProsperoMC', b'ProsperoPZ'), (b'prosperomc', b'prosperopz'), (b'PROSPEROMC', b'PROSPEROPZ'),
           (OLD_TITLE.encode(), NEW_TITLE.encode())]
# Title files that belong to ProsperoMC itself, never to the shared runtime.
TITLE_SKIP = {'eboot.bin', 'lapy.elf', 'lapy-manifest.json', 'STAGE-REPORT.json', 'APP-SOURCES.txt',
              'PROSPERO-WIN-PROSPEROMC-ADAPTATIONS.md', 'WEBP_LICENSE.txt', 'WEBP_PATENTS.txt',
              'THIRD_PARTY_NOTICES.md', 'LICENSE'}
TITLE_SKIP_DIRS = {'sce_sys', 'sce_module', 'launcher', 'prosperomc', 'prosperopz'}


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def rename_text(path: Path) -> None:
    if path.suffix.lower() not in TEXT:
        return
    data = original = path.read_bytes()
    for old, new in RENAMES:
        data = data.replace(old, new)
    if data != original:
        path.write_bytes(data)


def extract(archive: Path, target: Path) -> None:
    root = target.resolve()
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            destination = (target / item.filename).resolve()
            if not destination.is_relative_to(root):
                raise ValueError('Unsafe path in upstream archive: ' + item.filename)
        z.extractall(target)


def find(base: Path, name: str) -> Path:
    for candidate in [base / name, *base.glob('*/' + name)]:
        if candidate.is_dir():
            return candidate
    raise ValueError(f'{name} folder not found in the upstream archive')


def copy_filtered(source: Path, target: Path, skip) -> None:
    for path in sorted(source.rglob('*')):
        relative = path.relative_to(source)
        if path.is_symlink():
            raise ValueError('Symlink in upstream archive: ' + relative.as_posix())
        if not path.is_file() or skip(relative):
            continue
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)


PROTOCOL = ROOT / 'vendor/prospero-win/native/lapy_elevation_protocol.h'


def validate_helper(elf: Path, manifest_path: Path) -> dict:
    """Accept only a helper built for this title with the matching wire protocol."""
    for path in (elf, manifest_path):
        if not path.is_file() or not path.stat().st_size:
            raise ValueError(f'Helper input missing or empty: {path}')
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    problems = []
    if elf.read_bytes()[:4] != b'\x7fELF':
        problems.append('lapy.elf is not an ELF')
    if manifest.get('target_title') != NEW_TITLE:
        problems.append(f"manifest target_title {manifest.get('target_title')!r} != {NEW_TITLE}")
    if manifest.get('mode') != 'elf-helper':
        problems.append('manifest mode is not elf-helper')
    if manifest.get('elf_sha256') != digest(elf):
        problems.append('manifest elf_sha256 does not match lapy.elf')
    if manifest.get('protocol_sha256') != digest(PROTOCOL):
        problems.append('manifest protocol_sha256 does not match the title wire protocol')
    if 'root_layout_probe_retry' not in (manifest.get('features') or []):
        problems.append('manifest lacks the root_layout_probe_retry safety feature')
    if problems:
        raise ValueError('Elevation helper rejected:\n  ' + '\n  '.join(problems))
    return manifest


def rebase(upstream: Path, title: Path, launcher: Path, upstream_label: str,
           helper: Path = None, helper_manifest: Path = None) -> dict:
    helper_info = None
    if bool(helper) != bool(helper_manifest):
        raise ValueError('Pass --helper and --helper-manifest together, or neither')
    if helper:
        helper_info = validate_helper(helper, helper_manifest)
    for required in ('eboot.bin', 'sce_sys/param.json', 'sce_module/libc.prx'):
        if not (title / required).is_file():
            raise ValueError(f'Build the native title first (scripts/build-ps5.sh): {title / required} missing')
    if json.loads((title / 'sce_sys/param.json').read_text(encoding='utf-8-sig')).get('titleId') != NEW_TITLE:
        raise ValueError('dist title is not ' + NEW_TITLE)
    data = title / 'prosperopz'
    if data.exists():
        shutil.rmtree(data)
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        extract(upstream, work)
        old_title, old_data = find(work, OLD_TITLE), find(work, 'prosperomc')
        upstream_report = json.loads((old_title / 'STAGE-REPORT.json').read_text())

        def title_skip(relative: Path) -> bool:
            parts = relative.parts
            return (relative.as_posix() in TITLE_SKIP or parts[0] in TITLE_SKIP_DIRS
                    or relative.name.lower() == 'snd0.at9')
        copy_filtered(old_title, title, title_skip)

        def data_skip(relative: Path) -> bool:
            return relative.parts[0] == 'launcher' or relative.as_posix() == 'STAGE-REPORT.json'
        copy_filtered(old_data, data, data_skip)
        if (data / 'prefixes/minecraft').is_dir():
            (data / 'prefixes/minecraft').rename(data / 'prefixes/zomboid')
    # Runtime metadata named after the old title (e.g. prosperomc-prefix.json).
    for path in sorted(data.rglob('*'), key=lambda p: len(p.parts), reverse=True):
        if 'prosperomc' in path.name.lower():
            path.rename(path.with_name(path.name.replace('prosperomc', 'prosperopz').replace('ProsperoMC', 'ProsperoPZ')))
    if not (data / 'prefixes/zomboid/system.reg').is_file():
        raise ValueError('Upstream package lacks the offline Wine prefix')
    if not (data / 'runtimes/java8/bin/java.exe').is_file():
        raise ValueError('Upstream package lacks the launcher Java 8 runtime')
    # Only reports' path entries and the prefix configuration are renamed. Source
    # and license records (.txt/.md) keep their exact bytes: their hashes are pinned.
    for path in list(title.glob('*.json')) + list((data / 'prefixes/zomboid').glob('*.reg')) + \
            list((data / 'prefixes/zomboid').glob('*.json')):
        if path.is_file():
            rename_text(path)
    (data / 'prefixes/zomboid/dosdevices').mkdir(parents=True, exist_ok=True)
    (data / 'prefixes/zomboid/dosdevices/.pw-symlinks').write_bytes(DRIVE_TABLE)
    for base in (title, data):
        (base / 'launcher').mkdir(parents=True, exist_ok=True)
        shutil.copy2(launcher, base / 'launcher/prosperopz-launcher.jar')
    shutil.copy2(ROOT / 'vendor/prospero-win/PROSPEROPZ-ADAPTATIONS.md', title / 'PROSPERO-WIN-PROSPEROPZ-ADAPTATIONS.md')
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(ROOT / name, title / name)
    if helper_info:
        # The one-shot /data elevation helper, built for this title (PPSA99777).
        shutil.copy2(helper, title / 'lapy.elf')
        shutil.copy2(helper_manifest, title / 'lapy-manifest.json')
    (title / 'APP-SOURCES.txt').write_text(
        'ProsperoPZ: source in the accompanying repository.\n'
        'Runtime (Wine, Zink, Java 8, Wine prefix) taken unchanged from ' + upstream_label + '.\n'
        'Wine native title base: https://github.com/mpereiraesaa/prospero-win commit b510eb3db8709bb251d3da3c56850e96e057f489\n'
        'Native foundation: https://github.com/mpereiraesaa/ps5-native-app-boilerplate commit 9c0b994a048521af6fb84c73ded364504fe250e9\n'
        'Embedded font: Montserrat, SIL Open Font License 1.1.\n'
        'Wine/runtime module exact sources: WINE_SOURCES.txt.\n', encoding='utf-8')

    wine = dict(upstream_report.get('wine', {}))
    unix = title / 'win/wine/lib/wine/x86_64-unix'
    for name, key in (('ntdll', 'ntdllSha256'), ('win32u', 'win32uSha256')):
        if wine.get(key) and digest(unix / f'{name}.prx') != wine[key]:
            raise ValueError(f'Upstream {name}.prx does not match its own stage report')
    report = {
        'schema': 'prosperopz-stage/1', 'titleId': NEW_TITLE,
        'structurallyComplete': True, 'consoleValidated': False, 'readyToPlay': False,
        'wine': wine, 'prefix': upstream_report.get('prefix'),
        'javaSha256': digest(data / 'runtimes/java8/bin/java.exe'),
        'jarSha256': digest(launcher), 'ebootSha256': digest(title / 'eboot.bin'),
        'wineDriveTableSha256': digest(data / 'prefixes/zomboid/dosdevices/.pw-symlinks'),
        'dataRoot': '/data/ProsperoPZ', 'sandboxFallback': None, 'helper': helper_info,
        'runtimeSource': upstream_label,
        'limitations': ['Requires homebrew-enabled PS5 and Project Zomboid console validation (Java, LWJGL, FMOD, RakNet)',
                        'Wine/Java/Zink runtime reused unchanged from ' + upstream_label]
                       + (['Elevation helper built for PPSA99777 but not console-tested by the build']
                          if helper_info else
                          ['/data must already be granted to the title; no title-specific elevation helper is included']),
    }
    report = json.loads(json.dumps(report).replace('prefixes/minecraft', 'prefixes/zomboid'))
    for base in (title, data):
        (base / 'STAGE-REPORT.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for path in title.rglob('*'):
        if path.is_file() and path.suffix in {'.prx', '.bin', '.elf'}:
            path.chmod(0o755)
    leftovers = [p.relative_to(title).as_posix() for p in title.rglob('*')
                 if 'minecraft' in p.as_posix().lower() or 'prosperomc' in p.as_posix().lower()]
    if leftovers:
        raise ValueError('Minecraft-named files left in the package: ' + ', '.join(leftovers[:10]))
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--upstream', type=Path, required=True, help='ProsperoMC install .zip')
    parser.add_argument('--upstream-label', default='ProsperoMC public release')
    parser.add_argument('--title', type=Path, default=ROOT / 'dist' / NEW_TITLE)
    parser.add_argument('--launcher', type=Path, default=ROOT / 'artifacts/launcher/prosperopz-launcher.jar')
    parser.add_argument('--helper', type=Path, help='lapy.elf elevation helper built for PPSA99777')
    parser.add_argument('--helper-manifest', type=Path, help='its lapy-manifest.json')
    args = parser.parse_args()
    try:
        report = rebase(args.upstream, args.title, args.launcher, args.upstream_label,
                        args.helper, args.helper_manifest)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print('ERROR:', error, file=sys.stderr)
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
