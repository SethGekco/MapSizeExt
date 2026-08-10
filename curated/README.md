# Curated MapSizeExt (his base + our 300x300 fixes)

This is Krisztiaan's proven 512->1024 plane implementation
(`yr_map_512_plane_probe.c` + `yr_map_512_patch_table.h`) with our additions
layered on top to reach 300x300. Built with plain mingw (see `build.sh`):

    ./build.sh   # -> MapSizeExt.dll

Correct at <=250x250 out of his base (walls, shroud, foundation, sidebar,
radar, movement). Our additions for 300x300 (see repo docs/BUG-ATLAS.md M4):

- Patch table: 2 plane-iterator bounds (0x565bd0/0x565bf6), 27 iterator
  shl-0xB->0xC end-pointer sites his 74 lacked, 2 inverse-conversion
  sign-extension coords (0x565c7e/0x566f9a, the bottom-right->top-left wrap).
- Hooks (his .syhks00 style): Map512CellSlotGuard @0x5663BC (plane-init -1
  guard), Map512CellIteratorGuard @0x578290 (wild cell-pointer guard).

STATUS: 300x300 loads and plays; walls/sidebar/buildings/movement/pathfinding
normal. KNOWN REMAINING: ordering a unit to the extreme bottom-left corner via
radar fatals on the coord-transform path (0x660540 family reads the 0x880A04
singleton + virtual-calls); skipping 0x660540 is NOT a valid fix (its result is
used for routing -> regresses bottom-right). Root not yet pinned.
