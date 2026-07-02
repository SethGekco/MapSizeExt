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
int g_MapStride = 512;
int g_MapTotal  = 262144;   // 512 * 512
int g_MapMaxW   = 512;
int g_MapMaxH   = 512;

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
