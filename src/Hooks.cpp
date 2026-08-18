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
int g_CrashGuard      = 1;       // GetCellAt garbage-slot guard (INI PatchCrashGuard)
int g_StrideSkipFrom  = 0;       // bisection: skip kCellStrideSites [from,to)
int g_StrideSkipTo    = 0;
int g_CuratedBase     = 0;       // 1 = defer conflicting accessor/alloc hooks to his patches
int g_MapMaxW         = 512;
int g_MapMaxH         = 512;
int g_MapMaxDimension = 512;     // per-axis gate (replaces cmp ax,0x200)
int g_CoordBase       = 1000;    // per-map cell-number base (1000 = vanilla)

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

    // Diamond edge-band population audit (2026-08-18): user reports LEGAL cells
    // ~5-10 deep from the map edge refuse orders with a normal cursor at stride
    // 2048 -- consistent with NULL/garbage slots in a ring just inside the
    // diamond (population under-fill). Sample rings at cartesian edge distance
    // d and count null / identity-mismatched slots. All-zero = population OK
    // (then the dead band is elsewhere); nonzero pins the hole's depth.
    {
        const int W = I(0xF4), H = I(0xF8);
        DWORD items = *reinterpret_cast<DWORD*>(0x87F924);
        if (items && W > 4 && H > 4)
        {
            static const int bands[] = { 0, 1, 2, 3, 5, 8, 12 };
            fprintf(f, "\n-- edge-band population audit (d = cartesian ring distance) --\n");
            for (int bi = 0; bi < 7; ++bi)
            {
                const int d = bands[bi];
                if (2 * d >= W || 2 * d >= H) break;
                int total = 0, nullc = 0, badid = 0;
                for (int side = 0; side < 4; ++side)
                {
                    const int n = (side < 2 ? W - 2 * d : H - 2 * d);
                    const int step = (n > 200 ? n / 200 : 1);
                    for (int i = 0; i < n; i += step)
                    {
                        int X, Y;
                        if (side == 0)      { X = d + i;     Y = d; }
                        else if (side == 1) { X = d + i;     Y = H - 1 - d; }
                        else if (side == 2) { X = d;         Y = d + i; }
                        else                { X = W - 1 - d; Y = d + i; }
                        const int rx = X + Y + 1, ry = Y - X + W;
                        const int idx = ry * g_MapStride + rx;
                        ++total;
                        if (idx < 0 || idx >= g_MapTotal) { ++badid; continue; }
                        const DWORD c = *reinterpret_cast<DWORD*>(items + idx * 4);
                        if (!c) { ++nullc; continue; }
                        if (c < 0x400000 || c >= 0x60000000) { ++badid; continue; }
                        if (*reinterpret_cast<short*>(c + 0x24) != rx ||
                            *reinterpret_cast<short*>(c + 0x26) != ry) ++badid;
                    }
                }
                fprintf(f, "  band d=%2d: sampled=%4d  null=%4d  bad-identity=%4d\n",
                        d, total, nullc, badid);
            }
        }
    }
    fclose(f);
}

// Read the cells the tactical view is CURRENTLY drawing (TacticalClass::Instance
// @ *0x887324; VisibleCellCount @ +0xE0; VisibleCells[800] @ +0xE4) and check
// each one's LightConvert (cell+0x34). If the on-screen cells have null convert,
// objects on them render black -- proven, with no camera/timing confound. Fires
// once, a bit into gameplay (so the view is populated). Writes MapSizeExt_light.log.
static void DumpVisibleCellsLighting()
{
    static long long calls = 0;
    static bool done = false;
    if (done || g_MapStride <= 512) return;
    if (++calls < 3000LL) return;                   // let the game start drawing
    DWORD tac = *reinterpret_cast<DWORD*>(0x00887324);   // TacticalClass::Instance
    if (!tac) return;
    int vcount = *reinterpret_cast<int*>(tac + 0xE0);    // VisibleCellCount
    if (vcount <= 0) return;                         // nothing drawn yet -> wait
    done = true;

    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "MapSizeExt_light.log");
    FILE* f = nullptr; fopen_s(&f, path, "w");
    if (!f) return;

    DWORD* vcells = reinterpret_cast<DWORD*>(tac + 0xE4);
    int nullLC = 0, setLC = 0, minx = 1 << 30, maxx = -(1 << 30), miny = 1 << 30, maxy = -(1 << 30);
    fprintf(f, "TacticalClass=0x%X  VisibleCellCount=%d  stride=%d\n\n", tac, vcount, g_MapStride);
    int cap = vcount > 800 ? 800 : vcount;
    for (int i = 0; i < cap; ++i)
    {
        DWORD cell = vcells[i];
        if (!cell) continue;
        DWORD lc = *reinterpret_cast<DWORD*>(cell + 0x34);
        int x = *reinterpret_cast<short*>(cell + 0x24);
        int y = *reinterpret_cast<short*>(cell + 0x26);
        if (lc == 0) ++nullLC; else ++setLC;
        if (x < minx) minx = x; if (x > maxx) maxx = x;
        if (y < miny) miny = y; if (y > maxy) maxy = y;
        if (i < 40)
            fprintf(f, "  vis[%2d] cell(%d,%d)  LightConvert=0x%X  Level=%d\n",
                    i, x, y, lc, (int)*reinterpret_cast<signed char*>(cell + 0x11B));
    }
    fprintf(f, "\nSUMMARY: %d visible cells, %d have NULL LightConvert (black), %d set.\n",
            vcount, nullLC, setLC);
    fprintf(f, "Visible-cell coord range: X[%d..%d] Y[%d..%d]\n", minx, maxx, miny, maxy);
    fclose(f);
}


DEFINE_HOOK(5656EA, MapClass_OperatorBracket_Stride, 7)
{
    // NOTE: needed at 1024 even in curated mode -- deferring it (M2) broke the
    // multi-cell building foundation (1-cell) and standard-map elevation.
    int y = R->EAX<int>();
    int x = R->ECX<int>();
    int index = y * g_MapStride + x;
    R->EAX(index);

    DumpMapStateOnce();           // MapClass::Instance dimension dump (diagnostic, once)
    DumpVisibleCellsLighting();   // TacticalClass VisibleCells LightConvert probe (once, late)

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
    // DEFER ALWAYS (§2.14): this shl-9 is NOT the cell plane -- it is a
    // shroud/visibility buffer row size. Sizing it to *1024 over-sizes a shared
    // draw/palette buffer -> BRIGHT sidebar cameos (broad mode) and the curated
    // stripe. Leaving the original N*512 (native) fixes the sidebar; the cell
    // plane itself is sized by the dimension patches (0x565812/828), not here.
    return 0;
    // (was: int dim=[esi+0x16C]; R->EDX(dim*g_MapStride); return 0x48EB1B;)
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
    // DEFER ALWAYS -- paired with 0x48EB12 (see there). Original N*512 mid-row
    // pointer keeps the shroud buffer native-sized -> correct sidebar / no stripe.
    return 0;
    // (was: R->ECX(R->ECX()*g_MapStride + R->EAX()); return 0x48EB3A;)
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
    // NOTE: needed at 1024 even in curated mode -- this is a cell-store; at 512
    // it registers foundation cells wrong (1-cell building). Only the shroud-
    // buffer alloc hooks 0x48EB12/35 defer in curated mode (they caused the stripe).
    R->EDI(R->EDI<int>() * g_MapStride);                    // shl edi,0x9
    *reinterpret_cast<int*>(R->ESI() + 0xFC) = R->EBX();    // mov [esi+0xfc],ebx
    DumpMapStateOnce();                                     // diagnostic (once)
    DumpVisibleCellsLighting();                             // lighting probe (once)
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
    DumpVisibleCellsLighting();  // lighting probe (once)
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
//  COORDBASE: cell-number packing base (big-map spawn placement)
//
//  Map INIs pack a cell as N = Y*BASE + X. Vanilla hardcodes BASE=1000, so the
//  low coord X (= iso rx, which runs to W+H-1) must be < 1000 -> maps are capped
//  at W+H <= 1000 (~500x500 square), independent of stride. Big maps generated by
//  RA2MapGen carry a [MapSizeExt] CoordBase=<power-of-two> key and pack with that
//  base; we decode cell numbers with the same base. A vanilla map has no key ->
//  GetInt returns the 1000 default -> behaviour is byte-identical.
//
//  The vanilla [Waypoints] decoder @0x68BE0C computes X = N%1000 via `idiv` but
//  then RE-derives Y = N/1000 through the 0x10624dd3 reciprocal-multiply, so
//  merely retargeting the idiv divisor would fix X and leave Y wrong. Instead we
//  replace the entire decode+store: compute X=N%base, Y=N/base and write the
//  CellStruct{X,Y} at [edi] directly, then resume at the post-store loop check.
//    ecx = N (waypoint value, already tested != 0), edi = &Waypoints[i] (dest),
//    ebx = the scenario CCINIClass* (set at 0x68BDC4, callee-saved through here).
//  Resume 0x68BE35 = the `cmp esi,0x2be` that guards the GetCellAt + flag store.
//
//  Base is read from the scenario INI each call (<=702x at load; negligible) so it
//  can never go stale across maps, and published to g_CoordBase for the gameplay
//  cell decoders (mouse-click / triggers / forest-fire) added on top of this.
DEFINE_HOOK(68BE0C, Waypoint_CoordBaseDecode, 5)
{
    int base = 1000;
    if (g_MapStride > 512)
    {
        void* ini = reinterpret_cast<void*>(R->EBX());
        if (ini)
        {
            typedef int(__thiscall* GetIntFn)(void*, const char*, const char*, int);
            int b = reinterpret_cast<GetIntFn>(ADDR_INICLASS_GETINT)(
                        ini, "MapSizeExt", "CoordBase", 1000);
            if (b > 1000) base = b;
        }
    }
    g_CoordBase = base;

    const unsigned ub = static_cast<unsigned>(base);
    const unsigned N  = static_cast<unsigned>(R->ECX());
    short* dst = reinterpret_cast<short*>(R->EDI());
    dst[0] = static_cast<short>(N % ub);   // X = iso rx
    dst[1] = static_cast<short>(N / ub);   // Y = iso ry

    // Log the first few decodes UNCONDITIONALLY (even base==1000): this is the
    // direct proof of whether the reader ever sees a map's real waypoint values
    // (e.g. N=741739 for the 700x700 map). The 2026-08-16 run showed CoordBase
    // stuck at 1000 + zero engine waypoints at the (once-only) probe moment; the
    // 50x950 run then PLAYED, proving the real read happens later than the probe.
    static int decodes = 0;
    if (++decodes <= 10)
    {
        char p[MAX_PATH];
        GetModuleFileNameA(nullptr, p, MAX_PATH);
        char* s = strrchr(p, '\\'); if (s) *(s + 1) = '\0';
        strcat_s(p, "MapSizeExt.log");
        FILE* f = nullptr; fopen_s(&f, p, "a");
        if (f) { fprintf(f, "[coordbase #%d] N=%u base=%d -> X=%d Y=%d\n",
                         decodes, N, base, (int)dst[0], (int)dst[1]); fclose(f); }
    }
    return 0x68BE35;
}

// ============================================================
//  SPAWN DIAGNOSTIC: pinpoint where big-map MP starts go garbage (stride>512)
//
//  The 2048 spawn hang: the MP-start assigner (~0x6884D5) reads Scenario.Waypoints
//  (*(0xA8B230)+0x632, array[702] of packed cells) and finds garbage (last seen =
//  0x0087F7E8, the MapClass::Instance pointer) -> no valid start -> infinite search
//  -> black screen. Static tracing is ambiguous: INIClass_ReadScenario DOES call
//  the vanilla reader 0x68BDC0 at 0x6873DB (writes Waypoints from [Waypoints], our
//  CoordBase hook decodes it), but then a loop @0x6873F4 OVERWRITES Waypoints[0..7]
//  with Scenario+0x11C0[0..7] when flag Scenario+0x34BD is set. The 0x11C0 offset
//  aliases across unrelated structs so its filler can't be pinned statically.
//
//  This read-only probe snapshots the whole chain in ONE run. Hook point 0x687410
//  = `mov 0x887048,%edx` (6B), immediately AFTER the copy loop, so we see the final
//  Waypoints[0..7] the assigner will read, the source table, and the copy flag.
//  Logs once. Gated stride>512 so it is a strict no-op on vanilla-size play.
DEFINE_HOOK(687410, Spawn_WaypointDiag, 6)
{
    if (g_MapStride > 512)
    {
        static int calls = 0;
        if (++calls <= 24)          // log every call (capped), not once -- capture sequence
        {
            char* scen = *reinterpret_cast<char**>(0xA8B230);
            char p[MAX_PATH];
            GetModuleFileNameA(nullptr, p, MAX_PATH);
            char* s = strrchr(p, '\\'); if (s) *(s + 1) = '\0';
            strcat_s(p, "MapSizeExt.log");
            FILE* f = nullptr; fopen_s(&f, p, "a");
            if (f)
            {
                fprintf(f, "[spawndiag #%d] Scenario=%p CoordBase=%d copyFlag(0x34BD)=%d ini(ebx)=%p\n",
                        calls, (void*)scen, g_CoordBase,
                        scen ? (int)(unsigned char)scen[0x34BD] : -1, (void*)R->EBX());
                if (scen)
                {
                    unsigned* wp  = reinterpret_cast<unsigned*>(scen + 0x632);
                    unsigned* src = reinterpret_cast<unsigned*>(scen + 0x11C0);
                    for (int i = 0; i < 8; ++i)
                    {
                        unsigned w = wp[i], t = src[i];
                        // decode packed cell as low/high word (engine stores X=low,Y=high)
                        fprintf(f, "[spawndiag]  #%d  Waypoints[0x632]=%08X (X=%d Y=%d)   Src[0x11C0]=%08X (X=%d Y=%d)\n",
                                i, w, (short)(w & 0xFFFF), (short)(w >> 16),
                                   t, (short)(t & 0xFFFF), (short)(t >> 16));
                    }
                }
                fclose(f);
            }
        }
    }
    return 0;   // continue normally (read-only probe)
}

// Second probe: the MP-start assigner itself (fn @0x688380, hang loop scans
// Waypoints at 0x6883B7 / 0x688455). Hook 0x68839A = `mov 0xa8b230,%edx` (6B,
// post-prologue) so we log what the assigner ACTUALLY reads -- this fires even if
// INIClass_ReadScenario / the 0x687410 probe is bypassed in the CnCNet flow. The
// engine's empty-slot sentinel X is the word at 0xB05458; we flag matches.
DEFINE_HOOK(68839A, Spawn_AssignerDiag, 6)
{
    if (g_MapStride > 512)
    {
        static int calls = 0;
        if (++calls <= 8)           // assigner may loop forever on hang -> cap hard
        {
            char* scen = *reinterpret_cast<char**>(0xA8B230);
            unsigned short sentinel = *reinterpret_cast<unsigned short*>(0xB05458);
            char p[MAX_PATH];
            GetModuleFileNameA(nullptr, p, MAX_PATH);
            char* s = strrchr(p, '\\'); if (s) *(s + 1) = '\0';
            strcat_s(p, "MapSizeExt.log");
            FILE* f = nullptr; fopen_s(&f, p, "a");
            if (f)
            {
                fprintf(f, "[assigner #%d] Scenario=%p sentinelX=%04X -- Waypoints the assigner scans:\n",
                        calls, (void*)scen, sentinel);
                if (scen)
                {
                    unsigned* wp = reinterpret_cast<unsigned*>(scen + 0x632);
                    for (int i = 0; i < 8; ++i)
                    {
                        unsigned w = wp[i];
                        fprintf(f, "[assigner]  #%d = %08X  X=%d Y=%d %s\n",
                                i, w, (short)(w & 0xFFFF), (short)(w >> 16),
                                (unsigned short)(w & 0xFFFF) == sentinel ? "(empty)" : "");
                    }
                }
                fclose(f);
            }
        }
    }
    return 0;   // read-only probe
}

// ============================================================
//  TIBERIUM per-type buffer realloc guard + diagnostic  (fn @0x722D00)
//
//  On map (re)load the engine reallocates, for EACH tiberium type in the global
//  TiberiumClass array [0xB0F4EC] (count [0xB0F4F8]), three map-area-sized buffers:
//    [obj+0x110] a VectorClass, COUNT*4+4 bytes, zero-filled by a loop @0x722E19
//    [obj+0x114] COUNT bytes,  [obj+0x118] COUNT*8 bytes
//  where COUNT = (MapRect.Height+4) * MapRect.Width * 2  (fn 0x42B1F0, dims at
//  0x87F8DC/0x87F8E0). The +0x110 buffer malloc @0x722E01 is UNCHECKED -> if it
//  returns null the zero-fill @0x722E19 writes through null (the C0000005 that
//  blocks big maps). This is the fill loop head (xor eax,eax / cmp count / jl):
//    722E0F: xor eax,eax   722E11: cmp ecx,ebx   722E13: jl 0x722E28
//  Guard it: skip the fill when the buffer is null (turn the crash into graceful
//  empty-ore), and log MapRect/COUNT/buffer once so we can tell an INFLATED COUNT
//  (sizing bug) from a genuine allocation failure (32-bit OOM). esi = the vector
//  struct ([+4]=COUNT, [+8]=buffer). Resume 0x722E15 (fill) with eax=0, else 0x722E28.
DEFINE_HOOK(722E0F, Tiberium_BufferFillGuard, 6)
{
    char* self  = reinterpret_cast<char*>(R->ESI());
    int   count = *reinterpret_cast<int*>(self + 4);
    void* buf   = *reinterpret_cast<void**>(self + 8);

    if (g_MapStride > 512)
    {
        static int n = 0;
        if (n < 8)
        {
            ++n;
            int rw = *reinterpret_cast<int*>(0x87F8DC);   // MapRect.Width
            int rh = *reinterpret_cast<int*>(0x87F8E0);   // MapRect.Height
            char p[MAX_PATH];
            GetModuleFileNameA(nullptr, p, MAX_PATH);
            char* s = strrchr(p, '\\'); if (s) *(s + 1) = '\0';
            strcat_s(p, "MapSizeExt.log");
            FILE* f = nullptr; fopen_s(&f, p, "a");
            if (f)
            {
                fprintf(f, "[tiberium] MapRect=%dx%d COUNT=%d bytes=%d buffer=%p%s\n",
                        rw, rh, count, count * 4 + 4, buf, buf ? "" : "  <-- NULL (alloc failed)");
                fclose(f);
            }
        }
    }

    if (!buf || count <= 0) return 0x722E28;   // crash guard: skip the fill
    R->EAX(0);                                 // replicate the stolen 'xor eax,eax'
    return 0x722E15;                           // resume the zero-fill loop
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
    if (g_MapStride <= 512 || lines >= 500) return;
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
    // Fire LATER (not the first deploy check at game start, before the camera
    // has drawn the MCV area) so the tactical draw has run over the MCV. Fires
    // once, ~the 900th deploy check (a while into play / after panning).
    static int calls = 0;
    static bool gridDone = false;
    if (g_MapStride > 512 && !gridDone && ++calls >= 900)
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
        // WHOLE-MAP dump (cells 0..159 in both axes, one char each) so we can SEE
        // the geometry: where the MCV is (@) vs where the revealed region is.
        // A mirror/fold will be obvious (MCV bottom-left, reveal top-right).
        //   ' ' occluded/never-seen (-2)   '.' fog   'V' visible(-1)
        //   '@' MCV cell                    '#' null cell   '!' coord-mismatch
        // Shroudedness @ cell+0x120 (char); MapCoords.X@+0x24, .Y@+0x26.
        // ---- LIGHTING STATE PROBE ----
        // (1) Global ambient: ScenarioClass::Instance ptr @ ds:0xA8B230; the
        //     lighting inputs UpdateLighting reads live at [+0x354C..+0x3554]
        //     (Ground/Red/Green/Blue-ish) and the computed set at [+0x3530..].
        // (2) Per-cell LightConvert @ cell+0x34 and Level @ cell+0x11B.
        DWORD scen = *reinterpret_cast<DWORD*>(0x00A8B230);
        DeployDiagLog("LIGHT scenario=0x%X\n", scen);
        if (scen)
            for (DWORD off = 0x3528; off <= 0x3558; off += 4)
                DeployDiagLog("  scen[+0x%X] = %d (0x%X)\n", off,
                    *reinterpret_cast<int*>(scen + off), *reinterpret_cast<DWORD*>(scen + off));
        // MCV cell's lighting
        {
            int midx = cy * g_MapStride + cx;
            DWORD mcell = (items && midx >= 0 && midx < g_MapTotal) ? *reinterpret_cast<DWORD*>(items + midx * 4) : 0;
            if (mcell)
                DeployDiagLog("MCV cell (%d,%d): LightConvert@0x34=0x%X  Level@0x11B=%d\n",
                    cx, cy, *reinterpret_cast<DWORD*>(mcell + 0x34),
                    (int)*reinterpret_cast<signed char*>(mcell + 0x11B));
        }

        // Grid CENTERED on the MCV (works for any map size). 141x141 window.
        // If the L (lit) region is centered on '@', per-cell lighting follows the
        // view/units correctly (and the earlier "offset" was just the game-start
        // camera). If the L region sits OFFSET from '@', it's a real bug.
        const int R = 70;
        DeployDiagLog("=== per-cell LightConvert grid CENTERED on MCV (%d,%d), stride %d ===\n",
                      cx, cy, g_MapStride);
        DeployDiagLog("legend: @=mcv  L=has LightConvert  X=null convert(black)  (space)=null cell  cols %d..%d\n",
                      cx - R, cx + R);
        for (int gy = cy - R; gy <= cy + R; ++gy)
        {
            char line[2 * R + 4]; int n = 0;
            for (int gx = cx - R; gx <= cx + R; ++gx)
            {
                char ch;
                int idx = gy * g_MapStride + gx;
                DWORD cell = (items && idx >= 0 && idx < g_MapTotal)
                             ? *reinterpret_cast<DWORD*>(items + idx * 4) : 0;
                if (!cell) ch = ' ';
                else
                {
                    DWORD lc = *reinterpret_cast<DWORD*>(cell + 0x34);   // LightConvert
                    if (gx == cx && gy == cy) ch = '@';
                    else if (lc == 0) ch = 'X';                          // null convert -> black objects
                    else ch = 'L';
                }
                line[n++] = ch;
            }
            line[n] = '\0';
            DeployDiagLog("y%03d|%s\n", gy, line);
        }
    }
    R->ECX(0x87F7E8);   // replicate mov ecx,0x87f7e8
    return 0x700D74;
}

// ============================================================
//  REVEAL DIAGNOSTIC: RevealArea2 shroud write @ 0x567F34
//    88 83 20 01 00 00   mov [ebx+0x120],al   (6 bytes)
//  ebx = the cell being revealed; al = new Shroudedness. Log the cell's
//  MapCoords (+0x24/+0x26), the value written, and its Flags (+0x140,
//  whose low bits 0x3 are the "mapped/revealed" state 0x6d8700 keys on).
//  Shows whether reveal writes odd rows at all and whether their Flags
//  are set. Budget-capped via DeployDiagLog. Stride>512 only.
// ============================================================
DEFINE_HOOK(567F34, RevealArea2_ShroudWrite_Diag, 6)
{
    if (false)   // disabled: this path did not fire; keep hook inert
    {
        DWORD cell = R->EBX();
        int cxr = *reinterpret_cast<short*>(cell + 0x24);
        int cyr = *reinterpret_cast<short*>(cell + 0x26);
        signed char al = static_cast<signed char>(R->EAX() & 0xFF);
        DWORD flags = *reinterpret_cast<DWORD*>(cell + 0x140);
        DeployDiagLog("REVEAL (%d,%d) new=%d flags=0x%X rev=%d\n",
                      cxr, cyr, (int)al, flags, (int)(flags & 0x3));
    }
    *reinterpret_cast<signed char*>(R->EBX() + 0x120) = static_cast<signed char>(R->EAX() & 0xFF);
    return 0x567F3A;
}

// ============================================================
//  0x880A04 null-singleton crash guard  @ 0x660540
//  This gamemd function (thiscall; its only ret is a bare `ret` c3 at
//  0x660729) does a small coord-offset loop then UNCONDITIONALLY enters
//  a loop that dereferences the coord-transform singleton at ds:0x880A04
//  and calls virtuals on it (`mov esi,[ecx]; call [esi+0x78]` @0x66058C).
//  0x880A04 is written by NO code in gamemd (18 reads, 0 writes) -> it is
//  always null, so this whole function is DEAD in vanilla (never reached).
//  Our x1024 coords wrongly route here -> guaranteed AV at 0x66058C
//  (seen on deploy / sell / game-end). Since the function always crashes
//  when reached and does nothing reachable when the singleton is null,
//  skip it cleanly: at entry ESP = return address, and the function takes
//  no stack args (ret c3), so redirect to a bare `ret` (0x66053A) to
//  return to the caller with a balanced stack. eax=0 as a safe result.
// ============================================================
DEFINE_HOOK(660540, CoordTransform_NullSingleton_Guard, 5)
{
    // The function derefs ds:0x880A04 as `mov ecx,[0x880A04]; mov esi,[ecx];
    // call [esi+0x78]`. At stride 1024 that singleton is a NON-object (observed a
    // heap ptr whose vtable is garbage -> the virtual call jumps into heap junk
    // 0x07Cxxxxx on unit spawn). Validating the vtable range false-passed (the
    // garbage landed inside the exe range), so validate nothing: this function has
    // exactly ONE caller (0x65FE3B, inside the fn reached only from
    // Multiplay_LogToSync), and its result is consumed only by sync-checksum
    // logging -- never gameplay -- so at stride>512 skip it unconditionally.
    // eax=0 is a harmless logged value. Stride 512 keeps the vanilla null-check.
    DWORD s = *reinterpret_cast<DWORD*>(0x00880A04);
    if (g_MapStride > 512 || s == 0)
    {
        R->EAX(0);
        return 0x66053A;   // a bare `ret` (c3) -> clean return to caller
    }
    return 0;              // stride 512, non-null singleton -> run original
}

// ============================================================
//  MapClass::GetCellAt(coord) garbage-slot guard  @0x565766
//  0x565730 converts a lepton coord to a cell index (shl @0x565757 patched to
//  1024), bound-checks (js / cmp [Map+0x140]), then returns Items[index] unless
//  it is null (test/je -> a safe dummy cell). BUT the Items array corners outside
//  the populated iso-diamond hold NON-NULL GARBAGE at 1024 (leftover pointers,
//  e.g. 0x465F5445 = "ET_F" string data), so the null check passes and it returns
//  garbage -> caller derefs [garbage+0x140] -> AV (observed: a HunterSeeker/flying
//  unit's cell lookup @0x4CDD5F).
//  We hook the slot read: a real cell is a heap object whose MapCoords match the
//  slot index; anything else is garbage -> force it to 0 so the function's own
//  test/je routes it to the dummy cell. At entry ECX=Map, EDX=index (already
//  validated in-bounds). Replicate `mov ecx,[ecx+0x13c]; mov edx,[ecx+edx*4]`
//  and continue at the test. NO-OP at stride 512.
DEFINE_HOOK(565766, GetCellAt_GarbageGuard, 6)
{
    const DWORD map   = R->ECX();
    const DWORD items = *reinterpret_cast<DWORD*>(map + 0x13C);
    const int   index = static_cast<int>(R->EDX());
    DWORD cell = *reinterpret_cast<DWORD*>(items + static_cast<DWORD>(index) * 4);
    if (g_MapStride > 512 && g_CrashGuard && cell)
    {
        bool ok = (cell >= 0x04000000 && cell < 0x40000000);   // plausible heap object
        if (ok)
        {
            const int ex = index % static_cast<int>(g_MapStride);
            const int ey = index / static_cast<int>(g_MapStride);
            const int cx = *reinterpret_cast<short*>(cell + 0x24);
            const int cy = *reinterpret_cast<short*>(cell + 0x26);
            ok = (cx == ex && cy == ey);                        // identity: coords match slot
        }
        if (!ok) cell = 0;                                      // garbage -> null -> dummy
    }
    R->ECX(items);        // replicate mov ecx,[ecx+0x13c]
    R->EDX(cell);         // replicate (validated) mov edx,[ecx+edx*4]
    return 0x56576F;      // continue at `test edx,edx`
}

// ============================================================
//  PATHFINDING DIAGNOSTIC  (stride > 512)
//  FootClass::UpdatePathfinding @ 0x4D3920 solves a unit's path.
//    b8 9c 1f 00 00   mov eax,0x1f9c   (A* stack-buffer size; 5 stolen bytes)
//  thiscall: ECX = FootClass*; stack args (ret 0xC): [esp+4]=CellStruct start,
//  [esp+8]=CellStruct target, [esp+0xC]=int. Log LONG requests (Manhattan dist
//  > 15) -- the "far" moves the user reports failing -- so we can see whether
//  the pathfinder is even asked with a sane start/target. Reuses DeployDiagLog
//  (capped). NO-OP at 512.
// ============================================================
// Resolve a cell pointer from map coords (0 if null/out-of-range).
static DWORD CellAt(int x, int y)
{
    DWORD items = *reinterpret_cast<DWORD*>(0x87F924);   // MapClass Cells.Items
    if (!items) return 0;
    int idx = y * g_MapStride + x;
    if (idx < 0 || idx >= g_MapTotal) return 0;
    return *reinterpret_cast<DWORD*>(items + idx * 4);
}
// LandType (+0xEC): 0=Clear 1=Road 2=Water 3=Rock 4=Wall 5=Tiberium 6=Beach 7=Rough 8=Cliff. -1 = null cell.
static int LandTypeAt(int x, int y) { DWORD c = CellAt(x, y); return c ? *reinterpret_cast<int*>(c + 0xEC) : -1; }

// ============================================================
//  SENTINEL-FAIL TRAP (the human-order fold, stride 2048).
//
//  2026-08-17 bracket data: a corner-ordered unit's stored Destination is the
//  OOB DUMMY CELL 0xABDC50 -- the click->order decode fails a cell lookup, the
//  unit's destination becomes the dummy, and the dummy's scratch MapCoords
//  (rewritten by every failed lookup game-wide) make the unit wander (all the
//  observed behaviors: all-over-the-place / off-map north / mid-map / folds).
//  All three cell-lookup fail paths write the REQUESTED packed coords into the
//  dummy (0xABDC74) before returning it: op[](Cell&) @0x565712, op[](lepton)
//  @0x565789, GetCellAt @0x5657CF. In-diamond coords cannot fail these checks
//  at stride 2048, so every hit carries ALREADY-corrupt coords -- log request
//  + caller return address to identify the decoder. Per-caller cap, total cap.
static void LogSentinelFail(const char* site, DWORD packed, DWORD caller)
{
    static int total = 0;
    if (total > 400) return;
    ++total;
    static DWORD callers[64]; static int counts[64]; static int ncal = 0;
    int i;
    for (i = 0; i < ncal; ++i) if (callers[i] == caller) break;
    if (i == ncal)
    {
        if (ncal >= 64) return;
        callers[ncal] = caller; counts[ncal] = 0; ++ncal;
    }
    if (counts[i]++ >= 6) return;
    DeployDiagLog("OOB %s req(%d,%d) caller=0x%X\n", site,
                  (int)(short)(packed & 0xFFFF), (int)(short)(packed >> 16), caller);
}
DEFINE_HOOK(565712, CellLookup_Fail_OpCell, 6)
{
    if (g_MapStride > 512)
        LogSentinelFail("op[]", R->EDX(), *reinterpret_cast<DWORD*>(R->ESP() + 4));
    return 0;
}
DEFINE_HOOK(565789, CellLookup_Fail_OpLepton, 6)
{
    if (g_MapStride > 512)
        LogSentinelFail("lep ", R->EDX(), *reinterpret_cast<DWORD*>(R->ESP() + 4));
    return 0;
}
DEFINE_HOOK(5657CF, CellLookup_Fail_GetCellAt, 6)
{
    if (g_MapStride > 512)
        LogSentinelFail("gca ", R->EDX(), *reinterpret_cast<DWORD*>(R->ESP()));
    return 0;
}

// ============================================================
//  CELL-TARGET CODEC CoordBase FIX (root cause of the 2048 order bug).
//
//  The DUMMYDEST trap named the chain: EventClass::Execute (0x4C7482) ->
//  cell-target DECODE 0x6E6ED6 -> InfantryClass::SetDestination -> dummy.
//  Human orders travel through the event queue as TargetClass {N @+0, type
//  @+4}; for cells type=0xB and N = Y*1000 + X -- RA2's universal base-1000
//  external cell number. At stride 2048 any clicked cell with X>=1000 encodes
//  ambiguously and the decode yields off-diamond coords -> GetCellAt returns
//  the dummy -> the unit chases the dummy's drifting scratch coords (every
//  observed symptom). AI orders bypass the event queue -> never affected.
//
//  The complete codec (all in the 0x6E6xxx-0x6E7Cxx cluster):
//    ENCODE1 0x6E6B20 (from CellStruct): tag @0x6E6B46; Y*125 via lea-chain
//      @0x6E6B51-57; N = X + 8*(125Y) @0x6E6B5A; store @0x6E6B5D.
//    ENCODE2 0x6E6B70 (from CoordStruct/leptons): tag @0x6E6B75; Y*125
//      @0x6E6B89-8F into EDI; N = X + EDI*8 @0x6E6BA0.
//    DECODE1 0x6E6ED0 (type==0xB): X=N%1000 (idiv @0x6E6EDE), Y=N/1000
//      (magic), CellStruct -> GetCellAt @0x6E6F05.
//    DECODE2 0x6E7C20: identical twin (MegaMission destination field).
//  Both encoders share the "*8 tail", so substituting the 125 with
//  (CoordBase>>3) re-bases them exactly (1000>>3==125; CoordBase is pow2).
//  Decoders replace the div pair with N%base / N/base. All four hooks are
//  strict no-ops (return 0 -> original base-1000 code) unless the map carries
//  CoordBase>1000 (published at init from spawnmap.ini), so vanilla maps stay
//  byte-identical in behavior. Deferred same-class siblings (not in the order
//  path): 0x4AD232/0x4ADC17 (Display text/waypoint parse), 0x71CAEF (forest
//  fire jump) -- revisit if their features misbehave on CoordBase maps.
// CLICK-vs-ORDER TRACER (2026-08-18, user-requested): every encode logs
// cell -> number, every decode logs number -> cell. Clicking around with the
// waypoint tool then yields a paired trace; a decode with no matching encode
// (or coords that differ) fingers a FOREIGN codec (Phobos/Antares planning
// code) and its arithmetic. Shared cap keeps the log bounded.
static bool TraceBudget()
{
    static int n = 0;
    return ++n <= 600;
}
DEFINE_HOOK(6E6B51, CellTarget_Encode1_CoordBase, 6)
{
    if (g_CoordBase <= 1000) return 0;
    const DWORD y = R->ECX(), x = R->EDX();
    if (TraceBudget())
        DeployDiagLog("ENC1 (%d,%d) -> %u\n", (int)x, (int)y, x + y * (DWORD)g_CoordBase);
    R->ECX(y * (DWORD)(g_CoordBase >> 3));          // ecx=Y -> Y*(base/8)
    return 0x6E6B5A;                                // lea ecx,[edx+ecx*8]; store
}
DEFINE_HOOK(6E6B89, CellTarget_Encode2_CoordBase, 6)
{
    if (g_CoordBase <= 1000) return 0;
    const int y = (int)R->EAX();
    if (TraceBudget())
    {
        const int xlep = *reinterpret_cast<int*>(R->ESI());     // coord.X leptons
        DeployDiagLog("ENC2 (%d,%d)\n", xlep >> 8, y);
    }
    R->EDI((DWORD)y * (DWORD)(g_CoordBase >> 3));   // eax=Y -> edi=Y*(base/8)
    return 0x6E6B92;                                // X conv; lea eax,[eax+edi*8]
}
// Vanilla parity (user-verified behavior): an off-map order travels to the
// NEAREST map cell -- vanilla's click path clamps. Our decode is faithful, so
// off-diamond coords previously fell through to the dummy (-> no-op). Clamp
// them into the map diamond here instead: convert iso (rx,ry) -> cartesian,
// clamp to [0,W-1]x[0,H-1], convert back. Exact in-diamond coords (even
// cartesian doubles in range) pass through untouched.
static void ClampCellToMap(int& rx, int& ry)
{
    // The iso diamond is DENSE: valid cells exist at BOTH parities of rx+ry
    // (half-offset rows; (2W-1)*H cells total). From the map codec: dy = rx+ry
    // -W-1 in [0, 2H-1], dx = rx-ry+W-1 in [0, 2W-2], ANY integer pair in those
    // ranges is a real cell. (An earlier revision wrongly required even parity
    // and shifted half of all in-map clicks by one cell.)
    const int W = *reinterpret_cast<int*>(0x87F8DC);   // MapRect.W / .H
    const int H = *reinterpret_cast<int*>(0x87F8E0);
    if (W <= 0 || H <= 0) return;
    int dx = rx - ry + W - 1;
    int dy = rx + ry - W - 1;
    if (dx >= 0 && dx <= 2 * W - 2 && dy >= 0 && dy <= 2 * H - 1)
        return;                                        // already a valid cell
    if (dx < 0) dx = 0; else if (dx > 2 * W - 2) dx = 2 * W - 2;
    if (dy < 0) dy = 0; else if (dy > 2 * H - 1) dy = 2 * H - 1;
    if ((dx + dy) & 1) { if (dy > 0) --dy; else ++dy; }   // rx must be integral
    rx = ((dx + dy) >> 1) + 1;
    ry = ((dy - dx) >> 1) + W;
}
DEFINE_HOOK(6E6ED6, CellTarget_Decode1_CoordBase, 5)
{
    if (g_CoordBase <= 1000) return 0;
    const unsigned N = R->ECX(), b = (unsigned)g_CoordBase;
    int rx = (int)(N % b), ry = (int)(N / b);
    ClampCellToMap(rx, ry);
    if (TraceBudget()) DeployDiagLog("DEC1 %u -> (%d,%d)\n", N, rx, ry);
    *reinterpret_cast<short*>(R->ESP() + 4) = (short)rx;        // X (orig @0x6E6EE5)
    R->EDX((DWORD)ry);                                          // Y; tail: eax=edx,shr 31(=0),add
    return 0x6E6EEF;
}
DEFINE_HOOK(6E7C2C, CellTarget_Decode2_CoordBase, 5)
{
    if (g_CoordBase <= 1000) return 0;
    const unsigned N = R->ECX(), b = (unsigned)g_CoordBase;
    int rx = (int)(N % b), ry = (int)(N / b);
    ClampCellToMap(rx, ry);
    if (TraceBudget()) DeployDiagLog("DEC2 %u -> (%d,%d)\n", N, rx, ry);
    *reinterpret_cast<short*>(R->ESP() + 4) = (short)rx;        // X (orig @0x6E7C39)
    R->EDX((DWORD)ry);
    return 0x6E7C43;
}

// ============================================================
//  DISPLAYCLASS CELL-NUMBER DECODERS (the waypoint/planning feature, 2048).
//
//  The recon catalogued SIX base-1000 idiv cell decoders. The event codec pair
//  (0x6E6ED0/0x6E7C20) is patched above; the waypoint READER 0x68BE0C is owned
//  by the Phobos fix. These two DisplayClass decoders were deferred as "text
//  parse" -- WRONG: 0x4ADC17 is a loop decoding (index, packed-cell) pairs into
//  Scenario waypoint slots via the setter @0x68BF50 = the waypoint/planning
//  path decode, and 0x4AD232 is its sibling (same dual-format decode, gated on
//  the scenario-format flag @0xA8ED7C). Left at base-1000 they decode our
//  base-2048 numbers as Y ~= 2*Y -> planned paths point past the SE corner /
//  opposite directions (user-observed via the waypoint feature 2026-08-18).
//  Same hook shape as the codec decoders; the div/magic tails add +0 for Y>=0.
//  (Sixth decoder 0x71CAEF, forest-fire/random: an `add esp,4` sits inside the
//  hook window and R->ESP writes don't survive Syringe's popad -- deferred, not
//  order-related.)
DEFINE_HOOK(4AD232, CellNum_Decode_Display_CoordBase, 5)
{
    if (g_CoordBase <= 1000) return 0;
    const unsigned N = R->ECX(), b = (unsigned)g_CoordBase;
    if (TraceBudget()) DeployDiagLog("DSP  %u -> (%u,%u)\n", N, N % b, N / b);
    *reinterpret_cast<short*>(R->ESP() + 0x60) = (short)(N % b);   // X (orig @0x4AD23F)
    R->EDX(N / b);                                                 // Y
    return 0x4AD249;
}
DEFINE_HOOK(4ADC17, CellNum_Decode_WaypointList_CoordBase, 5)
{
    if (g_CoordBase <= 1000) return 0;
    const unsigned N = R->ECX(), b = (unsigned)g_CoordBase;
    R->ESI(R->ESI() + 4);                                          // replicate skipped add esi,4
    if (TraceBudget()) DeployDiagLog("WPL  %u -> (%u,%u) idx=%d\n", N, N % b, N / b, (int)R->EBX());
    *reinterpret_cast<short*>(R->ESP() + 0x18) = (short)(N % b);   // X (orig @0x4ADC26)
    R->EDX(N / b);                                                 // Y
    return 0x4ADC30;
}

// ============================================================
//  DUMMY-DESTINATION WRITE TRAP (the human-order decode bug, stride 2048).
//
//  The 3-site Phobos coord-cell fix did NOT cure it: clicked units still get
//  Destination = the OOB dummy 0xABDC50, and neither the sentinel-fail traps
//  nor the loose Phobos rescan see the failing decode. So trap the one step
//  the bug cannot avoid: the store into FootClass+0x5A4. All 13 writer
//  instructions (6-byte movs) are hooked; we log ONLY when the stored value is
//  the dummy, with a raw stack scan for return-address candidates (gamemd
//  .text 0x401000-0x7E0000 and DLL range 0x70000000+) -- the caller chain
//  names the decode function without needing any frame layout.
static void LogDummyDestWrite(DWORD site, DWORD value, DWORD obj, DWORD esp)
{
    if (value != 0xABDC50) return;
    static int total = 0;
    if (++total > 40) return;
    char chain[256]; chain[0] = '\0'; int pos = 0, found = 0;
    for (int off = 0; off <= 0x80 && found < 7; off += 4)
    {
        DWORD v = *reinterpret_cast<DWORD*>(esp + off);
        if ((v >= 0x401000 && v < 0x7E0000) || (v >= 0x70000000 && v < 0x80000000))
        {
            pos += sprintf_s(chain + pos, sizeof(chain) - pos, " %X", v);
            ++found;
        }
    }
    DeployDiagLog("DUMMYDEST @%X foot=%X stack:%s\n", site, obj, chain);
}
// LOG-ONLY. An earlier revision substituted NULL for the dummy here -- WRONG:
// the engine uses the dummy cell as a legitimate flow-control marker all over
// (10+ `cmp 0xABDC50` sites: infantry scatter 0x51FE41, cell-region code
// 0x56Bxxx, CellClass 0x47D2B8 ...), and nulling it corrupted normal unit
// behavior map-wide (user-observed 2026-08-18). The decode-side clamp now
// prevents clicks from ever resolving to the dummy, so these hooks are pure
// diagnostics again: original store always runs (return 0).
#define DEST_WRITE_HOOK(addr, objreg, valreg)                                  \
DEFINE_HOOK(addr, DestWrite_##addr, 6)                                         \
{                                                                              \
    if (g_MapStride > 512)                                                     \
        LogDummyDestWrite(0x##addr, R->valreg(), R->objreg(), R->ESP());       \
    return 0;                                                                  \
}
DEST_WRITE_HOOK(4D32C7, ESI, EBX)
DEST_WRITE_HOOK(4D5A01, EBX, EDI)
DEST_WRITE_HOOK(4D9510, EBP, ESI)
DEST_WRITE_HOOK(4D96BC, EBP, ESI)
DEST_WRITE_HOOK(4D9AC3, ESI, EBP)
DEST_WRITE_HOOK(4DF0D8, ECX, EAX)
DEST_WRITE_HOOK(54B44B, ECX, EAX)
DEST_WRITE_HOOK(665DD3, ESI, EBX)
DEST_WRITE_HOOK(66ECC1, ESI, EAX)
DEST_WRITE_HOOK(67890E, ESI, EAX)
DEST_WRITE_HOOK(710F93, ESI, EBP)
DEST_WRITE_HOOK(7138D7, EBP, EAX)
DEST_WRITE_HOOK(73A4E9, EBP, EAX)

DEFINE_HOOK(4D3920, UpdatePathfinding_Diag, 5)
{
    if (g_MapStride > 512)
    {
        DWORD esp = R->ESP();
        int sx = *reinterpret_cast<short*>(esp + 0x4);
        int sy = *reinterpret_cast<short*>(esp + 0x6);
        // Passability diagnostic. For the unit's start cell log: Level (height),
        // LandType (drives passability: 2=Water should be impassable to land units),
        // SlopeIndex (ramp shape), IsoTileTypeIndex (visual tile). Then the 8
        // neighbours' LandTypes so a land/water boundary is visible.
        // "Walk on water" => a unit standing on visual water shows LT != 2 (LandType
        // mis-assigned) OR LT==2 but pathing proceeds anyway (zone/pathfinder bug).
        // Stuck-on-ramp => same (sx,sy) repeats with a higher-Level neighbour.
        // CORRECTED arg layout (verified in disasm 0x4D392A-0x4D39CC): arg1
        // @[esp+4] is the packed TARGET CellStruct (converted *256+128 to
        // leptons and range-checked); arg2 @[esp+8] is a pointer. So (sx,sy)
        // here = the TARGET the pathfinder is asked to reach. The 2026-08-17
        // corner-order data (this very line, mislabeled "start") showed the
        // target arrives PRE-FOLDED: each axis truncated to 10 bits (rx/ry
        // -1024 when >=1024) -- invisible at strides 512/1024, folds at 2048.
        // Bracket probe: ALSO log the unit's stored Destination object
        // (FootClass+0x5A4, set by SetDestination at order execution). If the
        // stored destination is folded too -> fold is upstream (click/event
        // pipeline); if it is correct while the arg is folded -> the fold is in
        // the mission/locomotor target->cell conversion.
        int dx = -1, dy = -1; DWORD dvt = 0;
        DWORD dest = *reinterpret_cast<DWORD*>(R->ECX() + 0x5A4);
        if (dest > 0x10000 && dest < 0x60000000)
        {
            dvt = *reinterpret_cast<DWORD*>(dest);
            dx  = *reinterpret_cast<short*>(dest + 0x24);   // CellClass MapCoords
            dy  = *reinterpret_cast<short*>(dest + 0x26);
        }
        DeployDiagLog("PATH 0x%X TGT(%d,%d) dest=%08X vt=%08X destcell(%d,%d)\n",
                      R->ECX(), sx, sy, dest, dvt, dx, dy);
    }
    R->EAX(0x1F9C);          // replicate mov eax,0x1f9c
    return 0x4D3925;
}

// ============================================================
//  SUBZONE-ID saturation (big-map pathfinding fix, per external analysis)
//  The zone/subzone system stores IDs as 16-bit words in the MapClass arrays
//  @[Map+0x6c]/[+0x70]; ~14 consumers `movsx` (sign-extend) them. On big maps
//  the region count exceeds 0x7FFF -> IDs go negative, and a 0x10000 producer
//  value truncates to 0 (= "unvisited") -> recursive region construction revisits
//  cells and can overflow the stack. The producer @0x58215B assigns the ID with
//  `mov cx,[esp+0x10]`, where [esp+0x10] is the FULL 32-bit id (set to ebp
//  @0x58210F). We saturate it into the signed-16-bit range (<=0x7FFF): every
//  existing movsx stays positive, no value wraps to 0 (cells still get marked
//  visited -> no infinite recursion). No consumer/save-format changes. NO-OP at
//  stride 512 and for maps with <=0x7FFF subzones (the cap never triggers).
DEFINE_HOOK(58215B, Subzone_SaturateID, 5)   // mov cx,[esp+0x10]
{
    if (g_MapStride > 512)
    {
        DWORD id = *reinterpret_cast<DWORD*>(R->ESP() + 0x10);
        // Curated mode applies his 14 movsx->movzx consumers, so the full unsigned
        // range is safe: cap at 0xFFFE (reserve 0xFFFF). Broad mode keeps signed
        // consumers -> cap at 0x7FFF to stay positive.
        DWORD cap = g_CuratedBase ? 0xFFFEu : 0x7FFFu;
        if (id > cap) id = cap;
        R->ECX((R->ECX() & 0xFFFF0000u) | (id & 0xFFFF));  // replicate mov cx with the cap
        return 0x582160;                                   // continue past the mov
    }
    return 0;   // stride 512: run original
}

// ------------------------------------------------------------------
//  Safety net for the full-map cell iterator @0x578290 (the passability/
//  movement-zone walk fixed by ApplyIteratorStridePatches). The walk terminates
//  by hitting NULL border cells; if the stride-adjusted geometry ever fails to
//  land on that null border, the walk would step the slot pointer past the
//  Cells.Items array and return a GARBAGE cell -> UpdatePassability then writes
//  through it and corrupts a live object's vtable (observed crash: virtual call
//  to heap garbage 0x07BF0035, via the coord-transform path). This guard checks
//  the current slot pointer [Map+0x118] at entry; if it is outside
//  [Items, Items + MapTotal*4) it returns a NULL cell (EAX=0), which makes the
//  caller's `while (cell != null)` loop stop cleanly. Jumps to the bare `ret`
//  at 0x5782D4 (no registers were pushed yet at entry, so no imbalance).
//  When the geometry is correct this never fires; it only converts a would-be
//  OOB corruption into a clean, early loop termination. NO-OP at stride 512.
static int g_IterGuardFires = 0;
DEFINE_HOOK(578290, CellIterator_OOBGuard, 6)
{
    const DWORD map   = R->ECX();
    const DWORD items = *reinterpret_cast<DWORD*>(map + 0x13C);   // Cells.Items
    const DWORD cur   = *reinterpret_cast<DWORD*>(map + 0x118);   // current slot ptr
    if (g_MapStride > 512 && items)
    {
        bool stop = (cur < items || cur >= items + static_cast<DWORD>(g_MapTotal) * 4); // slot OOB
        DWORD cell = 0;
        if (!stop)
        {
            cell = *reinterpret_cast<DWORD*>(cur);   // cell ptr in this slot (slot is in-bounds)
            // The population loop fills only the iso-diamond of valid cells; the array
            // corners outside it are never written and hold GARBAGE cell pointers.
            // Address-range checks fail (real cells can be below 0x10000000), so use
            // CELL IDENTITY: a real cell at slot N has MapCoords (X@+0x24, Y@+0x26)
            // equal to N's (X = idx % stride, Y = idx / stride). Garbage won't match.
            // Stopping at the first mismatch = the valid/garbage boundary = the same
            // place vanilla's walk terminates on a null border cell.
            if (cell != 0)
            {
                if (cell < 0x00400000 || cell >= 0x40000000)
                    stop = true;                       // wild ptr: don't even deref it
                else
                {
                    unsigned idx = static_cast<unsigned>((cur - items) / 4);
                    int ex = static_cast<int>(idx % static_cast<unsigned>(g_MapStride));
                    int ey = static_cast<int>(idx / static_cast<unsigned>(g_MapStride));
                    int cx = *reinterpret_cast<short*>(cell + 0x24);
                    int cy = *reinterpret_cast<short*>(cell + 0x26);
                    if (cx != ex || cy != ey) stop = true;   // coords don't match slot -> garbage
                }
            }
        }
        if (stop)
        {
            if (g_IterGuardFires < 25)
            {
                ++g_IterGuardFires;
                DeployDiagLog("ITER guard #%d: slot=0x%X cell=0x%X items=0x%X -> stop\n",
                              g_IterGuardFires, cur, cell, items);
            }
            R->EAX(0);           // null cell -> caller's iteration loop stops
            return 0x5782D4;     // bare `ret`
        }
    }
    R->EAX(*reinterpret_cast<DWORD*>(map + 0x114));   // replicate mov eax,[ecx+0x114]
    return 0x578296;
}

// ============================================================
//  OVERLAY-DRAW DISPATCH DIAGNOSTIC (stride>512). Two paths read the wall frame:
//  PATH 1 @0x47f908 (SlopeIndex-based, terrain-following overlays) and PATH 2
//  @0x47f96a (wall-flag [ebx+0x2a8] -> connection draw). Probe BOTH so whichever
//  path a wall takes is captured, with the stored frame (cell+0x11E). If walls
//  draw with frame 0 while a neighbouring wall exists, the connection PRODUCER
//  (0x47E044) failed to see the neighbour at stride 1024; if the frame is
//  correct but segments still don't join, the fault is downstream (draw/iso).
//  ESI=cell in both; EBX=OverlayTypeClass in path 2.
// ============================================================
DEFINE_HOOK(47F908, OverlayP1_Diag, 6)
{
    static int fires = 0;
    if (g_MapStride > 512 && fires < 60)
    {
        DWORD cell = R->ESI();
        int X = *reinterpret_cast<short*>(cell + 0x24);
        int Y = *reinterpret_cast<short*>(cell + 0x26);
        if (X >= 0 && X < 1024 && Y >= 0 && Y < 1024)
        {
            int ovl   = *reinterpret_cast<int*>(cell + 0x44);
            int fr    = *reinterpret_cast<unsigned char*>(cell + 0x11E);
            int slope = *reinterpret_cast<unsigned char*>(cell + 0x11C);
            if (ovl >= 0)   // only overlay-bearing cells
            {
                ++fires;
                DeployDiagLog("OVL-P1 #%d cell(%d,%d) overlay=%d frame=0x%X slope=%d\n",
                              fires, X, Y, ovl, fr, slope);
            }
        }
    }
    return 0;   // continue (mov cl,[esi+0x11c])
}

DEFINE_HOOK(47F96A, OverlayP2_Diag, 6)
{
    static int fires = 0;
    if (g_MapStride > 512 && fires < 60)
    {
        DWORD cell  = R->ESI();
        DWORD otype = R->EBX();
        int X = *reinterpret_cast<short*>(cell + 0x24);
        int Y = *reinterpret_cast<short*>(cell + 0x26);
        if (X >= 0 && X < 1024 && Y >= 0 && Y < 1024)
        {
            int ovl  = *reinterpret_cast<int*>(cell + 0x44);
            int fr   = *reinterpret_cast<unsigned char*>(cell + 0x11E);
            int flag = *reinterpret_cast<unsigned char*>(otype + 0x2A8);
            int aidx = *reinterpret_cast<int*>(otype + 0x294);
            ++fires;
            DeployDiagLog("OVL-P2 #%d cell(%d,%d) overlay=%d frame=0x%X wallFlag=%d otypeArrayIdx=%d %s\n",
                          fires, X, Y, ovl, fr, flag, aidx,
                          flag ? "-> WALL connect draw" : "-> non-wall draw");
        }
    }
    return 0;   // continue (mov cl,[ebx+0x2a8])
}

// ============================================================
//  SHARED CELL-LOOKUP CALLER PROBE (stride>512). 0x5657a0 = MapClass::
//  operator[](CellStruct&) -- EVERY overlay/wall producer funnels its neighbour
//  resolution through it (EDX=CellStruct* {X@+0, Y@+2}). We hook @0x5657BB
//  (`mov ecx,[ecx+0x13c]`, 6 bytes) -- reached only after the in-bounds check
//  passed, so ESI is already popped and [ESP] holds the CALLER return address.
//  Logging caller + requested (X,Y), gated to the wall build area, reveals which
//  function computes NAWALL's connection and exactly which neighbour cell it asks
//  for -- so a wrong N/S request (row-crossing) becomes visible regardless of
//  which producer it is. EAX here already = the computed cell index.
// ============================================================
static int g_WConnFires = 0;
DEFINE_HOOK(5657BB, CellLookup_CallerProbe, 6)
{
    if (g_MapStride > 512 && g_WConnFires < 500)
    {
        DWORD esp    = R->ESP();
        DWORD caller = *reinterpret_cast<DWORD*>(esp);         // return address
        DWORD cs     = R->EDX();                               // CellStruct*
        int rx = *reinterpret_cast<short*>(cs + 0);
        int ry = *reinterpret_cast<short*>(cs + 2);
        if (rx >= 65 && rx < 90 && ry >= 65 && ry < 115)       // wall build window
        {
            DeployDiagLog("LOOK caller=0x%X req(%d,%d) idx=%d\n",
                          caller, rx, ry, R->EAX<int>());
            ++g_WConnFires;
        }
    }
    R->ECX(*reinterpret_cast<DWORD*>(R->ECX() + 0x13C));       // replicate mov ecx,[ecx+0x13c]
    return 0x5657C1;
}
