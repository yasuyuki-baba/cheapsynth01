#!/usr/bin/env python3
"""Verify complete, reviewable closeout of the frozen historical test inventory.

This checks traceability and unresolved rows, not semantic assertion equivalence.
Reviewed differences and deferred native verification remain explicit in the CSV.
"""
import csv
from collections import Counter
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '32b91c8344a0581232afd7e0a79c5d25da22f42a'
DISPOSITIONS = {
    'adapted-core-contract', 'replaced-core-assertions', 'replaced-core-observation',
    'replaced', 'deferred-native-runtime', 'deferred-native-ui',
    'retired-juce-internal', 'retired-test-double', 'retired-legacy-compatibility',
    'retired-linux-probe',
}
PATTERN = re.compile(r'\bTEST(?:_F)?\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)')


def main():
    archive = ROOT / 'Tests/legacy-juce'
    frozen = {}
    for directory in ['unit', 'integration']:
        for source in (archive / directory).glob('*.cpp'):
            text = source.read_text()
            original_path = 'Tests/' + source.relative_to(archive).as_posix()
            for match in PATTERN.finditer(text):
                key = (original_path, *match.groups())
                if key in frozen:
                    raise ValueError(f'Duplicate frozen definition: {key}')
                frozen[key] = text.count('\n', 0, match.start()) + 1
    rows = list(csv.DictReader((ROOT / 'docs/iPlug2-test-inventory.csv').open(newline='')))
    if len(frozen) != 272 or len(rows) != 272:
        raise ValueError('Frozen baseline/inventory must each contain 272 definitions')
    seen = set()
    for row in rows:
        key = (row['source_path'], row['suite'], row['case'])
        if key in seen or key not in frozen:
            raise ValueError(f'Duplicate or missing historical definition: {key}')
        seen.add(key)
        if row['source_commit'] != COMMIT or int(row['source_line']) != frozen[key]:
            raise ValueError(f'Incorrect frozen source provenance: {key}')
        if row['disposition'] not in DISPOSITIONS:
            raise ValueError(f'Unresolved or unknown disposition: {key}: {row["disposition"]}')
        evidence = row['replacement_and_evidence']
        if len(evidence.strip()) < 40:
            raise ValueError(f'Missing specific replacement/retirement/deferment evidence: {key}')
        for path in re.findall(r'(?:Tests|Source|CheapSynth01)/[A-Za-z0-9_./-]+\.(?:cpp|h)', evidence):
            if not (ROOT / path).is_file():
                raise ValueError(f'Missing evidence file: {key}: {path}')
    if seen != set(frozen):
        raise ValueError('Inventory does not cover every frozen definition')
    print('Reviewed all 272 historical definitions; no unresolved disposition.')
    for status, count in sorted(Counter(r['disposition'] for r in rows).items()):
        print(f'  {status}: {count}')
    print('Traceability check only; deferred native rows are not passing runtime tests.')


if __name__ == '__main__':
    main()
