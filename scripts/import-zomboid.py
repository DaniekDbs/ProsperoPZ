#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copy your own Project Zomboid (Windows build) to a PS5 running ProsperoPZ.

Finds the Steam (or GOG) installation on this PC, then either uploads it over
FTP to /data/ProsperoPZ/imports, or copies it to a folder such as a USB drive.
Optionally includes downloaded Steam Workshop mods and your saves.

  python scripts/import-zomboid.py --ftp 192.168.1.50
  python scripts/import-zomboid.py --ftp 192.168.1.50 --mods --saves
  python scripts/import-zomboid.py --dest E:/ --game "D:/Games/ProjectZomboid"

Then open ProsperoPZ > Games > Import Game (and Mods & Saves > Import).
Only files you already own are copied; nothing is downloaded or shared.
Re-running skips files that already have the same size, so it can resume.
"""
from __future__ import annotations
import argparse
import ftplib
import os
from pathlib import Path, PurePosixPath
import re
import sys

APP_ID = '108600'
DATA_ROOT = '/data/ProsperoPZ'
# Never needed on PS5: 32-bit runtime and launchers, debug symbols, Steam's crash handler.
SKIP_DIRS = {'jre', 'win32', '__pycache__'}
SKIP_FILES = re.compile(r'(?i)^(ProjectZomboid32\.(exe|json)|.*\.pdb|steamerrorreporter.*\.exe)$')


def steam_libraries() -> list[Path]:
    roots = []
    if os.name == 'nt':
        for env in ('ProgramFiles(x86)', 'ProgramFiles'):
            if os.environ.get(env):
                roots.append(Path(os.environ[env]) / 'Steam')
        try:
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam') as key:
                roots.insert(0, Path(winreg.QueryValueEx(key, 'SteamPath')[0]))
        except OSError:
            pass
    home = Path.home()
    roots += [home / '.steam/steam', home / '.local/share/Steam',
              home / 'Library/Application Support/Steam']
    libraries = []
    for root in roots:
        vdf = root / 'steamapps/libraryfolders.vdf'
        if vdf.is_file():
            for match in re.finditer(r'"path"\s+"([^"]+)"', vdf.read_text(encoding='utf-8', errors='replace')):
                libraries.append(Path(match.group(1).replace('\\\\', '\\')))
        libraries.append(root)
    unique = []
    for library in libraries:
        if library.is_dir() and library not in unique:
            unique.append(library)
    return unique


def find_game() -> Path | None:
    candidates = [library / 'steamapps/common/ProjectZomboid' for library in steam_libraries()]
    if os.name == 'nt':
        candidates += [Path('C:/GOG Games/Project Zomboid'), Path('C:/Program Files (x86)/GOG Galaxy/Games/Project Zomboid')]
    for candidate in candidates:
        if (candidate / 'ProjectZomboid64.json').is_file():
            return candidate
    return None


def find_workshop() -> Path | None:
    for library in steam_libraries():
        folder = library / 'steamapps/workshop/content' / APP_ID
        if folder.is_dir():
            return folder
    return None


def check_game(game: Path) -> None:
    if not (game / 'ProjectZomboid64.json').is_file():
        sys.exit(f'{game} is not a Windows Project Zomboid folder (ProjectZomboid64.json missing).')
    if not (game / 'media').is_dir():
        sys.exit(f'{game} has no media folder; verify the game files in Steam first.')
    if not (game / 'jre64/bin/java.exe').is_file():
        print('Warning: jre64 is missing. ProsperoPZ will download Java on the PS5 instead.')


def files(source: Path):
    for directory, folders, names in os.walk(source):
        folders[:] = sorted(f for f in folders if f.lower() not in SKIP_DIRS)
        for name in sorted(names):
            if not SKIP_FILES.match(name):
                path = Path(directory) / name
                yield path, path.relative_to(source)


class FtpTarget:
    def __init__(self, host: str, port: int):
        self.ftp = ftplib.FTP()
        self.ftp.connect(host, port, timeout=30)
        self.ftp.login()
        self.ftp.voidcmd('TYPE I')
        self.made = set()

    def mkdirs(self, path: PurePosixPath) -> None:
        parts = []
        for part in path.parts[1:]:
            parts.append(part)
            current = '/' + '/'.join(parts)
            if current in self.made:
                continue
            try:
                self.ftp.mkd(current)
            except ftplib.error_perm:
                pass
            self.made.add(current)

    def size(self, path: PurePosixPath) -> int | None:
        try:
            return self.ftp.size(str(path))
        except ftplib.all_errors:
            return None

    def put(self, local: Path, remote: PurePosixPath) -> bool:
        length = local.stat().st_size
        if self.size(remote) == length:
            return False
        self.mkdirs(remote.parent)
        partial = remote.with_name(remote.name + '.part')
        with local.open('rb') as handle:
            self.ftp.storbinary(f'STOR {partial}', handle, blocksize=1 << 20)
        if self.size(partial) not in (None, length):
            raise OSError(f'Upload of {local.name} was truncated')
        try:
            self.ftp.delete(str(remote))
        except ftplib.all_errors:
            pass
        self.ftp.rename(str(partial), str(remote))
        return True


class FolderTarget:
    def __init__(self, root: Path):
        self.root = root

    def put(self, local: Path, remote: PurePosixPath) -> bool:
        target = self.root.joinpath(*remote.parts[1:])
        if target.is_file() and target.stat().st_size == local.stat().st_size:
            return False
        target.parent.mkdir(parents=True, exist_ok=True)
        partial = target.with_name(target.name + '.part')
        with local.open('rb') as reader, partial.open('wb') as writer:
            while chunk := reader.read(1 << 20):
                writer.write(chunk)
        partial.replace(target)
        return True


def send(target, source: Path, remote: PurePosixPath, label: str) -> None:
    items = list(files(source))
    total = sum(path.stat().st_size for path, _ in items)
    done = sent = 0
    print(f'{label}: {len(items)} files, {total / 1e9:.2f} GB -> {remote}')
    for path, relative in items:
        if target.put(path, remote.joinpath(*relative.parts)):
            sent += 1
        done += path.stat().st_size
        print(f'\r  {done * 100 // max(total, 1):3d}%  {relative.as_posix()[:70]:<70}', end='', flush=True)
    print(f'\r  done: {sent} copied, {len(items) - sent} already present' + ' ' * 60)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    where = parser.add_mutually_exclusive_group(required=True)
    where.add_argument('--ftp', metavar='PS5_IP', help='PS5 address with an FTP server running')
    where.add_argument('--dest', type=Path, help='Folder or USB drive to copy to (creates ProsperoPZ/...)')
    parser.add_argument('--port', type=int, default=2121, help='FTP port (default 2121)')
    parser.add_argument('--game', type=Path, help='Project Zomboid folder (default: find Steam/GOG install)')
    parser.add_argument('--mods', action='store_true', help='Also copy downloaded Steam Workshop mods')
    parser.add_argument('--saves', action='store_true', help='Also copy your Zomboid/Saves worlds')
    parser.add_argument('--skip-game', action='store_true', help='Only copy mods and/or saves')
    args = parser.parse_args()

    if args.ftp:
        target, base = FtpTarget(args.ftp, args.port), PurePosixPath(DATA_ROOT)
    else:
        # On a USB drive the game folder sits at the top so the PS5 finds it as /mnt/usbN/ProjectZomboid.
        target, base = FolderTarget(args.dest), PurePosixPath('/')
    if not args.skip_game:
        game = args.game or find_game()
        if not game:
            sys.exit('Project Zomboid was not found. Pass --game "PATH" (Steam > Manage > Browse local files).')
        check_game(game)
        send(target, game, base / ('imports/ProjectZomboid' if args.ftp else 'ProjectZomboid'), 'Game')
    if args.mods:
        workshop = find_workshop()
        if not workshop:
            print('No Steam Workshop downloads found for Project Zomboid; skipping mods.')
        else:
            for item in sorted(p for p in workshop.iterdir() if p.is_dir()):
                send(target, item, base / ('imports/mods' if args.ftp else 'ProsperoPZ/imports/mods') / item.name,
                     f'Workshop item {item.name}')
    if args.saves:
        saves = Path.home() / 'Zomboid/Saves'
        if not saves.is_dir():
            print(f'No saves found at {saves}; skipping.')
        else:
            for mode in sorted(p for p in saves.iterdir() if p.is_dir()):
                for world in sorted(p for p in mode.iterdir() if p.is_dir()):
                    # One import item per world; the inner MODE folder keeps its game mode.
                    item = f'{mode.name}-{world.name}'
                    send(target, world, base / ('imports/saves' if args.ftp else 'ProsperoPZ/imports/saves') /
                         item / mode.name / world.name, f'World {mode.name}/{world.name}')
    if args.dest and (args.mods or args.saves):
        print('Copy the ProsperoPZ/imports folder from the drive to /data/ProsperoPZ/imports on the PS5 for mods and saves.')
    print('Done. On the PS5 open ProsperoPZ > Games > Import Game.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
