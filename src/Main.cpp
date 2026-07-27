#include "MapSizeExt.h"
#include "Config.h"
#include "Patches.h"
#include <Syringe.h>
#include <windows.h>
#include <cstdio>

// ============================================================
//  Main.cpp
//  DLL entry point and Syringe handshake.
// ============================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(hModule);
    return TRUE;
}

// Write a DWORD into the (read-only) .text section at runtime.
// gamemd's code pages are PAGE_EXECUTE_READ, so a raw store would
// fault; unprotect, write, then restore the original protection.
static void PatchDword(DWORD va, DWORD value)
{
    DWORD old = 0;
    if (VirtualProtect(reinterpret_cast<void*>(va), sizeof(DWORD),
                       PAGE_EXECUTE_READWRITE, &old))
    {
        *reinterpret_cast<DWORD*>(va) = value;
        VirtualProtect(reinterpret_cast<void*>(va), sizeof(DWORD), old, &old);
    }
}

// Called by Syringe before hooks are installed.
// This is where we read config and set global stride values.
SYRINGE_HANDSHAKE(pInfo)
{
    if (pInfo->cbSize >= sizeof(SyringeHandshakeInfo))
    {
        static const char msg[] = "MapSizeExt " __DATE__;
        pInfo->Message    = const_cast<char*>(msg);
        pInfo->cchMessage = sizeof(msg) - 1;
    }

    // Read MAPSIZEEXT.INI
    MapSizeConfig cfg = ReadConfig();

    g_MapStride       = cfg.Stride;
    g_MapTotal        = cfg.Total();    // Stride * Stride
    g_MapMaxW         = cfg.MaxWidth;
    g_MapMaxH         = cfg.MaxHeight;
    g_MapMaxDimension = cfg.MaxDimension;

    // ----------------------------------------------------------------
    //  IsoMapPack5 decode-buffer size extension.
    //  Three matched immediate operands in gamemd.exe 1.001:
    //
    //    push 0x190 (400)             @ imm 0x4AD344  buffer width
    //    push 0x280 (640)             @ imm 0x4AD349  buffer height
    //    push 0x7D000 (400*640*2)     @ imm 0x4AD357  buffer byte size
    //
    //  (The research notes mislabelled the third value as a WORD
    //   "2000"; it is really the DWORD 512000 = W*H*2.)
    //
    //  Bump ~3x to 768 x 1024, keeping byte size = W*H*2 consistent:
    //    768 * 1024 * 2 = 1,572,864 = 0x180000
    //
    //  This is a FIXED headroom bump, not tied to MaxDimension. It
    //  covers maps whose IsoMapPack5 payload fits a 768x1024 grid.
    // ----------------------------------------------------------------
    const DWORD isoW     = 768;
    const DWORD isoH     = 1024;
    const DWORD isoBytes = isoW * isoH * 2;   // 1,572,864
    PatchDword(ADDR_ISOPACK_W_IMM,     isoW);
    PatchDword(ADDR_ISOPACK_H_IMM,     isoH);
    PatchDword(ADDR_ISOPACK_BYTES_IMM, isoBytes);

    // Debug log to game directory
    char logPath[MAX_PATH];
    GetModuleFileNameA(nullptr, logPath, MAX_PATH);
    char* slash = strrchr(logPath, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(logPath, "MapSizeExt.log");

    FILE* f = nullptr;
    fopen_s(&f, logPath, "w");
    if (f)
    {
        fprintf(f, "MapSizeExt v0.2\n");
        fprintf(f, "Stride       = %d\n", g_MapStride);
        fprintf(f, "Total        = %d\n", g_MapTotal);
        fprintf(f, "MaxDimension = %d\n", g_MapMaxDimension);
        fprintf(f, "MaxWidth     = %d\n", g_MapMaxW);
        fprintf(f, "MaxHeight    = %d\n", g_MapMaxH);
        fprintf(f, "NOTE: engine gates W and H independently - no W+H sum check\n");
        fprintf(f, "IsoMapPack5 buffer patched to %ux%u (%u bytes)\n",
                isoW, isoH, isoBytes);
        fprintf(f, "Cell-grid hooks: bracket@0x%X lepton@0x%X iscellvalid@0x%X\n",
                ADDR_OPERATOR_BRACKET_SHL, ADDR_OPERATOR_LEPTON_SHL, ADDR_ISCELLVALID_SHL);
        fprintf(f, "Alloc hooks: dim@0x%X midrow@0x%X   inline@0x%X\n",
                ADDR_ALLOC_DIM_LOAD, ADDR_ALLOC_SHL_2, ADDR_INLINE_CELL_SHL);
        fprintf(f, "Dimension gates: 0x%X 0x%X 0x%X\n",
                ADDR_DIMGATE_SITE1, ADDR_DIMGATE_SITE2, ADDR_DIMGATE_SITE3);

        // Phase 2: sight/radar/distance stride sites + cell-array size
        // limits (both no-op at 512).
        ApplyStridePatches(f);
        ApplyBoundsPatches(f);

        fclose(f);
    }
    else
    {
        ApplyStridePatches(nullptr);
        ApplyBoundsPatches(nullptr);
    }

    return S_OK;
}
