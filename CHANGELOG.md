# Changelog

Newest first. Older history is in `git log` and `docs/HANDOFF.md`.

## Unreleased — per-map stride (`[MapSize] Auto=1`, default)

**Why:** the DLL applied the global `Stride=2048` patch set to every launch, and that
crashed ordinary skirmish maps in a 30-DLL stack (2026-09-15, 140x90 map; again
2026-10-25, EIP=0). It was pulled from the launch list, so big maps then crashed
at load instead (2026-11-06, 1000x1000: cell index past the 512x512 array).

**What:** at init (DllMain) the DLL reads the scenario the spawner will load
(`spawn.ini [Settings] Scenario=`, default `spawnmap.ini`) and picks the stride:

- **Vanilla-size map** (`W+H <= 512` and `[IsoMapPack5]` <= 512,000 base64-decoded
  bytes) -> stride 512: no byte patch, no crash watcher, and all 53 hooks pass
  through (`g_Active = 0`). 45 hooks `return 0`; the 8 whose stolen bytes hold a
  relative branch/call or end mid-instruction (dimension gates 0x4C5630/0x4C590E/
  0x554BC5, tiberium fill 0x722E0F, A* pool mallocs 0x42A7E8/0x42A81C/0x42A8E5,
  inline access 0x483B32) replay the original instructions themselves, because
  classic Syringe copies stolen bytes unrelocated.
- **Otherwise** the smallest of 1024 / 2048 with `W+H <= stride`, and the existing
  patch set. The IsoMapPack5 read buffer now also grows to fit the map's data
  (a 500x500 map can carry 2.4 MB, more than stride 1024's 1088x1088x2).
- The decision is the first line of `MapSizeExt.log`.

Rules measured, not assumed: cells run 1..W+H-1 on both axes on real and
generated maps; the 400x640x2 buffer at 0x4AD343 holds the base64-decoded
IsoMapPack5 (read at 0x4AD687). Saved-game loads (the client writes a 50x50 stub
spawnmap.ini) and scenarios inside a MIX are treated as vanilla-size; `Auto=0`
restores the old global `Stride=` behaviour for those.

Also fixed in passing: at stride 512 the A* pool hooks returned 0 over their
stolen `call 0x7C8E17`, a wild jump under classic Syringe.

**Consumer action:** none; `Auto` defaults to 1, so existing INIs switch to
per-map strides. Verified in-game: see the entry's commit message / follow-up.
