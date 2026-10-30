/*
 * The task scheduler normally spawns worker threads and switches stacks. The map loader only
 * asks it "which task am I?" (Sched_TryYield); outside a task the answer is NULL_TID and the
 * yield is a no-op. Compiling sched.c into this translation unit lets the harness set up just
 * that bookkeeping without starting any threads.
 */
#include "../src/sched.c"

void pfm_sched_init_main_thread(void)
{
    int status;

    if(s_thread_tid_map)
        return;

    g_main_thread_id = SDL_ThreadID();
    s_thread_tid_map = kh_init(tid);
    khiter_t k = kh_put(tid, s_thread_tid_map, thread_id_to_key(g_main_thread_id), &status);
    kh_value(s_thread_tid_map, k) = NULL_TID;
}
