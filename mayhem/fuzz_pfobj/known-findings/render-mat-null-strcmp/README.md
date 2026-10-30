# render-mat-null-strcmp

**Found by Mayhem** (`fuzz-pfobj`, first confirmation run): `al_read_material` (`src/render/render_asset_load.c:159`) calls `strcmp` on the second `pf_strtok_r` token of a `material` line without a NULL check; a line with no material name makes it NULL (UBSan nonnull-attribute, then a NULL strcmp).

Reproduce: `PFM_KEEP_KNOWN_BUGS=1 mayhem/fuzz_pfobj-standalone mayhem/fuzz_pfobj/known-findings/render-mat-null-strcmp/input`. The harness skips such inputs unless that variable is set, so the run reaches the animation and AABB code; the input is not in `testsuite/` because it aborts the run.
