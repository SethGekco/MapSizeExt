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

## ⚠️ CRITICAL DISCOVERY (2026-08-09): the DLL was a SILENT NO-OP
The crash snapshot `debug/snapshot-20260809-003327/` + its patch log
(`yr_map_512_plane_probe.csv`) proved that **`apply_map512_patches` was ABORTING
on every supported attach** and disabling the entire DLL:
```
dll_attach,supported,...,phobos=0x778c0000,...
extension_patch_preflight_mismatch,0,Phobos.dll,0x778fea34
dll_detach,...,-1,subzone,0,...,extension,0,...     <- status=-1, 0 patches applied
```
- **Root cause:** the module patch table's **Phobos entry 0** expects `c1 e3 09`
  (`shl ebx,9`) at Phobos RVA `0x3ea34`, but the installed **Phobos.dll has
  `04 2b c1 50 51 ff` there** — the site moved in a newer Phobos build. The old
  code treated a single module-patch mismatch as FATAL (`g_patch_status=-1;
  return 0`), which set `g_host_supported=0` and made the activation hook
  (`Map512PlaneActivate` @`0x565812`) return **eax=512** — so the plane stayed
  512 and every guard/coord hook no-op'd (`g_patch_status<=0`).
- **Consequence:** the "bottom-left crash" we chased was essentially **vanilla
  behaviour on an oversized map** — the map's extreme coord is `0x24F`=591 (>512),
  which overflows the stock 512 plane → garbage cell/object → heap vtable call
  `0x021B9CA4`. MapSizeExt was doing nothing. This also means **"adding 73 Antares
  shl didn't fix bottom-left" was a false negative — they never applied**, and the
  earlier "300×300 loads and plays" must have been with an older Phobos.dll that
  still matched.
- **FIX (build `aac7fb72`, installed):** module-patch preflight mismatches are now
  **non-fatal** — the entry is skipped (`module_present[i]=0`), matching sites
  still apply, and the gamemd core plane-widening activates. Now the 148 Antares
  patches (shl+cmp, verified matching) + gamemd core all apply; the 2 stale Phobos
  entries skip.
- **TODO (separate):** re-derive the 2 Phobos inline-stride sites for the current
  Phobos.dll (its `GetCellIndex` `shl 9` + `MaxCells cmp 0x40000`) so Phobos
  features work correctly on >512 maps. Skipped for now (no Phobos-feature bug
  reported). Prev build backup: `…/RA2/MapSizeExt.dll.pre-antarescmp.bak`.

## CURRENT STATE (build md5 `aac7fb72`, installed — AWAITING RE-TEST AT REAL 1024)
Previously *claimed*: 300×300 loads/plays; walls, sidebar, foundations, movement,
pathfinding, deploy, radar, shroud, bottom-right OK — **but that state must now be
re-confirmed**, because the DLL was a no-op in the last snapshot. First thing to
verify on next test: `yr_map_512_plane_probe.csv` shows a `patch_applied,…,
extension_patches,<nonzero>` line and NO `-1` detach. Only then are we truly at
1024 and can judge the bottom-left corner.

**BOTTOM-LEFT FIX ATTEMPT (pending in-game verification):** added the missing
**75 Antares.dll `cmp` cell-index bounds patches** (`cmp reg,0x3FFFF→0xFFFFF`
×73 + `cmp …,0x40000→0x100000` ×2). This is Lead 3 from the previous handoff and
the half of the Antares coverage the hybrid lacked — the **broad build has these
and survives** the bottom-left crash (§2.17). All 75 expected byte-patterns were
verified against the live `Antares.dll` (`…/RA2/Antares.dll`, ImageBase
`0x10000000`) before shipping, so preflight should match. Also fixed a **latent
stack-array overflow** in `apply_map512_patches`: `module_present[16]`/
`module_applied[16]` are indexed by patch index but the table already had 75
entries (now 150) → resized to `[256]`.
- Prev build `8efc3e67` backup: `…/RA2/MapSizeExt.dll.pre-antarescmp.bak`.
- **If it crashes on load / patches no-op:** check `yr_map_512_plane_probe.csv`
  for `extension_patch_preflight_mismatch` — a single mismatch aborts ALL patches
  (map breaks). Revert to the `.bak` and report the logged RVA.

**IF THE CRASH PERSISTS (bottom-left still fatal):** the fallback is the untried
Lead 1 — a **conditional wild-pointer guard on the `0x6601f1` singleton chain**
(skip that object's tactical projection when `[[0x880A04]]`'s vtable is not in
`.rdata`), analogous to the working `0x660540` guard. Deliberately NOT added yet
so this test isolates whether the Antares cmp patches alone are the root cause.
EIP `0x021B9CA4` (heap = virtual-call into garbage), coords ~(307,249). See §2.17.

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
