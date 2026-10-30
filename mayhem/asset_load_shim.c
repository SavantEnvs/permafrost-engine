/*
 * asset_load.c keeps its PFOBJ/PFMAP header parsers static. Compiling the upstream file into
 * this translation unit (instead of on its own) lets the harnesses call them directly.
 */
#include "../src/asset_load.c"

bool pfm_parse_pfobj_header(SDL_RWops *stream, struct pfobj_hdr *out)
{
    return al_parse_pfobj_header(stream, out);
}

bool pfm_parse_pfmap_header(SDL_RWops *stream, struct pfmap_hdr *out)
{
    return al_parse_pfmap_header(stream, out);
}
