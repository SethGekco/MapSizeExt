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

## PROBE RESULTS (build `9e320a54`, 2026-08-09) — 2 bugs left, both NON-cell-stride
Ran 3 read-only probes (WALLDRAW @0x47f96a, CELLCALLER @0x5657bb, CELLMISS
@0x56577a). Findings from `yr_map_512_plane_probe.csv`:
- **Every cell lookup computes the CORRECT 1024 index** (e.g. x300,y149→152876 =
  149*1024+300). The shared cell plane is fully healthy at 1024. The only 2
  CELLMISS were both coord (0,0) and benign (a coord→string tooltip fn 0x4AE5D0;
  and 0x4B0F20 reading a produced object's own limbo coord).
- **Walls: adjacency WORKS** (WALLDRAW: lone wall frame=0x0, adjacent walls
  frame=0x1/0x4). **BUT `GuardRange=` line-fill is BROKEN** — placing two wall
  sections apart on a straight line within GuardRange should auto-fill the gap;
  it doesn't at 1024. (User-corrected: this is the real wall bug, not adjacency.)
- **Drag-select WORKS.** (Earlier "broken" was just that freshly-built units sit
  idle on the exit cell, so they weren't selectable — a symptom of the exit bug.)

### REMAINING BUGS (both are higher-level logic, NOT cell-plane stride)
1. **Unit-exit / FreeUnit**: units produced from a building go idle on the exit
   cell instead of scattering/moving to gather; `FreeUnit=` (e.g. NAREFN's free
   harvester) doesn't appear. The cell plane is correct, so this is the kick-out/
   scatter/placement logic — likely a coordinate or occupancy-scan computation his
   base doesn't cover at 1024. NOT caught by the cell probes (no cell-miss).
   Candidate fn: `FactoryClass::AI`-family @0x4b0f20 (produces units), but the
   (0,0) it reads looked benign. Needs a targeted kick-out/scatter probe.
2. **Wall `GuardRange=` line-fill**: no hardcoded ±512 cell-index step exists in
   the wall/placement regions (0x47e-0x486), and the 8-neighbour table 0x7E3774
   (his base patches it) is used only by pathfinding — so line-fill uses coord
   stepping (stride-correct). The break is elsewhere in the fill logic (range/
   straight-line test or the intermediate cell-clear check). Needs its function
   located + probed. GuardRange is a BuildingTypeClass field (wall = building).
### METHOD for next round
Both need a targeted probe on their specific function (locate first). The generic
cell probes proved the plane is correct and ruled out the whole cell-stride class
for these two — a real result. Don't re-port MapClass sites for these.

## CHANGELOG since deploy/build troubles (what changed, in order)
Everything below happened AFTER the DLL first became active at real 1024
(`aac7fb72`). Use this to localize the wall / drag-select / infantry-exit
regressions.
| build | change | result |
|---|---|---|
| `aac7fb72` | non-fatal module preflight + 75 Antares cmp (DLL finally active @1024) | corners fixed; deploy 1-cell, can't build |
| `0a6f5d1c` | +foundation accessors `0x5656EA` shl, `0x5656F1` cmp, **`0x483B32` store** | still 1-cell foundation, still can't build |
| `fbba6760` | +`0x568xxx` occupancy suite (13 shl + 21 cmp, all verified cell accessors) | **build/deploy/foundation WORK**; walls, drag-select, infantry-exit BROKEN |
| `03d61c8f` | **−`0x483B32`** (suspected wall/select/exit FP) | redundant no-op; NOT the culprit (walls still broke, buildings still multi-cell) |
| `9e320a54` | +3 read-only diagnostic probes | proved cell plane 100% correct; walls-adjacency + drag-select WORK; 2 non-stride bugs left |
| `e0cc8938` | skip 73 Antares `shl` patches (bisection) | **RULED OUT**: walls stayed broken AND shroud striped + movement/AIBaseSpacing broke → Antares shl are load-bearing (shroud/tactical/AI cell math), NOT the wall FP |
| `1191a18980b3` | restore Antares `shl` (`g_skip_antares_shl=0`) | back to healthy state; walls line-fill still the open issue |

### ★ SYMBOL MAP FOUND (use this from now on)
`/home/rex/Claude/Antares/gamemd_names_from_antares_pdb.txt` = 1464 named gamemd
functions from the Antares PDB. **grep it before disassembling.** (memory:
[[gamemd-pdb-symbol-map]].) Partial set; sub-labels are points inside a larger fn.

### Function leads for the 2 open bugs (from the symbol map)
**Unit-exit / FreeUnit** (units idle on exit cell; NAREFN free HARV missing):
- `0x445F80 BuildingClass_Place`; `0x446E9F BuildingClass_Place_FreeUnit_Mission`;
  `0x446AAF ..._SkipFreeUnits`; `0x446EE2 ..._InitialPayload`.
- `BuildingClass_KickOutUnit_*` family @0x443CCA-0x445355 (one big KickOutUnit):
  `_FindAlternateKickout` 0x4444E2, `_Infantry` 0x444DBC, `_UnitType` 0x444119,
  `_ArmoryExitBug` 0x444D26.
- `0x51D0DD InfantryClass_Scatter`; `0x51C4C8 InfantryClass_IsCellOccupied`;
  `0x73F7B0 UnitClass_IsCellOccupied`; `0x7441B6 UnitClass_MarkOccupationBits`.
- **Checked so far, NO inline stride site:** FindAlternateKickout (list iter),
  FreeUnit_Mission (virtual calls + create), MarkOccupationBits (calls patched
  GetCellAt 0x565730 + height helper 0x578080), Scatter (uses a per-FACING flag
  table 0x7EAF7C, not cell offsets). So these use the shared *patched* accessors —
  the bug is a coordinate/mission/occupancy interaction, NOT a raw 512 index.
  Next: probe KickOutUnit's exit-destination coord + FreeUnit placement coord to
  see if an off-map/(0,0) destination is produced.

**Wall `GuardRange` line-fill**: no separately-named fn; lives inside
`0x445F80 BuildingClass_Place` (or a helper it calls). `0x480534
CellClass_AttachesToNeighbourOverlay` = adjacency (works). Next: probe inside
BuildingClass_Place during a wall placement to find the straight-line scan.

### Wall line-fill — narrowing so far (still open)
Confirmed NOT the cause: cell-plane stride (probe: all lookups correct), the
Antares shl patches (bisected out — needed for shroud/movement/AI), and `0x483B32`
(redundant). The line-fill has been broken since 1024 first became *active* (it was
only testable once building worked at `fbba6760`); the earlier "walls worked" was
the no-op/vanilla-512 state, so this is a genuine 1024 gap, not a single-patch
regression. No hardcoded ±512 cell step exists in the wall region (0x47e-0x486).
Next: locate the wall line-fill / GuardRange scan function (base RA2 feature in
gamemd; `GuardRange` = TechnoTypeClass field) and probe its straight-line scan —
suspect a distance/coordinate computation, or a scan step his base doesn't cover.
The unit-exit / FreeUnit bug is the other open item (kick-out/scatter logic).

### Wall line-fill bisection (build `e0cc8938`)
Walls broke *recently* → a recent change caused it (narrow window). Evidence points
at the Antares patches: broad build (full Antares) breaks walls; his base (no
Antares) has clean walls; we added 148 Antares patches and walls broke. The 73
Antares **shl** patches are removable — the corner fix came from the 75 Antares
**cmp** patches, not the shl (per M4 handoff) — so this build skips the shl and
keeps the cmp. Toggle: `g_skip_antares_shl` in `yr_map_512_plane_probe.c`.
- **If wall line-fill works now:** the Antares shl patches were the wall FP (and
  likely redundant — his base handles Antares shroud via its own accessor hook).
  Keep them off; verify shroud is still clean and corners still work.
- **If shroud goes striped:** the Antares shl WERE doing shroud work (§2.11) —
  note it, we'll need a narrower subset. **If walls still broken:** the FP is not
  the Antares shl; flip `g_skip_antares_shl=0` and suspect the occupancy `cmp`
  widen or an Antares `cmp`. Prev build backup: `MapSizeExt.dll.pre-antaresshl-off.bak`.

**Wall / drag-select / infantry-exit regression analysis:**
- The `0x568xxx` occupancy suite was audited site-by-site: all 34 are genuine cell
  accessors (`shl 9; cmp 0x40000; mov [Items+idx]`), NOT false positives — so they
  are not the cause and stay.
- The wall-connection producer `0x485390` is fully **coord-based** and calls the
  already-patched `0x5657A0`; it is stride-correct, so walls should work — pointing
  to an FP, not a missing patch. The **entire wall/overlay region (0x47-0x48) has
  exactly ONE shl site in the broad build: `0x483B32`** — which we added at
  `0a6f5d1c`. Broad (has it) breaks walls; his base (omits it) had clean walls.
  `0x483B32` also did NOT fix the 1-cell foundation, so it looks redundant now.
  Hence `03d61c8f` removes it as the single-variable wall test.

**BIG STRATEGIC FINDING (his base is far less complete than assumed):** diffing the
broad build's `kCellStrideSites` (`src/Patches.cpp`, 438 shl sites) against his
table shows **~395 cell-index sites his base does NOT patch**, densely spread across
the whole MapClass/DisplayClass region `0x569xxx-0x588xxx` (display, selection,
layers, occupancy, iteration…). His curated base only ever covered the subsystems
his manifests enumerated (~76 + our adds). Because the DLL was a no-op until
`aac7fb72`, none of this was exercised. So each subsystem we touch at real 1024
(occupancy done; drag-select + infantry-exit likely next) is another cluster to
port — verify each is a real cell accessor, avoid the palette FPs (`0x547DC7`,
`0x493CF1-0x499ADC`) and the ranges his HOOKS own (iterator `0x578xxx`, subzone
`0x582xxx`). Decision pending: incremental per-subsystem port vs. batch-port the
`0x569-0x577` display region.

## UPDATE (build `fbba6760`): fixing deploy/build via the occupancy subsystem
`0a6f5d1c`'s 3 foundation accessors applied (`patch_applied,115`) but deploy/build
was STILL broken. Real cause: the whole **`MapClass` content/occupancy subsystem
`0x568xxx`** (`AddContentAt`/`RemoveContentAt` + siblings) is unpatched in his base
— 13 `shl 9` + 21 `cmp 0x40000` sites. Left at 512, any cell with index ≥ 0x40000
(Y ≥ 256 @1024) returns the dummy off-map cell → can't deploy/build in >half the
map, footprint mis-registers. Added all 34 (BUG-ATLAS 2.15). Build `fbba6760`
installed. AWAITING deploy/build test. **General lesson for the rest of the
project:** his curated base only covers the subsystems his manifests enumerated;
each subsystem newly exercised at >512 (placement, occupancy, …) may need its
`shl 9`/`cmp 0x40000` sites ported from the broad build's `kCellStrideSites`
(`src/Patches.cpp`) — diff by address range, verify each is a real cell accessor.

## UPDATE (build `0a6f5d1c`): corner crash FIXED; now fixing 1-cell foundation
Re-test of `aac7fb72` at real 1024 (log confirmed `status=1, extension_patches=148`,
`MaxCells=1048576`, coord extent 601): **bottom-left AND bottom-right corners work,
units route correctly — the §2.17 crash is resolved.** Remaining at 1024: MCV
deploys 1-cell, can't build, >half the map refuses deploy (sidebar/path/MCV-grounded
all correct). Added the 3 foundation/occupancy accessor patches (`0x5656EA` shl +
`0x5656F1` cmp + `0x483B32` store) — see BUG-ATLAS 2.15. Build `0a6f5d1c` installed;
prev backup `…/RA2/MapSizeExt.dll.pre-foundation.bak`. AWAITING deploy/build test.

## PRIOR STATE (build md5 `aac7fb72` — superseded by `0a6f5d1c`)
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
