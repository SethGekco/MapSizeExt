#include "Patches.h"
#include "MapSizeExt.h"
#include "AresPhobosSites.h"
#include <windows.h>

// ============================================================
//  Phase 2 stride sites (map-stride `shl reg,0x9` multiplies).
//  All addresses re-verified against gamemd.exe 1.001 as
//  `C1 E[0-7] 09` (shl reg,0x9). NO-OP at stride 512.
//
//  These are grouped so a site can be commented out during
//  in-game crash-hunting without disturbing the others.
// ============================================================
// Cell-grid stride sites (shl reg,0x9 -> *stride). 431 CellClass* (*4) sites + 41 shroud (*1) sites; radar/isometric/bitfield excluded. NO-OP at 512.
static const DWORD kCellStrideSites[] =
{
    0x429AB1, 0x429AC2, 0x429DFA, 0x483B32, 0x493CF1, 0x493E22, 0x493F66, 0x4940B1,
    0x494214, 0x494361, 0x494B95, 0x494C75, 0x494DD2, 0x494F5A, 0x4950FC, 0x49528A,
    0x495429, 0x4955CA, 0x495769, 0x49590B, 0x495A81, 0x495BF9, 0x495D99, 0x495F39,
    0x497906, 0x497A5B, 0x497BC7, 0x497D30, 0x497EA7, 0x498021, 0x4981A1, 0x498338,
    0x498521, 0x49871E, 0x498911, 0x498B11, 0x498D1E, 0x498F21, 0x499129, 0x4992C9,
    0x499491, 0x49969C, 0x4998B1, 0x499ADC, 0x4F88D6, 0x547DC7, 0x5656EA, 0x565757,
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

    int patched = 0;
    for (int i = 0; i < count; ++i)
    {
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
int ApplyBoundsPatches(FILE* log)
{
    const DWORD newTotal = (DWORD)g_MapTotal;
    if (newTotal == 0x40000)
    {
        if (log) fprintf(log, "[bounds] cell-array limit stays 0x40000 (stride 512)  [no-op]\n");
        return 0;
    }

    const DWORD tStart = 0x00401000;   // .text start
    const DWORD tEnd   = 0x007E038D;   // .text end (VA)
    int cmpN = 0, pushN = 0;

    for (DWORD va = tStart; va < tEnd - 5; )
    {
        const BYTE op = *reinterpret_cast<BYTE*>(va);
        if ((op == 0x3D || op == 0x68) &&
            *reinterpret_cast<DWORD*>(va + 1) == 0x00040000)
        {
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
        else
        {
            va += 1;
        }
    }

    if (log) fprintf(log, "[bounds] 0x40000 -> 0x%X : cmp eax=%d, push=%d (total %d)\n",
                     newTotal, cmpN, pushN, cmpN + pushN);
    return cmpN + pushN;
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
    return grand;
}
