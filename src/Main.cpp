#include "MapSizeExt.h"
#include "Config.h"
#include "Patches.h"
#include <Syringe.h>
#include <windows.h>
#include <cstdio>

// ============================================================
//  Main.cpp  -  DLL entry + one-time patch/init.
//
//  CRITICAL: SyringeHandshake runs inside Syringe.exe's OWN
//  process (during DLL negotiation), BEFORE gamemd is loaded --
//  so at that point 0x400000 is Syringe.exe, not gamemd, and any
//  byte-patch/config there is useless (or crashes). All real work
//  must happen in GAMEMD's process. Syringe injects this DLL into
//  gamemd via LoadLibrary, so DllMain(DLL_PROCESS_ATTACH) runs in
//  gamemd BEFORE its WinMain and before the hook trampolines are
//  installed -- that is where we patch. We guard on the host exe
//  name so the init does NOT run during the Syringe-side load.
// ============================================================

// Write a DWORD into the (read-only) .text section at runtime.
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

// One-time init, run in gamemd's process from DllMain.
static void ApplyInit()
{
    MapSizeConfig cfg = ReadConfig();
    g_MapStride       = cfg.Stride;
    g_MapTotal        = cfg.Total();     // Stride * Stride
    g_MapMaxW         = cfg.MaxWidth;
    g_MapMaxH         = cfg.MaxHeight;
    g_MapMaxDimension = cfg.MaxDimension;

    // IsoMapPack5 decode-buffer bump: 400x640x2 -> 768x1024x2 (fixed 3x).
    const DWORD isoW = 768, isoH = 1024, isoBytes = isoW * isoH * 2;
    PatchDword(ADDR_ISOPACK_W_IMM,     isoW);
    PatchDword(ADDR_ISOPACK_H_IMM,     isoH);
    PatchDword(ADDR_ISOPACK_BYTES_IMM, isoBytes);

    char logPath[MAX_PATH];
    GetModuleFileNameA(nullptr, logPath, MAX_PATH);
    char* slash = strrchr(logPath, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(logPath, "MapSizeExt.log");

    FILE* f = nullptr;
    fopen_s(&f, logPath, "w");
    if (f)
    {
        fprintf(f, "MapSizeExt v0.3 (init in-game via DllMain)\n");
        fprintf(f, "Stride       = %d\n", g_MapStride);
        fprintf(f, "Total        = %d\n", g_MapTotal);
        fprintf(f, "MaxDimension = %d\n", g_MapMaxDimension);
        fprintf(f, "Toggles: PatchStride=%d PatchBounds=%d PatchBoundsCmp=%d PatchRootWH=%d PatchModules=%d PatchCoord=%d PatchGuard=%d\n",
                cfg.PatchStride, cfg.PatchBounds, cfg.PatchBoundsCmp, cfg.PatchRootWH, cfg.PatchModules, cfg.PatchCoord, cfg.PatchGuard);
        fprintf(f, "IsoMapPack5 buffer patched to %ux%u (%u bytes)\n",
                isoW, isoH, isoBytes);
        if (cfg.PatchStride)  ApplyStridePatches(f); else fprintf(f, "[stride]  DISABLED via INI\n");
        if (cfg.PatchBounds)  ApplyBoundsPatches(f, cfg.PatchBoundsCmp != 0, cfg.PatchRootWH != 0); else fprintf(f, "[bounds]  DISABLED via INI\n");
        if (cfg.PatchModules) ApplyModulePatches(f);  else fprintf(f, "[dll]     DISABLED via INI\n");
        if (cfg.PatchCoord)   ApplyCoordPatches(f);   else fprintf(f, "[coord]   DISABLED via INI\n");
        if (cfg.PatchGuard)   ApplyGuardPatches(f);   else fprintf(f, "[guard]   DISABLED via INI\n");
        if (cfg.PatchAdjacency) ApplyAdjacencyPatch(f); else fprintf(f, "[adj]     DISABLED via INI\n");
        fclose(f);
    }
    else
    {
        if (cfg.PatchStride)  ApplyStridePatches(nullptr);
        if (cfg.PatchBounds)  ApplyBoundsPatches(nullptr, cfg.PatchBoundsCmp != 0, cfg.PatchRootWH != 0);
        if (cfg.PatchModules) ApplyModulePatches(nullptr);
        if (cfg.PatchCoord)   ApplyCoordPatches(nullptr);
        if (cfg.PatchGuard)   ApplyGuardPatches(nullptr);
        if (cfg.PatchAdjacency) ApplyAdjacencyPatch(nullptr);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        // Run the patches ONLY in the game process. During the Syringe-side
        // handshake load the host exe is Syringe.exe -> skip.
        char exe[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        const char* base = strrchr(exe, '\\');
        base = base ? base + 1 : exe;
        if (_strnicmp(base, "gamemd", 6) == 0)
            ApplyInit();
    }
    return TRUE;
}

// Runs in Syringe.exe's process during DLL negotiation. Report info only;
// do NOT patch or read config here (wrong process / gamemd not loaded).
SYRINGE_HANDSHAKE(pInfo)
{
    if (pInfo->cbSize >= sizeof(SyringeHandshakeInfo))
    {
        static const char msg[] = "MapSizeExt " __DATE__;
        pInfo->Message    = const_cast<char*>(msg);
        pInfo->cchMessage = sizeof(msg) - 1;
    }
    return S_OK;
}
