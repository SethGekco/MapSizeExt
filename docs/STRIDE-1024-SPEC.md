# Raising the Yuri's Revenge map limit to 512×512 (cell-grid stride 512 → 1024)

**A complete engineering specification, derived from a working implementation.**

This document describes every change required to make `gamemd.exe` (YR 1.001) support
maps up to 512×512 cells, what each change fixes, and why. Every address was verified
by disassembly and every fix validated in-game on 300×300 and 500×500 maps (walls,
lighting, pathfinding, naval, harvesters, deploy, radar all confirmed working).

The reference implementation is **MapSizeExt** (GPLv3):
<https://github.com/SethGekco/MapSizeExt>, branch `fix/phase1-correctness`.
It applies these as runtime byte-patches from a Syringe DLL; a source-level port
(e.g. into Phobos) would express the same changes as constants/logic changes and can
ignore the patch-table mechanics. File pointers below name the repo file and
function/table that implements each item.

**Scope**: stride 1024 only (map dimensions up to 512 per axis). No LAA, no
coordinate-base changes are needed at this size (see §11 for the one sizing caveat).

---

## 0. Architecture of the limit

The engine stores the map as a flat array of `CellClass*`:

```
index = Y * 512 + X          // "stride" = 512, baked in as shl 9 / literals
capacity = 512 * 512         // 0x40000, baked in as cmp/push literals
```

There is **no single constant**. The 512 exists in five distinct encodings, each
requiring its own audit (this is the core reason previous attempts failed):

| form | example | count |
|---|---|---|
| A. index shifts `shl reg,9` (and `sar 9` inverses) | `Y<<9 + X` | ~436 real cell sites |
| B. capacity literals `0x40000` (cmp/push) | bounds checks, alloc | ~440 |
| C. plain dimension immediates `0x200` | array alloc dims, row walks | few, invisible to A/B scans |
| D. byte-offset forms `shl reg,0xB` (= row*512*4) | the map iterator | 32 + 1 lea |
| E. bit masks `and 0x1FF` / `or 0xFFFFFE00` etc. | index→(X,Y) inverse | 8 |

Additionally every YRpp-based extension DLL (Phobos, Ares, Antares, …) compiles the
inline `GetCellIndex = (Y<<9)+X` and `MaxCells = 0x40000` into its own code. A
source-level adoption in Phobos fixes its own copies by changing the YRpp constants
and rebuilding; the engine (gamemd) sites still need the patches below.

---

## 1. Per-axis dimension gates

**What**: three `cmp ax,0x200` checks reject maps with W≥512 or H≥512.
**Where**: `0x4C5630`, `0x4C590E`, `0x554BC5`.
**Fix**: compare against 1024 (or a configurable max).
**Bug fixed**: big maps refuse to load at all.
**Note**: there is *no* W+H≤512 check in the engine; that rule is a FinalAlert
editor artifact. The real square-map limit is the grid (see §0).
**Reference**: `src/Hooks.cpp`, `MapDimGate_Site1/2/3`.

## 2. Cell-array allocation, dimensions, and population

**What**:
- Root dimensions: `mov eax,0x200` @ `0x565812` (stride) and
  `mov [ebp+0x154],0x40000` @ `0x565828` (total) size the main array in
  `MapClass::Init`. These are plain immediates — invisible to shl/cmp scans.
- The array `Reserve` size: `push 0x40000` @ `0x565B87`.
- The population loop's **row stride is `add ecx,0x200`** @ `0x566437` — an
  add-form 512, again invisible to shift scans.

**Fix**: 0x200→0x400, 0x40000→0x100000 at all four.
**Bugs fixed**: constructor crash on cell index > 0x40000 (`C0000005 @0x410174`);
and the classic "map renders but MCV can't deploy / units can't move": the array
was *populated* at stride 512 while *read* at 1024, so every coordinate lookup hit
a null slot. Rendering worked because it walks the array linearly — a trap that
masks the bug.
**Reference**: `src/Patches.cpp` (`roots[]` in `ApplyBoundsPatches`,
`ApplyCellArrayPopulationStride`).

**Strongly recommended companion**: zero-initialize the array after allocation.
The population only writes the iso-diamond interior; the rectangular corners
otherwise hold garbage pointers that pass null-checks (crashes via
`GetCellAt` on flying units, iterator corruption). MapSizeExt guards each lookup
with a cell-identity check (`cell->MapCoords == (idx%stride, idx/stride)`) —
`GetCellAt_GarbageGuard`, `CellIterator_OOBGuard` in `src/Hooks.cpp` — but a
`memset` at source level is the clean fix and removes both guards.

## 3. Index-shift sites (form A) — with the false-positive list

**What**: ~496 `shl reg,9` sites exist; **436 are real cell indexes** (the result
feeds a `[base+reg*4]` access of `Cells.Items`); the rest must NOT be touched.
**Fix**: rewrite shift 9→10 at the 436 (source-level: they all come from
`GetCellIndex`-equivalent code; a single constant change covers them).
**Bugs fixed**: universal — every cell read/write.

**Critical false positives (cost us weeks — do not patch these)**:
- `0x547DC7` (`IsometricTileTypeClass::ReadFromFile`) and **40 sites in the SHP
  drawer block `0x493CF1..0x499ADC`**: these `shl 9` are *lighting-remap table
  row strides* (256 colors × 2 bytes). Signature: the value is first **clamped to
  ≤0xFE**, then shifted, then added to a table base (often `[this+0x8]`).
  Patching them = black/wrong-palette buildings and units, crashes on sloped maps.
- `0x4D0xxx` isometric projection cluster, `0x47E072`-style bitfield packers.

**Also make sure these are IN the list** (each was a found-the-hard-way bug):
- `0x5887C7` — vanilla wall line-fill (`sub_588750`); missing it = player-built
  walls don't connect N/S.
- `0x56BB1B` (+cap `0x56BB22`) — IsoMapPack5 decoder; missing = corrupt non-flat
  terrain.
- Visibility pair sites `0x5863C5/CC`, `0x586451/58`, `0x586505/0C`,
  `0x586590/97`, `0x586615/1C` — shroud correctness.

**Reference**: `src/Patches.cpp`, `kCellStrideSites` (the excluded blocks are
commented in place with their signatures).

## 4. Capacity bounds (form B) — with the re-validate trap

**What**: ~401 `cmp eax,0x40000`, ~37 `cmp reg,0x40000`, 2 `push 0x40000`.
**Fix**: 0x40000 → 0x100000.
**Trap**: three bounds in `MapClass::AddContentAt/RemoveContentAt`
(`0x568710`, `0x5687A7`, `0x568B58`) look like buffer checks because the array
access *precedes* the cmp (re-validate pattern). They are real cell bounds.
Left at 0x40000 they reject every cell with Y>256: units freeze in the lower
half of large maps (diagnosed via endless re-path at indexes ≥0x40000).
One true skip: `0x565B73` (the Reserve guard, handled with §2).
**Reference**: `src/Patches.cpp`, `ApplyBoundsPatches`,
`ApplyOccupancyBoundPatches`.

## 5. Inverse conversion (form E)

**What**: index→(X,Y) decomposition: `and 0x1FF` (X), `sar 9` (Y), and the signed
modulo idiom `and 0x800001FF` / `or 0xFFFFFE00`. Eight sites
(`0x565C88/96`, `0x566FA4/B2` families).
**Fix**: mask 0x3FF, shift 10, `0x800003FF`/`0xFFFFFC00`. Patch the **pair**
together — fixing only the divide half mangles X and crashes.
**Bug fixed**: the "bottom-right of the map wraps to top-left" coordinate fold.
**Reference**: `src/Patches.cpp`, `ApplyCoordPatches`.

## 6. Byte-offset iterator (form D)

**What**: the full-map cell iterator @`0x578290` walks with **byte** arithmetic:
32 setup sites compute the end bound with `shl reg,0xB` (rows×512×4), and the
diagonal step is `lea eax,[ebp-0x7FC]` (−511×4) @`0x5782BD`.
**Fix**: shl 0xB→0xC; −0x7FC→−0xFFC.
**Bugs fixed**: the entire passability/zone recompute walks the wrong range —
units walk on water, can't climb slopes, harvesters stall, vehicles only move on
a sold building's foundation. Flat single-zone maps hide this bug entirely; it
only shows on varied terrain.
**Reference**: `src/Patches.cpp`, `kIterBoundSites`,
`ApplyIteratorStridePatches`.

## 7. Static adjacency table

**What**: the 8-neighbour cell-index offset table @`0x7E3774`:
`[-512,-511,1,513,512,511,-1,-513]`.
**Fix**: rewrite as ±1024 equivalents.
**Reference**: `src/Patches.cpp`, `ApplyAdjacencyPatch`.

## 8. IsoMapPack5 load buffer

**What**: the map-file decode buffer is 400×640×2 bytes: `push 0x190/0x280/0x7D000`
@`0x4AD344/49/57`.
**Fix**: scale to cover the diamond of a 512² map (MapSizeExt uses 768×1024×2).
**Bug fixed**: maps beyond ~200/side fail to load at all (this is the *first*
wall anyone hits, before the grid).
**Reference**: `src/Patches.cpp` / `src/Main.cpp` (IsoMapPack patch at init).

## 9. Subzone system (pathfinding hierarchy)

Two independent problems:

- **Scale**: 17 sites in `MapClass::RecalculateSubZones(Passes)`
  (`0x5820B6`…`0x584E0B`) define the hierarchy block sizes (4- and 8-cell).
  Double them (4→8, 8→16) so subzone IDs fit on the bigger plane. The exact
  byte-for-byte set is in `src/Patches.cpp`, `kSubzoneScale[]` (originally
  derived and validated by a collaborating project; corners fully reachable).
- **Signedness**: subzone IDs are 16-bit words read back with `movsx`. Above
  0x7FFF they go negative → OOB indexing → `C0000005` (e.g. `@0x429EA4` reading
  the array at `MapClass+0x70`), and a producer wrap-to-0 causes infinite
  recursion (`stack overflow in RecalculateSubZonesPass @0x5824A0`). Minimal
  fix: saturate the producer @`0x58215B` (≤0x7FFF). Full fix: 14× movsx→movzx
  consumers + producer ceiling 0xFFFE. At stride 1024 with the 4→8 scale,
  saturation is not reached on ≤512² maps; ship both anyway for safety.
**Reference**: `src/Patches.cpp` `ApplySubzoneScalePatches`,
`src/Hooks.cpp` `Subzone_SaturateID`.

## 10. Radar / minimap

**What**: the radar surface is created 400×640 with hardcoded dims and 512 gates:
7 sites `0x5FD2FD`, `0x5FD302`, `0x5FD31C`, `0x5FD509`, `0x5FD516`, `0x5FD647`,
`0x5FD650`.
**Fix**: surface 800×1280, buffer 0x1F4000, gates 1024. Without this the radar
surfaces are never created at big dims and every minimap draw needs null-guarding.
**Known issue to design for**: save/load rebuilds the radar through the map
iterator and expects vanilla strides during the rebuild window (a collaborator
handled this with a temporary phase-switch of the §6 sites around
`sub_685120`/`0x67E694`). Test save/load explicitly.
**Reference**: `src/Patches.cpp`, `ApplyRadarPatches`.

## 11. Sizing caveat: keep W+H ≤ 1000

The engine's *external* cell numbering (map INI waypoints, event targets, several
gameplay decoders) is `N = Y*1000 + X` — base-1000, independent of the grid
stride. Since iso `rx` runs to W+H−1, any map with **W+H > 1000** breaks that
numbering even though stride 1024 could hold it. At this tier, simply reject or
don't author maps beyond W+H = 1000 (square 500×500 is the practical max, and
what we shipped/validated). Re-basing the external numbering is a much larger
campaign (it spans waypoints, order targets, and several serializers) — treat it
as out of scope for the 1024 adoption.

## 12. Extension DLLs (Phobos's own code)

Every YRpp-derived DLL inlines `(Y<<9)+X` and `0x3FFFF/0x40000` bounds. For a
native Phobos adoption: raise the constants in YRpp (`GetCellIndex`,
`MapClass::MaxCells`, the `CellStruct`→index helpers) and rebuild — that covers
all of Phobos's ~60+ inline sites at once. Do **not** blind-patch every `shl 9`
in a binary: Phobos contains non-cell `shl 9`s, and patching them breaks
pathfinding (verified the hard way). Ares/Antares binaries would still need the
runtime treatment (`ApplyModulePatches` in the reference repo shows a curated,
bounds-paired site classification and a version-robust pattern scanner).

## 13. Recommended hardening (optional but cheap)

The A* pathfinder's two node pools keep their counters **at the end of the
buffer** with no bounds check (allocator @`0x42A460`; pool A: 16-byte nodes,
counter at base+0x100000; pool B: 12-byte entries, counter at base+0x180000).
Pathological searches on large maps can overflow pool A, overwrite the counter,
and corrupt the heap (delayed, intermittent fatals). The buffers cannot simply be
enlarged (the game allocator won't return large contiguous blocks); clamp the
counters instead (≤0xFFFE / ≤0x1FFFE) so overflow degrades a search rather than
corrupting memory. **Reference**: `src/Hooks.cpp`, `AStar_PoolACap/PoolBCap`.

---

## Validation checklist (each item corresponds to a real regression we hit)

1. 300×300 and 500×500 maps load and start (§1, §2, §8).
2. MCV deploys; buildings place; factory exit works (§2 population).
3. Units path across the whole map including Y>256 and far corners (§4 trap, §6).
4. Slopes/ramps climbable, water impassable, harvesters auto-mine (§6).
5. Walls connect when built (§3, site 0x5887C7).
6. No black/off-palette buildings, units, or ore; sloped (cliff) maps don't crash
   (§3 false positives left untouched).
7. Shroud reveals correctly, no every-other-row striping (§3 visibility sites;
   in-house DLLs rebuilt per §12).
8. Radar/minimap works; save then load a game and check radar + movement (§10).
9. Ore grows adjacent to its spawner, not folded to the top of the map (§12 —
   this was a stale Phobos-side inline, not a gamemd site).
10. Long play session with heavy unit movement: no delayed random fatals (§13).

## Provenance

Everything above was found by disassembly of gamemd.exe 1.001 and verified
in-game across ~30 build-test cycles (two humans + AI-assisted RE; every address
independently confirmed by byte-verification before patching — the reference
patcher refuses to write when expected bytes don't match, so each site listed
here was positively identified in the shipping binary). The repo's
`docs/BUG-ATLAS.md` and commit history document each bug's full diagnosis.
