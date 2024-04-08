// https://github.com/tidwall/stack
//
// Copyright 2024 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// Coroutine stack allocator

#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stdlib.h>

struct stack_opts {
    size_t stacksz;       // Stack size (default 8388608)
    size_t defcap;        // Default stack_group capacity (default 4)
    size_t maxcap;        // Max stack_group capacity (default 8192)
    size_t gapsz;         // Size of gap (guard) pages (default 1048576)
    bool useguards;       // Use mprotect'ed guard pages (default false)
    bool nostackfreelist; // Do not use a stack free list (default false)
    bool nopagerelease;   // Do not early release mmapped pages (default false)
    bool onlymalloc;      // Only use malloc. Everything but stacksz is ignored.
};

struct stack { char _[32]; };
struct stack_mgr { char _[320]; };

void stack_mgr_init(struct stack_mgr *mgr, struct stack_opts *opts);
void stack_mgr_destroy(struct stack_mgr *mgr);
int stack_get(struct stack_mgr *mgr, struct stack *stack);
void stack_put(struct stack_mgr *mgr, struct stack *stack);

// The base address of the stack.
void *stack_addr(struct stack *stack);

// Returns the size of the stack.
size_t stack_size(struct stack *stack);

#endif // STACK_H
