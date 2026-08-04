#pragma once
#include <windows.h>

// ============================================================
//  Config.h
//  Reads MAPSIZEEXT.INI from the game directory.
//
//  [MapSize]
//  Stride=512        ; cell-array row stride (power of 2, >=512)
//  MaxDimension=512  ; per-axis engine gate (replaces cmp ax,0x200)
//  MaxWidth=512      ; informational
//  MaxHeight=512     ; informational
//
//  WARNING: Stride > 512 also requires the Phase 2 sight/shroud
//  and radar sites to be patched, or large maps will render with
//  broken fog of war. Raise MaxDimension alone (Stride left at 512)
//  to load maps with W or H up to 511 without touching Phase 2.
// ============================================================

struct MapSizeConfig
{
    int Stride;
    int MaxWidth;
    int MaxHeight;
    int MaxDimension;   // per-axis engine gate (replaces cmp ax,0x200)

    // Bisection toggles (all default 1 = on). Let a tester disable an
    // individual patch group from the INI to isolate a crash without a
    // rebuild. [Debug] PatchStride/PatchBounds/PatchModules/PatchCoord=0
    int PatchStride;
    int PatchBounds;      // patch the cell-array reserve (push 0x40000)
    int PatchBoundsCmp;   // ALSO patch the 405 suspect `cmp eax,0x40000` sites
    int PatchRootWH;      // patch MAP_CELL_W/H (0x14c/0x150) to stride; 0 keeps 512 (radar-safe)
    int PatchModules;     // Ares.dll + Phobos.dll cross-DLL cell patches
    int PatchCoord;       // gamemd inverse index->(X,Y) conversion
    int PatchGuard;       // force safe render path @0x657CF0 (avoid null 0x880A04)
    int PatchAdjacency;   // 8-neighbour cell-offset table @0x7E3774 (ore spread etc.)
    int PatchIterator;    // full-map cell-iterator byte-stride (walk-on-water/pathing fix)
    int PatchCrashGuard;  // GetCellAt garbage-slot guard (flying-unit crash fix)
    int PatchSubzone;     // subzone hierarchy block scale 4->8 (Krisztiaan handoff)
    int PatchRadar;       // radar/minimap surface resize 512->1024 (Krisztiaan handoff)
    int PatchIso;         // EXPERIMENTAL tactical-render iso cell sites (default 0)

    int Total() const { return Stride * Stride; }
};

inline MapSizeConfig ReadConfig()
{
    MapSizeConfig cfg;
    cfg.Stride       = 512;
    cfg.MaxWidth     = 512;
    cfg.MaxHeight    = 512;
    cfg.MaxDimension = 512;

    char iniPath[MAX_PATH];
    GetModuleFileNameA(nullptr, iniPath, MAX_PATH);
    // strip filename, append our INI name
    char* slash = strrchr(iniPath, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(iniPath, "MAPSIZEEXT.INI");

    cfg.Stride       = GetPrivateProfileIntA("MapSize", "Stride",       512, iniPath);
    cfg.MaxWidth     = GetPrivateProfileIntA("MapSize", "MaxWidth",     512, iniPath);
    cfg.MaxHeight    = GetPrivateProfileIntA("MapSize", "MaxHeight",    512, iniPath);
    cfg.MaxDimension = GetPrivateProfileIntA("MapSize", "MaxDimension", 512, iniPath);

    cfg.PatchStride    = GetPrivateProfileIntA("Debug", "PatchStride",    1, iniPath);
    cfg.PatchBounds    = GetPrivateProfileIntA("Debug", "PatchBounds",    1, iniPath);
    cfg.PatchBoundsCmp = GetPrivateProfileIntA("Debug", "PatchBoundsCmp", 1, iniPath);
    cfg.PatchRootWH    = GetPrivateProfileIntA("Debug", "PatchRootWH",    1, iniPath);
    cfg.PatchModules   = GetPrivateProfileIntA("Debug", "PatchModules",   1, iniPath);
    cfg.PatchCoord     = GetPrivateProfileIntA("Debug", "PatchCoord",     1, iniPath);
    cfg.PatchGuard     = GetPrivateProfileIntA("Debug", "PatchGuard",     1, iniPath);
    cfg.PatchAdjacency = GetPrivateProfileIntA("Debug", "PatchAdjacency", 1, iniPath);
    cfg.PatchIterator  = GetPrivateProfileIntA("Debug", "PatchIterator",  1, iniPath);
    cfg.PatchCrashGuard= GetPrivateProfileIntA("Debug", "PatchCrashGuard",1, iniPath);
    cfg.PatchSubzone   = GetPrivateProfileIntA("Debug", "PatchSubzone",   1, iniPath);
    cfg.PatchRadar     = GetPrivateProfileIntA("Debug", "PatchRadar",     1, iniPath);
    cfg.PatchIso       = GetPrivateProfileIntA("Debug", "PatchIso",       0, iniPath);

    // Clamp: never go below the vanilla values.
    if (cfg.Stride       < 512) cfg.Stride       = 512;
    if (cfg.MaxDimension < 512) cfg.MaxDimension = 512;
    if (cfg.MaxWidth     < 1)   cfg.MaxWidth     = cfg.Stride;
    if (cfg.MaxHeight    < 1)   cfg.MaxHeight    = cfg.Stride;

    return cfg;
}
