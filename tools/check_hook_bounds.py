#!/usr/bin/env python3
"""
Fail the build if a hook's declared size leaves Syringe resuming on a byte that
isn't the start of an instruction.

Why this exists
---------------
Syringe patches every hook site with a **5-byte `E9 rel32`**, no matter what
size you declare, and its stub resumes at **`addr + max(size, 5)`** — not at
`addr + size`. The stub copies only `size` bytes of the original instruction(s).

So a hook is safe only when `addr + max(size,5)` is an instruction boundary.
Two ways to break that:

  1. size < 5 — the 5-byte JMP overruns into the next instruction, and the
     resume address skips past its first byte(s), landing on orphaned operand
     bytes.
  2. size >= 5 but splitting an instruction — resume lands mid-instruction.

Either way the CPU executes garbage. It only triggers when the handler returns
0 (the copied-bytes path), so a hook whose early-outs all `return 0` — the
common shape — crashes almost immediately, while one that usually returns a
jump target can hide for months.

Real case this was written for: MirageTreesExt hooked 0x6F7CB1 with size 4.
The instruction there genuinely IS 4 bytes (8B 74 24 4C  mov esi,[esp+0x4C]),
so it looked right. But the 5-byte JMP ate the 8B of `mov edx,[edi]` at
0x6F7CB5, and the stub resumed at 0x6F7CB6 on the orphaned 17 byte = `pop ss`
-> #GP. Reported as C0000005 "READ at 0xFFFFFFFF", which is the signature of a
general protection fault, not a bad pointer.

Two tiers, because CI has no copy of the game
---------------------------------------------
SOURCE MODE (always; runs in CI, needs nothing):
    Flags every hook with size < 5. Such a hook is safe ONLY if it can never
    return 0. If you have verified that, annotate it and this check will pass:

        // syringe-size-ok: never returns 0, all paths jump
        DEFINE_HOOK(0x71C325, Foo, 0x3)

    With --registry, also cross-checks declared sizes against the YR Hook
    Encyclopedia's recorded StolenBytes for the same address.

BINARY MODE (--exe, local / pre-deploy):
    Full instruction-boundary verification of every hook against the real
    gamemd executable, via objdump. This is the complete check; run it before
    deploying a build.

Usage
-----
    check_hook_bounds.py [src-dir]                       # source mode
    check_hook_bounds.py --registry hooks.csv [src-dir]
    check_hook_bounds.py --exe gamemd-spawn.exe [src-dir]
"""
import argparse
import csv
import re
import struct
import subprocess
import sys
from pathlib import Path

# Two Syringe macro conventions are in use across these projects, and they read
# their arguments differently:
#
#   {  hook, size, ... }          -- values as written: DEFINE_HOOK(0x4C9EA0, n, 0x5)
#   { 0x ## hook, 0x ## size }    -- SYR_VER==2 pastes the prefix on:
#                                    DEFINE_HOOK(48EB12, n, 6)  ==  0x48EB12, 0x6
#
# Under the pasting form BOTH arguments are hex, so a size written `10` means 16.
# Detect it by the missing 0x on the address, and parse the size the same way.
HOOK_RE = re.compile(
    r'\bDEFINE_HOOK(?:_AGAIN)?\s*\(\s*((?:0[xX])?[0-9A-Fa-f]+)\s*,\s*(\w+)\s*,\s*'
    r'((?:0[xX])?[0-9A-Fa-f]+)\s*\)'
)
# An explicit, reviewed waiver. Suppresses ALL three defect classes, because the
# only thing that can clear them is a human having checked reachability — see
# 0x4F8361, where the patch really does clobber a jump table, but the table is
# unreachable because Antares owns the function's only entry.
WAIVER_RE = re.compile(r'syringe-(?:size|hook)-ok\s*:\s*(.*)')
RETURN_RE = re.compile(r'\breturn\b([^;]*);')
ENUM_RE = re.compile(r'\benum\b[^{]*\{([^}]*)\}')
TOKEN_RE = re.compile(r'0[xX][0-9A-Fa-f]+|\b\d+\b|\b[A-Za-z_]\w*\b')

# Syringe always writes this many bytes, and always resumes at addr + this
# many when the declared size is smaller.
SYRINGE_PATCH_BYTES = 5

# How a handler's return paths classify. Only ZERO handlers can ever reach the
# stub's copied-bytes path, so only they care about the resume address.
ZERO, NEVER_ZERO, UNKNOWN = 'zero', 'never-zero', 'unknown'


def resume_addr(addr, size):
    return addr + max(size, SYRINGE_PATCH_BYTES)


def strip_comments(text):
    """Drop // and /* */ comments.

    Not cosmetic: a hook whose body contains `// return address at function
    entry` was classified UNKNOWN because the scanner matched the word `return`
    inside that comment and then tried to resolve the following code as its
    operand.
    """
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', '', text)


def extract_body(lines, start):
    """Text of the hook body, by brace matching from the DEFINE_HOOK line."""
    depth, started, out = 0, False, []
    for line in lines[start:start + 400]:
        out.append(line)
        for ch in line:
            if ch == '{':
                depth += 1
                started = True
            elif ch == '}':
                depth -= 1
        if started and depth <= 0:
            break
    return '\n'.join(out)


def classify_returns(body):
    """Can this handler return 0 (i.e. fall through to the copied bytes)?

    Resolves local `enum { Name = 0x... }` constants, because the codebase
    normally returns a named target rather than a bare literal — and some of
    those names ARE zero (`enum { LetPhobosRun = 0 }`).
    """
    consts, nxt = {}, 0
    for block in ENUM_RE.findall(body):
        for item in block.split(','):
            item = item.strip()
            if not item:
                continue
            if '=' in item:
                nm, _, val = item.partition('=')
                try:
                    nxt = int(val.strip(), 0)
                except ValueError:
                    continue
                consts[nm.strip()] = nxt
            else:
                consts[item] = nxt
            nxt += 1

    def value_of(operand):
        """Resolve one return operand to an int, or None if we cannot."""
        operand = operand.strip().strip('()').strip()
        if operand in ('false', 'nullptr'):
            return 0
        if operand == 'true':
            return 1
        if re.fullmatch(r'0[xX][0-9A-Fa-f]+|\d+', operand):
            return int(operand, 0)
        return consts.get(operand)        # None when unknown

    saw_return, unknown = False, False
    for expr in RETURN_RE.findall(strip_comments(body)):
        expr = expr.strip()
        saw_return = True
        if not expr:                      # bare `return;` — not a jump target
            return ZERO
        # `cond ? A : B` — only the branches can become the return value.
        operands = ([s for s in expr.split('?', 1)[1].split(':')]
                    if '?' in expr else [expr])
        for operand in operands:
            v = value_of(operand)
            if v is None:
                unknown = True
            elif v == 0:
                return ZERO
    if not saw_return:
        return UNKNOWN
    return UNKNOWN if unknown else NEVER_ZERO


def parse_our_hooks(src_dir):
    """Every DEFINE_HOOK in the tree, with waiver and return-path class."""
    hooks = []
    for path in sorted(Path(src_dir).rglob('*.cpp')):
        lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
        for i, line in enumerate(lines):
            m = HOOK_RE.search(line)
            if not m:
                continue
            addr, name, size = m.groups()
            # A waiver may sit on the hook line or in the comment block above it.
            waiver = None
            for probe in [line] + lines[max(0, i - 6):i][::-1]:
                w = WAIVER_RE.search(probe)
                if w:
                    waiver = w.group(1).strip() or '(no reason given)'
                    break
            pasted = not addr.lower().startswith('0x')
            hooks.append({
                'addr': int(addr, 16),
                'size': (int(size, 16) if pasted or size.lower().startswith('0x')
                         else int(size)),
                'name': name,
                'where': f'{path}:{i + 1}',
                'waiver': waiver,
                'returns': classify_returns(extract_body(lines, i)),
            })
    return hooks


def check_sizes(hooks):
    """Source-only tier: size < 5 is a latent #GP — but ONLY for a handler that
    can return 0. A handler that always returns a jump target never reaches the
    stub's copied bytes, so its declared size is cosmetic; `size 0` is a
    deliberate idiom for exactly that."""
    problems, waived, benign = [], [], []
    for h in hooks:
        if h['size'] >= SYRINGE_PATCH_BYTES:
            continue
        if h['returns'] == NEVER_ZERO:
            benign.append(h)
            continue
        if h['waiver']:
            waived.append(h)
            continue
        problems.append(
            "0x{addr:06X} size {size} {name}   [{returns}]\n"
            "      {where}\n"
            "      Syringe writes {patch} bytes and resumes at 0x{res:06X} "
            "(addr+{patch}), not addr+{size}.".format(
                patch=SYRINGE_PATCH_BYTES, res=resume_addr(h['addr'], h['size']),
                **h))
    return problems, waived, benign


def check_against_registry(hooks, csv_path):
    """Our declared size vs the encyclopedia's recorded stolen-byte count."""
    known = {}
    with open(csv_path, newline='', encoding='utf-8', errors='replace') as fh:
        for row in csv.DictReader(fh):
            try:
                known.setdefault(int(row['Address'], 16), []).append(
                    (int(row['StolenBytes'], 16), row.get('Framework', '?')))
            except (ValueError, KeyError, TypeError):
                continue
    notes = []
    for h in hooks:
        for size, framework in known.get(h['addr'], []):
            if size != h['size']:
                notes.append(
                    f"0x{h['addr']:06X} {h['name']} declares size {h['size']}, "
                    f"but {framework} records {size} stolen bytes at the same "
                    f"address")
    return notes


# --- binary mode -----------------------------------------------------------

def load_text(exe_path):
    """Map the PE's sections so we can turn a VA into file bytes."""
    data = Path(exe_path).read_bytes()
    pe = struct.unpack('<I', data[0x3C:0x40])[0]
    nsec = struct.unpack('<H', data[pe + 6:pe + 8])[0]
    optsz = struct.unpack('<H', data[pe + 20:pe + 22])[0]
    imgbase = struct.unpack('<I', data[pe + 24 + 28:pe + 24 + 32])[0]
    tbl = pe + 24 + optsz
    secs = []
    for i in range(nsec):
        e = data[tbl + i * 40:tbl + i * 40 + 40]
        vsz, va, rsz, ra = struct.unpack('<IIII', e[8:24])
        secs.append((imgbase + va, max(vsz, rsz), ra))
    return data, secs


def read_va(data, secs, va, n):
    for start, size, raw in secs:
        if start <= va < start + size:
            off = raw + (va - start)
            return data[off:off + n]
    return None


def boundaries(code, base):
    """Instruction start addresses, by linear disassembly from a known start."""
    tmp = Path('.hookbounds.tmp')
    tmp.write_bytes(code)
    try:
        out = subprocess.run(
            ['objdump', '-D', '-b', 'binary', '-m', 'i386', '-M', 'intel',
             f'--adjust-vma={base}', str(tmp)],
            capture_output=True, text=True, check=True).stdout
    finally:
        tmp.unlink(missing_ok=True)
    found = []
    for line in out.splitlines():
        parts = line.strip().split('\t')
        head = parts[0].strip().rstrip(':')
        try:
            addr = int(head, 16)
        except ValueError:
            continue
        text = parts[-1].strip() if len(parts) > 2 else ''
        found.append((addr, text))
    return found


# An instruction after which control never falls through. Bytes following one
# are not reached linearly, so they are usually padding or a jump table.
TERMINATOR_RE = re.compile(r'^(ret|retf|jmp|iret|ud2)\b')


# Compiler alignment filler. Overwriting these costs nothing.
PADDING_BYTES = {0x90, 0xCC}


def patch_crosses_terminator(bounds, addr, code):
    """Does Syringe's 5-byte patch write past the end of the code here?

    Syringe stamps 5 bytes unconditionally. If a `ret`/`jmp` ends inside that
    window, whatever follows is not reached by fall-through, so the patch is
    writing over something that isn't part of this function.

    That is only a problem if the something is real. Behind an epilogue you get
    either alignment padding (harmless — SuperWeaponExt 0x6CE8EA spills into
    NOPs) or live data such as a switch jump table (harmful — 0x4F8361 spills
    into one). Distinguish them by looking at the bytes.
    """
    for i, (a, text) in enumerate(bounds):
        if a < addr:
            continue
        end = bounds[i + 1][0] if i + 1 < len(bounds) else None
        if end is None:
            return None
        if TERMINATOR_RE.match(text) and end < addr + SYRINGE_PATCH_BYTES:
            spilled = code[end - addr:SYRINGE_PATCH_BYTES]
            if spilled and all(b in PADDING_BYTES for b in spilled):
                return None       # only alignment filler is clobbered
            return a, text, end, len(spilled)
        if end >= addr + SYRINGE_PATCH_BYTES:
            return None
    return None


def check_bounds(hooks, exe_path):
    """Full tier: verify the resume address is a real instruction boundary.

    Split into fatal vs latent by return class. A handler that always returns a
    jump target never executes the stub's copied bytes, so a bad boundary can
    never be reached — reported for awareness, but it is not a live bug.
    """
    data, secs = load_text(exe_path)
    problems, latent, checked = [], [], 0
    for h in hooks:
        if h['waiver']:
            latent.append(
                "0x{addr:06X} size {size} {name}   [WAIVED]\n"
                "      {where}\n"
                "      {waiver}".format(**h))
            continue
        want = resume_addr(h['addr'], h['size'])
        window = (want - h['addr']) + 24
        code = read_va(data, secs, h['addr'], window)
        if not code or len(code) < window:
            problems.append(
                f"0x{h['addr']:06X} {h['name']}: address not in any section of "
                f"{exe_path} — wrong executable, or a bad address")
            continue
        bounds = boundaries(code, h['addr'])
        checked += 1

        # Worse than a bad resume: the patch itself lands on non-code.
        crossing = patch_crosses_terminator(bounds, h['addr'], code)
        if crossing:
            term_at, term_text, term_end, spill = crossing
            problems.append(
                "0x{addr:06X} size {size} {name}   [{returns}]\n"
                "      {where}\n"
                "      `{text}` at 0x{term:06X} ends at 0x{end:06X}, but Syringe "
                "stamps {patch} bytes to 0x{stop:06X}\n"
                "      -> {spill} byte(s) of whatever follows the function "
                "(padding, or a SWITCH JUMP TABLE) are overwritten.\n"
                "      This corrupts data even though the hook itself appears to "
                "work. Move it to a site inside the function body.".format(
                    text=term_text, term=term_at, end=term_end, spill=spill,
                    patch=SYRINGE_PATCH_BYTES,
                    stop=h['addr'] + SYRINGE_PATCH_BYTES, **h))
            continue

        addrs = [a for a, _ in bounds]
        if want in addrs:
            continue
        nxt = next((b for b in addrs if b > want), None)
        fix = f"; use size {nxt - h['addr']}" if nxt else ""
        entry = ("0x{addr:06X} size {size} {name}   [{returns}]\n"
                 "      {where}\n"
                 "      resumes at 0x{want:06X}, which is NOT an instruction "
                 "boundary{fix}".format(want=want, fix=fix, **h))
        (latent if h['returns'] == NEVER_ZERO else problems).append(entry)
    return problems, latent, checked


def main():
    ap = argparse.ArgumentParser(add_help=True)
    ap.add_argument('src', nargs='?', default='src')
    ap.add_argument('--exe', help='gamemd(-spawn).exe for full boundary checking')
    ap.add_argument('--registry', help="YR Hook Encyclopedia registry/hooks.csv")
    args = ap.parse_args()

    hooks = parse_our_hooks(args.src)
    if not hooks:
        print(f'check_hook_bounds: no DEFINE_HOOK found in {args.src} — '
              f'refusing to pass vacuously.')
        return 2

    failed = False

    size_problems, waived, benign = check_sizes(hooks)
    print(f'check_hook_bounds: {len(hooks)} hooks; {len(benign)} sub-'
          f'{SYRINGE_PATCH_BYTES}-byte but never return 0; {len(waived)} waived')
    for h in benign:
        print(f"  safe    0x{h['addr']:06X} size {h['size']} {h['name']} "
              f"— always returns a jump target, copied bytes unreachable")
    for h in waived:
        print(f"  waived  0x{h['addr']:06X} size {h['size']} {h['name']} "
              f"— {h['waiver']}")

    if size_problems:
        failed = True
        print(f'\nHOOK SMALLER THAN SYRINGE\'S {SYRINGE_PATCH_BYTES}-BYTE PATCH '
              f'— resumes on orphaned bytes:\n')
        for p in size_problems:
            print(f'  {p}')
        print('\n  Widen the hook to cover whole instructions through the resume\n'
              '  address. If the handler provably never returns 0, waive it:\n'
              '      // syringe-size-ok: <why it never returns 0>')

    if args.registry:
        notes = check_against_registry(hooks, args.registry)
        if notes:
            print('\nSize disagrees with the hook registry (not fatal, but one '
                  'of the two is wrong):\n')
            for n in notes:
                print(f'  {n}')

    if args.exe:
        bound_problems, latent, checked = check_bounds(hooks, args.exe)
        print(f'\ncheck_hook_bounds: boundary-checked {checked} hooks against '
              f'{args.exe}')
        if latent:
            print('\nKnown and reviewed — not failing the build, but re-check '
                  'these whenever the\nsurrounding framework changes:\n')
            for p in latent:
                print(f'  {p}')
        if bound_problems:
            failed = True
            print("\nSYRINGE'S 5-BYTE PATCH DOES NOT LINE UP WITH THE CODE:\n")
            for p in bound_problems:
                print(f'  {p}')
    else:
        print('check_hook_bounds: no --exe given; skipping full boundary check '
              '(source-only tier).')

    if failed:
        return 1
    print('check_hook_bounds: OK.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
