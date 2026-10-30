# anim-joint-sscanf-overflow

**Found by Mayhem** (`fuzz-pfobj`, run #3, `al_read_joint`, `src/anim/anim_asset_load.c:70`): a joint line whose first field is fused with a long run, such as `j 0AAAA...AAAA arrow1 ...` (90 bytes here), makes `sscanf(line, "j %d %s", &unfixed_idx, out->name)` copy that whole run into the 64-byte `joint.name`, a heap-buffer-overflow write. The loader's `%s` does not read the third token (`arrow1`); after `%d` it takes whatever follows the digits.

The input is a copy of `testsuite/tiny_anim.pfobj` with `num_as` set to 0 (the joints are then the last members of the animation buffer, so the overrun leaves the allocation) and the last joint line changed, ending after the joint lines.

Reproduce: `PFM_KEEP_KNOWN_BUGS=1 mayhem/fuzz_pfobj-standalone mayhem/fuzz_pfobj/known-findings/anim-joint-sscanf-overflow/input`. The harness skips such inputs unless that variable is set (it tests the name `sscanf` produces); the input is not in `testsuite/` because it aborts the run.

Related: `anim-joint-null-sscanf` covers the later `sscanf(NULL, ...)` calls at lines 84-99 of the same function.
