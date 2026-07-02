#include "MapSizeExt.h"
#include "Config.h"
#include <Syringe.h>
#include <windows.h>

// ============================================================
//  Hooks.cpp
//  Patches the three minimum-viable sites for map size.
//
//  PHASE 1 (this file):
//    Hook A - MapClass::operator[] stride + bounds check
//    Hook B - Heap allocation size
//    Hook C - Inline cell array access stride
//
//  All other sites (sight reveal cluster, radar, lepton
//  operator[]) are tagged VERIFY and left as-is for now.
//  The game will crash on large maps until those are patched,
//  but on a standard 512-stride map this DLL should be a no-op.
// ============================================================

// Global runtime values - set in SyringeHandshake
int g_MapStride       = 512;
int g_MapTotal        = 262144;  // 512 * 512
int g_MapMaxW         = 512;
int g_MapMaxH         = 512;
int g_MapMaxDimension = 512;     // per-axis gate (replaces cmp ax,0x200)

// ============================================================
//  HOOK A: MapClass::operator[] (Cell& version)
//  Original:
//    5656EA: shl eax, 0x9        ; Y * 512
//    5656ED: add eax, ecx        ; + X
//    5656EF: js  0x565709        ; negative -> fallback
//    5656F1: cmp eax, 0x40000    ; >= 262144 -> fallback
//    5656F6: jge 0x565709
//
//  We replace at 5656EA: compute Y * g_MapStride + X,
//  then check against g_MapTotal.
//
//  Hook entry: eax = Y (sign-extended), ecx = X (sign-extended)
//  We have 7 bytes available before the 'js' at 5656EF.
//  Strategy: hook at 5656EA, replace shl+add with a call
//  to our function, return the index in eax.
//
//  VERIFY: confirm register state at entry matches assumption.
//  If the game crashes here on load, check eax/ecx values
//  in a debugger at 0x5656EA.
// ============================================================

DEFINE_HOOK(0x5656EA, MapClass_OperatorBracket_Stride, 7)
{
    // At entry: eax = Y (movsx from word), ecx = X (movsx from word)
    // Original did: eax = (eax << 9) + ecx
    // We do:        eax = (eax * g_MapStride) + ecx
    GET_STACK(int, Y, 0x4);   // VERIFY: adjust offset if wrong
    GET_STACK(int, X, 0x0);   // VERIFY: adjust offset if wrong

    // Read directly from registers via R macro
    int y = R->EAX;
    int x = R->ECX;

    int index = y * g_MapStride + x;

    // Replicate the 'js' (negative index) check
    if (index < 0)
    {
        R->EAX = index;
        // Jump to the fallback at 0x565709
        return 0x565709;
    }

    R->EAX = index;

    // Fall through to the cmp at 5656F1 - but we patch that too below,
    // so skip it and do the check here, jump past it to 5656F8.
    if (index >= g_MapTotal)
        return 0x565709;    // fallback sentinel

    return 0x5656F8;        // skip original cmp/jge, go straight to array load
}

// ============================================================
//  HOOK B: Heap allocation sized by N * 512
//  Original at 0x48EB18:
//    mov  edx, [esi+0x16c]   ; load dimension N
//    shl  edx, 0x9           ; N * 512  <- we hook here
//    push edx
//    call 0x7C8E17           ; malloc(N * 512)
//
//  We replace shl edx,0x9 with edx * g_MapStride.
//  6 bytes available (shl is 3 bytes, push is 1, but we
//  hook at the shl so we have its 3 bytes + next instruction).
//
//  VERIFY: confirm [esi+0x16c] is actually the map height
//  and not something else. Check what value edx holds here
//  on a standard map load (expect 512 or map height value).
// ============================================================

DEFINE_HOOK(0x48EB18, MapClass_Alloc_Stride1, 6)
{
    // edx = map dimension loaded from [esi+0x16c]
    // original: edx <<= 9  (multiply by 512)
    // ours:     edx *= g_MapStride
    R->EDX = R->EDX * g_MapStride;

    // Resume at push edx (0x48EB1B)
    return 0x48EB1B;
}

DEFINE_HOOK(0x48EB35, MapClass_Alloc_Stride2, 6)
{
    // ecx = (map_height - 1) >> 1  (half-dimension calculation)
    // original: ecx <<= 9
    // ours:     ecx *= g_MapStride
    R->ECX = R->ECX * g_MapStride;

    // Resume at whatever follows (0x48EB38)
    // VERIFY: check what instruction is at 0x48EB38
    return 0x48EB38;
}

// ============================================================
//  HOOK C: Inline cell array access via global 0x87F924
//  Original at 0x483B32:
//    movsx edi, [esi+0x26]   ; Y
//    movsx ecx, [esi+0x24]   ; X
//    shl   edi, 0x9          ; Y * 512  <- hook here
//    add   edi, ecx          ; + X = index
//    mov   ecx, [eax+edi*4]  ; Array[index]
//
//  VERIFY: confirm edi=Y, ecx=X at entry.
// ============================================================

DEFINE_HOOK(0x483B32, MapClass_InlineAccess_Stride, 6)
{
    // edi = Y, ecx = X (both movsx'd from word fields)
    R->EDI = R->EDI * g_MapStride + R->ECX;

    // Resume at: add edi, ecx -- but we already did the add,
    // so skip to mov ecx,[eax+edi*4] at 0x483B42
    // VERIFY: confirm 0x483B42 is 'mov ecx,[eax+edi*4]'
    return 0x483B42;
}

// ============================================================
//  NOTE: Lepton operator[] at 0x565757
//  The bound check there reads [ecx+0x140] dynamically,
//  so only the stride needs patching.
//  VERIFY: is [ecx+0x140] actually updated to reflect
//  g_MapTotal somewhere, or is it set from a map INI value?
//  If it reads MapCellWidth*MapCellHeight (playable area)
//  rather than total grid size, it may already be correct
//  and only the stride at 0x565757 needs a hook.
// ============================================================

DEFINE_HOOK(0x565757, MapClass_LeptonOp_Stride, 6)
{
    // edx = Y (sar'd from coord), esi = X (sar'd from coord)
    // original: shl edx,0x9 ; add edx,esi
    R->EDX = R->EDX * g_MapStride + R->ESI;

    // Skip to: js 0x56577a (which follows add edx,esi at 0x56575A)
    // VERIFY: confirm 0x56575C is 'js 0x56577a'
    return 0x56575C;
}

// ============================================================
//  MAP DIMENSION GATE HOOKS
//  Three sites check W < 512 and H < 512 independently.
//  We replace the threshold with g_MapMaxDimension.
//
//  Register state at all sites:
//    ax = Width  (loaded from map header [ebx+0x24] or similar)
//    cx = Height (loaded from map header [ebx+0x26] or similar)
//    bp or bx = minimum dimension (small value, ~4)
//
//  Strategy: hook the 6-byte jge that follows cmp ax,0x200,
//  redo the comparison against g_MapMaxDimension, then
//  also check cx ourselves and jump to reject or accept.
// ============================================================

// --- Site 1: 0x4C562C/0x4C563F ---
// cmp ax,0x200   (4 bytes) at 0x4C562C
// jge 0x4C588B   (6 bytes) at 0x4C5630  <- hook here
// ...
// cmp cx,0x200   (5 bytes) at 0x4C563F
// jge 0x4C588B   (6 bytes) at 0x4C5644
//
// Hook the first jge (6 bytes), do both checks ourselves,
// jump to reject (0x4C588B) or accept (0x4C564A).

DEFINE_HOOK(0x4C5630, MapDimGate_Site1_Width, 6)
{
    short w = (short)(R->EAX & 0xFFFF);
    if (w >= (short)g_MapMaxDimension)
        return 0x4C588B;  // reject
    // width OK - now check height (cx)
    short h = (short)(R->ECX & 0xFFFF);
    if (h >= (short)g_MapMaxDimension)
        return 0x4C588B;  // reject
    // both OK - skip original cx check entirely, go to accept
    return 0x4C564A;
}

// --- Site 2: 0x4C590E/0x4C5919 ---
// jge here is only 2 bytes, so hook the cmp+jge together:
// cmp ax,0x200   (4 bytes) at 0x4C590E  <- hook here (4+2 = 6)
// jge 0x4C5972   (2 bytes) at 0x4C5912

DEFINE_HOOK(0x4C590E, MapDimGate_Site2_Width, 6)
{
    short w = (short)(R->EAX & 0xFFFF);
    if (w >= (short)g_MapMaxDimension)
        return 0x4C5972;  // reject
    short h = (short)(R->ECX & 0xFFFF);
    if (h >= (short)g_MapMaxDimension)
        return 0x4C5972;  // reject
    return 0x4C5920;      // skip original cx check, go to accept
}

// --- Site 3: 0x554BC1/0x554BD4 ---
// cmp ax,0x200   (4 bytes) at 0x554BC1
// jge 0x554CE4   (6 bytes) at 0x554BC5  <- hook here

DEFINE_HOOK(0x554BC5, MapDimGate_Site3_Width, 6)
{
    short w = (short)(R->EAX & 0xFFFF);
    if (w >= (short)g_MapMaxDimension)
        return 0x554CE4;  // reject
    short h = (short)(R->ECX & 0xFFFF);
    if (h >= (short)g_MapMaxDimension)
        return 0x554CE4;  // reject
    return 0x554BDF;      // skip original cx check, go to accept
}
