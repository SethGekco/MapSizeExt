#include "Patches.h"
#include "MapSizeExt.h"
#include "AresPhobosSites.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>

// ============================================================
//  Phase 2 stride sites (map-stride `shl reg,0x9` multiplies).
//  All addresses re-verified against gamemd.exe 1.001 as
//  `C1 E[0-7] 09` (shl reg,0x9). NO-OP at stride 512.
//
//  These are grouped so a site can be commented out during
//  in-game crash-hunting without disturbing the others.
// ============================================================
// Cell-grid stride sites (shl reg,0x9 -> *stride). 435 sites after removing SHP-drawer + iso lighting-remap false positives; radar/isometric/bitfield/lighting excluded. NO-OP at 512.
// EXCLUDED false-positive: 0x547DC7 is inside IsometricTileTypeClass::ReadFromFile.
//   Its `shl eax,9` is a LIGHTING-REMAP table stride (value clamped to [0,254] * 512
//   bytes-per-row) feeding ds:0xAA10D0, NOT a cell index. Patching it doubled the row
//   stride -> wrong palette (ore black / shroud blue-green) + OOB crash at 0x548DB1 on
//   any Level>0 (elevated/cliff) tile. Flat Level-0 maps (0*512==0*1024) were unaffected.
static const DWORD kCellStrideSites[] =
{
    0x429AB1, 0x429AC2, 0x429DFA, 0x483B32,
    // EXCLUDED (40): 0x493CF1..0x499ADC are the SHP object-drawer variants
    //   (normal/remap/shadow/translucent blits). Each does: scale a value, clamp it
    //   to [0,254] (`cmp reg,0xfe`), `shl reg,9` (*512 = 256-color WORD row) then
    //   `add reg,<table base>` = a LIGHTING-REMAP row pointer -- same false positive
    //   as 0x547DC7, NOT a cell index. Patching them 9->10 made revealed buildings/
    //   objects render black / wrong-palette (correct under shroud = unlit path).
    0x4F88D6, /* 0x547DC7 excluded */ 0x5656EA, 0x565757,
    0x5657AC, 0x5657F1, 0x566542, 0x566B83, 0x567704, 0x567775, 0x567C1E, 0x567C8C,
    0x567ED4, 0x568059, 0x568220, 0x568620, 0x5686EC, 0x568783, 0x568A52, 0x568D11,
    0x568D8B, 0x568E69, 0x568F1A, 0x569015, 0x56906B, 0x5690C1, 0x569238, 0x56928E,
    0x5692E4, 0x5694A3, 0x569604, 0x56964F, 0x569789, 0x56983A, 0x569935, 0x56998B,
    0x5699E1, 0x569B58, 0x569BAE, 0x569C04, 0x569DC3, 0x569F2A, 0x569F75, 0x56A1E5,
    0x56A247, 0x56A2A1, 0x56A398, 0x56A3EE, 0x56A56C, 0x56A6FA, 0x56A760, 0x56A7B9,
    0x56A873, 0x56A8D4, 0x56A92D, 0x56AA2C, 0x56ABBA, 0x56AC20, 0x56AC79, 0x56AD33,
    0x56AD94, 0x56ADED, 0x56B5F2, 0x56B688, 0x56B704, 0x56B7DB, 0x56B8E0, 0x56B9FB,
    0x56BB1B, 0x56BCDB, 0x56BDE2, 0x56BED9, 0x56BFD3, 0x56C09C, 0x56C3A4, 0x56D252,
    0x56D2BA, 0x56DC6B, 0x56DCAF, 0x56DD5E, 0x56DF74, 0x56E1B6, 0x56E3B8, 0x56E805,
    0x56E9A6, 0x56EB1C, 0x56EB96, 0x56ECD0, 0x56ED7A, 0x56EE7A, 0x56EF89, 0x56F088,
    0x56F0D6, 0x56F124, 0x56F16A, 0x56F1E6, 0x56F232, 0x56F329, 0x56F428, 0x56F476,
    0x56F4C4, 0x56F50A, 0x56F586, 0x56F5D2, 0x56F6CA, 0x56F7DA, 0x56F8E9, 0x56F9F0,
    0x56FA3D, 0x56FA8B, 0x56FAD8, 0x56FB49, 0x56FB93, 0x56FCB9, 0x56FDC0, 0x56FE0D,
    0x56FE5B, 0x56FEA8, 0x56FF19, 0x56FF63, 0x5700ED, 0x570159, 0x5701B3, 0x570209,
    0x5702D4, 0x5703EE, 0x570522, 0x57059D, 0x5705F4, 0x570649, 0x570741, 0x5707C8,
    0x57087A, 0x5708D0, 0x570926, 0x570A1E, 0x570AF2, 0x570B7E, 0x570CA5, 0x570F33,
    0x570FBF, 0x57109A, 0x57115E, 0x571288, 0x5714AC, 0x5715F2, 0x571631, 0x57169C,
    0x5716E5, 0x571733, 0x571783, 0x5717E5, 0x57182B, 0x571871, 0x571B25, 0x571B62,
    0x571BC9, 0x571C0C, 0x571C62, 0x571CB3, 0x571D1B, 0x571D5E, 0x571DAA, 0x57226A,
    0x57236A, 0x572479, 0x572578, 0x5725C6, 0x572614, 0x57265A, 0x5726D6, 0x572722,
    0x572819, 0x572918, 0x572966, 0x5729B4, 0x5729FA, 0x572A76, 0x572AC2, 0x572BBA,
    0x572CCA, 0x572DD9, 0x572EE0, 0x572F2D, 0x572F7B, 0x572FC8, 0x573039, 0x573083,
    0x5731A9, 0x5732B0, 0x5732FD, 0x57334B, 0x573398, 0x573409, 0x573453, 0x5735DE,
    0x573647, 0x5736A1, 0x5736F7, 0x5737BD, 0x5738EC, 0x573A37, 0x573ABB, 0x573B15,
    0x573B6B, 0x573C64, 0x573CEB, 0x573D9D, 0x573DF3, 0x573E49, 0x573F41, 0x5740A4,
    0x57411E, 0x574177, 0x5741DA, 0x5742B4, 0x5743D7, 0x5744C9, 0x574792, 0x5749D2,
    0x574CC0, 0x574D3A, 0x574D93, 0x574DF6, 0x574ED0, 0x574FF3, 0x5750E5, 0x5752D9,
    0x575334, 0x5754EB, 0x5755FE, 0x575656, 0x57580F, 0x575931, 0x57598F, 0x575B46,
    0x575C66, 0x575CC1, 0x575E7A, 0x575F58, 0x575FCA, 0x57602F, 0x576093, 0x5760FE,
    0x57615F, 0x576212, 0x57629E, 0x5763C5, 0x576653, 0x5766DF, 0x5767B8, 0x57687C,
    0x57699A, 0x576BB2, 0x576CD2, 0x576D19, 0x576D82, 0x576DCD, 0x576E1B, 0x576E6B,
    0x576ECD, 0x576F1E, 0x576F64, 0x577202, 0x577247, 0x5772AE, 0x5772F2, 0x577348,
    0x577399, 0x577405, 0x57744D, 0x57749A, 0x57795C, 0x5780B4, 0x578321, 0x57847B,
    0x57865F, 0x57881D, 0x57889B, 0x5789D0, 0x578A31, 0x578A74, 0x578ADB, 0x57906E,
    0x5790CC, 0x579129, 0x579186, 0x579676, 0x5796C8, 0x57974D, 0x5797A1, 0x579893,
    0x579928, 0x57997B, 0x579A1C, 0x579C14, 0x579C4D, 0x579C94, 0x579CD3, 0x579D12,
    0x579D98, 0x579DD3, 0x57A4FA, 0x57A551, 0x57A5BC, 0x57A610, 0x57A67C, 0x57A6CF,
    0x57A73B, 0x57A78E, 0x57B2EE, 0x57B51C, 0x57B810, 0x57B892, 0x57B8D1, 0x57B9B1,
    0x57B9F1, 0x57BAB2, 0x57BD11, 0x57BD52, 0x57BD93, 0x57C2D1, 0x57C312, 0x57C353,
    0x57C8AE, 0x57C928, 0x57C9CE, 0x57CA48, 0x57CAD2, 0x57CB11, 0x57CC01, 0x57CC41,
    0x57CD02, 0x57CF81, 0x57CFC2, 0x57D003, 0x57D551, 0x57D592, 0x57D5D3, 0x57DB2E,
    0x57DBAC, 0x57DC5E, 0x57DCDC, 0x57DD8F, 0x57DDDA, 0x57DE20, 0x57DFAA, 0x57E2E3,
    0x57E322, 0x57E363, 0x57E7E3, 0x57E82E, 0x57E874, 0x57E9FE, 0x57ED47, 0x57ED86,
    0x57EDC7, 0x57F212, 0x57F452, 0x57F716, 0x57F758, 0x57F794, 0x57FC3C, 0x57FC76,
    0x57FCB1, 0x58014A, 0x58018C, 0x5801C8, 0x580680, 0x5806BA, 0x5806F5, 0x580B27,
    0x580B77, 0x580F37, 0x581014, 0x581351, 0x5814AC, 0x581597, 0x58176A, 0x581A50,
    0x581BBE, 0x581C20, 0x581C95, 0x581E70, 0x582DAE, 0x582E63, 0x582EA0, 0x5834F0,
    0x5835DE, 0x583832, 0x58406E, 0x584E72, 0x5851D2, 0x585A85, 0x585B73, 0x5862F5,
    0x5863C5, 0x586451, 0x586505, 0x586590, 0x586615, 0x5867DF, 0x586A07, 0x586C92,
    0x586D89, 0x586DCF, 0x586ECD, 0x586FEB, 0x5871B0, 0x587249, 0x587279, 0x5873A6,
    0x58745A, 0x5874B8, 0x587535, 0x5875E2, 0x587679, 0x58786D, 0x5878F4, 0x58796E,
    0x5879F3, 0x587A91, 0x587B10, 0x587B82, 0x587BF7, 0x588169, 0x588256, 0x588467,
    // Vanilla NAWALL line-fill (sub_588750, reached from HouseClass::UnitFromFactory
    // @0x4FB27A for Wall=yes buildings): shl ecx,9 cell-index for the GuardRange
    // straight-line gap-fill scan. Missing here left it at Y*512+X -> the fill
    // read/wrote wrong cells -> "walls only connect adjacent, gaps don't fill" at
    // 1024 (the long-standing wall bug, present in BOTH builds). The 3 paired
    // cmp eax,0x40000 bounds in that fn are already covered by ApplyBoundsPatches.
    0x5887C7,
    // Foundation-occupancy misses in MapClass::AddContentAt / RemoveContentAt.
    // Each computes cell index Y*512+X, bounds-checks vs the RUNTIME total in
    // [this+0x140] (Cells.Capacity = MaxNumCells = 1048576, already patched), then
    // loads Cells.Items[index] via [reg+0x13c] with *4. Verified genuine cell
    // strides (not a fixed buffer/bitfield); originally excluded as "buffer".
    // These are the building placement/deploy path -- unpatched they mis-register
    // a building's footprint on the wrong cells at stride 1024.
    0x56846B, 0x56889B, 0x568B2F, 0x56A0CC,
};

// log2 for exact powers of two in [1, 2^31]; -1 if not a power of two.
static int Log2Exact(int v)
{
    if (v <= 0 || (v & (v - 1)) != 0) return -1;
    int n = 0;
    while ((v >>= 1) != 0) ++n;
    return n;
}

int ApplyStridePatches(FILE* log)
{
    const int shift = Log2Exact(g_MapStride);
    const int count = (int)(sizeof(kCellStrideSites) / sizeof(kCellStrideSites[0]));

    if (shift < 0)
    {
        if (log) fprintf(log, "[stride] ABORT: Stride %d is not a power of two\n",
                         g_MapStride);
        return 0;
    }

    if (log) fprintf(log, "[stride] %d sites, shift 0x09 -> 0x%02X (stride %d)%s\n",
                     count, shift, g_MapStride,
                     shift == 9 ? "  [no-op]" : "");

    if (shift == 9) return 0;  // stride 512: nothing to do

    if (log && g_StrideSkipTo > g_StrideSkipFrom)
        fprintf(log, "[stride] BISECT: skipping site indices [%d, %d) of %d\n",
                g_StrideSkipFrom, g_StrideSkipTo, count);

    int patched = 0, skippedBisect = 0;
    for (int i = 0; i < count; ++i)
    {
        // Bisection hunt for the wall false positive: leave these indices at 512.
        if (i >= g_StrideSkipFrom && i < g_StrideSkipTo)
        {
            ++skippedBisect;
            if (log) fprintf(log, "[stride] BISECT-SKIP [%d] 0x%06X\n", i, kCellStrideSites[i]);
            continue;
        }
        const DWORD va  = kCellStrideSites[i];
        const BYTE  op  = *reinterpret_cast<BYTE*>(va);        // expect 0xC1
        const BYTE  mod = *reinterpret_cast<BYTE*>(va + 1);    // E0..E7
        const BYTE  imm = *reinterpret_cast<BYTE*>(va + 2);    // expect 0x09

        if (op != 0xC1 || (mod & 0xF8) != 0xE0 || imm != 0x09)
        {
            if (log) fprintf(log, "[stride] SKIP 0x%06X: unexpected bytes "
                             "%02X %02X %02X\n", va, op, mod, imm);
            continue;
        }

        DWORD oldProt = 0;
        void* p = reinterpret_cast<void*>(va + 2);
        if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &oldProt))
        {
            if (log) fprintf(log, "[stride] SKIP 0x%06X: VirtualProtect failed\n", va);
            continue;
        }
        *reinterpret_cast<BYTE*>(va + 2) = (BYTE)shift;
        VirtualProtect(p, 1, oldProt, &oldProt);
        ++patched;
    }

    if (log) fprintf(log, "[stride] patched %d / %d sites\n", patched, count);
    return patched;
}

// ============================================================
//  Cell-pointer array POPULATION row stride  @ 0x566437
//  The Cells.Items pointer array is filled by a nested loop inside the
//  map-init function. Its ROW STRIDE is a hardcoded `add ecx,0x200`
//  (add 512) -- NOT a `shl reg,9`, so the stride audit never saw it.
//  The loop stores  Items[Y*512 + X] = cell  (verified: the MCV at
//  cell (63,102) has its CellClass* at Items[102*512+63]=Items[52287],
//  while operator[] now reads Items[102*1024+63]=Items[104511] = null).
//  Result at stride 1024: every COORDINATE lookup (operator[]) hits a
//  null cell -> deploy refuses, units cannot path/move, occupancy is
//  wrong; terrain still renders because rendering walks Items[] linearly.
//  Raise the row stride 0x200 -> g_MapStride so population and lookup
//  agree. NO-OP at stride 512.
//
//    0x566437:  81 C1 00 02 00 00   add ecx,0x200
//                        ^^^^^^^^^^^  imm32 @ +2  (patch to g_MapStride)
// ============================================================
int ApplyCellArrayPopulationStride(FILE* log)
{
    if (g_MapStride == 512)
    {
        if (log) fprintf(log, "[cellpop] row stride stays 512  [no-op]\n");
        return 0;
    }
    const DWORD site  = 0x566437;
    const DWORD immVA = site + 2;
    if (*reinterpret_cast<BYTE*>(site)     != 0x81 ||
        *reinterpret_cast<BYTE*>(site + 1) != 0xC1 ||
        *reinterpret_cast<DWORD*>(immVA)   != 0x200)
    {
        if (log) fprintf(log, "[cellpop] SKIP 0x%06X: unexpected bytes (%02X %02X imm=0x%X)\n",
                         site, *reinterpret_cast<BYTE*>(site),
                         *reinterpret_cast<BYTE*>(site + 1),
                         *reinterpret_cast<DWORD*>(immVA));
        return 0;
    }
    DWORD oldProt = 0; void* p = reinterpret_cast<void*>(immVA);
    if (!VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt))
    {
        if (log) fprintf(log, "[cellpop] SKIP 0x%06X: VirtualProtect failed\n", site);
        return 0;
    }
    *reinterpret_cast<DWORD*>(immVA) = (DWORD)g_MapStride;
    VirtualProtect(p, 4, oldProt, &oldProt);
    if (log) fprintf(log, "[cellpop] row stride 512 -> %d @0x%06X\n", g_MapStride, site);
    return 1;
}

// ============================================================
//  Cell-array size limit (262144 = 0x40000 = 512*512).
//  The engine inlines `cmp eax,0x40000` bounds checks all over
//  (~405 sites) plus the VectorClass reserve `push 0x40000`
//  @0x565B87. All must rise to stride*stride for a >512 grid.
//
//  We scan .text for the two instruction forms and rewrite the
//  0x40000 immediate to g_MapTotal. NO-OP at stride 512
//  (g_MapTotal stays 0x40000). This runs inside SyringeHandshake,
//  BEFORE Syringe installs its trampolines, so .text is pristine.
//
//  Forms handled:
//    3D 00 00 04 00   cmp eax,0x40000
//    68 00 00 04 00   push 0x40000
// ============================================================
int ApplyBoundsPatches(FILE* log, bool patchCmp, bool patchRootWH)
{
    const DWORD newTotal = (DWORD)g_MapTotal;
    if (newTotal == 0x40000)
    {
        if (log) fprintf(log, "[bounds] cell-array limit stays 0x40000 (stride 512)  [no-op]\n");
        return 0;
    }

    const DWORD tStart = 0x00401000;   // .text start
    const DWORD tEnd   = 0x007E038D;   // .text end (VA)
    const DWORD newDim = (DWORD)g_MapStride;   // 512->1024
    int cmpN = 0, pushN = 0, cmpSkip = 0, cmpRegN = 0, rootN = 0;

    // Of the 405 `cmp eax,0x40000` sites, 401 are followed by a cell-array
    // `[reg+eax*4]` access (genuine cell-index bound checks -> must rise).
    // These 4 are NOT (they compare a count/pointer, i.e. 256 KB buffer or
    // capacity fields) and corrupt state if bumped -> the Ares null-singleton
    // crash. Always skip them even when patchCmp is on.
    const DWORD kCmpSkip[] = { 0x565B73, 0x568710, 0x5687A7, 0x568B58 };

    // WARNING: 0x40000 is BOTH 512*512 (cell-array size) AND 256 KB, a very
    // common buffer/allocation constant. Blindly rewriting every `cmp
    // eax,0x40000` corrupts unrelated 256 KB buffers. The `push 0x40000`
    // (VectorClass reserve for the cell array) MUST rise or the array
    // overflows at stride>512; the `cmp` checks are suspect and gated by
    // patchCmp so we can bisect. The real cell-bounds checks are already
    // handled by the IsCellValid/operator[] hooks in Hooks.cpp.
    for (DWORD va = tStart; va < tEnd - 5; )
    {
        const BYTE op = *reinterpret_cast<BYTE*>(va);
        if ((op == 0x3D || op == 0x68) &&
            *reinterpret_cast<DWORD*>(va + 1) == 0x00040000)
        {
            if (op == 0x3D && !patchCmp) { ++cmpSkip; va += 5; continue; }
            if (op == 0x3D)
            {
                bool skip = false;
                for (DWORD s : kCmpSkip) if (s == va) { skip = true; break; }
                if (skip) { ++cmpSkip; va += 5; continue; }
            }
            DWORD oldProt = 0;
            void* p = reinterpret_cast<void*>(va + 1);
            if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt))
            {
                *reinterpret_cast<DWORD*>(va + 1) = newTotal;
                VirtualProtect(p, 4, oldProt, &oldProt);
                if (op == 0x3D) ++cmpN; else ++pushN;
            }
            va += 5;
        }
        // cmp reg,0x40000 (81 /7, modrm 0xF8..0xFF) -- the cell loop bounds my
        // `cmp eax` (3D) scan missed. All 37 are in cell code (verified). Only
        // when patchCmp (same class as the cmp-eax cell checks).
        else if (patchCmp && op == 0x81)
        {
            const BYTE modrm = *reinterpret_cast<BYTE*>(va + 1);
            if (modrm >= 0xF8 && modrm <= 0xFF &&
                *reinterpret_cast<DWORD*>(va + 2) == 0x00040000)
            {
                DWORD oldProt = 0;
                void* p = reinterpret_cast<void*>(va + 2);
                if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt))
                {
                    *reinterpret_cast<DWORD*>(va + 2) = newTotal;
                    VirtualProtect(p, 4, oldProt, &oldProt);
                    ++cmpRegN;
                }
                va += 6;
            }
            else va += 1;
        }
        else
        {
            va += 1;
        }
    }

    // Root map dimensioning in MapClass::Init @0x565800: the cell array is
    // allocated from these plain-immediate constants (not shl/cmp forms).
    //   mov eax,0x200  (B8) -> feeds MAP_CELL_W/H [+0x14c]/[+0x150]
    //   mov [ebp+0x154],0x40000 (C7) -> TotalCells (sizes the allocation)
    // Without these the array is only 512x512 and cells >=262144 read heap
    // garbage (0xFFFFFFFF) -> the constructor-on-bad-this crash at 0x410174.
    // `wh`=true is the MAP_CELL_W/H metadata (0x14c/0x150). RadarClass reads
    // these to size its minimap; forcing 1024 makes the radar dims degenerate
    // (<=0) -> radar surface creation is skipped -> null surface -> Antares
    // LockRadarSurfaces crashes. The array is sized by TotalCells (0x154), so
    // W/H can stay 512 for the array while total rises. Gated by patchRootWH.
    struct RootPatch { DWORD va, off, expect, nv; bool wh; };
    const RootPatch roots[] = {
        { 0x565812, 1, 0x00000200, newDim,   true  },   // mov eax,0x200 -> W/H
        { 0x565828, 6, 0x00040000, newTotal, false },   // mov [ebp+0x154],0x40000 -> total
    };
    for (const RootPatch& r : roots)
    {
        if (r.wh && !patchRootWH)
        {
            if (log) fprintf(log, "[bounds] root W/H 0x%06X kept at 512 (PatchRootWH=0)\n", r.va);
            continue;
        }
        if (*reinterpret_cast<DWORD*>(r.va + r.off) != r.expect)
        {
            if (log) fprintf(log, "[bounds] SKIP root 0x%06X: unexpected imm\n", r.va);
            continue;
        }
        DWORD oldProt = 0;
        void* p = reinterpret_cast<void*>(r.va + r.off);
        if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt))
        {
            *reinterpret_cast<DWORD*>(r.va + r.off) = r.nv;
            VirtualProtect(p, 4, oldProt, &oldProt);
            ++rootN;
        }
    }

    if (log) fprintf(log, "[bounds] 0x40000->0x%X : cmp eax=%d (skip %d), cmp reg=%d, push=%d, root dims=%d/2\n",
                     newTotal, cmpN, cmpSkip, cmpRegN, pushN, rootN);
    return cmpN + pushN + cmpRegN + rootN;
}

// ============================================================
//  Inverse cell<->coord conversion (index -> X,Y) in MapClass, used
//  by the camera, targeting and pathfinding:
//    X = index & 0x1FF   (and reg,0x1FF)   @ 0x565C88, 0x566FA4
//    Y = index >> 9      (sar reg,0x9)     @ 0x565C96, 0x566FB2
//  For a >512 grid these must be mask=(stride-1) and shift=log2(stride);
//  otherwise coords >=512 fold back to the top-left (the observed
//  bottom-right -> top-left wrap). NO-OP at stride 512.
// ============================================================
int ApplyCoordPatches(FILE* log)
{
    const int shift = Log2Exact(g_MapStride);
    if (shift < 0 || shift == 9)
    {
        if (log) fprintf(log, "[coord] inverse conv stays &0x1FF / >>9 (stride %d)  [no-op]\n",
                         g_MapStride);
        return 0;
    }
    const DWORD mask = (DWORD)g_MapStride - 1;          // 0x3FF for 1024
    const DWORD maskSites[]  = { 0x565C88, 0x566FA4 };  // and reg,0x1FF (imm32 @ +2)
    const DWORD shiftSites[] = { 0x565C96, 0x566FB2 };  // sar reg,0x9  (imm8  @ +2)
    // The div (Y = idx/512) above is only HALF of a signed div/mod-512 idiom.
    // Its paired mod (X = idx%512) uses `and reg,0x800001ff` + `or reg,
    // 0xfffffe00`. If we raise the div to /1024 but leave the mod at %512 the
    // coordinate is mangled -> a lookup returns null -> Ares crashes calling a
    // method on a null singleton (0x880a04). So patch the mod halves too.
    const DWORD modAndSites[] = { 0x565C75, 0x566F91 }; // and reg,0x800001FF (imm32 @ +2)
    const DWORD modOrSites[]  = { 0x565C7E, 0x566F9A }; // or  reg,0xFFFFFE00 (imm32 @ +2)
    const DWORD modAndNew = 0x80000000u | mask;         // keep sign bit + low log2 bits
    const DWORD modOrNew  = ~mask;                      // 0xFFFFFC00 for 1024
    int n = 0;

    for (int i = 0; i < 2; ++i)
    {
        const DWORD va = maskSites[i];
        if (*reinterpret_cast<DWORD*>(va + 2) != 0x1FF)
        {
            if (log) fprintf(log, "[coord] SKIP mask 0x%06X: not 0x1FF\n", va);
            continue;
        }
        DWORD oldProt = 0; void* p = reinterpret_cast<void*>(va + 2);
        if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt))
        {
            *reinterpret_cast<DWORD*>(va + 2) = mask;
            VirtualProtect(p, 4, oldProt, &oldProt); ++n;
        }
    }
    for (int i = 0; i < 2; ++i)
    {
        const DWORD va = shiftSites[i];
        if (*reinterpret_cast<BYTE*>(va) != 0xC1 ||
            *reinterpret_cast<BYTE*>(va + 2) != 0x09)
        {
            if (log) fprintf(log, "[coord] SKIP shift 0x%06X: unexpected bytes\n", va);
            continue;
        }
        DWORD oldProt = 0; void* p = reinterpret_cast<void*>(va + 2);
        if (VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &oldProt))
        {
            *reinterpret_cast<BYTE*>(va + 2) = (BYTE)shift;
            VirtualProtect(p, 1, oldProt, &oldProt); ++n;
        }
    }
    for (int i = 0; i < 2; ++i)   // signed mod-512 mask: and reg,0x800001FF
    {
        const DWORD va = modAndSites[i];
        if (*reinterpret_cast<DWORD*>(va + 2) != 0x800001FF)
        {
            if (log) fprintf(log, "[coord] SKIP mod-and 0x%06X: not 0x800001FF\n", va);
            continue;
        }
        DWORD oldProt = 0; void* p = reinterpret_cast<void*>(va + 2);
        if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt))
        {
            *reinterpret_cast<DWORD*>(va + 2) = modAndNew;
            VirtualProtect(p, 4, oldProt, &oldProt); ++n;
        }
    }
    for (int i = 0; i < 2; ++i)   // signed mod-512 fixup: or reg,0xFFFFFE00
    {
        const DWORD va = modOrSites[i];
        if (*reinterpret_cast<DWORD*>(va + 2) != 0xFFFFFE00)
        {
            if (log) fprintf(log, "[coord] SKIP mod-or 0x%06X: not 0xFFFFFE00\n", va);
            continue;
        }
        DWORD oldProt = 0; void* p = reinterpret_cast<void*>(va + 2);
        if (VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt))
        {
            *reinterpret_cast<DWORD*>(va + 2) = modOrNew;
            VirtualProtect(p, 4, oldProt, &oldProt); ++n;
        }
    }
    if (log) fprintf(log, "[coord] inverse conv -> mask 0x%X, shift %d, modAnd 0x%X, modOr 0x%X : patched %d/8\n",
                     mask, shift, modAndNew, modOrNew, n);
    return n;
}

// ============================================================
//  Cross-DLL cell-index patches (Ares.dll, Phobos.dll).
//  These modules have their OWN x512 cell math; if gamemd's grid is
//  1024 but theirs stays 512 they desync (e.g. Ares calls gamemd's
//  patched inverse conv with a x512 index -> crash). We patch their
//  cell sites (relative to GetModuleHandle base) to match. NO-OP at
//  stride 512. RVAs in AresPhobosSites.h (ImageBase 0x10000000).
//  Runs in DllMain AFTER Ares/Phobos are loaded (they inject before
//  MapSizeExt) but BEFORE Syringe installs trampolines -> pristine.
// ============================================================
static int PatchShiftC1(DWORD va, BYTE shift)   // C1 /r 09  ->  C1 /r shift
{
    if (*reinterpret_cast<BYTE*>(va) != 0xC1 ||
        *reinterpret_cast<BYTE*>(va + 2) != 0x09) return 0;
    DWORD old = 0; void* p = reinterpret_cast<void*>(va + 2);
    if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) return 0;
    *reinterpret_cast<BYTE*>(va + 2) = shift;
    VirtualProtect(p, 1, old, &old); return 1;
}
static int PatchImm32(DWORD va, int off, DWORD expect, DWORD nv)
{
    if (*reinterpret_cast<DWORD*>(va + off) != expect) return 0;
    DWORD old = 0; void* p = reinterpret_cast<void*>(va + off);
    if (!VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &old)) return 0;
    *reinterpret_cast<DWORD*>(va + off) = nv;
    VirtualProtect(p, 4, old, &old); return 1;
}

// ============================================================
//  Occupancy / content cell-index bounds  @ 0x568710/0x5687A7/0x568B58
//  Three `cmp eax,0x40000` sites in MapClass::AddContentAt / RemoveContentAt
//  that gate a Cells.Items[eax] cell access (eax = cell index). They were
//  wrongly classified as "buffer" bounds and skipped by ApplyBoundsPatches,
//  because the `mov reg,[Items+eax*4]` access is BEFORE the cmp (a re-validate),
//  not after. With the bound left at 0x40000, any cell whose index >= 0x40000
//  -- i.e. Y > 256 at stride 1024 -- is rejected, so a unit MOVING onto a cell
//  in the lower half of a big map (occupancy is updated via these functions)
//  gets stuck. The array is 0x100000 entries and the access already happened,
//  so raising the bound to g_MapTotal is safe. NO-OP at stride 512.
//  (This is the "can't path past the 512/Y>256 line" movement bug.)
// ============================================================
int ApplyOccupancyBoundPatches(FILE* log)
{
    const DWORD total = static_cast<DWORD>(g_MapTotal);
    if (total == 0x40000)
    {
        if (log) fprintf(log, "[occ] content cell-index bounds stay 0x40000  [no-op]\n");
        return 0;
    }
    static const DWORD sites[] = { 0x568710, 0x5687A7, 0x568B58 };
    int n = 0;
    for (int i = 0; i < 3; ++i)
        n += PatchImm32(sites[i], 1, 0x40000, total);   // imm32 is at +1 (opcode 3D)
    if (log) fprintf(log, "[occ] AddContentAt/RemoveContentAt cell bounds 0x40000->0x%X : %d/3\n",
                     total, n);
    return n;
}

// ============================================================
//  Exact-byte patches transplanted from Krisztiaan's yr-map512 handoff
//  (same code layout as our gamemd; addresses/expected bytes verified).
//  Each record is applied only if the current bytes match `expect`.
// ============================================================
struct ByteEdit { DWORD addr; int size; BYTE expect[10]; BYTE repl[10]; };
static int ApplyByteEdits(const ByteEdit* e, int count, FILE* log, const char* tag)
{
    int n = 0;
    for (int i = 0; i < count; ++i)
    {
        if (memcmp(reinterpret_cast<void*>(e[i].addr), e[i].expect, e[i].size) != 0)
        {
            if (log) fprintf(log, "[%s] SKIP 0x%06X: bytes differ\n", tag, e[i].addr);
            continue;
        }
        DWORD old = 0; void* p = reinterpret_cast<void*>(e[i].addr);
        if (!VirtualProtect(p, e[i].size, PAGE_EXECUTE_READWRITE, &old)) continue;
        memcpy(p, e[i].repl, e[i].size);
        VirtualProtect(p, e[i].size, old, &old);
        ++n;
    }
    return n;
}

// Subzone HIERARCHY SCALE: coarsen the subzone block grid from 4-cell to 8-cell
// blocks (sar 2->3, and 3->7, and 0x80000007->0x8000000F, or -8->-0x10, lea +8->
// +0x10, lea +1->+2). Halves the block-grid dimensions so subzone IDs stay within
// range on the doubled plane. Proper subzone fix (complements Subzone_SaturateID).
static const ByteEdit kSubzoneScale[] = {
    {0x5820B6,3,{0x8d,0x4d,0x01},{0x8d,0x4d,0x02}},
    {0x582272,3,{0x83,0xe2,0x03},{0x83,0xe2,0x07}},
    {0x58227E,3,{0xc1,0xf8,0x02},{0xc1,0xf8,0x03}},
    {0x582294,3,{0x83,0xe2,0x03},{0x83,0xe2,0x07}},
    {0x58229D,3,{0xc1,0xf8,0x02},{0xc1,0xf8,0x03}},
    {0x58458F,3,{0x8d,0x4d,0x01},{0x8d,0x4d,0x02}},
    {0x584A52,3,{0x83,0xe2,0x03},{0x83,0xe2,0x07}},
    {0x584A57,3,{0xc1,0xf8,0x02},{0xc1,0xf8,0x03}},
    {0x584A65,3,{0x83,0xe2,0x03},{0x83,0xe2,0x07}},
    {0x584A6A,3,{0xc1,0xf8,0x02},{0xc1,0xf8,0x03}},
    {0x584CD6,6,{0x81,0xe2,0x07,0x00,0x00,0x80},{0x81,0xe2,0x0f,0x00,0x00,0x80}},
    {0x584CDF,3,{0x83,0xca,0xf8},{0x83,0xca,0xf0}},
    {0x584CED,5,{0x25,0x07,0x00,0x00,0x80},{0x25,0x0f,0x00,0x00,0x80}},
    {0x584CF9,3,{0x83,0xc8,0xf8},{0x83,0xc8,0xf0}},
    {0x584D03,3,{0x8d,0x4f,0x08},{0x8d,0x4f,0x10}},
    {0x584D12,3,{0x8d,0x45,0x08},{0x8d,0x45,0x10}},
    {0x584E0B,3,{0x8d,0x45,0x08},{0x8d,0x45,0x10}},
};
// Stride 2048 needs the block grid coarsened ONE MORE doubling (8-cell -> 16-cell
// for the low hierarchy level, 16 -> 32 for the high level) so the subzone COUNT
// stays under the signed-16-bit limit 0x7FFF. Measured: at stride 2048 the 4->8
// scaling still produced 0x871E (34590) subzones on a 1000x1000 map -> a signed
// consumer read (movswl @0x429E9A) went negative -> C0000005 @0x429EA4. Same 17
// audited sites as kSubzoneScale, with each replacement immediate doubled again:
// lea +1->+3 (block 1<<(pass+3)), and 3->0xF (mod 16), sar 2->4 (/16), the 8-cell
// levels and 0x8000_0007->0x8000_001F / or -8->-32 / lea extent 8->32. ~1/4 the
// subzones of 4->8 -> ~8600 on 1000x1000, well under 0x7FFF (also cuts pathfinding
// cost = the severe big-map lag). Expected bytes are identical (native binary).
static const ByteEdit kSubzoneScale2048[] = {
    {0x5820B6,3,{0x8d,0x4d,0x01},{0x8d,0x4d,0x03}},
    {0x582272,3,{0x83,0xe2,0x03},{0x83,0xe2,0x0f}},
    {0x58227E,3,{0xc1,0xf8,0x02},{0xc1,0xf8,0x04}},
    {0x582294,3,{0x83,0xe2,0x03},{0x83,0xe2,0x0f}},
    {0x58229D,3,{0xc1,0xf8,0x02},{0xc1,0xf8,0x04}},
    {0x58458F,3,{0x8d,0x4d,0x01},{0x8d,0x4d,0x03}},
    {0x584A52,3,{0x83,0xe2,0x03},{0x83,0xe2,0x0f}},
    {0x584A57,3,{0xc1,0xf8,0x02},{0xc1,0xf8,0x04}},
    {0x584A65,3,{0x83,0xe2,0x03},{0x83,0xe2,0x0f}},
    {0x584A6A,3,{0xc1,0xf8,0x02},{0xc1,0xf8,0x04}},
    {0x584CD6,6,{0x81,0xe2,0x07,0x00,0x00,0x80},{0x81,0xe2,0x1f,0x00,0x00,0x80}},
    {0x584CDF,3,{0x83,0xca,0xf8},{0x83,0xca,0xe0}},
    {0x584CED,5,{0x25,0x07,0x00,0x00,0x80},{0x25,0x1f,0x00,0x00,0x80}},
    {0x584CF9,3,{0x83,0xc8,0xf8},{0x83,0xc8,0xe0}},
    {0x584D03,3,{0x8d,0x4f,0x08},{0x8d,0x4f,0x20}},
    {0x584D12,3,{0x8d,0x45,0x08},{0x8d,0x45,0x20}},
    {0x584E0B,3,{0x8d,0x45,0x08},{0x8d,0x45,0x20}},
};
int ApplySubzoneScalePatches(FILE* log)
{
    if (g_MapStride == 512) { if (log) fprintf(log, "[subzone] scale stays 4-cell  [no-op]\n"); return 0; }

    // Pick the scale by MAP SIZE, not stride (2026-08-18). The 4->8 table is
    // Krisztiaan's PROVEN set (corners fully reachable at 1024); the 4->16
    // doubling is derived and SUSPECT: at scale 16 the outermost partial block
    // is ~16 cells deep and user-observed edge cells (and the margin-12 spawn
    // positions!) refuse all pathing at 2048 -- consistent with broken zone
    // assignment in fringe blocks. 4->16 is only NEEDED when 8-cell-block
    // subzone ids would overflow the signed-16-bit cap (~1000x1000+), so use
    // the proven table whenever it fits. Map size read from spawnmap.ini
    // (written by the client before launch, same mechanism as CoordBase).
    bool use16 = false;
    if (g_MapStride >= 2048)
    {
        char ini[MAX_PATH];
        GetModuleFileNameA(nullptr, ini, MAX_PATH);
        char* s = strrchr(ini, '\\'); if (s) *(s + 1) = '\0';
        strcat_s(ini, "spawnmap.ini");
        char buf[64] = { 0 };
        GetPrivateProfileStringA("Map", "Size", "", buf, sizeof(buf), ini);
        int mx = 0, my = 0, mw = 0, mh = 0;
        if (sscanf_s(buf, "%d,%d,%d,%d", &mx, &my, &mw, &mh) == 4 && mw > 0 && mh > 0)
        {
            const int est = ((2 * mw - 1) * mh) / 64;   // ~subzone ids at 8-cell blocks
            use16 = est > 28000;                        // headroom under 0x7FFF
            if (log) fprintf(log, "[subzone] map %dx%d -> ~%d ids at scale 8\n", mw, mh, est);
        }
        else use16 = true;                              // size unknown -> overflow-safe
    }
    if (use16)
    {
        int n = ApplyByteEdits(kSubzoneScale2048, sizeof(kSubzoneScale2048)/sizeof(ByteEdit), log, "subzone");
        if (log) fprintf(log, "[subzone] hierarchy block scale 4->16 (big map) : %d/17\n", n);
        return n;
    }
    int n = ApplyByteEdits(kSubzoneScale, sizeof(kSubzoneScale)/sizeof(ByteEdit), log, "subzone");
    if (log) fprintf(log, "[subzone] hierarchy block scale 4->8 : %d/17\n", n);
    return n;
}

// RADAR/minimap surfaces. Vanilla draws the minimap into a 400x640 surface
// (buffer 400*640*2 = 0x7D000 bytes) and gates its creation on `cmp dim,512`.
// Both scale LINEARLY with the plane: at stride 1024 the surface doubled to
// 800x1280 and the gates rose to 1024; at stride 2048 they quadruple. Rewrite
// the immediates from `scale = stride/512` so the radar tracks ANY power-of-two
// stride instead of a value hardwired for the 512->1024 doubling.
//   push 0x190 (400)   -> 400*scale        surface width   @0x5FD2FD (imm @+1)
//   push 0x280 (640)   -> 640*scale        surface height  @0x5FD302 (imm @+1)
//   push 0x7D000       -> W*H*2 bytes       buffer size     @0x5FD31C (imm @+1)
//   cmp reg,0x200 (512)-> stride            per-axis gate   @0x5FD509/516/647/650 (imm @+2)
// Verify each site's CURRENT immediate equals the expected vanilla value before
// writing, so a shifted address is skipped (logged) rather than corrupted.
static bool PatchImm32(DWORD immVA, DWORD expect, DWORD nv, FILE* log, const char* tag)
{
    if (*reinterpret_cast<DWORD*>(immVA) != expect)
    {
        if (log) fprintf(log, "[radar] SKIP 0x%06X: imm 0x%X != expected 0x%X\n",
                         immVA, *reinterpret_cast<DWORD*>(immVA), expect);
        return false;
    }
    DWORD oldProt = 0;
    void* p = reinterpret_cast<void*>(immVA);
    if (!VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &oldProt)) return false;
    *reinterpret_cast<DWORD*>(immVA) = nv;
    VirtualProtect(p, 4, oldProt, &oldProt);
    (void)tag;
    return true;
}
int ApplyRadarPatches(FILE* log)
{
    if (g_MapStride == 512) { if (log) fprintf(log, "[radar] surfaces stay 512  [no-op]\n"); return 0; }
    DWORD scale = (DWORD)g_MapStride / 512u;         // 2 @1024, 4 @2048
    // Size by MAP DIMS, not just stride (2026-08-19, the 1000x1000 minimap
    // right-clip): the radar diamond needs ~(W+H) px of width on the 400-wide
    // base surface; stride/512 gave 1600 px which fits 700x700 (1400) but
    // clips 1000x1000 (2000). Read spawnmap.ini like the subzone picker does.
    {
        char ini[MAX_PATH];
        GetModuleFileNameA(nullptr, ini, MAX_PATH);
        char* s = strrchr(ini, '\\'); if (s) *(s + 1) = '\0';
        strcat_s(ini, "spawnmap.ini");
        char buf[64] = { 0 };
        GetPrivateProfileStringA("Map", "Size", "", buf, sizeof(buf), ini);
        int mx = 0, my = 0, mw = 0, mh = 0;
        if (sscanf_s(buf, "%d,%d,%d,%d", &mx, &my, &mw, &mh) == 4 && mw > 0 && mh > 0)
        {
            const DWORD need = (DWORD)(mw + mh) / 400u + 1u;   // 700x700 -> 4 (unchanged), 1000x1000 -> 6
            if (need > scale) scale = need;
        }
    }
    const DWORD surfW = 400u * scale;
    const DWORD surfH = 640u * scale;
    const DWORD bytes = surfW * surfH * 2u;          // 0x7D000 * scale^2
    const DWORD gate  = (DWORD)g_MapStride;           // per-axis dim gate
    int n = 0;
    n += PatchImm32(0x5FD2FD + 1, 0x190,   surfW, log, "radar");  // push surface W
    n += PatchImm32(0x5FD302 + 1, 0x280,   surfH, log, "radar");  // push surface H
    n += PatchImm32(0x5FD31C + 1, 0x7D000, bytes, log, "radar");  // push buffer bytes
    n += PatchImm32(0x5FD509 + 2, 0x200,   gate,  log, "radar");  // cmp esi,dim
    n += PatchImm32(0x5FD516 + 2, 0x200,   gate,  log, "radar");  // cmp edi,dim
    n += PatchImm32(0x5FD647 + 2, 0x200,   gate,  log, "radar");  // cmp esi,dim
    n += PatchImm32(0x5FD650 + 2, 0x200,   gate,  log, "radar");  // cmp edi,dim
    // Radar blit/clip helpers (0x68E8xx-0x6904xx) carry NINE more hardcoded
    // 400/640 surface-dim immediates. Left unscaled, the radar-EVENT erase
    // (the spinning under-attack rectangle) clips its restore rect to the
    // vanilla 400x640 corner of the enlarged surface -> animations outside it
    // never get erased = permanent "snail trail" patterns (user-observed on
    // 1000x1000, 2026-08-19). Scale them with the surface.
    n += PatchImm32(0x68E8B4, 0x280, surfH, log, "radar");   // mov ecx,640
    n += PatchImm32(0x68EAD8, 0x190, surfW, log, "radar");   // mov edx,400
    n += PatchImm32(0x68EAE4, 0x280, surfH, log, "radar");   // mov ecx,640
    n += PatchImm32(0x6901B8, 0x190, surfW, log, "radar");   // push 400 (clip)
    n += PatchImm32(0x6901BD, 0x280, surfH, log, "radar");   // push 640 (clip)
    n += PatchImm32(0x6901DF, 0x280, surfH, log, "radar");   // mov [esp+..],640
    n += PatchImm32(0x6901E7, 0x190, surfW, log, "radar");   // mov [esp+..],400
    n += PatchImm32(0x690449, 0x190, surfW, log, "radar");   // mov ecx,400
    n += PatchImm32(0x690460, 0x280, surfH, log, "radar");   // mov edx,640
    if (log) fprintf(log, "[radar] surface %ux%u (%u bytes), gate %u : %d/16\n",
                     surfW, surfH, bytes, gate, n);
    return n;
}

// ============================================================
//  Full-map cell-iterator stride  (the "walk on water / units can't path" bug)
//  MapClass's bounded cell iterator @0x578290 (its per-cell op is
//  CellClass::UpdatePassability @0x486A70/0x486BF0) walks the whole map with
//  stride 512 baked into BYTE-OFFSET arithmetic that the shl-9 (index) audit
//  could not see:
//    * 32 setup sites compute the iteration END bound as
//        [Map+0x118] = Cells.Items + (rows << 0xB) + 4
//      where rows<<0xB = rows * 512 * 4 bytes. At stride 1024 that must be <<0xC.
//    * the iterator's own diagonal step is `lea eax,[ebp-0x7FC]`
//      (-0x7FC = -(512-1)*4 bytes = one iso-diagonal cell). At stride 1024 it
//      must be -(1024-1)*4 = -0xFFC.
//  Left at 512, the full-map passability/movement-zone recompute scans the wrong
//  cell range, so most cells never get a valid zone: land units re-path in place
//  (can't reach their target), water is not marked impassable (units walk on it),
//  ramps/harvesters misbehave. Localized building-placement rezones still work,
//  which is why a unit could only move on a sold factory's former FOUNDATION.
//  The iterator's index recompute (shl 9 @0x578321) is already covered by the
//  main stride list. NO-OP at stride 512.
static const DWORD kIterBoundSites[] = {
    0x565D3D, 0x56648E, 0x566ADC, 0x567026, 0x568C1E, 0x568C75, 0x56D71C, 0x577B24,
    0x577C16, 0x577D05, 0x577E48, 0x577FD1, 0x57812E, 0x5781CA, 0x578375, 0x578F13,
    0x578F6F, 0x57A169, 0x57A1CE, 0x57A227, 0x57A284, 0x5855D0, 0x585770, 0x5857CB,
    0x58582D, 0x585886, 0x585DB5, 0x585E20, 0x5866E8, 0x586748, 0x587C9B, 0x588AEF,
};
int ApplyIteratorStridePatches(FILE* log)
{
    if (g_MapStride == 512)
    {
        if (log) fprintf(log, "[iter] cell-iterator stride stays 512  [no-op]\n");
        return 0;
    }
    int shiftBits = 0;
    for (int s = static_cast<int>(g_MapStride); s > 1; s >>= 1) ++shiftBits;
    const BYTE  newShift = static_cast<BYTE>(shiftBits + 2);              // 0xB -> 0xC @1024
    const DWORD newDisp  = static_cast<DWORD>(-((static_cast<int>(g_MapStride) - 1) * 4)); // -0x7FC -> -0xFFC

    int nb = 0;
    for (size_t i = 0; i < sizeof(kIterBoundSites) / sizeof(DWORD); ++i)
    {
        const DWORD va = kIterBoundSites[i];
        if (*reinterpret_cast<BYTE*>(va)       != 0xC1 ||          // shl reg,imm8
            (*reinterpret_cast<BYTE*>(va + 1) & 0xF8) != 0xE0 ||   // ModRM /4 (shl)
            *reinterpret_cast<BYTE*>(va + 2)   != 0x0B)            // imm8 == 0xB
            continue;
        DWORD old = 0; void* p = reinterpret_cast<void*>(va + 2);
        if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old)) continue;
        *reinterpret_cast<BYTE*>(va + 2) = newShift;
        VirtualProtect(p, 1, old, &old);
        ++nb;
    }
    // Iterator diagonal step @0x5782BD: lea eax,[ebp-0x7FC]  (8D 85 04 F8 FF FF)
    const int ns = PatchImm32(0x5782BD, 2, 0xFFFFF804, newDisp);

    if (log) fprintf(log, "[iter] cell-iterator stride 512->%d : bound shl 0xB->0x%X %d/32, step -0x7FC->-0x%X %d/1\n",
                     (int)g_MapStride, newShift, nb, (unsigned)(-(int)newDisp), ns);
    return nb + ns;
}

// ============================================================
//  Antares.dll cell-index patch.
//  Antares REIMPLEMENTS large parts of the engine (e.g. the shroud
//  reveal via its MapRevealer class) and looks up cells with YRpp's
//  GetCellAt -> GetCellIndex = (Y << 9) + X and MaxCells = 0x40000,
//  BOTH inlined into Antares.dll at compile time. A gamemd byte-patch
//  cannot reach them, so on a >512 grid Antares indexes Items[Y*512+X]
//  -> the every-other-row shroud scatter. We patch Antares.dll's own
//  .text the same way: 73 `shl reg,9` GetCellIndex sites -> shl shift,
//  and the paired MaxCells bounds (73x `cmp reg,0x3FFFF` + 2x
//  `cmp ,0x40000`) -> stride*stride. Verified: every shl-9 sampled is
//  `movsx <Y>; shl,9; add <X>` and pairs 1:1 with a 0x3FFFF bound.
//  Patched relative to GetModuleHandle("Antares.dll") (relocated base).
//  NO-OP at stride 512. Netcode-safe: identical on every client.
// ============================================================
// Shared cell-index patcher for a YRpp-based DLL. Each such DLL compiles
// YRpp's GetCellIndex ((Y<<9)+X) and MaxCells (0x40000) INLINE, so a gamemd
// byte-patch cannot reach them. We patch the DLL's own .text (relocated base):
//   shl[]  : `shl reg,9`  GetCellIndex sites            -> shl shift
//   cmp[]  : MaxCells bounds {rva, imm-off, 0x40000|0x3FFFF} -> total | total-1
//   andm[] : `and reg,0x1FF` inverse-conv X {rva, imm-off} -> stride-1 mask
//   sar[]  : `sar reg,9`  inverse-conv Y sites           -> sar shift
// NO-OP at 512. Netcode-safe (identical on every client).
static int ApplyDllStridePatches(const char* name,
    const DWORD* shl, int nshl,
    const DWORD (*cmp)[3], int ncmp,
    const DWORD (*andm)[2], int nand,
    const DWORD* sar, int nsar,
    FILE* log)
{
    const int shift = Log2Exact(g_MapStride);
    if (shift < 0 || shift == 9)
    {
        if (log) fprintf(log, "[dll] %-12s cell code stays x512 (stride %d)  [no-op]\n", name, g_MapStride);
        return 0;
    }
    HMODULE h = GetModuleHandleA(name);
    if (!h)
    {
        if (log) fprintf(log, "[dll] %-12s not loaded -> skip\n", name);
        return 0;
    }
    const DWORD base  = reinterpret_cast<DWORD>(h);
    const DWORD total = static_cast<DWORD>(g_MapTotal);   // stride*stride (0x100000 @1024)
    const DWORD mask  = static_cast<DWORD>(g_MapStride) - 1;
    int ns = 0, nc = 0, na = 0, nr = 0;
    for (int i = 0; i < nshl; ++i)
        ns += PatchShiftC1(base + shl[i], static_cast<BYTE>(shift));
    for (int i = 0; i < ncmp; ++i)
    {
        const DWORD oldv = cmp[i][2];                    // 0x40000 or 0x3FFFF
        const DWORD newv = (oldv == 0x40000) ? total : (total - 1);
        nc += PatchImm32(base + cmp[i][0], cmp[i][1], oldv, newv);
    }
    for (int i = 0; i < nand; ++i)
        na += PatchImm32(base + andm[i][0], andm[i][1], 0x1FF, mask);
    for (int i = 0; i < nsar; ++i)
        nr += PatchShiftC1(base + sar[i], static_cast<BYTE>(shift));
    if (log) fprintf(log, "[dll] %-12s @0x%X: shl %d/%d, cmp %d/%d, and %d/%d, sar %d/%d\n",
                     name, base, ns, nshl, nc, ncmp, na, nand, nr, nsar);
    return ns + nc + na + nr;
}

int ApplyAntaresPatches(FILE* log)
{
    return ApplyDllStridePatches("Antares.dll",
        kAntaresShl, kAntaresShl_n, kAntaresCmp, kAntaresCmp_n,
        nullptr, 0, nullptr, 0, log);
}

int ApplyPhobosPatches(FILE* log)
{
    return ApplyDllStridePatches("Phobos.dll",
        kPhobosShl2, kPhobosShl2_n, kPhobosCmp2, kPhobosCmp2_n,
        kPhobosAnd1ff2, kPhobosAnd1ff2_n, kPhobosSar2, kPhobosSar2_n, log);
}

// Phobos compiles YRpp's inline MapClass::TryGetCellAt / GetCellAt into many hooks:
//   GetCellIndex = (Y<<9)+X  (stride 512), then a MaxCells bound `cmp idx,0x3FFFF ; ja`,
//   then the cell fetch `Cells.Items[idx]` = `mov reg,[MapClass+0x13C] ; [reg+idx*4]`.
// Our curated kPhobosShl(48) was derived against an OLDER Phobos and misses every
// GetCellIndex added since (Customizable Ore Spawners was the one that folded ore to
// (X, Y/2); an audit found 12 more -- bullet trajectories, missile targeting, crate
// placement, deploy, radiation sites, save). Rather than hardcode build-specific RVAs
// (which go stale on every Phobos update -- exactly what caused this), SCAN Phobos.dll's
// .text at load for the full pattern and patch shift+bound on any site whose shift is
// STILL 0x09 (i.e. not already covered by kPhobosShl -- those run first and are left
// untouched). The three-part signature (shl9 + 0x3FFFF bound + [+0x13C] Cells.Items)
// is specific to real cell indices, so it never touches the non-cell shl9 that broke
// pathfinding when the whole list was patched blindly. Everything is byte-verified.
static int ApplyPhobosCellIndexScan(int shift, DWORD total, FILE* log)
{
    HMODULE h = GetModuleHandleA("Phobos.dll");
    if (!h) return 0;
    BYTE* base = reinterpret_cast<BYTE*>(h);
    BYTE* nt = base + *reinterpret_cast<DWORD*>(base + 0x3C);
    const int nsec = *reinterpret_cast<WORD*>(nt + 6);
    BYTE* sec = nt + 0x18 + *reinterpret_cast<WORD*>(nt + 0x14);
    BYTE* t0 = nullptr; BYTE* t1 = nullptr;
    for (int i = 0; i < nsec; ++i)
    {
        BYTE* s = sec + i * 40;
        if (memcmp(s, ".text", 5) == 0)
        {
            t0 = base + *reinterpret_cast<DWORD*>(s + 12);
            t1 = t0 + *reinterpret_cast<DWORD*>(s + 8);   // VirtualSize
            break;
        }
    }
    if (!t0) return 0;

    int n = 0;
    for (BYTE* p = t0; p + 48 < t1; ++p)
    {
        if (p[0] != 0xC1 || p[1] < 0xE0 || p[1] > 0xE7 || p[2] != 0x09) continue;  // shl reg,9 (only unpatched)
        BYTE* bnd = nullptr;                                        // cmp idx,0x3FFFF within 20 bytes
        for (BYTE* q = p + 3; q < p + 23; ++q)
            if (q[0]==0xFF && q[1]==0xFF && q[2]==0x03 && q[3]==0x00) { bnd = q; break; }
        if (!bnd) continue;
        bool cells = false;                                        // Cells.Items [reg+0x13C] within ~40 bytes
        for (BYTE* q = bnd; q + 4 < p + 48; ++q)
            if (q[0]==0x3C && q[1]==0x01 && q[2]==0x00 && q[3]==0x00) { cells = true; break; }
        if (!cells) continue;
        int did = PatchShiftC1(reinterpret_cast<DWORD>(p), static_cast<BYTE>(shift));   // 9 -> shift
        PatchImm32(reinterpret_cast<DWORD>(bnd), 0, 0x3FFFF, total - 1);                // 0x3FFFF -> stride^2-1
        n += did;
    }
    if (log) fprintf(log, "[dll] Phobos GetCellIndex scan: patched %d newer cell-index sites (ore spawner + others)\n", n);
    return n;
}

// ============================================================
//  Phobos WAYPOINT CoordBase patch (the stride-2048 spawn fix).
//
//  Phobos's waypoint rework REPLACES the vanilla [Waypoints] reader (its hook
//  returns straight to 0x68BDB3), parses every entry itself and hardcodes
//  base 1000:
//     RVA 0x800F5  Y = N/1000   (0x10624DD3 reciprocal-multiply, 17 bytes)
//     RVA 0x80109  imul eax,ebp,1000 ; ... ; sub esi,eax  -> X = N - Y*1000
//  so the engine's Scenario.Waypoints stays empty/sentinel and our gamemd
//  CoordBase hook @0x68BE0C is dead code in this flow. Proven 2026-08-17 on
//  700x700: every spawn landed at the base-1000 decode of its base-2048 value
//  (wp0 741739 -> (739,741) on-map; wp3/wp7 -> ry 1434/1433 > 1399 off-map),
//  and [coordbase] logged zero decodes.
//
//  Fix: the CnCNet client writes spawnmap.ini BEFORE launching gamemd, so at
//  init we read [MapSizeExt]CoordBase from it (absent/vanilla map -> 1000 ->
//  no-op). For base = power of two > 1000, rewrite the decode in Phobos.dll:
//     Y = N >> log2(base)   (mov eax,esi; shr eax,k; NOP-fill the magic)
//     X = N - Y*base        (imul imm 1000 -> base; existing sub unchanged)
//  Separately (any stride > 512, no CoordBase needed): the parser's inline
//  cell-index validity check (shl ebx,9 @0x80114 + cmp 0x3FFFF @0x8011E +
//  Items[+0x13C]) is in NEITHER kPhobosShl nor the scanner's matched set ->
//  patch it here. All writes byte-verified (Phobos Development Build 48);
//  a different build skips with a log line.
static int ApplyPhobosWaypointCoordBase(int shift, DWORD total, FILE* log)
{
    HMODULE h = GetModuleHandleA("Phobos.dll");
    if (!h) return 0;
    const DWORD base = reinterpret_cast<DWORD>(h);
    int n = 0;

    // -- validity-check cell index (stride fix, independent of CoordBase) --
    n += PatchShiftC1(base + 0x80114, (BYTE)shift);            // shl ebx,9 -> stride
    n += PatchImm32(base + 0x8011E, 2, 0x3FFFF, total - 1);    // bound -> stride^2-1

    // -- coord->cell inline GetCellIndex sites the scanner's strict window
    //    misses (lepton sar-8 conversion sits between the shl and the bound;
    //    found by a loose 2-part rescan, verified genuine Items[+0x13C] derefs).
    //    On a far-map coordinate the stale 0x3FFFF bound FAILS -> Phobos gets a
    //    NULL cell -> silent misbehavior (suspected click/action decode path of
    //    the 2048 "orders go to the dummy cell" bug). shl 9 -> stride, bound ->
    //    stride^2-1, byte-verified (Phobos Development Build 48).
    static const DWORD kCoordCell[][2] = {   // {shl RVA, cmp RVA}
        { 0x271D5, 0x271EB },
        { 0x6EAA3, 0x6EAB8 },
        { 0x9F307, 0x9F31D },
    };
    for (int i = 0; i < 3; ++i)
    {
        n += PatchShiftC1(base + kCoordCell[i][0], (BYTE)shift);
        n += PatchImm32(base + kCoordCell[i][1], 2, 0x3FFFF, total - 1);
    }

    // -- per-map decode base from spawnmap.ini --
    char ini[MAX_PATH];
    GetModuleFileNameA(nullptr, ini, MAX_PATH);
    char* s = strrchr(ini, '\\'); if (s) *(s + 1) = '\0';
    strcat_s(ini, "spawnmap.ini");
    const int cb = (int)GetPrivateProfileIntA("MapSizeExt", "CoordBase", 1000, ini);
    int k = -1;                                    // log2(cb) if power of two
    for (int b = 10; b <= 20; ++b) if (cb == (1 << b)) { k = b; break; }
    if (cb <= 1000 || k < 0)
    {
        if (log) fprintf(log, "[phobos-wp] spawnmap CoordBase=%d -> decode stays base-1000; validity+coordcell %d/8\n", cb, n);
        return n;
    }
    // Publish for the gamemd cell-target codec hooks (Hooks.cpp
    // CellTarget_Encode*/Decode*_CoordBase) and any other CoordBase consumer:
    // the client writes spawnmap.ini before launch, so this is per-session
    // correct and set before any scenario parsing runs.
    g_CoordBase = cb;

    // Y = N/1000 (magic) -> Y = N >> k
    static const BYTE expY[17] = { 0xB8,0xD3,0x4D,0x62,0x10,0xF7,0xEE,0xC1,0xFA,0x06,
                                   0x8B,0xC2,0xC1,0xE8,0x1F,0x03,0xC2 };
    BYTE repY[17] = { 0x8B,0xC6,0xC1,0xE8,(BYTE)k,0x90,0x90,0x90,0x90,0x90,
                      0x90,0x90,0x90,0x90,0x90,0x90,0x90 };
    void* pY = reinterpret_cast<void*>(base + 0x800F5);
    if (memcmp(pY, expY, 17) == 0)
    {
        DWORD old = 0;
        if (VirtualProtect(pY, 17, PAGE_EXECUTE_READWRITE, &old))
        {
            memcpy(pY, repY, 17);
            VirtualProtect(pY, 17, old, &old);
            ++n;
        }
    }
    // X = N - Y*1000 -> imul imm 1000 -> cb (sub stays)
    n += PatchImm32(base + 0x80109, 2, 0x3E8, (DWORD)cb);

    // Phobos's waypoint WRITER @0x8029A: imul ecx,[esi+0x16],1000; add [esi+0x14]
    // -> WriteInt("Waypoints", ...). NOT save-only: planning-mode clicks create
    // waypoints through it, and the written value resurfaces as the {type 0xB, N}
    // order target (proven: user's click at (360,365) -> N=365360 re-decoded
    // every frame by the event codec). Every reader is CoordBase now; re-base
    // the writer too or planning waypoints decode as garbage.
    n += PatchImm32(base + 0x8029A, 2, 0x3E8, (DWORD)cb);

    if (log) fprintf(log, "[phobos-wp] spawnmap CoordBase=%d -> Phobos waypoint decode Y=N>>%d, X=N-Y*%d (+validity+coordcell+wpwrite) : %d/11 sites\n",
                     cb, k, cb, n);
    return n;
}

// ============================================================
//  Planning-mode pack-base arguments (the final base-1000 encoder, 2026-08-18).
//
//  The waypoint/planning module (0x633xxx-0x63Fxxx; per-frame re-resolvers
//  0x633BF6 family caught by the DEC1 stack-scan) creates its orders through a
//  helper that receives the CELL-PACK BASE AS AN ARGUMENT: six call setups
//  `push 0x3E8` (0x63D7BD, 0x63FBED, 0x63FC49, 0x63FCA5, 0x63FD03, 0x63FD61).
//  That is why every instruction-pattern scan for x1000 math failed -- the
//  multiply lives behind a parameter. With all decoders CoordBase'd, these
//  six immediates were the last base-1000 legs: planning clicks packed
//  Y*1000+X (proven: click (369,363) -> N=363369 -> decoded (873,177)).
static int ApplyPlanningBasePatches(FILE* log)
{
    // REVERTED 2026-08-18: the six push-1000s applied (6/6) but clicks still
    // produced base-1000 targets -> these arguments are NOT the pack base
    // (likely durations/ranges); leave them vanilla to avoid side effects.
    (void)log;
    return 0;
}

// ============================================================
//  A* pathfinder node-pool overflow guard  (the delayed heap-corruption crash).
//
//  Root cause (memory mapsizeext-astar-pool-overflow; snapshot 110213): the
//  node allocator @0x42A460 keeps pool counters AT the buffer end (pool A 16B
//  nodes, counter@base+0x100000, 65,536 cap; pool B 12B, counter@base+0x180000,
//  131,072 cap) with NO bounds check; a big-map search that needs >65,536 nodes
//  writes the slot-65,536 node ONTO the counter, and the next allocation goes
//  wild -> heap corruption.
//
//  We do NOT widen the buffers: the game allocator (0x7C9442) would not return
//  a contiguous 8 MB block, so a bigger counter offset faulted at launch. The
//  fix is the counter-CAP hooks in Hooks.cpp (AStar_PoolACap/PoolBCap) which
//  clamp the node index just below capacity -> a pathological search degrades
//  (reuses the top slot) instead of corrupting the heap. Nothing to patch here.
//  v2 (2026-08-19): REAL widening via VirtualAlloc. The 2026-08-18 attempt
//  failed only because the game CRT allocator cannot serve multi-MB blocks;
//  the three ctor mallocs are now redirected to VirtualAlloc by hooks
//  (AStar_Pool{A,B}_VAlloc / AStar_HierPool_VAlloc in Hooks.cpp), and this
//  function widens everything that encodes the old geometry:
//    pool A 65,536->262,144 nodes (counter offset 0x100000->0x400000),
//    pool B 131,072->524,288    (0x180000->0x600000),
//    open-list heap slots 0x40004->0x100004 bytes / cap 0x10000->0x40000,
//    hier heap slots 0x9C44->0x4E204 / cap 0x2710->0x13880.
//  The cap hooks remain as last-resort guards at the new ceilings. Profiling
//  motive: cap-hits made big searches fail -> per-frame re-path storms
//  (stutter) + zigzag detours. NO-OP at stride 512 (hooks return 0 there).
int ApplyAStarPoolPatches(FILE* log)
{
    if (g_MapStride <= 512)
    {
        if (log) fprintf(log, "[astar]   pools stay vanilla (stride %d)\n", g_MapStride);
        return 0;
    }
    int n = 0;
    // pool A counter offset imm32s (0x100000 -> 0x400000)
    n += PatchImm32(0x42A47B, 0x100000, 0x400000, nullptr, "astar");
    n += PatchImm32(0x42A5C5, 0x100000, 0x400000, nullptr, "astar");
    n += PatchImm32(0x42A80C, 0x100000, 0x400000, nullptr, "astar");
    n += PatchImm32(0x42A842, 0x100000, 0x400000, nullptr, "astar");
    // pool A ctor init-loop node count (0x10000 -> 0x40000)
    n += PatchImm32(0x42A7F8, 0x10000, 0x40000, nullptr, "astar");
    // pool B counter offset imm32s (0x180000 -> 0x600000)
    n += PatchImm32(0x42A48E, 0x180000, 0x600000, nullptr, "astar");
    n += PatchImm32(0x42A5BB, 0x180000, 0x600000, nullptr, "astar");
    n += PatchImm32(0x42A82A, 0x180000, 0x600000, nullptr, "astar");
    n += PatchImm32(0x42A837, 0x180000, 0x600000, nullptr, "astar");
    // open-list heap: slots buffer bytes + capacity (game malloc handles 1 MB)
    n += PatchImm32(0x42A750, 0x40004, 0x100004, nullptr, "astar");
    n += PatchImm32(0x42A763, 0x10000, 0x40000, nullptr, "astar");
    // hier heap: slots buffer bytes + capacity
    n += PatchImm32(0x42A7A2, 0x9C44, 0x4E204, nullptr, "astar");
    n += PatchImm32(0x42A7B5, 0x2710, 0x13880, nullptr, "astar");
    if (log) fprintf(log, "[astar]   pools widened 4x via VirtualAlloc (A 256K, B 512K, hier 80K nodes) : %d/13 sites\n", n);
    return n;
}

// The three hottest cell-access sites, formerly Phase-1 TRAMPOLINE hooks
// (Syringe dispatch on per-cell paths = the top non-idle CPU bucket in the
// 2026-08-19 stutter profile). Now plain byte patches:
//   0x5656EA  operator[](Cell&): shl eax,9 -> log2(stride); its bound
//             cmp eax,0x40000 @0x5656F1 is covered by the broad sweep, but
//             patch it here too (verify-skip if already done) for curated mode.
//   0x565757  operator[](lepton): shl edx,9; bound is DYNAMIC ([ecx+0x140]).
//   0x5657F1  IsCellValid: shl edx,9; no immediate bound.
// Curated mode's table already rewrites 0x565757/0x5657F1 to 0x0A first;
// the expect-0x09 verify makes those skips harmless.
//  PERIODIC FULL-MAP SHROUD SWEEP THROTTLE  (the "runs fine then pauses for a
//  second" hitch, profiled 2026-08-20).
//  The main loop @0x55B2A6 runs `if (frame % 120 == 0) sub_578100()`, and that
//  function does TWO complete walks of the cell diamond: (1) redraw cells
//  flagged 0x20, (2) for EVERY cell recompute its shroud/fog tile index via
//  0x6D8700 (the 0x6D8640 family -- which samples 8 neighbours through
//  GetCellAt) and mark it dirty when it changed. Cost is O(map area) x ~8 cell
//  lookups: ~640K lookups on a vanilla 200x200 (invisible) but ~16 MILLION on
//  1000x1000 -- a ~1 s freeze every 120 frames. It is a consistency sweep;
//  interactive shroud still updates through the normal incremental reveal
//  path, so running it less often on big maps costs only how quickly a missed
//  fog edge is caught up.
//  Fix: scale the 120-frame period with map area (1x at vanilla sizes -> 8x at
//  1000x1000), so the hitch keeps its size but becomes rare. NO-OP at stride
//  512, and unchanged for maps that are not actually big.
int ApplyShroudSweepThrottle(FILE* log)
{
    if (g_MapStride <= 512)
    {
        if (log) fprintf(log, "[sweep]   full-map shroud sweep unchanged (stride %d)\n", g_MapStride);
        return 0;
    }
    char ini[MAX_PATH];
    GetModuleFileNameA(nullptr, ini, MAX_PATH);
    char* s = strrchr(ini, '\\'); if (s) *(s + 1) = '\0';
    strcat_s(ini, "spawnmap.ini");
    char buf[64] = { 0 };
    GetPrivateProfileStringA("Map", "Size", "", buf, sizeof(buf), ini);
    int mx = 0, my = 0, mw = 0, mh = 0;
    int factor = 4;                                   // unknown size -> middle ground
    if (sscanf_s(buf, "%d,%d,%d,%d", &mx, &my, &mw, &mh) == 4 && mw > 0 && mh > 0)
    {
        const int cells = (2 * mw - 1) * mh;           // iso diamond area
        factor = cells / 200000;                       // ~200x200 vanilla = 1
        if (factor < 1) factor = 1;
        if (factor > 8) factor = 8;                    // cap: >8x delays fog catch-up too long
    }
    const DWORD period = 120u * (DWORD)factor;
    const int n = PatchImm32(0x55B29D, 0x78, period, nullptr, "sweep");
    if (log) fprintf(log, "[sweep]   full-map shroud sweep every %u frames (was 120, map %dx%d) : %d/1\n",
                     period, mw, mh, n);
    return n;
}

int ApplyHotAccessorPatches(FILE* log)
{
    const int shift = Log2Exact(g_MapStride);
    if (shift < 0 || shift == 9)
    {
        if (log) fprintf(log, "[hotacc]  accessors stay x512  [no-op]\n");
        return 0;
    }
    int n = 0;
    n += PatchShiftC1(0x5656EA, static_cast<BYTE>(shift));
    n += PatchShiftC1(0x565757, static_cast<BYTE>(shift));
    n += PatchShiftC1(0x5657F1, static_cast<BYTE>(shift));
    n += PatchImm32(0x5656F1 + 1, 0x40000, (DWORD)g_MapTotal, nullptr, "hotacc");
    if (log) fprintf(log, "[hotacc]  hot cell accessors byte-patched (were trampoline hooks) : %d/4\n", n);
    return n;
}


// ============================================================
//  GENERIC co-loaded-DLL cell-index scanner  (the "spawns in the top-right /
//  off-map" bug class).
//
//  EVERY Syringe DLL built on YRpp compiles MapClass::GetCellAt/TryGetCellAt
//  INLINE as   (Y<<9)+X ; cmp idx,0x3FFFF ; Cells.Items[idx]  -- i.e. each DLL
//  carries its own private copy of the stride-512 grid. At a wider stride those
//  lookups fold to a completely different cell, and objects materialise far
//  from where the DLL asked for them (Phobos ore-spawn Y-halving 2026-08-11;
//  GiftBoxHost Host.RandomRange scatter putting GIs in the top-right corner
//  2026-08-21). The hardcoded per-DLL tables (kAresShl/kPhobosShl) only ever
//  covered the frameworks, and the Phobos-only scanner recognised just ONE of
//  the two Cells.Items idioms -- so any newly built extension DLL reintroduces
//  the bug silently.
//
//  This scans the .text of EVERY module loaded from the GAME DIRECTORY, so a
//  DLL written next week is covered without touching MapSizeExt. Both Items
//  forms are accepted:
//      [reg+0x13C]   MapClass field  (Phobos)
//      ds:0x87F924   absolute MapClass::Instance+0x13C (GiftBoxHost)
//  Sites already rewritten by the hardcoded tables carry shift != 9 and are
//  skipped, so nothing is patched twice. Requiring all three parts is what
//  keeps non-cell `shl reg,9` untouched -- patching those blindly broke
//  pathfinding once, and that lesson is baked into this signature.
static int ScanModuleCellIndex(HMODULE h, const char* name, int shift, DWORD total, FILE* log)
{
    BYTE* base = reinterpret_cast<BYTE*>(h);
    if (!base || *reinterpret_cast<WORD*>(base) != 0x5A4D) return 0;      // 'MZ'
    BYTE* nt = base + *reinterpret_cast<DWORD*>(base + 0x3C);
    if (*reinterpret_cast<DWORD*>(nt) != 0x00004550) return 0;            // 'PE\0\0'
    const int nsec = *reinterpret_cast<WORD*>(nt + 6);
    BYTE* sec = nt + 0x18 + *reinterpret_cast<WORD*>(nt + 0x14);
    BYTE* t0 = nullptr; BYTE* t1 = nullptr;
    for (int i = 0; i < nsec; ++i)
    {
        BYTE* sc = sec + i * 40;
        if (memcmp(sc, ".text", 5) == 0)
        {
            t0 = base + *reinterpret_cast<DWORD*>(sc + 12);               // VirtualAddress
            t1 = t0 + *reinterpret_cast<DWORD*>(sc + 8);                  // VirtualSize
            break;
        }
    }
    if (!t0 || t1 <= t0) return 0;

    int n = 0;
    for (BYTE* p = t0; p + 48 < t1; ++p)
    {
        if (p[0] != 0xC1 || p[1] < 0xE0 || p[1] > 0xE7 || p[2] != 0x09) continue;   // shl reg,9 (unpatched only)
        BYTE* bnd = nullptr;                                              // cmp idx,0x3FFFF within 20 bytes
        for (BYTE* q = p + 3; q < p + 23; ++q)
            if (q[0]==0xFF && q[1]==0xFF && q[2]==0x03 && q[3]==0x00) { bnd = q; break; }
        if (!bnd) continue;
        bool cells = false;                                               // Cells.Items, either idiom
        for (BYTE* q = bnd; q + 4 < p + 48; ++q)
        {
            if (q[0]==0x3C && q[1]==0x01 && q[2]==0x00 && q[3]==0x00) { cells = true; break; }  // [reg+0x13C]
            if (q[0]==0x24 && q[1]==0xF9 && q[2]==0x87 && q[3]==0x00) { cells = true; break; }  // ds:0x87F924
        }
        if (!cells) continue;
        const int did = PatchShiftC1(reinterpret_cast<DWORD>(p), static_cast<BYTE>(shift));
        PatchImm32(reinterpret_cast<DWORD>(bnd), 0, 0x3FFFF, total - 1);
        if (did && log)
            fprintf(log, "[dll]   %s +0x%X: inline GetCellIndex -> stride %d\n",
                    name, static_cast<DWORD>(p - base), g_MapStride);
        n += did;
    }
    return n;
}

// Scans every module loaded from the game directory (skipping gamemd itself and
// this DLL). Returns total sites patched.
static int ApplyAllModuleCellScans(int shift, DWORD total, FILE* log)
{
    char dir[MAX_PATH];
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* s = strrchr(dir, '\\');
    if (!s) return 0;
    *(s + 1) = '\0';
    const size_t dlen = strlen(dir);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snap == INVALID_HANDLE_VALUE)
    {
        if (log) fprintf(log, "[dll] module snapshot failed -> generic cell scan skipped\n");
        return 0;
    }
    MODULEENTRY32 me;
    me.dwSize = sizeof(me);
    const HMODULE self = GetModuleHandleA("MapSizeExt.dll");
    const HMODULE exe  = GetModuleHandleA(nullptr);
    int grand = 0, mods = 0;
    if (Module32First(snap, &me))
    {
        do
        {
            if (me.hModule == self || me.hModule == exe) continue;
            if (_strnicmp(me.szExePath, dir, dlen) != 0) continue;         // game dir only
            const int n = ScanModuleCellIndex(me.hModule, me.szModule, shift, total, log);
            if (n) ++mods;
            grand += n;
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
    if (log) fprintf(log, "[dll] generic cell-index scan: %d site(s) across %d game DLL(s)\n", grand, mods);
    return grand;
}

// Syringe injects DLLs in the -i= command-line order, so every extension DLL
// listed AFTER MapSizeExt (GiftBoxHost, TraitExt, ...) is simply NOT LOADED when
// our DllMain runs -- the init-time scan cannot see it, and its inlined
// stride-512 cell math survives. Proven 2026-08-21: the init scan logged
// "72 site(s) across 1 game DLL(s)" (Antares only) while GiftBoxHost kept
// spawning units in the top-right corner. This re-runs the scan once from
// WinMain, by which point Syringe has loaded every injected DLL. Sites the init
// pass already patched carry shift != 9 and are skipped, so it is idempotent.
int ApplyLateModuleCellScan(FILE* log)
{
    const int shift = Log2Exact(g_MapStride);
    if (shift < 0 || shift == 9) return 0;              // stride 512 -> nothing to do
    return ApplyAllModuleCellScans(shift, static_cast<DWORD>(g_MapTotal), log);
}

int ApplyModulePatches(FILE* log)
{
    const int shift = Log2Exact(g_MapStride);
    if (shift < 0 || shift == 9)
    {
        if (log) fprintf(log, "[dll] Ares/Phobos cell code stays x512 (stride %d)  [no-op]\n",
                         g_MapStride);
        return 0;
    }
    const DWORD total = (DWORD)g_MapTotal;
    const DWORD mask  = (DWORD)g_MapStride - 1;

    struct Mod {
        const char* name;
        const DWORD* shl;  int nshl;
        const DWORD* cmp;  int ncmp;
        const DWORD* andm; int nand;
        const DWORD* sar;  int nsar;
    };
    const Mod mods[] = {
        { "Ares.dll",   kAresShl,   kAresShl_n,   kAresCmp40,   kAresCmp40_n,
                        0, 0, 0, 0 },
        { "Phobos.dll", kPhobosShl, kPhobosShl_n, kPhobosCmp40, kPhobosCmp40_n,
                        kPhobosAnd1ff, kPhobosAnd1ff_n, kPhobosSar9, kPhobosSar9_n },
    };

    int grand = 0;
    for (int m = 0; m < 2; ++m)
    {
        const Mod& M = mods[m];
        HMODULE h = GetModuleHandleA(M.name);
        if (!h)
        {
            if (log) fprintf(log, "[dll] %s not loaded -> skip\n", M.name);
            continue;
        }
        const DWORD base = reinterpret_cast<DWORD>(h);
        int ns = 0, nc = 0, na = 0, nr = 0;
        for (int i = 0; i < M.nshl; ++i) ns += PatchShiftC1(base + M.shl[i], (BYTE)shift);
        for (int i = 0; i < M.ncmp; ++i) nc += PatchImm32(base + M.cmp[i], 1, 0x40000, total);
        for (int i = 0; i < M.nand; ++i)
        {
            const DWORD va = base + M.andm[i];
            const BYTE op = *reinterpret_cast<BYTE*>(va);
            if (op == 0x25)      na += PatchImm32(va, 1, 0x1FF, mask);  // and eax,0x1FF
            else if (op == 0x81) na += PatchImm32(va, 2, 0x1FF, mask);  // and reg,0x1FF
        }
        for (int i = 0; i < M.nsar; ++i) nr += PatchShiftC1(base + M.sar[i], (BYTE)shift);
        if (log) fprintf(log, "[dll] %s @0x%X: shl %d/%d, cmp %d/%d, and %d/%d, sar %d/%d\n",
                         M.name, base, ns, M.nshl, nc, M.ncmp, na, M.nand, nr, M.nsar);
        grand += ns + nc + na + nr;
    }
    grand += ApplyPhobosCellIndexScan(shift, total, log);
    grand += ApplyAllModuleCellScans(shift, total, log);   // any co-loaded DLL (GiftBoxHost etc.)   // ore spawner + 12 other newer GetCellIndex sites
    grand += ApplyPhobosWaypointCoordBase(shift, total, log);  // the 2048 spawn fix (per-map base from spawnmap.ini)
    grand += ApplyPlanningBasePatches(log);                     // planning-order pack-base args (g_CoordBase set above)
    return grand;
}

// ============================================================
//  Render-guard patch (0x657CF0).
//  DisplayClass coordinate-transform picks between two virtual
//  methods based on the global 0xB73550 (a DDraw/window resource
//  object, non-null on large maps):
//     0xB73550 == 0 -> call vtable+0x78  (safe)
//     0xB73550 != 0 -> call vtable+0x7C  (Ares override that
//                       dereferences the .bss singleton 0x880A04,
//                       which NOTHING ever initializes -> null ->
//                       crash at Ares+0x4DEEA)
//  Because 0x880A04 is never valid, the +0x7C branch is unusable;
//  force the safe +0x78 path by NOP-ing the guard `jne` (75 4B).
//  NO byte change unless the bytes match exactly.
// ============================================================
int ApplyGuardPatches(FILE* log)
{
    const DWORD va = 0x657CF0;               // jne 0x657D3D
    BYTE* p = reinterpret_cast<BYTE*>(va);
    if (p[0] != 0x75 || p[1] != 0x4B)
    {
        if (log) fprintf(log, "[guard] SKIP 0x%06X: expected 75 4B, got %02X %02X\n",
                         va, p[0], p[1]);
        return 0;
    }
    DWORD oldProt = 0;
    if (VirtualProtect(p, 2, PAGE_EXECUTE_READWRITE, &oldProt))
    {
        p[0] = 0x90; p[1] = 0x90;            // nop; nop  -> always take safe path
        VirtualProtect(p, 2, oldProt, &oldProt);
        if (log) fprintf(log, "[guard] 0x%06X jne->nop: forced safe +0x78 render path\n", va);
        return 1;
    }
    return 0;
}

// ============================================================
//  Cell-adjacency offset table @ 0x7E3774 (Stride > 512).
//  The engine's 8-neighbour cell offsets are a static .rdata table:
//    [-512,-511,1,513,512,511,-1,-513]  (N,NE,E,SE,S,SW,W,NW @ stride 512)
//  Ore/gem spread, some pathfinding and other neighbour walks add
//  these to a cell index. Each entry is row*512 + col (col in -1..1);
//  at stride 1024 it must be row*1024 + col, else "the cell below" (+512)
//  points into the wrong row -> ore scatters, adjacency breaks.
//  Verify the exact vanilla values before writing; NO-OP at stride 512.
// ============================================================
int ApplyAdjacencyPatch(FILE* log)
{
    if (g_MapStride == 512)
    {
        if (log) fprintf(log, "[adj] neighbour table stays 512  [no-op]\n");
        return 0;
    }
    const DWORD va = 0x007E3774;
    int* const table = reinterpret_cast<int*>(va);
    const int expected[8] = { -512, -511, 1, 513, 512, 511, -1, -513 };
    for (int i = 0; i < 8; ++i)
        if (table[i] != expected[i])
        {
            if (log) fprintf(log, "[adj] SKIP: table[%d]=%d != %d (unexpected)\n",
                             i, table[i], expected[i]);
            return 0;
        }
    DWORD oldProt = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(va), 32, PAGE_EXECUTE_READWRITE, &oldProt))
        return 0;
    for (int i = 0; i < 8; ++i)
    {
        const int O   = expected[i];
        const int row = (O >= 256) ? (O + 256) / 512
                      : (O <= -256) ? (O - 256) / 512 : 0;      // -1,0,+1
        table[i] = O + row * (g_MapStride - 512);               // row*stride + col
    }
    VirtualProtect(reinterpret_cast<void*>(va), 32, oldProt, &oldProt);
    if (log) fprintf(log, "[adj] neighbour table -> stride %d: "
                     "[%d,%d,%d,%d,%d,%d,%d,%d]\n", g_MapStride,
                     table[0],table[1],table[2],table[3],table[4],table[5],table[6],table[7]);
    return 8;
}

// ============================================================
//  Isometric/tactical-render cell sites (Stride > 512) -- EXPERIMENTAL.
//  Six `shl reg,0x9` sites in the tactical renderer (0x4D0xxx) were
//  originally excluded as "screen projection", but each is preceded by
//  lepton->cell (sar reg,0x8 = /256) and feeds an index into the buffer
//  at global 0x8b3cc4 -- i.e. they are CELL-index computations, not
//  pixel math. At stride>512 leaving them at *512 mis-indexes the
//  tactical cell buffer -> top-right dead cells + flicker. Same
//  shift-immediate rewrite as the main stride sites. Gated behind
//  [Debug] PatchIso (default 0) because a misclassification here would
//  garble the whole view -> flip it on to test, off to revert.
// ============================================================
static const DWORD kIsoStrideSites[] =
{
    0x4D0BBA, 0x4D0DF5, 0x4D1062, 0x4D1553, 0x4D16FA, 0x4D2682,
};

int ApplyIsoPatches(FILE* log)
{
    const int shift = Log2Exact(g_MapStride);
    const int count = (int)(sizeof(kIsoStrideSites) / sizeof(kIsoStrideSites[0]));
    if (shift < 0 || shift == 9)
    {
        if (log) fprintf(log, "[iso] tactical cell sites stay *512 (stride %d)  [no-op]\n",
                         g_MapStride);
        return 0;
    }
    int patched = 0;
    for (int i = 0; i < count; ++i)
    {
        const DWORD va = kIsoStrideSites[i];
        if (*reinterpret_cast<BYTE*>(va) != 0xC1 ||
            (*reinterpret_cast<BYTE*>(va + 1) & 0xF8) != 0xE0 ||
            *reinterpret_cast<BYTE*>(va + 2) != 0x09)
        {
            if (log) fprintf(log, "[iso] SKIP 0x%06X: unexpected bytes\n", va);
            continue;
        }
        DWORD oldProt = 0; void* p = reinterpret_cast<void*>(va + 2);
        if (VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &oldProt))
        {
            *reinterpret_cast<BYTE*>(va + 2) = (BYTE)shift;
            VirtualProtect(p, 1, oldProt, &oldProt);
            ++patched;
        }
    }
    if (log) fprintf(log, "[iso] tactical cell sites -> stride %d : patched %d/%d\n",
                     g_MapStride, patched, count);
    return patched;
}
