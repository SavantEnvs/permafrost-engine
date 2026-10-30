/* Fuzzes the PFMAP terrain loader: header, chunks, materials, splats, render buffers and navigation grid. */
#include "../src/asset_load.h"
#include "../src/map/public/map.h"
#include "../src/map/public/tile.h"
#include "../src/lib/public/pf_string.h"
#include "../src/settings.h"

#include <SDL.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* The loader rejects more than 256 materials or splats; a larger count only costs stack. */
#define MAX_MATERIALS 256
/* Chunks are ~25 KB of text each and cost megabytes to build; keep an exec fast. */
#define MAX_CHUNKS    4
/* A tile is a 24-character token; padding rows are made of this tile, four to a line (the loader
 * reads lines of at most 255 characters). */
#define FILLER_TILE   "0+0000000001101001111000 "
#define ROW_TILES     4

bool pfm_parse_pfmap_header(SDL_RWops *stream, struct pfmap_hdr *out);
void pfm_stub_reset(void);
void pfm_sched_init_main_thread(void);

/* Reproducer switch: with PFM_KEEP_KNOWN_BUGS set, inputs of the documented upstream bug shapes are not skipped. */
static bool keep_known_bugs(void)
{
    return getenv("PFM_KEEP_KNOWN_BUGS") != NULL;
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    pfm_sched_init_main_thread();
    return Settings_Init() == SS_OKAY ? 0 : 1;
}

/*
 * Upstream bugs documented under mayhem/fuzz_pfmap/known-findings/. Each fires on a handful of bytes
 * of the materials, splats or tile rows, so left in they end nearly every exec before the render and
 * navigation code is reached. Walks the file the way the loader does and returns true when it meets
 * one of them, so the input is skipped; anything else the loader rejects itself.
 */
static bool hits_known_bug(const uint8_t *data, size_t size, size_t body_off, const struct pfmap_hdr *hdr)
{
    SDL_RWops *s = SDL_RWFromConstMem(data, size);
    char line[MAX_LINE_LEN];
    bool bug = false;

    if(!s)
        return false;
    SDL_RWseek(s, body_off, RW_SEEK_SET);

    for(unsigned i = 0; i < hdr->num_materials; i++) {

        if(!AL_ReadLine(s, line))
            goto out;
        line[MAX_LINE_LEN - 1] = '\0';

        char *save;
        char *tok = pf_strtok_r(line, " \t\n", &save);
        if(!tok) { bug = true; goto out; }           /* strcmp(NULL, "material") */
        if(strcmp(tok, "material") != 0)
            goto out;
        if(!pf_strtok_r(NULL, " \t\n", &save)      /* material name */
        || !pf_strtok_r(NULL, " \t\n", &save)) {   /* texture name: strncpy(dst, NULL) */
            bug = true;
            goto out;
        }
    }

    for(unsigned i = 0; i < hdr->num_splats; i++) {

        if(!AL_ReadLine(s, line))
            goto out;
        line[MAX_LINE_LEN - 1] = '\0';

        char *save;
        char *tok = pf_strtok_r(line, " \t\n", &save);
        if(!tok) { bug = true; goto out; }           /* strcmp(NULL, "splat") */
        if(strcmp(tok, "splat") != 0)
            goto out;
        char *base = pf_strtok_r(NULL, " \t\n", &save);
        if(!base) { bug = true; goto out; }          /* sscanf(NULL, "%u") */
        unsigned unused;
        if(!sscanf(base, "%u", &unused))
            goto out;
        if(!pf_strtok_r(NULL, " \t\n", &save)) { bug = true; goto out; }
    }

    /* A row may carry more tiles than are left in its chunk; the extra ones are written past it. */
    for(unsigned c = 0; c < hdr->num_rows * hdr->num_cols; c++) {

        size_t tiles_read = 0;
        while(tiles_read < TILES_PER_CHUNK_WIDTH * TILES_PER_CHUNK_HEIGHT) {

            if(!AL_ReadLine(s, line))
                goto out;
            line[MAX_LINE_LEN - 1] = '\0';

            char *save;
            size_t n = 0;
            for(char *t = pf_strtok_r(line, " \t\n", &save); t; t = pf_strtok_r(NULL, " \t\n", &save)) {
                if(strlen(t) != 24)
                    goto out;
                if(tiles_read + n >= TILES_PER_CHUNK_WIDTH * TILES_PER_CHUNK_HEIGHT) {
                    bug = true;
                    goto out;
                }
                n++;
            }
            tiles_read += n;
        }
    }

out:
    SDL_RWclose(s);
    return bug;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct pfmap_hdr header;
    memset(&header, 0, sizeof(header));

    SDL_RWops *stream = SDL_RWFromConstMem(data, size);
    if(!stream)
        return -1;

    if(!pfm_parse_pfmap_header(stream, &header)) {
        SDL_RWclose(stream);
        return -1;
    }
    size_t body_off = SDL_RWtell(stream);
    SDL_RWclose(stream);

    /* num_materials == 0 is a zero-length VLA in M_AL_InitMapFromStream (known finding). */
    if(header.num_rows == 0 || header.num_cols == 0
    || header.num_rows > MAX_CHUNKS || header.num_cols > MAX_CHUNKS
    || header.num_rows * header.num_cols > MAX_CHUNKS
    || (header.num_materials == 0 && !keep_known_bugs())
    || header.num_materials > MAX_MATERIALS
    || header.num_splats > MAX_MATERIALS)
        return -1;

    /*
     * A chunk is 32x32 tiles of 24 characters, ~25 KB, but Mayhem caps libFuzzer inputs at 8 KB, so
     * a mutant can never carry a whole chunk and would stop in the tile rows every time. Keep the
     * mutated bytes up to the last complete line and finish the file with plain tile rows.
     */
    size_t keep = size;
    while(keep > body_off && data[keep - 1] != '\n')
        keep--;
    if(keep < body_off)
        keep = size;

    size_t row_len = ROW_TILES * (sizeof(FILLER_TILE) - 1) + 1;
    size_t pad_rows = (size_t)header.num_rows * header.num_cols
                    * TILES_PER_CHUNK_WIDTH * TILES_PER_CHUNK_HEIGHT / ROW_TILES;
    size_t total = keep + pad_rows * row_len;
    uint8_t *buf = malloc(total);
    if(!buf)
        return -1;
    memcpy(buf, data, keep);
    uint8_t *p = buf + keep;
    for(size_t r = 0; r < pad_rows; r++) {
        for(int t = 0; t < ROW_TILES; t++) {
            memcpy(p, FILLER_TILE, sizeof(FILLER_TILE) - 1);
            p += sizeof(FILLER_TILE) - 1;
        }
        *p++ = '\n';
    }

    if(!keep_known_bugs() && hits_known_bug(buf, total, body_off, &header)) {
        free(buf);
        return -1;
    }

    stream = SDL_RWFromConstMem(buf, total);
    if(!stream) {
        free(buf);
        return -1;
    }

    struct map *map = AL_MapFromPFMapStream(stream, false);
    SDL_RWclose(stream);
    free(buf);
    pfm_stub_reset();
    if(!map)
        return -1;

    AL_MapFree(map);
    return 0;
}
