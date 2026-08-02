#include "MapSizeExt.h"
#include "Config.h"
#include <Syringe.h>
#include <windows.h>
#include <cstdio>
#include <cstdarg>

// ============================================================
//  Hooks.cpp  -  Phase 1: cell-grid stride, bounds and the
//  per-axis dimension gate.
//
//  IMPORTANT register API note:
//    This YRpp exposes registers as METHODS on REGISTERS* R:
//      read : R->EAX(), R->EAX<int>()
//      write: R->EAX(value)
//    (NOT `R->EAX = value` - that does not compile.)
//
//  Syringe hook return semantics:
//    The `size` bytes at the hook address are overwritten with a
//    5-byte JMP + NOP padding. The address you `return` must NOT
//    fall inside [hook, hook+size) or you land in the trampoline.
//    Return an address >= hook+size, or a completely different one.
//
//  On a standard 512-stride map every hook below reduces to the
//  original behaviour, so the DLL is a no-op until the INI raises
//  Stride / MaxDimension above 512.
// ============================================================

// Global runtime values - set in SyringeHandshake (Main.cpp)
int g_MapStride       = 512;
int g_MapTotal        = 262144;  // 512 * 512
int g_MapMaxW         = 512;
int g_MapMaxH         = 512;
int g_MapMaxDimension = 512;     // per-axis gate (replaces cmp ax,0x200)

// ============================================================
//  HOOK A: MapClass::operator[](Cell&)  @ 0x5656EA (7 bytes)
//    5656EA: shl eax,0x9      Y * 512      \
//    5656ED: add eax,ecx      + X           } 7 stolen bytes
//    5656EF: js  0x565709     negative     /
//    5656F1: cmp eax,0x40000  (not stolen - we fold it in)
//    5656F6: jge 0x565709
//    5656F8: mov edx,[esp+8]  <- resume target on success
//  Entry: eax = Y (movsx), ecx = X (movsx).
// ============================================================
// Diagnostic: dump MapClass::Instance's real dimension/rect fields once we are
// deep enough into gameplay that the map is fully loaded, so we can compare the
// map's ACTUAL runtime layout against our x1024 metadata. Offsets below are
// derived from YRpp/MapClass.h (verified against the +0x14C MaxNumCells anchor):
//   MapRect        @ 0xEC  (X,Y,Width,Height)
//   VisibleRect    @ 0xFC  (X,Y,Width,Height)
//   MapCoordBounds @ 0x124 (Left,Top,Right,Bottom)
//   Cells vector   @ 0x138 (Items,Capacity,IsAllocated)
//   MaxWidth 0x144 / MaxHeight 0x148 / MaxNumCells 0x14C
// Trigger by call-count (unconditional) rather than a "map ready" guess -- the
// previous field-guard version never fired, so we no longer trust any single
// field to signal readiness. operator[] is called constantly in-game, so this
// lands well after load. Writes MapSizeExt_diag.log in the game dir.
static void DumpMapStateOnce()
{
    static bool done = false;
    if (done || g_MapStride <= 512) return;

    const DWORD m = 0x87F7E8;          // MapClass::Instance object base
    auto I = [m](DWORD off) { return *reinterpret_cast<int*>(m + off); };
    auto U = [m](DWORD off) { return *reinterpret_cast<DWORD*>(m + off); };

    // Readiness: cell array allocated. Dump on the FIRST post-load call -- the
    // hot cell access is the inlined byte-patched shl sites (no hook), so the
    // hooked accessors are cold and a call-count threshold never trips.
    if (U(0x138) == 0 || I(0x14C) <= 0) return;   // Cells.Items==0 / MaxNumCells<=0
    done = true;

    // Absolute path in the game exe dir (CWD at gameplay-time is NOT the exe dir,
    // which is why the earlier relative-path dumps never appeared).
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "MapSizeExt_diag.log");

    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (!f) return;
    fprintf(f, "MapSizeExt diag (first post-load cell access)\n");
    fprintf(f, "g_MapStride=%d  g_MapTotal=%d\n\n", g_MapStride, g_MapTotal);
    fprintf(f, "MapRect        X=%d Y=%d W=%d H=%d\n",   I(0xEC), I(0xF0), I(0xF4), I(0xF8));
    fprintf(f, "VisibleRect    X=%d Y=%d W=%d H=%d\n",   I(0xFC), I(0x100), I(0x104), I(0x108));
    fprintf(f, "MapCoordBounds L=%d T=%d R=%d B=%d\n",   I(0x124), I(0x128), I(0x12C), I(0x130));
    fprintf(f, "Cells.Items=0x%08X Capacity=%d IsAllocated=%d\n", U(0x138), I(0x13C), I(0x140) & 0xFF);
    fprintf(f, "MaxWidth=%d  MaxHeight=%d  MaxNumCells=%d\n\n", I(0x144), I(0x148), I(0x14C));
    fprintf(f, "-- raw window 0xE0..0x158 --\n");
    for (DWORD off = 0xE0; off <= 0x158; off += 4)
        fprintf(f, "  [+0x%03X] = %11d  (0x%08X)\n", off, I(off), U(off));
    fclose(f);
}

DEFINE_HOOK(5656EA, MapClass_OperatorBracket_Stride, 7)
{
    int y = R->EAX<int>();
    int x = R->ECX<int>();
    int index = y * g_MapStride + x;
    R->EAX(index);

    DumpMapStateOnce();           // MapClass::Instance dimension dump (diagnostic, once)

    if (index < 0)             return 0x565709;  // js  (negative)
    if (index >= g_MapTotal)   return 0x565709;  // cmp/jge (out of bounds)
    return 0x5656F8;                             // valid -> array load
}

// ============================================================
//  HOOK B1: shroud/visibility buffer alloc  @ 0x48EB12 (6 bytes)
//    48EB12: mov edx,[esi+0x16c]   dimension N   \ 6 stolen bytes
//    48EB18: shl edx,0x9           N * 512       (skipped)
//    48EB1B: push edx              <- resume target
//    48EB1C: call malloc
//  We fold mov+shl into `edx = [esi+0x16c] * stride` and jump
//  past the original shl to the push.
//  (The old hook sat on the 3-byte shl and returned 0x48EB1B,
//   which is inside the 6-byte trampoline -> guaranteed crash.)
// ============================================================
DEFINE_HOOK(48EB12, MapClass_Alloc_Stride1, 6)
{
    int dim = *reinterpret_cast<int*>(R->ESI() + 0x16C);
    R->EDX(dim * g_MapStride);
    return 0x48EB1B;  // push edx
}

// ============================================================
//  HOOK B2: mid-row pointer  @ 0x48EB35 (5 bytes)
//    48EB35: shl ecx,0x9      ((N-1)>>1) * 512  \ 5 stolen bytes
//    48EB38: add ecx,eax      + buffer base     /
//    48EB3A: mov [esi+0x174],ecx   <- resume target
//  eax = malloc result from B1. Fold shl+add together.
// ============================================================
DEFINE_HOOK(48EB35, MapClass_Alloc_Stride2, 5)
{
    R->ECX(R->ECX() * g_MapStride + R->EAX());
    return 0x48EB3A;  // mov [esi+0x174],ecx
}

// ============================================================
//  HOOK C: inline cell access via global 0x87F924 @ 0x483B32 (6 bytes)
//    483B32: shl edi,0x9            Y * 512       \ 6 stolen bytes
//    483B35: mov [esi+0xfc],ebx     (first 3 of 6) /  (must replicate)
//    483B3B: mov eax,ds:0x87f924    <- resume target (global cell array)
//    483B40: add edi,ecx            + X   (runs normally)
//    483B42: mov ecx,[eax+edi*4]    Array[index]
//  We must NOT jump straight to 483B42: that would skip the load of
//  the global array pointer into eax and the [esi+0xfc]=ebx store.
//  Instead: apply the stride, replicate the stolen store, and resume
//  at 483B3B so the original global load + add edi,ecx still run.
//  (Note: X is added later at 483B40, so do NOT add ecx here.)
// ============================================================
DEFINE_HOOK(483B32, MapClass_InlineAccess_Stride, 6)
{
    R->EDI(R->EDI<int>() * g_MapStride);                    // shl edi,0x9
    *reinterpret_cast<int*>(R->ESI() + 0xFC) = R->EBX();    // mov [esi+0xfc],ebx
    DumpMapStateOnce();                                     // diagnostic (once)
    return 0x483B3B;                                        // mov eax,ds:0x87f924
}

// ============================================================
//  HOOK D: MapClass::operator[](lepton)  @ 0x565757 (5 bytes)
//    565757: shl edx,0x9   Y * 512   \ 5 stolen bytes
//    56575A: add edx,esi   + X       /
//    56575C: js 0x56577a   <- resume target
//  Bound check at 56575E reads [ecx+0x140] (dynamic) so only the
//  stride needs patching. (Old hook used size 6 and returned
//  0x56575C, which was inside the trampoline.)
// ============================================================
DEFINE_HOOK(565757, MapClass_LeptonOp_Stride, 5)
{
    R->EDX(R->EDX<int>() * g_MapStride + R->ESI<int>());
    DumpMapStateOnce();  // diagnostic (once) - hot during pan/render
    return 0x56575C;  // js 0x56577a
}

// ============================================================
//  HOOK E: IsCellValid  @ 0x5657F1 (5 bytes)
//    5657F1: shl edx,0x9   Y * 512   \ 5 stolen bytes
//    5657F4: add edx,eax   + X       /
//    5657F6: cmp [ecx+edx*4],0x0   <- resume target
//  (This site was listed as "hooked" in the research notes but
//   had no actual DEFINE_HOOK; without it IsCellValid mis-indexes
//   on any stride != 512.)
// ============================================================
DEFINE_HOOK(5657F1, IsCellValid_Stride, 5)
{
    R->EDX(R->EDX<int>() * g_MapStride + R->EAX<int>());
    DumpMapStateOnce();  // diagnostic (once)
    return 0x5657F6;  // cmp [ecx+edx*4],0x0
}

// ============================================================
//  MAP DIMENSION GATE HOOKS
//  Three sites reject a map when W >= 512 or H >= 512, checked
//  independently (there is NO W+H sum check in the engine).
//  We hook the width test, redo both compares against
//  g_MapMaxDimension, and branch to the site's own reject/accept.
//  ax = Width, cx = Height at every site.
//
//  Note: the accept path skips the engine's minimum-dimension
//  re-check for height (cmp cx,bp/bx). That guard only rejects
//  degenerate sub-~4-cell maps, which a map-enlarging mod never
//  produces, so the simplification is safe here.
// ============================================================

// Site 1: hook the 6-byte `jge 0x4C588B` at 0x4C5630.
//   reject -> 0x4C588B, accept -> 0x4C564A
DEFINE_HOOK(4C5630, MapDimGate_Site1, 6)
{
    if ((short)R->AX() >= (short)g_MapMaxDimension) return 0x4C588B;
    if ((short)R->CX() >= (short)g_MapMaxDimension) return 0x4C588B;
    return 0x4C564A;
}

// Site 2: `jge` here is a 2-byte short jump, so hook the
//   `cmp ax,0x200` (4) + short jge (2) = 6 bytes at 0x4C590E.
//   reject -> 0x4C5972, accept -> 0x4C5920
DEFINE_HOOK(4C590E, MapDimGate_Site2, 6)
{
    if ((short)R->AX() >= (short)g_MapMaxDimension) return 0x4C5972;
    if ((short)R->CX() >= (short)g_MapMaxDimension) return 0x4C5972;
    return 0x4C5920;
}

// Site 3: hook the 6-byte `jge 0x554CE4` at 0x554BC5.
//   reject -> 0x554CE4, accept -> 0x554BDF
DEFINE_HOOK(554BC5, MapDimGate_Site3, 6)
{
    if ((short)R->AX() >= (short)g_MapMaxDimension) return 0x554CE4;
    if ((short)R->CX() >= (short)g_MapMaxDimension) return 0x554CE4;
    return 0x554BDF;
}

// ============================================================
//  Radar minimap null-guard (Stride > 512).
//  At stride > 512 the radar surface CREATION path is gated out
//  (its computed minimap dims go degenerate), so the two radar
//  surfaces RadarClass::Instance+0x121C / +0x1220 stay NULL.
//  Antares's minimap bugfix (Bugfixes.Minimap.cpp: LockRadarSurfaces)
//  then dereferences the null surface -> crash at MinimapChanged
//  (0x657D3D) during scenario load. There is nothing to update when
//  the minimap surfaces don't exist, so skip RadarClass::MinimapChanged
//  (0x657CE0) entirely in that case -- which also bypasses the Antares
//  lock hooks inside it. NO-OP on normal maps (surfaces are non-null)
//  and at stride 512. Guards both surfaces. this = ECX = RadarClass::
//  Instance (0x87F7E8); the function takes no stack args (plain `ret`).
// ============================================================
//  CRITICAL: do NOT skip by writing R->ESP() -- Syringe restores registers
//  with `popad`, which IGNORES the esp slot, so an esp write never sticks and
//  the stack ends up 4 bytes misaligned (that produced the bogus "corrupted
//  temp" at 0x5257AC and the wild EIP=0x0D control-flow smash). Instead jump
//  to a bare `ret` (0x657D3C) and let the CPU's own `ret` pop the caller's
//  return address -- a correct, esp-safe function skip.
DEFINE_HOOK(657CE0, RadarClass_MinimapChanged_NullGuard, 5)
{
    if (g_MapStride > 512)
    {
        const DWORD radar = 0x87F7E8;   // RadarClass::Instance
        if (*reinterpret_cast<DWORD*>(radar + 0x121C) == 0 ||
            *reinterpret_cast<DWORD*>(radar + 0x1220) == 0)
        {
            return 0x657D3C;            // a bare `ret` -> clean skip of the function
        }
    }
    return 0;                           // surfaces exist -> run normally
}

// ============================================================
//  Radar UpdateMinimap null-guard (Stride > 512) -- the second
//  radar path. RadarClass::UpdateMinimap (0x656EC0) also draws to
//  the (null-at-stride>512) radar surfaces; Antares's Lock/Unlock
//  hooks (Bugfixes.Minimap.cpp @0x65731F/0x65757C) deref the null
//  surface -> crash at gameplay (Antares+0x6FFA2). Same fix as
//  MinimapChanged: skip the whole function when the surfaces are
//  null by jumping to a bare `ret`. this = ECX = RadarClass::
//  Instance; plain `ret` (epilogue 0x6578EA, no stack args).
//  Stolen: sub esp,0x48; push ebx; push ebp = 5 bytes.
// ============================================================
DEFINE_HOOK(656EC0, RadarClass_UpdateMinimap_NullGuard, 5)
{
    if (g_MapStride > 512)
    {
        const DWORD radar = 0x87F7E8;   // RadarClass::Instance
        if (*reinterpret_cast<DWORD*>(radar + 0x121C) == 0 ||
            *reinterpret_cast<DWORD*>(radar + 0x1220) == 0)
        {
            return 0x657D3C;            // a bare `ret` -> clean skip
        }
    }
    return 0;
}

// ============================================================
//  Object radar-blip update null-guard (Stride > 512).
//  gamemd 0x70D990 (a per-object __thiscall(1 arg) that plots the
//  object onto the radar minimap) reads the .bss coordinate-transform
//  singleton 0x880A04 UNCONDITIONALLY and calls a method on it. That
//  singleton is written by NO code (gamemd/Ares/Antares) -> always
//  null -> this path is dead in normal play and only executes once our
//  x1024 coords route here -> AV at 0x70DA54. Skip the whole function
//  when 0x880A04 is null. this = ECX; ret 4 (1 stack arg) so we jump to
//  a bare `ret 4` (0x70DC42) -- NOT a plain `ret`, or the arg leaks.
//  Stolen: sub esp,0x2c; push esi; mov esi,ecx = 6 bytes.
//  NO-OP at stride 512 (this dead path isn't reached there anyway).
// ============================================================
DEFINE_HOOK(70D990, Object_PlotOnRadar_NullGuard, 6)
{
    if (g_MapStride > 512 && *reinterpret_cast<DWORD*>(0x00880A04) == 0)
        return 0x70DC42;                // bare `ret 4` -> clean skip
    return 0;
}

// ============================================================
//  DEPLOY DIAGNOSTIC (stride > 512 only)
//  TechnoClass::CanDeploySlashUnload @0x700D50 is the real deploy/
//  unload validity check (returns bool; false => the "blocked" cursor).
//  It fetches the unit's coords (virtual [this+0x1b8]) then operator[]
//  (patched) and runs cell predicates (0x484AE0 etc). On a normal map
//  at stride 1024 the MCV refuses to deploy though every cell read is
//  patched -- so log the coord the check sees and dump that cell's
//  occupancy fields to tell whether the cell is (wrongly) seen as
//  blocked. Appends to MapSizeExt_deploy.log (abs path, capped).
// ============================================================
static void DeployDiagLog(const char* fmt, ...)
{
    static int lines = 0;
    if (g_MapStride <= 512 || lines >= 200) return;
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "MapSizeExt_deploy.log");
    FILE* f = nullptr;
    fopen_s(&f, path, "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
    ++lines;
}

// Inside CanDeploySlashUnload, at the operator[] setup:
//   700D6F: mov ecx,0x87f7e8   (5 bytes: B9 E8 F7 87 00)  <- clean, no esp/call
// At this point EAX = pointer to the unit's coord/cell struct (from the
// virtual GetCoords at 700D62). Log the (X,Y) it will look up and, via our
// own stride math, dump the target cell's occupancy fields. This both
// CONFIRMS this is the deploy path (does it fire?) and shows whether the
// cell is wrongly seen as occupied/impassable.
DEFINE_HOOK(700D6F, CanDeploySlashUnload_Diag, 5)
{
    static bool gridDone = false;
    if (g_MapStride > 512 && !gridDone)
    {
        gridDone = true;
        // MCV cell from its stored Location (leptons @ +0x9C).
        DWORD self = R->ESI();
        int cx = *reinterpret_cast<int*>(self + 0x9C) >> 8;
        int cy = *reinterpret_cast<int*>(self + 0xA0) >> 8;
        DWORD items = *reinterpret_cast<DWORD*>(0x87F7E8 + 0x13C);  // Cells.Items

        // Dump an ASCII grid around the MCV. For each cell:
        //   '#' null cell pointer (should be none now)
        //   'V' Shroudedness==-1 (revealed/visible)   'O' ==-2 (occluded)
        //   '.' fogged (0..48)                          '@' the MCV's own cell
        //   '!' cell EXISTS but its stored MapCoords != its array position
        //       (cell-identity mismatch -> would break pathfinding/movement)
        // Shroudedness @ cell+0x120 (char); MapCoords.X@+0x24, .Y@+0x26 (shorts).
        DeployDiagLog("=== visibility/identity grid around MCV cell (%d,%d), stride %d ===\n",
                      cx, cy, g_MapStride);
        DeployDiagLog("legend: @=mcv V=visible O=occluded .=fog #=nullcell !=coord-mismatch\n");
        for (int gy = cy - 8; gy <= cy + 8; ++gy)
        {
            char line[64]; int n = 0;
            for (int gx = cx - 8; gx <= cx + 8 && n < 60; ++gx)
            {
                char ch;
                int idx = gy * g_MapStride + gx;
                DWORD cell = (items && idx >= 0 && idx < g_MapTotal)
                             ? *reinterpret_cast<DWORD*>(items + idx * 4) : 0;
                if (!cell) ch = '#';
                else
                {
                    int mcx = *reinterpret_cast<short*>(cell + 0x24);
                    int mcy = *reinterpret_cast<short*>(cell + 0x26);
                    char sh = *reinterpret_cast<char*>(cell + 0x120);
                    if (mcx != gx || mcy != gy) ch = '!';          // identity mismatch
                    else if (gx == cx && gy == cy) ch = '@';
                    else if (sh == -1) ch = 'V';
                    else if (sh == -2) ch = 'O';
                    else ch = '.';
                }
                line[n++] = ch;
            }
            line[n] = '\0';
            DeployDiagLog("  %s\n", line);
        }
    }
    R->ECX(0x87F7E8);   // replicate mov ecx,0x87f7e8
    return 0x700D74;
}
