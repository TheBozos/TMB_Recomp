"""Player-facing migration and timing regression checks; no game data required."""
import configparser
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import check_timing
import make_release
import tmb_build
from unittest.mock import patch

REQUIRED = (
    'TMB Launcher.exe', 'README.txt', 'CHANGELOG.md', 'LICENSES/TMB_Recomp/LICENSE',
    'LICENSES/BozoEngine/LICENSE', 'source/release.ini', 'kit/kit_manifest.json',
    'kit/python/python.exe', 'source/tools/tmb_build.py', 'source/BozoEngine/CMakeLists.txt',
    'kit/deps/sdl3-src/CMakeLists.txt', 'kit/deps/imgui-src/imgui.cpp',
)


class ReleaseTests(unittest.TestCase):
    def test_source_cleanup_cannot_escape_the_installation(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'source'
            root.mkdir()
            outside = Path(folder) / 'outside'
            outside.mkdir()
            (outside / 'keep.txt').write_bytes(b'outside source')
            (root / 'release_files.txt').write_text('../outside/keep.txt\n', encoding='utf-8')
            with patch.object(tmb_build, 'ROOT', str(root)), self.assertRaises(tmb_build.BuildError):
                tmb_build.remove_stale_sources()
            self.assertEqual((outside / 'keep.txt').read_bytes(), b'outside source')

    def test_update_removes_the_retired_source_component_and_keeps_player_data(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'source'
            root.mkdir()
            (root / 'release_files.txt').write_text('tools/kept.py\n', encoding='utf-8')
            for name in ('tools/kept.py', 'tools/removed.py', 'PS2Recomp/runtime.cpp', 'PS2Recomp/__pycache__/old.pyc',
                         'custom/untouched.txt', 'game/save.dat'):
                file = root / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b'preserve or retire by ownership')
            with patch.object(tmb_build, 'ROOT', str(root)):
                tmb_build.remove_stale_sources()
            self.assertFalse((root / 'PS2Recomp').exists())
            self.assertFalse((root / 'tools/removed.py').exists())
            for name in ('tools/kept.py', 'custom/untouched.txt', 'game/save.dat'):
                self.assertEqual((root/name).read_bytes(), b'preserve or retire by ownership')

    def test_update_never_follows_a_source_directory_link(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / 'source'
            external = Path(folder) / 'external'
            root.mkdir()
            external.mkdir()
            (root / 'tools').mkdir()
            (root / 'release_files.txt').write_text('tools/kept.py\n', encoding='utf-8')
            (external / 'keep.txt').write_bytes(b'outside source')
            if sys.platform == 'win32':
                import _winapi
                _winapi.CreateJunction(str(external), str(root / 'PS2Recomp'))
                _winapi.CreateJunction(str(external), str(root / 'tools/linked'))
            else:
                (root / 'PS2Recomp').symlink_to(external, target_is_directory=True)
                (root / 'tools/linked').symlink_to(external, target_is_directory=True)
            with patch.object(tmb_build, 'ROOT', str(root)):
                tmb_build.remove_stale_sources()
            self.assertEqual((external / 'keep.txt').read_bytes(), b'outside source')
            self.assertFalse((root / 'PS2Recomp').exists())
            self.assertTrue((root / 'tools/linked').exists())

    def test_only_native_engine_is_a_submodule(self):
        config = configparser.ConfigParser()
        config.read(ROOT / '.gitmodules')
        self.assertEqual(config.sections(), ['submodule "BozoEngine"'])
        self.assertEqual(config[config.sections()[0]]['path'], 'BozoEngine')

    def test_migration_zip_supports_the_existing_installation_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            stage = Path(folder) / 'stage'
            for name in REQUIRED:
                file = stage / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b'native test fixture\n')
            release = make_release.Release.__new__(make_release.Release)
            release.stage = str(stage)
            release.migration_zip = str(Path(folder) / 'TMB_Recomp-0.4.0.zip')
            release.zip_time = lambda: (2026, 11, 5, 12, 0, 0)
            release.step_migration_zip()
            self.assertTrue(make_release.sha256_matches(release.migration_zip))
            self.assertEqual(make_release.verify_zip(release.migration_zip, top='TMB_Recomp'), [])
            self.assertTrue(make_release.verify_zip(release.migration_zip))
            with zipfile.ZipFile(release.migration_zip) as archive:
                self.assertEqual({n.split('/')[0] for n in archive.namelist()}, {'TMB_Recomp'})
                for name in REQUIRED:
                    self.assertEqual(archive.read('TMB_Recomp/' + name), (stage / name).read_bytes())
            with zipfile.ZipFile(release.migration_zip, 'a') as archive:
                archive.writestr('TMB_Recomp/source/PS2Recomp/runtime.cpp', b'retired runtime')
            self.assertTrue(any('retired runtime' in error for error in
                                make_release.verify_zip(release.migration_zip, top='TMB_Recomp')))

    def test_release_rejects_game_executables(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'bad.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                for name in REQUIRED:
                    archive.writestr('TMB_Recomp_Bozo/' + name, b'fixture')
                archive.writestr('TMB_Recomp_Bozo/source/hidden.dat', b'\x7fELF' + b'\0' * 100)
            self.assertTrue(any('an ELF file' in error for error in make_release.verify_zip(path)))


class TimingTests(unittest.TestCase):
    LOG = '''[tmb:clock] frame 300: EE 5.000 s, PC 5.000 s, debt 0.0 ms; ticks 2880000 fields 300
[tmb:clock] frame 600: EE 10.000 s, PC 10.000 s, debt 0.0 ms; ticks 5760000 fields 600
[mpeg:clock] picture 0 at 0.000 s
[mpeg:clock] picture 30 at 1.001 s
[mpeg:clock] picture 60 at 2.002 s
'''

    def test_normal_game_speed(self):
        self.assertTrue(all(line.startswith('PASS:') for line in check_timing.check(self.LOG)))

    def test_fast_menus_fail_even_if_the_picture_rate_is_normal(self):
        log = self.LOG.replace('ticks 5760000 fields 600', 'ticks 8640000 fields 900')
        self.assertTrue(any(line.startswith('FAIL: menu') for line in check_timing.check(log)))

    def test_double_speed_movies_fail_independently(self):
        log = self.LOG.replace('at 2.002 s', 'at 1.5015 s')
        self.assertTrue(any(line.startswith('FAIL: movie') for line in check_timing.check(log)))

    def test_missing_measurements_are_not_a_pass(self):
        self.assertTrue(check_timing.check('')[0].startswith('FAIL:'))


if __name__ == '__main__':
    unittest.main()
