# jpeg-huffman-index-1713

**Found by Mayhem** (`fuzz-stb-image`, `stbi__build_huffman`, `src/lib/public/stb_image.h:1713`, 2 defects): same oversized DHT table as `jpeg-huffman-index-1712`, tripping the write of the terminating size entry at index 257. 32-byte reproducer.

Reproduce: `mayhem/fuzz_stb_image-standalone mayhem/fuzz_stb_image/known-findings/jpeg-huffman-index-1713/input`. Kept out of `testsuite/` because it aborts the run.
