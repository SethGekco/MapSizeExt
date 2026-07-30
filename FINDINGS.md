# MapSizeExt — Findings, RE Journey & Torch-Carrier Handoff

**Project:** Lift Red Alert 2 / Yuri's Revenge map-size limits beyond the vanilla ~512 grid.
**Companion doc:** `~/Downloads/MapSize_CrossCodebase_Reference.md` (Phobos/Ares/Antares/yr-patches/WAE source references — read that first for the cross-codebase map).
**This doc:** the *binary* RE journey on `gamemd.exe` + `Ares.dll`, the crash taxonomy, the reusable method, the "Claude vibe-coding" engineering lessons, and the strategic pivot to **Antares**.

> Purpose: so anyone (human or AI) can pick up the torch. Everything here was learned by driving a real game install through crash-after-crash with a CI-built Syringe DLL. Feed the address-keyed parts into the [YR Hook Encyclopedia](https://github.com/SethGekco/YR-Hook-Encyclopedia).

---

## 0. TL;DR — where this stands (2026-07-30)

- A 300×300 map (needs stride 1024) now **loads through ~8 distinct engine phases** that previously crashed. Each was a solvable byte-patch.
- We are stuck on a **null Ares coordinate singleton (`.bss` `0x880A04`)** that *no code anywhere initializes* — a dead-code path in the **compiled Ares.dll** that our modified map trips. Static byte-patching cannot cross it.
- **DECISION: pivot to Antares.** Ares is open source and being replaced by Antares (a clean 3.0p1 reimplementation with *dynamic* map dimensions). The `0x880A04` class of bug is a vanilla-Ares-binary artifact; Antares likely doesn't have it, and when it does hit a wall we can read the source instead of guessing.
- Everything is committed. The DLL has an INI **toggle/bisection harness** (`[Debug]` section) that reproduces any intermediate state.

---

## 1. The two real limits (engine, not editor)

- **W+H ≤ 512 is an *editor* limit** (FA2 and WAE `CreateNewMapWindow.cs`), NOT an engine limit. gamemd checks `W < 512` **and** `H < 512` independently — no sum check.
- The true engine constraint is the **cell-array stride**: cells are indexed `Y * 512 + X` into a `512*512 = 262144` array. A map whose iso-diamond reaches a coordinate ≥ 512 in *either* packed axis needs a bigger stride.
- **Why 300×300 specifically needs stride 1024:** an iso map of `[Map] Size=W,H` reaches packed cell coordinates up to ~`W+H`. A 300×300 map reaches row ~600 (> 512), so `Y*512+X` overflows the 262144 array. 200×200 (reaches ~400) fits at stride 512 and works today; 256×256 is borderline. Stride must be the next power of two > max-coord, i.e. **1024**.
- Stride is always a power of two, so raising it is mostly rewriting a shift immediate: `shl reg,0x9` (×512) → `shl reg,0xA` (×1024). **NO-OP at stride 512** (the shift byte is unchanged), which makes every patch safe to ship disabled-by-default.

---

## 2. The patch taxonomy (this is the Encyclopedia gold)

Raising the stride means finding **every** place `512` / `262144` is baked in, across **all instruction forms**, in **three binaries** (gamemd, Ares, Phobos), while NOT touching the coincidental `256 KB` / `bit-18` / enum uses. Forms found in `gamemd.exe` 1.001:

| Constant | Meaning | Forms seen | How we handled it |
|---|---|---|---|
| `shl reg,0x9` | `Y*512` forward cell index | 496 raw → 472 real (`*4` cell) | rewrite imm `09`→`0A`. Classified cell vs non-cell by whether a `[base+reg*4]` cell access follows. |
| `cmp eax,0x40000` | cell-index bound check (`3D` opcode) | **405** | 401 are real (followed by `[reg+eax*4]`); **4 are 256 KB buffer checks** (`0x565B73`,`0x568710`,`0x5687A7`,`0x568B58`) — MUST skip or you corrupt a buffer → the Ares crash. |
| `cmp reg,0x40000` | cell **loop bounds** (`81 /7`, non-eax regs) | **37** | all cell-code; my `cmp eax` scan missed them entirely → cell-construction loops stopped at 262144. |
| `push 0x40000` | VectorClass reserve for the cell ptr array | 2 | must rise → `0x100000`, else the array overflows at stride>512. |
| `mov eax,0x200` @`0x565812` | `MAP_CELL_W/H` (→ `[MapClass+0x14c]`/`+0x150`) | 1 | **root** map dimension. Plain immediate — invisible to shl/cmp scans. |
| `mov [ebp+0x154],0x40000` @`0x565828` | `TotalCells`, **sizes the cell-array allocation** | 1 | **root** allocation size in `MapClass::Init` @`0x565800`. This is why cells >262144 read heap garbage. |
| `and reg,0x1FF` + `sar reg,9` | inverse index→(X,Y) **division** | 2 blocks | signed div-512 idiom half. |
| `and reg,0x800001FF` + `or reg,0xFFFFFE00` | inverse index→(X,Y) **modulo** | 2 blocks | the *paired* signed mod-512 half. **Patching only the div half mangles X → null lookup → crash.** Both halves or neither. Marker `and reg,0x800001FF` occurs in exactly 2 places → idiom is fully bounded. |
| `test/and/xor/or reg,0x40000` | **bit 18 flag**, NOT a count | 12 | **DO NOT PATCH** — these are flags/enums, patching breaks logic. |

**Cross-DLL (Ares.dll / Phobos.dll):** they have their OWN ×512 cell math. Patched relative to `GetModuleHandle` base (ImageBase `0x10000000`, loaded/relocated at runtime). Ares: 63 `shl` (+1 `cmp`). Phobos: 48 `shl` + 3 `cmp0x40000` + 3 `and0x1FF` + 1 `sar9`. Ares has **0** inverse-conversion sites → it *calls gamemd's* inverse, so gamemd+Ares must agree or Ares crashes.

**Key MapClass offsets (gamemd 1.001):**
- `+0x13c` = `VectorClass<CellClass*>::data` (the cell pointer array)
- `+0x140` = cell count
- `+0x14c`/`+0x150` = MAP_CELL_W / MAP_CELL_H (512)
- `+0x154` = TotalCells (262144) — sizes the allocation via `call [vtable+0x58]`
- `+0xf4`/`+0xf8` = actual (per-map) dimensions used in bounds math
- global `0x87F924` = cell array pointer used by inline accessors
- global `0x87F7E8` ≈ MapClass instance region

---

## 3. The crash journey (each = one fixed phase)

| # | Crash | Root cause | Fix |
|---|---|---|---|
| 1 | can't load W+H>512 | FA2/editor limit + dimension gates | dimension-gate hooks (Phase 1) |
| 2 | `0x584DF7` | spawn on map edge | generator: interior spawn ring |
| 3 | `0x77945A38` (Phobos) | clear tile = 0 | clear tile = **0xFFFF** |
| 4 | `0x551CAC` | IsoMapPack5 record order | order `(ry,rx)` |
| 5 | garbage overlay | overlay compression | LZO → **Format80/LCW** |
| 6 | `0x77DBDEEA` (Ares) | one of 4 buffer `cmp` false-positives bumped 256KB→1MB | skip the 4 non-`*4` cmp sites |
| 7 | `0x410174` | cell array only 512² allocated (root dims unpatched) → cell idx 278773 reads heap garbage `0xFFFFFFFF` used as `this` | patch root dims @`0x565800` + 37 `cmp reg` loop bounds |
| 8 | `0x77DBDEEA` (Ares) **[WALL]** | see §4 | pivot to Antares |

**Method that worked:** classify by *instruction semantics*, not just the constant. The `*4` cell-array-access signature separates real cell sites from coincidental 256KB/512 constants. Verify each site's bytes before writing; NO-OP at stride 512; log every action.

---

## 4. The `0x880A04` wall (why we pivot)

- `.data` ends at VMA `0x87E000`; `0x880A04` is in **`.bss`** — a zero-init global pointer.
- It is **read 11×** across gamemd (`0x4312b0`, `0x660xxx`, `0x70da48`) and Ares (`0x4D2E2`, `0x4DEE0`) but **written by NO code** in gamemd/Ares/Phobos/Spawner (verified every write encoding: `a3`, `89 /r`, `c7 05`, and address-taken `lea`/`mov imm`/`push`). Permanently null.
- The object does FP coordinate transforms (`fmul`/`fidiv`; vtable methods `+0x5c`,`+0x78`,`+0x90`). It's an **Ares coordinate/projection singleton**.
- The crash: `mov ecx,[0x880A04]; mov eax,[ecx]` with `ecx=0`, at `Ares+0x4DEEA`, reached via a `DisplayClass` transform. `EAX=0x10064` is a stale cell index (`100,64` @ 1024).
- Guarded by global `0xB73550` (a DDraw/window resource, non-null on large maps) — but forcing the "safe" branch didn't help: **both** branches forward to `0x880A04`.
- **Conclusion:** this is dead code in the compiled Ares.dll that our modified coordinate range trips. Not a patchable constant. Needs runtime debugging OR — better — **Antares**, whose reimplemented coordinate/reveal system uses runtime `MapRect` dimensions and likely doesn't have this dead path.

---

## 5. "Claude vibe-coding" engineering lessons (YR + Syringe + CI)

Durable traps we paid for, so the next person doesn't:

- **`SyringeHandshake` runs in Syringe.exe's process**, BEFORE gamemd is loaded. Patching `0x400000` there hits Syringe, not gamemd → page fault at `mapsizeext+0x1c58`. **Do all real work in `DllMain(DLL_PROCESS_ATTACH)`**, guarded by `_strnicmp(host_exe,"gamemd",6)==0`. DllMain runs in gamemd before WinMain and before Syringe installs trampolines → `.text` is pristine there.
- **`DEFINE_HOOK` needs the `.syhks00` section**, which only exists when **`SYR_VER=2`** is defined. Without it the DLL builds but injects nothing.
- With `SYR_VER=2`, `DEFINE_HOOK` addresses are **bare hex** (`DEFINE_HOOK(5656EA, …)`) because the macro does `0x ## hook`.
- **Injection order = `-i=` order**; MapSizeExt is last, so Ares/Phobos are already loaded when its DllMain runs → `GetModuleHandleA("Ares.dll")` works for cross-DLL patching, and their `.text` is still their compiled code (Syringe installs gamemd-side trampolines after all DllMains).
- The **real CnCNet launcher** is `Resources/Compatibility/Unix/wine-game.sh` (the `-i=…MapSizeExt.dll` list lives there), NOT `ClientDefinitions.ini`. `ModINIRestore` picks up loose INIs in the game dir.
- **Windows builds are CI-only** (no local MSVC). vcxproj needs `SYR_VER=2;WINDOWS_IGNORE_PACKING_MISMATCH;_CRT_SECURE_NO_WARNINGS`, `StructMemberAlignment=1Byte`, `OptimizeReferences=false`, `EnableCOMDATFolding=false`. windows-2019 runner is retired → windows-2022.
- **Byte-patch philosophy:** power-of-two stride ⇒ most sites are one immediate byte; NO-OP at 512 by construction; verify opcode+imm before writing; VirtualProtect→write→restore; log everything; make each patch group INI-toggleable so a tester can bisect a crash **without a rebuild** (this was the single most valuable debugging tool — see `[Debug]` in `MAPSIZEEXT.INI`).
- **False positives are the enemy.** `0x40000` is both `512²` and `256 KB`; `0x200` is both `512` and countless other things; `0x40000` also = bit 18. Always classify by *how the value is used* (does a `[base+reg*4]` cell access follow?), never by the constant alone.
- **BIGGEST META-LESSON: check for open source before disassembling.** Ares (and now Antares) are open source. Weeks of binary RE on `Ares.dll` could have been a source lookup. Always search the ecosystem's repos first.

---

## 6. NEXT STEP — the Antares pivot (resume here)

1. **Obtain `Antares.dll`** (Phobos-developers/Antares release or CI artifact). Antares is **incompatible with Ares.dll** — they cannot coexist.
2. In `wine-game.sh`, replace `-i=Ares.dll` with `-i=Antares.dll` (keep the `.msx-bak` backup pattern). Note other DLLs (Kratos, TechnoAttachmentExt, etc.) that assume Ares — check compatibility.
3. Load the **200×200** first (regression check with Antares), then the **300×300** with MapSizeExt at Stride=1024.
4. If it gets past `0x880A04`: Antares's dynamic `MapRevealer` likely also solves Phase-2 sight/shroud for free (test whether the `0x493xxx` cluster still needs patching — per cross-codebase doc §3).
5. When a wall appears, **read the Antares source** for the relevant class instead of disassembling.
6. Keep this doc + the address tables updated; feed §2 into the Hook Encyclopedia.

**Fallback if Antares stalls too:** the `[Debug]` toggle harness + this taxonomy reproduce every state; a future session can attach a live debugger to the wine process and break at the `0x880A04` reader to see the real call path.

---

## 7. Procedural map editor / generator — SCOPE (RA2MapGen)

Added 2026-07-30 (companion project `~/Claude/RA2MapGen`, Python; validated IsoMapPack5 codec via liblzo2 + Format80). Target features:

- **Procedural generation** of full playable `.map` files (multi-size, multi-theater).
- **Structure placement:** civilian buildings, tech buildings, urban props — understand *ideal build area* (flat, clear, near resources).
- **Economy & balance awareness:** ore/gem field placement, expansion timing, **choke points**, fair starts.
- **Artistic integrity:** coherent terrain, believable **cities, roads, forests, rivers, bridges**.
- **Two map "modes":** tournament-friendly **symmetric/mirrored** maps vs. **asymmetric-but-visually-appealing** maps.
- **Grow an existing map by extension:** add new terrain onto the edges, keeping style coherent.
- **Grow an existing map by stretching:** scale up and *coherently fill the gaps* so it still looks hand-made.
- **Semantic terrain understanding:** classify/generate cities, roads, forests, shores, cliffs.

**Leverage the open source:** **Antares `RMG.cpp`/`RMG.h`** is the *original Ares Random Map Generator*, now open source, with all `MapSeedClass` hook addresses mapped (see cross-codebase doc §4). This is the reference implementation for in-engine procedural generation — study it for the generation algorithms (urban areas, paved roads, bridges, medians) rather than reinventing. `MapSeedClass_LoadFromINI` @`0x5982D5` is where size params would be read.

---

## 8. Repos & references

- MapSizeExt: https://github.com/SethGekco/MapSizeExt
- Hook Encyclopedia: https://github.com/SethGekco/YR-Hook-Encyclopedia
- Ares (open source 0.A): https://github.com/Ares-Developers/Ares
- **Antares (3.0p1 reimpl, the pivot target):** https://github.com/Phobos-developers/Antares
- Phobos: https://github.com/Phobos-developers/Phobos
- yr-patches (IsoMapPack5 asm): https://github.com/CnCNet/yr-patches
- WAE editor: https://github.com/CnCNet/WorldAlteringEditor
- Cross-codebase reference: `~/Downloads/MapSize_CrossCodebase_Reference.md`
