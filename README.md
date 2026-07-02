# MapSizeExt

A Syringe-injected DLL for Yuri's Revenge that patches the hardcoded 512-cell
map stride, enabling map grids larger than 512×512.

## Status: Phase 1 — Proof of Concept

Only the minimum three hook sites are patched. The game will load and run on
standard maps. Larger maps will crash until the sight/shroud cluster and radar
sites are also hooked (Phase 2).

## Hook sites confirmed by binary analysis of gamemd.exe

| Address | What | Status |
|---|---|---|
| `0x5656EA` | `MapClass::operator[]` stride (`shl eax,0x9`) | ✅ Hooked |
| `0x5656F1` | `MapClass::operator[]` bounds (`cmp eax,0x40000`) | ✅ Hooked (via operator[] hook) |
| `0x48EB18` | Heap allocation stride 1 | ✅ Hooked |
| `0x48EB35` | Heap allocation stride 2 | ✅ Hooked |
| `0x483B32` | Inline cell array access stride | ✅ Hooked |
| `0x565757` | `operator[]` lepton variant stride | ✅ Hooked |
| `0x493CF1`–`0x495F39` | Sight/shroud reveal row calc (~30 sites) | ⏳ Phase 2 |
| `0x497906`+ | More sight reveal row calcs | ⏳ Phase 2 |
| `0x53D49E` | Radar/minimap rendering | ⏳ Phase 2 |
| `0x547DC7` | Sight reveal row calc | ⏳ Phase 2 |
| `0x429AB1/AC2` | Two-cell distance calc | ⏳ Phase 2 |

## Setup

### Prerequisites
- YRpp headers (clone into `YRpp/` subdirectory or adjust include path)
- Syringe (already in your YR install)

### Build
Push to GitHub — Actions builds `MapSizeExt.dll` automatically.

### Install
1. Copy `MapSizeExt.dll` to your YR game directory
2. Copy `MAPSIZEEXT.INI` to your YR game directory
3. Add `-i=MapSizeExt.dll` to your Syringe arguments (alongside Ares/Phobos)
4. Launch the game

### Verify it loaded
Check `MapSizeExt.log` in the game directory. It should show:
```
MapSizeExt v0.1
Stride   = 512
Total    = 262144
...
```
If the log doesn't appear, Syringe didn't load the DLL.

## Key addresses discovered

```
MapClass::operator[](Cell&)     0x5656D0
  - Array.data ptr at [this+0x13C]
  - Total size at    [this+0x140]
  - Fallback sentinel cell at 0xABDC50

MapClass::operator[](lepton)    0x565757
  - Already reads bound from [ecx+0x140] dynamically

Heap alloc for map buffer       0x48EB18
  - malloc called at 0x7C8E17
  - Allocates N * stride bytes

Global cell array pointer       0x87F924
  - Used by inline access at 0x483B32
```

## VERIFY tags

Several places in `Hooks.cpp` are tagged `VERIFY:`. These are assumptions
about register state that need confirming in a debugger if the game crashes
on load. If you get a crash immediately after the Syringe splash:

1. Attach a debugger (x32dbg) to gamemd.exe after Syringe injects
2. Set a breakpoint at `0x5656EA`
3. Check that `eax` = Y coordinate and `ecx` = X coordinate
4. If reversed, swap `R->EAX` and `R->ECX` in `Hook A` in `Hooks.cpp`

## Phase 2 plan

The sight/shroud cluster (`0x493xxx–0x495xxx`, ~30 sites) all follow the
same pattern:
```asm
shl  eax,0x6
add  eax,eax     ; * 65
lea  eax,[...]
sar  eax,0xb     ; / 2048  = pixel-to-cell conversion
cmp  eax,0xfe    ; clamp to 254 (max visible row)  <-- this also needs updating
shl  eax,0x9     ; * 512 stride  <-- hook target
add  eax,edi     ; + base
```
Each of these can be handled with a single hook function called from all
30 sites, since the pattern is identical. The `cmp eax,0xfe` (row clamp)
also needs to change to `cmp eax, g_MapMaxH - 2` for larger maps.
