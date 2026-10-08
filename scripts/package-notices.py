#!/usr/bin/env python3
"""Stage version/source/dependency notices in each product format before zipping."""
import argparse
from pathlib import Path
import shutil
import hashlib
import json
import os
import platform
import subprocess
import re

ROOT = Path(__file__).resolve().parents[1]

def revision(path):
    return subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--products', type=Path, required=True)
    parser.add_argument('--juce', type=Path, required=True)
    args = parser.parse_args()
    if subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=no"]).strip():
        raise RuntimeError("Commit tracked source changes before release packaging")
    source = revision(ROOT)
    source_text = (f'CheapSynth01 corresponding source revision: {source}\n'
                   f'https://github.com/yasuyuki-baba/cheapsynth01/tree/{source}\n'
                   'Clone this revision with git submodule update --init --recursive.\n'
                   'JUCE is pinned to 9.0.3 by cmake/JUCE.cmake.\n'
                   'Use CONTRIBUTING.md and the CI workflow for build instructions.\n'
                   'Distributor must supply exact dependency sources/patches and build environment\n'
                   'for the shipped binaries, and resolve the JUCE licence route before release.\n')
    for product in args.products.iterdir():
        if not product.is_dir() or product.name not in {'Standalone','VST3','AU','LV2','CLAP'}:
            continue
        shutil.copyfile(ROOT / 'LICENSE', product / 'LICENSE.txt')
        shutil.copyfile(ROOT / 'ThirdParty/AGPL-3.0.txt', product / 'AGPL-3.0.txt')
        (product / 'SOURCE.txt').write_text(source_text)
        (product / 'ThirdPartyNotices.txt').write_text(
            'CheapSynth01: GPLv3 (LICENSE.txt).\n'
            'JUCE 9.0.3: AGPLv3 or applicable JUCE commercial licence.\n'
            'See JUCE-LICENSE.md and JUCE.spdx.json for the framework and bundled dependencies.\n'
            'See docs/Distribution.md in corresponding source for the distributor decision.\n'
            'CLAP and clap-helpers / clap-juce-extensions: see attached licence files.\n')
        shutil.copyfile(args.juce / 'LICENSE.md', product / 'JUCE-LICENSE.md')
        shutil.copyfile(args.juce / 'JUCE.spdx.json', product / 'JUCE.spdx.json')
        for notice in args.juce.joinpath('modules').rglob('*'):
            if notice.is_file() and notice.name.upper().startswith(('LICENSE', 'LICENCE', 'COPYING', 'COPYRIGHT')):
                destination = product / 'ThirdPartyLicenses' / notice.relative_to(args.juce)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(notice, destination)
        for label, directory in (
            ('clap-juce-extensions', ROOT / 'libs/clap-juce-extensions'),
            ('clap', ROOT / 'libs/clap-juce-extensions/clap-libs/clap'),
            ('clap-helpers', ROOT / 'libs/clap-juce-extensions/clap-libs/clap-helpers')):
            licenses = sorted(directory.glob('LICENSE*'))
            if not licenses:
                raise RuntimeError(f'missing licence: {directory}')
            for number, license_path in enumerate(licenses):
                shutil.copyfile(license_path, product / f'{label}-LICENSE-{number}.txt')
        hashes = {str(path.relative_to(product)): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in product.rglob('*') if path.is_file() and path.name != 'BUILD-INFO.json'}
        (product / 'BUILD-INFO.json').write_text(json.dumps({
            'source_commit': source,
            'product_version': re.search(r'project\(CheapSynth01 VERSION (\d+\.\d+\.\d+)', (ROOT / 'CMakeLists.txt').read_text()).group(1),
            'juce_commit': revision(args.juce),
            'juce_version': '9.0.3',
            'juce_patch': 'cmake/JuceGraphRealtimePatch.cmake',
            'build_machine': platform.machine(),
            'build_platform': platform.platform(),
            'runner': {key: os.environ.get(key) for key in ('RUNNER_OS', 'RUNNER_ARCH', 'ImageOS', 'ImageVersion')},
            'signing_notarization': 'not performed by this workflow; inspect actual binaries',
            'sha256': hashes,
        }, indent=2) + '\n')
