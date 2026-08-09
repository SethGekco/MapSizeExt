# MapSizeExt — Bug Atlas & Reverse-Engineering Knowledge Base

Target: **Yuri's Revenge 1.001**, 32-bit `gamemd.exe` /
`gamemd-spawn.exe` (CnCNet spawner). Pinned analysis SHA-256
`3e81a617…d308600`. Goal: nominal 512×512 maps on a **1024×1024 internal
pointer plane** (raise cell stride 512→1024).

This document records, in detail, every bug we have characterised, its exact
mechanism and addresses, its symptom, which implementation approach exhibits it,
and the fix (or fix direction). It is the map for pinpointing the next bug fast.

Two independent implementations exist and their bug profiles are **complementary**
— that split is the single most useful diagnostic fact we have.

---

## 0. The two approaches (why the bug profiles differ)

### Ours — MapSizeExt (broad inline byte-patch sweep)
- ~**435** `shl reg,9 → shl reg,0xA` inline cell-index sites.
- ~**438** auto-scanned `cmp/push …,0x40000 → 0x100000` bounds sites.
- Plus: cell-population row stride, coord inverse-conversion, adjacency table,
  cell-iterator byte-stride (32 sites), subzone scale/saturation, overlay/radar
  surface, Antares/Phobos module patches, and a set of compiled `DEFINE_HOOK`
  guards.
- **Strength:** broad coverage → **loads 300×300** and larger.
- **Weakness:** broad sweeps are **false-positive-prone**. A `shl 9` or
  `cmp 0x40000` that is *not* a cell index (palette row pointer, 256 KB buffer
  size, bitfield) gets rewritten and corrupts an unrelated subsystem. We have
  already had to hand-exclude 41 such sites (see §2.8). The **wall** and
  **sidebar-brightness** bugs are almost certainly one or two more of the same
  class that we have not yet isolated.

### His — Krisztiaan's `yr_map_512_plane_probe` (curated hook-based)
- Only **74** exact byte patches, generated from per-subsystem manifests
  (owner-core 30, pathfinding 6, subzone-scale 17, overlay-load 7, IsoMapPack5 2,
  visibility/shroud 10, veinhole 2).
- **Hooks the central cell accessor** + delayed activation at MapClass init,
  unsigned-subzone consumers + producer ceiling, iterator phase-switching.
- **Strength:** no broad sweep → **no false positives**. Walls connect, sidebar
  lighting correct, radar works, movement correct, all corners reachable.
- **Weakness:** curated for **≤250×250**. Crashes constructing a 300×300 map
  (see §2.5) — a plane-init/bounds site his set doesn't cover but ours does.
- Handoff: `~/Desktop/Krisztiaan Map Proj/yr-map512-solution-author-handoff-20260804/`
  (source `source/yr_map_512_plane_probe.c`, table `source/yr_map_512_patch_table.h`,
  manifests, evidence-notes, and two DLLs in `bin/`). His DLL host-check accepts
  the CnCNet spawner (`CNCNET_SPAWNER_IMAGE_SIZE`, file 4,813,072 bytes) so it
  activates on Rex's `gamemd-spawn.exe`.

### The complementary table

| Behaviour | His curated base | Our broad sweep |
|---|---|---|
| Player-built walls connect | ✅ clear | ❌ **wall bug** (§2.1) |
| Sidebar cameo lighting | ✅ clear | ❌ **brightness bug** (§2.2) |
| Radar / satellite minimap | ✅ works | ✅ works (§2.3) |
| Unit pathfinding (slopes/water/harvesters) | ✅ works | ✅ works, we own the fix (§2.4) |
| Load & play 300×300 | ❌ **plane-init crash** (§2.5) | ✅ works |

**Plan:** rebuild MapSizeExt on his curated base (kills wall + sidebar bugs),
then carry over our plane-init/bounds coverage (adds 300×300). Best of both.

---

## 1. Memory / structure reference (verified this session)

**Cell accessors**
- `0x5656EA` `MapClass::operator[](CellStruct&)` — `shl eax,9` then bounds. (our HOOK)
- `0x5657A0` `operator[](CellStruct&)` variant used by wall producers.
  `shl eax,9` @`0x5657AC`, capacity guard `cmp eax,0x40000` @`0x5657B4`,
  reads `Cells.Items` at `[ecx+0x13C]`. **Both his and our patches identical here.**
- `0x565757` unchecked pointer-plane helper (`shl edx,9`).
- `0x5657F1` `IsCellValid` (`shl edx,9`).
- `0x565730` `GetCellAt(lepton)` → cell index.

**Cell array / MapClass**
- `Cells.Items` (pointer plane, `CellClass*[]`) at **`[MapClass+0x13C]`**.
- Static base pointer read site `ds:0x87F924` (399 refs).
- MapClass instance `0x87F7E8`.
- Plane W/H publish `0x565812` (`mov eax,0x200`→1024). MaxNumCells
  `0x565828` (`0x40000`→`0x100000`). Destruction/reserve bounds
  `0x565B73`/`0x565B87`.
- Cell-pointer-array **population** row stride `0x566437`
  (`add ecx,0x200`→`0x400`). Without it Items[] is filled at Y·512+X while
  reads use Y·1024+X → null cells → can't deploy/move.

**CellClass fields**
- MapCoords.X `+0x24` (word), MapCoords.Y `+0x26` (word).
- Overlay index `+0x44` (int, `-1`/`0xFFFFFFFF` = none).
- Height/level `+0x11B` (sbyte), SlopeIndex `+0x11C` (byte).
- **Wall/overlay frame (OverlayData) `+0x11E`** (byte) — the connection bitmask.
- Pixel/draw rect fields `+0x108/+0x10A/+0x10C/+0x10E` (computed @`0x484xxx`,
  fixed-point `imul … ; sar 0x10`, clamp `0x7D0`). **NB:** `+0x10A` is a *draw
  coordinate*, not the frame — do not confuse with `+0x11E`.

**Overlay / wall drawing & connection**
- Draw dispatch: PATH 1 `0x47F908` (SlopeIndex/terrain-following overlays),
  PATH 2 `0x47F96A` (`OverlayType+0x2A8` wall-flag → connection draw).
- Frame producers (all coord-based, verified stride-correct): `0x485390`
  (loops N/E/S/W via table+lookup, returns bitmask), `0x481810`
  (get-neighbour helper), `0x47E044` (writes `+0x11E`).
- **Neighbour coord table `0x89F688`** — 8 × `{Xword,Yword}` deltas
  (`N{0,-1} NE{1,-1} E{1,0} SE{1,1} S{0,1} SW{-1,1} W{-1,0} NW{-1,-1}`).
  **Built at runtime @`0x49F2F0`**, pure coordinate deltas → stride-independent,
  never patched.
- Wall-placement neighbour-notify + frame write: `0x485460`-region,
  `+0x11E` writes at `0x485760`/`0x48593D`.
- Sprite picker `0x45F160` (no stride math).
- 8-neighbour **cell-index** offset table `0x7E3774`
  (`[-512,-511,+1,+513,+512,+511,-1,-513]`) — used by the flood/zone walk at
  `0x429xxx`. We rewrite to 1024 offsets; his pathfinding manifest patches the
  same 6 row-crossing entries (leaving the two horizontals ±1). **Equivalent.**

**Iterator / passability / zones**
- Full-map cell iterator `0x578290`; per-cell op = `CellClass::UpdatePassability`
  `0x486A70`.
- Byte-offset form uses `shl reg,0xB` (rows·512·4 = rows<<11) + diagonal step
  `lea [reg-0x7FC]`.
- Dynamic diamond stride variable **`ds:0x89C2DC` = (Width+Height+1)**, written
  once @`0x42AC60`, read 163× as `imul <Ycoord>` — this grid **auto-scales** to
  any map, needs no patch. Its neighbour offsets `0x89A304` family.

**Subzones**
- 16-bit IDs in `[MapClass+0x6C]`/`[+0x70]`; producer @`0x58215B`;
  recompute/overflow @`0x5824A0`.

**Coord transform / crash-prone singletons**
- `ds:0x880A04` coord-transform singleton, read by the `0x660540` family;
  garbage at stride>512.

**Overlay-load / radar surface**
- `ScenarioClass::LoadOverlayPacks` @`0x5FD2E0` (owns the temporary decode
  surface + the OverlayPack/OverlayDataPack row traversals).

**300×300 crash chain**
- Sub-object CTOR `0x410170` (writes vtable to `[this+8]`), CellClass CTOR
  `0x47BBF0` (returns to `0x47BBFB`), construction loop `0x56634E`-`0x566432`
  in MapClass setup (return `0x566401`).

---

## 2. Bug atlas

### 2.1 Player-built walls don't connect (vertical/N–S specifically)
- **Approach:** OURS has it. **His base is clear.**
- **Symptom:** walls you build in-game (NAWALL, overlay **102**; also 27) draw as
  isolated posts — horizontal (E/W) joints register, vertical (N/S) don't.
  Map-*placed* walls (overlay **241/204**) connect fine because their `+0x11E`
  frames load from the map file, not computed live.
- **Diagnosis performed (all negative — code is correct):**
  - `+0x11E` frame is the connection bitmask; per-overlay frame-vs-neighbour
    reconstruction from the draw probe: 204 = 18/18 match, 241 = 34/38, but
    **102 = 6/51** and 27 = 0/9; bit-to-direction brute force showed 102's E/W
    bits track E/W but **N/S bits are garbage**.
  - Producers `0x485390`/`0x481810`/`0x47E044`, table `0x89F688`, lookup
    `0x5657A0`, sprite picker `0x45F160` — **all verified stride-correct**.
  - Runtime caller probe on `0x5657A0` (gated to the build area) caught only a
    redraw scan (callers `0x47DB96`-`0x47DCED`) with **correct** indices — the
    live frame producer for 102 does **not** funnel through `0x5657A0`.
  - Ruled out by INI toggle / evidence: `PatchCoord`, `PatchAdjacency`,
    `PatchModules`, `PatchSubzone`, `PatchBoundsCmp`, `PatchIterator`,
    `CellIterator_OOBGuard` (fired 0×). **Culprit is inside the load-bearing
    core (435 `shl` sweep or `cmp 0x40000` scan), which can't be toggled off to
    bisect.**
- **Root cause (class):** a **false positive in our broad sweep** — a `shl 9` /
  `cmp 0x40000` that is not a cell index. His accessor-hook design never patches
  those sites, so his walls are clean. Exact site not yet isolated.
- **Fix direction:** adopt his curated/hook base (removes the whole class). If
  staying on the sweep, binary-search the 435 sites by building variants that
  swap site-groups to his values.

### 2.2 Sidebar cameo brightness
- **Approach:** OURS has it. **His base is clear** (confirmed in-game).
- **Symptom:** sidebar build cameos render too bright.
- **Root cause (class):** same as §2.1 — a broad-sweep false positive, here on
  the **palette/lighting** path (cameos are SHP draws with a lighting multiplier).
  Related known-excluded sites in §2.8. Strong hypothesis: one more palette-row
  `shl 9` we have not excluded.
- **Fix direction:** same as §2.1 (his base avoids it). Or find the specific
  palette-row FP and exclude it.

### 2.3 Radar / satellite minimap not rendering — FIXED (full detail)
- **Approach:** both work when the 7 patches are present. His base has them.
- **Root cause:** at stride>512 the radar/overlay decode **surface is never
  created** — the decode surface is 640×400 and the row-traversal dimension gates
  compare against `0x200` (512), which fail past 512.
- **Fix (7 sites, `ScenarioClass::LoadOverlayPacks`):**
  | addr | change | meaning |
  |---|---|---|
  | `0x5FD2FD` | push `0x190`→`0x320` | surface height 400→800 |
  | `0x5FD302` | push `0x280`→`0x500` | surface width 640→1280 |
  | `0x5FD31C` | push `0x7D000`→`0x1F4000` | backing bytes |
  | `0x5FD509` | `cmp esi,0x200`→`0x400` | OverlayPack X traversal |
  | `0x5FD516` | `cmp edi,0x200`→`0x400` | OverlayPack Y traversal |
  | `0x5FD647` | `cmp esi,0x200`→`0x400` | OverlayDataPack X traversal |
  | `0x5FD650` | `cmp edi,0x200`→`0x400` | OverlayDataPack Y traversal |
- **NOTE:** these were mislabeled "radar" in early MapSizeExt code (`ApplyRadarPatches`/
  `kRadar`). They are Krisztiaan's **overlay-load** patches. Rename on port.
- **Save/load caveat:** 5 iterator sites (`0x5782BD/578321/578375/57847B/578482`)
  must be **stock** during the post-load rebuild (`sub_685120` @`0x68512B`, after
  `TabClass::Init_IO` @`0x67E694`) — his approach handles this via iterator
  phase-switching.

### 2.4 Unit pathfinding — walk-on-water / can't path — FIXED (full detail)
- **Approach:** both work when the iterator is patched. His base works.
- **Root cause:** the full-map passability/movement-zone recompute is driven by
  the cell iterator `0x578290` (per-cell `UpdatePassability` `0x486A70`). It
  encodes the row stride in the **byte-offset form** `shl reg,0xB` (rows·512·4)
  and steps diagonally with `lea [reg-0x7FC]`. Left at 512, the walk visits the
  wrong cells → units treat water/slopes as passable/impassable wrongly.
- **Fix (ours):** 32 × `shl 0xB`→`0xC` (row byte-stride) + step
  `-0x7FC`→`-0xFFC`; plus `CellIterator_OOBGuard` @`0x578290` (cell-identity
  guard: a real cell at slot N has coords `(N%stride, N/stride)`; on mismatch
  return null → clean loop termination instead of walking off the array).
- **Fix (his):** 5 iterator step sites + runtime **phase-switching** (patch on
  for the walk, stock during post-load rebuild).

### 2.5 300×300 map fatal crash (plane-init garbage) — HIS base has it
- **Approach:** HIS base crashes. **Ours is clear** (loads 300×300).
- **Crash:** `C0000005 @0x00410174`, `EAX=0xFFFFFFFF`, `EBX=0x87F7E8`,
  `EDI=0x440F5` (cell index 278,773), map dim `0x12C`=300 on stack. Chain:
  construction loop `0x566401` → CellClass CTOR `0x47BBF0`→ sub-CTOR `0x410170`
  writes vtable to `[0xFFFFFFFF+8]`.
- **Mechanism:** construction loop `0x56634E`-`0x566432` reads
  `plane[edi]` (`0x5663BC`); if **non-zero** it constructs onto that pointer, if
  zero it allocates a fresh `0x148`-byte cell. For cell `0x440F5` (row 272) the
  slot holds uninitialised **`0xFFFFFFFF`** → constructs onto `-1`. The pointer
  plane is **not zero-initialised far enough** for the 300×300 cell range.
- **Key fact:** his and our **dimension patches are byte-identical** (`0x565812`
  →1024, `0x565828`→`0x100000`, `0x565B73/B87`→`0x100000`). So the ceiling is
  **not** the obvious size patch — it's a **plane-init / zero-fill / bounds site**
  in the alloc/setup path (plane pointer stored at `[MapClass+0x13C]`; live plane
  seen at `0x12B30020`). Our broad sweep covers it; his 74 don't.
- **Fix:** carry over our plane-init/bounds coverage. **TODO:** pin the exact
  site (in the plane alloc/zero path near the `0x13C` write in the main CTOR).

### 2.6 Coord-transform crash on unit spawn/build — FIXED
- **Symptom:** building infantry / attack-dog → AV, EIP `0x07Cxxxxx`, signature
  `EBX=0x8809F4`, coords ~168/110.
- **Root cause:** coord-transform singleton `ds:0x880A04` holds garbage at
  stride>512; `0x660540` does `mov ecx,[0x880A04]; mov esi,[ecx]; call [esi+0x78]`
  → virtual call into heap junk.
- **Fix:** `CoordTransform_NullSingleton_Guard` @`0x660540` — at stride>512 skip
  unconditionally (result feeds only sync-checksum logging, never gameplay;
  `eax=0` is a harmless logged value; return the bare `ret` at `0x66053A`).

### 2.7 Flying-unit (HunterSeeker) crash — FIXED
- **Symptom:** AV `0x4CDD5F`, garbage `0x465F5445` ("ET_F").
- **Root cause:** `GetCellAt` returns non-null **garbage** cell pointers from the
  never-populated iso-diamond corners of the array.
- **Fix:** `GetCellAt_GarbageGuard` @`0x565766` — cell-identity check gated on
  `g_MapStride>512 && g_CrashGuard`.

### 2.8 Lighting / black objects (false-positive class) — EXCLUDED
- **Symptom when wrong:** black voxels/objects, crash `0x548DB1` on cliff maps.
- **Root cause:** **41** `shl reg,9` sites that are **palette-remap row pointers**,
  not cell indices. Tell: the value is clamped to `[0,254]` (`cmp reg,0xFE`) then
  `×512` (a 256-colour WORD row) + a table base = a remap-row pointer. Sites:
  `0x547DC7` + 40 SHP-drawer sites `0x493CF1`-`0x499ADC`. Patching → black/crash.
- **Status:** excluded from `kCellStrideSites`. **The sidebar-brightness bug
  (§2.2) is very likely a 42nd site of this exact class.**

### 2.11 Striped shroud / halved-coordinate reveal (Antares MapRevealer at 512)
- **Symptom:** fog-of-war reveals in **stripes** (every other row); the initial
  MCV reveal lands in the **top-right quarter** of the map regardless of map
  size; as a unit moves, cells **cycle high/low** (alternating elevation), and
  units on shrouded tiles render "high". Classic **halved-coordinate** signature:
  a system reads/writes the cell plane at **stride 512** while it is 1024, so row
  Y → row Y/2 (stripe) and X compresses into a quarter.
- **Approach:** appears when **Antares.dll is not patched** — i.e. our broad
  build with `PatchModules=0`, and the initial **CuratedBase** port (which skips
  our Antares patches). Our broad build with `PatchModules=1` is clean.
- **Root cause:** Antares.dll (open-source Ares reimplementation) contains the
  MapRevealer / tactical reveal and compiles **YRpp `GetCellIndex = Y<<9 + X`**
  and `MaxCells 0x40000` **inline**. Unpatched, its reveals index the plane at
  stride 512.
- **Fix (ours):** `ApplyAntaresPatches` — patch Antares.dll's inline `shl 9→0xA`
  (~73 sites) + `MaxCells 0x40000→0x100000` (~75 `cmp`) **relative to
  `GetModuleHandle("Antares.dll")`**. Also `ApplyModulePatches` for Phobos's own
  inline stride. NO-OP at stride 512.
- **NB — his base does NOT patch Antares yet works:** Krisztiaan's DLL has no
  Antares patch but no stripe, because his **central-accessor hook /
  invalid-axis rejection** makes Antares's stride-512 access resolve correctly.
  We have not ported that hook; the pragmatic fix in CuratedBase mode is to keep
  applying **our** Antares/Phobos patches (they carry no wall/sidebar false
  positive — those live in the gamemd broad sweep, not the module patches).

### 2.13 Piecemeal-port conflict: his byte-patches + OUR Syringe hooks (CuratedBase)
- **Symptom (CuratedBase + our Antares patches):** shroud stripe PERSISTS
  (so it is NOT Antares); MCV **cycles elevation** high/ground every cell it
  moves; base buildings can only be placed in a **far north-east region**
  (top-right quarter); MCV struggles to deploy. All the halved-coordinate /
  quarter signatures of the **cell-iteration & reveal path reading at 512**.
- **Root cause:** the port copied only his **74 byte-patches**, not his
  **hooks**, and substituted OUR compiled `DEFINE_HOOK`s. His working solution is
  a *coherent whole* — the 74 patches PLUS: **delayed activation** at MapClass
  init (`0x565812`; his patches are applied then, not at DllMain), **iterator
  phase-switching** (the 5 sites `0x5782BD..578482` toggled stock/widened across
  save-load, `set_reload_sensitive_iterators`), the **unsigned-subzone ceiling**,
  and a **central-accessor guard / invalid-axis rejection**. Our hooks
  (operator[] `0x5656EA`, alloc `0x48EB12/35`, inline-access `0x483B32`,
  lepton/IsCellValid) sit at addresses his 74 deliberately does NOT patch and/or
  overlap ones it does — so the two mechanisms disagree (Syringe saved original
  bytes vs his in-place patch; our alloc sizing vs his `MaxNumCells`). Result:
  parts of the iteration/reveal path resolve at 512 → stripe/quarter/elevation.
- **Conclusion:** do NOT mix his byte-patches with our hook architecture. Either
  (a) **adopt his complete source** (patches + hooks, proven-correct ≤250×250)
  as the MapSizeExt base and add ONLY the 300×300 plane extension on top, or
  (b) keep OUR broad sweep and surgically exclude the wall/sidebar false-positive
  site(s). Option (a) is cleaner given his set is proven and self-consistent.

### 2.12 Compiled-hook vs curated-base interactions (CuratedBase mode)
- **Observed in CuratedBase milestone 1:** his 74 patches + OUR compiled
  `DEFINE_HOOK`s → maps load & play (250×250 AND 300×300) but **striped shroud**
  (§2.11, Antares not applied) and **sidebar still bright**. His standalone DLL
  (his 74 + HIS hooks, our hooks absent) has neither bug. Therefore the
  **sidebar-brightness bug tracks OUR compiled hooks**, not the gamemd byte
  sweep — a strong new lead for §2.2 (audit the always-on hooks:
  operator[]/alloc/inline-access/lepton/IsCellValid and the render-adjacent
  guards; one perturbs the cameo lighting multiplier).
- **300×300 loads in CuratedBase** where his standalone DLL crashes (§2.5) →
  **our alloc hooks `0x48EB12/0x48EB35` are (part of) the plane-sizing his base
  lacks.** Keep them.

### 2.14 Sidebar-brightness pinned to the shroud-buffer alloc hooks (0x48EB12/35)
- **Finding (curated M2b):** deferring only `MapClass_Alloc_Stride1/2`
  (`0x48EB12/35`) — the shroud/visibility buffer alloc — makes the **sidebar
  cameo brightness correct** (broad build, with these active at 1024, is bright).
  So §2.2 is these hooks: their 1024 buffer sizing perturbs a shared draw/palette
  buffer that the sidebar cameos read. **To fix sidebar in the broad build, fix
  or defer `0x48EB12/35`** (they also caused the curated stripe — likely the same
  over-sized buffer). Real fix TBD: size the buffer correctly without the side
  effect.

### 2.15 Building foundation stays 1-cell in curated mode (coverage his 74 lacks)
- **Symptom (curated M2b):** even with operator[] `0x5656EA` and store
  `0x483B32` active at 1024 and shroud clean, the **building foundation is
  1-cell** (SHP overhangs footprint), MCV **deploy is unreliable**, and standard
  maps still show **elevation cycling**. So multi-cell placement/occupancy and
  cell-height read go through **another cell path** that is at 512 — one his 74
  does not patch and our active hooks do not cover.
- **Open question:** does his STANDALONE DLL build multi-cell foundations? If NO,
  his approach lacks foundation coverage (only our broad sweep has it) → base on
  the broad build. If YES, his HOOK (not yet ported) supplies it → finish porting
  his hook. **This test decides the whole strategy.**
- Note: our **broad build builds foundations fine** — so the foundation coverage
  is somewhere in the ~360 gamemd sites his 74 omits (and it is NOT the wall FP:
  broad builds foundations AND breaks walls).

### 2.16 Coordinate wrap: bottom-right folds to top-left (inverse-conversion sign-extend)
- **Symptom:** on a >512-coord map (300×300), units ordered to the **bottom-right
  edge** route to the **top-left**; scrolling there produces a **virtual-call
  fatal** — EIP in the heap (`0x021B9CA4`) with `EAX/ECX` = the target coords
  (301/297). The wrapped coordinate yields a garbage cell/object whose vtable is
  then called.
- **How it is created:** raising the plane stride to 1024 requires patching the
  **entire** inverse `index → (X,Y)` conversion, which is:
  `X = ((index & 0x1FF) ^ 0xFFFFFE00-signext) ...`, `Y = index sar 9`. There are
  FOUR site classes per conversion:
  1. positive mask `and reg,0x1FF → 0x3FF`  (`0x565c88`,`0x566fa4`)
  2. arithmetic shift `sar reg,9 → 0xA`      (`0x565c96`,`0x566fb2`)
  3. modulo mask `and reg,0x800001FF → 0x800003FF` (`0x565c75`,`0x566f91`)
  4. **sign-extension `or reg,0xFFFFFE00 → 0xFFFFFC00`** (`0x565c7e`,`0x566f9a`)
  His 74 patch classes 1–3 but **miss class 4**. `0xFFFFFE00` sign-extends a
  negative modulo at the **512** boundary; unpatched, coordinates in the upper
  (extended) half sign-extend as if the axis were 512 wide → they wrap into the
  low/upper-left region. Fix: patch `0x565c7e`/`0x566f9a` to `0xFFFFFC00`. (If the
  garbage-object virtual call persists after the wrap fix, add our
  `CoordTransform_NullSingleton_Guard` @`0x660540`, §2.6, as a safety net.)

### 2.9 Subzone signedness / stack overflow on big maps
- **Root cause:** 16-bit subzone IDs; ~14 `movsx` consumers sign-extend; values
  >`0x7FFF` go negative and `0x10000` truncates to `0` (="unvisited") → infinite
  region recursion `RecalculateSubZonesPass` `0x5824A0` → stack overflow.
- **His fix (better):** 14× `movsx`→`movzx` (unsigned) + producer ceiling
  saturating at `0xFFFE`, reserving `0xFFFF`. Full namespace.
- **Our fix (shortcut):** producer cap `0x7FFF` at hook `0x58215B`
  (`Subzone_SaturateID`) — keeps `movsx` positive but **halves** the namespace;
  wall-heavy / large maps can exhaust it. Adopt his unsigned approach on port.

---

## 3. Diagnostic playbook (how to pinpoint next time)

1. **512-vs-1024 A/B.** Set `[MapSize] Stride=512` → every patch/hook becomes a
   no-op → pure vanilla baseline. If the bug persists at 512 it is NOT MapSizeExt
   (another injected DLL — Rex's stack also loads Antares, Phobos, Kratos,
   GiftBoxHost, etc.). If it only appears at 1024 it is our patching.
2. **His-DLL oracle.** Swap in `bin/yr_map_512_plane_probe_74_static-only.dll` as
   `MapSizeExt.dll` (host-check passes on the spawner). If the bug is absent under
   his build, it is a **broad-sweep false positive** on our side → the fix is to
   avoid patching that site (his curated set is the "known-good" allow-list).
   His build is only safe **≤250×250**; use it as a *correctness* reference, not
   a size reference.
3. **Category bisect by INI toggle** (rules a category in/out without a rebuild):
   `PatchCoord`, `PatchAdjacency`, `PatchModules`, `PatchSubzone`,
   `PatchBoundsCmp`, `PatchIterator`, `PatchCrashGuard`. Note: some logic lives in
   compiled `DEFINE_HOOK`s that INI does **not** gate (e.g. `Subzone_SaturateID`,
   `CellIterator_OOBGuard`, the coord/GetCellAt guards) — gate them if you need
   them in the bisect.
4. **Runtime probes.** `DEFINE_HOOK` → `DeployDiagLog` (writes
   `MapSizeExt_deploy.log`, capped, gated on `g_MapStride>512`). Log cell coords
   `+0x24/+0x26`, overlay `+0x44`, frame `+0x11E`, and hook the shared lookup
   `0x5657A0` to capture caller + requested coords. Reconstruct layout in Python:
   compare stored `+0x11E` bitmask to actual neighbour occupancy → wrong frames =
   producer fault; correct frames + bad visuals = draw fault.
5. **Crash triage.** Read `debug/snapshot-*/except.txt`; symbolise the stack
   against §1. Watch for stale week-old snapshot dirs (they have no `except.txt`).
   `EAX/this = 0xFFFFFFFF` = uninitialised-plane construct; `0x07xxxxxx` EIP =
   garbage vtable (coord-transform / garbage-cell class).

---

## 4. Build / deploy reference
- Repo `/home/rex/MapSizeExt`, branch `fix/phase1-correctness`.
- Build: push → GitHub Actions **MSBuild** (`.syhks00` Syringe hooks need MSVC;
  mingw won't link them) ~40 s. `gh run download`, install DLL to
  `~/snap/cncra2yr/common/.wine/drive_c/Westwood/RA2/MapSizeExt.dll`.
- Injection: Syringe `-i=MapSizeExt.dll` (host `gamemd-spawn.exe`, 4,813,072 B).
  Swapping the file swaps the implementation under that injection name.
- Config: `MAPSIZEEXT.INI` `[MapSize] Stride/MaxDimension`, `[Debug] Patch*`.
  Always confirm the top of `MapSizeExt.log` says `Stride = 1024` — the INI has
  silently reverted to 512 before and made everything a confusing no-op.
- His handoff dir (source/table/manifests/DLLs/notes):
  `~/Desktop/Krisztiaan Map Proj/yr-map512-solution-author-handoff-20260804/`.

---

## 5. Port plan (his base → MapSizeExt) — in progress

### Progress log
- **M1 (done):** `src/CuratedBase.cpp` applies his 74 patches + 14 subzone
  `movzx` + 2 Phobos. `[Debug] CuratedBase=1` uses it instead of the broad sweep.
  Result: maps load/play (250×250 AND 300×300) but **striped shroud + broken
  placement** — because our compiled hooks ran on top of his patches and fought
  them (BUG-ATLAS 2.11/2.13). Bisecting the broad sweep to find the wall FP is
  **confounded** (removing sites kills the building/foundation layer before walls
  are testable) — abandoned.
- **M2 (this change):** added `g_CuratedBase`; in curated mode we **defer** the
  four accessor/alloc hooks that sit at addresses his 74 deliberately leave at
  512 — `MapClass_OperatorBracket_Stride 0x5656EA`, `MapClass_Alloc_Stride1/2
  0x48EB12/35`, `MapClass_InlineAccess_Stride 0x483B32` (`if (g_CuratedBase)
  return 0;`). We **keep** the hooks at his-patched addresses (`0x565757`,
  `0x5657F1` — they do the 1024 math his skipped byte-patch would), the
  **dimension-gate hooks** (`0x4C5630/590E`, `0x554BC5` — the >512 coord
  extension his base lacks, i.e. the 300×300 enabler), and the crash guards
  (§2.6/2.7). Subzone ceiling switches to `0xFFFE` in curated mode (his movzx).
  Goal: reproduce his clean walls+buildings+shroud, then verify 300×300.
- **M2b (correction):** M2 over-deferred. Result of deferring all four:
  **shroud fully clear** (stripe gone) BUT building foundation went **1-cell**
  (SHP overhangs) and standard maps got the elevation bug. So: only the
  **shroud-buffer alloc hooks `0x48EB12/35` defer** in curated mode (their 1024
  sizing was the stripe); the cell-access/store hooks **`0x5656EA` and
  `0x483B32` stay active at 1024** (foundation + elevation need them). The
  standard-vs-large split (standard buggier) is unexplained but consistent with a
  registration-stride mismatch that only bit small maps.
- **M3 — DECISION (his standalone DLL builds multi-cell foundations fine):**
  his foundation coverage comes from his HOOKS, which the piecemeal port never
  reproduced — confirmed dead end. **His complete source builds locally with
  plain mingw** (`i686-w64-mingw32-g++ -std=gnu++11 -shared -static-libgcc
  -Wl,--enable-stdcall-fixup -lpsapi`; his `.syhks00` hooks are GCC
  `__attribute__((section))`, no Docker/MSVC needed). **New plan: build his
  source directly as the MapSizeExt DLL (patches + hooks, proven), add ONLY the
  300×300 fix.** Abandon the CuratedBase-in-MSVC hook-gating branch.
- **300×300 fix (§2.5), refined:** crash reads plane slot = `-1`
  (`0xFFFFFFFF`) at index `0x440F5`; construction loop @`0x5663BC` tests `!= 0`,
  so `-1` is taken as an existing cell → constructs onto `-1` → AV `0x410170`.
  The plane is init'd (to 0/-1) for a count that covers ≤250×250 but not
  300×300. TODO: pin the plane-init count/stride in `MapClass_CTOR`
  (`0x565xxx`-`0x566xxx`); add it as one entry to his patch table.

### M4 — build his source + walk the 300×300 crash chain (in progress, working)
Building his source with local mingw and adding fixes on top. 300×300 now loads
and plays; crash chain walked so far (each a permanent addition):
1. **Plane-init `-1`** @`0x410174` → `Map512CellSlotGuard` hook @`0x5663BC`
   (treat non-heap slot pointer as empty so the loop allocates fresh). §2.5.
2. **Iterator end-pointer** — 27 `shl 0xB→0xC` sites his 74 lacked (0x568c1e … ).
3. **Iterator walks into garbage** @`0x568C3B` → `Map512CellIteratorGuard` hook
   @`0x578290` (stop on wild cell pointer, range `[0x400000,0x40000000)`; a
   coord-identity variant over-stopped and must NOT be used). §2.4.
4. **Coordinate wrap / garbage vtable** @heap → coord sign-extend `0x565c7e`,
   `0x566f9a` (§2.16).
Build: `i686-w64-mingw32-g++ -std=gnu++11 -shared -static-libgcc
-Wl,--enable-stdcall-fixup -o MapSizeExt.dll yr_map_512_plane_probe.c -lpsapi`.
Two guard hooks added in his `.syhks00`/`SyringeRegisters` style. Next: keep
clearing edge/scroll crashes, then vendor this into the repo as the deliverable.

### Sidebar-brightness answer (see 2.14)
Pinned to the shroud-buffer alloc hooks `0x48EB12/35`. His source doesn't have
them, so building from his source fixes the sidebar for free.

### Remaining
- Rename `ApplyRadarPatches`/`kRadar` → overlay-load (they are §2.3).
- Port his delayed-activation timing + iterator phase-switch if save/load needs it.
- Keep INI-configurable; keep the crash guards as belt-and-braces.

### 2.17 Bottom-left corner radar-order fatal (dangling tactical singleton 0x880A04)
- **Symptom (curated 300x300):** ordering a unit to the extreme BOTTOM-LEFT
  corner via the radar/minimap (without scrolling) fatals; EIP in the heap
  (`0x021B9CA4`), the crashing object at ESI (e.g. `0x17CE3498`) whose vtable is
  a heap pointer, not `.rdata`. Bottom-RIGHT and other edges are fine.
- **Path:** per-object tactical loop `0x660000` iterates `ds:0xb04dac[]` and for
  each object runs a 4-corner coord-transform (`0x6601F1` family, twin of
  `0x660540`) that does `mov ecx,[0x880A04]; mov esi,[ecx]; call [esi+0x78]`.
- **Root:** **`ds:0x880A04` has ZERO writes in gamemd** (18 reads, 0 writes) --
  it is a tactical/coord-transform singleton set up (or not) by a MODULE. In our
  broad build it does not fatal here; broad patches **Antares.dll** (73 shl + 75
  cmp) while the curated his-source build patches only Phobos (2). Hypothesis:
  Antares initialises the tactical context / this singleton, so unpatched Antares
  at 1024 leaves it dangling for off-screen bottom-left cells. Guarding `0x660540`
  does NOT fix it (crash is the twin `0x6601F1`); guarding `0x660000` is too broad
  (it is the whole object-render loop); the `==0` radar guards do not fire
  (garbage is non-null).
- **Next:** port our Antares coverage into his source's ModuleOpcodePatch table
  (scan Antares.dll for shl-9 + cmp-0x40000 relative to its base), OR find the
  exact Antares site that sets up `0x880A04` / the tactical projection.
- **Not the wall/sidebar FP:** curated build has correct walls + sidebar; this is
  an independent module-init gap.
