#pragma once
#include <Syringe.h>

// ============================================================
//  MapSizeExt.h
//  Addresses confirmed by binary analysis of gamemd.exe
//  All VAs assume ImageBase = 0x00400000
// ============================================================

// --- MapClass::operator[] (Cell& version) -------------------
// 5656D0: function start
// 5656EA: shl eax,0x9       <- Y * 512 stride
// 5656F1: cmp eax,0x40000   <- bounds check (262144 = 512*512)
// 5656F6: jge fallback

#define ADDR_OPERATOR_BRACKET_SHL   0x5656EA
#define ADDR_OPERATOR_BRACKET_CMP   0x5656F1

// --- MapClass::operator[] (lepton/coord version) ------------
// 565757: shl edx,0x9       <- stride only (bound is dynamic already)

#define ADDR_OPERATOR_LEPTON_SHL    0x565757

// --- Heap allocation sized by N*512 -------------------------
// 48EB18: shl edx,0x9 then call 0x7C8E17 (malloc)
// 48EB35: shl ecx,0x9 (second dimension)

#define ADDR_ALLOC_SHL_1            0x48EB18
#define ADDR_ALLOC_SHL_2            0x48EB35

// --- Inline cell array access via global 0x87F924 -----------
// 483B32: shl edi,0x9

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

// ============================================================
//  Runtime configuration
//  These are set once at startup from MAPSIZEEXT.INI
// ============================================================

extern int g_MapStride;         // replaces hardcoded 512 stride
extern int g_MapTotal;          // replaces hardcoded 262144 (512*512)
extern int g_MapMaxW;           // max map width  (default 512)
extern int g_MapMaxH;           // max map height (default 512)
extern int g_MapMaxDimension;   // per-axis gate: replaces cmp ax,0x200
                                // Note: W and H are checked independently,
                                // there is NO W+H sum check in the engine.
