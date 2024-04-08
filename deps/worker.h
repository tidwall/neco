// https://github.com/tidwall/worker.c
//
// Copyright 2024 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// Simple background worker for C

#ifndef WORKER_H
#define WORKER_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct worker_opts {
    int max_threads;           // def: 2
    int max_thread_entries;    // def: 32
    int64_t thread_timeout;    // nanoseconds, def: 1 second
    void*(*malloc)(size_t);    // def: system malloc
    void(*free)(void*);        // def: system free
};

struct worker *worker_new(struct worker_opts *opts);
void worker_free(struct worker *worker);
bool worker_submit(struct worker *worker, int64_t pin, void(*work)(void *udata), void *udata);
bool worker_submit_read(struct worker *worker, int64_t pin, int fd, void *data, size_t count, void(*complete)(int res, void *udata), void *udata);
bool worker_submit_write(struct worker *worker, int64_t pin, int fd, const void *data, size_t count, void(*complete)(int res, void *udata), void *udata);

#endif // WORKER_H
