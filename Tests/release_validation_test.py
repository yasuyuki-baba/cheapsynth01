"""Check release controls with synthetic archives; no release is published."""
import importlib.util
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
        with zipfile.ZipFile(assets / package, 'w') as archive:
            archive.writestr(payload, 'synthetic payload, not a plugin')
            manifest = {'source_commit': '1' * 40, 'product_version': '0.3.0'}
            if bad_manifest and package == 'cheapsynth01-source.zip':
                manifest.update(bad_manifest)
            for name in NOTICES:
                archive.writestr(name, json.dumps(manifest) if name == 'BUILD-INFO.json'
                                 else 'fixture data')
    return assets


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


if __name__ == '__main__':
    unittest.main()
