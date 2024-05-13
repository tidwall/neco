// https://github.com/tidwall/worker.c
//
// Copyright 2024 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// Simple background worker for C

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#ifdef WORKER_STATIC
#define WORKER_API static
#else
#define WORKER_API
#endif

#define WORKER_DEF_TIMEOUT INT64_C(1000000000) // one second
#define WORKER_DEF_MAX_THREADS 2
#define WORKER_DEF_MAX_THREAD_ENTRIES 32

struct worker_opts {
    int max_threads;           // def: 2
    int max_thread_entries;    // def: 32
    int64_t thread_timeout;    // nanoseconds, def: 1 second
    void*(*malloc)(size_t);    // def: system malloc
    void(*free)(void*);        // def: system free
};

struct worker_entry {
    void (*work)(void *udata);
    void *udata;
};

struct worker_thread {
    pthread_mutex_t mu;
    pthread_cond_t cond;
    pthread_t th;
    int64_t timeout;
    bool end;
    int pos, len;
    int nentries;
    struct worker_entry *entries;
};

struct worker {
    int nthreads;
    struct worker_thread **threads;
    void (*free)(void*);
};

WORKER_API
void worker_free(struct worker *worker) {
    if (worker) {
        if (worker->threads) {
            for (int i = 0; i < worker->nthreads; i++) {
                struct worker_thread *thread = worker->threads[i];
                if (thread) {
                    pthread_mutex_lock(&thread->mu);
                    thread->end = true;
                    pthread_t th = thread->th;
                    pthread_cond_signal(&thread->cond);
                    pthread_mutex_unlock(&thread->mu);
                    if (th) {
                        pthread_join(th, 0);
                    }
                    worker->free(thread->entries);
                    worker->free(thread);
                }
            }
            worker->free(worker->threads);
        }
        worker->free(worker);
    }
}

WORKER_API
struct worker *worker_new(struct worker_opts *opts) {
    // Load options
    int nthreads = opts ? opts->max_threads : 0;
    int nentries = opts ? opts->max_thread_entries : 0;
    int64_t timeout = opts ? opts->thread_timeout : 0;
    void*(*malloc_)(size_t) = opts ? opts->malloc : 0;
    void(*free_)(void*) = opts ? opts->free : 0;
    nthreads = nthreads <= 0 ? WORKER_DEF_MAX_THREADS : 
               nthreads > 65536 ? 65536 : nthreads;
    nentries = nentries <= 0 ? WORKER_DEF_MAX_THREAD_ENTRIES : 
               nentries > 65536 ? 65536 : nentries;
    timeout = timeout <= 0 ? WORKER_DEF_TIMEOUT : timeout;
    malloc_ = malloc_ ? malloc_ : malloc;
    free_ = free_ ? free_ : free;

    struct worker *worker = malloc_(sizeof(struct worker));
    if (!worker) {
        return NULL;
    }
    memset(worker, 0, sizeof(struct worker));
    worker->free = free_;
    worker->nthreads = nthreads;
    worker->threads = malloc_(sizeof(struct worker_thread*) * nthreads);
    if (!worker->threads) {
        worker_free(worker);
        return NULL;
    }
    memset(worker->threads, 0, sizeof(struct worker_thread*) * nthreads);
    for (int i = 0; i < worker->nthreads; i++) {
        struct worker_thread *thread = malloc_(sizeof(struct worker_thread));
        if (!thread) {
            worker_free(worker);
            return NULL;
        }
        memset(thread, 0, sizeof(struct worker_thread));
        worker->threads[i] = thread;
        thread->timeout = timeout;
        thread->nentries = nentries;
        thread->entries = malloc_(sizeof(struct worker_entry) * nentries);
        if (!thread->entries) {
            worker_free(worker);
            return NULL;
        }
        memset(thread->entries, 0, sizeof(struct worker_entry) * nentries);
        pthread_mutex_init(&thread->mu, 0);
        pthread_cond_init(&thread->cond, 0);
        thread->nentries = nentries;
    }
    return worker;
}

static void *worker_entry(void *arg) {
    // printf("thread created\n");
    struct worker_thread *thread = arg;
    pthread_mutex_lock(&thread->mu);
    while (1) {
        while (thread->len > 0) {
            struct worker_entry entry = thread->entries[thread->pos];
            thread->pos++;
            if (thread->pos == thread->nentries) {
                thread->pos = 0;
            }
            thread->len--;
            pthread_mutex_unlock(&thread->mu);
            if (entry.work) {
                entry.work(entry.udata);
            }
            pthread_mutex_lock(&thread->mu);
        }
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        pthread_cond_timedwait(&thread->cond, &thread->mu, &ts);
        if (thread->len == 0) {
            if (!thread->end) {
                pthread_detach(thread->th);
            }
            thread->th = 0;
            thread->end = false;
            break;
        }
    }
    pthread_mutex_unlock(&thread->mu);
    // printf("thread ended\n");
    return NULL;
}

/// Submit work
/// @param worker the worker
/// @param pin pin to a thread or set to -1 for Round-robin selection
/// @param work the work to perform
/// @param udata any user data
/// @return true for success or false if no worker is available. 
/// @return false for invalid arguments. Worker and work must no be null.
WORKER_API
bool worker_submit(struct worker *worker, int64_t pin, void(*work)(void *udata),
    void *udata)
{
    if (!worker || !work) {
        return false;
    }
    static __thread uint32_t worker_next_index = 0;
    if (pin < 0) {
        pin = worker_next_index;
    }
    worker_next_index++;
    struct worker_thread *thread = worker->threads[pin%worker->nthreads];
    bool submitted = false;
    pthread_mutex_lock(&thread->mu);
    if (thread->len < thread->nentries) {
        int pos = thread->pos + thread->len;
        if (pos >= thread->nentries) {
            pos -= thread->nentries;
        }
        thread->entries[pos].work = work;
        thread->entries[pos].udata = udata;
        thread->len++;
        if (!thread->th) {
            int ret = pthread_create(&thread->th, 0, worker_entry, thread);
            if (ret == -1) {
                pthread_mutex_unlock(&thread->mu);
                return false;
            }
        }
        submitted = true;
        pthread_cond_signal(&thread->cond);
    }
    pthread_mutex_unlock(&thread->mu);
    return submitted;
}
