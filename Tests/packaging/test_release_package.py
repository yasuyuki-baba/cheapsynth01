"""Archive contracts, independently constructed fixtures; no plugin is loaded."""
import hashlib
import importlib.util
import json
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest
import warnings
import zipfile

MODULE = Path(__file__).resolve().parents[2] / 'scripts/release_package.py'
spec = importlib.util.spec_from_file_location('release_package', MODULE)
pkg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pkg)
COMMIT = '1234567890abcdef1234567890abcdef12345678'
VERSION = '2.0.0'
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
