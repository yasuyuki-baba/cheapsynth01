#!/usr/bin/env python3
"""Instrument project/tests with ASan+UBSan, reusing uninstrumented JUCE/gtest.

This is a limited project diagnostic, not a fully instrumented framework build.
First build the normal Debug CheapSynth01Tests target using Ninja. Run this
script in the same compiler/runtime environment. It does not execute tests.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--output', type=Path, default=Path('build-sanitized'))
parser.add_argument('--jobs', type=int, default=2)
parser.add_argument('--incremental', action='store_true')
options = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = options.build.resolve()
output = options.output.resolve()
output.mkdir(parents=True, exist_ok=True)
commands = json.loads((build / 'compile_commands.json').read_text())
flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-O1']
headers = max(path.stat().st_mtime for folder in (root / 'Source', root / 'Tests')
              for path in folder.rglob('*.h'))
project = []
for command in commands:
    args = shlex.split(command['command'])
    old = args[args.index('-o') + 1]
    if (old.startswith('Tests/CMakeFiles/CheapSynth01Tests.dir/') and
            Path(command['file']).is_relative_to(root) and
            not command['file'].endswith('RealtimeAudit.cpp')):
        project.append(command)


def compile_source(command):
    args = shlex.split(command['command'])
    args = [arg for arg in args if arg != '-DCHEAPSYNTH_RT_AUDIT=1']
    old = args[args.index('-o') + 1]
    new = output / (Path(command['file']).stem + '.o')
    fresh = (new.exists() and new.stat().st_mtime >=
             max(headers, Path(command['file']).stat().st_mtime))
    if not (options.incremental and fresh):
        args[args.index('-o') + 1] = str(new)
        if '-MF' in args:
            args[args.index('-MF') + 1] = str(new) + '.d'
        if '-MT' in args:
            args[args.index('-MT') + 1] = str(new)
        subprocess.run(args + flags, cwd=command['directory'], check=True)
    return old, str(new)


lines = subprocess.check_output(['ninja', '-t', 'commands', 'CheapSynth01Tests'],
                                cwd=build, text=True).splitlines()
link = next(line for line in reversed(lines)
            if '-o Tests/CheapSynth01Tests_artefacts/' in line)
args = shlex.split(link)
start = next(i for i, arg in enumerate(args) if arg.endswith(('/c++', '/g++')))
end = args.index('&&', start) if '&&' in args[start:] else len(args)
args = args[start:end]
args = [arg for arg in args if not arg.startswith('-Wl,--wrap=') and
        not arg.endswith('RealtimeAudit.cpp.o')]
args[args.index('-o') + 1] = str(output / 'audit-sanitized')
with ThreadPoolExecutor(max_workers=options.jobs) as executor:
    for old, new in executor.map(compile_source, project):
        args = [new if arg == old else arg for arg in args]
subprocess.run(args + flags, cwd=build, check=True)
(output / 'instrumentation.json').write_text(json.dumps({
    'instrumented_sources': [command['file'] for command in project],
    'flags': flags,
    'not_instrumented': ['JUCE modules', 'GoogleTest', 'system shared libraries'],
    'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                             cwd=root, text=True).strip(),
    'tracked_source_dirty': bool(subprocess.check_output(
        ['git', 'status', '--porcelain', '--untracked-files=no'], cwd=root)),
}, indent=2) + '\n')
print(f'Instrumented {len(project)} project/test sources: {output / "audit-sanitized"}')
