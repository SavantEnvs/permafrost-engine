# jpeg-extend-receive-shift

**Found by Mayhem** (`fuzz-stb-image`, `stbi__extend_receive`, `src/lib/public/stb_image.h:1857` and `:1859`): a Huffman symbol with a magnitude `n` of more than 16 bits is not rejected. `stbi__extend_receive` then shifts a 32-bit value by 32 or more (UBSan shift-exponent, `:1857`), and the same `n` indexes the 17-entry `stbi__bmask` out of bounds (`:1859`). Mayhem reported the two lines as separate defects (`:1859` in run #3); they share one root cause. 164-byte reproducer.

Reproduce: `mayhem/fuzz_stb_image-standalone mayhem/fuzz_stb_image/known-findings/jpeg-extend-receive-shift/input`. Kept out of `testsuite/` because it aborts the run.
