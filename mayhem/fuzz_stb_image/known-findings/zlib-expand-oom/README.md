# zlib-expand-oom

**Found locally** (not among the defects of the Mayhem run): a ~2.8 KB PNG whose IDAT stream is a deflate bomb makes `stbi__zexpand` (`src/lib/public/stb_image.h:3906`) realloc past 2 GB; stb_image has no decompressed-size cap, only the declared image dimensions are checked.

Reproduce: `mayhem/fuzz_stb_image-standalone mayhem/fuzz_stb_image/known-findings/zlib-expand-oom/input` (or libFuzzer's default `-rss_limit_mb=2048`). Kept out of `testsuite/` because it exhausts memory.
