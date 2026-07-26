#include "Patches.h"
#include "MapSizeExt.h"
#include <windows.h>

// ============================================================
//  Phase 2 stride sites (map-stride `shl reg,0x9` multiplies).
//  All addresses re-verified against gamemd.exe 1.001 as
//  `C1 E[0-7] 09` (shl reg,0x9). NO-OP at stride 512.
//
//  These are grouped so a site can be commented out during
//  in-game crash-hunting without disturbing the others.
// ============================================================
static const DWORD kStrideSites[] =
{
    // --- Sight/shroud reveal cluster A (0x493CF1..0x495F39) ---
    0x493CF1, 0x493E22, 0x493F66, 0x4940B1, 0x494214, 0x494361,
    0x494B95, 0x494C75, 0x494DD2, 0x494F5A, 0x4950FC, 0x49528A,
    0x495429, 0x4955CA, 0x495769, 0x49590B, 0x495A81, 0x495BF9,
    0x495D99, 0x495F39,

    // --- Sight/shroud reveal cluster B (0x497906..0x499ADC) ---
    //  High confidence (contiguous, immediately follows cluster A)
    //  but not yet verified in-game.
    0x497906, 0x497A5B, 0x497BC7, 0x497D30, 0x497EA7, 0x498021,
    0x4981A1, 0x498338, 0x498521, 0x49871E, 0x498911, 0x498B11,
    0x498D1E, 0x498F21, 0x499129, 0x4992C9, 0x499491, 0x49969C,
    0x4998B1, 0x499ADC,

    // --- Extra sight row calc ---
    0x547DC7,

    // --- Radar / minimap buffer stride ---
    0x53D49E,

    // --- Two-cell distance / path index ---
    0x429AB1, 0x429AC2,
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
    const int count = (int)(sizeof(kStrideSites) / sizeof(kStrideSites[0]));

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
        const DWORD va  = kStrideSites[i];
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
