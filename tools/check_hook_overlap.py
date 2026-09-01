#!/usr/bin/env python3
"""
Fail the build if any of this project's hooks OVERLAPS a hook from another injected DLL.

Why this exists
---------------
Syringe writes every hook as an `E9 rel32` patch over the bytes at its address.

  * Two hooks at the SAME address chain legally — Syringe runs both handlers.
  * Two hooks at DIFFERENT addresses whose byte ranges INTERSECT corrupt each
    other. The second patch written lands in the middle of the first one's jump
    instruction, so the first DLL's jump now points at whatever the overlapping
    bytes happen to encode. Execution leaves the program.

That is not hypothetical. IntelExt hooked 0x6FE354+6 while Phobos hooks
0x6FE352+8 — two bytes inside it. Both hooks are individually
instruction-aligned, so every "is this a valid hook site?" check passes. The
result in game was Phobos' jump landing in heap data, `pop ebp; pop esp` on
garbage, a destroyed stack, and an access violation at 0x00005280 with no
module to blame.

Only a range-intersection check catches this, so here it is.

Usage
-----
    check_hook_overlap.py <registry-hooks.csv> [src-dir]

The CSV is the YR Hook Encyclopedia's registry/hooks.csv:
    Address,Framework,Channel,Function,StolenBytes,Subsystem,SourceFile
"""
import csv
import re
import sys
from pathlib import Path

# Accepts both Syringe macro conventions: `DEFINE_HOOK(0x4C9EA0, n, 0x5)` and the
# SYR_VER==2 token-pasting form `DEFINE_HOOK(48EB12, n, 6)`, where the macro
# prepends 0x to BOTH arguments (so a bare size is hex too).
HOOK_RE = re.compile(
    r'\bDEFINE_HOOK(?:_AGAIN)?\s*\(\s*((?:0[xX])?[0-9A-Fa-f]+)\s*,\s*(\w+)\s*,\s*'
    r'((?:0[xX])?[0-9A-Fa-f]+)\s*\)'
)


def parse_our_hooks(src_dir):
    hooks = []
    for path in sorted(Path(src_dir).rglob('*.cpp')):
        text = path.read_text(encoding='utf-8', errors='replace')
        for addr, name, size in HOOK_RE.findall(text):
            pasted = not addr.lower().startswith('0x')
            hooks.append({
                'addr': int(addr, 16),
                'size': (int(size, 16) if pasted or size.lower().startswith('0x')
                         else int(size)),
                'name': name,
                'where': f'{path}',
            })
    return hooks


def parse_registry(csv_path):
    entries = []
    with open(csv_path, newline='', encoding='utf-8', errors='replace') as fh:
        for row in csv.DictReader(fh):
            try:
                addr = int(row['Address'], 16)
                size = int(row['StolenBytes'], 16)
            except (ValueError, KeyError, TypeError):
                continue
            if size <= 0:
                continue
            entries.append({
                'addr': addr,
                'size': size,
                'framework': row.get('Framework', '?'),
                'name': row.get('Function', '?'),
            })
    return entries


def overlaps(a_start, a_size, b_start, b_size):
    """True when the two byte ranges intersect at DIFFERENT start addresses.

    Equal start addresses are excluded on purpose: Syringe chains those.
    """
    if a_start == b_start:
        return False
    return a_start < b_start + b_size and b_start < a_start + a_size


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2

    csv_path = sys.argv[1]
    src_dir = sys.argv[2] if len(sys.argv) > 2 else 'src'

    ours = parse_our_hooks(src_dir)
    if not ours:
        print('check_hook_overlap: found no DEFINE_HOOK in %s — refusing to pass '
              'vacuously.' % src_dir)
        return 2

    problems = []

    # Our hooks against each other.
    for i, a in enumerate(ours):
        for b in ours[i + 1:]:
            if overlaps(a['addr'], a['size'], b['addr'], b['size']):
                problems.append(
                    f"0x{a['addr']:06X}+{a['size']} {a['name']} overlaps "
                    f"0x{b['addr']:06X}+{b['size']} {b['name']} (both ours)")

    # Our hooks against every other injected DLL.
    registry = parse_registry(csv_path)
    for a in ours:
        for b in registry:
            if overlaps(a['addr'], a['size'], b['addr'], b['size']):
                problems.append(
                    f"0x{a['addr']:06X}+{a['size']} {a['name']} overlaps "
                    f"0x{b['addr']:06X}+{b['size']} {b['framework']}::{b['name']}")

    print(f'check_hook_overlap: {len(ours)} project hooks vs '
          f'{len(registry)} registry entries')

    if problems:
        print('\nOVERLAPPING HOOK RANGES — this corrupts the other DLL\'s jump:\n')
        for line in sorted(set(problems)):
            print(f'  {line}')
        print('\nMove the hook onto the other DLL\'s exact address so the two '
              'chain, or to a site outside its range.')
        return 1

    print('check_hook_overlap: no overlaps.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
