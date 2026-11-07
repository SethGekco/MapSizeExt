# Changelog

Newest first. Older history is in `git log` and `docs/HANDOFF.md`.

## Unreleased — per-house threat grid sized to the map (`[Debug] PatchThreat=1`)

**Why:** big maps crashed or froze more often the more players there were
(reported: 2 players fine, 4 shaky, more fail at load or soon after). Measured
2026-11-06 on `mapsizeext_1000x1000.map`: 8 players ran 866 s clean; 30 players
crashed at game frame 2632 (C0000005 at 0x4E96A3, `FindItemIndex` on a house's
`UnitRepairStations` with Items=NULL and Count=15) while 922 MB of address space
was still free.

Cause: every house carries an inline threat grid, `unsigned[130][130]` at
house+0x57E4, indexed by 4x4-cell regions (`MapClass::Cell_Region` @0x56BC50).
`HouseClass::AdjustThreat` @0x4FA2E0 writes region±131 into the grid of every
enemy house, with no upper bound. Antares replaces `Cell_Region` (returns -1 for
X or Y >= 512) but computes `130*Y/4` instead of `(Y/4)*130`, so cells at
Y 509-511 index up to 97 entries past the grid. Houses are allocated back to back,
so those writes hit the next house's vtable and vectors. A >2 GB pointer reads as
negative and gets zeroed by the threat clamp. The test map's start 0 is (513,512),
and PlayerCountExt parks every seat-less house there, so 30 players corrupted
houses within a minute.

**What:** on big maps the region becomes 2^(log2(stride)-7) cells (8 at 1024,
16 at 2048), in both gamemd's body and Antares' handler (bound 0x200 -> stride).
Every coordinate below the stride now lands inside the grid, and threat is
tracked over the whole map instead of only the 512x512 corner. This also matches
the subzone block scale (4->8 table at 1024, 4->16 table at 2048), so the zone
threat read @0x5862A2 uses the same grid. Byte-verified; vanilla-size maps are
untouched.

**Known limits:** stride-2048 maps that keep the 4->8 subzone table (W+H in
1025..~1430, e.g. 700x700) still give zone records a scale-8 region, so the read
@0x5862A2 can look past the grid; it only reads. Antares' `130*Y/4` slip also
exists at stride 512 on maps reaching Y 509; MapSizeExt leaves that alone because
it never patches vanilla-size maps.

**Consumer action:** none (default on). `PatchThreat=0` restores the old behaviour.

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
