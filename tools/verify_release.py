#!/usr/bin/env python3
"""Focused static gate for MapSizeExt's pinned YR 1.001 executable profile."""
import argparse
import hashlib
import re
import struct
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--exe", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
data = Path(args.exe).read_bytes()
expected_hash = "3e81a61775d2745d1dabe397325ef663cd994ffc194da4e998e3bf5d2d308600"
assert hashlib.sha256(data).hexdigest() == expected_hash, "wrong pinned executable"

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
        struct.unpack_from("<I", data, offset + 12)[0],
        struct.unpack_from("<I", data, offset + 8)[0],
        struct.unpack_from("<I", data, offset + 20)[0],
        struct.unpack_from("<I", data, offset + 16)[0],
    ))

def read(va, size):
    relative = va - image_base
    for section_va, virtual_size, raw_offset, raw_size in sections:
        if section_va <= relative < section_va + max(virtual_size, raw_size):
            start = raw_offset + relative - section_va
            return data[start:start + size]
    raise AssertionError(hex(va))

hooks = (root / "src/Hooks.cpp").read_text()
entries = []
for match in re.finditer(
    r"DEFINE_HOOK\(([0-9A-Fa-f]+),\s*([A-Za-z0-9_]+),\s*(\d+)\)", hooks
):
    va = int(match.group(1), 16)
    size = int(match.group(3))
    assert len(read(va, size)) == size
    entries.append((va, match.group(2), size))

critical_hooks = {
    0x68512B: bytes.fromhex("A1 30 B2 A8 00"),
    0x67E694: bytes.fromhex("BB 01 00 00 00"),
    0x722E0F: bytes.fromhex("33 C0 3B CB 7C 13"),
}
for va, expected_bytes in critical_hooks.items():
    assert read(va, len(expected_bytes)) == expected_bytes, f"hook drift at {va:#x}"

patches = (root / "src/Patches.cpp").read_text()
match = re.search(r"static const DWORD kCellStrideSites\[\]\s*=\s*\{(.*?)\};", patches, re.S)
body = re.sub(r"//.*", "", match.group(1))
stride_sites = [int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]+", body)]
assert len(stride_sites) == 437
assert all(read(va, 3)[0] == 0xC1 and read(va, 3)[2] == 9 for va in stride_sites)

with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / "bounds.h"
    subprocess.run(
        [str(root / "tools/generate_bounds_manifest.py"), args.exe, "--output", str(output)],
        check=True,
    )
    assert output.read_bytes() == (root / "src/generated/BoundsSites_yr1001.h").read_bytes()

config = (root / "src/Config.h").read_text()
main = (root / "src/Main.cpp").read_text()
assert "ValidateConfig" in config and "RuntimeHostProfileSupported" in main
assert "PlaneScale" in config and "GeometryConflict" in config
assert struct.unpack_from("<I", data, pe + 8)[0] == 0x3BDF544E
assert struct.unpack_from("<I", data, optional + 16)[0] == 0x003CD80F
assert len(data) == 0x0050A940
print(f"PASS target={expected_hash} hooks={len(entries)} stride_sites={len(stride_sites)}")
