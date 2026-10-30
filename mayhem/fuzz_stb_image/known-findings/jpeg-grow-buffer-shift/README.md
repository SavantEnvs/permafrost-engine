# jpeg-grow-buffer-shift

**Found by Mayhem** (`fuzz-stb-image`, `stbi__grow_buffer_unsafe`, `src/lib/public/stb_image.h:1787`): the bit buffer is shifted by a negative count once a corrupt scan drains it (UBSan shift-exponent -1). 155-byte reproducer.

Reproduce: `mayhem/fuzz_stb_image-standalone mayhem/fuzz_stb_image/known-findings/jpeg-grow-buffer-shift/input`. Kept out of `testsuite/` because it aborts the run.
