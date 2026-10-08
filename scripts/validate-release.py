#!/usr/bin/env python3
"""Reject mismatched product/tag versions or incomplete release assets."""
import argparse
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    'linux': ('standalone', 'vst3', 'lv2', 'clap'),
    'macos': ('standalone', 'vst3', 'au', 'lv2', 'clap'),
    'windows': ('standalone', 'vst3', 'clap'),
}


def product_version():
    cmake = (ROOT / 'CMakeLists.txt').read_text()
    if 'VERSION ${PROJECT_VERSION}' not in cmake:
        raise ValueError('plugin version must use PROJECT_VERSION')
    return re.search(r'project\(CheapSynth01 VERSION (\d+\.\d+\.\d+)', cmake).group(1)


def validate(tag, assets=None, platform=None):
    version = product_version()
    if tag.removeprefix('v') != version:
        raise ValueError(f'tag {tag!r} differs from product version {version}')
    if assets:
        platforms = {platform: EXPECTED[platform]} if platform else EXPECTED
        expected = {f'cheapsynth01-{os}-{fmt}.zip'
                    for os, formats in platforms.items() for fmt in formats}
        expected.add('cheapsynth01-source.zip')
        actual = {p.name for p in assets.glob('*.zip')}
        if expected != actual:
            raise ValueError(f'missing assets: {sorted(expected-actual)}; '
                             f'unexpected: {sorted(actual-expected)}')
        source_commits = set()
        for name in sorted(expected):
            with zipfile.ZipFile(assets / name) as archive:
                files = [entry for entry in archive.infolist()
                         if not entry.is_dir() and entry.file_size > 0]
                if not files:
                    raise ValueError(f'empty package: {name}')
                if archive.testzip():
                    raise ValueError(f'corrupt package: {name}')
                names = [entry.filename for entry in files]
                if name == 'cheapsynth01-source.zip':
                    payload = any(path.endswith('.cpp') for path in names)
                elif name.endswith('-standalone.zip'):
                    payload = any(Path(path).name in ('CheapSynth01', 'CheapSynth01.exe')
                                  for path in names)
                else:
                    fmt = name.rsplit('-', 1)[1].removesuffix('.zip')
                    extension = 'component' if fmt == 'au' else fmt
                    payload = any('.' + extension in path for path in names)
                if not payload:
                    raise ValueError(f'package lacks product/source payload: {name}')
                for notice in ('LICENSE.txt', 'ThirdPartyNotices.txt', 'SOURCE.txt',
                               'AGPL-3.0.txt', 'BUILD-INFO.json'):
                    if not any(Path(path).name == notice for path in names):
                        raise ValueError(f'{name} lacks {notice}')
                manifest_name = next(path for path in names
                                     if Path(path).name == 'BUILD-INFO.json')
                manifest = json.loads(archive.read(manifest_name))
                if manifest.get('product_version') != version:
                    raise ValueError(f'{name} manifest product version differs from {version}')
                commit = manifest.get('source_commit', '')
                if not re.fullmatch('[0-9a-f]{40}', commit):
                    raise ValueError(f'{name} lacks an exact source commit')
                source_commits.add(commit)
        if len(source_commits) != 1:
            raise ValueError('binary/source packages refer to different source commits')
    print(f'version {version}: tag and supplied assets validated')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tag', required=True)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--platform', choices=EXPECTED)
    args = parser.parse_args()
    try:
        validate(args.tag, args.assets, args.platform)
    except (ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f'{error}\n')
