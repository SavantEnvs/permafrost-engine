# map-texname-null-strncpy

**Found locally** (not among the defects of the Mayhem run, which stopped earlier at `map-materials-zero-vla`): `m_al_read_material` (`src/map/map_asset_load.c:210`) calls `strncpy` with the NULL result of `pf_strtok_r` when a `material` line has no texture name (UBSan nonnull-attribute).

Reproduce: `PFM_KEEP_KNOWN_BUGS=1 mayhem/fuzz_pfmap-standalone mayhem/fuzz_pfmap/known-findings/map-texname-null-strncpy/input`. The harness skips such inputs unless that variable is set.
