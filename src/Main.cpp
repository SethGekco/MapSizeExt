#include "MapSizeExt.h"
#include "Config.h"
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

// Called by Syringe before hooks are installed.
// This is where we read config and set global stride values.
SYRINGE_HANDSHAKE(pInfo)
{
    if (pInfo->cbSize >= sizeof(SyringeHandshakeInfo))
    {
        pInfo->Message  = "MapSizeExt loaded";
        pInfo->ExtName  = "MapSizeExt";
        pInfo->ExtVersion = "0.1";
        pInfo->ExtAuthor  = "SethGekco";
    }

    // Read MAPSIZEEXT.INI
    MapSizeConfig cfg = ReadConfig();

    g_MapStride = cfg.Stride;
    g_MapTotal  = cfg.Total();    // Stride * Stride
    g_MapMaxW   = cfg.MaxWidth;
    g_MapMaxH   = cfg.MaxHeight;

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
        fprintf(f, "MapSizeExt v0.1\n");
        fprintf(f, "Stride   = %d\n", g_MapStride);
        fprintf(f, "Total    = %d\n", g_MapTotal);
        fprintf(f, "MaxWidth = %d\n", g_MapMaxW);
        fprintf(f, "MaxHeight= %d\n", g_MapMaxH);
        fprintf(f, "Hooks: operator[] stride @ 0x%X\n", ADDR_OPERATOR_BRACKET_SHL);
        fprintf(f, "Hooks: operator[] bounds @ 0x%X\n", ADDR_OPERATOR_BRACKET_CMP);
        fprintf(f, "Hooks: alloc stride 1    @ 0x%X\n", ADDR_ALLOC_SHL_1);
        fprintf(f, "Hooks: alloc stride 2    @ 0x%X\n", ADDR_ALLOC_SHL_2);
        fprintf(f, "Hooks: inline cell access@ 0x%X\n", ADDR_INLINE_CELL_SHL);
        fprintf(f, "Hooks: lepton op[]       @ 0x%X\n", ADDR_OPERATOR_LEPTON_SHL);
        fclose(f);
    }

    return S_OK;
}
