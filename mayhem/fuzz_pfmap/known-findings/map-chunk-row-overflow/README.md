# map-chunk-row-overflow

**Found locally** while emulating the Mayhem run: `m_al_read_pfchunk` reads tile rows until a chunk has 1024 tiles, but a row may hold more tokens than are left, so `m_al_parse_tile` (`src/map/map_asset_load.c:111`, via `m_al_read_row` at `:170`) writes the extra tiles past the chunk (ASan heap-buffer-overflow). The input has 1027 tiles in the first chunk.

Reproduce: `PFM_KEEP_KNOWN_BUGS=1 mayhem/fuzz_pfmap-standalone mayhem/fuzz_pfmap/known-findings/map-chunk-row-overflow/input`. The harness skips such rows unless that variable is set.
