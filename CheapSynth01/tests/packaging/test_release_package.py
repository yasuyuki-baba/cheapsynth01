"""Archive contracts, independently constructed fixtures; no plugin is loaded."""
import hashlib
import importlib.util
import json
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest
import warnings
import zipfile

MODULE = Path(__file__).resolve().parents[3] / 'scripts/release_package.py'
spec = importlib.util.spec_from_file_location('release_package', MODULE)
pkg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pkg)
COMMIT = '1234567890abcdef1234567890abcdef12345678'
VERSION = '0.4.0'
REGULAR = stat.S_IFREG | 0o644


class ArchiveContractTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'release.zip'
        names = ['LICENSE', 'THIRD_PARTY_NOTICES.md', 'CMakeLists.txt', 'CheapSynth01/config.h',
                 'iPlug2/LICENSE.txt', 'dependencies/tinyxml2/CMakeLists.txt',
                 'dependencies/googletest/CMakeLists.txt']
        self.data = {name: (b'example payload\n', REGULAR) for name in names}
        self.info = {'schema': 1, 'product': 'CheapSynth01', 'version': VERSION,
                     'source_commit': COMMIT, 'platform': 'source',
                     'files': {name: {'sha256': hashlib.sha256(data).hexdigest(), 'mode': mode}
                               for name, (data, mode) in self.data.items()}}

    def write(self, extras=()):
        with warnings.catch_warnings():
            warnings.simplefilter('ignore', UserWarning)
            with zipfile.ZipFile(self.path, 'w') as archive:
                for name, (data, mode) in list(self.data.items()) + list(extras):
                    member = zipfile.ZipInfo(name)
                    member.create_system = 3
                    member.external_attr = mode << 16
                    archive.writestr(member, data)
                archive.writestr('BUILD-INFO.json', json.dumps(self.info))

    def check(self):
        return pkg.verify(self.path, COMMIT, VERSION, 'source')

    def test_complete_archive(self):
        self.write()
        self.assertEqual(self.check()['source_commit'], COMMIT)

    def test_tampered_file_rejected(self):
        self.data['LICENSE'] = (b'modified', REGULAR)
        self.write()
        with self.assertRaisesRegex(ValueError, 'Hash mismatch'):
            self.check()

    def test_missing_and_unlisted_members_rejected(self):
        del self.data['LICENSE']
        self.write()
        with self.assertRaisesRegex(ValueError, 'exactly every'):
            self.check()
        del self.info['files']['LICENSE']
        self.write([('unlisted.txt', (b'extra', REGULAR))])
        with self.assertRaisesRegex(ValueError, 'exactly every'):
            self.check()

    def test_required_license_cannot_be_omitted_from_manifest(self):
        del self.data['LICENSE']
        del self.info['files']['LICENSE']
        self.write()
        with self.assertRaisesRegex(ValueError, 'Missing required'):
            self.check()

    def test_duplicate_members_rejected(self):
        self.write([('LICENSE', self.data['LICENSE'])])
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            self.check()

    def test_absolute_parent_and_windows_paths_rejected(self):
        for name in ('../escape', '/absolute', 'C:/drive', 'out\\file', 'out/../escape'):
            with self.subTest(name=name):
                self.write([(name, (b'bad', REGULAR))])
                with self.assertRaisesRegex(ValueError, 'Unsafe archive path'):
                    self.check()

    def test_wrong_commit_version_platform_and_schema_rejected(self):
        for field, value in [('source_commit', 'a' * 40), ('version', '0.3.0'),
                             ('platform', 'macOS'), ('schema', 2)]:
            original = self.info[field]
            self.info[field] = value
            self.write()
            with self.assertRaisesRegex(ValueError, 'does not match release'):
                self.check()
            self.info[field] = original

    def test_executable_mode_cannot_change(self):
        self.data['LICENSE'] = (self.data['LICENSE'][0], stat.S_IFREG | 0o755)
        self.write()
        with self.assertRaisesRegex(ValueError, 'Unexpected file mode'):
            self.check()

    def test_internal_framework_symlink_is_preserved(self):
        name = 'out/Example.app/Contents/Frameworks/Test.framework/Versions/Current'
        data = b'A'
        mode = stat.S_IFLNK | 0o777
        self.data[name] = (data, mode)
        self.info['files'][name] = {'sha256': hashlib.sha256(data).hexdigest(), 'mode': mode}
        self.write()
        self.check()
        for target in (b'/external', b'../../../../../../../../escape', b'C:\\external'):
            self.data[name] = (target, mode)
            self.info['files'][name]['sha256'] = hashlib.sha256(target).hexdigest()
            self.write()
            with self.assertRaisesRegex(ValueError, 'symlink|Symlink'):
                self.check()

    def test_metadata_cannot_replace_missing_native_products(self):
        self.info['platform'] = 'Windows'
        self.write()
        with self.assertRaisesRegex(ValueError, 'Missing product'):
            pkg.verify(self.path, COMMIT, VERSION, 'Windows')


class LinuxArchiveTest(unittest.TestCase):
    write = ArchiveContractTest.write

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'linux.zip'
        names = ['LICENSE', 'THIRD_PARTY_NOTICES.md', 'notices/iPlug2/LICENSE.txt',
                 'notices/Roboto/LICENSE-Apache-2.0.txt', 'notices/tinyxml2/LICENSE.txt',
                 'notices/googletest/LICENSE', 'out/CheapSynth01.clap',
                 'out/CheapSynth01.vst3/Contents/x86_64-linux/CheapSynth01.so',
                 'out/CheapSynth01.resources/ttf/Roboto-Regular.ttf',
                 'out/CheapSynth01.clap.resources/ttf/Roboto-Regular.ttf',
                 'out/CheapSynth01.vst3/Contents/Resources/ttf/Roboto-Regular.ttf']
        self.data = {name: (b'fixture payload', REGULAR) for name in names}
        self.data['out/CheapSynth01'] = (b'ELF fixture', stat.S_IFREG | 0o755)

    def check(self):
        self.info = {'schema': 1, 'product': 'CheapSynth01', 'version': VERSION,
                     'source_commit': COMMIT, 'platform': 'Linux',
                     'files': {name: {'sha256': hashlib.sha256(data).hexdigest(), 'mode': mode}
                               for name, (data, mode) in self.data.items()}}
        self.write()
        return pkg.verify(self.path, COMMIT, VERSION, 'Linux')

    def test_complete_linux_archive_and_executable_mode(self):
        info = self.check()
        self.assertEqual(info['files']['out/CheapSynth01']['mode'], stat.S_IFREG | 0o755)
        with zipfile.ZipFile(self.path) as archive:
            self.assertEqual(archive.getinfo('out/CheapSynth01').external_attr >> 16,
                             stat.S_IFREG | 0o755)
        result = subprocess.run([sys.executable, str(MODULE), 'verify', '--commit', COMMIT,
                                 '--version', VERSION, '--platform', 'Linux', str(self.path)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_each_product_and_resource_is_required(self):
        for name in tuple(self.data):
            if not name.startswith('out/'):
                continue
            with self.subTest(name=name):
                value = self.data.pop(name)
                with self.assertRaisesRegex(ValueError, 'Missing product|Missing required'):
                    self.check()
                self.data[name] = value

    def test_nonexecutable_standalone_rejected_even_with_matching_manifest(self):
        self.data['out/CheapSynth01'] = (b'ELF fixture', REGULAR)
        with self.assertRaisesRegex(ValueError, 'regular executable'):
            self.check()

    def test_arm64_vst3_bundle_is_accepted(self):
        binary = self.data.pop('out/CheapSynth01.vst3/Contents/x86_64-linux/CheapSynth01.so')
        self.data['out/CheapSynth01.vst3/Contents/aarch64-linux/CheapSynth01.so'] = binary
        self.check()

    def test_macos_vst3_bundle_cannot_substitute_for_linux_binary(self):
        binary = self.data.pop('out/CheapSynth01.vst3/Contents/x86_64-linux/CheapSynth01.so')
        self.data['out/CheapSynth01.vst3/Contents/MacOS/CheapSynth01'] = binary
        with self.assertRaisesRegex(ValueError, 'Linux VST3 shared library'):
            self.check()


class RestoredNoticeTest(unittest.TestCase):
    def test_only_pinned_notice_text_and_line_endings_are_accepted(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            name = 'Dependencies/IPlug/CLAP_HELPERS/readme.txt'
            path = root / name
            path.parent.mkdir(parents=True)
            original = b'Place the pinned SDK here\nLicense remains in its source tree\n'
            path.write_bytes(original)
            for command in (['init'], ['config', 'core.autocrlf', 'false'], ['add', '.'],
                            ['-c', 'user.name=Archive Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-m', 'fixture']):
                subprocess.run(['git', '-C', str(root), *command], check=True,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.assertTrue(pkg.restored_notice_matches(root, name))
            path.write_bytes(original.replace(b'\n', b'\r\n'))
            self.assertTrue(pkg.restored_notice_matches(root, name))
            path.write_bytes(b'Changed notice text\r\n')
            self.assertFalse(pkg.restored_notice_matches(root, name))
            self.assertFalse(pkg.restored_notice_matches(root, 'Dependencies/IPlug/CLAP_HELPERS/LICENSE'))
            self.assertFalse(pkg.restored_notice_matches(root, '../readme.txt'))


if __name__ == '__main__':
    unittest.main()
