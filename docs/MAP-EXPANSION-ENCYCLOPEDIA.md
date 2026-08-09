# Map-Expansion Encyclopedia (RA2/YR stride 512 → 1024)

A durable, reusable knowledge base for expanding the RA2/Yuri's Revenge internal
cell plane from a 512-row stride to 1024, distilled from the full debugging of
three builds. Use this to **predict which subsystem a symptom points to, which
build it's unique to, and what fixed it**. Companion to `BUG-ATLAS.md` (per-bug
detail) and `HANDOFF.md` (current state). Pinned `gamemd.exe` SHA
`3e81a617…d308600`.

The core change: cell index `Y*512+X` → `Y*1024+X`; MaxCells `0x40000`→`0x100000`;
the pointer plane `Cells.Items` (`[MapClass+0x13C]`) must be populated, iterated,
bounds-checked, coordinate-converted, and rendered **consistently** at 1024.
Every subsystem that inlines the 512 assumption is a landmine.

---

## Part 1 — The three builds and their bug profiles

We debugged three distinct implementations. Their bug profiles are the single
most useful diagnostic tool: **a bug present in one but not another localizes the
cause to what differs between them.**

### A. ORIGINAL — our broad MSVC byte-sweep (`src/*.cpp`)
- **Method:** blanket-patch ~435 `shl reg,9→0xA` + ~438 auto-scanned
  `cmp/push 0x40000→0x100000`, plus compiled `DEFINE_HOOK` guards, plus Antares
  (73 shl) + Phobos module patches.
- **Loads 300×300+.** Handles shroud, coord-transform crash, foundations,
  movement, radar, deploy.
- **UNIQUE BUGS (broad-sweep false positives):**
  - **Wall-connect bug** — player-built walls don't join (a `shl 9` we patch that
    is NOT a cell index corrupts the in-game wall connection). Never isolated the
    exact site.
  - **Sidebar cameo brightness** — pinned to the shroud-buffer **alloc hooks
    `0x48EB12/35`** (their 1024 buffer sizing perturbs a shared draw buffer).
- **Lesson:** a broad sweep is *false-positive-prone* — `shl 9`, `cmp 0x40000`,
  and byte constants collide with palette-row pointers, 256 KB buffers, bitfields,
  and non-cell math. We already hand-excluded **41 palette-remap FPs** (§2.8);
  walls + sidebar are 1–2 more of the same class.

### B. INHERITED — Krisztiaan's curated source (`curated/` = his `.c` + table)
- **Method:** only **74 curated exact patches** + a few `.syhks00` hooks
  (delayed activation, boundary probe, subzone ceiling, iterator phase-switch).
  Patches Phobos (2), **no Antares**.
- **CLEAN:** walls connect, sidebar correct, foundations correct, movement,
  pathfinding, radar, shroud — **no false positives** (curated = no stray sites).
- **UNIQUE BUG:** **caps at ≤250×250.** A 300×300 map fatals in construction
  (plane-init `-1`, §2.5) — his set doesn't cover the higher cell-index range.

### C. HYBRID — his source + our 300×300 fixes (current, `curated/`, md5 `8efc3e67`)
- **Method:** B + the minimal additions needed for 300×300 (2 plane-iterator
  bounds, 27 iterator end-pointer sites, 8 iterator shl-9, 2 coord sign-extend,
  73 Antares shl) + 3 guard hooks (cell-slot, cell-iterator, coord-transform).
- **300×300 LOADS AND PLAYS.** Walls, sidebar, foundations, movement, pathfinding,
  radar, shroud, bottom-right/most edges all correct.
- **UNIQUE BUG:** **bottom-left corner radar-order fatal** — the tactical
  coord-transform (`0x6601F1`) on the never-written `0x880A04` singleton. Present
  in the hybrid; the ORIGINAL broad build does NOT have it (path/patch difference,
  not yet pinned). §2.17.

### Cross-build truth table

| Subsystem / behaviour | A. Original broad | B. Inherited curated | C. Hybrid (now) |
|---|---|---|---|
| Load & play 300×300 | ✅ | ❌ plane-init crash | ✅ |
| Player-built wall connect | ❌ FP bug | ✅ | ✅ |
| Sidebar cameo brightness | ❌ FP (alloc hooks) | ✅ | ✅ |
| Building foundation (multi-cell) | ✅ | ✅ | ✅ |
| Shroud (fog reveal) | ✅ (Antares patched) | ✅ (his gamemd visibility) | ✅ |
| Movement / pathfinding | ✅ | ✅ | ✅ (deep cliffs untested) |
| Radar / minimap | ✅ | ✅ | ✅ |
| Coord wrap (BR→TL) | ✅ fixed | n/a (≤250) | ✅ fixed (sign-extend) |
| Bottom-left corner order | ✅ survives | n/a (≤250) | ❌ fatal (`0x6601F1`) |

**Reading it:** wall + sidebar are **unique to the broad sweep** → for any future
wall/sidebar regression, the **broad `shl`/`cmp` sweep and the alloc hooks are the
prime suspects**, and the curated approach is the safe reference. The bottom-left
crash is **unique to the hybrid/curated tactical path** → suspect the module-set
difference (Antares/tactical-singleton setup), not the base cell math.

---

## Part 2 — Subsystem interaction map ("touch X → get Y")

For each subsystem: what it is, the failure if you get the 512→1024 conversion
wrong, the symptom / crash EIP, and the fix. This is the causal encyclopedia.

### 2.1 Cell pointer plane — allocation & population
- **Population row stride** (`0x566437` `add ecx,0x200→0x400`): if unpatched,
  `Items[]` filled at `Y*512+X` while reads use `Y*1024+X` → every lookup hits a
  null cell → **can't deploy/move**. Fix: patch it (both builds do).
- **Plane not zero-init far enough** (300×300): construction loop `0x5663BC`
  reads a slot holding `-1`/garbage, treats non-zero as an existing cell, and
  constructs onto a wild pointer. **Symptom:** load fatal `EIP 0x410174`,
  `EAX=0xFFFFFFFF`, `EDI` = cell index (0x440F5). **Fix (hybrid):**
  `Map512CellSlotGuard @0x5663BC` — non-heap slot pointer → treat as empty.

### 2.2 Cell accessors (`operator[]`, `GetCellAt`, `IsCellValid`)
- `0x5657A0`/`0x5657AC` (shl) + `0x5657B4` (cmp 0x40000): the wall/producer
  lookup. Both builds patch it. If wrong → producers read wrong neighbours.
- `0x5656EA` (operator[]), `0x483B32` (inline cell-store): needed at 1024 even in
  the hybrid — **deferring them broke the multi-cell foundation (1-cell building)
  and standard-map elevation** (BUG-ATLAS 2.15/M2b). Keep at 1024.

### 2.3 Full-map cell iterator (`0x578290`)
- Byte-offset **end pointer** uses `shl reg,0xB→0xC` (rows·512·4). His 74 lacked
  27+8 of these → the iterator walks past valid cells into garbage.
  **Symptom:** `0x568C3B`/`0x578162`, garbage `EDX`/`ESI`. **Fix:** add the
  iterator `shl` sites **and** `Map512CellIteratorGuard @0x578290`.
- **GUARD DESIGN LESSON:** the iterator guard must stop only on a **wild pointer**
  (`[0x400000,0x40000000)`). A coord-identity check (`idx==Y*1024+X`)
  **over-stops during the load-time passability passes → subzone recursion →
  ntdll stack overflow `0x77DAFF41`.** `IsBadReadPtr` per-cell is too slow (load
  hangs). Minimal guards only.

### 2.4 Inverse coordinate conversion (index → X,Y)  ★ high-value
Four independent site classes; miss any one and coordinates wrap:
1. positive mask `and reg,0x1FF→0x3FF` (`0x565c88`,`0x566fa4`)
2. arithmetic shift `sar reg,9→0xA` (`0x565c96`,`0x566fb2`)
3. modulo mask `and reg,0x800001FF→0x800003FF` (`0x565c75`,`0x566f91`)
4. **sign-extension `or reg,0xFFFFFE00→0xFFFFFC00`** (`0x565c7e`,`0x566f9a`)
- Missing class 4 (his 74 did) → **bottom-right/high coords wrap to top-left**
  (units route to the wrong corner) and the wrapped coord makes a garbage object
  → virtual-call fatal. **Fix:** patch all four classes.

### 2.5 Coord-transform singleton `ds:0x880A04`  ★ landmine
- Read by the `0x660540` and `0x6601F1` families:
  `mov ecx,[0x880A04]; mov esi,[ecx]; call [esi+0x78]`.
- **`0x880A04` has ZERO writes in gamemd** — set up by a module (or never). At
  1024 its object's vtable is garbage → **virtual call into heap** `0x021B9CA4`.
- `0x660540`: skippable (result feeds only sync-checksum logging) — but skipping
  **needs the 8 iterator shl-9 sites or it wraps bottom-right routing**.
- `0x6601F1` (twin, inside the per-object tactical loop `0x660000`): **the
  current unsolved bottom-left crash.** Skipping the whole loop is too broad.
  Original broad build survives it; hybrid does not — cause not yet pinned
  (suspect module/tactical-singleton setup, but Antares shl patches did NOT fix).

### 2.6 Shroud / fog-of-war
- Reveal geometry: **Antares.dll MapRevealer** compiles stride-512 inline. If
  Antares is unpatched → shroud reveals in **stripes**, MCV reveal lands in the
  **top-right quarter**, cells cycle high/low. Broad build fixes via Antares
  patches; his curated fixes via **gamemd visibility patches** (`0x586xxx`) — two
  different valid routes. **Also:** the shroud-buffer **alloc hooks `0x48EB12/35`
  at 1024 caused a stripe** in the MSVC-curated experiment AND drive the sidebar
  brightness — a shared-buffer footgun.

### 2.7 Overlay load (map-placed walls/ore) vs in-game wall connection
- `ScenarioClass::LoadOverlayPacks @0x5FD2E0`: temporary decode surface
  (640×400→1280×800, `0x7D000→0x1F4000`) + OverlayPack/OverlayDataPack row
  traversals (`cmp 0x200→0x400`). Fixes map-loaded overlays/radar surface.
- **In-game wall connection** is a DIFFERENT path (frame `cell+0x11E`,
  producers `0x485390`/`0x47E044`, coord table `0x89F688`). All verified
  stride-correct — so the **broad-sweep wall bug is a false positive elsewhere**,
  not a missing wall patch (his fewer patches get walls right).

### 2.8 Subzones (pathfinding zones)
- 16-bit IDs; ~14 `movsx` consumers sign-extend; big maps exceed `0x7FFF` → IDs
  go negative and `0x10000` truncates to 0 ("unvisited") → **infinite recursion,
  stack overflow** (`0x5824A0`). His fix (better): 14 `movsx→movzx` + producer
  ceiling `0xFFFE`. Ours: cap `0x7FFF` (half namespace). Also the *symptom* of an
  over-stopping cell-iterator guard is this same recursion (§2.3 lesson).

### 2.9 Modules (Antares/Phobos)
- Each compiles its own inline stride-512 (`GetCellIndex Y<<9+X`, `MaxCells
  0x40000`). Patch relative to `GetModuleHandle`. Antares = shroud/tactical;
  Phobos = waypoints/overlay features. **Curated (his source) patches only 2
  Phobos**; adding 73 Antares shl did NOT fix the bottom-left crash (so the
  singleton isn't set by those particular sites).

### 2.10 Palette / lighting (false-positive class)
- `shl reg,9` sites that are **palette-row pointers** (value clamped `[0,254]`,
  ×512 = a 256-colour WORD row) NOT cell indices — patching → **black
  objects/crash**. 41 excluded (`0x547DC7` + `0x493CF1`-`0x499ADC`). **Sidebar
  brightness is likely a sibling of this class + the alloc hooks.**

---

## Part 3 — Symptom → suspect (diagnostic quick-map)

| Symptom / crash | Suspect subsystem | Which build | Fix / note |
|---|---|---|---|
| Can't deploy/move at all | cell population `0x566437` | any at 1024 unpatched | patch add 0x200→0x400 |
| Load fatal `0x410174`, EAX=−1 | plane not zero-init far enough | ≤250 base at 300×300 | CellSlotGuard @0x5663BC |
| Fatal `0x568C3B`/`0x578162`, garbage cell | iterator end-ptr/walk | curated w/o iter sites | 27+8 iter sites + iter guard |
| ntdll `0x77DAFF41`, zeroed stack | over-stopping guard / subzone recursion | any | make guards minimal; movzx subzones |
| Units route to wrong (opposite) corner | inverse-conv sign-extend (class 4) | curated w/o `0x565c7e/566f9a` | patch `or 0xFFFFFE00→FC00` |
| Heap `0x02xxxxxx` virtual-call on edge order | `0x880A04` coord-transform | hybrid (bottom-left) | UNSOLVED; original survives |
| Striped shroud / top-right-quarter reveal | Antares MapRevealer unpatched | broad w/ PatchModules=0; curated no-Antares experiment | patch Antares OR gamemd visibility |
| Building acts as 1 cell / SHP overhang | operator[]/inline-store at 512 | curated deferring 0x5656EA/483B32 | keep them at 1024 |
| Sidebar cameos too bright | shroud-buffer alloc hooks `0x48EB12/35` | ORIGINAL broad only | curated is clean; defer/fix the hooks |
| Player-built walls don't join | broad-sweep `shl` false positive | ORIGINAL broad only | curated is clean; don't broad-sweep |
| Black objects/voxels | palette-row `shl` false positive | broad | exclude the 41 palette sites |

---

## Part 4 — Meta-lessons for future map-expansion projects

1. **Prefer curated + hooks over a broad sweep.** A blanket `shl 9→0xA` /
   `cmp 0x40000` sweep WILL hit palette rows, buffer sizes, and bitfields that
   look identical to cell math → subtle, hard-to-localize corruption (walls,
   sidebar, black objects). Curated per-subsystem manifests avoid the whole class.
2. **Build-profile differencing is the best debugger.** Keep ≥2 implementations
   whose coverage differs; a bug in one but not the other localizes the cause to
   the delta. That's how we pinned sidebar→alloc-hooks and wall→broad-sweep.
3. **Keep guards minimal and cheap.** A guard that validates too much (coord
   identity) or too slowly (`IsBadReadPtr`) over-stops or hangs the load-time
   full-map passes → stack overflow / freeze. Stop only on unambiguous garbage
   (wild pointer / `-1`).
4. **The conversion is multi-class per subsystem.** e.g. inverse coord conversion
   is FOUR classes (mask/shift/modulo-mask/sign-extend); patching 3 of 4 leaves a
   corner-wrap. Enumerate all forms (`shl 9`, `shl 0xB` byte-offset, `add 0x200`
   row-walk, `cmp 0x40000`, `and 0x1FF`, `or 0xFFFFFE00`, `sar 9`).
5. **Some globals are never written by gamemd** (`0x880A04`) — a module owns
   them; a stride change can leave them dangling. Module init order matters.
6. **Build/deploy that worked:** his source builds with **plain local mingw**
   (`i686-w64-mingw32-g++ … -lpsapi`) — no Docker/MSVC/CI — because his
   `.syhks00` hooks use GCC `__attribute__((section))`. Fast iteration.
7. **Crash-EIP fingerprints** (reuse across projects): `0x410174`=plane-init −1;
   `0x568C3B`/`0x578162`=iterator; `0x77DAFF41`=ntdll stack overflow;
   `0x02xxxxxx`/`0x021B9CA4`=garbage-vtable virtual call; heap EIP + coord
   registers = a wrapped/garbage coordinate producing a bad object.
8. **Disassembly setup:** `objdump -D -b binary -m i386 --adjust-vma=0x400000
   gamemd.exe` — for this exe, file-offset == RVA, so VAs are directly usable.
