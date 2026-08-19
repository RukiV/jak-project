"""Validate every commit in a range: no conflict markers in the tracked config/source files
and every jsonc config parses as JSON with comments stripped.

Usage: python check_commits.py <repo_dir> <base>..<tip>
Exit 1 on any finding.
"""
import json
import re
import subprocess
import sys

repo, rng = sys.argv[1], sys.argv[2]
FILES = [
    'decompiler/config/jakx/all-types.gc',
    'decompiler/config/jakx/ntsc_v1/type_casts.jsonc',
    'decompiler/config/jakx/ntsc_v1/stack_structures.jsonc',
    'decompiler/config/jakx/ntsc_v1/label_types.jsonc',
    'decompiler/config/jakx/ntsc_v1/hacks.jsonc',
    'decompiler/config/jakx/ntsc_v1/anonymous_function_types.jsonc',
    'test/offline/config/jakx/config.jsonc',
]
commits = subprocess.run(['git', '-C', repo, 'log', '--format=%h', rng], capture_output=True, text=True).stdout.split()
bad = 0
for c in commits:
    for f in FILES:
        r = subprocess.run(['git', '-C', repo, 'show', f'{c}:{f}'], capture_output=True)
        if r.returncode != 0:
            continue
        s = r.stdout.decode('utf-8', errors='replace')
        if re.search(r'^(<<<<<<< |>>>>>>> |=======$)', s, re.M):
            print(f'{c} {f}: CONFLICT MARKERS')
            bad += 1
        if f.endswith('.jsonc'):
            t = re.sub(r'//[^\n]*', '', s.replace('\r\n', '\n'))
            t = re.sub(r',(\s*[}\]])', r'\1', t)
            try:
                json.loads(t)
            except json.JSONDecodeError as e:
                print(f'{c} {f}: JSON ERROR {e}')
                bad += 1
print(f'{len(commits)} commits checked, {bad} finding(s)')
sys.exit(1 if bad else 0)
