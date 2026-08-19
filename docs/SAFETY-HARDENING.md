# Pinned-profile safety hardening

This branch layers bounded safety fixes on the latest
`fix/phase1-correctness` research line without removing its newer CoordBase,
order-codec, Phobos, subzone, or A* work.

- Configuration is validated with checked geometry before any mutation.
- Syringe negotiation rejects unknown executable size/timestamp/CRC profiles.
- In-process initialization verifies the expected YR 1.001 PE shape.
- The broad `0x40000` runtime scan is replaced by a generated exact-site
  manifest tied to the pinned executable.
- Save/load restores the five phase-sensitive iterator operands while stock
  post-load reconstruction runs, then widens only for an exact known capacity.
- Tiberium validates its derived count and all three related allocations.
  Continuing with an empty first vector only moved the crash into the immediate
  downstream consumer, so allocation failure now terminates deterministically.
- Garbage-cell identity checks use `VirtualQuery` before reading candidate
  object coordinates.

`tools/verify_release.py` pins the executable SHA-256, critical hook prologues,
all 437 stride sites, and reproducibility of the generated bounds manifest.
This is static proof only; the PR still requires an MSVC build and fresh Windows
runtime regression on 512- and 1024-stride scenarios.
