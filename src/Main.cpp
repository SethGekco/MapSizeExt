#include "MapSizeExt.h"
#include "Config.h"
#include "Patches.h"
#include <Syringe.h>
#include <windows.h>
#include <cstdio>
#include <cstring>

namespace {
constexpr DWORD kExpectedTimestamp = 0x3BDF544E;
constexpr DWORD kSteamFileSize = 0x0050A940;
constexpr DWORD kSteamCRC = 0xA3F19485;
constexpr DWORD kSpawnerFileSize = 0x00497110;
constexpr DWORD kSpawnerCRC = 0x098465B3;
constexpr DWORD kTesterSpawnerCRC = 0xD114B054;

bool RuntimeHostProfileSupported()
{
    const BYTE* base = reinterpret_cast<const BYTE*>(GetModuleHandleA(NULL));
    if (!base) return false;
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const IMAGE_NT_HEADERS32* nt =
        reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
    return nt->FileHeader.TimeDateStamp == kExpectedTimestamp &&
        nt->OptionalHeader.AddressOfEntryPoint == 0x003CD80F &&
        (nt->OptionalHeader.SizeOfImage == 0x00804000 ||
         nt->OptionalHeader.SizeOfImage == 0x00793000);
}

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
static bool ApplyInit()
{
    MapSizeConfig cfg = ReadConfig();
    char reason[256] = {};
    if (!ValidateConfig(cfg, reason, sizeof(reason)) || !RuntimeHostProfileSupported())
    {
        OutputDebugStringA(reason[0] ? reason : "MapSizeExt: unsupported executable profile");
        return false;
    }
    g_MapStride       = cfg.Stride;
    g_MapTotal        = cfg.Total();     // Stride * Stride
    g_MapMaxW         = cfg.MaxWidth;
    g_MapMaxH         = cfg.MaxHeight;
    g_MapMaxDimension = cfg.MaxDimension;
    g_CrashGuard      = cfg.PatchCrashGuard;
    g_StrideSkipFrom  = cfg.StrideSkipFrom;
    g_StrideSkipTo    = cfg.StrideSkipTo;
    g_CuratedBase     = cfg.CuratedBase;

    // IsoMapPack5 decode-buffer bump: original 400x640x2. The buffer is indexed
    // by (rx,ry) up to W+H-1, which for a stride-N map is <= N-1. Size it to the
    // stride (+64 margin) so it auto-scales: 768x1024 (fixed) was only enough for
    // ~W+H<=767 (300x300 rx<=599 ok; 500x500 rx<=999 OVERFLOWED at width 768).
    const DWORD isoDim  = (g_MapStride > 512 ? g_MapStride : 640u) + 64u;
    const DWORD isoW = isoDim, isoH = isoDim, isoBytes = isoW * isoH * 2;
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
        if (cfg.PlaneScale) fprintf(f, "PlaneScale  = %d\n", cfg.PlaneScale);
        fprintf(f, "Stride       = %d\n", g_MapStride);
        fprintf(f, "Total        = %d\n", g_MapTotal);
        fprintf(f, "MaxDimension = %d\n", g_MapMaxDimension);
        fprintf(f, "Toggles: PatchStride=%d PatchBounds=%d PatchBoundsCmp=%d PatchRootWH=%d PatchModules=%d PatchCoord=%d PatchGuard=%d\n",
                cfg.PatchStride, cfg.PatchBounds, cfg.PatchBoundsCmp, cfg.PatchRootWH, cfg.PatchModules, cfg.PatchCoord, cfg.PatchGuard);
        fprintf(f, "IsoMapPack5 buffer patched to %ux%u (%u bytes)\n",
                isoW, isoH, isoBytes);
        if (cfg.CuratedBase)
        {
            fprintf(f, "[mode] CuratedBase=1 -> Krisztiaan curated 74-patch conversion; broad sweep SKIPPED\n");
            ApplyCuratedBase(f);
            // His base handles Antares's stride-512 MapRevealer via his accessor
            // hook (not yet ported); until then we still need OUR Antares/Phobos
            // module patches or the shroud reveals in stripes (BUG-ATLAS 2.11).
            // These carry no wall/sidebar false positive (that is in the gamemd
            // broad sweep, which stays skipped).
            ApplyModulePatches(f);
            ApplyAntaresPatches(f);
            // Compiled crash-guard hooks remain installed via .syhks00.
        }
        else
        {
        if (cfg.PatchStride)  ApplyStridePatches(f); else fprintf(f, "[stride]  DISABLED via INI\n");
        if (cfg.PatchStride)  ApplyCellArrayPopulationStride(f); else fprintf(f, "[cellpop] DISABLED via INI\n");
        if (cfg.PatchBounds)  ApplyBoundsPatches(f, cfg.PatchBoundsCmp != 0, cfg.PatchRootWH != 0); else fprintf(f, "[bounds]  DISABLED via INI\n");
        if (cfg.PatchBounds)  ApplyOccupancyBoundPatches(f); else fprintf(f, "[occ]     DISABLED via INI\n");
        if (cfg.PatchIterator) ApplyIteratorStridePatches(f); else fprintf(f, "[iter]    DISABLED via INI\n");
        if (cfg.PatchSubzone) ApplySubzoneScalePatches(f); else fprintf(f, "[subzone] DISABLED via INI\n");
        if (cfg.PatchSubzone) ApplySubzoneMovzxPatches(f);
        ApplyAStarPoolPatches(f);
        if (cfg.PatchRadar)   ApplyRadarPatches(f);        else fprintf(f, "[radar]   DISABLED via INI\n");
        if (cfg.PatchModules) ApplyModulePatches(f);  else fprintf(f, "[dll]     DISABLED via INI\n");
        if (cfg.PatchModules) ApplyAntaresPatches(f); else fprintf(f, "[antares] DISABLED via INI\n");
        if (cfg.PatchCoord)   ApplyCoordPatches(f);   else fprintf(f, "[coord]   DISABLED via INI\n");
        if (cfg.PatchGuard)   ApplyGuardPatches(f);   else fprintf(f, "[guard]   DISABLED via INI\n");
        if (cfg.PatchAdjacency) ApplyAdjacencyPatch(f); else fprintf(f, "[adj]     DISABLED via INI\n");
        if (cfg.PatchIso)     ApplyIsoPatches(f);     else fprintf(f, "[iso]     DISABLED via INI\n");
        }
        fclose(f);
    }
    else
    {
        if (cfg.CuratedBase)
        {
            ApplyCuratedBase(nullptr);
            ApplyModulePatches(nullptr);
            ApplyAntaresPatches(nullptr);
        }
        else
        {
        if (cfg.PatchStride)  ApplyStridePatches(nullptr);
        if (cfg.PatchStride)  ApplyCellArrayPopulationStride(nullptr);
        if (cfg.PatchBounds)  ApplyBoundsPatches(nullptr, cfg.PatchBoundsCmp != 0, cfg.PatchRootWH != 0);
        if (cfg.PatchBounds)  ApplyOccupancyBoundPatches(nullptr);
        if (cfg.PatchIterator) ApplyIteratorStridePatches(nullptr);
        if (cfg.PatchSubzone) ApplySubzoneScalePatches(nullptr);
        if (cfg.PatchSubzone) ApplySubzoneMovzxPatches(nullptr);
        ApplyAStarPoolPatches(nullptr);
        if (cfg.PatchRadar)   ApplyRadarPatches(nullptr);
        if (cfg.PatchModules) ApplyModulePatches(nullptr);
        if (cfg.PatchModules) ApplyAntaresPatches(nullptr);
        if (cfg.PatchCoord)   ApplyCoordPatches(nullptr);
        if (cfg.PatchGuard)   ApplyGuardPatches(nullptr);
        if (cfg.PatchAdjacency) ApplyAdjacencyPatch(nullptr);
        if (cfg.PatchIso)     ApplyIsoPatches(nullptr);
        }
    }
    return true;
}
} // namespace

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
        if (_strnicmp(base, "gamemd", 6) == 0 && !ApplyInit()) return FALSE;
    }
    return TRUE;
}

// Runs in Syringe.exe's process during DLL negotiation. Report info only;
// do NOT patch or read config here (wrong process / gamemd not loaded).
SYRINGE_HANDSHAKE(pInfo)
{
    if (!pInfo || pInfo->cbSize < static_cast<int>(sizeof(SyringeHandshakeInfo)))
        return E_INVALIDARG;
    const bool steam = pInfo->exeFilesize == kSteamFileSize &&
        pInfo->exeTimestamp == kExpectedTimestamp && pInfo->exeCRC == kSteamCRC;
    const bool spawner = pInfo->exeFilesize == kSpawnerFileSize &&
        pInfo->exeTimestamp == kExpectedTimestamp &&
        (pInfo->exeCRC == kSpawnerCRC || pInfo->exeCRC == kTesterSpawnerCRC);
    static char accepted[] = "MapSizeExt: supported YR 1.001 profile";
    static char rejected[] = "MapSizeExt: unsupported executable profile";
    pInfo->Message = (steam || spawner) ? accepted : rejected;
    pInfo->cchMessage = static_cast<int>(strlen(pInfo->Message));
    return (steam || spawner) ? S_OK : S_FALSE;
}
