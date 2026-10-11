#!/usr/bin/env python3
"""Linux diagnostic: recompile baseline production code and a subset of audit tests.

Run only after building the current Debug tests. No baseline files are edited.
The subset contains behavioral tests, not saving or directory override APIs.
The reused JUCE object must be the *unpatched* 9.0.3 module. Pass its saved path.
"""
from pathlib import Path
import json, shlex, subprocess
import argparse
parser = argparse.ArgumentParser()
parser.add_argument('--baseline', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
parser.add_argument('--juce-object', type=Path, required=True)
parser.add_argument('--link-command', type=Path, required=True)
options = parser.parse_args()
base = options.baseline.resolve()
work = options.work.resolve()
s=(work/'Tests/unit/AuditStabilityTest.cpp').read_text()
s=s[:s.index('TEST(AuditPresetTest')]
s=s.replace('render(p, 0, &off);','render(p, 1, &off);')
probe=Path('/tmp/BaselineAuditTest.cpp');probe.write_text(s)
commands=json.loads((work/'build/compile_commands.json').read_text())
command=next(c for c in commands if c['file'].endswith('Tests/unit/NoiseGeneratorTest.cpp'))
args=shlex.split(command['command'])
args=[a.replace(str(work / 'Tests/../Source'), str(base / 'Source')) for a in args]
args[args.index('-o')+1]='/tmp/BaselineAuditTest.o'
if '-MF' in args: args[args.index('-MF')+1]='/tmp/BaselineAuditTest.o.d'
if '-MT' in args: args[args.index('-MT')+1]='/tmp/BaselineAuditTest.o'
args[-1]=str(probe)
subprocess.run(args,cwd=command['directory'],check=True)
link=options.link_command.read_text()
args=shlex.split(link)
start=next(i for i,a in enumerate(args) if a.endswith('/c++') or a.endswith('/g++'))
end=args.index('&&',start) if '&&' in args[start:] else len(args)
args=args[start:end]
args[args.index('-o')+1]=str(work/'build/baseline-audit')
args=[('/tmp/BaselineAuditTest.o' if a.endswith('unit/AuditStabilityTest.cpp.o') else a) for a in args]
args=[a for a in args if not any('/'+part+'/' in a and a.endswith('.o') for part in ['unit','integration','reference'])]
args=[str(options.juce_object) if a.endswith('juce_audio_processors_headless.cpp.o') else a for a in args]
# Cached product objects may predate HEAD. Recompile every production source
# against the unchanged baseline checkout; reuse only pinned JUCE/GoogleTest objects.
from concurrent.futures import ThreadPoolExecutor
objects=Path('/tmp/baseline-audit-objects'); objects.mkdir(exist_ok=True)
production=[c for c in commands if '/Source/' in c['file'] and c['file'].endswith('.cpp') and '/Tests/' in c['command']]
def compile_source(c):
    compile_args=shlex.split(c['command'])
    compile_args=[a.replace(str(work / 'Tests/../Source'), str(base / 'Source')).replace(str(work / 'Source') + '/', str(base / 'Source') + '/') for a in compile_args]
    old=compile_args[compile_args.index('-o')+1]
    new=str(objects / (Path(c['file']).stem+'.o'))
    compile_args[compile_args.index('-o')+1]=new
    if '-MF' in compile_args: compile_args[compile_args.index('-MF')+1]=new+'.d'
    if '-MT' in compile_args: compile_args[compile_args.index('-MT')+1]=new
    subprocess.run(compile_args,cwd=c['directory'],check=True)
    return old,new
with ThreadPoolExecutor(max_workers=3) as executor:
    for old,new in executor.map(compile_source,production):
        args=[new if a==old else a for a in args]
subprocess.run(args,cwd=work/'build',check=True)
