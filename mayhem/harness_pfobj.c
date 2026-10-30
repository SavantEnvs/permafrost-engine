/* Fuzzes the PFOBJ model loader: header, mesh vertices, materials, skeleton, animation clips and bounds. */
#include "../src/asset_load.h"
#include "../src/anim/anim_texture.h"
#include "../src/render/public/render_al.h"
#include "../src/anim/public/anim.h"
#include "../src/phys/public/collision.h"
#include "../src/pf_math.h"
#include "../src/mem.h"
#include "../src/lib/public/pf_string.h"

#include <SDL.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool pfm_parse_pfobj_header(SDL_RWops *stream, struct pfobj_hdr *out);
void pfm_stub_reset(void);
void pfm_sched_init_main_thread(void);

/* sizeof(((struct material*)0)->texname) and ANIM_NAME_LEN; a longer token overruns the buffer. */
#define TEXNAME_LEN    64
#define JOINT_NAME_LEN 64

/*
 * Upstream bugs documented under mayhem/fuzz_pfobj/known-findings/. Each fires on a handful of bytes
 * of the material or joint lines, so left in they end every exec before the animation clips and the
 * AABB are reached. Walks the file the way the loaders do and returns true when it meets one of them,
 * so the input is skipped; anything else the loaders reject themselves.
 */
static bool hits_known_bug(const uint8_t *data, size_t size, size_t body_off, const struct pfobj_hdr *hdr)
{
    SDL_RWops *s = SDL_RWFromConstMem(data, size);
    char line[MAX_LINE_LEN];
    bool bug = false;
    int *parents = calloc(hdr->num_joints + 1, sizeof(int));

    if(!s || !parents) {
        free(parents);
        if(s)
            SDL_RWclose(s);
        return false;
    }
    SDL_RWseek(s, body_off, RW_SEEK_SET);

    /* Five lines per vertex: v, vt, vn, vw, vm. */
    for(uint64_t i = 0; i < (uint64_t)hdr->num_verts * 5; i++) {
        if(!AL_ReadLine(s, line))
            goto out;
    }

    for(unsigned i = 0; i < hdr->num_materials; i++) {

        if(!AL_ReadLine(s, line))
            goto out;
        line[MAX_LINE_LEN - 1] = '\0';

        char *save;
        pf_strtok_r(line, " \t\n", &save);
        char *name = pf_strtok_r(NULL, " \t\n", &save);
        if(!name) { bug = true; goto out; }          /* strcmp(NULL, "__none__") */
        if(0 == strcmp(name, "__none__"))
            continue;

        float f[3];
        for(int l = 0; l < 3; l++) {
            if(!AL_ReadLine(s, line))
                goto out;
            line[MAX_LINE_LEN - 1] = '\0';
            int ok = (l == 0) ? sscanf(line, " ambient %f", &f[0])
                   : (l == 1) ? sscanf(line, " diffuse %f %f %f", &f[0], &f[1], &f[2])
                              : sscanf(line, " specular %f %f %f", &f[0], &f[1], &f[2]);
            if(!ok)
                goto out;
        }

        if(!AL_ReadLine(s, line))
            goto out;
        line[MAX_LINE_LEN - 1] = '\0';
        char tex[MAX_LINE_LEN];
        if(sscanf(line, " texture %255s", tex) == 1 && strlen(tex) >= TEXNAME_LEN) {
            bug = true;                              /* unbounded %s into texname[64] */
            goto out;
        }
    }

    for(unsigned i = 0; i < hdr->num_joints; i++) {

        if(!AL_ReadLine(s, line))
            goto out;
        line[MAX_LINE_LEN - 1] = '\0';

        int parent;
        char name[MAX_LINE_LEN];
        int nconv = sscanf(line, "j %d %255s", &parent, name);
        if(nconv == 0)
            goto out;
        /* The loader's %s reads whatever follows the digits, which need not be the third token. */
        if(nconv == 2 && strlen(name) >= JOINT_NAME_LEN) { bug = true; goto out; }

        char *save;
        char *toks[7];
        int n = 0;
        for(char *t = pf_strtok_r(line, " \t", &save); t && n < 7; t = pf_strtok_r(NULL, " \t", &save))
            toks[n++] = t;
        if(n < 7) { bug = true; goto out; }          /* sscanf(NULL, ...) on a short joint line */
        if(strlen(toks[2]) >= JOINT_NAME_LEN) { bug = true; goto out; }
        /* The parent index (1-based, 0 = root) indexes the joint array when bind poses are built. */
        if(parent < 0 || (unsigned)parent > hdr->num_joints) { bug = true; goto out; }
        parents[i] = parent - 1;
    }

    /* A joint that is its own ancestor makes A_PrepareInvBindMatrices loop forever. */
    for(unsigned i = 0; i < hdr->num_joints; i++) {
        unsigned steps = 0;
        for(int p = parents[i]; p >= 0; p = parents[p]) {
            if(++steps > hdr->num_joints) { bug = true; goto out; }
        }
    }

    for(unsigned i = 0; i < hdr->num_as; i++) {

        if(!AL_ReadLine(s, line))
            goto out;
        line[MAX_LINE_LEN - 1] = '\0';

        /* The loader only rejects a line when no conversion succeeds. */
        char name[MAX_LINE_LEN];
        unsigned frames = hdr->frame_counts[i];
        int n = sscanf(line, "as %255s %u", name, &frames);
        if(n == 0)
            goto out;
        if(n >= 1 && strlen(name) >= JOINT_NAME_LEN) { bug = true; goto out; }
        /* The clip's samples are carved out of the buffer by the header's frame count. */
        if(frames > hdr->frame_counts[i]) { bug = true; goto out; }

        for(uint64_t l = 0; l < (uint64_t)frames * (hdr->num_joints + (hdr->has_collision ? 3 : 0)); l++) {
            if(!AL_ReadLine(s, line))
                goto out;
        }
    }

out:
    free(parents);
    SDL_RWclose(s);
    return bug;
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    pfm_sched_init_main_thread();
    return A_Texture_Init() ? 0 : 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    int ret = -1;
    struct pfobj_hdr header;
    struct aabb aabb;
    void *vbuff = NULL;
    size_t vbuff_size = 0;
    void *render_priv = NULL, *anim_priv = NULL;

    memset(&header, 0, sizeof(header));

    SDL_RWops *stream = SDL_RWFromConstMem(data, size);
    if(!stream)
        return -1;

    if(!pfm_parse_pfobj_header(stream, &header))
        goto out;

    /* Every vertex, joint and material takes at least one byte of the file. */
    if(header.num_verts > size || header.num_joints > size || header.num_materials > size)
        goto out;
    /* Likewise every animation frame takes at least one line; a larger count only asks for gigabytes. */
    for(unsigned i = 0; i < header.num_as && i < sizeof(header.frame_counts) / sizeof(header.frame_counts[0]); i++)
        if(header.frame_counts[i] > size)
            goto out;

    if(!getenv("PFM_KEEP_KNOWN_BUGS") /* reproducer switch */ && hits_known_bug(data, size, (size_t)SDL_RWtell(stream), &header))
        goto out;

    render_priv = R_AL_PrivFromStream("/tmp", &header, stream, &vbuff, &vbuff_size);
    if(!render_priv)
        goto out;

    anim_priv = A_AL_PrivFromStream("fuzz.pfobj", &header, stream);
    if(!anim_priv)
        goto out;

    if(!header.has_collision || !AL_ParseAABB(stream, &aabb))
        goto out;

    ret = 0;
out:
    if(anim_priv && header.num_as > 0) {
        /* The animation table caches by name; reset it so every input reaches the copy. */
        A_Texture_Shutdown();
        A_Texture_Init();
    }
    PF_FREE(vbuff);
    PF_FREE(render_priv);
    PF_FREE(anim_priv);
    SDL_RWclose(stream);
    pfm_stub_reset();
    return ret;
}
