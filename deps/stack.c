// https://github.com/tidwall/stack
//
// Copyright 2024 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// Coroutine stack allocator

#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#ifndef _WIN32
#include <sys/mman.h>
#endif

#ifndef STACK_STATIC
#include "stack.h"
#else
#define STACK_API static
struct stack_opts {
    size_t stacksz;
    size_t defcap;
    size_t maxcap;
    size_t gapsz;
    bool useguards;
    bool nostackfreelist;
    bool nopagerelease;
    bool onlymalloc;
};
struct stack { char _[32]; };
struct stack_mgr { char _[320]; };
#endif

#ifndef STACK_API
#define STACK_API
#endif

struct stack_group {
    struct stack_group *prev;
    struct stack_group *next;
    size_t allocsz;
    size_t stacksz;
    size_t gapsz;
    size_t pagesz;
    bool guards;
    char *stack0;
    size_t cap; // max number of stacks
    size_t pos; // index of next stack to use
    size_t use; // how many are used
};

struct stack_freed {
    struct stack_freed *prev;
    struct stack_freed *next;
    struct stack_group *group;
};

struct stack_mgr0 {
    size_t pagesz;
    size_t stacksz;
    size_t defcap;
    size_t maxcap;
    size_t gapsz;
    bool useguards;
    bool nostackfreelist;
    bool nopagerelease;
    bool onlymalloc;
    struct stack_group gendcaps[2];
    struct stack_group *group_head;
    struct stack_group *group_tail;
    struct stack_freed fendcaps[2];
    struct stack_freed *free_head;
    struct stack_freed *free_tail;
};

struct stack0 {
    void *addr;
    size_t size;
    struct stack_group *group;
};

static_assert(sizeof(struct stack) >= sizeof(struct stack0), "");
static_assert(sizeof(struct stack_mgr) >= sizeof(struct stack_mgr0), "");

// Returns a size that is aligned  to a boundary.
// The boundary must be a power of 2.
static size_t stack_align_size(size_t size, size_t boundary) {
    return size < boundary ? boundary :
           size&(boundary-1) ? size+boundary-(size&(boundary-1)) : 
           size;
}

#ifndef _WIN32

// allocate memory using mmap. Used primarily for stack group memory.
static void *stack_mmap_alloc(size_t size) {
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED || addr == NULL) {
        return NULL;
    }
    return addr;
}

// free the stack group memory
static void stack_mmap_free(void *addr, size_t size) {
    if (addr) {
        munmap(addr, size);
    }
}

static struct stack_group *stack_group_new(size_t stacksz, size_t pagesz,
    size_t cap, size_t gapsz, bool useguards)
{
    bool guards;
    if (gapsz == 0) {
        guards = false;
    } else {
        gapsz = stack_align_size(gapsz, pagesz);
        guards = useguards;
    }
    // Calculate the allocation size of the group. 
    // A group allocation contains the group struct and all its stacks. 
    // Each stack is separated by an optional gap page, which can also act as
    // the guard page. There will be one more gap pages than stack to ensure
    // that each stack is sandwiched by gaps or guards, allowing for overflow
    // padding or segv detection for stacks that grow up or down.
    // For example, let's say the group has a capacity of 3. The memory layout
    // will end up look something like this
    //
    //   [ GUARD ][ Stack1 ][ GUARD ][ Stack2 ][ GUARD ][ Stack3 ][ GUARD ]
    //
    // where the entire group is a single mmap and each stack is sandwiched by
    // guard pages.
    size_t allocsz = stack_align_size(sizeof(struct stack_group), pagesz);
    allocsz += gapsz;                   // add space for prefix gap/guard
    size_t stack0 = allocsz;            // offset of first stack
    allocsz += (stacksz + gapsz) * cap; // add the remainder size
    struct stack_group *group = stack_mmap_alloc(allocsz);
    if (!group) {
        return NULL;
    }
    memset(group, 0, sizeof(struct stack_group));
    group->allocsz = allocsz;
    group->next = group;
    group->prev = group;
    group->guards = guards;
    group->gapsz = gapsz;
    group->stacksz = stacksz;
    group->pagesz = pagesz;
    group->stack0 = ((char*)group)+stack0;
    group->cap = cap;
    return group;
}

static void stack_group_free(struct stack_group *group) {
    stack_mmap_free(group, group->allocsz);
}

static void stack_group_remove(struct stack_group *group) {
    group->prev->next = group->next;
    group->next->prev = group->prev;
    group->next = NULL;
    group->prev = NULL;
}

static struct stack_group *stack_freed_remove(struct stack_freed *stack) {
    stack->prev->next = stack->next;
    stack->next->prev = stack->prev;
    stack->next = NULL;
    stack->prev = NULL;
    struct stack_group *group = stack->group;
    stack->group = NULL;
    return group;
}

// push a stack_group to the end of the manager group list.
static void stack_push_group(struct stack_mgr0 *mgr, struct stack_group *group)
{
    mgr->group_tail->prev->next = group;
    group->prev = mgr->group_tail->prev;
    group->next = mgr->group_tail;
    mgr->group_tail->prev = group;
}

static void stack_push_freed_stack(struct stack_mgr0 *mgr, 
    struct stack_freed *stack, struct stack_group *group)
{
    mgr->free_tail->prev->next = stack;
    stack->prev = mgr->free_tail->prev;
    stack->next = mgr->free_tail;
    mgr->free_tail->prev = stack;
    stack->group = group;
}
#endif

// initialize a stack manager

static void stack_mgr_init_(struct stack_mgr0 *mgr, struct stack_opts *opts) {
#ifdef _WIN32
    size_t pagesz = 4096;
#else
    size_t pagesz = (size_t)sysconf(_SC_PAGESIZE);
#endif
    size_t stacksz = opts && opts->stacksz ? opts->stacksz : 8388608;
    stacksz = stack_align_size(stacksz, pagesz);
    memset(mgr, 0, sizeof(struct stack_mgr0));
    mgr->stacksz = stacksz;
    mgr->defcap = opts && opts->defcap ? opts->defcap : 4;
    mgr->maxcap = opts && opts->maxcap ? opts->maxcap : 8192;
    mgr->gapsz = opts && opts->gapsz ? opts->gapsz : 1048576;
    mgr->useguards = opts && opts->useguards;
    mgr->nostackfreelist = opts && opts->nostackfreelist;
    mgr->nopagerelease = opts && opts->nopagerelease;
    mgr->onlymalloc = opts && opts->onlymalloc;
    mgr->pagesz = pagesz;
    mgr->group_head = &mgr->gendcaps[0];
    mgr->group_tail = &mgr->gendcaps[1];
    mgr->group_head->next = mgr->group_tail;
    mgr->group_tail->prev = mgr->group_head;
    if (!mgr->nostackfreelist) {
        mgr->free_head = &mgr->fendcaps[0];
        mgr->free_tail = &mgr->fendcaps[1];
        mgr->free_head->next = mgr->free_tail;
        mgr->free_tail->prev = mgr->free_head;
    }
#ifdef _WIN32
    mgr->onlymalloc = true;
#endif
}

STACK_API
void stack_mgr_init(struct stack_mgr *mgr, struct stack_opts *opts) {
    stack_mgr_init_((void*)mgr, opts);
}


static void stack_mgr_destroy_(struct stack_mgr0 *mgr) {
#ifndef _WIN32
    struct stack_group *group = mgr->group_head->next;
    while (group != mgr->group_tail) {
        struct stack_group *next = group->next;
        stack_group_free(group);
        group = next;
    }
#endif
    memset(mgr, 0, sizeof(struct stack_mgr0));
}

STACK_API
void stack_mgr_destroy(struct stack_mgr *mgr) {
    stack_mgr_destroy_((void*)mgr);
}

#ifndef _WIN32
static void stack_release_group(struct stack_group *group, bool nofreelist) {
    // Remove all stacks from free list, remove the group from the group list,
    // and free group.
    if (!nofreelist) {
        struct stack_freed *stack;
        for (size_t i = 0; i < group->pos; i++) {
            stack = (void*)(group->stack0 + (group->stacksz+group->gapsz) * i);
            stack_freed_remove(stack);
        }
    }
    stack_group_remove(group);
    stack_group_free(group);
}
#endif

static int stack_get_(struct stack_mgr0 *mgr, struct stack0 *stack) {
    if (mgr->onlymalloc) {
        void *addr = malloc(mgr->stacksz);
        if (!addr) {
            return -1;
        }
        stack->addr = addr;
        stack->size = mgr->stacksz;
        stack->group = 0;
        return 0;
    }
#ifndef _WIN32
    struct stack_group *group;
    if (!mgr->nostackfreelist) {
        struct stack_freed *fstack = mgr->free_tail->prev;
        if (fstack != mgr->free_head) {
            group = stack_freed_remove(fstack);
            group->use++;
            stack->addr = fstack;
            stack->size = mgr->stacksz;
            stack->group = group;
            return 0;
        }
    }
    group = mgr->group_tail->prev;
    if (group->pos == group->cap) {
        size_t cap = group->cap ? group->cap * 2 : mgr->defcap;
        if (cap > mgr->maxcap) {
            cap = mgr->maxcap;
        }
        group = stack_group_new(mgr->stacksz, mgr->pagesz, cap, mgr->gapsz, 
            mgr->useguards);
        if (!group) {
            return -1;
        }
        stack_push_group(mgr, group);
    }
    char *addr = group->stack0 + (group->stacksz+group->gapsz) * group->pos;
    if (group->guards) {
        // Add page guards to the coroutine stack.
        // A failure here usually means that there isn't enough system memory 
        // to split the a mmap'd virtual memory region into two. 
        // Linux assigns limits to how many distinct mapped regions a process
        // may have. Typically around 64K. This means that when used with
        // stack guards on Linux, you will probably be limited to about 64K
        // concurrent threads and coroutines. To increase this limit, increase
        // the value in the /proc/sys/vm/max_map_count, or disable page guards
        // by setting 'guards' option to false.
        int ret = 0;
        if (addr == group->stack0) {
            // Add the prefix guard page.
            // This separates the group struct page and the first stack.
            ret = mprotect(addr-group->gapsz, group->gapsz, PROT_NONE);
        }
        // Add the suffix guard page.
        if (ret == 0) {
            ret = mprotect(addr+group->stacksz, group->gapsz, PROT_NONE);
        }
        if (ret == -1) {
            return -1;
        }
    }
    group->pos++;
    group->use++;
    stack->addr = addr;
    stack->size = mgr->stacksz;
    stack->group = group;
#endif
    return 0;
}

STACK_API
int stack_get(struct stack_mgr *mgr, struct stack *stack) {
    return stack_get_((void*)mgr, (void*)stack);
}

static void stack_put_(struct stack_mgr0 *mgr, struct stack0 *stack) {
    if (mgr->onlymalloc) {
        free(stack->addr);
        return;
    }
#ifndef _WIN32
    void *addr = stack->addr;
    struct stack_group *group = stack->group;
    if (!mgr->nopagerelease){
        char *stack0 = addr;
        size_t stacksz = group->stacksz;
        if (!mgr->nostackfreelist) {
            // The first page does not need to be released.
            stack0 += group->pagesz;
            stacksz -= group->pagesz;
        }
        if (stacksz > 0) {
            // Re-mmap the pages that encompass the stack. The MAP_FIXED option
            // releases the pages back to the operating system. Yet the entire
            // stack will still exists in the processes virtual memory.
            mmap(stack0, stacksz, PROT_READ | PROT_WRITE, 
                MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
    }
    group->use--;
    if (!mgr->nostackfreelist) {
        // Add the stack to the stack freed linked list. 
        // This will cause the first page of the stack to being paged into
        // process memory, thus using up at least page_size of data per linked
        // stack.
        stack_push_freed_stack(mgr, addr, group);
    }
    if (group->use == 0) {
        // There are no more stacks in use for this group that belongs to the 
        // provided stack, and all other stacks have been used at least once in
        // the past.
        // The group should be fully released back to the operating system.
        stack_release_group(group, mgr->nostackfreelist);        
    }
#endif
}

STACK_API
void stack_put(struct stack_mgr *mgr, struct stack *stack) {
    stack_put_((void*)mgr, (void*)stack);
}

static size_t stack_size_(struct stack0 *stack) {
    return stack->size;
}

STACK_API
size_t stack_size(struct stack *stack) {
    return stack_size_((void*)stack);
}

static void *stack_addr_(struct stack0 *stack) {
    return stack->addr;
}

STACK_API
void *stack_addr(struct stack *stack) {
    return stack_addr_((void*)stack);
}
