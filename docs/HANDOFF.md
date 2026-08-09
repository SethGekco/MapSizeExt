# MapSizeExt — Session Handoff (continue here)

**Read `docs/BUG-ATLAS.md` first** — it is the full bug encyclopedia. This file
is the "where we are right now / what to do next" pointer.

## Goal
Raise RA2/Yuri's Revenge cell-plane stride 512→1024 so large maps (goal
**300×300**, his base did 250×250) load and play. Target `gamemd-spawn.exe`
(CnCNet spawner), pinned gamemd SHA `3e81a617…d308600`.

## THE WINNING APPROACH (current)
Do **NOT** use our old broad MSVC byte-sweep (`src/*.cpp`, `CuratedBase` mode) —
it has wall + sidebar false positives (§2.1/2.2). Instead **build Krisztiaan's
proven source + our 300×300 fixes**, which is vendored in **`curated/`**:

```
cd /home/rex/MapSizeExt/curated && ./build.sh      # local mingw, no Docker/CI/MSVC
cp MapSizeExt.dll /home/rex/snap/cncra2yr/common/.wine/drive_c/Westwood/RA2/MapSizeExt.dll
```
(build = `i686-w64-mingw32-g++ -std=gnu++11 -shared -static-libgcc
-Wl,--enable-stdcall-fixup -o MapSizeExt.dll yr_map_512_plane_probe.c -lpsapi`).
His DLL is **hardcoded 1024**, ignores `MAPSIZEEXT.INI`. Logs to
`yr_map_512_plane_probe.csv` in the RA2 dir.

## CURRENT STATE (build md5 `8efc3e67`, installed)
**300×300 LOADS AND PLAYS.** Working: walls connect, sidebar correct, building
foundations correct, unit movement, pathfinding, deploy, radar, shroud,
bottom-right + most edge orders.

**ONE REMAINING BUG:** ordering a unit to the extreme **BOTTOM-LEFT corner via
the radar/minimap** → fatal. EIP `0x021B9CA4` (heap = virtual-call into garbage),
coords ~(307,249). See §2.17.

## What we added to his source (all in `curated/`, documented in BUG-ATLAS M4)
Patch table (`yr_map_512_patch_table.h`) additions on top of his 74:
- 2 plane-iterator bounds: `0x565bd0` (mov `0x200`→`0x400`), `0x565bf6`
  (cmp `0x40000`→`0x100000`).
- 27 iterator `shl 0xB→0xC` end-pointer sites (`0x568c1e` …). §2.4.
- 8 iterator `shl 9→0xA` sites: `0x5780b4 0x57865f 0x57881d 0x57889b 0x5789d0
  0x578a31 0x578a74 0x578adb` (broad had these; fix routing so the 0x660540
  guard doesn't wrap bottom-right).
- 2 inverse-conversion sign-extend: `0x565c7e`,`0x566f9a`
  (`or 0xFFFFFE00`→`0xFFFFFC00`) — fixes bottom-right→top-left **wrap**. §2.16.
- 73 Antares.dll `shl 9→0xA` (module table). **Did NOT fix bottom-left — ruled
  out.** (The 50 CMP Antares sites were NOT added — my parse of `kAntaresCmp`
  in `src/AresPhobosSites.h` was misaligned; it is a `[3]` array
  `{rva, imm_offset, oldval}`. Fix the parse if you want them.)

Hooks (added in his `.syhks00` / `SyringeRegisters` style, `return 0` = continue
after stolen bytes):
- `Map512CellSlotGuard` @`0x5663BC` — plane-init `-1` guard: in the construction
  loop, a plane slot holding a non-heap pointer is treated as empty (0) so a
  fresh cell is allocated. Fixes the load crash `0x410174`. §2.5.
- `Map512CellIteratorGuard` @`0x578290` — stop the full-map iterator on a **wild
  cell pointer** (outside `[0x400000,0x40000000)`). Fixes `0x568C3B`/`0x578162`.
  **DO NOT** re-add the coord-identity check (`idx==Y*1024+X`): it over-stops
  during the load-time passability passes → subzone recursion → ntdll stack
  overflow `0x77DAFF41`. IsBadReadPtr per-cell is also too slow (load hangs).
- `Map512CoordTransformGuard` @`0x660540` — unconditional skip (return bare ret
  `0x66053A`, eax=0). Its result feeds only sync-checksum logging. **Requires the
  8 iterator sites above or it wraps bottom-right routing.**

## THE REMAINING BUG (bottom-left, §2.17) — next leads
- Per-object **tactical loop** `0x660000` iterates `ds:0xb04dac[]` objects and
  coord-transforms each via the `0x6601F1` family (twin of `0x660540`):
  `mov ecx,[0x880A04]; mov esi,[ecx]; call [esi+0x78]`.
- **`ds:0x880A04` has ZERO writes in all of gamemd** (18 reads, 0 writes) — it's
  a tactical/coord singleton set up by a MODULE or never. Its object's vtable is
  a heap pointer (garbage) → virtual call into heap.
- **Our BROAD build SURVIVES this** (user confirmed as fact). Broad guards
  `0x660540` (we ported that) but NOT `0x6601F1`, and broad patches Antares — but
  adding Antares here did NOT fix it. So broad likely **avoids reaching
  `0x6601F1`** for bottom-left via a path difference from its full sweep, OR the
  singleton is valid in broad for another reason.
- **Leads not yet tried:**
  1. Guard the `0x6601F1` twin function's entry like `0x660540` (find its entry;
     it's inside the `0x660000` loop, so guard the *inner* transform, not the
     whole loop — risk: it may be render, skipping could blank objects).
  2. Compare broad's runtime path for a bottom-left order vs curated (probe which
     function reaches the `[0x880A04]` read).
  3. Add the 50 Antares CMP bounds patches (fix the parse first).
  4. Find who (which module/site) is *supposed* to write `0x880A04` and why it's
     skipped at 1024.

## Reference / environment
- His handoff: `~/Desktop/Krisztiaan Map Proj/yr-map512-solution-author-handoff-20260804/`
  (his `source/`, `manifests/`, `evidence-notes/`, 2 DLLs in `bin/`). His
  74-static DLL = correctness oracle (works ≤250×250; host-check passes on
  gamemd-spawn.exe).
- Disassembly: `objdump -D -b binary -m i386 --adjust-vma=0x400000
  /home/rex/gamemd.exe > gamemd.disasm` (file-offset == RVA, VAs are real).
- Our broad build (reference; has `0x660540` guard + full sweep + Antares):
  installed via `src/*.cpp` MSVC CI (push branch `fix/phase1-correctness`,
  `gh run watch/download`), config `[Debug] CuratedBase=0`, INI `Stride=1024`.
  A built copy is `…/RA2/MapSizeExt.mine-probe.dll.bak` (md5 `10fe5d42`).
- Crash-EIP triage: `0x77DAFF41` ntdll = stack overflow (over-stopping guard /
  subzone recursion); `0x410174` = plane-init −1; `0x568C3B`/`0x578162` =
  iterator end-ptr; `0x021B9CA4` (heap) = coord-transform garbage vtable
  (`0x880A04` singleton, `0x660540`/`0x6601F1`).
- Deploy: build → copy DLL to the snap RA2 dir → user tests → read
  `debug/snapshot-*/except.txt` (symbolise stack against BUG-ATLAS §1).
