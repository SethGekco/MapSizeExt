#pragma once
#include <windows.h>

// ============================================================
//  Config.h
//  Reads MAPSIZEEXT.INI from the game directory.
//
//  [MapSize]
//  Stride=512        ; map grid width (must be power of 2)
//  MaxWidth=512      ; max playable width
//  MaxHeight=512     ; max playable height
//
//  To test: set Stride=1024, MaxWidth=1024, MaxHeight=1024
//  WARNING: values > 512 are untested and WILL crash unless
//  all hook sites are patched. Start with the default to
//  confirm hooks load, then increment.
// ============================================================

struct MapSizeConfig
{
    int Stride;
    int MaxWidth;
    int MaxHeight;

    int Total() const { return Stride * Stride; }
};

inline MapSizeConfig ReadConfig()
{
    MapSizeConfig cfg;
    cfg.Stride    = 512;
    cfg.MaxWidth  = 512;
    cfg.MaxHeight = 512;

    char iniPath[MAX_PATH];
    GetModuleFileNameA(nullptr, iniPath, MAX_PATH);
    // strip filename, append our INI name
    char* slash = strrchr(iniPath, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(iniPath, "MAPSIZEEXT.INI");

    cfg.Stride    = GetPrivateProfileIntA("MapSize", "Stride",    512, iniPath);
    cfg.MaxWidth  = GetPrivateProfileIntA("MapSize", "MaxWidth",  512, iniPath);
    cfg.MaxHeight = GetPrivateProfileIntA("MapSize", "MaxHeight", 512, iniPath);

    // Clamp stride: minimum 512 (default), must be > 0
    if (cfg.Stride    < 512) cfg.Stride    = 512;
    if (cfg.MaxWidth  < 1)   cfg.MaxWidth  = cfg.Stride;
    if (cfg.MaxHeight < 1)   cfg.MaxHeight = cfg.Stride;

    return cfg;
}
