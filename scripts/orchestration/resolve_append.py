"""Resolve jsonc conflicts where both sides append entries to the same list or object.

Usage: python resolve_append.py <path>
For every conflict hunk, keep HEAD's block then THEIRS' block, trying the seam repairs in
order until the whole file parses as JSON (comments stripped): (a) as is, (b) HEAD's last
line given a trailing comma, (c) HEAD's last entry closed with "  ]," (the case where HEAD
ended mid-list because the common tail used to close it), (d) both. Writes only when a
candidate parses; exits 1 otherwise, so a caller must never stage on failure. CRLF kept.
"""
import json
import re
import sys

p = sys.argv[1]
s = open(p, 'rb').read().decode('utf-8')


def parses(text):
    t = re.sub(r'//[^\n]*', '', text.replace('\r\n', '\n'))
    t = re.sub(r',(\s*[}\]])', r'\1', t)
    try:
        json.loads(t)
        return True
    except json.JSONDecodeError:
        return False


n = 0
while '<<<<<<< ' in s:
    i = s.index('<<<<<<< ')
    m = s.index('\n=======', i)
    j = s.index('>>>>>>> ', m)
    j = s.index('\n', j) + 1
    head = s[s.index('\n', i) + 1:m + 1]
    theirs = s[s.index('\n', m + 1) + 1:s.index('>>>>>>> ', m)]
    hl = head.rstrip('\r\n')
    variants = [
        hl + '\r\n' + theirs,
        (hl if hl.rstrip().endswith((',', '[', '{')) else hl + ',') + '\r\n' + theirs,
        hl.rstrip(',') + '\r\n  ],\r\n' + theirs,
        hl.rstrip(',') + ',\r\n  ],\r\n' + theirs,
    ]
    chosen = None
    for v in variants:
        cand = s[:i] + v + s[j:]
        # a later hunk may still be unresolved; strip its markers for the trial parse
        trial = re.sub(r'^<<<<<<< [^\n]*\n', '', cand, flags=re.M)
        trial = re.sub(r'^=======\n', '', trial, flags=re.M)
        trial = re.sub(r'^>>>>>>> [^\n]*\n', '', trial, flags=re.M)
        if parses(trial):
            chosen = cand
            break
    if chosen is None:
        print(f'UNRESOLVED hunk at char {i}: no seam variant parses; file left untouched')
        sys.exit(1)
    s = chosen
    n += 1

if not parses(s):
    print('resolved hunks but the file still does not parse; not written')
    sys.exit(1)
open(p, 'wb').write(s.encode('utf-8'))
print(f'resolved {n} hunk(s), json ok')
