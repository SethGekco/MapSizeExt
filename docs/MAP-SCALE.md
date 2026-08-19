# Single map-scale input

`PlaneScale` is a user-facing multiplier over the stock 512-cell plane stride.
It is deliberately thin: existing patch code continues to derive its operands
from `Stride`, while configuration derives that one authority as
`Stride = 512 * PlaneScale`.

| PlaneScale | Derived stride | Largest square implied by `W+H <= Stride` | Runtime status |
|---:|---:|---:|---|
| 1 | 512 | 256x256 | stock no-op |
| 2 | 1024 | 512x512 | current tested milestone |
| 4 | 2048 | 1024x1024 | accepted research configuration |
| 8 | 4096 | 2048x2048 | materialization only; rejected at runtime |

Omitted `MaxDimension`, `MaxWidth`, and `MaxHeight` values are derived from the
scale. Existing `Stride` INIs remain compatible. If both keys are present they
must describe the same geometry, preventing partially edited configurations.

Run `tools/materialize_geometry.py` for exact shifts, masks, displacements, and
the dominant directly derived allocations. Those values are arithmetic, not a
playability claim: scale 4 still needs full Windows regression, and scale 8
exceeds the reviewed runtime range. The tool intentionally does not generate
patch tables because current subzone policy is map-size-dependent rather than a
single linear formula.

Larger logical maps also require all external coordinate producers, consumers,
save/load paths, triggers, and multiplayer serialization to agree on the widened
cell-number contract. The latest research branch covers several of those paths,
but a successful operand materialization alone does not close that protocol.
