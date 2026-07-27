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

// Rewrites every cell-array size limit (0x40000 = 512*512) in .text --
// the inlined `cmp eax,0x40000` bounds checks and the `push 0x40000`
// VectorClass reserve -- to g_MapTotal (stride*stride). NO-OP at stride
// 512. Returns the number of immediates patched.
int ApplyBoundsPatches(FILE* log);
