#!/usr/bin/env python3
"""Bundle release source and the pinned fetched/submodule sources, without build outputs."""
import argparse
from pathlib import Path
import subprocess
import zipfile
import json
import hashlib
import re

ROOT = Path(__file__).resolve().parents[1]

def add_tree(archive, directory, prefix):
    for file in sorted(directory.rglob('*')):
        if '.git' in file.relative_to(directory).parts or not file.is_file():
            continue
        archive.write(file, str(Path(prefix) / file.relative_to(directory)))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--juce', type=Path, required=True)
    parser.add_argument('--googletest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=no"]).strip():
        raise RuntimeError("Commit tracked source changes before release packaging")
    files = subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z']).decode().split('\0')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    commit = subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'], text=True).strip()
    with zipfile.ZipFile(args.output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name in files:
            if name and (ROOT / name).is_file():
                archive.write(ROOT / name, name)
        add_tree(archive, ROOT / 'libs/clap-juce-extensions', 'libs/clap-juce-extensions')
        add_tree(archive, args.juce, 'dependencies/juce')
        add_tree(archive, args.googletest, 'dependencies/googletest')
        archive.write(ROOT / 'LICENSE', 'LICENSE.txt')
        archive.write(ROOT / 'ThirdParty/AGPL-3.0.txt', 'AGPL-3.0.txt')
        archive.writestr('SOURCE.txt', f'Source commit: {commit}\n'
            'This archive includes JUCE 9.0.3, GoogleTest v1.14.0 and CLAP submodule sources.\n'
            'Configure with -DFETCHCONTENT_SOURCE_DIR_JUCE=<source>/dependencies/juce and\n'
            '-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<source>/dependencies/googletest.\n'
            'See CONTRIBUTING.md and .github/workflows/ci.yml for build conditions.\n'
            'cmake/JuceGraphRealtimePatch.cmake applies the documented JUCE memory patch.\n')
        archive.writestr('ThirdPartyNotices.txt', 'See JUCE LICENSE.md/SPDX inventory, AGPL-3.0.txt,\n'
            'CLAP dependency licence files and docs/software/Distribution.md.\n')
        hashes = {}
        for entry in archive.infolist():
            digest = hashlib.sha256()
            with archive.open(entry) as member:
                for chunk in iter(lambda: member.read(1024 * 1024), b''):
                    digest.update(chunk)
            hashes[entry.filename] = digest.hexdigest()
        archive.writestr('BUILD-INFO.json', json.dumps({
            'source_commit': commit,
            'product_version': re.search(r'project\(CheapSynth01 VERSION (\d+\.\d+\.\d+)', (ROOT / 'CMakeLists.txt').read_text()).group(1),
            'juce_version': '9.0.3',
            'sha256': hashes,
        }, indent=2) + '\n')
