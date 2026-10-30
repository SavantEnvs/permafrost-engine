# render-texname-heap-overflow

**Found by Mayhem** (`fuzz-pfobj`, first confirmation run): `al_read_material` (`src/render/render_asset_load.c:177`) reads a `texture` line into the 64-byte `texname` with an unbounded `%s`, so a longer token is a heap-buffer-overflow WRITE (ASan reports 75 bytes in the stored reproducer).

Reproduce: `PFM_KEEP_KNOWN_BUGS=1 mayhem/fuzz_pfobj-standalone mayhem/fuzz_pfobj/known-findings/render-texname-heap-overflow/input`. The harness skips such inputs unless that variable is set; the input is not in `testsuite/` because it aborts the run.
