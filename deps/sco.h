// https://github.com/tidwall/sco
//
// Copyright 2023 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#ifndef SCO_H
#define SCO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCO_MINSTACKSIZE 131072 // Recommended minimum stack size

struct sco_desc {
    void *stack;
    size_t stack_size;
    void (*entry)(void *udata);
    void (*cleanup)(void *stack, size_t stack_size, void *udata);
    void *udata;
};

// Starts a new coroutine with the provided description.
void sco_start(struct sco_desc *desc);

// Causes the calling coroutine to relinquish the CPU.
// This operation should be called from a coroutine, otherwise it does nothing.
void sco_yield(void);

// Get the identifier for the current coroutine.
// This operation should be called from a coroutine, otherwise it returns zero.
int64_t sco_id(void);

// Pause the current coroutine.
// This operation should be called from a coroutine, otherwise it does nothing.
void sco_pause(void);

// Resume a paused coroutine.
// If the id is invalid or does not belong to a paused coroutine then this
// operation does nothing.
// Calling sco_resume(0) is a special case that continues a runloop. See the
// README for an example.
void sco_resume(int64_t id);

// Returns true if there are any coroutines running, yielding, or paused.
bool sco_active(void);

// Detach a coroutine from a thread.
// This allows for moving coroutines between threads.
// The coroutine must be currently paused before it can be detached, thus this
// operation cannot be called from the coroutine belonging to the provided id.
// If the id is invalid or does not belong to a paused coroutine then this
// operation does nothing.
void sco_detach(int64_t id);

// Attach a coroutine to a thread.
// This allows for moving coroutines between threads.
// If the id is invalid or does not belong to a detached coroutine then this
// operation does nothing.
// Once attached, the coroutine will be paused.
void sco_attach(int64_t id);

// Exit a coroutine early.
// This _will not_ exit the program. Rather, it's for ending the current 
// coroutine and quickly switching to the thread's runloop before any other
// scheduled (yielded) coroutines run.
// This operation should be called from a coroutine, otherwise it does nothing.
void sco_exit(void);

// Returns the user data of the currently running coroutine.
void *sco_udata(void);

// General information and statistics
size_t sco_info_scheduled(void);
size_t sco_info_running(void);
size_t sco_info_paused(void);
size_t sco_info_detached(void);
const char *sco_info_method(void);

// Coroutine stack unwinding
struct sco_symbol {
    void *cfa;            // Canonical Frame Address
    void *ip;             // Instruction Pointer
    const char *fname;    // Pathname of shared object
    void *fbase;          // Base address of shared object
    const char *sname;    // Name of nearest symbol
    void *saddr;          // Address of nearest symbol
};

// Unwinds the stack and returns the number of symbols
int sco_unwind(bool (*func)(struct sco_symbol *sym, void *udata), void *udata);

#endif // SCO_H
