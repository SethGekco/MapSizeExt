#pragma once
#include <Syringe.h>

// ============================================================
//  MapSizeExt.h
//  Addresses confirmed by binary analysis of gamemd.exe 1.001
//  (re-verified against the shipped binary with objdump).
//  All VAs assume ImageBase = 0x00400000.
// ============================================================

// --- MapClass::operator[] (Cell& version) -------------------
// 5656EA: shl eax,0x9       Y * 512 stride   (hook, 7 bytes: shl+add+js)
// 5656F1: cmp eax,0x40000   bounds (262144 = 512*512)  (folded into hook)
#define ADDR_OPERATOR_BRACKET_SHL   0x5656EA

// --- MapClass::operator[] (lepton/coord version) ------------
// 565757: shl edx,0x9       stride only (bound at [ecx+0x140] is dynamic)
#define ADDR_OPERATOR_LEPTON_SHL    0x565757

// --- IsCellValid --------------------------------------------
// 5657F1: shl edx,0x9 ; add edx,eax
#define ADDR_ISCELLVALID_SHL        0x5657F1

// --- Shroud/visibility buffer allocation (N * stride) -------
// 48EB12: mov edx,[esi+0x16c] ; 48EB18: shl edx,0x9 ; push ; malloc
// 48EB35: shl ecx,0x9 ; add ecx,eax  (mid-row pointer)
#define ADDR_ALLOC_DIM_LOAD         0x48EB12
#define ADDR_ALLOC_SHL_2            0x48EB35

// --- Inline cell array access via global 0x87F924 -----------
// 483B32: shl edi,0x9  (add edi,ecx happens later at 483B40)
#define ADDR_INLINE_CELL_SHL        0x483B32

// --- MapClass internal offsets (confirmed from disasm) -------
// [this+0x13C] = VectorClass<CellClass*>::data  (Array data pointer)
// [this+0x140] = stored total cell count        (used by lepton variant)
#define MAPCLASS_ARRAY_DATA_OFFSET  0x13C
#define MAPCLASS_ARRAY_SIZE_OFFSET  0x140

// --- Static sentinel cell (out-of-bounds fallback) ----------
#define ADDR_SENTINEL_CELL          0xABDC50
#define ADDR_SENTINEL_STORE         0xABDC74

// --- Global cell array pointer used by inline access --------
#define ADDR_GLOBAL_CELL_ARRAY      0x87F924

// --- Per-axis dimension gate (three sites) ------------------
#define ADDR_DIMGATE_SITE1          0x4C5630
#define ADDR_DIMGATE_SITE2          0x4C590E
#define ADDR_DIMGATE_SITE3          0x554BC5

// ============================================================
//  IsoMapPack5 decode-buffer immediate operands (in .text)
//  push 0x190 (400)   @ imm 0x4AD344  -> buffer width
//  push 0x280 (640)   @ imm 0x4AD349  -> buffer height
//  push 0x7D000 (512000 = 400*640*2)  @ imm 0x4AD357 -> byte size
//  Patched as DWORD immediates (see Main.cpp), guarded by
//  VirtualProtect because .text is read-only at runtime.
// ============================================================
#define ADDR_ISOPACK_W_IMM          0x4AD344
#define ADDR_ISOPACK_H_IMM          0x4AD349
#define ADDR_ISOPACK_BYTES_IMM      0x4AD357

// ============================================================
//  Runtime configuration (set once at startup from MAPSIZEEXT.INI)
// ============================================================
extern int g_MapStride;         // replaces hardcoded 512 stride
extern int g_MapTotal;          // replaces hardcoded 262144 (512*512)
extern int g_MapMaxW;           // max map width  (default 512)
extern int g_MapMaxH;           // max map height (default 512)
extern int g_MapMaxDimension;   // per-axis gate: replaces cmp ax,0x200.
extern int g_CrashGuard;        // 1 = GetCellAt garbage-slot guard active (INI PatchCrashGuard)

// Bisection: skip kCellStrideSites indices [g_StrideSkipFrom, g_StrideSkipTo)
// in ApplyStridePatches (leave them at stride 512), to hunt the wall false
// positive by binary search via INI without rebuilding. 0,0 = skip nothing.
extern int g_StrideSkipFrom;
extern int g_StrideSkipTo;

// 1 = CuratedBase mode (Krisztiaan's byte patches drive cell math). Our compiled
// accessor/alloc hooks defer to his patches then (they otherwise fight his
// conversion); our dimension-gate + crash-guard hooks stay active as the 300x300
// size extension he lacks. See docs/BUG-ATLAS.md 2.12/2.13.
extern int g_CuratedBase;
                                // W and H are checked independently;
                                // there is NO W+H sum check in the engine.
