#pragma once
#include <cstdio>

// ============================================================
//  Patches.h  -  Phase 2 stride expansion via shift-immediate
//  byte patching.
//
//  Every map-stride multiply in the engine is `shl reg,0x9`
//  (x512). Because the stride is always a power of two, raising
//  it only requires rewriting the shift immediate byte:
//      0x09 (x512) -> 0x0A (x1024) -> 0x0B (x2048) ...
//  This is register/base agnostic and adds zero runtime cost,
//  unlike a trampoline hook.
//
//  ApplyStridePatches() is a NO-OP when g_MapStride == 512 (the
//  new shift equals the original 0x09). It verifies each site's
//  opcode/immediate before writing and logs every action, so a
//  mis-identified address is skipped rather than corrupting code.
// ============================================================

// Rewrites the shift immediate at every Phase 2 sight/radar/distance
// stride site to match g_MapStride. Writes a report to `log` (may be
// null). Returns the number of sites successfully patched.
int ApplyStridePatches(FILE* log);

// Cell-pointer array population row stride (add ecx,0x200 @0x566437 -> stride).
// NO-OP at 512. Without this, Items[] is filled at Y*512+X while operator[]
// reads Y*1024+X -> every coordinate lookup hits a null cell (deploy/move fail).
int ApplyCellArrayPopulationStride(FILE* log);

// Rewrites every cell-array size limit (0x40000 = 512*512) in .text --
// the inlined `cmp eax,0x40000` bounds checks and the `push 0x40000`
// VectorClass reserve -- to g_MapTotal (stride*stride). NO-OP at stride
// 512. Returns the number of immediates patched.
int ApplyBoundsPatches(FILE* log, bool patchCmp, bool patchRootWH);

// Patches 3 cell-index bounds (cmp eax,0x40000) in AddContentAt/RemoveContentAt
// that ApplyBoundsPatches wrongly skipped -> without this, units cannot update
// occupancy on cells with index>=0x40000 (Y>256 @ stride 1024) = stuck moving
// toward the bottom of big maps. NO-OP at 512.
int ApplyOccupancyBoundPatches(FILE* log);

// Full-map cell-iterator stride: 32 `shl reg,0xB` byte-offset bounds -> 0xC and
// the iterator's `lea [ebp-0x7FC]` diagonal step -> -0xFFC. Fixes the broken
// full-map passability/movement-zone recompute (walk-on-water, can't-path). 512-safe.
int ApplyIteratorStridePatches(FILE* log);

// Subzone hierarchy block scale 4->8 (Krisztiaan handoff): keeps subzone IDs in
// range on big maps. Radar surface resize 512->1024: enables the minimap.
int ApplySubzoneScalePatches(FILE* log);
// A* node-pool 8x widening (heap-corruption fix); no-op at stride 512.
int ApplyAStarPoolPatches(FILE* log);
int ApplyRadarPatches(FILE* log);

// Fixes the inverse index->(X,Y) conversions (and 0x1FF / sar 9) so
// coordinates >=512 don't wrap to the top-left. NO-OP at stride 512.
int ApplyCoordPatches(FILE* log);

// Patches Ares.dll and Phobos.dll's own cell-index code (relative to
// their GetModuleHandle base) to match gamemd's stride. NO-OP at 512.
int ApplyModulePatches(FILE* log);

// Re-runs the generic co-loaded-DLL cell-index scan from WinMain, so DLLs that
// Syringe injects AFTER MapSizeExt are covered too. Idempotent; no-op at 512.
int ApplyLateModuleCellScan(FILE* log);

// The three hottest cell-access sites (0x5656EA/0x565757/0x5657F1), formerly
// trampoline hooks; now byte patches. Call in BOTH broad and curated modes.
int ApplyHotAccessorPatches(FILE* log);

// Throttles the every-120-frame full-map shroud/fog consistency sweep
// (0x578100) by map area -- the periodic ~1s hitch on big maps. NO-OP at 512.
int ApplyShroudSweepThrottle(FILE* log);

// Antares.dll has its OWN compiled-in stride 512 (YRpp GetCellIndex Y<<9 +
// MaxCells 0x40000), inlined 73x. Patch its .text like gamemd so Antares-
// reimplemented features (shroud reveal via MapRevealer, etc.) use stride 1024.
// NO-OP at 512. Patched relative to GetModuleHandle("Antares.dll").
int ApplyAntaresPatches(FILE* log);

// Phobos.dll also compiles stride 512 inline. The old kPhobosShl (48) was
// INCOMPLETE for the current Phobos build (73 shl-9 sites) -> unpatched cell
// lookups (e.g. Phobos voxel LightingFix) stayed at 512 = black voxels. This
// patches the complete current-build set. NO-OP at 512.
int ApplyPhobosPatches(FILE* log);

// Rewrites the static 8-neighbour cell-adjacency offset table @0x7E3774
// ([-512,-511,1,513,512,511,-1,-513]) to the current stride (row*stride+col).
// Fixes ore/gem spread and index-based neighbour walks. NO-OP at 512.
int ApplyAdjacencyPatch(FILE* log);

// EXPERIMENTAL: rewrite the 6 tactical-render iso cell sites (0x4D0xxx) to
// the stride. Gated by [Debug] PatchIso (default 0). NO-OP at 512.
int ApplyIsoPatches(FILE* log);

// Forces the safe DisplayClass render path at 0x657CF0 (avoids the Ares
// +0x7C override that dereferences the never-initialized 0x880A04).
int ApplyGuardPatches(FILE* log);

// Applies Krisztiaan's curated 74-patch map-512 conversion (+14 subzone movzx,
// +2 Phobos), the false-positive-free alternative to our broad inline sweep.
// Used when [Debug] CuratedBase=1. See src/CuratedBase.cpp and docs/BUG-ATLAS.md.
int ApplyCuratedBase(FILE* log);

// The 14 subzone-id movsx->movzx consumer patches (unsigned 16-bit id
// namespace). Applied in BOTH curated and broad modes; no-op at stride 512.
int ApplySubzoneMovzxPatches(FILE* log);
