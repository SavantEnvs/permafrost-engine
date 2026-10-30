/*
 * Link-time stand-ins for the parts of the engine that need a window, a GL context, the render
 * thread or the embedded Python interpreter. The fuzz harnesses only run the engine's file-format
 * parsers; the parsers hand their results to the renderer as queued commands, which here are
 * simply dropped. The scripting layer is not linked (it needs a Python 2.7 build), so the
 * entry points the simulation code reaches into are inert.
 */
#include "../src/main.h"
#include "../src/render/public/render_ctrl.h"
#include "../src/script/public/script.h"

#include <stdlib.h>
#include <string.h>

SDL_threadID g_main_thread_id;
SDL_threadID g_render_thread_id;
unsigned long g_frame_idx;
const char   *g_basepath = "/tmp";

bool Engine_ShuttingDown(void)                          { return false; }
void Engine_WinDrawableSize(int *out_w, int *out_h)     { *out_w = 1920; *out_h = 1080; }
void Engine_WaitRenderWorkDone(void)                    { }
void Engine_FlushRenderWorkQueue(void)                  { }

bool R_InitWS(struct render_workspace *ws)              { (void)ws; return true; }
void R_DestroyWS(struct render_workspace *ws)           { (void)ws; }
void R_ClearWS(struct render_workspace *ws)             { (void)ws; }
bool R_ComputeShaderSupported(void)                     { return false; }
void R_Yield(void)                                      { }

bool S_UI_MouseOverWindow(int x, int y)                 { (void)x; (void)y; return false; }
bool S_UI_MouseOverObscuringWindow(int x, int y)        { (void)x; (void)y; return false; }
bool S_UI_TextEditHasFocus(void)                        { return false; }

void           S_Release(script_opaque_t obj)           { (void)obj; }
void           S_Retain(script_opaque_t obj)            { (void)obj; }
script_opaque_t S_UnwrapIfWeakref(script_opaque_t arg)  { return arg; }
bool           S_WeakrefDied(script_opaque_t arg)       { (void)arg; return false; }
bool           S_ObjectsEqual(script_opaque_t a, script_opaque_t b) { return a == b; }
script_opaque_t S_WrapEngineEventArg(int ev, void *arg) { (void)ev; (void)arg; return NULL; }
void           S_RunEventHandler(script_opaque_t c, script_opaque_t u, void *e) { (void)c; (void)u; (void)e; }
bool           S_RunFile(const char *path, int argc, char **argv) { (void)path; (void)argc; (void)argv; return false; }
bool           S_GetFilePath(char *out, size_t maxout)  { (void)out; (void)maxout; return false; }

void S_ClearState(void)                                 { }
bool S_SaveState(SDL_RWops *stream)                     { (void)stream; return false; }
bool S_LoadState(SDL_RWops *stream)                     { (void)stream; return false; }
void S_Task_MaybeExit(void)                             { }
void S_Task_MaybeEnter(void)                            { }
void S_Task_KillAll(void)                               { }

int      S_FormationPriority(uint32_t uid)              { (void)uid; return 0; }
uint64_t S_ScriptTypeID(uint32_t uid)                   { (void)uid; return 0; }
uint64_t S_Entity_TypeID(script_opaque_t obj)           { (void)obj; return 0; }
bool     S_Entity_UIDForObj(script_opaque_t obj, uint32_t *out) { (void)obj; (void)out; return false; }
script_opaque_t S_Entity_ObjForUID(uint32_t uid)        { (void)uid; return NULL; }
script_opaque_t S_Entity_ObjFromAtts(const char *path, const char *name,
    const khash_t(attr) *attr_table, const vec_attr_t *construct_args)
{
    (void)path; (void)name; (void)attr_table; (void)construct_args;
    return NULL;
}
script_opaque_t S_Region_ObjFromAtts(const char *name, int type, vec2_t pos,
    float radius, float xlen, float zlen)
{
    (void)name; (void)type; (void)pos; (void)radius; (void)xlen; (void)zlen;
    return NULL;
}
void S_Region_NotifyContentsChanged(const char *name)   { (void)name; }
script_opaque_t S_Camera_ObjFromAtts(const char *name, vec3_t pos, float pitch, float yaw)
{
    (void)name; (void)pos; (void)pitch; (void)yaw;
    return NULL;
}

bool Engine_InRunningState(void)
{
    return false;
}

struct arg_node{
    struct arg_node *next;
};

static struct arg_node *s_args;

void *R_AllocArg(size_t size)
{
    struct arg_node *node = malloc(sizeof(*node) + size);
    if(!node)
        return NULL;
    node->next = s_args;
    s_args = node;
    return node + 1;
}

void *R_PushArg(const void *src, size_t size)
{
    void *ret = R_AllocArg(size);
    if(ret)
        memcpy(ret, src, size);
    return ret;
}

void R_PushCmd(struct rcmd cmd)                { (void)cmd; }
void R_PushCmdImmediate(struct rcmd cmd)       { (void)cmd; }
void R_PushCmdImmediateFront(struct rcmd cmd)  { (void)cmd; }

/* Called by the harnesses once per input, dropping everything the parsers queued. */
void pfm_stub_reset(void)
{
    while(s_args) {
        struct arg_node *next = s_args->next;
        free(s_args);
        s_args = next;
    }
}
