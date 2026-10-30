/*
 * Command-line probe over the engine's asset parsers, built with the engine's normal flags (no
 * sanitizers). mayhem/test.sh feeds it real assets and compares the parsed values it prints with
 * known answers.
 *
 *   pf_probe attr  <text>   parse one attribute line (named: 'key type value', unnamed: 'type value')
 *   pf_probe pfobj <file>   parse a PFOBJ model: header, mesh, materials, skeleton, clips, bounds
 *   pf_probe pfmap <file>   parse a PFMAP terrain and print its size and sampled heights
 *   pf_probe image <file>   decode an image and print its size and pixel checksum
 */
#include "../src/asset_load.h"
#include "../src/anim/anim_data.h"
#include "../src/anim/anim_texture.h"
#include "../src/anim/public/anim.h"
#include "../src/render/public/render_al.h"
#include "../src/render/render_private.h"
#include "../src/render/gl_material.h"
#include "../src/lib/public/attr.h"
#include "../src/lib/public/stb_image.h"
#include "../src/map/map_private.h"
#include "../src/map/public/map.h"
#include "../src/mem.h"
#include "../src/phys/public/collision.h"
#include "../src/settings.h"

#include <SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool pfm_parse_pfobj_header(SDL_RWops *stream, struct pfobj_hdr *out);
void pfm_stub_reset(void);
void pfm_sched_init_main_thread(void);

static unsigned char *slurp(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if(!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc(len > 0 ? len : 1);
    if(!buf || fread(buf, 1, len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    *out_len = len;
    return buf;
}

static int probe_attr(const char *text)
{
    struct attr attr;
    char line[256];
    memset(&attr, 0, sizeof(attr));
    snprintf(line, sizeof(line), "%s\n", text);
    SDL_RWops *stream = SDL_RWFromConstMem(line, strlen(line));
    bool named = text[0] != '\0' && strncmp(text, "string ", 7) && strncmp(text, "float ", 6)
              && strncmp(text, "int ", 4) && strncmp(text, "vec2 ", 5) && strncmp(text, "vec3 ", 5)
              && strncmp(text, "quat ", 5) && strncmp(text, "bool ", 5);

    if(!Attr_Parse(stream, &attr, named)) {
        printf("attr parse=failed\n");
        SDL_RWclose(stream);
        return 1;
    }
    SDL_RWclose(stream);

    printf("attr key=%s type=%d\n", named ? attr.key : "-", (int)attr.type);
    switch(attr.type) {
    case TYPE_STRING: printf("attr value=%s\n", attr.val.as_string); break;
    case TYPE_FLOAT:  printf("attr value=%.4f\n", attr.val.as_float); break;
    case TYPE_INT:    printf("attr value=%d\n", attr.val.as_int); break;
    case TYPE_BOOL:   printf("attr value=%d\n", (int)attr.val.as_bool); break;
    case TYPE_VEC2:   printf("attr value=%.4f,%.4f\n", attr.val.as_vec2.x, attr.val.as_vec2.y); break;
    case TYPE_VEC3:   printf("attr value=%.4f,%.4f,%.4f\n",
                             attr.val.as_vec3.x, attr.val.as_vec3.y, attr.val.as_vec3.z); break;
    case TYPE_QUAT:   printf("attr value=%.4f,%.4f,%.4f,%.4f\n", attr.val.as_quat.x,
                             attr.val.as_quat.y, attr.val.as_quat.z, attr.val.as_quat.w); break;
    default:          printf("attr value=?\n"); break;
    }
    return 0;
}

static int probe_pfobj(const char *path)
{
    size_t len;
    unsigned char *buf = slurp(path, &len);
    if(!buf)
        return 2;

    int ret = 1;
    struct pfobj_hdr header;
    struct aabb aabb;
    void *vbuff = NULL, *render_priv = NULL, *anim_priv = NULL;
    size_t vbuff_size = 0;
    SDL_RWops *stream = SDL_RWFromConstMem(buf, len);

    if(!pfm_parse_pfobj_header(stream, &header)) {
        printf("pfobj header=failed\n");
        goto out;
    }
    printf("pfobj header verts=%u joints=%u materials=%u anim_sets=%u collision=%d\n",
        header.num_verts, header.num_joints, header.num_materials, header.num_as,
        (int)header.has_collision);

    render_priv = R_AL_PrivFromStream("/tmp", &header, stream, &vbuff, &vbuff_size);
    if(!render_priv) {
        printf("pfobj render=failed\n");
        goto out;
    }
    const struct render_private *rpriv = render_priv;
    printf("pfobj render verts=%u materials=%zu vbuff_bytes=%zu\n",
        rpriv->mesh.num_verts, rpriv->num_materials, vbuff_size);
    for(size_t i = 0; i < rpriv->num_materials; i++) {
        const struct material *mat = &rpriv->materials[i];
        printf("pfobj material %zu texture=%s ambient=%.3f diffuse=%.3f,%.3f,%.3f\n", i, mat->texname,
            mat->ambient_intensity, mat->diffuse_clr.x, mat->diffuse_clr.y, mat->diffuse_clr.z);
    }

    anim_priv = A_AL_PrivFromStream("probe.pfobj", &header, stream);
    if(!anim_priv) {
        printf("pfobj anim=failed\n");
        goto out;
    }
    const struct anim_data *adata = anim_priv;
    printf("pfobj skeleton joints=%zu clips=%u\n", adata->skel.num_joints, adata->num_anims);
    for(size_t i = 0; i < adata->skel.num_joints; i++) {
        const struct joint *j = &adata->skel.joints[i];
        const struct SQT *b = &adata->skel.bind_sqts[i];
        printf("pfobj joint %zu name=%s parent=%d scale=%.3f,%.3f,%.3f trans=%.3f,%.3f,%.3f\n", i,
            j->name, j->parent_idx, b->scale.x, b->scale.y, b->scale.z,
            b->trans.x, b->trans.y, b->trans.z);
    }
    for(unsigned i = 0; i < adata->num_anims; i++) {
        const struct anim_clip *clip = &adata->anims[i];
        const struct aabb *sb = &clip->samples[0].sample_aabb;
        printf("pfobj clip %u name=%s frames=%u frame0_aabb=%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", i,
            clip->name, clip->num_frames, sb->x_min, sb->x_max, sb->y_min, sb->y_max,
            sb->z_min, sb->z_max);
    }

    if(!header.has_collision || !AL_ParseAABB(stream, &aabb)) {
        printf("pfobj aabb=failed\n");
        goto out;
    }
    printf("pfobj aabb=%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
        aabb.x_min, aabb.x_max, aabb.y_min, aabb.y_max, aabb.z_min, aabb.z_max);
    ret = 0;
out:
    SDL_RWclose(stream);
    free(buf);
    return ret;
}

static int probe_pfmap(const char *path)
{
    size_t len;
    unsigned char *buf = slurp(path, &len);
    if(!buf)
        return 2;

    SDL_RWops *stream = SDL_RWFromConstMem(buf, len);
    struct map *map = AL_MapFromPFMapStream(stream, false);
    SDL_RWclose(stream);
    free(buf);
    if(!map) {
        printf("pfmap load=failed\n");
        return 1;
    }

    vec3_t pos = M_GetPos(map);
    printf("pfmap chunks=%zux%zu pos=%.1f,%.1f,%.1f\n", map->width, map->height, pos.x, pos.y, pos.z);

    /* Tile centers, as (row, col), 8 world units per tile. */
    static const int tiles[][2] = {{0, 0}, {3, 5}, {20, 10}, {31, 31}, {3, 6}, {4, 5}};
    for(size_t i = 0; i < sizeof(tiles) / sizeof(tiles[0]); i++) {
        vec2_t xz = {pos.x - (tiles[i][1] * 8 + 4), pos.z + (tiles[i][0] * 8 + 4)};
        printf("pfmap height(row=%d,col=%d)=%.4f\n", tiles[i][0], tiles[i][1], M_HeightAtPoint(map, xz));
    }
    AL_MapFree(map);
    return 0;
}

static int probe_image(const char *path)
{
    size_t len;
    unsigned char *buf = slurp(path, &len);
    if(!buf)
        return 2;

    int w, h, n;
    unsigned char *img = stbi_load_from_memory(buf, len, &w, &h, &n, 0);
    free(buf);
    if(!img) {
        printf("image decode=failed\n");
        return 1;
    }

    uint64_t sum = 0, hash = 1469598103934665603ULL;
    for(size_t i = 0; i < (size_t)w * h * n; i++) {
        sum += img[i];
        hash = (hash ^ img[i]) * 1099511628211ULL;
    }
    printf("image size=%dx%d channels=%d sum=%" PRIu64 " fnv=%016" PRIx64 "\n", w, h, n, sum, hash);
    printf("image first_pixel=");
    for(int c = 0; c < n; c++)
        printf("%s%u", c ? "," : "", img[c]);
    printf("\n");
    stbi_image_free(img);
    return 0;
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        fprintf(stderr, "usage: pf_probe attr|pfobj|pfmap|image <arg>\n");
        return 2;
    }

    pfm_sched_init_main_thread();
    if(Settings_Init() != SS_OKAY || !A_Texture_Init())
        return 2;

    int ret = 2;
    if(!strcmp(argv[1], "attr"))
        ret = probe_attr(argv[2]);
    else if(!strcmp(argv[1], "pfobj"))
        ret = probe_pfobj(argv[2]);
    else if(!strcmp(argv[1], "pfmap"))
        ret = probe_pfmap(argv[2]);
    else if(!strcmp(argv[1], "image"))
        ret = probe_image(argv[2]);
    pfm_stub_reset();
    return ret;
}
