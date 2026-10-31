"""The release rebase keeps ProsperoMC's runtime and replaces everything title-specific."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'scripts' / file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


rebase = load('rebase', 'rebase-runtime-release.py')
deploy = load('deploy', 'deploy-ps5.py')


def pe64():
    data = bytearray(512)
    data[0:2] = b'MZ'
    struct.pack_into('<I', data, 60, 64)
    data[64:68] = b'PE\0\0'
    struct.pack_into('<H', data, 68, 0x8664)
    return bytes(data)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class RebaseTests(unittest.TestCase):
    def upstream(self, path):
        ntdll, win32u = b'ntdll module', b'win32u eglGetDisplay eglCreateContext'
        report = {'titleId': 'PPSA99888', 'structurallyComplete': True,
                  'wine': {'ntdllSha256': sha(ntdll), 'win32uSha256': sha(win32u)},
                  'prefix': {'path': 'prefixes/minecraft'}, 'helper': {'elf_sha256': 'x'}}
        files = {
            'install/PPSA99888/eboot.bin': b'old eboot',
            'install/PPSA99888/lapy.elf': b'\x7fELF helper for PPSA99888',
            'install/PPSA99888/lapy-manifest.json': b'{}',
            'install/PPSA99888/sce_sys/param.json': b'{"titleId":"PPSA99888"}',
            'install/PPSA99888/sce_sys/snd0.at9': b'music',
            'install/PPSA99888/launcher/prosperomc-launcher.jar': b'old jar',
            'install/PPSA99888/win/wine/lib/wine/x86_64-unix/ntdll.prx': ntdll,
            'install/PPSA99888/win/wine/lib/wine/x86_64-unix/win32u.prx': win32u,
            'install/PPSA99888/WINE_SOURCES.txt': b'wine sources built for ProsperoMC',
            'install/PPSA99888/WINE_CORE_V020_REPORT.json': b'{"dataFiles":{"prefixes/minecraft/x":"1"}}',
            'install/PPSA99888/STAGE-REPORT.json': json.dumps(report).encode(),
            'install/prosperomc/launcher/prosperomc-launcher.jar': b'old jar',
            'install/prosperomc/launcher/prosperomc-zipfs.jar': b'zipfs',
            'install/prosperomc/runtimes/java8/bin/java.exe': pe64(),
            'install/prosperomc/prefixes/minecraft/system.reg': b'"Path"="Z:\\\\data\\\\ProsperoMC\\\\x"\n',
            'install/prosperomc/prefixes/minecraft/user.reg': b'user\n',
            'install/prosperomc/prefixes/minecraft/prosperomc-prefix.json': b'{"root":"/data/ProsperoMC"}',
            'install/prosperomc/prefixes/minecraft/drive_c/windows/system32/ntdll.dll': pe64(),
            'install/prosperomc/prefixes/minecraft/dosdevices/.pw-symlinks': b'c:\t../drive_c\np:\t/data/ProsperoMC\nz:\t/\n',
            'install/prosperomc/STAGE-REPORT.json': json.dumps(report).encode(),
        }
        with zipfile.ZipFile(path, 'w') as z:
            for name, data in files.items():
                z.writestr(name, data)

    def test_rebased_package_is_valid_and_has_no_minecraft_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            upstream, title, jar = base / 'mc.zip', base / 'PPSA99777', base / 'prosperopz-launcher.jar'
            self.upstream(upstream)
            (title / 'sce_sys').mkdir(parents=True)
            (title / 'sce_module').mkdir()
            (title / 'eboot.bin').write_bytes(b'new eboot')
            (title / 'sce_module/libc.prx').write_bytes(b'libc')
            (title / 'sce_sys/param.json').write_text('{"titleId":"PPSA99777"}')
            jar.write_bytes(b'PK new jar')
            report = rebase.rebase(upstream, title, jar, 'ProsperoMC test')
            data = title / 'prosperopz'
            self.assertEqual((title / 'eboot.bin').read_bytes(), b'new eboot')
            self.assertFalse((title / 'lapy.elf').exists())
            self.assertFalse((title / 'sce_sys/snd0.at9').exists())
            self.assertFalse((data / 'launcher/prosperomc-zipfs.jar').exists())
            self.assertEqual((data / 'launcher/prosperopz-launcher.jar').read_bytes(), b'PK new jar')
            self.assertTrue((data / 'prefixes/zomboid/drive_c/windows/system32/ntdll.dll').is_file())
            self.assertIn(b'ProsperoPZ', (data / 'prefixes/zomboid/system.reg').read_bytes())
            self.assertEqual(report['prefix'], {'path': 'prefixes/zomboid'})
            self.assertIn(b'ProsperoPZ', (data / 'prefixes/zomboid/prosperopz-prefix.json').read_bytes())
            self.assertIsNone(report['helper'])
            self.assertEqual((title / 'WINE_SOURCES.txt').read_bytes(), b'wine sources built for ProsperoMC')
            self.assertIn(b'prefixes/zomboid/x', (title / 'WINE_CORE_V020_REPORT.json').read_bytes())
            original = deploy.TITLE_ID
            self.assertEqual(original, 'PPSA99777')
            deploy.validate_package(title, data)
            for path in title.rglob('*'):
                self.assertNotIn('minecraft', path.as_posix().lower())

    def helper_pair(self, base, title='PPSA99777', protocol_ok=True, feature_ok=True):
        import hashlib, json
        protocol = ROOT / 'vendor/prospero-win/native/lapy_elevation_protocol.h'
        elf = base / 'lapy.elf'
        elf.write_bytes(b'\x7fELF fake helper bytes for ' + title.encode())
        proto_sha = hashlib.sha256(protocol.read_bytes()).hexdigest()
        manifest = {
            'schema': 'lapy-owned-build/1', 'target_title': title, 'mode': 'elf-helper',
            'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
            'protocol_sha256': proto_sha if protocol_ok else '0' * 64,
            'features': ['root_layout_probe_retry'] if feature_ok else [],
        }
        manifest_path = base / 'lapy-manifest.json'
        manifest_path.write_text(json.dumps(manifest))
        return elf, manifest_path

    def test_valid_helper_is_bundled_and_recorded(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            upstream, title, jar = base / 'mc.zip', base / 'PPSA99777', base / 'jar.jar'
            self.upstream(upstream)
            (title / 'sce_sys').mkdir(parents=True); (title / 'sce_module').mkdir()
            (title / 'eboot.bin').write_bytes(b'eboot'); (title / 'sce_module/libc.prx').write_bytes(b'libc')
            (title / 'sce_sys/param.json').write_text('{"titleId":"PPSA99777"}')
            jar.write_bytes(b'PK jar')
            elf, manifest_path = self.helper_pair(base)
            report = rebase.rebase(upstream, title, jar, 'ProsperoMC test', elf, manifest_path)
            self.assertEqual((title / 'lapy.elf').read_bytes(), elf.read_bytes())
            self.assertTrue((title / 'lapy-manifest.json').is_file())
            self.assertEqual(report['helper']['target_title'], 'PPSA99777')
            self.assertEqual(report['helper']['elf_sha256'], __import__('hashlib').sha256(elf.read_bytes()).hexdigest())
            deploy.validate_package(title, title / 'prosperopz')

    def test_helper_for_wrong_title_or_protocol_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            elf, manifest_path = self.helper_pair(base, title='PPSA99888')
            with self.assertRaisesRegex(ValueError, 'target_title'):
                rebase.validate_helper(elf, manifest_path)
            elf2, manifest2 = self.helper_pair(base / 'a' if (base / 'a').mkdir() or True else base, protocol_ok=False)
            with self.assertRaisesRegex(ValueError, 'protocol'):
                rebase.validate_helper(elf2, manifest2)
            elf3, manifest3 = self.helper_pair(base / 'b' if (base / 'b').mkdir() or True else base, feature_ok=False)
            with self.assertRaisesRegex(ValueError, 'root_layout_probe_retry'):
                rebase.validate_helper(elf3, manifest3)

    def test_wrong_title_build_is_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            self.upstream(base / 'mc.zip')
            with self.assertRaisesRegex(ValueError, 'Build the native title'):
                rebase.rebase(base / 'mc.zip', base / 'missing', base / 'jar', 'x')


if __name__ == '__main__':
    unittest.main()
