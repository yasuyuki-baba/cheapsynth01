#!/usr/bin/env python3
"""Build and verify iPlug2 release archives without loading a plugin."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import zipfile

MANIFEST = 'BUILD-INFO.json'


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args]).decode().strip()


def version(root):
    config = (root / 'CheapSynth01/config.h').read_text()
    value = re.search(r'#define PLUG_VERSION_STR "([0-9]+\.[0-9]+\.[0-9]+)"', config).group(1)
    numbers = list(map(int, value.split('.')))
    hex_value = int(re.search(r'#define PLUG_VERSION_HEX (0x[0-9a-fA-F]+)', config).group(1), 16)
    if hex_value != (numbers[0] << 16 | numbers[1] << 8 | numbers[2]):
        raise ValueError('config.h version string and hex differ')
    cmake = (root / 'CMakeLists.txt').read_text()
    if not re.search(r'project\(CheapSynth01 VERSION ' + re.escape(value) + r'\b', cmake):
        raise ValueError('CMake and plugin versions differ')
    return value


def safe_name(name):
    parts = PurePosixPath(name).parts
    if not name or '\\' in name or ':' in name or name.startswith('/') or any(p in ('.', '..') for p in name.split('/')):
        raise ValueError(f'Unsafe archive path: {name}')
    return parts


def safe_link(name, target):
    if not target or target.startswith('/') or '\\' in target or ':' in target:
        raise ValueError(f'Unsafe symlink: {name}')
    depth = len(PurePosixPath(name).parts) - 1
    for part in target.split('/'):
        if part == '..':
            depth -= 1
            if depth < 0:
                raise ValueError(f'Symlink escapes archive: {name}')
        elif part not in ('', '.'):
            depth += 1


def tree_files(root):
    for path in sorted(root.rglob('*')):
        if '.git' not in path.relative_to(root).parts and (path.is_file() or path.is_symlink()):
            yield path


def add_file(files, name, path):
    safe_name(name)
    if name == MANIFEST or name in files:
        raise ValueError(f'Duplicate archive member: {name}')
    if path.is_symlink():
        target = os.readlink(path)
        safe_link(name, target)
        data = target.encode()
        mode = stat.S_IFLNK | 0o777
    else:
        data = path.read_bytes()
        mode = stat.S_IFREG | (0o755 if path.stat().st_mode & 0o111 else 0o644)
    files[name] = (data, mode)


def dependency_revisions(root, build):
    cache = (build / 'CMakeCache.txt').read_text()
    result = {}
    for name in ('tinyxml2', 'googletest'):
        override = re.search(r'^FETCHCONTENT_SOURCE_DIR_' + name.upper() + r':PATH=(.+)$', cache, re.M)
        result[name] = Path(override.group(1)) if override else build / '_deps' / (name + '-src')
        if not (result[name] / 'CMakeLists.txt').is_file():
            raise ValueError(f'Missing fetched source: {result[name]}')
    return result


def package(root, build, platform, output):
    if git(root, 'status', '--porcelain', '--untracked-files=no'):
        raise ValueError('Commit tracked source changes before packaging')
    sdk = root / 'iPlug2'
    if git(sdk, 'status', '--porcelain', '--untracked-files=no'):
        raise ValueError('iPlug2 checkout has tracked modifications')
    dependencies = dependency_revisions(root, build)
    commits = {'iPlug2': git(sdk, 'rev-parse', 'HEAD')}
    if git(root, 'ls-tree', 'HEAD', 'iPlug2').split()[2] != commits['iPlug2']:
        raise ValueError('iPlug2 checkout differs from committed pin')
    for name in ('VST3_SDK', 'CLAP_SDK', 'CLAP_HELPERS'):
        directory = sdk / 'Dependencies/IPlug' / name
        dirty = git(directory, 'status', '--porcelain', '--untracked-files=no')
        # setup-iplug2.sh deliberately restores the outer SDK's placeholder notice.
        for line in dirty.splitlines():
            notice = line[3:]
            # git() strips the leading space on the first porcelain line.
            if line.startswith('M '):
                notice = line[2:]
            if notice not in ('README.md', 'readme.txt'):
                raise ValueError(f'{name} has tracked modifications: {notice}')
            expected = subprocess.check_output(['git', '-C', str(sdk), 'show',
                                                f'HEAD:Dependencies/IPlug/{name}/{notice}'])
            if (directory / notice).read_bytes() != expected:
                raise ValueError(f'{name} has a modified placeholder notice')
        if any(line.startswith(('+', 'U')) for line in git(directory, 'submodule', 'status', '--recursive').splitlines()):
            raise ValueError(f'{name} submodules differ from pinned revisions')
        commits[name] = git(directory, 'rev-parse', 'HEAD')
    for name, path in dependencies.items():
        if git(path, 'status', '--porcelain', '--untracked-files=no'):
            raise ValueError(f'{name} has tracked modifications')
        commits[name] = git(path, 'rev-parse', 'HEAD')
    files = {}
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
        add_file(files, name, root / name)
    if platform == 'source':
        for raw in subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).split(b'\0'):
            if raw:
                name = raw.decode()
                path = root / name
                if path.is_file() or path.is_symlink():
                    if name not in files:
                        add_file(files, name, path)
        for path in tree_files(sdk):
            add_file(files, 'iPlug2/' + path.relative_to(sdk).as_posix(), path)
        for name, directory in dependencies.items():
            for path in tree_files(directory):
                add_file(files, 'dependencies/' + name + '/' + path.relative_to(directory).as_posix(), path)
    else:
        product = build / 'out'
        if not product.is_dir():
            raise ValueError('Native output directory missing')
        for path in tree_files(product):
            add_file(files, 'out/' + path.relative_to(product).as_posix(), path)
        # Include SDK notices beside the binaries, including nested component licenses.
        for path in tree_files(sdk):
            if path.name.lower().startswith(('license', 'copying', 'copyright')):
                add_file(files, 'notices/iPlug2/' + path.relative_to(sdk).as_posix(), path)
        # These components carry their license in the source header itself.
        embedded = ['WDL/heapbuf.h', 'Dependencies/IPlug/RTMidi/RtMidi.h',
                    'Dependencies/IGraphics/glad_GL2/include/KHR/khrplatform.h']
        embedded += [p.relative_to(sdk).as_posix() for p in (sdk / 'Dependencies/IGraphics/STB').glob('*.h')]
        for name in embedded:
            add_file(files, 'notices/iPlug2/' + name, sdk / name)
        for name, directory in dependencies.items():
            for path in tree_files(directory):
                if path.name.lower().startswith(('license', 'copying', 'copyright')):
                    add_file(files, 'notices/' + name + '/' + path.relative_to(directory).as_posix(), path)
        add_file(files, 'notices/Roboto/LICENSE-Apache-2.0.txt', root / 'CheapSynth01/resources/fonts/LICENSE-Apache-2.0.txt')
    info = {'schema': 1, 'product': 'CheapSynth01', 'version': version(root),
            'source_commit': git(root, 'rev-parse', 'HEAD'), 'platform': platform,
            'dependencies': commits, 'signing': 'not performed by this packager',
            'files': {name: {'sha256': hashlib.sha256(data).hexdigest(), 'mode': mode}
                      for name, (data, mode) in sorted(files.items())}}
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, (data, mode) in sorted(files.items()):
            member = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            member.create_system = 3
            member.external_attr = mode << 16
            member.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(member, data)
        archive.writestr(MANIFEST, json.dumps(info, indent=2, sort_keys=True) + '\n')
    verify(output, info['source_commit'], info['version'], platform)
    return info


def verify(path, commit, expected_version, platform):
    with zipfile.ZipFile(path) as archive:
        members = archive.infolist()
        names = [member.filename for member in members]
        for name in names:
            safe_name(name)
        if len(names) != len(set(names)) or any(member.is_dir() for member in members):
            raise ValueError('Duplicate or directory archive members')
        info = json.loads(archive.read(MANIFEST))
        if (info.get('schema'), info.get('product'), info.get('source_commit'), info.get('version'), info.get('platform')) != (1, 'CheapSynth01', commit, expected_version, platform):
            raise ValueError('Package identity/version/source does not match release')
        if not re.fullmatch(r'[0-9a-f]{40}', commit):
            raise ValueError('Source commit must be a full SHA')
        if set(info['files']) != set(names) - {MANIFEST}:
            raise ValueError('Manifest does not cover exactly every payload file')
        for member in members:
            if member.filename == MANIFEST:
                continue
            data = archive.read(member)
            record = info['files'][member.filename]
            if hashlib.sha256(data).hexdigest() != record['sha256']:
                raise ValueError(f'Hash mismatch: {member.filename}')
            mode = member.external_attr >> 16
            if mode != record['mode'] or stat.S_IFMT(mode) not in (stat.S_IFREG, stat.S_IFLNK):
                raise ValueError(f'Unexpected file mode: {member.filename}')
            if stat.S_ISLNK(mode):
                safe_link(member.filename, data.decode())
        required = {'LICENSE', 'THIRD_PARTY_NOTICES.md'}
        if platform == 'source':
            required |= {'CMakeLists.txt', 'CheapSynth01/config.h', 'iPlug2/LICENSE.txt',
                         'dependencies/tinyxml2/CMakeLists.txt', 'dependencies/googletest/CMakeLists.txt'}
        else:
            required |= {'notices/iPlug2/LICENSE.txt', 'notices/Roboto/LICENSE-Apache-2.0.txt',
                         'notices/tinyxml2/LICENSE.txt', 'notices/googletest/LICENSE'}
            suffixes = ('.vst3/', '.clap', '.exe') if platform == 'Windows' else ('.vst3/', '.clap/', '.component/', '.app/')
            for suffix in suffixes:
                if not any(name.startswith('out/') and (suffix in name if suffix.endswith('/') else name.endswith(suffix)) for name in names):
                    raise ValueError(f'Missing product: {suffix}')
        if not required <= set(names):
            raise ValueError('Missing required source/license files: ' + str(required - set(names)))
        return info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    create = sub.add_parser('create')
    create.add_argument('--root', type=Path, default=Path(__file__).resolve().parent.parent)
    create.add_argument('--build', type=Path, required=True)
    create.add_argument('--platform', choices=['Windows', 'macOS', 'source'], required=True)
    create.add_argument('--output', type=Path, required=True)
    check = sub.add_parser('verify')
    check.add_argument('--commit', required=True)
    check.add_argument('--version', required=True)
    check.add_argument('--platform', choices=['Windows', 'macOS', 'source'], required=True)
    check.add_argument('archive', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'create':
            info = package(args.root.resolve(), args.build.resolve(), args.platform, args.output)
        else:
            info = verify(args.archive, args.commit, args.version, args.platform)
        print(f"Verified {info['platform']} {info['version']} at {info['source_commit']}: {len(info['files'])} files")
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
