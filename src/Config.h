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

    // Clamp: never go below the vanilla values.
    if (cfg.Stride       < 512) cfg.Stride       = 512;
    if (cfg.MaxDimension < 512) cfg.MaxDimension = 512;
    if (cfg.MaxWidth     < 1)   cfg.MaxWidth     = cfg.Stride;
    if (cfg.MaxHeight    < 1)   cfg.MaxHeight    = cfg.Stride;

    return cfg;
}
