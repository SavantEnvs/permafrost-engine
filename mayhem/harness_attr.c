/* Fuzzes the attribute-line parser used by the PFOBJ/PFSCENE/PFSAVE text formats, then writes
 * each parsed attribute back out. */
#include "../src/lib/public/attr.h"

#include <SDL.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if(size < 1)
        return -1;

    bool named = data[0] & 1;
    SDL_RWops *stream = SDL_RWFromConstMem(data + 1, size - 1);
    if(!stream)
        return -1;

    char out_buff[4096];
    struct attr attr;
    bool parsed = false;

    while(true) {
        memset(&attr, 0, sizeof(attr));
        if(!Attr_Parse(stream, &attr, named))
            break;
        parsed = true;

        SDL_RWops *out = SDL_RWFromMem(out_buff, sizeof(out_buff));
        if(out) {
            Attr_Write(out, &attr, named ? attr.key : NULL);
            SDL_RWclose(out);
        }
    }

    SDL_RWclose(stream);
    return parsed ? 0 : -1;
}
