# jpeg-huffman-index-1712

**Found by Mayhem** (`fuzz-stb-image`, `stbi__build_huffman`, `src/lib/public/stb_image.h:1712`, 4 defects): a JPEG DHT table whose code counts total more than 256 symbols indexes `h->size[257]` out of bounds (UBSan array-bounds). 29-byte reproducer.

Reproduce: `mayhem/fuzz_stb_image-standalone mayhem/fuzz_stb_image/known-findings/jpeg-huffman-index-1712/input`. Kept out of `testsuite/` because it aborts the run.
