#!/usr/bin/env python3
"""Static pre-release gate for MapSizeExt against a YR 1.001 gamemd.exe.

Adapted from PR #3 (SethGekco/MapSizeExt), keeping the parts that are pure
verification and dropping those that were tied to that PR's fail-closed runtime
design (which we deliberately did not adopt -- see docs and the project notes:
a preflight mismatch that aborts all patches once turned the DLL into a silent
no-op and cost a whole debugging session; every patch site is byte-verified
individually instead, so an unexpected binary degrades gracefully).

What it proves, all statically:
  * the target executable is the pinned YR 1.001 build (SHA-256 + PE identity)
  * every DEFINE_HOOK address exists in the image and its stolen bytes are
    readable
  * no DEFINE_HOOK declares fewer than 5 bytes -- Syringe always writes a
    5-byte jump and resumes at addr+max(size,5), so a smaller size leaves
    orphaned bytes executing (a crash class this project has actually hit)
  * hook prologues at a few critical sites have not drifted
  * every kCellStrideSites entry really is `shl reg, 9` in the image

Usage:
    tools/verify_release.py --exe /path/to/gamemd.exe
    tools/verify_release.py --exe ... --allow-unpinned   # different build: skip identity
"""
import argparse
import hashlib
import re
import struct
import sys
from pathlib import Path

PINNED_SHA256 = "3e81a61775d2745d1dabe397325ef663cd994ffc194da4e998e3bf5d2d308600"
PINNED_TIMESTAMP = 0x3BDF544E
PINNED_ENTRYPOINT = 0x003CD80F
PINNED_FILESIZE = 0x0050A940

# Prologues that must still be intact for the hooks placed on them.
CRITICAL_HOOKS = {
    0x68512B: bytes.fromhex("A130B2A800"),
    0x67E694: bytes.fromhex("BB01000000"),
    0x722E0F: bytes.fromhex("33C03BCB7C13"),
}

SYRINGE_MIN_HOOK_BYTES = 5


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, help="path to gamemd.exe")
    parser.add_argument("--allow-unpinned", action="store_true",
                        help="continue when the executable is not the pinned build")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    data = Path(args.exe).read_bytes()
    failures: list[str] = []
    notes: list[str] = []

    # --- executable identity -------------------------------------------------
    digest = hashlib.sha256(data).hexdigest()
    pinned = digest == PINNED_SHA256
    if not pinned:
        message = f"not the pinned executable (sha256={digest})"
        (notes if args.allow_unpinned else failures).append(message)

    pe = struct.unpack_from("<I", data, 0x3C)[0]
    section_count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    image_base = struct.unpack_from("<I", data, optional + 28)[0]
    section_table = optional + optional_size

    sections = []
    for index in range(section_count):
        offset = section_table + 40 * index
        sections.append((
            struct.unpack_from("<I", data, offset + 12)[0],   # VirtualAddress
            struct.unpack_from("<I", data, offset + 8)[0],    # VirtualSize
            struct.unpack_from("<I", data, offset + 20)[0],   # PointerToRawData
            struct.unpack_from("<I", data, offset + 16)[0],   # SizeOfRawData
        ))

    def read(va: int, size: int):
        relative = va - image_base
        for section_va, virtual_size, raw_offset, raw_size in sections:
            if section_va <= relative < section_va + max(virtual_size, raw_size):
                start = raw_offset + relative - section_va
                chunk = data[start:start + size]
                return chunk if len(chunk) == size else None
        return None

    if pinned:
        if struct.unpack_from("<I", data, pe + 8)[0] != PINNED_TIMESTAMP:
            failures.append("PE timestamp drift")
        if struct.unpack_from("<I", data, optional + 16)[0] != PINNED_ENTRYPOINT:
            failures.append("entry point drift")
        if len(data) != PINNED_FILESIZE:
            failures.append("file size drift")

    # --- hook inventory ------------------------------------------------------
    hooks_src = (root / "src/Hooks.cpp").read_text()
    entries = []
    for match in re.finditer(
            r"DEFINE_HOOK(?:_AGAIN)?\(([0-9A-Fa-f]+),\s*([A-Za-z0-9_]+),\s*(\d+)\)", hooks_src):
        va = int(match.group(1), 16)
        name = match.group(2)
        size = int(match.group(3))
        entries.append((va, name, size))

        if read(va, max(size, SYRINGE_MIN_HOOK_BYTES)) is None:
            failures.append(f"{name} @{va:#x}: address not present in the image")
        if size < SYRINGE_MIN_HOOK_BYTES:
            failures.append(
                f"{name} @{va:#x}: declares {size} bytes; Syringe writes "
                f"{SYRINGE_MIN_HOOK_BYTES} and resumes past them")

    if not entries:
        failures.append("no DEFINE_HOOK entries found -- regex drift?")

    for va, expected in CRITICAL_HOOKS.items():
        actual = read(va, len(expected))
        if actual is None:
            notes.append(f"critical site {va:#x} not readable")
        elif actual != expected:
            failures.append(f"prologue drift at {va:#x}: {actual.hex()} != {expected.hex()}")

    # --- stride site manifest ------------------------------------------------
    patches_src = (root / "src/Patches.cpp").read_text()
    match = re.search(r"kCellStrideSites\[\]\s*=\s*\{(.*?)\};", patches_src, re.S)
    stride_sites: list[int] = []
    if not match:
        failures.append("kCellStrideSites not found in Patches.cpp")
    else:
        body = re.sub(r"//.*", "", match.group(1))
        stride_sites = [int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]+", body)]
        bad = []
        for va in stride_sites:
            chunk = read(va, 3)
            # C1 /r 09 == shl reg, 9
            if chunk is None or chunk[0] != 0xC1 or chunk[2] != 0x09:
                bad.append(va)
        if bad:
            failures.append(
                f"{len(bad)} stride site(s) are not `shl reg,9`: "
                + ", ".join(f"{va:#x}" for va in bad[:8])
                + (" ..." if len(bad) > 8 else ""))

    # --- report --------------------------------------------------------------
    for note in notes:
        print(f"note: {note}")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1

    print(f"PASS  exe={'pinned' if pinned else 'unpinned'} "
          f"hooks={len(entries)} stride_sites={len(stride_sites)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
