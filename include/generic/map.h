
#if !defined(TYPE)
    #error not defined: TYPE(t)
#endif

#if !defined(HASH)
    #error not defined: HASH(v)
#endif

#include <internal/map_base.h>

extern int MAP_TOMBSTONE;
extern int MAP_NO_MATCH_OLD;

extern _Thread_local bool map_ebr_init;
extern _Thread_local map_ebr_entry_t map_ebr;
extern _Atomic(int) map_ebr_count;
extern _Atomic(map_ebr_entry_t*) map_ebr_list;

extern atomic_flag map_free_thread_init;

extern _Atomic(map_freenode_t*) map_free_head;
extern _Atomic(map_freenode_t*) map_free_tail;

extern _Atomic uint32_t map_free_done;
extern _Atomic uint32_t map_free_count;

extern int map_thread_fn(void *);

static void *TYPE(put_if_match)(map_t *hs, map_table_t *latest, map_table_t *prev, void *key, void *val, void *exp);

static size_t TYPE(hash2index)(map_table_t *table, uint64_t h) {
    // MulHi(h, table->a)
    #if defined(__GNUC__) || defined(__clang__)
        uint64_t hi = (uint64_t) (((unsigned __int128)h * table->a) >> 64);
    #elif defined(_MSC_VER)
        uint64_t hi;
        _umul128(a, b, &hi);
    #else
        #error "Unsupported target"
    #endif

    uint64_t q  = hi >> table->sh;
    uint64_t q2 = h - (q * table->cap);

    assert(q2 == h % table->cap);
    return q2;
}

// flips the top bit on
static void TYPE(enter_pinned)(void) {
    uint64_t t = atomic_load_explicit(&map_ebr.time, memory_order_relaxed);
    atomic_store_explicit(&map_ebr.time, t + MAP_PINNED_BIT, memory_order_release);
}

// flips the top bit off AND increments time by one
static void TYPE(exit_pinned)(void) {
    uint64_t t = atomic_load_explicit(&map_ebr.time, memory_order_relaxed);
    atomic_store_explicit(&map_ebr.time, t + MAP_PINNED_BIT + 1, memory_order_release);
}

map_table_t *TYPE(move_items)(map_t *hm, map_table_t *latest, map_table_t *prev, int items_to_move) {
    assert(prev);
    size_t cap = prev->cap;

    // snatch up some number of items
    uint32_t old, new;
    do {
        old = atomic_load(&prev->moved);
        if (old == cap) { return prev; }
        // cap the number of items to copy... by the cap
        new = old + items_to_move;
        if (new > cap) { new = cap; }
    } while (!atomic_compare_exchange_strong(&prev->moved, &(uint32_t){ old }, new));

    if (old == new) {
        return prev;
    }

    MAP_BEGIN("copying old");
    for (size_t i = old; i < new; i++) {
        void *old_v = atomic_load(&prev->data[i].val);
        void *k     = atomic_load(&prev->data[i].key);

        // freeze the values by adding a prime bit.
        while (((uintptr_t) old_v & MAP_PRIME_BIT) == 0) {
            uintptr_t primed_v = (old_v == &MAP_TOMBSTONE ? 0 : (uintptr_t) old_v) | MAP_PRIME_BIT;
            if (atomic_compare_exchange_strong(&prev->data[i].val, &old_v, (void *) primed_v)) {
                if (old_v != NULL && old_v != &MAP_TOMBSTONE) {
                    // once we've frozen, we can move it to the new table.
                    // we pass NULL for prev since we already know the entries exist in prev.
                    TYPE(put_if_match)(hm, latest, NULL, k, old_v, &MAP_NO_MATCH_OLD);
                }
                break;
            }
            // btw, CAS updated old_v
        }
    }
    MAP_END();

    uint32_t done = atomic_fetch_add(&prev->move_done, new - old);
    done += new - old;

    assert(done <= cap);
    if (done == cap) {
        // dettach now
        MAP_BEGIN("detach");
        latest->prev = NULL;

        if (!atomic_flag_test_and_set(&map_free_thread_init)) {
            thrd_t freeing_thread; // don't care to track it
            thrd_create(&freeing_thread, map_thread_fn, NULL);
        }

        map_freenode_t *new_node = MAP_REALLOC(NULL, sizeof(map_freenode_t));
        new_node->table = prev;
        new_node->next  = NULL;

        // enqueue, it's a low-size low-contention list i just don't wanna block my lovely normie threads
        map_freenode_t *p = atomic_load_explicit(&map_free_tail, memory_order_relaxed);
        map_freenode_t *old_p = p;
        do {
            while (p->next != NULL) {
                p = p->next;
            }
        } while (!atomic_compare_exchange_strong(&p->next, &(map_freenode_t*){ NULL }, new_node));
        atomic_compare_exchange_strong(&map_free_tail, &old_p, new_node);

        map_free_count++;

        // signal futex
        #if defined(_WIN32)
            extern void WakeByAddressSingle(void *addr);
            WakeByAddressSingle(&map_free_count);
        #elif defined(__linux__)
            int futex_rc = futex(SYS_futex, &map_free_count, FUTEX_WAKE, 1, NULL, NULL, 0);
            assert(futex_rc >= 0);
        #elif defined(__APPLE__)
            extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);
            int res = __ulock_wake(0x01000001, &map_free_count);
            assert(res >= 0);
        #endif

        prev = NULL;
        MAP_END();
    }
    return prev;
}

static void TYPE(ebr_try_init)(void) {
    if (!map_ebr_init) {
        MAP_BEGIN("init");
        map_ebr_init = true;
        map_ebr.id = map_ebr_count++;

        // add to ebr list, we never free this because i don't care
        // TODO(NeGate): i do care, this is a nightmare when threads die figure it out
        map_ebr_entry_t *old;
        do {
            old = atomic_load_explicit(&map_ebr_list, memory_order_relaxed);
            map_ebr.next = old;
        } while (!atomic_compare_exchange_strong(&map_ebr_list, &old, &map_ebr));
        MAP_END();
    }
}

static void *TYPE(raw_lookup)(map_t *hs, map_table_t *table, uint32_t h, void *key) {
    size_t cap = table->cap;
    size_t first = TYPE(hash2index)(table, h), i = first;
    do {
        void *v = atomic_load(&table->data[i].val);
        void *k = atomic_load(&table->data[i].key);

        if (k == NULL) {
            return NULL;
        } else if (TYPE(cmp)(k, key)) {
            return v != &MAP_TOMBSTONE ? v : NULL;
        }

        // inc & wrap around
        i = (i == cap-1) ? 0 : i + 1;
    } while (i != first);

    return NULL;
}

static void *TYPE(put_if_match)(map_t *hs, map_table_t *latest, map_table_t *prev, void *key, void *val, void *exp) {
    assert(key);

    void *k, *v;
    for (;;) {
        uint32_t cap = latest->cap;
        size_t limit = (cap * MAP_LOAD_FACTOR) / 100;
        if (prev == NULL && latest->count >= limit) {
            // make resized table, we'll amortize the moves upward
            size_t new_cap = map_compute_cap(limit * 2);

            map_table_t *new_top = MAP_VIRTUAL_ALLOC(sizeof(map_table_t) + new_cap*sizeof(map_entry_t));
            map_compute_size(new_top, new_cap);

            // CAS latest -> new_table, if another thread wins the race we'll use its table
            new_top->prev = latest;
            if (!atomic_compare_exchange_strong(&hs->latest, &latest, new_top)) {
                MAP_VIRTUAL_FREE(new_top, sizeof(map_table_t) + new_cap*sizeof(map_entry_t));
                prev = atomic_load(&latest->prev);
            } else {
                prev   = latest;
                latest = new_top;

                // float s = sizeof(map_table_t) + new_cap*sizeof(map_entry_t);
                // printf("Resize: %.2f KiB (cap=%zu)\n", s / 1024.0f, new_cap);
            }
            continue;
        }

        // key claiming phase:
        //   once completed we'll have a key inserted into the latest
        //   table (the value might be NULL which means that the entry
        //   is still empty but we've at least agreed where the value
        //   goes).
        bool found = false;
        uint32_t h = HASH(key);
        size_t first = TYPE(hash2index)(latest, h), i = first;
        do {
            v = atomic_load_explicit(&latest->data[i].val, memory_order_acquire);
            k = atomic_load_explicit(&latest->data[i].key, memory_order_acquire);

            if (k == NULL) {
                // key was never in the table
                if (val == &MAP_TOMBSTONE) { return NULL; }

                // fight for empty slot
                if (atomic_compare_exchange_strong(&latest->data[i].key, &k, key)) {
                    atomic_fetch_add_explicit(&latest->count, 1, memory_order_relaxed);
                    found = true;
                    break;
                }
            }

            if (TYPE(cmp)(k, key)) {
                found = true;
                break;
            }

            // inc & wrap around
            i = (i == cap-1) ? 0 : i + 1;
        } while (i != first);

        // we didn't claim a key, that means the table is entirely full, retry
        // to use or make a bigger table.
        if (!found) {
            latest = atomic_load(&hs->latest);
            prev   = atomic_load(&latest->prev);
            continue;
        }

        // migration barrier, we only insert our item once we've
        // "logically" moved it
        if (v == NULL && prev != NULL) {
            assert(prev->prev == NULL);
            void *old = TYPE(raw_lookup)(hs, prev, h, val);
            if (old != NULL) {
                // the old value might've been primed, we don't want to propagate the prime bit tho
                old = (void *) (((uintptr_t) old) & ~MAP_PRIME_BIT);

                // if we lost, then we just get replaced by a separate fella (which is fine ig)
                if (atomic_compare_exchange_strong(&latest->data[i].val, &v, old)) {
                    v = old;
                }
            }
        }

        // if the old value is a prime, we've been had (we're resizing)
        if (((uintptr_t) v) & MAP_PRIME_BIT) {
            continue;
        }

        // if the existing value is:
        // * incompatible with the expected value, we don't write.
        // * equal, we don't write (speed opt, one CAS is slower than no CAS).
        if (v != val &&
            // exp is tombstone, we'll only insert if it's empty (and not a migration)
            (exp != &MAP_TOMBSTONE || (v == NULL || v == &MAP_TOMBSTONE))
        ) {
            // value writing attempt, if we lose the CAS it means someone could've written a
            // prime (thus the entry was migrated to a later table). It could also mean we lost
            // the insertion fight to another writer and in that case we'll take their value.
            if (atomic_compare_exchange_strong(&latest->data[i].val, &v, val)) {
                v = val;
            } else {
                // if we see a prime, the entry has been migrated
                // and we should write to that later table. if not,
                // we simply lost the race to update the value.
                uintptr_t v_raw = (uintptr_t) v;
                if (v_raw & MAP_PRIME_BIT) {
                    continue;
                }
            }
        }

        return v;
    }
}

void *TYPE(put)(map_t *hm, void *key, void *val) {
    MAP_BEGIN("put");

    assert(val);
    TYPE(ebr_try_init)();

    TYPE(enter_pinned)();
    map_table_t *latest = atomic_load(&hm->latest);

    // if there's earlier versions of the table we can move up entries as we go along.
    map_table_t *prev = atomic_load(&latest->prev);
    if (prev != NULL) {
        prev = TYPE(move_items)(hm, latest, prev, MAP_MOVE_AMOUNT);
        if (prev == NULL) {
            latest = atomic_load(&hm->latest);
        }
    }

    void *v = TYPE(put_if_match)(hm, latest, prev, key, val, &MAP_NO_MATCH_OLD);

    TYPE(exit_pinned)();
    MAP_END();
    return v;
}

void *TYPE(remove)(map_t *hm, void *key) {
    MAP_BEGIN("remove");

    assert(key);
    TYPE(ebr_try_init)();

    TYPE(enter_pinned)();
    map_table_t *latest = atomic_load(&hm->latest);

    // if there's earlier versions of the table we can move up entries as we go along.
    map_table_t *prev = atomic_load(&latest->prev);
    if (prev != NULL) {
        prev = TYPE(move_items)(hm, latest, prev, MAP_MOVE_AMOUNT);
        if (prev == NULL) {
            latest = atomic_load(&hm->latest);
        }
    }

    void *v = TYPE(put_if_match)(hm, latest, prev, key, &MAP_TOMBSTONE, &MAP_NO_MATCH_OLD);

    TYPE(exit_pinned)();
    MAP_END();
    return v;
}

void *TYPE(get)(map_t *hm, void *key) {
    MAP_BEGIN("get");

    assert(key);
    TYPE(ebr_try_init)();

    TYPE(enter_pinned)();
    map_table_t *latest = atomic_load(&hm->latest);

    // if there's earlier versions of the table we can move up entries as we go along.
    map_table_t *prev = atomic_load(&latest->prev);
    if (prev != NULL) {
        prev = TYPE(move_items)(hm, latest, prev, MAP_MOVE_AMOUNT);
        if (prev == NULL) {
            latest = atomic_load(&hm->latest);
        }
    }

    uint32_t cap = latest->cap;
    uint32_t h = HASH(key);
    size_t first = TYPE(hash2index)(latest, h), i = first;

    void *k, *v;
    do {
        v = atomic_load(&latest->data[i].val);
        k = atomic_load(&latest->data[i].key);

        if (k == NULL) {
            // first time seen, maybe the entry hasn't been moved yet
            if (prev != NULL) {
                v = TYPE(raw_lookup)(hm, prev, h, key);
            }
            break;
        } else if (TYPE(cmp)(k, key)) {
            return v;
        }

        // inc & wrap around
        i = (i == cap-1) ? 0 : i + 1;
    } while (i != first);

    TYPE(exit_pinned)();
    MAP_END();
    return v;
}

#if 0
void *TYPE(put_if_null)(map_t *hm, void *val) {
    MAP_BEGIN("put");

    assert(val);
    TYPE(ebr_try_init)();

    TYPE(enter_pinned)();
    map_table_t *latest = atomic_load(&hm->latest);

    // if there's earlier versions of the table we can move up entries as we go along.
    map_table_t *prev = atomic_load(&latest->prev);
    if (prev != NULL) {
        prev = TYPE(move_items)(hm, latest, prev, MAP_MOVE_AMOUNT);
        if (prev == NULL) {
            latest = atomic_load(&hm->latest);
        }
    }

    void *v = TYPE(put_if_match)(hm, latest, prev, val, val, &MAP_TOMBSTONE);

    TYPE(exit_pinned)();
    MAP_END();
    return v;
}
#endif

// waits for all items to be moved up before continuing
void TYPE(resize_barrier)(map_t *hm) {
    MAP_BEGIN("resize_barrier");
    TYPE(ebr_try_init)();

    TYPE(enter_pinned)();
    map_table_t *prev, *latest = atomic_load(&hm->latest);
    while (prev = atomic_load(&latest->prev), prev != NULL) {
        TYPE(move_items)(hm, latest, prev, prev->cap);
    }

    TYPE(exit_pinned)();
    MAP_END();
}

#undef HASH
#undef TYPE
