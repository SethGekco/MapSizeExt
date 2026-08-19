#pragma once
#include <windows.h>
#include <cstdio>

// ============================================================
//  Config.h
//  Reads MAPSIZEEXT.INI from the game directory.
//
//  [MapSize]
//  PlaneScale=1      ; preferred: derives Stride and omitted limits
//  Stride=512        ; legacy cell-array row stride
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
    int PlaneScale;
    int Stride;
    int MaxWidth;
    int MaxHeight;
    int MaxDimension;   // per-axis engine gate (replaces cmp ax,0x200)
    bool GeometryConflict;

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

    // CuratedBase=1 applies Krisztiaan's 74-patch curated conversion instead of
    // our broad inline sweep (avoids the broad-sweep false-positive class that
    // gives us the wall-connect + sidebar-brightness bugs). See docs/BUG-ATLAS.md.
    // Our compiled crash-guard hooks and 300x300 size-extension stay active.
    int CuratedBase;

    // Bisection hunt for the wall false positive: leave kCellStrideSites indices
    // [StrideSkipFrom, StrideSkipTo) at 512. Edit via INI, no rebuild per step.
    int StrideSkipFrom;
    int StrideSkipTo;

    int Total() const { return Stride * Stride; }
};

inline bool IsPowerOfTwo(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

// Reject inconsistent geometry before any executable or extension bytes are
// mutated. The current research line has reviewed strides through 2048.
inline bool ValidateConfig(const MapSizeConfig& cfg, char* reason, size_t reasonSize)
{
    if (cfg.GeometryConflict)
    {
        snprintf(reason, reasonSize, "PlaneScale conflicts with explicit Stride");
        return false;
    }
    if (cfg.PlaneScale != 0 &&
        (!IsPowerOfTwo(cfg.PlaneScale) || cfg.PlaneScale > 4 ||
         cfg.Stride != 512 * cfg.PlaneScale))
    {
        snprintf(reason, reasonSize, "PlaneScale must be one of 1, 2, or 4");
        return false;
    }
    if (!IsPowerOfTwo(cfg.Stride) || cfg.Stride < 512 || cfg.Stride > 2048)
    {
        snprintf(reason, reasonSize, "Stride must be one of 512, 1024, or 2048");
        return false;
    }

    const unsigned __int64 total =
        static_cast<unsigned __int64>(cfg.Stride) * static_cast<unsigned>(cfg.Stride);
    if (total > 0x7FFFFFFFu || total * 4ull > 0xFFFFFFFFu)
    {
        snprintf(reason, reasonSize, "derived cell plane exceeds engine operand widths");
        return false;
    }

    const __int64 configuredSpan = static_cast<__int64>(cfg.MaxWidth) + cfg.MaxHeight;
    if (cfg.MaxWidth < 1 || cfg.MaxHeight < 1 || cfg.MaxDimension < 1 ||
        cfg.MaxDimension > cfg.Stride ||
        (cfg.Stride > 512 && configuredSpan > cfg.Stride))
    {
        snprintf(reason, reasonSize,
                 "map limits must be positive, fit Stride, and have MaxWidth+MaxHeight <= Stride");
        return false;
    }
    return true;
}

inline MapSizeConfig ReadConfig()
{
    MapSizeConfig cfg;
    cfg.PlaneScale  = 0;
    cfg.Stride       = 512;
    cfg.MaxWidth     = 512;
    cfg.MaxHeight    = 512;
    cfg.MaxDimension = 512;
    cfg.GeometryConflict = false;

    char iniPath[MAX_PATH];
    GetModuleFileNameA(nullptr, iniPath, MAX_PATH);
    // strip filename, append our INI name
    char* slash = strrchr(iniPath, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(iniPath, "MAPSIZEEXT.INI");

    const UINT missing = 0x80000000u;
    const UINT configuredScale = GetPrivateProfileIntA(
        "MapSize", "PlaneScale", static_cast<int>(missing), iniPath);
    const UINT configuredStride = GetPrivateProfileIntA(
        "MapSize", "Stride", static_cast<int>(missing), iniPath);
    const bool hasScale = configuredScale != missing;
    const bool hasStride = configuredStride != missing;
    if (hasScale)
    {
        cfg.PlaneScale = static_cast<int>(configuredScale);
        if (cfg.PlaneScale > 0 && cfg.PlaneScale <= 4)
            cfg.Stride = 512 * cfg.PlaneScale;
        else
            cfg.Stride = 0;
        cfg.GeometryConflict = hasStride &&
            static_cast<int>(configuredStride) != cfg.Stride;
    }
    else if (hasStride)
    {
        cfg.Stride = static_cast<int>(configuredStride);
    }

    const int derivedLogicalSquare = cfg.Stride > 0 ? cfg.Stride / 2 : 0;
    cfg.MaxWidth = GetPrivateProfileIntA(
        "MapSize", "MaxWidth", hasScale ? derivedLogicalSquare : 512, iniPath);
    cfg.MaxHeight = GetPrivateProfileIntA(
        "MapSize", "MaxHeight", hasScale ? derivedLogicalSquare : 512, iniPath);
    cfg.MaxDimension = GetPrivateProfileIntA(
        "MapSize", "MaxDimension", hasScale ? cfg.Stride : 512, iniPath);

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
    cfg.CuratedBase    = GetPrivateProfileIntA("Debug", "CuratedBase",    0, iniPath);
    cfg.StrideSkipFrom = GetPrivateProfileIntA("Debug", "StrideSkipFrom", 0, iniPath);
    cfg.StrideSkipTo   = GetPrivateProfileIntA("Debug", "StrideSkipTo",   0, iniPath);

    return cfg;
}
