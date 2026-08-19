#!/usr/bin/env python3
"""Report plane-derived operands from one MapSizeExt scale value."""
import argparse
import json


def geometry(scale: int) -> dict[str, int | str]:
    if scale not in (1, 2, 4, 8):
        raise ValueError("scale must be one of 1, 2, 4, or 8")
    stride = 512 * scale
    logical_square = stride // 2
    total = stride * stride
    tiberium_count = 2 * logical_square * (logical_square + 4)
    return {
        "plane_scale": scale,
        "runtime_status": "accepted" if scale <= 4 else "materialization-only",
        "stride": stride,
        "stride_shift": stride.bit_length() - 1,
        "total_cells": total,
        "index_mask": stride - 1,
        "byte_stride_shift": stride.bit_length() + 1,
        "iterator_displacement": 4 - 4 * stride,
        "max_square_from_plane": logical_square,
        "radar_width": 400 * scale,
        "radar_height": 640 * scale,
        "radar_bytes": 512_000 * scale * scale,
        "iso_dimension": stride + 64,
        "iso_bytes": (stride + 64) ** 2 * 2,
        "cell_pointer_plane_bytes": total * 4,
        "tiberium_bytes_per_type_at_max_square": 13 * tiberium_count + 24,
    }


parser = argparse.ArgumentParser()
parser.add_argument("--scale", type=int, choices=(1, 2, 4, 8))
parser.add_argument("--json", action="store_true")
args = parser.parse_args()
rows = [geometry(args.scale)] if args.scale else [geometry(x) for x in (1, 2, 4, 8)]
if args.json:
    print(json.dumps(rows[0] if args.scale else rows, indent=2))
else:
    columns = (
        "plane_scale", "runtime_status", "stride", "max_square_from_plane",
        "total_cells", "cell_pointer_plane_bytes", "radar_bytes", "iso_bytes",
        "tiberium_bytes_per_type_at_max_square",
    )
    print(" ".join(f"{column:>24}" for column in columns))
    for row in rows:
        print(" ".join(f"{str(row[column]):>24}" for column in columns))
