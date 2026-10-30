# anim-joint-null-sscanf

**Found locally** (not among the defects of the Mayhem run, which stopped earlier at the two `render_asset_load.c` findings): `al_read_joint` (`src/anim/anim_asset_load.c:84`, also lines 89/94/99) passes the NULL result of `pf_strtok_r` to `sscanf` on a truncated joint (`j`) line, a SEGV in `strlen`.

Reproduce: `PFM_KEEP_KNOWN_BUGS=1 mayhem/fuzz_pfobj-standalone mayhem/fuzz_pfobj/known-findings/anim-joint-null-sscanf/input`. The harness skips such inputs unless that variable is set; the input is not in `testsuite/` because it aborts the run.
