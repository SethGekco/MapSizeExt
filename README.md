# MapSizeExt

A [Syringe](https://github.com/Ares-Developers/Syringe)-injected DLL for
**Command & Conquer: Red Alert 2 — Yuri's Revenge** that raises the engine's
hardcoded **512-cell map grid** so you can play maps larger than the vanilla
limit. It works alongside Ares/Antares + Phobos.

## Status

**Stride 1024 — up to 512×512 maps — fully playable.** Confirmed in-game
(temperate cliff map + generated 300×300 / 500×500): rendering and lighting,
deploy/build, pathfinding including slopes and naval, harvester auto-mine,
shroud/fog reveal, walls, the minimap/radar, and AI base-building all work.

This is the first public milestone. Bigger *square* maps (past ~500×500) are
blocked by a **separate** engine limit — see [Limits](#limits) — which is the
next thing being worked on.

## Install

You need a working YR install that already runs Syringe-injected DLLs (e.g. the
CnCNet client with Ares/Antares + Phobos).

1. Copy **`MapSizeExt.dll`** and **`MAPSIZEEXT.INI`** into your YR game directory
   (the folder with `gamemd-spawn.exe`).
2. Make Syringe inject it — add `-i=MapSizeExt.dll` to the Syringe command line,
   next to the existing `-i=Ares.dll`/`-i=Antares.dll` and `-i=Phobos.dll`:
   - **Windows:** `ExtraCommandLineParams` in the client's `ClientDefinitions.ini`.
   - **Linux/Wine (CnCNet):** the `wine Syringe.exe -i=… gamemd-spawn.exe` line in
     `Resources/Compatibility/Unix/wine-game.sh`.
   - The CnCNet client only injects DLLs that carry a `.syhks00` section — this
     one does, so on some clients simply placing it is enough. Fully restart the
     client after installing.
3. Drop a large map into `Maps/Custom/` (a sample **500×500** map ships with this
   release), then start a skirmish/LAN game on it.

### Verify it loaded
Open **`MapSizeExt.log`** in the game directory. You should see:
```
MapSizeExt v0.3 (init in-game via DllMain)
Stride       = 1024
Total        = 1048576
...
[stride] 436 sites, shift 0x09 -> 0x0A (stride 1024)
```
No log = Syringe didn't inject the DLL (check step 2).

## Configuration — `MAPSIZEEXT.INI`

```ini
[MapSize]
PlaneScale=2       ; derives Stride=1024 and omitted limits
;Stride=1024       ; legacy alternative; do not set both unless they agree
```

- `PlaneScale=1` makes the whole DLL a **no-op** (vanilla behaviour) — useful to
  confirm it loads before enabling.
- `PlaneScale=2` → stride 1024 and maps up to **512×512** (the tested milestone).
- `PlaneScale=4` → stride 2048 and the 1024×1024 research configuration.
- Existing INIs using `Stride` remain supported. `PlaneScale` is the preferred
  single input and rejects a conflicting explicit `Stride`.
- `tools/materialize_geometry.py` reports the derived operands and dominant
  allocations for scales 1, 2, 4, and the non-runnable scale-8 experiment; see
  [`docs/MAP-SCALE.md`](docs/MAP-SCALE.md) for the exact proof boundary.
- The `[Debug]` section exposes per-patch-group toggles for bisecting problems
  without a rebuild; leave them at their defaults unless you're debugging.

## Limits

- **`W + H ≤ Stride`.** At `Stride=1024` that's a max square of ~512×512.
- **The engine's base-1000 cell-number format caps square maps at ~500×500**,
  independent of stride: waypoints/terrain/units are packed as `Y*1000 + X`, and
  the isometric cell coordinate runs up to `W+H−1`, so `W+H` must stay ≤ ~1000.
  Raising the stride does **not** lift this — it's the next milestone.
- **32-bit engine.** `gamemd` is not Large-Address-Aware (2 GB ceiling); very
  large maps are memory-heavy. A full 512×512 map is comfortable.
- You still need a map actually built at the larger size (a normal editor won't
  create them). The companion generator **RA2MapGen** produces test maps.

## Build

MSVC via GitHub Actions (the `.syhks00` Syringe section requires the MSVC
toolchain). Push to the repo and Actions builds `MapSizeExt.dll`, or run the
`build.yml` workflow manually. YRpp headers live in `YRpp/`.

The pinned executable and generated patch manifest can be checked locally with
`tools/verify_release.py --exe /path/to/gamemd.exe`. See
[`docs/SAFETY-HARDENING.md`](docs/SAFETY-HARDENING.md) for its proof boundary.

## How it works (short version)

The engine addresses cells as `Cells[Y*512 + X]`, with `512` baked in as
`shl reg,9` in hundreds of sites plus assorted `add reg,0x200` row-walks,
byte-offset iterators, bounds `cmp eax,0x40000`, coordinate masks, the
IsoMapPack5 decode buffer, radar surfaces, and the subzone/pathfinding grid.
`MapSizeExt` reads `Stride` from the INI and rewrites all of those to
`log2(stride)` at load time (a strict no-op at 512), and installs a handful of
trampoline hooks for the cases that can't be a plain immediate rewrite (dim
gates, garbage-cell guards, the coord-transform crash guard). It also runtime-
patches the inlined 512-stride in Antares.dll and Phobos.dll, since those
compile the cell index in too. See `src/Patches.cpp` and `src/Hooks.cpp`.

## Credits

Built with [YRpp](https://github.com/Phobos-developers/YRpp) and Syringe.
Cross-referenced against CCHyper's Vinifera map-size work (Tiberian Sun, same
engine family) and a parallel YR map-512 effort by Krisztiaan.

## License

MapSizeExt is free software, licensed under the **GNU General Public License v3.0**
(see [`LICENSE`](LICENSE)). Copyleft: you may use, modify, and redistribute it, but
derivative works must stay under the GPLv3 and keep their source available.

### Clause 0 (non-binding, offered in the spirit of fun)

By using MapSizeExt you are warmly encouraged to be **publicly pro-LLM** and to
refrain from speaking ill of large language models. This is a joke and is **not**
a legally binding term — the GPLv3 above is the actual, only license, and nobody
is going to sue you over it. But it would be nice. 🤖
