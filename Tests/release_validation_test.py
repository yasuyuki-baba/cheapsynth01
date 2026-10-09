"""Check release controls with synthetic archives; no release is published."""
import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_check', ROOT / 'scripts/validate-release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
NOTICES = ('LICENSE.txt', 'ThirdPartyNotices.txt', 'SOURCE.txt', 'AGPL-3.0.txt', 'BUILD-INFO.json')


def fixture_assets(folder, platform=None, bad_manifest=None):
    assets = Path(folder)
    platforms = {platform: release.EXPECTED[platform]} if platform else release.EXPECTED
    packages = {f'cheapsynth01-{os}-{fmt}.zip':
                ('CheapSynth01' if fmt == 'standalone' else
                 f'CheapSynth01.{"component" if fmt == "au" else fmt}/binary')
                for os, formats in platforms.items() for fmt in formats}
    packages['cheapsynth01-source.zip'] = 'Source/main.cpp'
    for package, payload in packages.items():
        contents = {payload: b'synthetic payload, not a plugin'}
        contents['extra-notice.txt'] = b'dependency notice'
        contents.update({name: b'fixture data' for name in NOTICES if name != 'BUILD-INFO.json'})
        manifest = {'source_commit': '1' * 40, 'product_version': '0.3.0',
                    'sha256': {name: hashlib.sha256(data).hexdigest()
                               for name, data in contents.items()}}
        if bad_manifest and package == 'cheapsynth01-source.zip':
            manifest.update(bad_manifest)
        with zipfile.ZipFile(assets / package, 'w') as archive:
            for name, data in contents.items():
                archive.writestr(name, data)
            archive.writestr('BUILD-INFO.json', json.dumps(manifest))
    return assets


def rewrite(path, transform):
    with zipfile.ZipFile(path) as archive:
        entries = [(entry.filename, archive.read(entry)) for entry in archive.infolist()]
    with zipfile.ZipFile(path, 'w') as archive:
        for name, data in transform(entries):
            archive.writestr(name, data)


class ReleaseValidationTest(unittest.TestCase):
    def test_tag_version_matches(self):
        release.validate('v0.3.0')

    def test_mismatch_is_rejected(self):
        with self.assertRaises(ValueError):
            release.validate('v9.9.9')

    def test_missing_windows_clap_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder)
            (assets / 'cheapsynth01-windows-clap.zip').unlink()
            with self.assertRaises(ValueError):
                release.validate('v0.3.0', assets)

    def test_complete_assets_and_notices(self):
        with tempfile.TemporaryDirectory() as folder:
            release.validate('v0.3.0', fixture_assets(folder))

    def test_linux_subset_can_be_verified_locally(self):
        with tempfile.TemporaryDirectory() as folder:
            release.validate('v0.3.0', fixture_assets(folder, 'linux'), 'linux')

    def test_manifest_product_version_must_match(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, bad_manifest={'product_version': '0.2.9'})
            with self.assertRaises(ValueError):
                release.validate('v0.3.0', assets)

    def test_binary_and_source_commits_must_match(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, bad_manifest={'source_commit': '2' * 40})
            with self.assertRaises(ValueError):
                release.validate('v0.3.0', assets)

    def test_payload_without_notices_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder)
            with zipfile.ZipFile(assets / 'cheapsynth01-windows-clap.zip', 'w') as archive:
                archive.writestr('CheapSynth01.clap', 'synthetic payload')
            with self.assertRaises(ValueError):
                release.validate('v0.3.0', assets)

    def test_changed_payload_with_valid_crc_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, 'linux')
            path = assets / 'cheapsynth01-linux-clap.zip'
            rewrite(path, lambda entries: [(name, b'changed payload' if name.endswith('/binary')
                                           else data) for name, data in entries])
            with zipfile.ZipFile(path) as archive:
                self.assertIsNone(archive.testzip())
            with self.assertRaisesRegex(ValueError, 'hash'):
                release.validate('v0.3.0', assets, 'linux')

    def test_missing_declared_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, 'linux')
            rewrite(assets / 'cheapsynth01-linux-clap.zip',
                    lambda entries: [(name, data) for name, data in entries
                                     if name != 'extra-notice.txt'])
            with self.assertRaisesRegex(ValueError, 'hash'):
                release.validate('v0.3.0', assets, 'linux')

    def test_unlisted_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, 'linux')
            rewrite(assets / 'cheapsynth01-linux-clap.zip',
                    lambda entries: entries + [('unexpected.txt', b'extra content')])
            with self.assertRaisesRegex(ValueError, 'hash'):
                release.validate('v0.3.0', assets, 'linux')

    def test_source_requires_complete_hashes(self):
        for hashes in (None, {}, {'Source/main.cpp': '0' * 64}):
            with self.subTest(hashes=hashes), tempfile.TemporaryDirectory() as folder:
                assets = fixture_assets(folder, 'linux', bad_manifest={'sha256': hashes})
                with self.assertRaisesRegex(ValueError, 'hash'):
                    release.validate('v0.3.0', assets, 'linux')

    def test_malformed_hash_is_rejected(self):
        for digest in ('not a digest', '0' * 63, 123):
            with self.subTest(digest=digest), tempfile.TemporaryDirectory() as folder:
                assets = fixture_assets(folder, 'linux')
                def change(entries):
                    manifest = json.loads(dict(entries)['BUILD-INFO.json'])
                    manifest['sha256']['CheapSynth01.clap/binary'] = digest
                    return [(name, json.dumps(manifest) if name == 'BUILD-INFO.json' else data)
                            for name, data in entries]
                rewrite(assets / 'cheapsynth01-linux-clap.zip', change)
                with self.assertRaisesRegex(ValueError, 'hash'):
                    release.validate('v0.3.0', assets, 'linux')

    def test_duplicate_file_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, 'linux')
            with self.assertWarns(UserWarning):
                rewrite(assets / 'cheapsynth01-linux-clap.zip',
                        lambda entries: entries + [entries[0]])
            with self.assertRaisesRegex(ValueError, 'duplicate'):
                release.validate('v0.3.0', assets, 'linux')

    def test_ambiguous_manifests_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, 'linux')
            rewrite(assets / 'cheapsynth01-linux-clap.zip',
                    lambda entries: entries + [('nested/BUILD-INFO.json',
                                                dict(entries)['BUILD-INFO.json'])])
            with self.assertRaisesRegex(ValueError, 'manifest'):
                release.validate('v0.3.0', assets, 'linux')

    def test_noncanonical_member_paths_are_rejected(self):
        for name in ('../outside.txt', '/absolute.txt', 'C:/absolute.txt', 'nested/../outside.txt',
                     'nested\\file.txt', 'nested//file.txt'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as folder:
                assets = fixture_assets(folder, 'linux')
                rewrite(assets / 'cheapsynth01-linux-clap.zip',
                        lambda entries: entries + [(name, b'content')])
                with self.assertRaisesRegex(ValueError, 'path'):
                    release.validate('v0.3.0', assets, 'linux')

    def test_nonobject_manifest_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, 'linux')
            rewrite(assets / 'cheapsynth01-linux-clap.zip',
                    lambda entries: [(name, '[]' if name == 'BUILD-INFO.json' else data)
                                     for name, data in entries])
            with self.assertRaisesRegex(ValueError, 'manifest'):
                release.validate('v0.3.0', assets, 'linux')

    def test_directory_and_empty_file_have_unambiguous_hash_coverage(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, 'linux')
            def change(entries):
                manifest = json.loads(dict(entries)['BUILD-INFO.json'])
                manifest['sha256']['empty.txt'] = hashlib.sha256(b'').hexdigest()
                return [(name, json.dumps(manifest) if name == 'BUILD-INFO.json' else data)
                        for name, data in entries] + [('empty.txt', b''), ('folder/', b'')]
            rewrite(assets / 'cheapsynth01-linux-clap.zip', change)
            release.validate('v0.3.0', assets, 'linux')

    def test_prefixed_product_manifest_is_valid(self):
        with tempfile.TemporaryDirectory() as folder:
            assets = fixture_assets(folder, 'linux')
            rewrite(assets / 'cheapsynth01-linux-clap.zip',
                    lambda entries: [('CLAP/' + name, data) for name, data in entries])
            release.validate('v0.3.0', assets, 'linux')

    def test_manifest_commit_has_exact_string_form(self):
        for commit in (None, 123, 'g' * 40):
            with self.subTest(commit=commit), tempfile.TemporaryDirectory() as folder:
                assets = fixture_assets(folder, 'linux', bad_manifest={'source_commit': commit})
                with self.assertRaisesRegex(ValueError, 'commit'):
                    release.validate('v0.3.0', assets, 'linux')


if __name__ == '__main__':
    unittest.main()
