# map-materials-zero-vla

**Found by Mayhem** (first confirmation run, `fuzz-pfmap`, high): `M_AL_InitMapFromStream` (`src/map/map_asset_load.c:610`) declares `STALLOC(char, texnames, header->num_materials * 256)`, a variable-length array, so a header with `num_materials 0` is a zero-length VLA (UBSan `vla-bound`).

Reproduce: `PFM_KEEP_KNOWN_BUGS=1 mayhem/fuzz_pfmap-standalone mayhem/fuzz_pfmap/known-findings/map-materials-zero-vla/input`. The harness skips such inputs unless that variable is set, so the run reaches the render and navigation code; the input is not in `testsuite/` because it aborts the run.
