/* Fuzzes the stb_image decoder the engine uses for every texture, icon and skybox. */
#include "../src/lib/public/stb_image.h"

#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if(size < 2 || size > (1 << 18))
        return -1;

    int w, h, n;
    if(!stbi_info_from_memory(data, size, &w, &h, &n))
        return -1;

    /* Keep the decode bounded; the info call above already reads the header. */
    if((long)w * (long)h > (1 << 22))
        return -1;

    int req_comp = data[0] % 5;
    unsigned char *img = stbi_load_from_memory(data, size, &w, &h, &n, req_comp);
    if(!img)
        return -1;

    volatile unsigned char sink = img[0];
    (void)sink;
    stbi_image_free(img);
    return 0;
}
