
#include <internal/map_base.h>

#include <stdbool.h>

#if defined(_WIN32)
#pragma comment(lib, "synchronization.lib")
#endif

int MAP_TOMBSTONE;
int MAP_NO_MATCH_OLD;

_Thread_local bool map_ebr_init;
_Thread_local map_ebr_entry_t map_ebr;
atomic_int map_ebr_count;
_Atomic(map_ebr_entry_t*) map_ebr_list;

atomic_flag map_free_thread_init;

map_freenode_t MAP_DUMMY;

_Atomic(map_freenode_t*) map_free_head = &MAP_DUMMY;
_Atomic(map_freenode_t*) map_free_tail = &MAP_DUMMY;

_Atomic uint32_t map_free_done;
_Atomic uint32_t map_free_count;

int map_thread_fn(void *arg) {
    #if MAP_DEBUG
        spall_auto_thread_init(111111, SPALL_DEFAULT_BUFFER_SIZE);
    #endif

    for (;;) retry: {
        // Use a futex to avoid polling too hard
        uint32_t old;
        while (old = map_free_count, old == map_free_done) {
            #if defined(_WIN32)
                int WaitOnAddress(volatile void *addr, void *compare_addr, size_t addr_size, unsigned long millis);
                WaitOnAddress(&map_free_count, &old, 4, -1);
            #elif defined(__linux__)
                int futex_rc = futex(SYS_futex, &map_free_count, FUTEX_WAIT, old, NULL, NULL, 0);
                assert(futex_rc >= 0);
            #elif defined(__APPLE__)
                int __ulock_wait(uint32_t operation, void *addr, uint64_t value, uint32_t timeout);
                int res = __ulock_wait(0x01000001, &map_free_count, old, 0);
                assert(res >= 0);
            #else
                // No futex support, just sleep once we run out of tasks
                thrd_sleep(&(struct timespec){.tv_nsec=100000000}, NULL);
            #endif
        }

        map_freenode_t *p = atomic_load_explicit(&map_free_head, memory_order_relaxed);
        do {
            if (p->next == NULL) {
                // it's empty
                goto retry;
            }
        } while (!atomic_compare_exchange_strong(&map_free_head, &p, p->next));
        map_table_t *table = p->next->table;

        // Handling deferred freeing without blocking up the normie threads
        int state_count  = map_ebr_count;
        uint64_t* states = MAP_REALLOC(NULL, state_count * sizeof(uint64_t));

        MAP_BEGIN("scan");
        map_ebr_entry_t *us = &map_ebr;
        // "snapshot" the current statuses, once the other threads either advance or aren't in the
        // hashset functions we know we can free.
        for (map_ebr_entry_t *list = atomic_load(&map_ebr_list); list; list = list->next) {
            // mark sure no ptrs refer to prev
            if (list != us && list->id < state_count) {
                states[list->id] = list->time;
            }
        }

        // important bit is that pointers can't be held across the critical sections, they'd need
        // to reload from `map_t.latest`.
        //
        // Here's the states of our "epoch" critical section thingy:
        //
        // UNPINNED(id) -> PINNED(id) -> UNPINNED(id + 1) -> UNPINNED(id + 1) -> ...
        //
        // survey on if we can free the pointer if the status changed from X -> Y:
        //
        //   # YES: if we started unlocked then we weren't holding pointers in the first place.
        //   UNPINNED(A) -> PINNED(A)
        //   UNPINNED(A) -> UNPINNED(A)
        //   UNPINNED(A) -> UNPINNED(B)
        //
        //   # YES: if we're locked we need to wait until we've stopped holding pointers.
        //   PINNED(A)   -> PINNED(B)     we're a different call so we've let it go by now.
        //   PINNED(A)   -> UNPINNED(B)   we've stopped caring about the state of the pointer at this point.
        //
        //   # NO: we're still doing shit, wait a sec.
        //   PINNED(A)   -> PINNED(A)
        //
        // these aren't quite blocking the other threads, we're simply checking what their progress is concurrently.
        for (map_ebr_entry_t *list = atomic_load(&map_ebr_list); list; list = list->next) {
            if (list != us && list->id < state_count && (states[list->id] & MAP_PINNED_BIT)) {
                uint64_t before_t = states[list->id], now_t;
                do {
                    // idk, maybe this should be a better spinlock
                    now_t = atomic_load(&list->time);
                } while (before_t == now_t);
            }
        }
        MAP_END();

        // no more refs, we can immediately free
        MAP_VIRTUAL_FREE(table, sizeof(map_table_t) + table->cap * sizeof(map_entry_t));
        MAP_REALLOC(states, 0);

        map_free_done++;

        #if MAP_DEBUG
            spall_auto_buffer_flush();
        #endif
    }
}
