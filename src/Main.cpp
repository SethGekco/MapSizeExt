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

static void GameDir(char (&out)[MAX_PATH])
{
    GetModuleFileNameA(nullptr, out, MAX_PATH);
    char* slash = strrchr(out, '\\');
    if (slash) *(slash + 1) = '\0';
}

// ------------------------------------------------------------
//  Which map is this launch loading? The CnCNet client writes spawn.ini and
//  the scenario it names (normally spawnmap.ini) before it starts gamemd, so
//  both are on disk when DllMain runs. One pass over the scenario collects the
//  two things the vanilla engine cannot exceed:
//    [Map] Size=x,y,W,H  - cells run 1..W+H-1 on both axes (measured on real and
//                          generated maps), so the grid stride must be >= W+H.
//    [IsoMapPack5]       - gamemd reads the base64-decoded (still LZO-packed)
//                          bytes into a 400*640*2 = 512,000-byte buffer
//                          (0x4AD343..0x4AD687); more than that does not fit.
// ------------------------------------------------------------
struct ScenarioSize
{
    bool  known = false;
    int   x = 0, y = 0, w = 0, h = 0;
    DWORD isoBytes = 0;              // base64-decoded [IsoMapPack5] length
    char  name[MAX_PATH] = "";       // scenario file, for the log
    const char* why = "";            // why the size is unknown
};

static ScenarioSize ReadScenarioSize()
{
    ScenarioSize ss;
    char dir[MAX_PATH];
    GameDir(dir);

    char spawn[MAX_PATH];
    strcpy_s(spawn, dir);
    strcat_s(spawn, "spawn.ini");
    if (GetFileAttributesA(spawn) == INVALID_FILE_ATTRIBUTES) { ss.why = "no spawn.ini"; return ss; }

    // A saved-game load names no real map: the client writes a 50x50 stub
    // spawnmap.ini and the map comes out of the save file.
    char yn[8] = "";
    GetPrivateProfileStringA("Settings", "LoadSaveGame", "", yn, sizeof(yn), spawn);
    if (yn[0] && strchr("YyTt1", yn[0])) { ss.why = "saved-game load"; return ss; }

    GetPrivateProfileStringA("Settings", "Scenario", "spawnmap.ini", ss.name, sizeof(ss.name), spawn);
    char path[MAX_PATH];
    strcpy_s(path, dir);
    strcat_s(path, ss.name);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f) { ss.why = "scenario not a loose file"; return ss; }  // e.g. a campaign map in a MIX
    setvbuf(f, nullptr, _IOFBF, 1 << 16);

    char line[4096];
    bool lineStart = true;
    int  section = 0;                // 1 = [Map], 2 = [IsoMapPack5]
    DWORD b64 = 0;
    while (fgets(line, sizeof(line), f))
    {
        const size_t len = strlen(line);
        const bool first = lineStart;           // false = tail of an over-long line
        lineStart = len && line[len - 1] == '\n';
        const char* p = line;
        if (first)
        {
            while (*p == ' ' || *p == '\t') ++p;
            if (*p == '[')
            {
                section = !_strnicmp(p, "[Map]", 5) ? 1 : !_strnicmp(p, "[IsoMapPack5]", 13) ? 2 : 0;
                continue;
            }
            if (section == 1 && !ss.known && !_strnicmp(p, "Size=", 5))
                ss.known = sscanf_s(p + 5, "%d,%d,%d,%d", &ss.x, &ss.y, &ss.w, &ss.h) == 4
                           && ss.w > 0 && ss.h > 0;
            if (section != 2 || !(p = strchr(p, '='))) continue;
            ++p;                                // value of "N=<base64>"
        }
        else if (section != 2) continue;
        for (; *p; ++p)
            if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') || *p == '+' || *p == '/')
                ++b64;
    }
    fclose(f);
    ss.isoBytes = b64 * 3 / 4;
    if (!ss.known) ss.why = "no [Map] Size in scenario";
    return ss;
}

// Smallest grid stride that holds the map; 512 = vanilla = the DLL stays inert.
static int StrideForMap(const ScenarioSize& ss)
{
    if (!ss.known) return 512;
    const int need = ss.w + ss.h;
    if (need <= 512 && ss.isoBytes <= 512000) return 512;
    return need <= 1024 ? 1024 : 2048;
}

// One-time init, run in gamemd's process from DllMain.
static void ApplyInit()
{
    MapSizeConfig cfg = ReadConfig();
    const ScenarioSize map = ReadScenarioSize();

    // The decision, as the first (on a vanilla-size map, the only) log line.
    char decision[768];
    char mapDesc[400];               // scenario name (<= MAX_PATH) + ~80 chars
    if (map.known)
        sprintf_s(mapDesc, "%s map %d,%d,%d,%d (W+H=%d, IsoMapPack5 %lu bytes)", map.name,
                  map.x, map.y, map.w, map.h, map.w + map.h, map.isoBytes);
    else
        sprintf_s(mapDesc, "map size unknown (%s)", map.why);
    if (cfg.Auto)
    {
        cfg.Stride = cfg.MaxDimension = cfg.MaxWidth = cfg.MaxHeight = StrideForMap(map);
        if (cfg.Stride == 512)
            sprintf_s(decision, "MapSizeExt: %s fits vanilla (W+H <= 512, IsoMapPack5 <= 512000) "
                                "-> stride 512, NO patches, every hook passes through", mapDesc);
        else
            sprintf_s(decision, "MapSizeExt: %s exceeds vanilla -> stride %d, patch set applied%s", mapDesc,
                      cfg.Stride, map.w + map.h > 2048 ? "  WARNING: W+H > 2048, the largest stride -- will not load" : "");
    }
    else
        sprintf_s(decision, "MapSizeExt: [MapSize] Auto=0 -> stride %d from MAPSIZEEXT.INI for %s -> %s",
                  cfg.Stride, mapDesc, cfg.Stride > 512 ? "patch set applied" : "NO patches, every hook passes through");

    g_Active          = cfg.Stride > 512;
    g_MapStride       = cfg.Stride;
    g_MapTotal        = cfg.Total();     // Stride * Stride
    g_MapMaxW         = cfg.MaxWidth;
    g_MapMaxH         = cfg.MaxHeight;
    g_MapMaxDimension = cfg.MaxDimension;
    g_CrashGuard      = cfg.PatchCrashGuard;
    g_StrideSkipFrom  = cfg.StrideSkipFrom;
    g_StrideSkipTo    = cfg.StrideSkipTo;
    g_DiagVerbose     = cfg.DiagVerbose;
    g_RadarEvents     = cfg.RadarEvents;
    g_ReuseSteps      = cfg.ReuseSteps < 0 ? 0 : cfg.ReuseSteps;
    g_ReuseABPeriod   = cfg.ReuseABPeriod < 0 ? 0 : cfg.ReuseABPeriod;
    g_CuratedBase     = cfg.CuratedBase;

    char logPath[MAX_PATH];
    GameDir(logPath);
    strcat_s(logPath, "MapSizeExt.log");

    FILE* f = nullptr;
    fopen_s(&f, logPath, "w");
    if (f) fprintf(f, "%s\n", decision);

    // Vanilla-size map: touch nothing -- no byte patch, no crash watcher.
    if (!g_Active)
    {
        if (f) fclose(f);
        return;
    }

    // IsoMapPack5 read-buffer bump: original 400x640x2 = 512,000 bytes, and it
    // holds the base64-decoded (still packed) section, so it is the section's
    // byte length that must fit. Size it to the stride (+64 margin), then grow
    // it if this map's data needs more (a 500x500 map can carry 2.4 MB, past
    // stride 1024's 1088x1088x2).
    DWORD isoDim = (g_MapStride > 512 ? g_MapStride : 640u) + 64u;
    while (isoDim * isoDim * 2 < map.isoBytes) isoDim += 64u;
    const DWORD isoW = isoDim, isoH = isoDim, isoBytes = isoW * isoH * 2;
    PatchDword(ADDR_ISOPACK_W_IMM,     isoW);
    PatchDword(ADDR_ISOPACK_H_IMM,     isoH);
    PatchDword(ADDR_ISOPACK_BYTES_IMM, isoBytes);

    // Arm the crash watcher before any patching, so a fault during init is
    // reported too. It only observes -- see CrashWatch.cpp.
    InstallCrashWatch();

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
        fprintf(f, "Path continuation: ReuseSteps = %d%s", g_ReuseSteps, g_ReuseSteps ? "" : " (off)");
        if (g_ReuseABPeriod) fprintf(f, ", ABPeriod = %d (TEST: hand-outs only in even blocks)", g_ReuseABPeriod);
        fprintf(f, "\n");
        if (cfg.CuratedBase)
        {
            fprintf(f, "[mode] CuratedBase=1 -> Krisztiaan curated 74-patch conversion; broad sweep SKIPPED\n");
            ApplyCuratedBase(f);
            ApplyHotAccessorPatches(f);
            ApplyShroudSweepThrottle(f);
            ApplyAntaresIniParseFix(f, cfg.AntaresIniFix);
            // His base handles Antares's stride-512 MapRevealer via his accessor
            // hook (not yet ported); until then we still need OUR Antares/Phobos
            // module patches or the shroud reveals in stripes (BUG-ATLAS 2.11).
            // These carry no wall/sidebar false positive (that is in the gamemd
            // broad sweep, which stays skipped).
            ApplyModulePatches(f);
            ApplyAntaresPatches(f);
            if (cfg.PatchThreat) ApplyThreatRegionPatches(f);
            // Compiled crash-guard hooks remain installed via .syhks00.
        }
        else
        {
        if (cfg.PatchStride)  ApplyStridePatches(f); else fprintf(f, "[stride]  DISABLED via INI\n");
        if (cfg.PatchStride)  ApplyHotAccessorPatches(f);
        ApplyShroudSweepThrottle(f);
        ApplyAntaresIniParseFix(f, cfg.AntaresIniFix);
        if (cfg.PatchStride)  ApplyCellArrayPopulationStride(f); else fprintf(f, "[cellpop] DISABLED via INI\n");
        if (cfg.PatchBounds)  ApplyBoundsPatches(f, cfg.PatchBoundsCmp != 0, cfg.PatchRootWH != 0); else fprintf(f, "[bounds]  DISABLED via INI\n");
        if (cfg.PatchBounds)  ApplyOccupancyBoundPatches(f); else fprintf(f, "[occ]     DISABLED via INI\n");
        if (cfg.PatchIterator) ApplyIteratorStridePatches(f); else fprintf(f, "[iter]    DISABLED via INI\n");
        if (cfg.PatchSubzone) ApplySubzoneScalePatches(f); else fprintf(f, "[subzone] DISABLED via INI\n");
        if (cfg.PatchSubzone) ApplySubzoneMovzxPatches(f);
        if (cfg.PatchThreat)  ApplyThreatRegionPatches(f); else fprintf(f, "[threat]  DISABLED via INI\n");
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
            ApplyHotAccessorPatches(nullptr);
            ApplyShroudSweepThrottle(nullptr);
            ApplyAntaresIniParseFix(nullptr, cfg.AntaresIniFix);
            ApplyModulePatches(nullptr);
            ApplyAntaresPatches(nullptr);
            if (cfg.PatchThreat) ApplyThreatRegionPatches(nullptr);
        }
        else
        {
        if (cfg.PatchStride)  ApplyStridePatches(nullptr);
        if (cfg.PatchStride)  ApplyHotAccessorPatches(nullptr);
        ApplyShroudSweepThrottle(nullptr);
        ApplyAntaresIniParseFix(nullptr, cfg.AntaresIniFix);
        if (cfg.PatchStride)  ApplyCellArrayPopulationStride(nullptr);
        if (cfg.PatchBounds)  ApplyBoundsPatches(nullptr, cfg.PatchBoundsCmp != 0, cfg.PatchRootWH != 0);
        if (cfg.PatchBounds)  ApplyOccupancyBoundPatches(nullptr);
        if (cfg.PatchIterator) ApplyIteratorStridePatches(nullptr);
        if (cfg.PatchSubzone) ApplySubzoneScalePatches(nullptr);
        if (cfg.PatchSubzone) ApplySubzoneMovzxPatches(nullptr);
        if (cfg.PatchThreat)  ApplyThreatRegionPatches(nullptr);
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
