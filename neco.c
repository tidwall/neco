// https://github.com/tidwall/neco
//
// Copyright 2024 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
//
// Neco: Coroutine library for C

/*
////////////////////////////////////////////////////////////////////////////////
// Compilation options
////////////////////////////////////////////////////////////////////////////////

NECO_STACKSIZE       // Size of each stack
NECO_DEFCAP          // Default stack_group capacity
NECO_MAXCAP          // Max stack_group capacity
NECO_GAPSIZE         // Size of gap (guard) pages
NECO_SIGSTKSZ        // Size of signal stack on main thread
NECO_BURST           // Number of read attempts before waiting, def: disabled
NECO_MAXWORKERS      // Max number of worker threads, def: 64
NECO_MAXIOWORKERS    // Max number of io threads, def: 2

// Additional options that activate features

NECO_USEGUARDS        // Use mprotect'ed guard pages
NECO_NOPAGERELEASE    // Do not early release mmapped pages
NECO_NOSTACKFREELIST  // Do not use a stack free list
NECO_NOPOOL           // Do not use a coroutine and channel pools
NECO_USEHEAPSTACK     // Allocate stacks on the heap using malloc
NECO_NOSIGNALS        // Disable signal handling
NECO_NOWORKERS        // Disable all worker threads, work will run in coroutine
NECO_USEREADWORKERS   // Use read workers, disabled by default
NECO_USEWRITEWORKERS  // Use write workers, enabled by default on Linux
NECO_NOREADWORKERS    // Disable all read workers
NECO_NOWRITEWORKERS   // Disable all write workers
*/

// Windows and Webassembly have limited features.
#if defined(__EMSCRIPTEN__) || defined(__WIN32)
#define DEF_STACKSIZE     1048576
#define DEF_DEFCAP        0
#define DEF_MAXCAP        0
#define DEF_GAPSIZE       0
#define DEF_SIGSTKSZ      0
#define DEF_BURST        -1
#define NECO_USEHEAPSTACK
#define NECO_NOSIGNALS
#define NECO_NOWORKERS
#else
#define DEF_STACKSIZE     8388608
#define DEF_DEFCAP        4
#define DEF_MAXCAP        8192
#define DEF_GAPSIZE       1048576
#define DEF_SIGSTKSZ      1048576
#define DEF_BURST        -1
#define DEF_MAXWORKERS    64
#define DEF_MAXRINGSIZE   32
#define DEF_MAXIOWORKERS  2
#endif

#ifdef __linux__
#ifndef NECO_USEWRITEWORKERS
#define NECO_USEWRITEWORKERS
#endif
#endif

#ifndef NECO_STACKSIZE
#define NECO_STACKSIZE DEF_STACKSIZE
#endif
#ifndef NECO_DEFCAP
#define NECO_DEFCAP DEF_DEFCAP
#endif
#ifndef NECO_MAXCAP
#define NECO_MAXCAP DEF_MAXCAP
#endif
#ifndef NECO_GAPSIZE
#define NECO_GAPSIZE DEF_GAPSIZE
#endif
#ifndef NECO_SIGSTKSZ
#define NECO_SIGSTKSZ DEF_SIGSTKSZ
#endif
#ifndef NECO_BURST
#define NECO_BURST DEF_BURST
#endif
#ifndef NECO_MAXWORKERS
#define NECO_MAXWORKERS DEF_MAXWORKERS
#endif
#ifndef NECO_MAXRINGSIZE
#define NECO_MAXRINGSIZE DEF_MAXRINGSIZE
#endif
#ifndef NECO_MAXIOWORKERS
#define NECO_MAXIOWORKERS DEF_MAXIOWORKERS
#endif

#ifdef NECO_TESTING
#if NECO_BURST <= 0
#undef NECO_BURST
#define NECO_BURST 1
#endif
#if NECO_MAXWORKERS > 8
#undef NECO_MAXWORKERS
#define NECO_MAXWORKERS 8
#endif
#if NECO_MAXRINGSIZE > 4
#undef NECO_MAXRINGSIZE
#define NECO_MAXRINGSIZE 4
#endif
#endif

// The following is only needed when LLCO_NOASM or LLCO_STACKJMP is defined.
// This same block is duplicated in the llco.c block below.
#ifdef _FORTIFY_SOURCE
#define LLCO_FORTIFY_SOURCE _FORTIFY_SOURCE
// Disable __longjmp_chk validation so that we can jump between stacks.
#pragma push_macro("_FORTIFY_SOURCE")
#undef _FORTIFY_SOURCE
#include <setjmp.h>
#define _FORTIFY_SOURCE LLCO_FORTIFY_SOURCE
#undef LLCO_FORTIFY_SOURCE
#pragma pop_macro("_FORTIFY_SOURCE")
#endif

#ifdef _WIN32
#define _POSIX
#define __USE_MINGW_ALARM
#endif

////////////////////////////////////////////////////////////////////////////////
// Embedded dependencies
// The deps/embed.sh command embeds all dependencies into this source file to
// create a single amalgamation file.
////////////////////////////////////////////////////////////////////////////////
#ifdef NECO_NOAMALGA

#include "deps/sco.h"
#include "deps/aat.h"
#include "deps/stack.h"

#ifndef NECO_NOWORKERS
#include "deps/worker.h"
#endif

#else

#define SCO_STATIC
#define STACK_STATIC
#define WORKER_STATIC

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

// BEGIN sco.c
// https://github.com/tidwall/sco
//
// Copyright 2023 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// Coroutine scheduler

#include <stdatomic.h>
#include <stdbool.h>

#ifndef SCO_STATIC
#include "sco.h"
#else
#define SCO_EXTERN static
#include <stddef.h>
#include <stdint.h>
struct sco_desc {
    void *stack;
    size_t stack_size;
    void (*entry)(void *udata);
    void (*cleanup)(void *stack, size_t stack_size, void *udata);
    void *udata;
};
struct sco_symbol {
    void *cfa;            // Canonical Frame Address
    void *ip;             // Instruction Pointer
    const char *fname;    // Pathname of shared object
    void *fbase;          // Base address of shared object
    const char *sname;    // Name of nearest symbol
    void *saddr;          // Address of nearest symbol
};
#define SCO_MINSTACKSIZE 131072
#endif

#ifndef SCO_EXTERN
#define SCO_EXTERN
#endif

////////////////////////////////////////////////////////////////////////////////
// llco.c
////////////////////////////////////////////////////////////////////////////////
#ifdef SCO_NOAMALGA

#include "deps/llco.h"

#else

#define LLCO_STATIC

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

// BEGIN llco.c
// https://github.com/tidwall/llco
//
// Copyright (c) 2024 Joshua J Baker.
// This software is available as a choice of Public Domain or MIT-0.

#ifdef _FORTIFY_SOURCE
#define LLCO_FORTIFY_SOURCE _FORTIFY_SOURCE
// Disable __longjmp_chk validation so that we can jump between stacks.
#pragma push_macro("_FORTIFY_SOURCE")
#undef _FORTIFY_SOURCE
#include <setjmp.h>
#define _FORTIFY_SOURCE LLCO_FORTIFY_SOURCE
#undef LLCO_FORTIFY_SOURCE
#pragma pop_macro("_FORTIFY_SOURCE")
#endif

#ifndef LLCO_STATIC
#include "llco.h"
#else
#include <stddef.h>
#include <stdbool.h>
#define LLCO_MINSTACKSIZE 16384
#define LLCO_EXTERN static
struct llco_desc {
    void *stack;
    size_t stack_size;
    void (*entry)(void *udata);
    void (*cleanup)(void *stack, size_t stack_size, void *udata);
    void *udata;
};
struct llco_symbol {
    void *cfa;
    void *ip;
    const char *fname;
    void *fbase;
    const char *sname;
    void *saddr;
};
#endif

#include <stdlib.h>

#ifdef LLCO_VALGRIND
#include <valgrind/valgrind.h>
#endif

#ifndef LLCO_EXTERN
#define LLCO_EXTERN
#endif

#if defined(__GNUC__)
#ifdef noinline
#define LLCO_NOINLINE noinline
#else
#define LLCO_NOINLINE __attribute__ ((noinline))
#endif
#ifdef noreturn
#define LLCO_NORETURN noreturn
#else
#define LLCO_NORETURN __attribute__ ((noreturn))
#endif
#else
#define LLCO_NOINLINE
#define LLCO_NORETURN
#endif

#if defined(_MSC_VER)
#define __thread __declspec(thread)
#endif

static void llco_entry(void *arg);

LLCO_NORETURN
static void llco_exit(void) {
    _Exit(0);
}

#ifdef LLCO_ASM
#error LLCO_ASM must not be defined
#endif

// Passing the entry function into assembly requires casting the function 
// pointer to an object pointer, which is forbidden in the ISO C spec but
// allowed in posix. Ignore the warning attributed to this  requirement when
// the -pedantic compiler flag is provide.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

////////////////////////////////////////////////////////////////////////////////
// Below is various assembly code adapted from the Lua Coco [MIT] and Minicoro
// [MIT-0] projects by Mike Pall and Eduardo Bart respectively.
////////////////////////////////////////////////////////////////////////////////

/*
Lua Coco (coco.luajit.org) 
Copyright (C) 2004-2016 Mike Pall. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

////////////////////////////////////////////////////////////////////////////////
// ARM
////////////////////////////////////////////////////////////////////////////////
#if defined(__ARM_EABI__) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,arm_eabi"

struct llco_asmctx {
#ifndef __SOFTFP__
    void* f[16];
#endif
    void *d[4]; /* d8-d15 */
    void *r[4]; /* r4-r11 */
    void *lr;
    void *sp;
};

void _llco_asm_entry(void);
int _llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to);

__asm__(
    ".text\n"
#ifdef __APPLE__
    ".globl __llco_asm_switch\n"
    "__llco_asm_switch:\n"
#else
    ".globl _llco_asm_switch\n"
    ".type _llco_asm_switch #function\n"
    ".hidden _llco_asm_switch\n"
    "_llco_asm_switch:\n"
#endif
#ifndef __SOFTFP__
    "  vstmia r0!, {d8-d15}\n"
#endif
    "  stmia r0, {r4-r11, lr}\n"
    "  str sp, [r0, #9*4]\n"
#ifndef __SOFTFP__
    "  vldmia r1!, {d8-d15}\n"
#endif
    "  ldr sp, [r1, #9*4]\n"
    "  ldmia r1, {r4-r11, pc}\n"
#ifndef __APPLE__
    ".size _llco_asm_switch, .-_llco_asm_switch\n"
#endif
);

__asm__(
    ".text\n"
#ifdef __APPLE__
    ".globl __llco_asm_entry\n"
    "__llco_asm_entry:\n"
#else
    ".globl _llco_asm_entry\n"
    ".type _llco_asm_entry #function\n"
    ".hidden _llco_asm_entry\n"
    "_llco_asm_entry:\n"
#endif
    "  mov r0, r4\n"
    "  mov ip, r5\n"
    "  mov lr, r6\n"
    "  bx ip\n"
#ifndef __APPLE__
    ".size _llco_asm_entry, .-_llco_asm_entry\n"
#endif
);

static void llco_asmctx_make(struct llco_asmctx *ctx, void* stack_base, 
    size_t stack_size, void *arg)
{
    ctx->d[0] = (void*)(arg);
    ctx->d[1] = (void*)(llco_entry);
    ctx->d[2] = (void*)(0xdeaddead); /* Dummy return address. */
    ctx->lr = (void*)(_llco_asm_entry);
    ctx->sp = (void*)((size_t)stack_base + stack_size);
}

#endif

////////////////////////////////////////////////////////////////////////////////
// ARM 64-bit
////////////////////////////////////////////////////////////////////////////////
#if defined(__aarch64__) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,aarch64"

struct llco_asmctx {
    void *x[12]; /* x19-x30 */
    void *sp;
    void *lr;
    void *d[8]; /* d8-d15 */
};

void _llco_asm_entry(void);
int _llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to);

__asm__(
    ".text\n"
#ifdef __APPLE__
    ".globl __llco_asm_switch\n"
    "__llco_asm_switch:\n"
#else
    ".globl _llco_asm_switch\n"
    ".type _llco_asm_switch #function\n"
    ".hidden _llco_asm_switch\n"
    "_llco_asm_switch:\n"
#endif

    "  mov x10, sp\n"
    "  mov x11, x30\n"
    "  stp x19, x20, [x0, #(0*16)]\n"
    "  stp x21, x22, [x0, #(1*16)]\n"
    "  stp d8, d9, [x0, #(7*16)]\n"
    "  stp x23, x24, [x0, #(2*16)]\n"
    "  stp d10, d11, [x0, #(8*16)]\n"
    "  stp x25, x26, [x0, #(3*16)]\n"
    "  stp d12, d13, [x0, #(9*16)]\n"
    "  stp x27, x28, [x0, #(4*16)]\n"
    "  stp d14, d15, [x0, #(10*16)]\n"
    "  stp x29, x30, [x0, #(5*16)]\n"
    "  stp x10, x11, [x0, #(6*16)]\n"
    "  ldp x19, x20, [x1, #(0*16)]\n"
    "  ldp x21, x22, [x1, #(1*16)]\n"
    "  ldp d8, d9, [x1, #(7*16)]\n"
    "  ldp x23, x24, [x1, #(2*16)]\n"
    "  ldp d10, d11, [x1, #(8*16)]\n"
    "  ldp x25, x26, [x1, #(3*16)]\n"
    "  ldp d12, d13, [x1, #(9*16)]\n"
    "  ldp x27, x28, [x1, #(4*16)]\n"
    "  ldp d14, d15, [x1, #(10*16)]\n"
    "  ldp x29, x30, [x1, #(5*16)]\n"
    "  ldp x10, x11, [x1, #(6*16)]\n"
    "  mov sp, x10\n"
    "  br x11\n"
#ifndef __APPLE__
    ".size _llco_asm_switch, .-_llco_asm_switch\n"
#endif
);

__asm__(
    ".text\n"
#ifdef __APPLE__
    ".globl __llco_asm_entry\n"
    "__llco_asm_entry:\n"
#else
    ".globl _llco_asm_entry\n"
    ".type _llco_asm_entry #function\n"
    ".hidden _llco_asm_entry\n"
    "_llco_asm_entry:\n"
#endif
    "  mov x0, x19\n"
    "  mov x30, x21\n"
    "  br x20\n"
#ifndef __APPLE__
    ".size _llco_asm_entry, .-_llco_asm_entry\n"
#endif
);

static void llco_asmctx_make(struct llco_asmctx *ctx, void* stack_base,
  size_t stack_size, void *arg)
{
    ctx->x[0] = (void*)(arg);
    ctx->x[1] = (void*)(llco_entry);
    ctx->x[2] = (void*)(0xdeaddeaddeaddead); /* Dummy return address. */
    ctx->sp = (void*)((size_t)stack_base + stack_size);
    ctx->lr = (void*)(_llco_asm_entry);
}
#endif 

////////////////////////////////////////////////////////////////////////////////
// RISC-V (rv64/rv32)
////////////////////////////////////////////////////////////////////////////////
#if defined(__riscv) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,riscv"

struct llco_asmctx {
    void* s[12]; /* s0-s11 */
    void* ra;
    void* pc;
    void* sp;
#ifdef __riscv_flen
#if __riscv_flen == 64
    double fs[12]; /* fs0-fs11 */
#elif __riscv_flen == 32
    float fs[12]; /* fs0-fs11 */
#endif
#endif /* __riscv_flen */
};

void _llco_asm_entry(void);
int _llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to);

__asm__(
    ".text\n"
    ".globl _llco_asm_entry\n"
    ".type _llco_asm_entry @function\n"
    ".hidden _llco_asm_entry\n"
    "_llco_asm_entry:\n"
    "  mv a0, s0\n"
    "  jr s1\n"
    ".size _llco_asm_entry, .-_llco_asm_entry\n"
);

__asm__(
    ".text\n"
    ".globl _llco_asm_switch\n"
    ".type _llco_asm_switch @function\n"
    ".hidden _llco_asm_switch\n"
    "_llco_asm_switch:\n"
#if __riscv_xlen == 64
    "  sd s0, 0x00(a0)\n"
    "  sd s1, 0x08(a0)\n"
    "  sd s2, 0x10(a0)\n"
    "  sd s3, 0x18(a0)\n"
    "  sd s4, 0x20(a0)\n"
    "  sd s5, 0x28(a0)\n"
    "  sd s6, 0x30(a0)\n"
    "  sd s7, 0x38(a0)\n"
    "  sd s8, 0x40(a0)\n"
    "  sd s9, 0x48(a0)\n"
    "  sd s10, 0x50(a0)\n"
    "  sd s11, 0x58(a0)\n"
    "  sd ra, 0x60(a0)\n"
    "  sd ra, 0x68(a0)\n" /* pc */
    "  sd sp, 0x70(a0)\n"
#ifdef __riscv_flen
#if __riscv_flen == 64
    "  fsd fs0, 0x78(a0)\n"
    "  fsd fs1, 0x80(a0)\n"
    "  fsd fs2, 0x88(a0)\n"
    "  fsd fs3, 0x90(a0)\n"
    "  fsd fs4, 0x98(a0)\n"
    "  fsd fs5, 0xa0(a0)\n"
    "  fsd fs6, 0xa8(a0)\n"
    "  fsd fs7, 0xb0(a0)\n"
    "  fsd fs8, 0xb8(a0)\n"
    "  fsd fs9, 0xc0(a0)\n"
    "  fsd fs10, 0xc8(a0)\n"
    "  fsd fs11, 0xd0(a0)\n"
    "  fld fs0, 0x78(a1)\n"
    "  fld fs1, 0x80(a1)\n"
    "  fld fs2, 0x88(a1)\n"
    "  fld fs3, 0x90(a1)\n"
    "  fld fs4, 0x98(a1)\n"
    "  fld fs5, 0xa0(a1)\n"
    "  fld fs6, 0xa8(a1)\n"
    "  fld fs7, 0xb0(a1)\n"
    "  fld fs8, 0xb8(a1)\n"
    "  fld fs9, 0xc0(a1)\n"
    "  fld fs10, 0xc8(a1)\n"
    "  fld fs11, 0xd0(a1)\n"
#else
#error "Unsupported RISC-V FLEN"
#endif
#endif /* __riscv_flen */
    "  ld s0, 0x00(a1)\n"
    "  ld s1, 0x08(a1)\n"
    "  ld s2, 0x10(a1)\n"
    "  ld s3, 0x18(a1)\n"
    "  ld s4, 0x20(a1)\n"
    "  ld s5, 0x28(a1)\n"
    "  ld s6, 0x30(a1)\n"
    "  ld s7, 0x38(a1)\n"
    "  ld s8, 0x40(a1)\n"
    "  ld s9, 0x48(a1)\n"
    "  ld s10, 0x50(a1)\n"
    "  ld s11, 0x58(a1)\n"
    "  ld ra, 0x60(a1)\n"
    "  ld a2, 0x68(a1)\n" /* pc */
    "  ld sp, 0x70(a1)\n"
    "  jr a2\n"
#elif __riscv_xlen == 32
    "  sw s0, 0x00(a0)\n"
    "  sw s1, 0x04(a0)\n"
    "  sw s2, 0x08(a0)\n"
    "  sw s3, 0x0c(a0)\n"
    "  sw s4, 0x10(a0)\n"
    "  sw s5, 0x14(a0)\n"
    "  sw s6, 0x18(a0)\n"
    "  sw s7, 0x1c(a0)\n"
    "  sw s8, 0x20(a0)\n"
    "  sw s9, 0x24(a0)\n"
    "  sw s10, 0x28(a0)\n"
    "  sw s11, 0x2c(a0)\n"
    "  sw ra, 0x30(a0)\n"
    "  sw ra, 0x34(a0)\n" /* pc */
    "  sw sp, 0x38(a0)\n"
#ifdef __riscv_flen
#if __riscv_flen == 64
    "  fsd fs0, 0x3c(a0)\n"
    "  fsd fs1, 0x44(a0)\n"
    "  fsd fs2, 0x4c(a0)\n"
    "  fsd fs3, 0x54(a0)\n"
    "  fsd fs4, 0x5c(a0)\n"
    "  fsd fs5, 0x64(a0)\n"
    "  fsd fs6, 0x6c(a0)\n"
    "  fsd fs7, 0x74(a0)\n"
    "  fsd fs8, 0x7c(a0)\n"
    "  fsd fs9, 0x84(a0)\n"
    "  fsd fs10, 0x8c(a0)\n"
    "  fsd fs11, 0x94(a0)\n"
    "  fld fs0, 0x3c(a1)\n"
    "  fld fs1, 0x44(a1)\n"
    "  fld fs2, 0x4c(a1)\n"
    "  fld fs3, 0x54(a1)\n"
    "  fld fs4, 0x5c(a1)\n"
    "  fld fs5, 0x64(a1)\n"
    "  fld fs6, 0x6c(a1)\n"
    "  fld fs7, 0x74(a1)\n"
    "  fld fs8, 0x7c(a1)\n"
    "  fld fs9, 0x84(a1)\n"
    "  fld fs10, 0x8c(a1)\n"
    "  fld fs11, 0x94(a1)\n"
#elif __riscv_flen == 32
    "  fsw fs0, 0x3c(a0)\n"
    "  fsw fs1, 0x40(a0)\n"
    "  fsw fs2, 0x44(a0)\n"
    "  fsw fs3, 0x48(a0)\n"
    "  fsw fs4, 0x4c(a0)\n"
    "  fsw fs5, 0x50(a0)\n"
    "  fsw fs6, 0x54(a0)\n"
    "  fsw fs7, 0x58(a0)\n"
    "  fsw fs8, 0x5c(a0)\n"
    "  fsw fs9, 0x60(a0)\n"
    "  fsw fs10, 0x64(a0)\n"
    "  fsw fs11, 0x68(a0)\n"
    "  flw fs0, 0x3c(a1)\n"
    "  flw fs1, 0x40(a1)\n"
    "  flw fs2, 0x44(a1)\n"
    "  flw fs3, 0x48(a1)\n"
    "  flw fs4, 0x4c(a1)\n"
    "  flw fs5, 0x50(a1)\n"
    "  flw fs6, 0x54(a1)\n"
    "  flw fs7, 0x58(a1)\n"
    "  flw fs8, 0x5c(a1)\n"
    "  flw fs9, 0x60(a1)\n"
    "  flw fs10, 0x64(a1)\n"
    "  flw fs11, 0x68(a1)\n"
#else
#error "Unsupported RISC-V FLEN"
#endif
#endif /* __riscv_flen */
    "  lw s0, 0x00(a1)\n"
    "  lw s1, 0x04(a1)\n"
    "  lw s2, 0x08(a1)\n"
    "  lw s3, 0x0c(a1)\n"
    "  lw s4, 0x10(a1)\n"
    "  lw s5, 0x14(a1)\n"
    "  lw s6, 0x18(a1)\n"
    "  lw s7, 0x1c(a1)\n"
    "  lw s8, 0x20(a1)\n"
    "  lw s9, 0x24(a1)\n"
    "  lw s10, 0x28(a1)\n"
    "  lw s11, 0x2c(a1)\n"
    "  lw ra, 0x30(a1)\n"
    "  lw a2, 0x34(a1)\n" /* pc */
    "  lw sp, 0x38(a1)\n"
    "  jr a2\n"
#else
#error "Unsupported RISC-V XLEN"
#endif /* __riscv_xlen */
  ".size _llco_asm_switch, .-_llco_asm_switch\n"
);

static void llco_asmctx_make(struct llco_asmctx *ctx, 
    void* stack_base, size_t stack_size, void *arg)
{
    ctx->s[0] = (void*)(arg);
    ctx->s[1] = (void*)(llco_entry);
    ctx->pc = (void*)(_llco_asm_entry);
#if __riscv_xlen == 64
    ctx->ra = (void*)(0xdeaddeaddeaddead);
#elif __riscv_xlen == 32
    ctx->ra = (void*)(0xdeaddead);
#endif
    ctx->sp = (void*)((size_t)stack_base + stack_size);
}

#endif // riscv

////////////////////////////////////////////////////////////////////////////////
// x86
////////////////////////////////////////////////////////////////////////////////
#if (defined(__i386) || defined(__i386__)) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,i386"

struct llco_asmctx {
    void *eip, *esp, *ebp, *ebx, *esi, *edi;
};

void _llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to);

__asm__(
#ifdef __DJGPP__ /* DOS compiler */
    "__llco_asm_switch:\n"
#else
    ".text\n"
    ".globl _llco_asm_switch\n"
    ".type _llco_asm_switch @function\n"
    ".hidden _llco_asm_switch\n"
    "_llco_asm_switch:\n"
#endif
    "  call 1f\n"
    "  1:\n"
    "  popl %ecx\n"
    "  addl $(2f-1b), %ecx\n"
    "  movl 4(%esp), %eax\n"
    "  movl 8(%esp), %edx\n"
    "  movl %ecx, (%eax)\n"
    "  movl %esp, 4(%eax)\n"
    "  movl %ebp, 8(%eax)\n"
    "  movl %ebx, 12(%eax)\n"
    "  movl %esi, 16(%eax)\n"
    "  movl %edi, 20(%eax)\n"
    "  movl 20(%edx), %edi\n"
    "  movl 16(%edx), %esi\n"
    "  movl 12(%edx), %ebx\n"
    "  movl 8(%edx), %ebp\n"
    "  movl 4(%edx), %esp\n"
    "  jmp *(%edx)\n"
    "  2:\n"
    "  ret\n"
#ifndef __DJGPP__
    ".size _llco_asm_switch, .-_llco_asm_switch\n"
#endif
);

static void llco_asmctx_make(struct llco_asmctx *ctx,
    void* stack_base, size_t stack_size, void *arg)
{
    void** stack_high_ptr = (void**)((size_t)stack_base + stack_size - 16 - 
        1*sizeof(size_t));
    stack_high_ptr[0] = (void*)(0xdeaddead);  // Dummy return address.
    stack_high_ptr[1] = (void*)(arg);
    ctx->eip = (void*)(llco_entry);
    ctx->esp = (void*)(stack_high_ptr);
}
#endif // __i386__

////////////////////////////////////////////////////////////////////////////////
// x64
////////////////////////////////////////////////////////////////////////////////
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,x64"

#ifdef _WIN32

struct llco_asmctx {
    void *rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15, *rdi, *rsi;
    void* xmm[20]; /* xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, 
        xmm14, xmm15 */
    void* fiber_storage;
    void* dealloc_stack;
    void* stack_limit;
    void* stack_base;
};

#if defined(__GNUC__)
#define LLCO_ASM_BLOB __attribute__((section(".text")))
#elif defined(_MSC_VER)
#define LLCO_ASM_BLOB __declspec(allocate(".text"))
#pragma section(".text")
#endif

LLCO_ASM_BLOB static unsigned char llco_wrap_main_code_entry[] = {
    0x4c,0x89,0xe9,                               // mov    %r13,%rcx
    0x41,0xff,0xe4,                               // jmpq   *%r12
    0xc3,                                         // retq
    0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90  // nop
};

LLCO_ASM_BLOB static unsigned char llco_asm_switch_code[] = {
    0x48,0x8d,0x05,0x3e,0x01,0x00,0x00,           // lea    0x13e(%rip),%rax
    0x48,0x89,0x01,                               // mov    %rax,(%rcx)
    0x48,0x89,0x61,0x08,                          // mov    %rsp,0x8(%rcx)
    0x48,0x89,0x69,0x10,                          // mov    %rbp,0x10(%rcx)
    0x48,0x89,0x59,0x18,                          // mov    %rbx,0x18(%rcx)
    0x4c,0x89,0x61,0x20,                          // mov    %r12,0x20(%rcx)
    0x4c,0x89,0x69,0x28,                          // mov    %r13,0x28(%rcx)
    0x4c,0x89,0x71,0x30,                          // mov    %r14,0x30(%rcx)
    0x4c,0x89,0x79,0x38,                          // mov    %r15,0x38(%rcx)
    0x48,0x89,0x79,0x40,                          // mov    %rdi,0x40(%rcx)
    0x48,0x89,0x71,0x48,                          // mov    %rsi,0x48(%rcx)
    0x0f,0x11,0x71,0x50,                          // movups %xmm6,0x50(%rcx)
    0x0f,0x11,0x79,0x60,                          // movups %xmm7,0x60(%rcx)
    0x44,0x0f,0x11,0x41,0x70,                     // movups %xmm8,0x70(%rcx)
    0x44,0x0f,0x11,0x89,0x80,0x00,0x00,0x00,      // movups %xmm9,0x80(%rcx)
    0x44,0x0f,0x11,0x91,0x90,0x00,0x00,0x00,      // movups %xmm10,0x90(%rcx)
    0x44,0x0f,0x11,0x99,0xa0,0x00,0x00,0x00,      // movups %xmm11,0xa0(%rcx)
    0x44,0x0f,0x11,0xa1,0xb0,0x00,0x00,0x00,      // movups %xmm12,0xb0(%rcx)
    0x44,0x0f,0x11,0xa9,0xc0,0x00,0x00,0x00,      // movups %xmm13,0xc0(%rcx)
    0x44,0x0f,0x11,0xb1,0xd0,0x00,0x00,0x00,      // movups %xmm14,0xd0(%rcx)
    0x44,0x0f,0x11,0xb9,0xe0,0x00,0x00,0x00,      // movups %xmm15,0xe0(%rcx)
    0x65,0x4c,0x8b,0x14,0x25,0x30,0x00,0x00,0x00, // mov    %gs:0x30,%r10
    0x49,0x8b,0x42,0x20,                          // mov    0x20(%r10),%rax
    0x48,0x89,0x81,0xf0,0x00,0x00,0x00,           // mov    %rax,0xf0(%rcx)
    0x49,0x8b,0x82,0x78,0x14,0x00,0x00,           // mov    0x1478(%r10),%rax
    0x48,0x89,0x81,0xf8,0x00,0x00,0x00,           // mov    %rax,0xf8(%rcx)
    0x49,0x8b,0x42,0x10,                          // mov    0x10(%r10),%rax
    0x48,0x89,0x81,0x00,0x01,0x00,0x00,           // mov    %rax,0x100(%rcx)
    0x49,0x8b,0x42,0x08,                          // mov    0x8(%r10),%rax
    0x48,0x89,0x81,0x08,0x01,0x00,0x00,           // mov    %rax,0x108(%rcx)
    0x48,0x8b,0x82,0x08,0x01,0x00,0x00,           // mov    0x108(%rdx),%rax
    0x49,0x89,0x42,0x08,                          // mov    %rax,0x8(%r10)
    0x48,0x8b,0x82,0x00,0x01, 0x00, 0x00,         // mov    0x100(%rdx),%rax
    0x49,0x89,0x42,0x10,                          // mov    %rax,0x10(%r10)
    0x48,0x8b,0x82,0xf8,0x00, 0x00, 0x00,         // mov    0xf8(%rdx),%rax
    0x49,0x89,0x82,0x78,0x14, 0x00, 0x00,         // mov    %rax,0x1478(%r10)
    0x48,0x8b,0x82,0xf0,0x00, 0x00, 0x00,         // mov    0xf0(%rdx),%rax
    0x49,0x89,0x42,0x20,                          // mov    %rax,0x20(%r10)
    0x44,0x0f,0x10,0xba,0xe0,0x00,0x00,0x00,      // movups 0xe0(%rdx),%xmm15
    0x44,0x0f,0x10,0xb2,0xd0,0x00,0x00,0x00,      // movups 0xd0(%rdx),%xmm14
    0x44,0x0f,0x10,0xaa,0xc0,0x00,0x00,0x00,      // movups 0xc0(%rdx),%xmm13
    0x44,0x0f,0x10,0xa2,0xb0,0x00,0x00,0x00,      // movups 0xb0(%rdx),%xmm12
    0x44,0x0f,0x10,0x9a,0xa0,0x00,0x00,0x00,      // movups 0xa0(%rdx),%xmm11
    0x44,0x0f,0x10,0x92,0x90,0x00,0x00,0x00,      // movups 0x90(%rdx),%xmm10
    0x44,0x0f,0x10,0x8a,0x80,0x00,0x00,0x00,      // movups 0x80(%rdx),%xmm9
    0x44,0x0f,0x10,0x42,0x70,                     // movups 0x70(%rdx),%xmm8
    0x0f,0x10,0x7a,0x60,                          // movups 0x60(%rdx),%xmm7
    0x0f,0x10,0x72,0x50,                          // movups 0x50(%rdx),%xmm6
    0x48,0x8b,0x72,0x48,                          // mov    0x48(%rdx),%rsi
    0x48,0x8b,0x7a,0x40,                          // mov    0x40(%rdx),%rdi
    0x4c,0x8b,0x7a,0x38,                          // mov    0x38(%rdx),%r15
    0x4c,0x8b,0x72,0x30,                          // mov    0x30(%rdx),%r14
    0x4c,0x8b,0x6a,0x28,                          // mov    0x28(%rdx),%r13
    0x4c,0x8b,0x62,0x20,                          // mov    0x20(%rdx),%r12
    0x48,0x8b,0x5a,0x18,                          // mov    0x18(%rdx),%rbx
    0x48,0x8b,0x6a,0x10,                          // mov    0x10(%rdx),%rbp
    0x48,0x8b,0x62,0x08,                          // mov    0x8(%rdx),%rsp
    0xff,0x22,                                    // jmpq   *(%rdx)
    0xc3,                                         // retq
    0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,      // nop
    0x90,0x90,                                    // nop
};

void (*_llco_asm_entry)(void) = 
    (void(*)(void))(void*)llco_wrap_main_code_entry;
void (*_llco_asm_switch)(struct llco_asmctx *from, 
    struct llco_asmctx *to) = (void(*)(struct llco_asmctx *from, 
    struct llco_asmctx *to))(void*)llco_asm_switch_code;

static void llco_asmctx_make(struct llco_asmctx *ctx, 
    void* stack_base, size_t stack_size, void *arg)
{
    stack_size = stack_size - 32; // Reserve 32 bytes for the shadow space.
    void** stack_high_ptr = (void**)((size_t)stack_base + stack_size - 
        sizeof(size_t));
    stack_high_ptr[0] = (void*)(0xdeaddeaddeaddead);  // Dummy return address.
    ctx->rip = (void*)(_llco_asm_entry);
    ctx->rsp = (void*)(stack_high_ptr);
    ctx->r12 = (void*)(llco_entry);
    ctx->r13 = (void*)(arg);
    void* stack_top = (void*)((size_t)stack_base + stack_size);
    ctx->stack_base = stack_top;
    ctx->stack_limit = stack_base;
    ctx->dealloc_stack = stack_base;
}

#else

struct llco_asmctx {
    void *rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
};

void _llco_asm_entry(void);
int _llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to);

__asm__(
    ".text\n"
#ifdef __MACH__ /* Mac OS X assembler */
    ".globl __llco_asm_entry\n"
    "__llco_asm_entry:\n"
#else /* Linux assembler */
    ".globl _llco_asm_entry\n"
    ".type _llco_asm_entry @function\n"
    ".hidden _llco_asm_entry\n"
    "_llco_asm_entry:\n"
#endif
    "  movq %r13, %rdi\n"
    "  jmpq *%r12\n"
#ifndef __MACH__
    ".size _llco_asm_entry, .-_llco_asm_entry\n"
#endif
);

__asm__(
    ".text\n"
#ifdef __MACH__ /* Mac OS assembler */
    ".globl __llco_asm_switch\n"
    "__llco_asm_switch:\n"
#else /* Linux assembler */
    ".globl _llco_asm_switch\n"
    ".type _llco_asm_switch @function\n"
    ".hidden _llco_asm_switch\n"
    "_llco_asm_switch:\n"
#endif
    "  leaq 0x3d(%rip), %rax\n"
    "  movq %rax, (%rdi)\n"
    "  movq %rsp, 8(%rdi)\n"
    "  movq %rbp, 16(%rdi)\n"
    "  movq %rbx, 24(%rdi)\n"
    "  movq %r12, 32(%rdi)\n"
    "  movq %r13, 40(%rdi)\n"
    "  movq %r14, 48(%rdi)\n"
    "  movq %r15, 56(%rdi)\n"
    "  movq 56(%rsi), %r15\n"
    "  movq 48(%rsi), %r14\n"
    "  movq 40(%rsi), %r13\n"
    "  movq 32(%rsi), %r12\n"
    "  movq 24(%rsi), %rbx\n"
    "  movq 16(%rsi), %rbp\n"
    "  movq 8(%rsi), %rsp\n"
    "  jmpq *(%rsi)\n"
    "  ret\n"
#ifndef __MACH__
    ".size _llco_asm_switch, .-_llco_asm_switch\n"
#endif
);

static void llco_asmctx_make(struct llco_asmctx *ctx, 
    void* stack_base, size_t stack_size, void *arg)
{
    // Reserve 128 bytes for the Red Zone space (System V AMD64 ABI).
    stack_size = stack_size - 128; 
    void** stack_high_ptr = (void**)((size_t)stack_base + stack_size - 
        sizeof(size_t));
    stack_high_ptr[0] = (void*)(0xdeaddeaddeaddead);  // Dummy return address.
    ctx->rip = (void*)(_llco_asm_entry);
    ctx->rsp = (void*)(stack_high_ptr);
    ctx->r12 = (void*)(llco_entry);
    ctx->r13 = (void*)(arg);
}

#endif
#endif // x64

// --- END ASM Code --- //

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

////////////////////////////////////////////////////////////////////////////////
// ASM with stackjmp activated
////////////////////////////////////////////////////////////////////////////////
#if defined(LLCO_READY) && defined(LLCO_STACKJMP)
LLCO_NOINLINE LLCO_NORETURN
static void llco_stackjmp(void *stack, size_t stack_size, 
    void(*entry)(void *arg))
{
    struct llco_asmctx ctx = { 0 };
    llco_asmctx_make(&ctx, stack, stack_size, 0);
    struct llco_asmctx ctx0 = { 0 };
    _llco_asm_switch(&ctx0, &ctx);
    llco_exit();
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Windows Fibers
////////////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) && !defined(LLCO_READY)
#define LLCO_WINDOWS
#define LLCO_READY
#define LLCO_METHOD "fibers,windows"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#error Windows fibers unsupported

#endif 

////////////////////////////////////////////////////////////////////////////////
// Webassembly Fibers
////////////////////////////////////////////////////////////////////////////////
#if defined(__EMSCRIPTEN__) && !defined(LLCO_READY)
#define LLCO_WASM
#define LLCO_READY
#define LLCO_METHOD "fibers,emscripten"

#include <emscripten/fiber.h>
#include <string.h>

#ifndef LLCO_ASYNCIFY_STACK_SIZE
#define LLCO_ASYNCIFY_STACK_SIZE 4096
#endif

static __thread char llco_main_stack[LLCO_ASYNCIFY_STACK_SIZE];

#endif

////////////////////////////////////////////////////////////////////////////////
// Ucontext
////////////////////////////////////////////////////////////////////////////////
#if !defined(LLCO_READY)
#define LLCO_UCONTEXT
#define LLCO_READY
#define LLCO_METHOD "ucontext"
#ifndef LLCO_STACKJMP
#define LLCO_STACKJMP
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>

static __thread ucontext_t stackjmp_ucallee;
static __thread int stackjmp_ucallee_gotten = 0;

#if defined(__APPLE__) && defined(__aarch64__) && !defined(LLCO_NOSTACKADJUST)
// Here we ensure that the initial context switch will *not* page the 
// entire stack into process memory before executing the entry point
// function. Which is a behavior that can be observed on Mac OS with
// Apple Silicon. This "trick" can be optionally removed at the expense
// of slower initial jumping into large stacks.
enum llco_stack_grows { DOWNWARDS, UPWARDS }; 

static enum llco_stack_grows llco_stack_grows0(int *addr0) { 
    int addr1; 
    return addr0 < &addr1 ? UPWARDS : DOWNWARDS;
} 

static enum llco_stack_grows llco_stack_grows(void) {
    int addr0;
    return llco_stack_grows0(&addr0);
}

static void llco_adjust_ucontext_stack(ucontext_t *ucp) {
    if (llco_stack_grows() == UPWARDS) {
        ucp->uc_stack.ss_sp = (char*)ucp->uc_stack.ss_sp+ucp->uc_stack.ss_size;
        ucp->uc_stack.ss_size = 0;
    }
}
#else 
#define llco_adjust_ucontext_stack(ucp)
#endif

// Ucontext always uses stackjmp with setjmp/longjmp, instead of swapcontext
// becuase it's much faster.
LLCO_NOINLINE LLCO_NORETURN
static void llco_stackjmp(void *stack, size_t stack_size, 
    void(*entry)(void *arg))
{
    if (!stackjmp_ucallee_gotten) {
        stackjmp_ucallee_gotten = 1;
        getcontext(&stackjmp_ucallee);
    }
    stackjmp_ucallee.uc_stack.ss_sp = stack;
    stackjmp_ucallee.uc_stack.ss_size = stack_size;
    llco_adjust_ucontext_stack(&stackjmp_ucallee);
    makecontext(&stackjmp_ucallee, (void(*)(void))entry, 0);
    setcontext(&stackjmp_ucallee);
    llco_exit();
}

#endif // Ucontext

#if defined(LLCO_STACKJMP)
#include <setjmp.h>
#ifdef _WIN32
// For reasons outside of my understanding, Windows does not allow for jumping
// between stacks using the setjmp/longjmp mechanism.
#error Windows stackjmp not supported
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
// llco switching code
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

struct llco {
    struct llco_desc desc;
#if defined(LLCO_STACKJMP)
    jmp_buf buf;
#elif defined(LLCO_ASM)
    struct llco_asmctx ctx;
#elif defined(LLCO_WASM)
    emscripten_fiber_t fiber;
#elif defined(LLCO_WINDOWS)
    LPVOID fiber;
#endif
#ifdef LLCO_VALGRIND
    int valgrind_stack_id;
#endif
#if defined(__GNUC__)
    void *uw_stop_ip; // record of the last unwind ip.
#endif
};

#ifdef LLCO_VALGRIND
static __thread unsigned int llco_valgrind_stack_id = 0;
static __thread unsigned int llco_cleanup_valgrind_stack_id = 0;
#endif

static __thread struct llco llco_thread = { 0 };
static __thread struct llco *llco_cur = NULL;
static __thread struct llco_desc llco_desc;
static __thread volatile bool llco_cleanup_needed = false;
static __thread volatile struct llco_desc llco_cleanup_desc;
static __thread volatile bool llco_cleanup_active = false;

#define llco_cleanup_guard() { \
    if (llco_cleanup_active) { \
        fprintf(stderr, "%s not available during cleanup\n", __func__); \
        abort(); \
    } \
}

static void llco_cleanup_last(void) {
    if (llco_cleanup_needed) {
        if (llco_cleanup_desc.cleanup) {
            llco_cleanup_active = true;
#ifdef LLCO_VALGRIND
            VALGRIND_STACK_DEREGISTER(llco_cleanup_valgrind_stack_id);
#endif
            llco_cleanup_desc.cleanup(llco_cleanup_desc.stack, 
                llco_cleanup_desc.stack_size, llco_cleanup_desc.udata);
            llco_cleanup_active = false;
        }
        llco_cleanup_needed = false;
    }
}

LLCO_NOINLINE
static void llco_entry_wrap(void *arg) {
    llco_cleanup_last();
#if defined(LLCO_WASM)
    llco_cur = arg;
    llco_cur->desc = llco_desc;
#else
    (void)arg;
    struct llco self = { .desc = llco_desc };
    llco_cur = &self;
#endif
#ifdef LLCO_VALGRIND
    llco_cur->valgrind_stack_id = llco_valgrind_stack_id;
#endif
#if defined(__GNUC__) && !defined(__EMSCRIPTEN__)
    llco_cur->uw_stop_ip = __builtin_return_address(0);
#endif
    llco_cur->desc.entry(llco_cur->desc.udata);
}


LLCO_NOINLINE LLCO_NORETURN
static void llco_entry(void *arg) {
    llco_entry_wrap(arg);
    llco_exit();
}

LLCO_NOINLINE
static void llco_switch1(struct llco *from, struct llco *to, 
    void *stack, size_t stack_size)
{
#ifdef LLCO_VALGRIND
    llco_valgrind_stack_id = VALGRIND_STACK_REGISTER(stack, stack + stack_size);
#endif
#if defined(LLCO_STACKJMP)
    if (to) {
        if (!_setjmp(from->buf)) {
            _longjmp(to->buf, 1);
        }
    } else {
        if (!_setjmp(from->buf)) {
            llco_stackjmp(stack, stack_size, llco_entry);
        }
    }
#elif defined(LLCO_ASM)
    if (to) {
        _llco_asm_switch(&from->ctx, &to->ctx);
    } else {
        struct llco_asmctx ctx = { 0 };
        llco_asmctx_make(&ctx, stack, stack_size, 0);
        _llco_asm_switch(&from->ctx, &ctx);
    }
#elif defined(LLCO_WASM)
    if (to) {
        emscripten_fiber_swap(&from->fiber, &to->fiber);
    } else {
        if (from == &llco_thread) {
            emscripten_fiber_init_from_current_context(&from->fiber, 
                llco_main_stack, LLCO_ASYNCIFY_STACK_SIZE);
        }
        stack_size -= LLCO_ASYNCIFY_STACK_SIZE;
        char *astack = ((char*)stack) + stack_size;
        size_t astack_size = LLCO_ASYNCIFY_STACK_SIZE - sizeof(struct llco);
        struct llco *self = (void*)(astack + astack_size);
        memset(self, 0, sizeof(struct llco));
        emscripten_fiber_init(&self->fiber, llco_entry, 
           self, stack, stack_size, astack, astack_size);
        emscripten_fiber_swap(&from->fiber, &self->fiber);
    }
#elif defined(LLCO_WINDOWS)
    // Unsupported
#endif
}

static void llco_switch0(struct llco_desc *desc, struct llco *co, 
    bool final)
{
    struct llco *from = llco_cur ? llco_cur : &llco_thread;
    struct llco *to = desc ? NULL : co ? co : &llco_thread;
    if (from != to) {
        if (final) {
            llco_cleanup_needed = true;
            llco_cleanup_desc = from->desc;
#ifdef LLCO_VALGRIND
            llco_cleanup_valgrind_stack_id = from->valgrind_stack_id;
#endif
        }
        if (desc) {
            llco_desc = *desc;
            llco_switch1(from, 0, desc->stack, desc->stack_size);
        } else {
            llco_cur = to;
            llco_switch1(from, to, 0, 0);
        }
        llco_cleanup_last();
    }
}



////////////////////////////////////////////////////////////////////////////////
// Exported methods
////////////////////////////////////////////////////////////////////////////////

// Start a new coroutine.
LLCO_EXTERN
void llco_start(struct llco_desc *desc, bool final) {
    if (!desc || desc->stack_size < LLCO_MINSTACKSIZE) {
        fprintf(stderr, "stack too small\n");
        abort();
    }
    llco_cleanup_guard();
    llco_switch0(desc, 0, final);
}

// Switch to another coroutine.
LLCO_EXTERN
void llco_switch(struct llco *co, bool final) {
#if defined(LLCO_ASM)
    // fast track context switch. Saves a few nanoseconds by checking the 
    // exception condition first.
    if (!llco_cleanup_active && llco_cur && co && llco_cur != co && !final) {
        struct llco *from = llco_cur;
        llco_cur = co;
        _llco_asm_switch(&from->ctx, &co->ctx);
        llco_cleanup_last();
        return;
    }
#endif
    llco_cleanup_guard();
    llco_switch0(0, co, final);
}

// Return the current coroutine or NULL if not currently running in a
// coroutine.
LLCO_EXTERN
struct llco *llco_current(void) {
    llco_cleanup_guard();
    return llco_cur == &llco_thread ? 0 : llco_cur;
}

// Returns a string that indicates which coroutine method is being used by
// the program. Such as "asm" or "ucontext", etc.
LLCO_EXTERN
const char *llco_method(void *caps) {
    (void)caps;
    return LLCO_METHOD
#ifdef LLCO_STACKJMP
        ",stackjmp"
#endif
    ;
}

#if defined(__GNUC__) && !defined(__EMSCRIPTEN__) && !defined(_WIN32) && \
    !defined(LLCO_NOUNWIND)

#include <unwind.h>
#include <string.h>
#include <dlfcn.h>

struct llco_dlinfo {
    const char      *dli_fname;     /* Pathname of shared object */
    void            *dli_fbase;     /* Base address of shared object */
    const char      *dli_sname;     /* Name of nearest symbol */
    void            *dli_saddr;     /* Address of nearest symbol */
};


#if defined(__linux__) && !defined(_GNU_SOURCE) 
int dladdr(const void *, void *);
#endif

static void llco_getsymbol(struct _Unwind_Context *uwc, 
    struct llco_symbol *sym)
{
    memset(sym, 0, sizeof(struct llco_symbol));
    sym->cfa = (void*)_Unwind_GetCFA(uwc);
    int ip_before; /* unused */
    sym->ip = (void*)_Unwind_GetIPInfo(uwc, &ip_before);
    struct llco_dlinfo dlinfo = { 0 };
    if (sym->ip && dladdr(sym->ip, (void*)&dlinfo)) {
        sym->fname = dlinfo.dli_fname;
        sym->fbase = dlinfo.dli_fbase;
        sym->sname = dlinfo.dli_sname;
        sym->saddr = dlinfo.dli_saddr;
    }
}

struct llco_unwind_context {
    void *udata;
    void *start_ip;
    bool started;
    int nsymbols;
    int nsymbols_actual;
    struct llco_symbol last;
    bool (*func)(struct llco_symbol *, void *);
    void *unwind_addr;
};

static _Unwind_Reason_Code llco_func(struct _Unwind_Context *uwc, void *ptr) {
    struct llco_unwind_context *ctx = ptr;
    
    struct llco *cur = llco_current();
    if (cur && !cur->uw_stop_ip) {
        return _URC_END_OF_STACK;
    }
    struct llco_symbol sym;
    llco_getsymbol(uwc, &sym);
    if (ctx->start_ip && !ctx->started && sym.ip != ctx->start_ip) {
        return _URC_NO_REASON;
    }
    ctx->started = true;
    if (!sym.ip || (cur && sym.ip == cur->uw_stop_ip)) {
        return _URC_END_OF_STACK;
    }
    ctx->nsymbols++;
    if (!cur) {
        ctx->nsymbols_actual++;
        if (ctx->func && !ctx->func(&sym, ctx->udata)) {
            return _URC_END_OF_STACK;
        }
    } else {
        if (ctx->nsymbols > 1) {
            ctx->nsymbols_actual++;
            if (ctx->func && !ctx->func(&ctx->last, ctx->udata)) {
                return _URC_END_OF_STACK;
            }
        }
        ctx->last = sym;
    }
    return _URC_NO_REASON;
}

LLCO_EXTERN
int llco_unwind(bool(*func)(struct llco_symbol *sym, void *udata), void *udata){
    struct llco_unwind_context ctx = { 
#if defined(__GNUC__) && !defined(__EMSCRIPTEN__)
        .start_ip = __builtin_return_address(0),
#endif
        .func = func, 
        .udata = udata 
    };
    _Unwind_Backtrace(llco_func, &ctx);
    return ctx.nsymbols_actual;
}

#else

LLCO_EXTERN
int llco_unwind(bool(*func)(struct llco_symbol *sym, void *udata), void *udata){
    (void)func; (void)udata;
    /* Unsupported */
    return 0;
}

#endif
// END llco.c

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif

#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__linux__)
#include <sched.h>
static void sched_yield0(void) {
    sched_yield();
}
#else
#define sched_yield0()
#endif

////////////////////////////////////////////////////////////////////////////////
// aat.h
////////////////////////////////////////////////////////////////////////////////
#ifdef SCO_NOAMALGA

#include "deps/aat.h"

#else

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

// BEGIN aat.h
// https://github.com/tidwall/aatree
//
// Copyright 2023 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

// Single header file for generating aat binary search trees.

#ifndef AAT_H
#define AAT_H

#define AAT_DEF(specifiers, prefix, type)                                      \
specifiers type *prefix##_insert(type **root, type *item);                     \
specifiers type *prefix##_delete(type **root, type *key);                      \
specifiers type *prefix##_search(type **root, type *key);                      \
specifiers type *prefix##_delete_first(type **root);                           \
specifiers type *prefix##_delete_last(type **root);                            \
specifiers type *prefix##_first(type **root);                                  \
specifiers type *prefix##_last(type **root);                                   \
specifiers type *prefix##_iter(type **root, type *key);                        \
specifiers type *prefix##_prev(type **root, type *item);                       \
specifiers type *prefix##_next(type **root, type *item);                       \

#define AAT_FIELDS(type, left, right, level)                                   \
type *left;                                                                    \
type *right;                                                                   \
int level;                                                                     \

#define AAT_IMPL(prefix, type, left, right, level, compare)                    \
static void prefix##_clear(type *node) {                                       \
    if (node) {                                                                \
        node->left = 0;                                                        \
        node->right = 0;                                                       \
        node->level = 0;                                                       \
    }                                                                          \
}                                                                              \
                                                                               \
static type *prefix##_skew(type *node) {                                       \
    if (node && node->left &&                                                  \
        node->left->level == node->level)                                      \
    {                                                                          \
        type *left_node = node->left;                                          \
        node->left = left_node->right;                                         \
        left_node->right = node;                                               \
        node = left_node;                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_split(type *node) {                                      \
    if (node && node->right && node->right->right &&                           \
        node->right->right->level == node->level)                              \
    {                                                                          \
        type *right_node = node->right;                                        \
        node->right = right_node->left;                                        \
        right_node->left = node;                                               \
        right_node->level++;                                                   \
        node = right_node;                                                     \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_insert0(type *node, type *item, type **replaced) {       \
    if (!node) {                                                               \
        item->left = 0;                                                        \
        item->right = 0;                                                       \
        item->level = 1;                                                       \
        node = item;                                                           \
    } else {                                                                   \
        int cmp = compare(item, node);                                         \
        if (cmp < 0) {                                                         \
            node->left = prefix##_insert0(node->left, item, replaced);         \
        } else if (cmp > 0) {                                                  \
            node->right = prefix##_insert0(node->right, item, replaced);       \
        } else {                                                               \
            *replaced = node;                                                  \
            item->left = node->left;                                           \
            item->right = node->right;                                         \
            item->level = node->level;                                         \
            node = item;                                                       \
        }                                                                      \
    }                                                                          \
    node = prefix##_skew(node);                                                \
    node = prefix##_split(node);                                               \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_insert(type **root, type *item) {                               \
    type *replaced = 0;                                                        \
    *root = prefix##_insert0(*root, item, &replaced);                          \
    if (replaced != item) {                                                    \
        prefix##_clear(replaced);                                              \
    }                                                                          \
    return replaced;                                                           \
}                                                                              \
                                                                               \
static type *prefix##_decrease_level(type *node) {                             \
    if (node->left || node->right) {                                           \
        int new_level = 0;                                                     \
        if (node->left && node->right) {                                       \
            if (node->left->level < node->right->level) {                      \
                new_level = node->left->level;                                 \
            } else {                                                           \
                new_level = node->right->level;                                \
            }                                                                  \
        }                                                                      \
        new_level++;                                                           \
        if (new_level < node->level) {                                         \
            node->level = new_level;                                           \
            if (node->right && new_level < node->right->level) {               \
                node->right->level = new_level;                                \
            }                                                                  \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_delete_fixup(type *node) {                               \
    node = prefix##_decrease_level(node);                                      \
    node = prefix##_skew(node);                                                \
    node->right = prefix##_skew(node->right);                                  \
    if (node->right && node->right->right) {                                   \
        node->right->right = prefix##_skew(node->right->right);                \
    }                                                                          \
    node = prefix##_split(node);                                               \
    node->right = prefix##_split(node->right);                                 \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_delete_first0(type *node,                                \
    type **deleted)                                                            \
{                                                                              \
    if (node) {                                                                \
        if (!node->left) {                                                     \
            *deleted = node;                                                   \
            if (node->right) {                                                 \
                node = node->right;                                            \
            } else {                                                           \
                node = 0;                                                      \
            }                                                                  \
        } else {                                                               \
            node->left = prefix##_delete_first0(node->left, deleted);          \
            node = prefix##_delete_fixup(node);                                \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
static type *prefix##_delete_last0(type *node,                                 \
    type **deleted)                                                            \
{                                                                              \
    if (node) {                                                                \
        if (!node->right) {                                                    \
            *deleted = node;                                                   \
            if (node->left) {                                                  \
                node = node->left;                                             \
            } else {                                                           \
                node = 0;                                                      \
            }                                                                  \
        } else {                                                               \
            node->right = prefix##_delete_last0(node->right, deleted);         \
            node = prefix##_delete_fixup(node);                                \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_delete_first(type **root) {                                     \
    type *deleted = 0;                                                         \
    *root = prefix##_delete_first0(*root, &deleted);                           \
    prefix##_clear(deleted);                                                   \
    return deleted;                                                            \
}                                                                              \
                                                                               \
type *prefix##_delete_last(type **root) {                                      \
    type *deleted = 0;                                                         \
    *root = prefix##_delete_last0(*root, &deleted);                            \
    prefix##_clear(deleted);                                                   \
    return deleted;                                                            \
}                                                                              \
                                                                               \
static type *prefix##_delete0(type *node,                                      \
    type *key, type **deleted)                                                 \
{                                                                              \
    if (node) {                                                                \
        int cmp = compare(key, node);                                          \
        if (cmp < 0) {                                                         \
            node->left = prefix##_delete0(node->left, key, deleted);           \
        } else if (cmp > 0) {                                                  \
            node->right = prefix##_delete0(node->right, key, deleted);         \
        } else {                                                               \
            *deleted = node;                                                   \
            if (!node->left && !node->right) {                                 \
                node = 0;                                                      \
            } else {                                                           \
                type *leaf_deleted = 0;                                        \
                if (!node->left) {                                             \
                    node->right = prefix##_delete_first0(node->right,          \
                        &leaf_deleted);                                        \
                } else {                                                       \
                    node->left = prefix##_delete_last0(node->left,             \
                        &leaf_deleted);                                        \
                }                                                              \
                leaf_deleted->left = node->left;                               \
                leaf_deleted->right = node->right;                             \
                leaf_deleted->level = node->level;                             \
                node = leaf_deleted;                                           \
            }                                                                  \
        }                                                                      \
        if (node) {                                                            \
            node = prefix##_delete_fixup(node);                                \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_delete(type **root, type *key) {                                \
    type *deleted = 0;                                                         \
    *root = prefix##_delete0(*root, key, &deleted);                            \
    prefix##_clear(deleted);                                                   \
    return deleted;                                                            \
}                                                                              \
                                                                               \
type *prefix##_search(type **root, type *key) {                                \
    type *found = 0;                                                           \
    type *node = *root;                                                        \
    while (node) {                                                             \
        int cmp = compare(key, node);                                          \
        if (cmp < 0) {                                                         \
            node = node->left;                                                 \
        } else if (cmp > 0) {                                                  \
            node = node->right;                                                \
        } else {                                                               \
            found = node;                                                      \
            node = 0;                                                          \
        }                                                                      \
    }                                                                          \
    return found;                                                              \
}                                                                              \
                                                                               \
type *prefix##_first(type **root) {                                            \
    type *node = *root;                                                        \
    if (node) {                                                                \
        while (node->left) {                                                   \
            node = node->left;                                                 \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_last(type **root) {                                             \
    type *node = *root;                                                        \
    if (node) {                                                                \
        while (node->right) {                                                  \
            node = node->right;                                                \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_iter(type **root, type *key) {                                  \
    type *found = 0;                                                           \
    type *node = *root;                                                        \
    while (node) {                                                             \
        int cmp = compare(key, node);                                          \
        if (cmp < 0) {                                                         \
            found = node;                                                      \
            node = node->left;                                                 \
        } else if (cmp > 0) {                                                  \
            node = node->right;                                                \
        } else {                                                               \
            found = node;                                                      \
            node = 0;                                                          \
        }                                                                      \
    }                                                                          \
    return found;                                                              \
}                                                                              \
                                                                               \
static type *prefix##_parent(type **root,                                      \
    type *item)                                                                \
{                                                                              \
    type *parent = 0;                                                          \
    type *node = *root;                                                        \
    while (node) {                                                             \
        int cmp = compare(item, node);                                         \
        if (cmp < 0) {                                                         \
            parent = node;                                                     \
            node = node->left;                                                 \
        } else if (cmp > 0) {                                                  \
            parent = node;                                                     \
            node = node->right;                                                \
        } else {                                                               \
            node = 0;                                                          \
        }                                                                      \
    }                                                                          \
    return parent;                                                             \
}                                                                              \
                                                                               \
type *prefix##_next(type **root, type *node) {                                 \
    if (node) {                                                                \
        if (node->right) {                                                     \
            node = node->right;                                                \
            while (node->left) {                                               \
                node = node->left;                                             \
            }                                                                  \
        } else {                                                               \
            type *parent = prefix##_parent(root, node);                        \
            while (parent && parent->left != node) {                           \
                node = parent;                                                 \
                parent = prefix##_parent(root, parent);                        \
            }                                                                  \
            node = parent;                                                     \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \
                                                                               \
type *prefix##_prev(type **root, type *node) {                                 \
    if (node) {                                                                \
        if (node->left) {                                                      \
            node = node->left;                                                 \
            while (node->right) {                                              \
                node = node->right;                                            \
            }                                                                  \
        } else {                                                               \
            type *parent = prefix##_parent(root, node);                        \
            while (parent && parent->right != node) {                          \
                node = parent;                                                 \
                parent = prefix##_parent(root, parent);                        \
            }                                                                  \
            node = parent;                                                     \
        }                                                                      \
    }                                                                          \
    return node;                                                               \
}                                                                              \

#endif // AAT_H
// END aat.h

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif


////////////////////////////////////////////////////////////////////////////////
// Platform independent code below
////////////////////////////////////////////////////////////////////////////////

struct sco_link {
    struct sco *prev;
    struct sco *next;
};

struct sco {
    union {
        // Linked list
        struct {
            struct sco *prev;
            struct sco *next;
        };
        // Binary tree (AA-tree)
        struct {
            AAT_FIELDS(struct sco, left, right, level)
        };
    };
    int64_t id;
    void *udata;
    struct llco *llco;
};

static int sco_compare(struct sco *a, struct sco *b) {
    return a->id < b->id ? -1: a->id > b->id;
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

AAT_DEF(static, sco_aat, struct sco)
AAT_IMPL(sco_aat, struct sco, left, right, level, sco_compare)

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// https://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
// hash u64 using mix13
static uint64_t sco_mix13(uint64_t key) {
    key ^= (key >> 30);
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= (key >> 27);
    key *= UINT64_C(0x94d049bb133111eb);
    key ^= (key >> 31);
    return key;
}


////////////////////////////////////////////////////////////////////////////////
// sco_map - A hashmap-style structure that stores sco types using multiple
// binary search trees (aa-tree) in hashed shards. This allows for the map to
// grow evenly, without allocations, and performing much faster than using a
// single BST.
////////////////////////////////////////////////////////////////////////////////

#ifndef SCO_NSHARDS
#define SCO_NSHARDS 512
#endif

struct sco_map {
    struct sco *roots[SCO_NSHARDS];
    int count;
};

#define scp_map_getaat(sco) \
    (&map->roots[sco_mix13((sco)->id) & (SCO_NSHARDS-1)])

static struct sco *sco_map_insert(struct sco_map *map, struct sco *sco) {
    struct sco *prev = sco_aat_insert(scp_map_getaat(sco), sco);
    if (!prev) {
        map->count++;
    }
    return prev;
}

static struct sco *sco_map_delete(struct sco_map *map, struct sco *key){
    struct sco *prev = sco_aat_delete(scp_map_getaat(key), key);
    if (prev) {
        map->count--;
    }
    return prev;
}

struct sco_list {
    struct sco_link head;
    struct sco_link tail;
};

////////////////////////////////////////////////////////////////////////////////
// Global and thread-local variables.
////////////////////////////////////////////////////////////////////////////////

static __thread bool sco_initialized = false;
static __thread size_t sco_nrunners = 0;
static __thread struct sco_list sco_runners = { 0 };
static __thread size_t sco_nyielders = 0;
static __thread struct sco_list sco_yielders = { 0 };
static __thread struct sco *sco_cur = NULL;
static __thread struct sco_map sco_paused = { 0 };
static __thread size_t sco_npaused = 0;
static __thread bool sco_exit_to_main_requested = false;
static __thread void(*sco_user_entry)(void *udata);

static atomic_int_fast64_t sco_next_id = 0;
static atomic_bool sco_locker = 0;
static struct sco_map sco_detached = { 0 };
static size_t sco_ndetached = 0;

static void sco_lock(void) {
    bool expected = false;
    while(!atomic_compare_exchange_weak(&sco_locker, &expected, true)) {
        expected = false;
        sched_yield0();
    }
}

static void sco_unlock(void) {
    atomic_store(&sco_locker, false);
}

static void sco_list_init(struct sco_list *list) {
    list->head.prev = NULL;
    list->head.next = (struct sco*)&list->tail;
    list->tail.prev = (struct sco*)&list->head;
    list->tail.next = NULL;
}

// Remove the coroutine from the runners or yielders list.
static void sco_remove_from_list(struct sco *co) {
    co->prev->next = co->next;
    co->next->prev = co->prev;
    co->next = co;
    co->prev = co;
}

static void sco_init(void) {
    if (!sco_initialized) {
        sco_list_init(&sco_runners);
        sco_list_init(&sco_yielders);
        sco_initialized = true;
    }
}

static struct sco *sco_list_pop_front(struct sco_list *list) {
    struct sco *co = NULL;
    if (list->head.next != (struct sco*)&list->tail) {
        co = list->head.next;
        sco_remove_from_list(co);
    }
    return co;
}

static void sco_list_push_back(struct sco_list *list, struct sco *co) {
    sco_remove_from_list(co);
    list->tail.prev->next = co;
    co->prev = list->tail.prev;
    co->next = (struct sco*)&list->tail;
    list->tail.prev = co;
}

static void sco_return_to_main(bool final) {
    sco_cur = NULL;
    sco_exit_to_main_requested = false;
    llco_switch(0, final);
}

static void sco_switch(bool resumed_from_main, bool final) {
    if (sco_nrunners == 0) {
        // No more runners.
        if (sco_nyielders == 0 || sco_exit_to_main_requested ||
            (!resumed_from_main && sco_npaused > 0)) {
            sco_return_to_main(final);
            return;
        }
        // Convert the yielders to runners
        sco_runners.head.next = sco_yielders.head.next;
        sco_runners.head.next->prev = (struct sco*)&sco_runners.head;
        sco_runners.tail.prev = sco_yielders.tail.prev;
        sco_runners.tail.prev->next = (struct sco*)&sco_runners.tail;
        sco_yielders.head.next = (struct sco*)&sco_yielders.tail;
        sco_yielders.tail.prev = (struct sco*)&sco_yielders.head;
        sco_nrunners = sco_nyielders;
        sco_nyielders = 0;
    }
    sco_cur = sco_list_pop_front(&sco_runners);
    sco_nrunners--;
    llco_switch(sco_cur->llco, final);
}

static void sco_entry(void *udata) {
    // Initialize a new coroutine on the user's stack.
    struct sco scostk = { 0 };
    struct sco *co = &scostk;
    co->llco = llco_current();
    co->id = atomic_fetch_add(&sco_next_id, 1) + 1;
    co->udata = udata;
    co->prev = co;
    co->next = co;
    if (sco_cur) {
        // Reschedule the coroutine that started this one immediately after
        // all running coroutines, but before any yielding coroutines, and
        // continue running the started coroutine.
        sco_list_push_back(&sco_runners, sco_cur);
        sco_nrunners++;
    }
    sco_cur = co;
    if (sco_user_entry) {
        sco_user_entry(udata);
    }
    // This coroutine is finished. Switch to the next coroutine.
    sco_switch(false, true);
}

SCO_EXTERN
void sco_exit(void) {
    if (sco_cur) {
        sco_exit_to_main_requested = true;
        sco_switch(false, true);
    }
}

SCO_EXTERN
void sco_start(struct sco_desc *desc) {
    sco_init();
    struct llco_desc llco_desc = {
        .entry = sco_entry,
        .cleanup = desc->cleanup,
        .stack = desc->stack,
        .stack_size = desc->stack_size,
        .udata = desc->udata,
    };
    sco_user_entry = desc->entry;
    llco_start(&llco_desc, false);
}

SCO_EXTERN
int64_t sco_id(void) {
    return sco_cur ? sco_cur->id : 0;
}

SCO_EXTERN
void sco_yield(void) {
    if (sco_cur) {
        sco_list_push_back(&sco_yielders, sco_cur);
        sco_nyielders++;
        sco_switch(false, false);
    }
}

SCO_EXTERN
void sco_pause(void) {
    if (sco_cur) {
        sco_map_insert(&sco_paused, sco_cur);
        sco_npaused++;
        sco_switch(false, false);
    }
}

SCO_EXTERN
void sco_resume(int64_t id) {
    sco_init();
    if (id == 0 && !sco_cur) {
        // Resuming from main
        sco_switch(true, false);
    } else {
        // Resuming from coroutine
        struct sco *co = sco_map_delete(&sco_paused, &(struct sco){ .id = id });
        if (co) {
            sco_npaused--;
            co->prev = co;
            co->next = co;
            sco_list_push_back(&sco_yielders, co);
            sco_nyielders++;
            sco_yield();
        }
    }
}

SCO_EXTERN
void sco_detach(int64_t id) {
    struct sco *co = sco_map_delete(&sco_paused, &(struct sco){ .id = id });
    if (co) {
        sco_npaused--;
        sco_lock();
        sco_map_insert(&sco_detached, co);
        sco_ndetached++;
        sco_unlock();
    }
}

SCO_EXTERN
void sco_attach(int64_t id) {
    sco_lock();
    struct sco *co = sco_map_delete(&sco_detached, &(struct sco){ .id = id });
    if (co) {
        sco_ndetached--;
    }
    sco_unlock();
    if (co) {
        sco_map_insert(&sco_paused, co);
        sco_npaused++;
    }
}

SCO_EXTERN
void *sco_udata(void) {
    return sco_cur ? sco_cur->udata : NULL;
}

SCO_EXTERN
size_t sco_info_scheduled(void) {
    return sco_nyielders;
}

SCO_EXTERN
size_t sco_info_paused(void) {
    return sco_npaused;
}

SCO_EXTERN
size_t sco_info_running(void) {
    size_t running = sco_nrunners;
    if (sco_cur) {
        // Count the current coroutine
        running++;
    }
    return running;
}

SCO_EXTERN
size_t sco_info_detached(void) {
    sco_lock();
    size_t ndetached = sco_ndetached;
    sco_unlock();
    return ndetached;
}

// Returns true if there are any coroutines running, yielding, or paused.
SCO_EXTERN
bool sco_active(void) {
    // Notice that detached coroutinues are not included.
    return (sco_nyielders + sco_npaused + sco_nrunners + !!sco_cur) > 0;
}

SCO_EXTERN
const char *sco_info_method(void) {
    return llco_method(0);
}

struct sco_unwind_context {
    int nsymbols_actual;
    bool started;
    void *start_ip;
    void *udata;
    bool (*func)(struct sco_symbol*, void*);
};

static bool sco_unwind_step(struct llco_symbol *llco_sym, void *udata) {
    struct sco_unwind_context *ctx = udata;
    if (ctx->start_ip && !ctx->started && llco_sym->ip != ctx->start_ip) {
        return true;
    }
    struct sco_symbol sym = {
        .cfa = llco_sym->cfa,
        .fbase = llco_sym->fbase,
        .fname = llco_sym->fname,
        .ip = llco_sym->ip,
        .saddr = llco_sym->saddr,
        .sname = llco_sym->sname,
    };
    ctx->started = true;
    ctx->nsymbols_actual++;
    return !ctx->func || ctx->func(&sym, ctx->udata);
}

// Unwinds the stack and returns the number of symbols
SCO_EXTERN
int sco_unwind(bool(*func)(struct sco_symbol *sym, void *udata), void *udata) {
    struct sco_unwind_context ctx = { 
#if defined(__GNUC__) && !defined(__EMSCRIPTEN__)
        .start_ip = __builtin_return_address(0),
#endif
        .func = func, 
        .udata = udata,
    };
    llco_unwind(sco_unwind_step, &ctx);
    return ctx.nsymbols_actual;
}
// END sco.c

// BEGIN stack.c
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
// END stack.c

#ifndef NECO_NOWORKERS
// BEGIN worker.c
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
// END worker.c
#endif // NECO_NOWORKERS

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <setjmp.h>
#include <inttypes.h>

#include <fcntl.h>
#include <unistd.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <sys/syscall.h>
#endif
#include <pthread.h>


#include "neco.h"

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/event.h>
#define NECO_POLL_KQUEUE
#elif defined(__linux__)
#include <sys/epoll.h>
#include <sys/eventfd.h>
#define NECO_POLL_EPOLL
#elif defined(__EMSCRIPTEN__) || defined(_WIN32)
// #warning Webassembly has no polling
#define NECO_POLL_DISABLED
#else
// Only FreeBSD, Apple, and Linux
#error Platform not supported
#endif

#ifdef _WIN32
#ifndef SIGUSR1
#define SIGUSR1 30      /* user defined signal 1 */
#endif
#ifndef SIGUSR2
#define SIGUSR2 31      /* user defined signal 2 */
#endif
#endif

#define CLAMP(x, min, max) ((x)<=(min)?(min):(x)>=(max)?(max):(x))

static_assert(sizeof(int) >= sizeof(uint32_t), "32-bit or higher required");

#if defined(__GNUC__)
#define noinline __attribute__ ((noinline))
#define noreturn __attribute__ ((noreturn))
#define aligned16 __attribute__((aligned(16)))
#else
#define noinline
#define noreturn
#define aligned16
#endif

#ifndef NECO_NOPOOL
#define POOL_ENABLED true
#else
#define POOL_ENABLED false
#endif

#ifdef NECO_TESTING
#define TESTING_EXTERN(specifier) extern
#else
#define TESTING_EXTERN(specifier) specifier
#endif

void sco_yield(void);

TESTING_EXTERN(static)
const char *strsignal0(int signo) {
    static __thread char buf[32];
    if (signo <= 0 || signo >= 32) {
        snprintf(buf, sizeof(buf), "Unknown signal: %d", signo);
        return buf;
    } else {
#ifdef _WIN32
        snprintf(buf, sizeof(buf), "Signal: %d", signo);
        return buf;
 #else
        return strsignal(signo);
#endif
    }
}

// https://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
// hash u64 using mix13
static uint64_t mix13(uint64_t key) {
    key ^= (key >> 30);
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= (key >> 27);
    key *= UINT64_C(0x94d049bb133111eb);
    key ^= (key >> 31);
    return key;
}

#ifdef NECO_TESTING
#include "tests/panic.h"
#else
// print a string, function, file, and line number.
#define pwhere(str) \
    fprintf(stderr, "%s, function %s, file %s, line %d.\n", \
        (str), __func__, __FILE__, __LINE__)

// define an unreachable section of code. 
// This will not signal like __builtin_unreachable()
#define unreachable() \
    pwhere("Unreachable"); \
    abort()

// must is like assert but it cannot be disabled with -DNDEBUG
// This is primarily used for a syscall that, when provided valid paramaters, 
// should never fail. Yet if it does fail we want the 411.
#define must(cond) \
    if (!(cond)) { \
        pwhere("Must failed: " #cond); \
        perror("System error:"); \
        abort(); \
    } \
    (void)0

#define panic(fmt, ...) \
    fprintf(stderr, "panic: " fmt "\n", __VA_ARGS__); \
    print_stacktrace(1, true); \
    fprintf(stderr, "\n"); \
    abort();

#endif

#ifdef NECO_TESTING
#include "tests/signals.h"
#else
static bool bt_iscommon(const char *symbol) {
    return symbol && (
        strstr(symbol, "coentry") ||
        strstr(symbol, "sco_entry") ||
        strstr(symbol, "print_stacktrace") ||
        strstr(symbol, "sighandler") ||
        strstr(symbol, "_sigtramp")
    );
}

// system_out works like the system() but returns the contents to a newly
// allocated string.
// You must call free() on the output when you are done.
static char *system_out(const char *command) {
    FILE *ls = popen(command, "r");
    if (!ls) {
        return NULL;
    }
    size_t nout = 0;
    char *out = NULL;
    char buf[128];
    while (fgets(buf, sizeof(buf), ls) != 0) {
        size_t nbuf = strlen(buf);
        char *out2 = realloc(out, nout+nbuf+1);
        if (!out2) {
            free(out);
            pclose(ls);
            return NULL;
        }
        out = out2;
        memcpy(out+nout, buf, nbuf);
        nout += nbuf;
        out[nout] = '\0';
    }
    pclose(ls);
    if (out == NULL) {
        out = malloc(1);
        if (out) {
            out[0] = '\0';
        }
    }
    return out;
}

struct bt_context {
    int n;
    int skip;
    bool omit_commons;
};

static bool print_symbol(struct sco_symbol *sym, void *udata) {
    struct bt_context *ctx = udata;
    const char *fname = (char*)sym->fname;
    size_t off1 = (size_t)sym->ip - (size_t)sym->fbase;
    size_t off2 = off1;
    const char *sname = sym->sname;
    if (!sname) {
        sname = fname;
        off2 = (size_t)sym->ip - (size_t)sym->fbase;
    }
    char cmd[256];
    bool printed = false;
    size_t n = 0;
#if defined(__linux__) 
    n = snprintf(cmd, sizeof(cmd), "addr2line -psfe '%s' %p", 
        fname, (void*)off1);
#elif !defined(__FreeBSD__)
    n = snprintf(cmd, sizeof(cmd), "atos -o %s -l 0x%zx 0x%016zx", 
        fname, (size_t)sym->fbase, (size_t)sym->ip);
#endif
    if (n > 0 && n < sizeof(cmd)-1) {
        char *out = system_out(cmd);
        if (out) {
            if (!bt_iscommon(out)) {
                if (ctx->skip == 0) {
                    fprintf(stderr, "0x%016zx: %s", (size_t)off2, out);
                } else {
                    ctx->skip--;
                }
            }
            free(out);
            printed = true;
        }
    }
    if (!printed) {
        if (!bt_iscommon(sname)) {
            if (ctx->skip == 0) {
                fprintf(stderr, "0x%016zx: %s + %zu\n", 
                    (size_t)off2, sname, off2);
            } else {
                ctx->skip--;
            }
        }
    }
    ctx->n++;
    return true;
}

static void print_stacktrace(int skip, bool omit_commons) {
    fprintf(stderr, "\n------ STACK TRACE ------\n");
    struct bt_context ctx = {
        .skip = skip,
        .omit_commons = omit_commons,
    };
    sco_unwind(print_symbol, &ctx);
    // stacktrace all coroutines? 
}

noinline noreturn
static void sigexitnow(int signo) {
    fprintf(stderr, "%s\n", signo == SIGINT ? "" : strsignal0(signo));
    _Exit(128+signo);
}
#endif

// Private functions. See decl for descriptions.
int neco_errconv_from_sys(void);
void neco_errconv_to_sys(int);

static bool env_paniconerror = false;
static int env_canceltype = NECO_CANCEL_ASYNC;
static int env_cancelstate = NECO_CANCEL_ENABLE;
static void *(*malloc_)(size_t) = NULL;
static void *(*realloc_)(void*, size_t) = NULL;
static void (*free_)(void*) = NULL;


/// Globally set the allocators for all Neco functions.
///
/// _This should only be run once at program startup and before the first 
/// neco_start function is called_.
/// @see GlobalFuncs
void neco_env_setallocator(void *(*malloc)(size_t), 
    void *(*realloc)(void*, size_t), void (*free)(void*)) 
{
    malloc_ = malloc;
    realloc_ = realloc;
    free_ = free;
}

void *neco_malloc(size_t nbytes) {
    void *ptr = 0;
    if (nbytes < SIZE_MAX) {
        if (malloc_) {
            ptr = malloc_(nbytes);
        } else {
            ptr = malloc(nbytes);
        }
    }
    if (!ptr && nbytes > 0 && env_paniconerror) {
        panic("%s", strerror(ENOMEM));
    }
    return ptr;
}

void *neco_realloc(void *ptr, size_t nbytes) {
    void *ptr2 = 0;
    if (nbytes < SIZE_MAX) {
        if (!ptr) {
            ptr2 = neco_malloc(nbytes);
        } else if (realloc_) {
            ptr2 = realloc_(ptr, nbytes);
        } else {
            ptr2 = realloc(ptr, nbytes);
        }
    }
    if (!ptr2 && nbytes > 0 && env_paniconerror) {
        panic("%s", strerror(ENOMEM));
    }
    return ptr2;
}

void neco_free(void *ptr) {
    if (ptr) {
        if (free_) {
            free_(ptr);
        } else {
            free(ptr);
        }
    }
}

/// Globally set the panic-on-error state for all coroutines.
///
/// This will cause panics (instead of returning the error) for three errors:
/// `NECO_INVAL`, `NECO_PERM`, and `NECO_NOMEM`.
///
/// _This should only be run once at program startup and before the first 
/// neco_start function is called_.
/// @see GlobalFuncs
void neco_env_setpaniconerror(bool paniconerror) {
    env_paniconerror = paniconerror;
}

/// Globally set the canceltype for all coroutines.
///
/// _This should only be run once at program startup and before the first 
/// neco_start function is called_.
/// @see GlobalFuncs
void neco_env_setcanceltype(int type) {
    env_canceltype = type;
}

/// Globally set the cancelstate for all coroutines.
///
/// _This should only be run once at program startup and before the first 
/// neco_start function is called_.
/// @see GlobalFuncs
void neco_env_setcancelstate(int state) {
    env_cancelstate = state;
}

// return either a BSD kqueue or Linux epoll type.
static int evqueue(void) {
#if defined(NECO_POLL_EPOLL) 
    return epoll_create1(0);
#elif defined(NECO_POLL_KQUEUE) 
    return kqueue();
#else
    return -1;
#endif
}

#define read0 read
#define recv0 recv
#define write0 write
#define send0 send
#define accept0 accept
#define connect0 connect
#define socket0 socket
#define bind0 bind
#define listen0 listen
#define setsockopt0 setsockopt
#define nanosleep0 nanosleep
#define fcntl0 fcntl
#define pthread_create0 pthread_create
#define pthread_detach0 pthread_detach
#define pipe0 pipe
#define malloc0 neco_malloc
#define realloc0 neco_realloc
#define free0 neco_free
#define stack_get0 stack_get
#define evqueue0 evqueue
#define kevent0 kevent
#define epoll_ctl0 epoll_ctl

// Provide the ability to cause system calls to fail during testing.
// Not for production.
#ifdef NECO_TESTING
#include "tests/fail_counters.h"
#endif

// Return monotonic nanoseconds of the CPU clock.
static int64_t getnow(void) {
    struct timespec now = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * INT64_C(1000000000) + now.tv_nsec;
}

enum evkind {
    EVREAD  = 1, // NECO_WAIT_READ
    EVWRITE = 2, // NECO_WAIT_WRITE
};

static_assert(EVREAD == NECO_WAIT_READ, "");
static_assert(EVWRITE == NECO_WAIT_WRITE, "");

struct cleanup {
    void (*routine)(void *);
    void *arg;
    struct cleanup *next;
} aligned16;

////////////////////////////////////////////////////////////////////////////////
// colist - The standard queue type that is just a doubly linked list storing
// the highest priority coroutine at the head and lowest at the tail.
////////////////////////////////////////////////////////////////////////////////

struct coroutine;

struct colink {
    struct coroutine *prev;
    struct coroutine *next;
} aligned16;

struct colist {
    struct colink head;
    struct colink tail;
};

// Remove the coroutine from any list, eg. yielders, sigwaiters, or free.
static void remove_from_list(struct coroutine *co) {
    struct colink *link = (void*)co;
    ((struct colink*)link->prev)->next = link->next;
    ((struct colink*)link->next)->prev = link->prev;
    link->next = (struct coroutine*)link;
    link->prev = (struct coroutine*)link;
}

static void colist_init(struct colist *list) {
    list->head.prev = NULL;
    list->head.next = (struct coroutine*)&list->tail;
    list->tail.prev = (struct coroutine*)&list->head;
    list->tail.next = NULL;
}

static void colist_push_back(struct colist *list, struct coroutine *co) {
    remove_from_list(co);
    struct colink *link = (void*)co;
    ((struct colink*)list->tail.prev)->next = (struct coroutine*)link;
    link->prev = list->tail.prev;
    link->next = (struct coroutine*)&list->tail;
    list->tail.prev = (struct coroutine*)link;
}

static struct coroutine *colist_pop_front(struct colist *list) {
    struct coroutine *co = list->head.next;
    if (co == (struct coroutine*)&list->tail) {
        return NULL;
    }
    remove_from_list(co);
    return co;
}

static bool colist_is_empty(struct colist *list) {
    return list->head.next == (struct coroutine*)&list->tail;
}

////////////////////////////////////////////////////////////////////////////////
// coroutine - structure for a single coroutine object, state, and state
////////////////////////////////////////////////////////////////////////////////

enum cokind { COROUTINE, SELECTCASE };

struct coroutine {
    struct coroutine *prev;
    struct coroutine *next;
    enum cokind kind;             // always COROUTINE

    int64_t id;                   // coroutine id (sco_id())
    struct stack stack;           // coroutine stack
    int argc;                     // number of coroutine arguments
    void **argv;                  // the coroutine arguments
    void *aargv[4];               // preallocated arguments
    void(*coroutine)(int,void**); // user coroutine function

    int64_t lastid;               // identifer of the last started coroutine
    int64_t starterid;            // identifer of the starter coroutine
    bool paused;                  // coroutine is paused
    bool deadlined;               // coroutine operation was deadlined

    struct cleanup *cleanup;      // cancelation cleanup stack

    bool rlocked;
    bool suspended;

    int64_t pool_ts;              // timestamp when added to a pool

    char *cmsg;                   // channel message data from sender
    bool cclosed;                 // channel closed by sender

    uint32_t sigwatch;            // coroutine is watching these signals (mask)
    uint32_t sigmask;             // coroutine wait mask and result

    int canceltype;               // cancelation type
    int cancelstate;              // cancelation state
    bool canceled;                // coroutine operation was cancelled
    struct colist cancellist;     // waiting coroutines
    int ncancellist;              // number of waiting coroutines

    struct colist joinlist;       // waiting coroutines
    int njoinlist;                // number of waiting coroutines

    struct neco_chan *gen;        // self generator (actually a channel)

    // For the rt->all comap, which stores all active coroutines
    AAT_FIELDS(struct coroutine,  all_left, all_right, all_level)

    // Deadline for pause. All paused will have this set to something.
    int64_t deadline;
    AAT_FIELDS(struct coroutine, dl_left, dl_right, dl_level)

    // File event node
    int evfd;
    enum evkind evkind;
    AAT_FIELDS(struct coroutine, evleft, evright, evlevel)
} aligned16;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

static int all_compare(struct coroutine *a, struct coroutine *b) {
    // order by id
    return a->id < b->id ? -1 : a->id > b->id;
}

AAT_DEF(static, all, struct coroutine)
AAT_IMPL(all, struct coroutine, all_left, all_right, all_level, all_compare)

static int dl_compare(struct coroutine *a, struct coroutine *b) {
    // order by deadline, id
    return 
        a->deadline < b->deadline ? -1 : a->deadline > b->deadline ? 1 :
        a->id < b->id ? -1 : a->id > b->id;
}

AAT_DEF(static, dlqueue, struct coroutine)
AAT_IMPL(dlqueue, struct coroutine, dl_left, dl_right, dl_level, dl_compare)

static int evcompare(struct coroutine *a, struct coroutine *b) {
    // order by evfd, evkind, id
    return
        a->evfd < b->evfd ? -1 : a->evfd > b->evfd ? 1 :
        a->evkind < b->evkind ? -1 : a->evkind > b->evkind ? 1 :
        a->id < b->id ? -1 : a->id > b->id;
}

AAT_DEF(static, evaat, struct coroutine)
AAT_IMPL(evaat, struct coroutine, evleft, evright, evlevel, evcompare)

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static __thread int lasterr = 0;

/// Returns last known error from a Neco operation
/// 
/// See [Neco errors](./API.md#errors) for a list.
int neco_lasterr(void) {
    return lasterr;
}

noinline
static void errhpnd(int ret) {
    if (ret == -1) {
        lasterr = neco_errconv_from_sys();
    } else {
        lasterr = ret;
    }
    if (env_paniconerror) {
        // The user has requested to panic on 
        // Only certain errors may cause a panic. 
        switch (lasterr) {
        case NECO_INVAL:
        case NECO_PERM:
        case NECO_NOMEM:
            panic("%s", neco_strerror(ret));
        default:
            break;
        }
    }
}

// error_guard is a small macro that is used in every public neco_* api 
// function. It's called just before the function returns and ensures that
// the provided return value is stored to the "lasterr" thread-local variable
// and that if the error is a posix system error, such as EPERM, that it is
// correctly converted into a neco error code.
// If the user has requested to panic on errors by using the environment
// option `neco_setpaniconerror(true)` then this call will cause a panic if
// the provided return value is an qualifying error, such as NECO_NOMEM.
#define error_guard(ret) \
    lasterr = 0; \
    if (ret < 0) { \
        errhpnd(ret); \
    } \
    (void)0

// async_error_guard is an extension of error_guard that also provides async
// coroutine cancellation.
// When the provided return value is NECO_CANCELED _and_ the user canceltype
// is NECO_CANCEL_ASYNC, then the currently running coroutine will immediately
// be terminated. Any clean-up handlers established by neco_cleanup_push() that
// have not yet been popped, are popped and executed.
#define async_error_guard(ret) \
    error_guard(ret); \
    if (lasterr == NECO_CANCELED) { \
        struct coroutine *co = coself(); \
        if (co->canceltype == NECO_CANCEL_ASYNC) { \
            coexit(true); \
        } \
    } \
    (void)0 \

////////////////////////////////////////////////////////////////////////////////
// comap - A hashmap-style structure that stores coroutines using multiple
// binary search trees (aa-tree) in hashed shards. This allows for the map to
// grow evenly, without allocations, and performing much faster than using a
// single BST.
////////////////////////////////////////////////////////////////////////////////

#ifndef COMAP_NSHARDS
#define COMAP_NSHARDS 512
#endif

struct comap {
    struct coroutine *roots[COMAP_NSHARDS];
    int count;
};

#define comap_getaat(co) \
    (&map->roots[mix13((co)->id) & (COMAP_NSHARDS-1)])

static struct coroutine *comap_insert(struct comap *map, struct coroutine *co) {
    struct coroutine *prev = all_insert(comap_getaat(co), co);
    map->count++;
    return prev;
}

static struct coroutine *comap_search(struct comap *map, struct coroutine *key){
    return all_search(comap_getaat(key), key);
}

static struct coroutine *comap_delete(struct comap *map, struct coroutine *key){
    struct coroutine *prev = all_delete(comap_getaat(key), key);
    map->count--;
    return prev;
}

////////////////////////////////////////////////////////////////////////////////
// evmap - A hashmap-style structure that stores fd/events using multiple
// binary search trees (aa-tree) in hashed shards. This allows for the map to
// grow evenly, without allocations, and performing much faster than using a
// single BST.
////////////////////////////////////////////////////////////////////////////////

#ifndef EVMAP_NSHARDS
#define EVMAP_NSHARDS 512
#endif

struct evmap {
    struct coroutine *roots[EVMAP_NSHARDS];
    int count;
};

#define evmap_getaat(co) (&map->roots[mix13(co->evfd) & (EVMAP_NSHARDS-1)])

static struct coroutine *evmap_insert(struct evmap *map, struct coroutine *co) {
    struct coroutine *prev = evaat_insert(evmap_getaat(co), co);
    map->count++;
    return prev;
}

static struct coroutine *evmap_iter(struct evmap *map, struct coroutine *key){
    return evaat_iter(evmap_getaat(key), key);
}

static struct coroutine *evmap_next(struct evmap *map, struct coroutine *key){
    return evaat_next(evmap_getaat(key), key);
}

static struct coroutine *evmap_delete(struct evmap *map, struct coroutine *key){
    struct coroutine *prev = evaat_delete(evmap_getaat(key), key);
    map->count--;
    return prev;
}



#ifndef NECO_TESTING
static
#endif
__thread int neco_gai_errno = 0;

/// Get the last error from a neco_getaddrinfo() call.
///
/// See the [man page](https://man.freebsd.org/cgi/man.cgi?query=gai_strerror) 
/// for a list of errors.
int neco_gai_lasterr(void) {
    return neco_gai_errno;
}

// For atomically incrementing the runtime ID.
// The runtime ID is only used to protect channels from being accicentally
// used by another thread. 
static atomic_int_fast64_t next_runtime_id = 1;

// The neco runtime
struct runtime {
    int64_t id;                    // unique runtime identifier
    struct stack_mgr stkmgr;       // stack memory manager
    int64_t coid;                  // unique coroutine id incrementer
    struct coroutine *costarter;   // current coroutine starter
    struct comap all;              // all running coroutines.
    struct coroutine *deadlines;   // paused coroutines (aat root)
    size_t ndeadlines;             // total number of paused coroutines
    size_t ntotal;                 // total number of coroutines ever created
    size_t nsleepers;
    size_t nlocked;
    size_t nreceivers;
    size_t nsenders;
    size_t nwaitgroupers;          // number of waitgroup blocked coroutines
    size_t ncondwaiters;           // number of cond blocked coroutines
    size_t nworkers;               // number of background workers
    size_t nsuspended;             // number of suspended coroutines

    struct evmap evwaiters;        // coroutines waiting on events (aat root)
    size_t nevwaiters;

    // list of coroutines waiting to be resumed by the scheduler
    int nresumers;
    struct colist resumers;

    // coroutine pool (reusables)
    int npool;                     // number of coroutines in a pool
    struct colist pool;            // pool lists of coroutines for each size

    int qfd;                       // queue file descriptor (epoll or kqueue)
    int64_t qfdcreated;            // when the queue was created

    // zerochan pool (reusables)
    struct neco_chan **zchanpool;  // pool of zero sized channels
    int zchanpoollen;              // number of zero sized channels in pool
    int zchanpoolcap;              // capacity of pool

    struct colist sigwaiters;      // signal waiting coroutines
    size_t nsigwaiters;
    uint32_t sigmask;              // signal mask from handler
    int sigwatchers[32];           // signals being watched, reference counted
    bool mainthread;               // running on the main thread
    int sigqueue[32];              // queue of signals waiting for delivery
#ifndef _WIN32
    struct sigaction sigold[32];   // previous signal handlers, for restoring
#endif
    char *sigstack;                // stack for signals
    int sigcrashed;

    int64_t rand_seed;             // used for PRNG (non-crypto)
#ifdef __linux__
    void *libbsd_handle;           // used for arc4random_buf
    void (*arc4random_buf)(void *, size_t);
#endif

#ifndef NECO_NOWORKERS
    struct worker *worker;
    pthread_mutex_t iomu;
    struct colist iolist;
    size_t niowaiters;
#endif

    unsigned int burstcount;
};

#define RUNTIME_DEFAULTS (struct runtime) { 0 }

static __thread struct runtime *rt = NULL;

static void rt_release(void) {
    free0(rt);
    rt = NULL;
}

// coself returns the currently running coroutine.
static struct coroutine *coself(void) {
    return (struct coroutine*)sco_udata();
}

noinline
static void coexit(bool async);

// Use coyield() instead of sco_yield() in neco so that an async cancelation 
// can be detected
static void coyield(void) {
    sco_yield();
    struct coroutine *co = coself();
    if (co->canceled && co->canceltype == NECO_CANCEL_ASYNC) {
        coexit(true);
    }
}

// Schedule a coroutine to resume at the next pause phase.
// It's expected that the coroutine is currently paused.
// The difference between sched_resume(co) and sco_resume(co->id) is that with
// sched_resume() the current coroutine is not yielded by the call, allowing
// for multiple coroutines to be scheduled as a batch. While sco_resume() will 
// immediately yield the current coroutine to the provided coroutine.
static void sched_resume(struct coroutine *co) {
    colist_push_back(&rt->resumers, co);
    rt->nresumers++;
}

static void yield_for_sched_resume(void) {
    // Two yields may be required. The first will resume the paused coroutines
    // that are in the resumers queue. The second yield ensures that the
    // current coroutine waits until _after_ the others have resumed.
    if (rt->nresumers > 0) {
        coyield();
    }
    coyield();
}

static struct coroutine *evexists(int fd, enum evkind kind) {
    struct coroutine *key = &(struct coroutine){ .evfd = fd, .evkind = kind };
    struct coroutine *iter = evmap_iter(&rt->evwaiters, key);
    return iter && iter->evfd == fd && iter->evkind == kind ? iter : NULL;
}

#if defined(_WIN32)
#include <winuser.h>
static int is_main_thread(void) {
    return IsGUIThread(false);
}
#elif defined(__linux__)
static int is_main_thread(void) {
    return getpid() == (pid_t)syscall(SYS_gettid);
}
#elif defined(__EMSCRIPTEN__) 
int gettid(void);
static int is_main_thread(void) {
    return getpid() == gettid();
}
#else
int pthread_main_np(void);
static int is_main_thread(void) {
    return pthread_main_np();
}
#endif

static int _is_main_thread_(void) {
    if (!rt) {
        return NECO_PERM;
    }
    return is_main_thread();
}

/// Test if coroutine is running on main thread.
/// @return 1 for true, 0 for false, or a negative value for error.
int neco_is_main_thread(void) {
    int ret = _is_main_thread_();
    error_guard(ret);
    return ret;
}

static bool costackget(struct coroutine *co) {
    return stack_get0(&rt->stkmgr, &co->stack) == 0;
}

static void costackfree(struct coroutine *co) {
    stack_put(&rt->stkmgr, &co->stack);
}

static size_t costacksize(struct coroutine *co) {
    return stack_size(&co->stack);
}

static void *costackaddr(struct coroutine *co) {
    return stack_addr(&co->stack);
}

// Create a new coroutines with the provided coroutine function and stack size.
// Returns NULL if out of memory.
static struct coroutine *coroutine_new(void) {
    struct coroutine *co = malloc0(sizeof(struct coroutine));
    if (!co) {
        return NULL;
    }
    memset(co, 0, sizeof(struct coroutine));
    co->kind = COROUTINE;
    if (!costackget(co)) {
        free0(co);
        return NULL;
    }
    co->next = co;
    co->prev = co;
    colist_init(&co->cancellist);
    colist_init(&co->joinlist);

    // Return the coroutine in a non-running state. 
    return co;
}

static void cofreeargs(struct coroutine *co) {
    if (co->argv && co->argv != co->aargv) {
        free0(co->argv);
    }
    co->argc = 0;
    co->argv = NULL;
}

static struct neco_chan *chan_fastmake(size_t data_size, size_t capacity, 
    bool as_generator);
static void chan_fastrelease(struct neco_chan *chan);
static void chan_fastretain(struct neco_chan *chan);

static void coroutine_free(struct coroutine *co) {
    if (co) {
        cofreeargs(co);
        costackfree(co);
        free0(co);
    }
}


#ifndef NECO_TESTING

static __thread bool immediate_exit = false;
static __thread int immediate_exit_code = 0;

void __neco_exit_prog(int code) {
    immediate_exit = true;
    immediate_exit_code = code;
    neco_exit();
}
#else
void __neco_exit_prog(int code) {
    (void)code;
    // Cannot exit program in testing
}
#endif

static void cleanup(void *stack, size_t stack_size, void *udata) {
    // The coroutine is finished. Cleanup resources
    (void)stack; (void)stack_size;
#ifndef NECO_TESTING
    if (immediate_exit) {
        _Exit(immediate_exit_code);
    }
#endif
    struct coroutine *co = udata;
#ifndef NECO_NOPOOL
    colist_push_back(&rt->pool, co);
    rt->npool++;
#else
    coroutine_free(co);
#endif
}

static void coentry(void *udata) {
    struct coroutine *co = udata;
    co->id = sco_id();
    if (rt->costarter) {
        rt->costarter->lastid = co->id;
        co->starterid = rt->costarter->id;
    } else {
        co->starterid = 0;
    }
    rt->ntotal++;
    comap_insert(&rt->all, co);
    if (co->coroutine) {
        co->coroutine(co->argc, co->argv);
    }
    coexit(false);
}


static int start(void(*coroutine)(int, void**), int argc, va_list *args,
    void *argv[], neco_gen **gen, size_t gen_data_size)
{
    struct coroutine *co;
#ifndef NECO_NOPOOL
    co = colist_pop_front(&rt->pool);
    if (co) {
        rt->npool--;
        co->pool_ts = 0;
    } else {
        co = coroutine_new();
    }
#else
    co = coroutine_new();
#endif
    if (!co) {
        goto fail;
    }
    co->coroutine = coroutine;
    co->canceltype = env_canceltype;
    co->cancelstate = env_cancelstate;

    if (gen) {
        co->gen = chan_fastmake(gen_data_size, 0, true);
        if (!co->gen) {
            goto fail;
        }
        chan_fastretain(co->gen);
        *gen = (neco_gen*)co->gen;
    }

    // set the arguments
    if (argc <= (int)(sizeof(co->aargv)/sizeof(void*))) {
        co->argv = co->aargv;
    } else {
        co->argv = malloc0((size_t)argc * sizeof(void*));
        if (!co->argv) {
            goto fail;
        }
    }
    co->argc = argc;
    for (int i = 0; i < argc; i++) {
        co->argv[i] = args ? va_arg(*args, void*) : argv[i];
    }
    struct sco_desc desc = {
        .stack = costackaddr(co),
        .stack_size = costacksize(co),
        .entry = coentry,
        .cleanup = cleanup,
        .udata = co,
    };
    rt->costarter = coself();
    sco_start(&desc);
    return NECO_OK;
fail:
    if (co) {
        if (co->gen) {
            chan_fastrelease(co->gen); // once for fastmake
            chan_fastrelease(co->gen); // once for fastretain
        }
        coroutine_free(co);
    }
    return NECO_NOMEM;
}

#ifndef NECO_NOSIGNALS

// These are signals that are allowed to be "watched" by a coroutine using
// the neco_signal_watch() and neco_signal_wait() operations.
static const int ALLOWED_SIGNALS[] = { 
    SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGPIPE, SIGUSR1, SIGUSR2, SIGALRM
};

// These are signals that trigger faults, such as a stack overflow or an 
// invalid instruction.
// Notice that SIGABRT is intentionally not TRAPPED or ALLOWED. This is because
// it's been observed that trapping "Abort: 6" on Apple Silicon in Rosetta mode
// can lead to a dead lock that requires a "kill -9" to exit.
// Similar to issue: https://github.com/JuliaLang/julia/issues/42398
static const int TRAPPED_SIGNALS[] = {
    SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP
};

static void sigallowhandler(int signo) {
    if (rt->sigwatchers[signo] == 0) {
        // No coroutines are watching for this signal. Without an interested 
        // outside party, the program should exit immediately.
        if (signo != SIGPIPE) {
            sigexitnow(signo);
        }
        return;
    }
    // The Neco runtime has a signal watching for this specific signal.
    // Queue the signal and let the runtime take over.
    rt->sigmask |= (UINT32_C(1) << signo);
    rt->sigqueue[signo]++;
}

static void crash_and_exit(int signo) {
#ifndef NECO_TESTING
    if (rt->mainthread) {
        fprintf(stderr, "\n=== Crash on main thread ===\n");
    } else {
        fprintf(stderr, "\n=== Crash on child thread ===\n");
    }
    print_stacktrace(0, true);
    fprintf(stderr, "\n");
#endif
    sigexitnow(signo);
}

#ifdef NECO_TESTING
// During testing, allow for resetting the last crash.
void neco_reset_sigcrashed(void) {
    if (rt) {
        rt->sigcrashed = 0;
    }
}
#endif

static void sighandler(int signo, siginfo_t *info, void *ptr) {
    (void)info; (void)ptr;
    must(signo > 0 && signo < 32);
    if (rt->sigcrashed) {
        sigexitnow(rt->sigcrashed);
        return;
    }
    for (size_t i = 0; i < sizeof(ALLOWED_SIGNALS)/sizeof(int); i++) {
        if (signo == ALLOWED_SIGNALS[i]) {
            sigallowhandler(signo);
            return;
        }
    }
    for (size_t i = 0; i < sizeof(TRAPPED_SIGNALS)/sizeof(int); i++) {
        if (signo == TRAPPED_SIGNALS[i]) {
            // Print the crash and exit.
            rt->sigcrashed = signo;
            crash_and_exit(signo);
            return;
        }
    }
}
#endif

// Set up a custom handler for certain signals
static int rt_handle_signals(void) {
#ifndef NECO_NOSIGNALS
    if (!rt->mainthread) {
        // Only handle signals on the main thread.
        return NECO_OK;
    }
    static struct sigaction act = { 0 };
    sigemptyset(&act.sa_mask);
    act.sa_sigaction = &sighandler;
    act.sa_flags = SA_SIGINFO;
    rt->sigstack = malloc0(NECO_SIGSTKSZ);
    if (!rt->sigstack) {
        return NECO_NOMEM;
    }
#if NECO_SIGSTKSZ > 0
    act.sa_flags |= SA_ONSTACK;
    stack_t ss = {
        .ss_sp = rt->sigstack,
        .ss_size = NECO_SIGSTKSZ,
        .ss_flags = 0,
    };
    must(sigaltstack(&ss, NULL) == 0);
#endif
    for (size_t i = 0; i < sizeof(ALLOWED_SIGNALS)/sizeof(int); i++) {
        int signo = ALLOWED_SIGNALS[i];
        must(sigaction(signo, &act, &rt->sigold[signo]) == 0);
    }
    for (size_t i = 0; i < sizeof(TRAPPED_SIGNALS)/sizeof(int); i++) {
        int signo = TRAPPED_SIGNALS[i];
        must(sigaction(signo, &act, &rt->sigold[signo]) == 0);
    }
#endif
    return NECO_OK;
}

// Restore the signal handlers
static void rt_restore_signal_handlers(void) {
#ifndef NECO_NOSIGNALS
    if (!rt->mainthread) {
        // Only handle signals on the main thread.
        return;
    }
    if (!rt->sigstack) {
        // Allocation failed during the initialization.
        return;
    }
    free0(rt->sigstack);
    for (size_t i = 0; i < sizeof(ALLOWED_SIGNALS)/sizeof(int); i++) {
        int signo = ALLOWED_SIGNALS[i];
        must(sigaction(signo, &rt->sigold[signo], NULL) == 0);
    }
    for (size_t i = 0; i < sizeof(TRAPPED_SIGNALS)/sizeof(int); i++) {
        int signo = TRAPPED_SIGNALS[i];
        must(sigaction(signo, &rt->sigold[signo], NULL) == 0);
    }
#endif
}

static void rt_sched_signal_step(void) {
    for (int signo = 0; signo < 32; signo++) {
        if (rt->sigqueue[signo] == 0) {
            continue;
        } else if (rt->sigwatchers[signo] == 0) {
            // The signal is no longer being watched.
            rt->sigqueue[signo]--;
            if (rt->sigqueue[signo] == 0) {
                rt->sigmask &= ~(UINT32_C(1) << signo);
            }
            if (signo != SIGPIPE) {
                sigexitnow(signo);
            }
        } else {
            struct coroutine *co = rt->sigwaiters.head.next;
            while (co != (struct coroutine*)&rt->sigwaiters.tail) {
                struct coroutine *next = co->next;
                if (co->sigmask & (UINT32_C(1) << signo)) {
                    // A coroutine is waiting on this signal.
                    // Assign the signal to its sigmask and resume
                    // the coroutine.
                    co->sigmask = (uint32_t)signo;
                    rt->sigqueue[signo]--;
                    if (rt->sigqueue[signo] == 0) {
                        rt->sigmask &= ~(UINT32_C(1) << signo);
                    }
                    sco_resume(co->id);
                    next = (struct coroutine*)&rt->sigwaiters.tail;
                }
                co = next;
            }
        }
        break;
    }
}

#define NEVENTS 16

static void rt_sched_event_step(int64_t timeout) {
    (void)timeout;
#if defined(NECO_POLL_EPOLL) 
    struct epoll_event evs[NEVENTS];
    int timeout_ms = (int)(timeout/NECO_MILLISECOND);
    int nevents = epoll_wait(rt->qfd, evs, NEVENTS, timeout_ms);
#elif defined(NECO_POLL_KQUEUE) 
    struct kevent evs[NEVENTS];
    struct timespec timeoutspec = { .tv_nsec = timeout };
    int nevents = kevent0(rt->qfd, NULL, 0, evs, NEVENTS, &timeoutspec);
#else
    int nevents = 0;
#endif
    // A sane kqueue/epoll instance can only ever return the number of
    // events or EINTR when waiting on events.
    // If the queue was interrupted, the error can be safely ignored because
    // the sighandler() will responsibly manage the incoming signals, which
    // are then dealt with by this function, right after the event loop.
    must(nevents != -1 || errno == EINTR);

    // Consume each event.
    for (int i = 0; i < nevents; i++) {
#if defined(NECO_POLL_EPOLL)
        int fd = evs[i].data.fd;
        bool read = evs[i].events & EPOLLIN;
        bool write = evs[i].events & EPOLLOUT;
#elif defined(NECO_POLL_KQUEUE)
        int fd = (int)evs[i].ident;
        bool read = evs[i].filter == EVFILT_READ;
        bool write = evs[i].filter == EVFILT_WRITE;
#else
        int fd = 0;
        bool read = false;
        bool write = false;
#endif
        // For linux, a single event may describe both a read and write status
        // for a file descriptor so we have to break them apart and deal with
        // each one independently.
        while (read || write) {
            enum evkind kind;
            if (read) {
                kind = EVREAD;
                read = false;
            } else {
                kind = EVWRITE;
                write = false;
            }
            // Now that we have an event type (read or write) and a file
            // descriptor, it's time to wake up the coroutines that are waiting
            // on that event.
            struct coroutine *key = &(struct coroutine) { 
                .evfd = fd, 
                .evkind = kind,
            };
            struct coroutine *co = evmap_iter(&rt->evwaiters, key);
            while (co && co->evfd == fd && co->evkind == kind) {
                sco_resume(co->id);
                co = evmap_next(&rt->evwaiters, co);
            }
        }
    }
}

// Adds a and b, clamping overflows to INT64_MIN or INT64_MAX
TESTING_EXTERN(static)
int64_t i64_add_clamp(int64_t a, int64_t b) {
    if (!((a ^ b) < 0)) { // Opposite signs can't overflow
        if (a > 0) {
            if (b > INT64_MAX - a) {
                return INT64_MAX;
            }
        } else if (b < INT64_MIN - a) {
            return INT64_MIN;
        }
    }
    return a + b;
}

#define MAX_TIMEOUT 500000000 // 500 ms

// Handle paused couroutines.
static void rt_sched_paused_step(void) {
    // Resume all the paused coroutines that are waiting for immediate 
    // attention.
    struct coroutine *co = colist_pop_front(&rt->resumers);
    while (co) {
        sco_resume(co->id);
        rt->nresumers--;
        co = colist_pop_front(&rt->resumers);
    }

    // Calculate the minimum timeout for this step.
    int64_t timeout = MAX_TIMEOUT;
    if (timeout > 0 && sco_info_scheduled() > 0) {
        // No timeout when there is at least one yielding coroutine or
        // when there are pending signals.
        timeout = 0;
    }
    if (timeout > 0 && rt->sigmask) {
        // There are pending signals.
        // Do not wait to deliver them.
        timeout = 0;
    }
    if (timeout > 0 && rt->ndeadlines > 0) {
        // There's at least one deadline coroutine. Use the one with
        // the minimum 'deadline' value to determine the timeout.
        int64_t min_deadline = dlqueue_first(&rt->deadlines)->deadline;
        int64_t timeout0 = i64_add_clamp(min_deadline, -getnow());
        if (timeout0 < timeout) {
            timeout = timeout0;
        }
    }
    timeout = CLAMP(timeout, 0, MAX_TIMEOUT);
    
#ifndef NECO_NOWORKERS
    if (rt->niowaiters > 0) {
        timeout = 0;
        while (1) {
            pthread_mutex_lock(&rt->iomu);
            struct coroutine *co = colist_pop_front(&rt->iolist);
            pthread_mutex_unlock(&rt->iomu);
            if (!co) {
                break;
            }
            sco_resume(co->id);
        }
    }
#endif

    if (rt->nevwaiters > 0) {
        // Event waiters need their own logic.
        rt_sched_event_step(timeout);
    } else if (timeout > 0) {
        // There are sleepers (or signal waiters), but no event waiters.
        // Therefore no need for an event queue. We can just do a simple sleep
        // using the timeout provided the sleeper with the minimum deadline.
        // A signal will interrupt this sleeper if it's running on the main
        // thread.
        nanosleep(&(struct timespec){
            .tv_sec = timeout/1000000000,
            .tv_nsec = timeout%1000000000,
        }, 0);
    }

    // Handle pending signals from the sighandler().
    if (rt->sigmask) {
        // There's at least one signal pending.
        rt_sched_signal_step();
    }

    // Check for deadliners and wake them up. 
    int64_t now = getnow();
    co = dlqueue_first(&rt->deadlines);
    while (co && co->deadline < now) {
        // Deadline has been reached. Resume the coroutine
        co->deadlined = true;
        sco_resume(co->id);
        co = dlqueue_next(&rt->deadlines, co);
    }
}

// Resource collection step
static void rt_rc_step(void) {
    int64_t now = getnow();
    if (rt->nevwaiters == 0 && rt->qfd > 0) {
        if (now - rt->qfdcreated > NECO_MILLISECOND * 100) {
            // Close the event queue file descriptor if it's no longer needed.
            // When the queue goes unused for more than 100 ms it will
            // automatically be closed.
            close(rt->qfd);
            rt->qfd = 0;
        }
    }
    // Deal with coroutine pools.
    if (rt->npool > 0) {
        // First assign timestamps to newly pooled coroutines, then remove
        // coroutines that have been at waiting in the pool more than 100 ms.
        struct coroutine *co = rt->pool.tail.prev;
        while (co != (struct coroutine*)&rt->pool.head && co->pool_ts == 0) {
            co->pool_ts = now;
            co = co->prev;
        }
        while (rt->pool.head.next != (struct coroutine*)&rt->pool.tail) {
            co = (struct coroutine*)rt->pool.head.next;
            if (now - co->pool_ts < NECO_MILLISECOND * 100) {
                break;
            }
            remove_from_list(co);
            rt->npool--;
            coroutine_free(co);
        }
    }
}

static int rt_scheduler(void) {
    // The schdeduler is just a standard sco runtime loop. When thare are 
    // paused coroutines the paused step executes. That's where the real 
    // magic happens.
    int ret = NECO_OK;
    while (sco_active()) {
        if (sco_info_paused() > 0) {
            rt_sched_paused_step();
        }
        rt_rc_step();
        sco_resume(0);
    }
    // Cleanup extra resources
    if (rt->qfd) {
        close(rt->qfd);
    }
    struct coroutine *co = colist_pop_front(&rt->pool);
    while (co) {
        coroutine_free(co);
        co = colist_pop_front(&rt->pool);
    }
    return ret;
}

static void rt_freezchanpool(void) {
    for (int i = 0; i < rt->zchanpoollen; i++) {
        free0(rt->zchanpool[i]);
    }
    if (rt->zchanpool) {
        free0(rt->zchanpool);
    }
}

static struct stack_opts stack_opts_make(void) {
    return (struct stack_opts) { 
        .stacksz = NECO_STACKSIZE,
        .defcap = NECO_DEFCAP,
        .maxcap = NECO_MAXCAP,
        .gapsz = NECO_GAPSIZE,
#ifdef NECO_USEGUARDS
        .useguards = true,
#endif
#ifdef NECO_NOSTACKFREELIST
        .nostackfreelist = true,
#endif
#ifdef NECO_USEHEAPSTACK
        .onlymalloc = true,
#endif
    };
}

static void init_networking(void) {
#ifdef _WIN32
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(2, 2);
    WSAStartup(wVersionRequested, &wsaData);
#endif
}

static void rt_release_dlhandles(void) {
#if __linux__
    if (rt->libbsd_handle) {
        dlclose(rt->libbsd_handle);
    }
#endif
}

static int run(void(*coroutine)(int, void**), int nargs, va_list *args,
    void *argv[])
{
    init_networking();
    rt = malloc0(sizeof(struct runtime));
    if (!rt) {
        return NECO_NOMEM;
    }
    *rt = RUNTIME_DEFAULTS;
    rt->mainthread = is_main_thread();
    rt->id = atomic_fetch_add(&next_runtime_id, 1);


    struct stack_opts sopts = stack_opts_make();
    stack_mgr_init(&rt->stkmgr, &sopts);

    colist_init(&rt->sigwaiters);
    colist_init(&rt->pool);
    colist_init(&rt->resumers);

    // Initialize the signal handlers
    int ret = rt_handle_signals();
    if (ret != NECO_OK) {
        goto fail;
    }

#ifndef NECO_NOWORKERS
    struct worker_opts wopts = {
        .max_threads = NECO_MAXWORKERS,
        .max_thread_entries = NECO_MAXRINGSIZE,
        .malloc = malloc0,
        .free = free0,
    };
    rt->worker = worker_new(&wopts);
    if (!rt->worker) {
        ret = NECO_NOMEM;
        goto fail;
    }
    pthread_mutex_init(&rt->iomu, 0);
    colist_init(&rt->iolist);
#endif


    // Start the main coroutine. Actually, it's just queued to run first.
    ret = start(coroutine, nargs, args, argv, 0, 0);
    if (ret != NECO_OK) {
        goto fail;
    }

    // Run the scheduler
    ret = rt_scheduler();

fail:
    stack_mgr_destroy(&rt->stkmgr);
    rt_freezchanpool();
    rt_restore_signal_handlers();
    rt_release_dlhandles();
#ifndef NECO_NOWORKERS
    worker_free(rt->worker);
#endif
    rt_release();
    return ret;
}

static int startv(void(*coroutine)(int argc, void *argv[]), int argc, 
    va_list *args, void *argv[], neco_gen **gen, size_t gen_data_size)
{
    if (!coroutine || argc < 0) {
        return NECO_INVAL;
    }
    int ret;
    if (!rt) {
        ret = run(coroutine, argc, args, argv);
    } else {
        ret = start(coroutine, argc, args, argv, gen, gen_data_size);
    }
    return ret;
}

/// Starts a new coroutine.
///
/// If this is the first coroutine started for the program (or thread) then
/// this will also create a neco runtime scheduler which blocks until the
/// provided coroutine and all of its subsequent child coroutines finish.
///
/// **Example**
///
/// ```
/// // Here we'll start a coroutine that prints "hello world".
///
/// void coroutine(int argc, void *argv[]) {
///     char *msg = argv[0];
///     printf("%s\n", msg);
/// }
///
/// neco_start(coroutine, 1, "hello world");
/// ```
///
/// @param coroutine The coroutine that will soon run
/// @param argc Number of arguments
/// @param ... Arguments passed to the coroutine
/// @return NECO_OK Success
/// @return NECO_NOMEM The system lacked the necessary resources
/// @return NECO_INVAL An invalid parameter was provided
int neco_start(void(*coroutine)(int argc, void *argv[]), int argc, ...) {
    va_list args;
    va_start(args, argc);
    int ret = startv(coroutine, argc, &args, 0, 0, 0);
    va_end(args);
    error_guard(ret);
    return ret;
}


/// Starts a new coroutine using an array for arguments.
/// @see neco_start
int neco_startv(void(*coroutine)(int argc, void *argv[]), int argc, 
    void *argv[])
{
    int ret = startv(coroutine, argc, 0, argv, 0, 0);
    error_guard(ret);
    return ret;
}

static int yield(void) {
    if (!rt) {
        return NECO_PERM;
    }
    coyield();
    return NECO_OK;
}

/// Cause the calling coroutine to relinquish the CPU.
/// The coroutine is moved to the end of the queue.
/// @return NECO_OK Success
/// @return NECO_PERM Operation called outside of a coroutine
int neco_yield(void) {
    int ret = yield();
    error_guard(ret);
    return ret;
}

// cofind returns the coroutine belonging to the provided identifier.
static struct coroutine *cofind(int64_t id) {
    struct coroutine *co = NULL;
    if (rt) {
        co = comap_search(&rt->all, &(struct coroutine){ .id = id });
    }
    return co;
}

// pause the currently running coroutine with the provided deadline.
static void copause(int64_t deadline) {
    struct coroutine *co = coself();
    // Cannot pause an already canceled or deadlined coroutine.
    if (!co->canceled && !co->deadlined) {
        co->deadline = deadline;
        if (co->deadline < INT64_MAX) {
            dlqueue_insert(&rt->deadlines, co);
            rt->ndeadlines++;
        }
        co->paused = true;
        sco_pause();
        co->paused = false;
        if (co->deadline < INT64_MAX) {
            dlqueue_delete(&rt->deadlines, co);
            rt->ndeadlines--;
        }
        co->deadline = 0;
    }
}

static int sleep0(int64_t deadline) {
    struct coroutine *co = coself();
    rt->nsleepers++;
    copause(deadline);
    rt->nsleepers--;
    int ret = co->canceled ? NECO_CANCELED : NECO_OK;
    co->canceled = false;
    co->deadlined = false;
    return ret;
}

static int sleep_dl(int64_t deadline) {
    if (!rt) {
        return NECO_PERM;
    } else if (getnow() > deadline) {
        return NECO_TIMEDOUT;
    }
    return sleep0(deadline);
}

/// Same as neco_sleep() but with a deadline parameter. 
int neco_sleep_dl(int64_t deadline) {
    int ret = sleep_dl(deadline);
    async_error_guard(ret);
    return ret;
}

/// Causes the calling coroutine to sleep until the number of specified
/// nanoseconds have elapsed.
/// @param nanosecs duration nanoseconds
/// @return NECO_OK Coroutine slept until nanosecs elapsed
/// @return NECO_TIMEDOUT nanosecs is a negative number
/// @return NECO_CANCELED Operation canceled 
/// @return NECO_PERM Operation called outside of a coroutine
/// @see neco_sleep_dl()
int neco_sleep(int64_t nanosecs) {
    return neco_sleep_dl(i64_add_clamp(getnow(), nanosecs));
}

static int64_t getid(void) {
    if (!rt) {
        return NECO_PERM;
    }
    return sco_id();
}

/// Returns the identifier for the currently running coroutine.
///
/// This value is guaranteed to be unique for the duration of the program.
/// @return The coroutine identifier
/// @return NECO_PERM Operation called outside of a coroutine
int64_t neco_getid(void) {
    int64_t ret = getid();
    error_guard(ret);
    return ret;
}

static int64_t lastid(void) {
    if (!rt) {
        return NECO_PERM;
    }
    return coself()->lastid;
}

/// Returns the identifier for the coroutine started by the current coroutine.
///
/// For example, here a coroutine is started and its identifer is then
/// retreived.
///
/// ```
/// neco_start(coroutine, 0);
/// int64_t id = neco_lastid();
/// ```
///
/// @return A coroutine identifier, or zero if the current coroutine has not 
/// yet started any coroutines.
/// @return NECO_PERM Operation called outside of a coroutine
int64_t neco_lastid(void) {
    int64_t ret = lastid();
    error_guard(ret);
    return ret;
}

static int64_t starterid(void) {
    if (!rt) {
        return NECO_PERM;
    }
    return coself()->starterid;
}

// checkdl checks if the coroutine has been canceled or timedout, resets the
// co->canceled and co->deadlined flags to false, and returns the appropriate
// error code. This is typically used from *_dl operations.
static int checkdl(struct coroutine *co, int64_t deadline) {
    if (!co->canceled && !co->deadlined && deadline == INT64_MAX) {
        // Most cases.
        return NECO_OK;
    }
    bool canceled = co->canceled;
    bool deadlined = co->deadlined;
    co->canceled = false;
    co->deadlined = false;
    if (!canceled && !deadlined && deadline < INT64_MAX && getnow() > deadline){
        deadlined = true;
    }
    return canceled ? NECO_CANCELED : deadlined ? NECO_TIMEDOUT : NECO_OK;
}

/// Get the identifier for the coroutine that started the current coroutine.
///
/// ```
/// void child_coroutine(int argc, void *argv[]) {
///     int parent_id = neco_starterid();
///     // The parent_id is equal as the neco_getid() from the parent_coroutine
///     // below. 
/// }
///
/// void parent_coroutine(int argc, void *argv[]) {
///    int id = neco_getid();
///    neco_start(child_coroutine, 0);
/// }
/// ```
/// @return A coroutine identifier, or zero if the coroutine is the first
/// coroutine started.
int64_t neco_starterid(void) {
    int64_t ret = starterid();
    error_guard(ret);
    return ret;
}

static int cancel_dl(int64_t id, int64_t deadline) {
    struct coroutine *co = coself();
    if (!co) {
        return NECO_PERM;
    }
    struct coroutine *cotarg;
    while (1) {
        cotarg = cofind(id);
        if (!cotarg) {
            return NECO_NOTFOUND;
        }
        int ret = checkdl(co, deadline);
        if (ret != NECO_OK) {
            return ret;
        }
        if (cotarg->cancelstate == NECO_CANCEL_ENABLE) {
            // Coroutine was found and its cancel state is enabled.
            // Set the cancel flag and wake it up.
            cotarg->canceled = true;
            sco_resume(id);
            coyield();
            return NECO_OK;
        }
        // Target coroutine is blocking their cancel state.
        // Wait for it to be released.
        colist_push_back(&cotarg->cancellist, co);
        cotarg->ncancellist++;
        copause(deadline);
        remove_from_list(co);
        cotarg->ncancellist--;
    }
}

int neco_cancel_dl(int64_t id, int64_t deadline) {
    int ret = cancel_dl(id, deadline);
    async_error_guard(ret);
    return ret;
}

int neco_cancel(int64_t id) {
    return neco_cancel_dl(id, INT64_MAX);
}

////////////////////////////////////////////////////////////////////////////////
// signals
////////////////////////////////////////////////////////////////////////////////

static int signal_watch(int signo) {
    if (signo < 1 || signo > 31) {
        return NECO_INVAL;
    }
    struct coroutine *co = coself();
    if (!co || !rt->mainthread) {
        return NECO_PERM;
    }
    if ((co->sigwatch & (UINT32_C(1) << signo)) == 0) {
        co->sigwatch |= UINT32_C(1) << signo;
        rt->sigwatchers[signo]++;
    }
    return NECO_OK;
}


/// Have the current coroutine watch for a signal.
///
/// This can be used to intercept or ignore signals.
///
/// Signals that can be watched: SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGPIPE,
/// SIGUSR1, SIGUSR2, SIGALRM
///
/// Signals that _can not_ be watched: SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP
///
/// **Example**
///
/// ```
/// // Set up this coroutine to watich for the SIGINT (Ctrl-C) and SIGQUIT
/// // (Ctrl-\) signals.
/// neco_signal_watch(SIGINT);
/// neco_signal_watch(SIGQUIT);
///
/// printf("Waiting for Ctrl-C or Ctrl-\\ signals...\n");
/// int sig = neco_signal_wait();
/// if (sig == SIGINT) {
///     printf("\nReceived Ctrl-C\n");
/// } else if (sig == SIGQUIT) {
///     printf("\nReceived Ctrl-\\\n");
/// }
///
/// // The neco_signal_unwatch is used to stop watching.
/// neco_signal_unwatch(SIGINT);
/// neco_signal_unwatch(SIGQUIT);
/// ```
///
/// @param signo The signal number
/// @return NECO_OK Success
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @see Signals
int neco_signal_watch(int signo) {
    int ret = signal_watch(signo);
    error_guard(ret);
    return ret;
}

static int signal_unwatch(int signo) {
    if (signo < 1 || signo > 31) {
        return NECO_INVAL;
    }
    struct coroutine *co = coself();
    if (!co || !rt->mainthread) {
        return NECO_PERM;
    }
    if ((co->sigwatch & (UINT32_C(1) << signo)) != 0) {
        co->sigwatch &= ~(UINT32_C(1) << signo);
        rt->sigwatchers[signo]--;
    }
    return NECO_OK;
}

/// Stop watching for a siganl to arrive
/// @param signo The signal number
/// @return NECO_OK on success or an error
int neco_signal_unwatch(int signo) {
    int ret = signal_unwatch(signo);
    error_guard(ret);
    return ret;
}

static int signal_wait_dl(int64_t deadline) {
    struct coroutine *co = coself();
    if (!rt || !rt->mainthread) {
        return NECO_PERM;
    } 
    if (co->sigwatch == 0) {
        return NECO_NOSIGWATCH;
    }
    if (co->canceled) {
        co->canceled = false;
        return NECO_CANCELED;
    }
    co->sigmask = co->sigwatch;
    colist_push_back(&rt->sigwaiters, co);
    rt->nsigwaiters++;
    copause(deadline);
    remove_from_list(co);
    rt->nsigwaiters--;
    int signo = (int)co->sigmask;
    co->sigmask = 0;
    int ret = checkdl(co, INT64_MAX);
    return ret == NECO_OK ? signo : ret;
}

/// Same as neco_signal_wait() but with a deadline parameter. 
int neco_signal_wait_dl(int64_t deadline) {
    int ret = signal_wait_dl(deadline);
    async_error_guard(ret);
    return ret;
}

/// Wait for a signal to arrive.
/// @return A signal number or an error.
/// @return NECO_PERM
/// @return NECO_NOSIGWATCH if not currently watching for signals.
/// @return NECO_CANCELED
/// @see Signals
/// @see neco_signal_wait_dl()
int neco_signal_wait(void) {
    return neco_signal_wait_dl(INT64_MAX);
}

static int64_t now0(void) {
    if (!rt) {
        return NECO_PERM;
    } else {
        return getnow();
    }
}

/// Get the current time.
///
/// This operation calls gettime(CLOCK_MONOTONIC) to retreive a monotonically
/// increasing value that is not affected by discontinuous jumps in the system
/// time.
///
/// This value IS NOT the same as the local system time; for that the user
/// should call gettime(CLOCK_REALTIME).
///
/// The main purpose of this function to work with operations that use
/// deadlines, i.e. functions with the `*_dl()` suffix.
///
/// **Example**
///
/// ```
/// // Receive a message from a channel using a deadline of one second from now.
/// int ret = neco_chan_recv_dl(ch, &msg, neco_now() + NECO_SECOND);
/// if (ret == NECO_TIMEDOUT) {
///     // The operation timed out
/// }
/// ```
///
/// @return On success, the current time as nanoseconds.
/// @return NECO_PERM Operation called outside of a coroutine
/// @see Time
int64_t neco_now(void) {
    int64_t now = now0();
    error_guard(now); 
    return now;
}

#if defined(NECO_TESTING)
// Allow for tests to set a fail flag for testing event queue failures.
__thread bool neco_fail_cowait = false;
#endif

#if defined(NECO_POLL_EPOLL)
static int wait_dl_addevent(struct coroutine *co, int fd, enum evkind kind) {
    (void)co;
    struct epoll_event ev = { 0 };
    ev.data.fd = fd;
    if (kind == EVREAD) {
        ev.events = EPOLLIN | EPOLLONESHOT;
        ev.events |= evexists(fd, EVWRITE) ? EPOLLOUT : 0;
    } else {
        ev.events = EPOLLOUT | EPOLLONESHOT;
        ev.events |= evexists(fd, EVREAD) ? EPOLLIN : 0;
    }
    int ret = epoll_ctl0(rt->qfd, EPOLL_CTL_MOD, fd, &ev);
    if (ret == -1) {
        ret = epoll_ctl0(rt->qfd, EPOLL_CTL_ADD, fd, &ev);
    }
    return ret;
}
static void wait_dl_delevent(struct coroutine *co, int fd, enum evkind kind) {
    // Using oneshot. Nothing to do.
    (void)co; (void)fd; (void)kind;
}

#elif defined(NECO_POLL_KQUEUE)
static int wait_dl_addevent(struct coroutine *co, int fd, enum evkind kind) {
    (void)co;
    int ret = 0;
    if (!evexists(fd, kind)) {
        struct kevent ev = { 
            .ident = (uintptr_t)fd, 
            .flags = EV_ADD | EV_ONESHOT,
            .filter = kind == EVREAD ? EVFILT_READ : EVFILT_WRITE,
        };
        ret = kevent0(rt->qfd, &ev, 1, NULL, 0, NULL);
    }
    return ret;
}

static void wait_dl_delevent(struct coroutine *co, int fd, enum evkind kind) {
    // Using oneshot. Nothing to do.
    (void)co; (void)fd; (void)kind;
}
#endif

// wait_dl makes the current coroutine wait for the file descriptor to be
// available for reading or writing.
static int wait_dl(int fd, enum evkind kind, int64_t deadline) {
    if (kind != EVREAD && kind != EVWRITE) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    }
    struct coroutine *co = coself();
#if !defined(NECO_POLL_EPOLL) && !defined(NECO_POLL_KQUEUE)
    // Windows and Emscripten only yield, until support for events queues
    // are enabled.
    (void)fd; (void)deadline;
    (void)evmap_insert; (void)evmap_delete; (void)evexists;

    sco_yield();
#else
    if (rt->qfd == 0) {
        // The scheduler currently does not have an event queue for handling
        // file events. Create one now. This new queue will be shared for the 
        // entirety of the scheduler. It will be automatically freed when it's
        // no longer needed, by the scheduler.
        rt->qfd = evqueue0();
        if (rt->qfd == -1) {
            // Error creating the event queue. This is usually due to a system
            // that is low on resources or is limiting the number of file
            // descriptors allowed by a program, e.g. ulimit. 
            rt->qfd = 0;
            return -1;
        }
        // The queue was successfully created.
        rt->qfdcreated = getnow();
    }

    int ret = wait_dl_addevent(co, fd, kind);
    if (ret == -1) {
        return -1;
    }

    co->evfd = fd;
    co->evkind = kind;

    // Add this coroutine, chaining it to the distinct fd/kind.
    // This creates a unique record in the evwaiters list
    evmap_insert(&rt->evwaiters, co);
    rt->nevwaiters++;

    // Now wait for the scheduler to wake this coroutine up again.
    copause(deadline);

    // Delete from evwaiters.
    evmap_delete(&rt->evwaiters, co);
    rt->nevwaiters--;

    co->evfd = 0;
    co->evkind = 0;

    wait_dl_delevent(co, fd, kind);
#endif
    return checkdl(co, INT64_MAX);
}

/// Same as neco_wait() but with a deadline parameter. 
int neco_wait_dl(int fd, int mode, int64_t deadline) {
    int ret = wait_dl(fd, mode, deadline);
    async_error_guard(ret);
    return ret;
}

/// Wait for a file descriptor to be ready for reading or writing.
///
/// Normally one should use neco_read() and neco_write() to read and write
/// data. But there may be times when you need more involved logic or to use
/// alternative functions such as `recvmsg()` or `sendmsg()`.
///
/// ```
/// while (1) {
///     int n = recvmsg(sockfd, msg, MSG_DONTWAIT);
///     if (n == -1) {
///         if (errno == EAGAIN) {
///             // The socket is not ready for reading.
///             neco_wait(sockfd, NECO_WAIT_READ);
///             continue;
///         }
///         // Socket error.
///         return;
///     }
///     // Message received.
///     break;
/// }
/// ```
/// 
/// @param fd The file descriptor
/// @param mode NECO_WAIT_READ or NECO_WAIT_WRITE
/// @return NECO_OK Success
/// @return NECO_NOMEM The system lacked the necessary resources
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_ERROR Check errno for more info
/// @see Posix2
int neco_wait(int fd, int mode) {
    return neco_wait_dl(fd, mode, INT64_MAX);
}

// cowait makes the current coroutine wait for the file descriptor to be
// available for reading or writing. Any kind of error encountered by this 
// operation will cause the coroutine to be rescheduled, with the error
// gracefully being ignored, while leaving the file, coroutine, and scheduler
// in the state that it was prior to calling the function.
// Alternatively, the wait_dl can be used if the error is needed. 
static void cowait(int fd, enum evkind kind, int64_t deadline) {
    int ret = wait_dl(fd, kind, deadline);
    if (ret == NECO_CANCELED) {
        // The cancel flag should be set to true if the wait was canceled, 
        // because the caller is responsible for handling deadlines and
        // checking the canceled flag upon return.
        struct coroutine *co = coself();
        co->canceled = true;
    }
    if (ret != NECO_OK) {
        coyield();
    } 
}

////////////////////////////////////////////////////////////////////////////////
// Networking code
////////////////////////////////////////////////////////////////////////////////

#if !defined(NECO_NOWORKERS) && defined(NECO_USEREADWORKERS) && \
    !defined(NECO_NOREADWORKERS)

struct ioread {
    int fd;
    void *data;
    size_t count;
    ssize_t res;
    struct runtime *rt;
    struct coroutine *co;
} aligned16;

static void ioread(void *udata) {
    struct ioread *info = udata;
    info->res = read0(info->fd, info->data, info->count);
    if (info->res == -1) {
        info->res = -errno;
    }
    pthread_mutex_lock(&info->rt->iomu);
    colist_push_back(&info->rt->iolist, info->co);
    pthread_mutex_unlock(&info->rt->iomu);
}

static ssize_t read1(int fd, void *data, size_t nbytes) {
    ssize_t n;
    bool nowork = true;
#ifdef NECO_TESTING
    if (neco_fail_read_counter > 0) {
        nowork = true;
    } 
#endif
#if NECO_MAXIOWORKERS <= 0
    nowork = true;
#endif
    if (!nowork) {
        nowork = true;
        struct coroutine *co = coself();
        struct ioread info = { 
            .fd = fd, 
            .data = data, 
            .count = nbytes,
            .co = co,
            .rt = rt,
        };
        int64_t pin = co->id % NECO_MAXIOWORKERS;
        if (worker_submit(rt->worker, pin, ioread, &info)) {
            rt->niowaiters++;
            sco_pause();
            rt->niowaiters--;
            n = info.res;
            if (n < 0) {
                errno = -n;
                n = -1;
            }
            nowork = false;
        }
    }
    if (nowork) {
        n = read0(fd, data, nbytes);
    }
    return n;
}
#else
#define read1 read0
#endif

static ssize_t read_dl(int fd, void *data, size_t nbytes, int64_t deadline) {
    struct coroutine *co = coself();
    if (!co) {
        errno = EPERM;
        return -1;
    }
    while (1) {
        int ret = checkdl(co, deadline);
        if (ret != NECO_OK) {
            errno = ret == NECO_CANCELED ? ECANCELED : ETIMEDOUT;
            return -1;
        }
#if NECO_BURST < 0
        cowait(fd, EVREAD, deadline);
#endif
        ssize_t n = read1(fd, data, nbytes);
        if (n == -1) {
            if (errno == EINTR || errno == EAGAIN) {
#if NECO_BURST >= 0
                if (rt->burstcount == NECO_BURST) {
                    rt->burstcount = 0;
                    cowait(fd, EVREAD, deadline);
                } else {
                    rt->burstcount++;
                    sco_yield();
                }
#endif
                // continue;
            } else {
                return -1;
            }
        } else {
            return n;
        }
    }
}

/// Same as neco_read() but with a deadline parameter. 
ssize_t neco_read_dl(int fd, void *buf, size_t count, int64_t deadline) {
    ssize_t ret = read_dl(fd, buf, count, deadline);
    async_error_guard(ret);
    return ret;
}

/// Read from a file descriptor.
///
/// This operation attempts to read up to count from file descriptor fd 
/// into the buffer starting at buf.
///
/// This is a Posix wrapper function for the purpose of running in a Neco
/// coroutine. It's expected that the provided file descriptor is in 
/// non-blocking state.
///
/// @return On success, the number of bytes read is returned (zero indicates
///         end of file)
/// @return On error, value -1 (NECO_ERROR) is returned, and errno is set to
///         indicate the error.
/// @see    Posix
/// @see    neco_setnonblock()
/// @see    https://www.man7.org/linux/man-pages/man2/read.2.html
ssize_t neco_read(int fd, void *buf, size_t count) {
    return neco_read_dl(fd, buf, count, INT64_MAX);
}

#ifdef NECO_TESTING
#define write1 write0
// Allow for testing partial writes
__thread int neco_partial_write = 0;
static ssize_t write2(int fd, const void *data, size_t nbytes) {
    bool fail = false;
    if (neco_partial_write) {
        neco_partial_write--;
        nbytes = nbytes > 128 ? 128 : nbytes;
        // nbytes /= 2;
        fail = neco_partial_write == 0;
    }
    if (fail) {
        errno = EIO;
        return -1;
    }
    return write1(fd, data, nbytes);
}
#else
// In production, a write to a broken stdout or stderr pipe should end the 
// program immediately.
inline
static ssize_t write1(int fd, const void *data, size_t nbytes) {
    ssize_t n = write0(fd, data, nbytes);
    if (n == -1 && errno == EPIPE && (fd == 1 || fd == 2)) {
        // Broken pipe on stdout or stderr
        _Exit(128+EPIPE);
    }
    return n;
}
#define write2 write1
#endif

#if !defined(NECO_NOWORKERS) && defined(NECO_USEWRITEWORKERS) && \
    !defined(NECO_NOWRITEWORKERS)
struct iowrite {
    int fd;
    const void *data;
    size_t count;
    ssize_t res;
    struct runtime *rt;
    struct coroutine *co;
} aligned16;

static void iowrite(void *udata) {
    struct iowrite *info = udata;
    info->res = write2(info->fd, info->data, info->count);
    if (info->res == -1) {
        info->res = -errno;
    }
    pthread_mutex_lock(&info->rt->iomu);
    colist_push_back(&info->rt->iolist, info->co);
    pthread_mutex_unlock(&info->rt->iomu);
}

static ssize_t write3(int fd, const void *data, size_t nbytes) {
    bool nowork = false;
    ssize_t n;
#ifdef NECO_TESTING
    if (neco_fail_write_counter > 0 || neco_partial_write > 0) {
        nowork = true;
    }
#endif
#if NECO_MAXIOWORKERS <= 0
    nowork = true;
#endif
    if (!nowork) {
        nowork = true;
        struct coroutine *co = coself();
        struct iowrite info = { 
            .fd = fd, 
            .data = data, 
            .count = nbytes,
            .co = co,
            .rt = rt,
        };
        int64_t pin = co->id % NECO_MAXIOWORKERS;
        if (worker_submit(rt->worker, pin, iowrite, &info)) {
            rt->niowaiters++;
            sco_pause();
            rt->niowaiters--;
            n = info.res;
            if (n < 0) {
                errno = -n;
                n = -1;
            }
            nowork = false;
        }
    }
    if (nowork) {
        n = write2(fd, data, nbytes);
    }
    return n;
}
#else
#define write3 write2
#endif

static ssize_t write_dl(int fd, const void *data, size_t nbytes,
    int64_t deadline)
{
    struct coroutine *co = coself();
    if (!co) {
        errno = EPERM;
        return -1;
    }
    ssize_t written = 0;
    while (1) {
        int ret = checkdl(co, deadline);
        if (ret != NECO_OK) {
            errno = ret == NECO_CANCELED ? ECANCELED : ETIMEDOUT;
            return -1;
        }
        // size_t maxnbytes = CLAMP(nbytes, 0, 8096);
        // ssize_t n = write2(fd, data, maxnbytes);
        ssize_t n = write3(fd, data, nbytes);
        if (n == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                cowait(fd, EVWRITE, deadline);
            } else if (written == 0) {
                return -1;
            } else {
                // There was an error but data has also been written. It's
                // important to notify the caller about the amount of data
                // written. The caller will need to then check to see if the
                // return value is less than nbytes, and if so check
                // the system errno.
                return written;
            }
        } else if (n > 0) {
            nbytes -= (size_t)n;
            written += (size_t)n;
            data = (char*)data + n;
        }
        if (nbytes == 0) {
            break;
        }
        if (n >= 0) {
            // Some data was written but there's more yet. 
            // Avoiding starving the other coroutines.
            coyield();
        }
    }
    return written;
}

/// Same as neco_write() but with a deadline parameter. 
ssize_t neco_write_dl(int fd, const void *buf, size_t count,
    int64_t deadline)
{
    ssize_t ret = write_dl(fd, buf, count, deadline);
    async_error_guard(ret);
    if (ret >= 0 && (size_t)ret < count) {
        lasterr = NECO_PARTIALWRITE;
    } 
    return ret;
}

/// Write to a file descriptor.
///
/// This operation attempts to write all bytes in the buffer starting at buf
/// to the file referred to by the file descriptor fd.
///
/// This is a Posix wrapper function for the purpose of running in a Neco
/// coroutine. It's expected that the provided file descriptor is in 
/// non-blocking state.
///
/// One difference from the Posix version is that this function will attempt to
/// write _all_ bytes in buffer. The programmer, at their discretion, may 
/// considered it as an error when fewer than count is returned. If so, the
/// neco_lasterr() will return the NECO_PARTIALWRITE.
///
/// @return On success, the number of bytes written is returned.
/// @return On error, value -1 (NECO_ERROR) is returned, and errno is set to
///         indicate the error.
/// @see    Posix
/// @see    neco_setnonblock()
/// @see    https://www.man7.org/linux/man-pages/man2/write.2.html
ssize_t neco_write(int fd, const void *buf, size_t count) {
    return neco_write_dl(fd, buf, count, INT64_MAX);
}

#ifdef _WIN32

static int wsa_err_to_errno(int wsaerr) {
    switch (wsaerr) {
    case WSAEINTR: return EINTR;
    case WSAEBADF: return EBADF;
    case WSAEACCES: return EACCES;
    case WSAEFAULT: return EFAULT;
    case WSAEINVAL: return EINVAL;
    case WSAEMFILE: return EMFILE;
    case WSAEWOULDBLOCK: return EAGAIN;
    case WSAEINPROGRESS: return EINPROGRESS;
    case WSAEALREADY: return EALREADY;
    case WSAENOTSOCK: return ENOTSOCK;
    case WSAEDESTADDRREQ: return EDESTADDRREQ;
    case WSAEMSGSIZE: return EMSGSIZE;
    case WSAEPROTOTYPE: return EPROTOTYPE;
    case WSAENOPROTOOPT: return ENOPROTOOPT;
    case WSAEPROTONOSUPPORT: return EPROTONOSUPPORT;
    case WSAEOPNOTSUPP: return EOPNOTSUPP;
    case WSAEADDRINUSE: return EADDRINUSE;
    case WSAEADDRNOTAVAIL: return EADDRNOTAVAIL;
    case WSAENETDOWN: return ENETDOWN;
    case WSAENETUNREACH: return ENETUNREACH;
    case WSAENETRESET: return ENETRESET;
    case WSAECONNABORTED: return ECONNABORTED;
    case WSAECONNRESET: return ECONNRESET;
    case WSAENOBUFS: return ENOBUFS;
    case WSAEISCONN: return EISCONN;
    case WSAENOTCONN: return ENOTCONN;
    case WSAETIMEDOUT: return ETIMEDOUT;
    case WSAECONNREFUSED: return ECONNREFUSED;
    case WSAELOOP: return ELOOP;
    case WSAENAMETOOLONG: return ENAMETOOLONG;
    case WSAEHOSTUNREACH: return EHOSTUNREACH;
    case WSAENOTEMPTY: return ENOTEMPTY;
    case WSAECANCELLED: return ECANCELED;
    case WSA_E_CANCELLED: return ECANCELED;
    }
    return wsaerr;
}

int accept1(int sock, struct sockaddr *addr, socklen_t *len) {
    int fd = accept0(sock, addr, len);
    if (fd == -1) {
        errno = wsa_err_to_errno(WSAGetLastError());
    }
    return fd;
}
#else
#define accept1 accept0
#endif

static int accept_dl(int sockfd, struct sockaddr *addr, socklen_t *addrlen, 
    int64_t deadline)
{
    struct coroutine *co = coself();
    if (!co) {
        errno = EPERM;
        return -1;
    }
    while (1) {
        int ret = checkdl(co, deadline);
        if (ret != NECO_OK) {
            errno = ret == NECO_CANCELED ? ECANCELED : ETIMEDOUT;
            return -1;
        }
        int fd = accept1(sockfd, addr, addrlen);
        if (fd == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                cowait(sockfd, EVREAD, deadline);
            } else {
                return -1;
            }
        } else {
            if (neco_setnonblock(fd, true, 0) == -1) {
                close(fd);
                return -1;
            }
            return fd;
        }
    }
}

/// Same as neco_accept() but with a deadline parameter. 
int neco_accept_dl(int sockfd, struct sockaddr *addr, socklen_t *addrlen, 
    int64_t deadline)
{
    int ret = accept_dl(sockfd, addr, addrlen, deadline);
    async_error_guard(ret);
    return ret;
}

/// Accept a connection on a socket.
///
/// While in a coroutine, this function should be used instead of the standard
/// accept() to avoid blocking other coroutines from running concurrently.
///
/// The the accepted file descriptor is returned in non-blocking mode.
///
/// @param sockfd Socket file descriptor
/// @param addr Socket address out
/// @param addrlen Socket address length out
/// @return On success, file descriptor (non-blocking)
/// @return On error, value -1 (NECO_ERROR) is returned, and errno is set to
///         indicate the error.
/// @see Posix
int neco_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return neco_accept_dl(sockfd, addr, addrlen, INT64_MAX);
}

static int connect_dl(int fd, const struct sockaddr *addr, socklen_t addrlen, 
    int64_t deadline)
{
    struct coroutine *co = coself();
    if (!co) {
        errno = EPERM;
        return -1;
    }
    bool inprog = false;
    while (1) {
        int ret = checkdl(co, deadline);
        if (ret != NECO_OK) {
            errno = ret == NECO_CANCELED ? ECANCELED : ETIMEDOUT;
            return -1;
        }
        errno = 0;
        ret = connect0(fd, addr, addrlen);
        if (ret == -1) {
            switch (errno) {
            case EISCONN:
                // The socket is already connected. 
                // This could be because we just tried a moment ago and
                // received an EINPROGRESS. In that case return success,
                // otherwise return an error.
                ret = inprog ? 0 : -1;
                break;
            case EAGAIN:
                // Only unix domain sockets can get this error.
                // Linux manual does not describe why this can happen or 
                // what to do when it does. FreeBSD says it means that there
                // are not enough ports available on the system to assign to
                // the  file descriptor. We can (maybe??) try again. But
                // probably best to return an error that is different than
                // EAGAIN. 
                errno = ECONNREFUSED;
                break;
            case EINPROGRESS:
                // EINPROGRESS means the connection is in progress and may
                // take a while. We are allowed to add this file descriptor
                // to an event queue and wait for the connection to be ready.
                // This can be done by waiting on a write event. 
                // printf("D\n");
                inprog = true;
                cowait(fd, EVWRITE, deadline);
                continue;
            case EINTR:  // Interrupted. Just try again.
            case ENOMEM: // Mac OS can sometimes return ENOMEM (bug)
                continue;
            }
        }
        return ret;
    }
}

/// Same as neco_connect() but with a deadline parameter.
int neco_connect_dl(int sockfd, const struct sockaddr *addr, socklen_t addrlen, 
    int64_t deadline)
{
    int ret = connect_dl(sockfd, addr, addrlen, deadline);
    async_error_guard(ret);
    return ret;
}

/// Connects the socket referred to by the file descriptor sockfd to the
/// address specified by addr.
///
/// While in a coroutine, this function should be used instead of the standard
/// connect() to avoid blocking other coroutines from running concurrently.
///
/// @param sockfd Socket file descriptor
/// @param addr Socket address out
/// @param addrlen Socket address length out
/// @return NECO_OK Success
/// @return On error, value -1 (NECO_ERROR) is returned, and errno is set to
///         indicate the error.
int neco_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return neco_connect_dl(sockfd, addr, addrlen, INT64_MAX);
}

void neco_errconv_to_sys(int err) {
    switch (err) {
    case NECO_OK:
        errno = 0;
        break;
    case NECO_INVAL:
        errno = EINVAL;
        break;
    case NECO_PERM:
        errno = EPERM;
        break;
    case NECO_NOMEM:
        errno = ENOMEM;
        break;
    case NECO_CANCELED:
        errno = ECANCELED;
        break;
    case NECO_TIMEDOUT:
        errno = ETIMEDOUT;
        break;
    }
}

int neco_errconv_from_sys(void) {
    switch (errno) {
    case EINVAL:
        return NECO_INVAL;
    case EPERM:
        return NECO_PERM;
    case ENOMEM:
        return NECO_NOMEM;
    case ECANCELED:
        return NECO_CANCELED;
    case ETIMEDOUT:
        return NECO_TIMEDOUT;
    default:
        return NECO_ERROR;
    }
}

const char *neco_shortstrerror(int code) {
    switch (code) {
    case NECO_OK:
        return "NECO_OK";
    case NECO_ERROR:
        return "NECO_ERROR";
    case NECO_INVAL:
        return "NECO_INVAL";
    case NECO_PERM:
        return "NECO_PERM";
    case NECO_NOMEM:
        return "NECO_NOMEM";
    case NECO_NOTFOUND:
        return "NECO_NOTFOUND";
    case NECO_NOSIGWATCH:
        return "NECO_NOSIGWATCH";
    case NECO_CLOSED:
        return "NECO_CLOSED";
    case NECO_EMPTY:
        return "NECO_EMPTY";
    case NECO_TIMEDOUT:
        return "NECO_TIMEDOUT";
    case NECO_CANCELED:
        return "NECO_CANCELED";
    case NECO_BUSY:
        return "NECO_BUSY";
    case NECO_NEGWAITGRP:
        return "NECO_NEGWAITGRP";
    case NECO_GAIERROR:
        return "NECO_GAIERROR";
    case NECO_UNREADFAIL:
        return "NECO_UNREADFAIL";
    case NECO_PARTIALWRITE:
        return "NECO_PARTIALWRITE";
    case NECO_NOTGENERATOR:
        return "NECO_NOTGENERATOR";
    case NECO_NOTSUSPENDED:
        return "NECO_NOTSUSPENDED";
    default:
        return "UNKNOWN";
    }
}

/// Returns a string representation of an error code.
/// @see Errors
const char *neco_strerror(ssize_t errcode) {
    switch (errcode) {
    case NECO_OK:
        return "Success";
    case NECO_ERROR:
        return strerror(errno);
    case NECO_INVAL:
        return strerror(EINVAL);
    case NECO_PERM:
        return strerror(EPERM);
    case NECO_NOMEM:
        return strerror(ENOMEM);
    case NECO_NOTFOUND:
        return "No such coroutine";
    case NECO_NOSIGWATCH:
        return "Not watching on a signal";
    case NECO_CLOSED:
        return "Channel closed";
    case NECO_EMPTY:
        return "Channel empty";
    case NECO_TIMEDOUT:
        return strerror(ETIMEDOUT);
    case NECO_CANCELED:
        return strerror(ECANCELED);
    case NECO_BUSY:
        return strerror(EBUSY);
    case NECO_NEGWAITGRP:
        return "Negative waitgroup counter";
    case NECO_GAIERROR:
        if (neco_gai_errno == EAI_SYSTEM) {
            return strerror(errno);
        } else {
            return gai_strerror(neco_gai_errno);
        }
    case NECO_UNREADFAIL:
        return "Failed to unread byte";
    case NECO_PARTIALWRITE:
        return "Failed to write all bytes";
    case NECO_NOTGENERATOR:
        return "Coroutine is not a generator";
    case NECO_NOTSUSPENDED:
        return "Coroutine is not suspended";
    default: {
        static __thread char udmsg[32];
        snprintf(udmsg, sizeof(udmsg)-1, "Undefined error: %zd", errcode);
        return udmsg;
    }
    }
}

static int getstats(neco_stats *stats) {
    if (!stats) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    }
    *stats = (neco_stats) {
        .coroutines = rt->all.count,
        .sleepers = rt->nsleepers,
        .evwaiters = rt->nevwaiters,
        .sigwaiters = rt->nsigwaiters,
        .senders = rt->nsenders,
        .receivers = rt->nreceivers,
        .locked = rt->nlocked,
        .waitgroupers = rt->nwaitgroupers,
        .condwaiters = rt->ncondwaiters,
        .suspended = rt->nsuspended,
    };
    return NECO_OK;
}

/// Returns various stats for the current Neco runtime.
/// 
/// ```c
/// // Print the number of active coroutines
/// neco_stats stats;
/// neco_getstats(&stats);
/// printf("%zu\n", stats.coroutines);
/// ```
///
/// Other stats include:
///
/// ```c
/// coroutines
/// sleepers
/// evwaiters
/// sigwaiters
/// senders
/// receivers
/// locked
/// waitgroupers
/// condwaiters
/// suspended
/// ```

int neco_getstats(neco_stats *stats) {
    int ret = getstats(stats);
    error_guard(ret);
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
// channels
////////////////////////////////////////////////////////////////////////////////

struct neco_chan {
    int64_t rtid;         // runtime id. for runtime/thread isolation
    int rc;               // reference counter
    bool sclosed;         // sender closed
    bool rclosed;         // receiver closed
    bool qrecv;           // queue has all receivers, otherwise all senders
    bool lok;             // used for the select-case 'closed' result
    struct colist queue;  // waiting coroutines. Either senders or receivers
    int msgsize;          // size of each message
    int bufcap;           // max number of messages in ring buffer
    int buflen;           // number of messages in ring buffer
    int bufpos;           // position of first message in ring buffer
    char data[];          // message ring buffer + one extra entry for 'lmsg'
};

// coselectcase pretends to be a coroutine for the purpose of multiplexing
// select-case channels into a single coroutine. 
// It's required that this structure is 16-byte aligned.
struct coselectcase {
    struct coroutine *prev;
    struct coroutine *next;
    enum cokind kind;

    struct neco_chan *chan;
    struct coroutine *co;
    void *data;
    bool *ok;
    int idx;
    int *ret_idx;
} aligned16;

// returns the message slot
static char *cbufslot(struct neco_chan *chan, int index) {
    return chan->data + (chan->msgsize * index);
}

// push a message to the back by copying from data
static void cbuf_push(struct neco_chan *chan, void *data) {
    int pos = chan->bufpos + chan->buflen;
    if (pos >= chan->bufcap) {
        pos -= chan->bufcap;
    }
    if (chan->msgsize > 0) {
        memcpy(cbufslot(chan, pos), data, (size_t)chan->msgsize);
    }
    chan->buflen++;
}

// pop a message from the front and copy to data
static void cbuf_pop(struct neco_chan *chan, void *data) {
    if (chan->msgsize) {
        memcpy(data, cbufslot(chan, chan->bufpos), (size_t)chan->msgsize);
    }
    chan->bufpos++;
    if (chan->bufpos == chan->bufcap) {
        chan->bufpos = 0;
    }
    chan->buflen--;
}

static struct neco_chan *chan_fastmake(size_t data_size, size_t capacity,
    bool as_generator)
{
    struct neco_chan *chan;
    // Generators do not need buffered capacity of any size because they can't
    // use the neco_select, which requires at least one buffered slot for the 
    // neco_case operation to store the pending data.
    size_t ring_size = as_generator ? 0 : data_size * (capacity+1);
    if (POOL_ENABLED && ring_size == 0 && rt->zchanpoollen > 0) {
        chan = rt->zchanpool[--rt->zchanpoollen];
    } else {
        size_t memsize = sizeof(struct neco_chan) + ring_size;
        chan = malloc0(memsize);
        if (!chan) {
            return NULL;
        }
    }
    // Zero the struct memory space, not the data space.
    memset(chan, 0, sizeof(struct neco_chan));
    chan->rtid = rt->id;
    chan->msgsize = (int)data_size;
    chan->bufcap = (int)capacity;
    colist_init(&chan->queue);
    return chan;
}

static int chan_make(struct neco_chan **chan, size_t data_size, size_t capacity)
{
    if (!chan || data_size > INT_MAX || capacity > INT_MAX) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM; 
    }
    *chan = chan_fastmake(data_size, capacity, 0);
    if (!*chan) {
        return NECO_NOMEM;
    }
    return NECO_OK;
}

/// Creates a new channel for sharing messages with other coroutines.
///
/// **Example**
///
/// ```
/// void coroutine(int argc, void *argv[]) {
///     neco_chan *ch = argv[0];
/// 
///     // Send a message
///     neco_chan_send(ch, &(int){ 1 });
/// 
///     // Release the channel
///     neco_chan_release(ch);
/// }
/// 
/// int neco_start(int argc, char *argv[]) {
///     neco_chan *ch;
///     neco_chan_make(&ch, sizeof(int), 0);
///     
///     // Retain a reference of the channel and provide it to a newly started
///     // coroutine. 
///     neco_chan_retain(ch);
///     neco_start(coroutine, 1, ch);
///     
///     // Receive a message
///     int msg;
///     neco_chan_recv(ch, &msg);
///     printf("%d\n", msg);      // prints '1'
///     
///     // Always release the channel when you are done
///     neco_chan_release(ch);
/// }
/// ```
///
/// @param chan Channel
/// @param data_size Data size of messages
/// @param capacity Buffer capacity
/// @return NECO_OK Success
/// @return NECO_NOMEM The system lacked the necessary resources
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @note The caller is responsible for freeing with neco_chan_release()
/// @note data_size and capacity cannot be greater than INT_MAX
int neco_chan_make(struct neco_chan **chan, size_t data_size, size_t capacity) {
    int ret = chan_make(chan, data_size, capacity);
    error_guard(ret);
    return ret;
}

static void chan_fastretain(struct neco_chan *chan) {
   chan->rc++;
}

static int chan_retain(struct neco_chan *chan) {
    if (!chan) {
        return NECO_INVAL;
    } else if (!rt || rt->id != chan->rtid) {
        return NECO_PERM;
    }
    chan_fastretain(chan);
    return NECO_OK;
}

/// Retain a reference of the channel so it can be shared with other coroutines.
///
/// This is needed for avoiding use-after-free bugs.
/// 
/// See neco_chan_make() for an example.
///
/// @param chan The channel
/// @return NECO_OK Success
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @note The caller is responsible for releasing the reference with 
///       neco_chan_release()
/// @see Channels
/// @see neco_chan_release()
int neco_chan_retain(struct neco_chan *chan) {
    int ret = chan_retain(chan);
    error_guard(ret);
    return ret;
}

static bool zchanpush(struct neco_chan *chan) {
    if (rt->zchanpoollen == rt->zchanpoolcap) {
        if (rt->zchanpoolcap == 256) {
            return false;
        }
        int cap = rt->zchanpoolcap == 0 ? 16 : rt->zchanpoolcap * 2;
        void *zchanpool2 = realloc0(rt->zchanpool, 
            sizeof(struct neco_chan) * (size_t)cap);
        if (!zchanpool2) {
            return false;
        }
        rt->zchanpoolcap = cap;
        rt->zchanpool = zchanpool2;
    }
    rt->zchanpool[rt->zchanpoollen++] = chan;
    return true;
}

static void chan_fastrelease(struct neco_chan *chan) {
    chan->rc--;
    if (chan->rc < 0) {
        if (!POOL_ENABLED || chan->msgsize > 0 || !zchanpush(chan)) {
            free0(chan);
        }
    }
}

static int chan_release(struct neco_chan *chan) {
    if (!chan) {
        return NECO_INVAL;
    } else if (!rt || rt->id != chan->rtid) {
        return NECO_PERM;
    }
    chan_fastrelease(chan);
    return NECO_OK;
}

/// Release a reference to a channel
///
/// See neco_chan_make() for an example.
///
/// @param chan The channel
/// @return NECO_OK Success
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @see Channels
/// @see neco_chan_retain()
int neco_chan_release(struct neco_chan *chan) {
    int ret = chan_release(chan);
    error_guard(ret);
    return ret;
}

static int chan_send0(struct neco_chan *chan, void *data, bool broadcast, 
    int64_t deadline)
{
    if (!chan) {
        return NECO_INVAL;
    } else if (!rt || chan->rtid != rt->id) {
        return NECO_PERM;
    } else if (chan->sclosed) {
        return NECO_CLOSED;
    }
    struct coroutine *co = coself();
    if (co->canceled && !broadcast) {
        co->canceled = false;
        return NECO_CANCELED;
    }
    int sent = 0;
    while (!colist_is_empty(&chan->queue) && chan->qrecv) {
        // A receiver is currently waiting for a message.
        // Pop it from the queue.
        struct coroutine *recv = colist_pop_front(&chan->queue);
        if (recv->kind == SELECTCASE) {
            // The receiver is a select-case. 
            struct coselectcase *cocase = (struct coselectcase *)recv;
            if (*cocase->ret_idx != -1) {
                // This select-case has already been handled
                continue;
            }
            // Set the far stack index pointer and exchange the select-case
            // with the real coroutine.
            *cocase->ret_idx = cocase->idx;
            recv = cocase->co;
            recv->cmsg = cocase->data;
            *cocase->ok = true;
        }
        // Directly copy the message to the receiver's 'data' argument.
        if (chan->msgsize > 0) {
            memcpy(recv->cmsg, data, (size_t)chan->msgsize);
        }
        if (!broadcast) {
            // Resume receiver immediately.
            sco_resume(recv->id);
            return NECO_OK;
        } else {
            // Schedule the reciever.
            sched_resume(recv);
            sent++;
        }
    }
    if (broadcast) {
        yield_for_sched_resume();
        return sent;
    }

    if (chan->buflen < chan->bufcap) {
        // There room to write to the ring buffer. 
        // Add this message and return immediately.
        cbuf_push(chan, data);
        return NECO_OK;
    }

    // Push this sender to queue
    colist_push_back(&chan->queue, co);
    chan->qrecv = false;
    co->cmsg = data;

    // Wait for a receiver to consume this message.
    rt->nsenders++;
    copause(deadline);
    rt->nsenders--;
    remove_from_list(co);

    co->cmsg = NULL;
    return checkdl(co, INT64_MAX);
}

/// Same as neco_chan_send() but with a deadline parameter.
int neco_chan_send_dl(neco_chan *chan, void *data, int64_t deadline) {
    int ret = chan_send0(chan, data, false, deadline);
    async_error_guard(ret);
    return ret;
}

/// Send a message
///
/// See neco_chan_make() for an example.
///
/// @return NECO_OK Success
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_CANCELED Operation canceled
/// @return NECO_CLOSED Channel closed
/// @see Channels
/// @see neco_chan_recv()
int neco_chan_send(struct neco_chan *chan, void *data) {
    return neco_chan_send_dl(chan, data, INT64_MAX);
}

/// Sends message to all receiving channels.
/// @return The number of channels that received the message
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_CLOSED Channel closed
/// @note This operation cannot be canceled and does not timeout
/// @see Channels
/// @see neco_chan_recv()
int neco_chan_broadcast(struct neco_chan *chan, void *data) {
    int ret = chan_send0(chan, data, true, INT64_MAX);
    error_guard(ret);
    return ret;
}

static int chan_tryrecv0(struct neco_chan *chan, void *data, bool try,
    int64_t deadline)
{
    if (!chan) {
        return NECO_INVAL;
    } else if (!rt || chan->rtid != rt->id) {
        return NECO_PERM;
    } else if (chan->rclosed) {
        return NECO_CLOSED;
    }
    struct coroutine *co = coself();
    if (co->canceled) {
        co->canceled = false;
        return NECO_CANCELED;
    }
    if (chan->buflen > 0) {
        // Take from the buffer
        cbuf_pop(chan, data);
        struct coroutine *send = NULL;
        if (!colist_is_empty(&chan->queue)) {
            // There's a sender waiting to send a message.
            // Put the sender's message in the buffer and wake it up.
            send = colist_pop_front(&chan->queue);
            cbuf_push(chan, send->cmsg);
        }
        if (chan->sclosed && colist_is_empty(&chan->queue) && 
            chan->buflen == 0)
        {
            // The channel was closed and there are no more messages.
            // Close the receiving side too
            chan->rclosed = true;
        }
        if (send) {
            sco_resume(send->id);
        }
        return NECO_OK;
    }

    if (!colist_is_empty(&chan->queue) && !chan->qrecv) {
        // A sender is currently waiting to send a message.
        // This message must be consumed immediately.
        struct coroutine *send = colist_pop_front(&chan->queue);
        if (chan->msgsize) {
            memcpy(data, send->cmsg, (size_t)chan->msgsize);
        }
        if (chan->sclosed && colist_is_empty(&chan->queue) && 
            chan->buflen == 0)
        {
            // The channel was closed and there are no more messages.
            // Now close the receiving side too.
            chan->rclosed = true;
        }
        sco_resume(send->id);
        return NECO_OK;
    }
    if (try) {
        return NECO_EMPTY;
    }
    // Push the receiver to the queue. 
    colist_push_back(&chan->queue, co);
    chan->qrecv = true;
    // Assign the 'cmsg' message slot so that the sender knows where to copy
    // the message.
    co->cmsg = data;
    // Set the channel closed flag that *may* be changed by the sender.
    co->cclosed = false;
    // Wait for a sender.
    rt->nreceivers++;
    copause(deadline);
    rt->nreceivers--;
    remove_from_list(co);
    // The sender has already copied its message to 'data'.
    co->cmsg = NULL;
    int ret = checkdl(co, INT64_MAX);
    if (ret != NECO_OK) {
        return ret;
    }
    if (co->cclosed) {
        // The channel was closed for receiving while this coroutine was
        // waiting on a new message.
        if (chan->msgsize) {
            memset(data, 0, (size_t)chan->msgsize);
        }
        co->cclosed = false;
        return NECO_CLOSED;
    } else {
        return NECO_OK;
    }
}

/// Same as neco_chan_recv() but with a deadline parameter.
int neco_chan_recv_dl(struct neco_chan *chan, void *data, int64_t deadline) {
    int ret = chan_tryrecv0(chan, data, false, deadline);
    async_error_guard(ret);
    return ret;
}

/// Receive a message
///
/// See neco_chan_make() for an example.
///
/// @param chan channel
/// @param data data pointer
/// @return NECO_OK Success
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_CANCELED Operation canceled
/// @return NECO_CLOSED Channel closed
/// @see Channels
/// @see neco_chan_send()
int neco_chan_recv(struct neco_chan *chan, void *data) {
    return neco_chan_recv_dl(chan, data, INT64_MAX);
}

/// Receive a message, but do not wait if the message is not available.
/// @param chan channel
/// @param data data pointer
/// @return NECO_OK Success
/// @return NECO_EMPTY No message available
/// @return NECO_CLOSED Channel closed
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_INVAL An invalid parameter was provided
/// @see Channels
/// @see neco_chan_recv()
int neco_chan_tryrecv(struct neco_chan *chan, void *data) {
    int ret = chan_tryrecv0(chan, data, true, INT64_MAX);
    async_error_guard(ret);
    return ret;
}

static int chan_close(struct neco_chan *chan) {
    if (!chan) {
        return NECO_INVAL;
    } else if (!rt || chan->rtid != rt->id) {
        return NECO_PERM;
    } else if (chan->sclosed) {
        return NECO_CLOSED;
    }
    chan->sclosed = true;
    if (chan->buflen > 0 || (!colist_is_empty(&chan->queue) && !chan->qrecv)) {
        // There are currently messages in the buffer or senders still waiting
        // to send messages. Do not close the receiver side yet.
        return NECO_OK;
    }
    // No buffered messages or senders waiting. 
    // Close the receiver side and wake up all receivers.
    while (!colist_is_empty(&chan->queue)) {
        struct coroutine *recv = colist_pop_front(&chan->queue);
        if (recv->kind == SELECTCASE) {
            // The receiver is a select-case. 
            struct coselectcase *cocase = (struct coselectcase *)recv;
            if (*cocase->ret_idx != -1) {
                // This select-case has already been handled
                continue;
            }
            // Set the far stack index pointer and exchange the select-case
            // with the real coroutine.
            *cocase->ret_idx = cocase->idx;
            recv = cocase->co;
            *cocase->ok = false;
        }
        recv->cclosed = true;
        sched_resume(recv);
    }
    chan->rclosed = true;
    chan->qrecv = false;
    yield_for_sched_resume();
    return NECO_OK;
}

/// Close a channel for sending.
/// @param chan channel
/// @return NECO_OK Success
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_CLOSED Channel already closed
int neco_chan_close(struct neco_chan *chan) {
    int ret = chan_close(chan);
    error_guard(ret);
    return ret;
}

static int chan_select(int ncases, struct coselectcase *cases, int *ret_idx,
    int64_t deadline, bool try)
{
    // Check that the channels are valid before continuing.
    for (int i = 0; i < ncases; i++) {
        struct neco_chan *chan = cases[i].chan;
        if (!chan) {
            return NECO_INVAL;
        } else if (chan->rtid != rt->id) {
            return NECO_PERM;
        }
    }

    struct coroutine *co = coself();

    if (co->canceled) {
        co->canceled = false;
        return NECO_CANCELED;
    }

    // Scan each channel and see if there are any messages waiting in their
    // queue or if any are closed. 
    // If so then receive that channel immediately.
    for (int i = 0; i < ncases; i++) {
        struct neco_chan *chan = cases[i].chan;
        if ((!colist_is_empty(&chan->queue) && !chan->qrecv) || 
            chan->buflen > 0 || chan->rclosed)
        {
            int ret = neco_chan_recv(cases[i].chan, cases[i].data);
            *cases[i].ok = ret == NECO_OK;
            return i;
        }
    }

    if (try) {
        return NECO_EMPTY;
    }

    // Push all cases into their repsective channel queue.
    for (int i = 0; i < ncases; i++) {
        colist_push_back(&cases[i].chan->queue, (struct coroutine*)&cases[i]);
        cases[i].chan->qrecv = true;
    }

    // Wait for a sender to wake us up
    rt->nreceivers++;
    copause(deadline);
    rt->nreceivers--;

    // Remove all cases
    for (int i = 0; i < ncases; i++) {
        remove_from_list((struct coroutine*)&cases[i]);
    }
    int ret = checkdl(co, INT64_MAX);
    return ret == NECO_OK ? *ret_idx : ret;
}

static int chan_selectv_dl(int ncases, va_list *args, struct neco_chan **chans, 
    int64_t deadline, bool try)
{
    if (ncases < 0) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    }

    // Allocate space for storing each select-case struct. 
    // These are used in place of a coroutine for the channel receiver queues.
    bool must_free;
    struct coselectcase stack_cases[8];
    struct coselectcase *cases;
    if (ncases > 8) {
        cases = malloc0((size_t)ncases * sizeof(struct coselectcase));
        if (!cases) {
            return NECO_NOMEM;
        }
        must_free = true;
    } else {
        cases = stack_cases;
        must_free = false;
    }
    struct coroutine *co = coself();
    int ret_idx = -1;

    // Copy the select-case arguments into the array.
    for (int i = 0; i < ncases; i++) {
        struct neco_chan *chan;
        chan = args ? va_arg(*args, struct neco_chan*) : chans[i];
        cases[i] = (struct coselectcase){
            .chan = chan,
            .kind = SELECTCASE,
            .idx = i,
            .ret_idx = &ret_idx,
            .co = co,
            .data = chan ? cbufslot(chan, chan->bufcap) : 0,
            .ok = chan ? &chan->lok : 0,
        };
        cases[i].next = (struct coroutine*)&cases[i];
        cases[i].prev = (struct coroutine*)&cases[i];
    }
    int ret = chan_select(ncases, cases, &ret_idx, deadline, try);
    if (must_free) {
        free0(cases);
    }
    return ret;
}

/// Same as neco_chan_selectv() but with a deadline parameter.
int neco_chan_selectv_dl(int nchans, struct neco_chan *chans[],
    int64_t deadline)
{
    int ret = chan_selectv_dl(nchans, 0, chans, deadline, false);
    async_error_guard(ret);
    return ret;
}

/// Same as neco_chan_select() but using an array for arguments.
int neco_chan_selectv(int nchans, struct neco_chan *chans[]) {
    return neco_chan_selectv_dl(nchans, chans, INT64_MAX);
}

/// Same as neco_chan_select() but with a deadline parameter.
int neco_chan_select_dl(int64_t deadline, int nchans, ...) {
    va_list args;
    va_start(args, nchans);
    int ret = chan_selectv_dl(nchans, &args, 0, deadline, false);
    va_end(args);
    async_error_guard(ret);
    return ret;
}

/// Wait on multiple channel operations at the same time. 
///
/// **Example**
///
/// ```
/// 
/// // Let's say we have two channels 'c1' and 'c2' that both transmit 'char *'
/// // messages.
///
/// // Use neco_chan_select() to wait on both channels.
///
/// char *msg;
/// int idx = neco_chan_select(2, c1, c2);
/// switch (idx) {
/// case 0:
///     neco_chan_case(c1, &msg);
///     break;
/// case 1:
///     neco_chan_case(c2, &msg);
///     break;
/// default:
///     // Error occured. The return value 'idx' is the error
/// }
///
/// printf("%s\n", msg);
/// ```
///
/// @param nchans Number of channels 
/// @param ... The channels 
/// @return The index of channel with an available message
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_NOMEM The system lacked the necessary resources
/// @return NECO_CANCELED Operation canceled
/// @see Channels
int neco_chan_select(int nchans, ...) {
    va_list args;
    va_start(args, nchans);
    int ret = chan_selectv_dl(nchans, &args, 0, INT64_MAX, false);
    va_end(args);
    async_error_guard(ret);
    return ret;
}

/// Same as neco_chan_select() but does not wait if a message is not available
/// @return NECO_EMPTY No message available
/// @see Channels
/// @see neco_chan_select()
int neco_chan_tryselect(int nchans, ...) {
    va_list args;
    va_start(args, nchans);
    int ret = chan_selectv_dl(nchans, &args, 0, INT64_MAX, true);
    va_end(args);
    async_error_guard(ret);
    return ret;
}

/// Same as neco_chan_tryselect() but uses an array for arguments.
int neco_chan_tryselectv(int nchans, struct neco_chan *chans[]) {
    int ret = chan_selectv_dl(nchans, 0, chans, 0, true);
    async_error_guard(ret);
    return ret;
}

static int chan_case(struct neco_chan *chan, void *data) {
    if (!chan) {
        return NECO_INVAL;
    } else if (!rt || chan->rtid != rt->id) {
        return NECO_PERM;
    } else if (!chan->lok) {
        return NECO_CLOSED;
    }
    if (chan->msgsize) {
        memcpy(data, cbufslot(chan, chan->bufcap), (size_t)chan->msgsize);
    }
    return NECO_OK;
}

/// Receive the message after a successful neco_chan_select().
/// See neco_chan_select() for an example.
/// @param chan The channel
/// @param data The data
/// @return NECO_OK Success
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_CLOSED Channel closed
int neco_chan_case(struct neco_chan *chan, void *data) {
    int ret = chan_case(chan, data);
    error_guard(ret);
    return ret;
}

struct getaddrinfo_args {
    atomic_int returned;
    char *node;
    char *service;
    struct addrinfo *hints;
    struct addrinfo *res;
    int fds[2];
    int ret;
    int errnum;
#ifdef __FreeBSD__
    void *dlhandle;
#endif
};

static void gai_args_free(struct getaddrinfo_args *args) {
    if (args) {
        free0(args->service);
        free0(args->node);
        free0(args->hints);
        freeaddrinfo(args->res);
        if (args->fds[0]) {
            must(close(args->fds[0]) == 0);
        }
        if (args->fds[1]) {
            must(close(args->fds[1]) == 0);
        }
#ifdef __FreeBSD__
        if (args->dlhandle) {
            dlclose(args->dlhandle);
        }
#endif
        free0(args);
    }
}

static struct getaddrinfo_args *gai_args_new(const char *node, 
    const char *service, const struct addrinfo *hints)
{
    struct getaddrinfo_args *args = malloc0(sizeof(struct getaddrinfo_args));
    if (!args) {
        return NULL;
    }
    memset(args, 0, sizeof(struct getaddrinfo_args));
    if (node) {
        size_t nnode = strlen(node);
        args->node = malloc0(nnode+1);
        if (!args->node) {
            gai_args_free(args);
            return NULL;
        }
        memcpy(args->node, node, nnode+1);
    }
    if (service) {
        size_t nservice = strlen(service);
        args->service = malloc0(nservice+1);
        if (!args->service) {
            gai_args_free(args);
            return NULL;
        }
        memcpy(args->service, service, nservice+1);
    }
    if (hints) {
        args->hints = malloc0(sizeof(struct addrinfo));
        if (!args->hints) {
            gai_args_free(args);
            return NULL;
        }
        memset(args->hints, 0, sizeof(struct addrinfo));
        args->hints->ai_flags = hints->ai_flags;
        args->hints->ai_family = hints->ai_family;
        args->hints->ai_socktype = hints->ai_socktype;
        args->hints->ai_protocol = hints->ai_protocol;
        args->hints->ai_next = NULL;
    }
    return args;
}

static atomic_int getaddrinfo_th_counter = 0;

#ifdef NECO_TESTING
int neco_getaddrinfo_nthreads(void) {
    return atomic_load(&getaddrinfo_th_counter);
}
#endif

static void *getaddrinfo_th(void *v) {
    struct getaddrinfo_args *a = v;
    a->ret = getaddrinfo(a->node, a->service, a->hints, &a->res);
    a->errnum = errno;
    must(write(a->fds[1], &(int){1}, sizeof(int)) == sizeof(int));
    while (!atomic_load(&a->returned)) {
        sched_yield();
    }
    gai_args_free(a);
    atomic_fetch_sub(&getaddrinfo_th_counter, 1);
    return NULL;
}

static bool is_ip_address(const char *addr) {
    bool ok = false;
    if (addr) {
        struct in6_addr result;
        ok = inet_pton(AF_INET, addr, &result) == 1 || 
             inet_pton(AF_INET6, addr, &result) == 1;
    }
    return ok;
}

static void cleanup_gai(void *ptr) {
    // This notifies the gai thread that it should return. The thread itself
    // will do the actual cleanup of resources.
    struct getaddrinfo_args *args = ptr;
    atomic_store(&args->returned, 1);
}

#ifdef _WIN32
int pipe1(int fds[2]) {
    return _pipe(fds, 64, _O_BINARY);
}
#else
#define pipe1 pipe0
#endif

static int getaddrinfo_dl(const char *node, const char *service,
    const struct addrinfo *hints, struct addrinfo **res, int64_t deadline)
{
    struct coroutine *co = coself();
    if (!co) {
        errno = EPERM;
        return EAI_SYSTEM;
    }
    int ret = checkdl(co, deadline);
    if (ret != NECO_OK) {
        errno = ret == NECO_CANCELED ? ECANCELED : ETIMEDOUT;
        return EAI_SYSTEM;
    }
    if (is_ip_address(node)) {
        // This is a simple address. Since there's no DNS lookup involved we
        // can use the standard getaddrinfo function without worrying about
        // much delay.
        return getaddrinfo(node, service, hints, res);
    }

    // DNS lookup is probably needed. This may cause network usage and who
    // knows how long it will take to return. Here we'll use a background
    // thread to do the work and we'll wait by using a local pipe.
    struct getaddrinfo_args *args = gai_args_new(node, service, hints);
    if (!args) {
        return EAI_MEMORY;
    }

    // Create a pipe to communicate with the thread. This allows us to use the
    // async neco_read() operation, which includes builtin deadline and cancel
    // support. Using a traditional pthread mutexes or cond variables are not
    // an option.
    if (pipe1(args->fds) == -1) {
        gai_args_free(args);
        return EAI_SYSTEM;
    }
#ifndef _WIN32
    if (neco_setnonblock(args->fds[0], true, 0) == -1) {
        gai_args_free(args);
        return EAI_SYSTEM;
    }
#endif

    pthread_t th;
#ifdef __FreeBSD__
    // The pthread functions are not included in libc for FreeBSD, dynamically
    // load the function instead.
    int (*pthread_create)(pthread_t*,const pthread_attr_t*,void*(*)(void*),
        void*);
    int (*pthread_detach)(pthread_t);
    args->dlhandle = dlopen("/usr/lib/libpthread.so", RTLD_LAZY);
    if (!args->dlhandle) {
        gai_args_free(args);
        return EAI_SYSTEM;
    }
    pthread_create = (int(*)(pthread_t*,const pthread_attr_t*,void*(*)(void*),
        void*))dlsym(args->dlhandle, "pthread_create");
    pthread_detach = (int(*)(pthread_t))
        dlsym(args->dlhandle, "pthread_detach");
    if (!pthread_create || !pthread_detach) {
        gai_args_free(args);
        return EAI_SYSTEM;
    }
#endif
    atomic_fetch_add(&getaddrinfo_th_counter, 1);
    ret = pthread_create0(&th, 0, getaddrinfo_th, args);
    if (ret != 0) {
        errno = ret;
        gai_args_free(args);
        atomic_fetch_sub(&getaddrinfo_th_counter, 1);
        return EAI_SYSTEM;
    }
    must(pthread_detach0(th) == 0);

    // At this point the args are owned by the thread and all cleanup will
    // happen there. The thread will complete the getaddrinfo operation.
    // Then we'll steal the result and return it to the caller.
    neco_cleanup_push(cleanup_gai, args);
    int ready = 0;
    ssize_t n = neco_read_dl(args->fds[0], &ready, sizeof(int), deadline);
    ret = EAI_SYSTEM;
    if (n != -1) {
        must(ready == 1 && n == sizeof(int));
        *res = args->res;
        args->res = NULL;
        ret = args->ret;
        errno = args->errnum;
        coyield();
    }
    neco_cleanup_pop(1);
    return ret;
}

/// Same as neco_getaddrinfo() but with a deadline parameter.
int neco_getaddrinfo_dl(const char *node, const char *service,
    const struct addrinfo *hints, struct addrinfo **res, int64_t deadline)
{
    int ret = getaddrinfo_dl(node, service, hints, res, deadline);
    int err;
    if (ret == 0) {
        err = NECO_OK;
    } else if (ret == EAI_SYSTEM) {
        err = NECO_ERROR;
    } else {
        err = NECO_GAIERROR;
    }
    async_error_guard(err);
    return ret;
}

/// The getaddrinfo() function is used to get a list of addresses and port
/// numbers for node (hostname) and service.
/// 
/// This is functionally identical to the Posix getaddrinfo function with the
/// exception that it does not block, allowing for usage in a Neco coroutine.
///
/// @return On success, 0 is returned
/// @return On error, a nonzero error code defined by the system. See the link
///         below for a list.
/// @see Posix
/// @see https://www.man7.org/linux/man-pages/man3/getaddrinfo.3.html
int neco_getaddrinfo(const char *node, const char *service,
    const struct addrinfo *hints, struct addrinfo **res)
{
    return neco_getaddrinfo_dl(node, service, hints, res, INT64_MAX);
}

// Returns a host and port.
// The host will need to be freed by the caller.  
static int parse_tcp_addr(const char *addr, char **host, const char **port) {
    size_t naddr = strlen(addr);
    if (naddr == 0) {
        return NECO_INVAL;
    }
    const char *colon = NULL;
    for (size_t i = naddr-1; ; i--) {
        if (addr[i] == ':') {
            colon = addr+i;
            break;
        }
        if (i == 0) {
            break;
        }
    }
    if (!colon) {
        return NECO_INVAL;
    }
    *port = colon+1;
    naddr = (size_t)(colon-addr);
    if (addr[0] == '[' && addr[naddr-1] == ']') {
        addr++;
        naddr -= 2;
    }
    *host = malloc0(naddr+1);
    if (!*host) {
        return NECO_NOMEM;
    }
    memcpy(*host, addr, naddr);
    (*host)[naddr] = '\0';
    return NECO_OK;
}

int neco_errconv_from_gai(int errnum) {
    switch (errnum) {
    case EAI_MEMORY:
        return NECO_NOMEM;
    case EAI_SYSTEM:
        return neco_errconv_from_sys();
    default:
        neco_gai_errno = errnum;
        return NECO_GAIERROR;
    }
}

static void cleanup_free_host(void *host) {
    free0(host);
}

// Returns NECO errors
static int getaddrinfo_from_tcp_addr_dl(const char *addr, int tcp_vers, 
    struct addrinfo **res, int64_t deadline)
{
    char *host = NULL;
    const char *port = 0;
    int ret = parse_tcp_addr(addr, &host, &port);
    if (ret != NECO_OK) {
        return ret;
    }
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family = tcp_vers;
    struct addrinfo *ainfo = NULL;
    const char *vhost = host;
    if (vhost[0] == '\0') {
        if (tcp_vers == AF_INET6) {
            vhost = "::";
        } else {
            vhost = "0.0.0.0";
        }
    }
    neco_cleanup_push(cleanup_free_host, host);
    ret = neco_getaddrinfo_dl(vhost, port, &hints, &ainfo, deadline);
    neco_cleanup_pop(1);
    if (ret != 0) {
        return neco_errconv_from_gai(ret);
    }
    // It has been observed on Linux that a getaddrinfo can successfully return
    // but with an empty result. Let's check for that case and ensure an error
    // is returned to the caller.
    neco_gai_errno = EAI_FAIL;
    ret = NECO_GAIERROR;
    *res = 0;
    if (ainfo) {
        *res = ainfo;
        ret = NECO_OK; 
        neco_gai_errno = EAI_FAIL;
    }
    return ret;
}



static int getaddrinfo_from_tcp_addr_dl(const char *addr, int tcp_vers, 
    struct addrinfo **res, int64_t deadline);

int neco_errconv_from_sys(void);

static int setnonblock(int fd, bool nonblock, bool *oldnonblock) {
#ifdef _WIN32
    // There's no way to detect if a socket is in non-blocking mode in Windows.
    return ioctlsocket(fd, FIONBIO, &(unsigned long){ nonblock });
#else
    int flags = fcntl0(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    if (oldnonblock) {
        *oldnonblock = (flags & O_NONBLOCK) == O_NONBLOCK;
    }
    if (nonblock) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl0(fd, F_SETFL, flags);
#endif
}

/// Change the non-blocking state for a file descriptor.
/// @see Posix2
int neco_setnonblock(int fd, bool nonblock, bool *oldnonblock) {
    int ret = setnonblock(fd, nonblock, oldnonblock);
    error_guard(ret);
    return ret;
}

static void cleanup_close_socket(void *arg) {
    close(*(int*)arg);
}

static int dial_connect_dl(int domain, int type, int protocol, 
    const struct sockaddr *addr, socklen_t addrlen, int64_t deadline)
{
    int fd = socket0(domain, type, protocol);
    if (fd == -1) {
        return -1;
    }
    if (neco_setnonblock(fd, true, 0) == -1) {
        close(fd);
        return -1;
    }
    int ret;
    neco_cleanup_push(cleanup_close_socket, &fd);
    ret = neco_connect_dl(fd, addr, addrlen, deadline);
    if (ret == 0) {
        ret = fd;
        fd = -1;
    }
    neco_cleanup_pop(1);
    return ret;
}

static void cleanup_addrinfo(void *arg) {
    struct addrinfo *ainfo = arg;
    freeaddrinfo(ainfo);
}

static int dial_tcp_dl(const char *addr, int tcp_vers, int64_t deadline) {
    struct addrinfo *ainfo;
    int ret = getaddrinfo_from_tcp_addr_dl(addr, tcp_vers, &ainfo, deadline);
    if (ret != NECO_OK) {
        return ret;
    }
    int fd;
    neco_cleanup_push(cleanup_addrinfo, ainfo);
    struct addrinfo *ai = ainfo;
    do {
        fd = dial_connect_dl(ai->ai_family, ai->ai_socktype, ai->ai_protocol, 
            ai->ai_addr, ai->ai_addrlen, deadline);
        if (fd != -1) {
            break;
        }
        ai = ai->ai_next;
    } while (ai);
    if (fd == -1) {
        fd = neco_errconv_from_sys();
    }
    neco_cleanup_pop(1);
    return fd;
}

static int dial_unix_dl(const char *addr, int64_t deadline) {
    (void)addr; (void)deadline;
#ifdef _WIN32
    return NECO_PERM;
#else
    struct sockaddr_un unaddr = { .sun_family = AF_UNIX };
    size_t naddr = strlen(addr);
    if (naddr > sizeof(unaddr.sun_path) - 1) {
        return NECO_INVAL;
    }
    strncpy(unaddr.sun_path, addr, sizeof(unaddr.sun_path) - 1);
    int fd = dial_connect_dl(AF_UNIX, SOCK_STREAM, 0, (void*)&unaddr,
        sizeof(struct sockaddr_un), deadline);
    if (fd == -1) {
        fd = neco_errconv_from_sys();
    }
    return fd;
#endif
}

static int dial_dl(const char *network, const char *address, int64_t deadline) {
    if (neco_getid() <= 0) {
        return NECO_PERM; 
    } else if (!network || !address) {
        return NECO_INVAL;
    } else if (strcmp(network, "tcp") == 0) {
        return dial_tcp_dl(address, 0, deadline);
    } else if (strcmp(network, "tcp4") == 0) {
        return dial_tcp_dl(address, AF_INET, deadline);
    } else if (strcmp(network, "tcp6") == 0) {
        return dial_tcp_dl(address, AF_INET6, deadline);
    } else if (strcmp(network, "unix") == 0) {
        return dial_unix_dl(address, deadline);
    } else {
        return NECO_INVAL;
    }
}

/// Same as neco_dial() but with a deadline parameter. 
int neco_dial_dl(const char *network, const char *address, int64_t deadline) {
    int ret = dial_dl(network, address, deadline);
    error_guard(ret);
    return ret;
}

/// Connect to a remote server.
///
/// **Example**
///
/// ```c 
/// int fd = neco_dial("tcp", "google.com:80");
/// if (fd < 0) {
///    // .. error, do something with it.
/// }
/// // Connected to google.com. Use neco_read(), neco_write(), or create a 
/// // stream using neco_stream_make(fd).
/// close(fd);
/// ```
/// @param network must be "tcp", "tcp4", "tcp6", or "unix".
/// @param address the address to dial
/// @return On success, file descriptor (non-blocking)
/// @return On error, Neco error
/// @see Networking
/// @see neco_serve()
int neco_dial(const char *network, const char *address) {
    return neco_dial_dl(network, address, INT64_MAX);
}

static int listen_tcp_dl(const char *addr, int tcp_vers, int64_t deadline) {
    struct addrinfo *ainfo;
    int ret = getaddrinfo_from_tcp_addr_dl(addr, tcp_vers, &ainfo, deadline);
    if (ret != NECO_OK) {
        return ret;
    }
    int fd = socket0(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    bool ok = fd != -1 && setsockopt0(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, 
        sizeof(int)) != -1;
    ok = ok && bind0(fd, ainfo->ai_addr, ainfo->ai_addrlen) != -1;
    freeaddrinfo(ainfo);
    ok = ok && listen0(fd, SOMAXCONN) != -1;
    ok = ok && neco_setnonblock(fd, true, 0) != -1;
    if (!ok) {
        if (fd != -1) {
            close(fd);
            fd = -1;
        }
    }
    return fd;
}

static int listen_unix_dl(const char *addr, int64_t deadline) {
    (void)addr; (void)deadline;
#ifdef _WIN32
    return NECO_PERM;
#else
    int fd = socket0(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        return NECO_ERROR;
    }
    struct sockaddr_un unaddr;
    memset(&unaddr, 0, sizeof(struct sockaddr_un));
    unaddr.sun_family = AF_UNIX;
    strncpy(unaddr.sun_path, addr, sizeof(unaddr.sun_path) - 1);
    if (bind0(fd, (void*)&unaddr, sizeof(struct sockaddr_un)) == -1) {
        close(fd);
        return NECO_ERROR;
    }
    if (listen0(fd, SOMAXCONN) == -1) {
        close(fd);
        return NECO_ERROR;
    }
    if (neco_setnonblock(fd, true, 0) == -1) {
        close(fd);
        return NECO_ERROR;
    }
    return fd;
#endif
}

static int serve_dl(const char *network, const char *address, int64_t deadline)
{
    if (!network || !address) {
        return NECO_INVAL;
    } else if (neco_getid() <= 0) {
        return NECO_PERM; 
    } else if (strcmp(network, "tcp") == 0) {
        return listen_tcp_dl(address, 0, deadline);
    } else if (strcmp(network, "tcp4") == 0) {
        return listen_tcp_dl(address, AF_INET, deadline);
    } else if (strcmp(network, "tcp6") == 0) {
        return listen_tcp_dl(address, AF_INET6, deadline);
    } else if (strcmp(network, "unix") == 0) {
        return listen_unix_dl(address, deadline);
    } else {
        return NECO_INVAL;
    }
}

/// Same as neco_serve() but with a deadline parameter. 
int neco_serve_dl(const char *network, const char *address, int64_t deadline) {
    int ret = serve_dl(network, address, deadline);
    async_error_guard(ret);
    return ret;
}


/// Listen on a local network address.
///
/// **Example**
///
/// ```c 
/// int servefd = neco_serve("tcp", "127.0.0.1:8080");
/// if (servefd < 0) {
///    // .. error, do something with it.
/// }
/// while (1) {
///     int fd = neco_accept(servefd, 0, 0);
///     // client accepted
/// }
///
/// close(servefd);
/// ```
/// @param network must be "tcp", "tcp4", "tcp6", or "unix".
/// @param address the address to serve on
/// @return On success, file descriptor (non-blocking)
/// @return On error, Neco error
/// @see Networking
/// @see neco_dial()
int neco_serve(const char *network, const char *address) {
    return neco_serve_dl(network, address, INT64_MAX);
}

////////////////////////////////////////////////////////////////////////////////
// sync 
////////////////////////////////////////////////////////////////////////////////

struct neco_mutex {
    _Alignas(16)         // needed for opaque type alias
    int64_t rtid;        // runtime id
    bool locked;         // mutex is locked (read or write)
    int  rlocked;        // read lock counter
    struct colist queue; // coroutine doubly linked list
};


static_assert(sizeof(neco_mutex) >= sizeof(struct neco_mutex), "");
static_assert(_Alignof(neco_mutex) == _Alignof(struct neco_mutex), "");

static int mutex_init(neco_mutex *mutex) {
    struct neco_mutex *mu = (void*)mutex;
    if (!mu) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    }
    memset(mu, 0, sizeof(struct neco_mutex));
    mu->rtid = rt->id;
    colist_init(&mu->queue);
    return NECO_OK;
}

int neco_mutex_init(neco_mutex *mutex) {
    int ret = mutex_init(mutex);
    error_guard(ret);
    return ret;
}

inline
static int check_mutex(struct coroutine *co, struct neco_mutex *mu) {
    if (!mu) {
        return NECO_INVAL;
    } else if (!co) {
        return NECO_PERM;
    } else if (mu->rtid == 0) {
        return neco_mutex_init((neco_mutex*)mu);
    } else if (rt->id != mu->rtid) {
        return NECO_PERM;
    }
    return NECO_OK;
}

inline
static int mutex_trylock(struct coroutine *co, struct neco_mutex *mu, 
    bool tryonly, int64_t deadline)
{
    if (!tryonly) {
        int ret = checkdl(co, deadline);
        if (ret != NECO_OK) {
            return ret;
        }
    }
    if (mu->locked) {
        return NECO_BUSY;
    }
    mu->locked = true;
    return NECO_OK;
}

inline
static int mutex_tryrdlock(struct coroutine *co, struct neco_mutex *mu,
    bool tryonly, int64_t deadline)
{
    if (!tryonly) {
        int ret = checkdl(co, deadline);
        if (ret != NECO_OK) {
            return ret;
        }
    }
    if (!colist_is_empty(&mu->queue) || (mu->rlocked == 0 && mu->locked)) {
        return NECO_BUSY;
    }
    mu->rlocked++;
    mu->locked = true;
    return NECO_OK;
}

int neco_mutex_trylock(neco_mutex *mutex) {
    struct coroutine *co = coself();
    struct neco_mutex *mu = (void*)mutex;
    int ret = check_mutex(co, mu);
    if (ret != NECO_OK) {
        return ret;
    }
    ret = mutex_trylock(co, mu, true, 0);
    async_error_guard(ret);
    return ret;
}

int neco_mutex_tryrdlock(neco_mutex *mutex) {
    struct coroutine *co = coself();
    struct neco_mutex *mu = (void*)mutex;
    int ret = check_mutex(co, mu);
    if (ret != NECO_OK) {
        return ret;
    }
    ret = mutex_tryrdlock(co, mu, true, 0);
    async_error_guard(ret);
    return ret;
}

noinline
static int finish_lock(struct coroutine *co, struct neco_mutex *mu, 
    bool rlocked, int64_t deadline)
{
    co->rlocked = rlocked;
    colist_push_back(&mu->queue, co);
    rt->nlocked++;
    copause(deadline);
    rt->nlocked--;
    remove_from_list(co);
    co->rlocked = false;
    return checkdl(co, INT64_MAX);
}

static int mutex_lock_dl(struct coroutine *co, struct neco_mutex *mu,
    int64_t deadline)
{
    int ret = mutex_trylock(co, mu, false, deadline);
    if (ret == NECO_BUSY) {
        // Another coroutine is holding this lock.
        ret = finish_lock(co, mu, false, deadline);
    }
    return ret;
}

int neco_mutex_lock_dl(neco_mutex *mutex, int64_t deadline) {
    struct coroutine *co = coself();
    struct neco_mutex *mu = (void*)mutex;
    int ret = check_mutex(co, mu);
    if (ret != NECO_OK) {
        return ret;
    }
    ret = mutex_lock_dl(co, mu, deadline);
    async_error_guard(ret);
    return ret;
}

int neco_mutex_lock(neco_mutex *mutex) {
    return neco_mutex_lock_dl(mutex, INT64_MAX);
}

int neco_mutex_rdlock_dl(neco_mutex *mutex, int64_t deadline) {
    struct coroutine *co = coself();
    struct neco_mutex *mu = (void*)mutex;
    int ret = check_mutex(co, mu);
    if (ret != NECO_OK) {
        return ret;
    }
    ret = mutex_tryrdlock(co, mu, false, deadline);
    if (ret == NECO_BUSY) {
        // Another coroutine is holding this lock.
        ret = finish_lock(co, mu, true, deadline);
    }
    async_error_guard(ret);
    return ret;
}

int neco_mutex_rdlock(neco_mutex *mutex) {
    return neco_mutex_rdlock_dl(mutex, INT64_MAX);
}

static void mutex_fastunlock(struct neco_mutex *mu) {
    if (!mu->locked) {
        return;
    }
    if (mu->rlocked > 0) {
        // This lock is currently being used by a reader.
        mu->rlocked--;
        if (mu->rlocked > 0) {
            // There are still more readers using this lock.
            return;
        }
    }
    if (colist_is_empty(&mu->queue)) {
        // There are no more coroutines in the queue.
        mu->locked = false;
        return;
    }
    // Choose next coroutine to take the lock.
    while (1) {
        struct coroutine *co = colist_pop_front(&mu->queue);
        // Schedule the coroutines to resume.
        sched_resume(co);
        if (co->rlocked) {
            // The popped coroutine was a reader.
            mu->rlocked++;
            if (!colist_is_empty(&mu->queue) && mu->queue.head.next->rlocked) {
                // The next in queue is also reader. 
                // Allow it to continue too.
                continue;
            }
        }
        break;
    }
    yield_for_sched_resume();
}

static int mutex_unlock(neco_mutex *mutex) {
    struct coroutine *co = coself();
    struct neco_mutex *mu = (void*)mutex;
    int ret = check_mutex(co, mu);
    if (ret != NECO_OK) {
        return ret;
    }
    mutex_fastunlock(mu);
    coyield();
    return NECO_OK;
}

int neco_mutex_unlock(neco_mutex *mutex) {
    int ret = mutex_unlock(mutex);
    error_guard(ret);
    return ret;
}

inline
static int mutex_fastlock(struct coroutine *co, struct neco_mutex *mu,
    int64_t deadline)
{
    if (!mu->locked) {
        mu->locked = true;
        return NECO_OK;
    }
    int ret = mutex_lock_dl(co, mu, deadline);
    async_error_guard(ret);
    return ret;
}

#ifdef NECO_TESTING
// An optimistic lock routine for testing only
int neco_mutex_fastlock(neco_mutex *mutex, int64_t deadline) {
    struct coroutine *co = coself();
    return mutex_fastlock(co, (struct neco_mutex *)mutex, deadline);
}
#endif

static int mutex_destroy(neco_mutex *mutex) {
    struct coroutine *co = coself();
    struct neco_mutex *mu = (void*)mutex;
    int ret = check_mutex(co, mu);
    if (ret != NECO_OK) {
        return ret;
    }
    if (mu->locked) {
        return NECO_BUSY;
    }
    memset(mu, 0, sizeof(struct neco_mutex));
    return NECO_OK;
}

int neco_mutex_destroy(neco_mutex *mutex) {
    int ret = mutex_destroy(mutex);
    error_guard(ret);
    return ret;
}

struct neco_waitgroup {
    _Alignas(16)          // needed for opaque type alias
    int64_t rtid;         // runtime id
    int count;            // current wait count
    struct colist queue;  // coroutine doubly linked list
};

static_assert(sizeof(neco_waitgroup) >= sizeof(struct neco_waitgroup), "");
static_assert(_Alignof(neco_waitgroup) == _Alignof(struct neco_waitgroup), "");

inline
static int check_waitgroup(struct neco_waitgroup *wg) {
    if (!wg) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    } else if (wg->rtid == 0) {
        return neco_waitgroup_init((neco_waitgroup*)wg);
    } else if (rt->id != wg->rtid) {
        return NECO_PERM;
    }
    return NECO_OK;
}

static int waitgroup_init(neco_waitgroup *waitgroup) {
    struct neco_waitgroup *wg = (void*)waitgroup;
    if (!wg) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    }
    memset(wg, 0, sizeof(struct neco_waitgroup));
    wg->rtid = rt->id;
    colist_init(&wg->queue);
    return NECO_OK;
}


int neco_waitgroup_init(neco_waitgroup *waitgroup) {
    int ret = waitgroup_init(waitgroup);
    error_guard(ret);
    return ret;
}

static int waitgroup_add(neco_waitgroup *waitgroup, int delta) {
    struct neco_waitgroup *wg = (void*)waitgroup;
    int ret = check_waitgroup(wg);
    if (ret != NECO_OK) {
        return ret;
    }
    int waiters = wg->count + delta;
    if (waiters < 0) {
        return NECO_NEGWAITGRP;
    }
    wg->count = waiters;
    return NECO_OK;
}


int neco_waitgroup_add(neco_waitgroup *waitgroup, int delta) {
    int ret = waitgroup_add(waitgroup, delta);
    error_guard(ret);
    return ret;
}

static int waitgroup_done(neco_waitgroup *waitgroup) {
    struct neco_waitgroup *wg = (void*)waitgroup;
    int ret = check_waitgroup(wg);
    if (ret != NECO_OK) {
        return ret;
    }
    if (wg->count == 0) {
        return NECO_NEGWAITGRP;
    }
    wg->count--;
    if (wg->count == 0 && !colist_is_empty(&wg->queue)) {
        struct coroutine *co = colist_pop_front(&wg->queue);
        if (colist_is_empty(&wg->queue)) {
            // Only one waiter. Do a quick switch.
            sco_resume(co->id);
        } else {
            // Many waiters. Batch them together.
            do {
                sched_resume(co);
                co = colist_pop_front(&wg->queue);  
            } while (co);
            yield_for_sched_resume();
        }
    }
    return NECO_OK;
}

int neco_waitgroup_done(neco_waitgroup *waitgroup) {
    int ret = waitgroup_done(waitgroup);
    error_guard(ret);
    return ret;
}

static int waitgroup_wait_dl(neco_waitgroup *waitgroup, int64_t deadline) {
    struct neco_waitgroup *wg = (void*)waitgroup;
    int ret = check_waitgroup(wg);
    if (ret != NECO_OK) {
        return ret;
    }
    struct coroutine *co = coself();
    ret = checkdl(co, deadline);
    if (ret != NECO_OK) {
        return ret;
    }
    if (wg->count == 0) {
        // It's probably a good idea to yield to another coroutine.
        coyield();
        return NECO_OK;
    }
    // The coroutine was added to the deadliner queue. We can safely
    // yield and wait to be woken at some point in the future.
    colist_push_back(&wg->queue, co);
    rt->nwaitgroupers++;
    copause(deadline);
    rt->nwaitgroupers--;
    remove_from_list(co);
    return checkdl(co, INT64_MAX);
}

int neco_waitgroup_wait_dl(neco_waitgroup *waitgroup, int64_t deadline) {
    int ret = waitgroup_wait_dl(waitgroup, deadline);
    async_error_guard(ret);
    return ret;
}

int neco_waitgroup_wait(neco_waitgroup *waitgroup) {
    return neco_waitgroup_wait_dl(waitgroup, INT64_MAX);
}

static int waitgroup_destroy(neco_waitgroup *waitgroup) {
    struct neco_waitgroup *wg = (void*)waitgroup;
    int ret = check_waitgroup(wg);
    if (ret != NECO_OK) {
        return ret;
    }
    memset(wg, 0, sizeof(struct neco_waitgroup));
    return NECO_OK;
}

int neco_waitgroup_destroy(neco_waitgroup *waitgroup) {
    int ret = waitgroup_destroy(waitgroup);
    error_guard(ret);
    return ret;
}

struct neco_cond {
    _Alignas(16)         // needed for opaque type alias
    int64_t rtid;        // runtime id
    struct colist queue; // coroutine doubly linked list
};

static_assert(sizeof(neco_cond) >= sizeof(struct neco_cond), "");
static_assert(_Alignof(neco_cond) == _Alignof(struct neco_cond), "");

static int cond_init0(struct neco_cond *cv) {
    memset(cv, 0, sizeof(struct neco_cond));
    cv->rtid = rt->id;
    colist_init(&cv->queue);
    return NECO_OK;
}

static int cond_init(neco_cond *cond) {
    struct neco_cond *cv = (void*)cond;
    if (!cv) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    }
    return cond_init0((struct neco_cond*)cond);
}

int neco_cond_init(neco_cond *cond) {
    int ret = cond_init(cond);
    error_guard(ret);
    return ret;
}

inline
static int check_cond(struct neco_cond *cv) {
    if (!cv) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    } else if (cv->rtid == 0) {
        return cond_init0(cv);
    } else if (rt->id != cv->rtid) {
        return NECO_PERM;
    }
    return NECO_OK;
}

static int cond_destroy(neco_cond *cond) {
    struct neco_cond *cv = (void*)cond;
    int ret = check_cond(cv);
    if (ret != NECO_OK) {
        return ret;
    }
    memset(cv, 0, sizeof(struct neco_cond));
    return NECO_OK;
}

int neco_cond_destroy(neco_cond *cond) {
    int ret = cond_destroy(cond);
    error_guard(ret);
    return ret;
}

static int cond_signal(neco_cond *cond) {
    struct neco_cond *cvar = (void*)cond;
    int ret = check_cond(cvar);
    if (ret != NECO_OK) {
        return ret;
    }
    struct coroutine *co = colist_pop_front(&cvar->queue);
    if (co) {
        sco_resume(co->id);
    }
    return NECO_OK;
}

int neco_cond_signal(neco_cond *cond) {
    int ret = cond_signal(cond);
    error_guard(ret);
    return ret;
}

static int cond_broadcast(neco_cond *cond) {
    struct neco_cond *cvar = (void*)cond;
    int ret = check_cond(cvar);
    if (ret != NECO_OK) {
        return ret;
    }
    struct coroutine *co = colist_pop_front(&cvar->queue);
    while (co) {
        sched_resume(co);
        co = colist_pop_front(&cvar->queue);
    }
    yield_for_sched_resume();
    return NECO_OK;
}

int neco_cond_broadcast(neco_cond *cond) {
    int ret = cond_broadcast(cond);
    error_guard(ret);
    return ret;
}

static int cond_wait_dl(neco_cond *cond, neco_mutex *mutex, int64_t deadline) {
    struct neco_cond *cvar = (struct neco_cond*)cond;
    int ret = check_cond(cvar);
    if (ret != NECO_OK) {
        return ret;
    }
    struct coroutine *co = coself();
    struct neco_mutex *mu = (struct neco_mutex*)mutex;
    ret = check_mutex(co, mu);
    if (ret != NECO_OK) {
        return ret;
    }
    if (co->canceled) {
        co->canceled = false;
        return NECO_CANCELED;
    }
    mutex_fastunlock(mu);
    // The coroutine was added to the deadliner queue. We can safely
    // yield and wait to be woken at some point in the future.
    colist_push_back(&cvar->queue, co);
    rt->ncondwaiters++;
    copause(deadline);
    rt->ncondwaiters--;
    remove_from_list(co);
    ret = checkdl(co, INT64_MAX);
    // Must relock.
    while (mutex_fastlock(co, mu, INT64_MAX) != NECO_OK) { }
    return ret;
}

int neco_cond_wait_dl(neco_cond *cond, neco_mutex *mutex, int64_t deadline) {
    int ret = cond_wait_dl(cond, mutex, deadline);
    async_error_guard(ret);
    return ret;
}

int neco_cond_wait(neco_cond *cond, neco_mutex *mutex) {
    return neco_cond_wait_dl(cond, mutex, INT64_MAX);
}

// Returns a string that indicates which coroutine method is being used by
// the program. Such as "asm,aarch64" or "ucontext", etc.
const char *neco_switch_method(void) {
    return sco_info_method();
}

static int setcanceltype(int type, int *oldtype) {
    if (type != NECO_CANCEL_ASYNC && type != NECO_CANCEL_INLINE) {
        return NECO_INVAL;
    }
    struct coroutine *co = coself();
    if (!co) {
        return NECO_PERM;
    }
    if (oldtype) {
        *oldtype = co->canceltype;
    }
    co->canceltype = type;
    return NECO_OK;
}

int neco_setcanceltype(int type, int *oldtype) {
    int ret = setcanceltype(type, oldtype);
    error_guard(ret);
    return ret;
}

static int setcancelstate(int state, int *oldstate) {
    if (state != NECO_CANCEL_ENABLE && state != NECO_CANCEL_DISABLE) {
        return NECO_INVAL;
    }
    struct coroutine *co = coself();
    if (!co) {
        return NECO_PERM;
    }
    if (oldstate) {
        *oldstate = co->cancelstate;
    }
    co->cancelstate = state;
    return NECO_OK;
}

int neco_setcancelstate(int state, int *oldstate) {
    int ret = setcancelstate(state, oldstate);
    error_guard(ret);
    return ret;
}

static void cleanup_push(struct cleanup *handler, void (*routine)(void *),
    void *arg)
{
    struct coroutine *co = coself();
    handler->routine = routine;
    handler->arg = arg;
    handler->next = co->cleanup;
    co->cleanup = handler;
}

static void cleanup_pop(int execute) {
    struct coroutine *co = coself();
    struct cleanup *handler = co->cleanup;
    co->cleanup = handler->next;
    if (execute && handler->routine) {
        handler->routine(handler->arg);
    }
}

static_assert(sizeof(struct cleanup) <= 32, "");

void __neco_c0(void *cl, void (*routine)(void *), void *arg) {
    cleanup_push(cl, routine, arg);
}

void __neco_c1(int execute) {
    cleanup_pop(execute);
}

// coexit is performed at the exit of every coroutine. 
// Most general coroutine cleanup is performed here because this operation
// always runs from inside of the coroutine context/stack and during a standard
// runtime scheduler step.
// The async flag indicates that the coroutine is being exited before the entry
// function has been completed. In this case the cleanup push/pop stack stack
// will immediately be unrolled and the coroutine will cease execution.
noinline
static void coexit(bool async) {
    // The current coroutine _must_ exit.
    struct coroutine *co = coself();

    if (async) {
        // Run the cleanup stack
        while (co->cleanup) {
            cleanup_pop(1);
        }
    }

    // Delete from map 
    comap_delete(&rt->all, co);

    // Notify all cancel waiters (if any)
    bool sched = false;
    struct coroutine *cowaiter = colist_pop_front(&co->cancellist);
    while (cowaiter) {
        sched_resume(cowaiter);
        sched = true;
        cowaiter = colist_pop_front(&co->cancellist);
    }

    // Notify all join waiters (if any)
    cowaiter = colist_pop_front(&co->joinlist);
    while (cowaiter) {
        sched_resume(cowaiter);
        sched = true;
        cowaiter = colist_pop_front(&co->joinlist);
    }

    // Close the generator
    if (co->gen) {
        chan_close((void*)co->gen);
        chan_fastrelease((void*)co->gen);
        co->gen = 0;
    }

    // Free the call arguments
    cofreeargs(co);

    if (sched) { 
        yield_for_sched_resume();
    }

    if (async) {
        sco_exit();
    }
}

static void co_pipe(int argc, void *argv[]) {
    must(argc == 3);
    char *path = argv[0];
    int64_t secret = *(int64_t*)argv[1];
    int *fd_out = argv[2];
    int fd = neco_dial("unix", path);
    neco_write(fd, &secret, 8);
    *fd_out = fd;
}

static int _pipe_(int pipefd[2]) {
    if (!pipefd) {
        errno = EINVAL;
        return -1;
    }
    if (!rt) {
        errno = EPERM;
        return -1;
    }
    int oldstate = 0;
    neco_setcancelstate(NECO_CANCEL_DISABLE, &oldstate);
    int fd0 = -2;
    int fd1 = -2;
    int ret = -1;
    int64_t secret;
    uint64_t tmpkey;
    neco_rand(&secret, 8, NECO_PRNG);
    neco_rand(&tmpkey, 8, NECO_PRNG);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/tmp/neco.%" PRIu64 ".sock", tmpkey);
    int ln = neco_serve("unix", path);
    int64_t childid = 0;
    if (ln > 0) {
        // State a coroutine to connect to the listener.
        int nret = neco_start(co_pipe, 3, path, &secret, &fd0);
        neco_errconv_to_sys(nret);
        if (nret == NECO_OK) {
            childid = neco_lastid();
            int64_t dl = neco_now() + NECO_SECOND * 5;
            fd1 = neco_accept_dl(ln, 0, 0, dl);
            neco_errconv_to_sys(fd1);
            if (fd1 > 0) {
                // Other side of pipe is connected.
                // Waiting for the handshake.
                int64_t data;
                ssize_t n = neco_read_dl(fd1, &data, 8, dl);
                neco_errconv_to_sys(n);
                if (n == 8 && data == secret) {
                    // Handshake good.
                    ret = 0;
                }
            }
        }
        close(ln);
        unlink(path);
    }
    int perrno = errno;
    if (ret == 0) {
        neco_join(childid);
        pipefd[0] = fd0;
        pipefd[1] = fd1;
        fd0 = -1;
        fd1 = -1;
    }
    close(fd0);
    close(fd1);
    neco_setcancelstate(oldstate, 0);
    errno = perrno;
    return ret;
}

/// Create a bidirection data channel for communicating between threads.
/// @param pipefd The array pipefd is used to return two file descriptors 
//  referring to the ends of the pipe.
/// @return On success, zero is returned. On error, -1 is returned, errno is
/// set to indicate the error, and pipefd is left unchanged.
/// @note This is the POSIX equivalent of `pipe()` with the following changes:
/// The original `pipe()` function only creates a unidirectional data channel
/// while this one is bidirectional, meaning that both file descriptors can
/// be used by different threads for both reading and writing, similar to a
/// socket. Also, the file descriptors are returned in non-blocking state.
int neco_pipe(int pipefd[2]) {
    int ret = _pipe_(pipefd);
    error_guard(ret);
    return ret;
}

static int join_dl(int64_t id, int64_t deadline) {
     struct coroutine *co = coself();
    if (!co) {
        return NECO_PERM;
    }
    struct coroutine *cotarg = cofind(id);
    if (!cotarg) {
        return NECO_OK;
    }
    int ret = checkdl(co, deadline);
    if (ret != NECO_OK) {
        return ret;
    }
    if (cotarg == co) {
        return NECO_PERM;
    }
    colist_push_back(&cotarg->joinlist, co);
    cotarg->njoinlist++;
    copause(deadline);
    remove_from_list(co);
    cotarg->njoinlist--;
    return checkdl(co, INT64_MAX);
}

/// Same as neco_join() but with a deadline parameter. 
int neco_join_dl(int64_t id, int64_t deadline) {
    int ret = join_dl(id, deadline);
    async_error_guard(ret);
    return ret;
}

/// Wait for a coroutine to terminate.
/// If that coroutine has already terminated or is not found, then this
/// operation returns immediately.
///
/// **Example**
///
/// ```
/// // Start a new coroutine
/// neco_start(coroutine, 0);
///
/// // Get the identifier of the new coroutine.
/// int64_t id = neco_lastid();
///
/// // Wait until the coroutine has terminated.
/// neco_join(id);
/// ```
///
/// @param id Coroutine identifier
/// @return NECO_OK Success
/// @return NECO_CANCELED Operation canceled
/// @return NECO_PERM Operation called outside of a coroutine
/// @see neco_join_dl()
int neco_join(int64_t id) {
    return neco_join_dl(id, INT64_MAX);
}

#define DEFAULT_BUFFER_SIZE 4096

struct bufrd {
    size_t len;
    size_t pos;
    char *data;
};

struct bufwr {
    size_t len;
    char *data;
};

struct neco_stream {
    int fd;
    int64_t rtid;
    bool buffered;

    struct bufrd rd;
    struct bufwr wr;
    size_t cap;
    char data[];
};

typedef struct neco_stream neco_stream;

static int stream_make_buffered_size(neco_stream **stream, int fd, bool buffered,
    size_t buffer_size)
{
    if (!stream || fd < 0) {
        return NECO_INVAL;
    } else if (!rt) {
        return NECO_PERM;
    }
    size_t memsize;
    if (buffered) {
        buffer_size = buffer_size == 0 ? DEFAULT_BUFFER_SIZE : buffer_size;
        memsize = sizeof(neco_stream) + buffer_size;
    } else {
        memsize = offsetof(neco_stream, rd);
    }
    *stream = malloc0(memsize);
    if (!*stream) {
        return NECO_NOMEM;
    }
    memset(*stream, 0, memsize);
    (*stream)->rtid = rt->id;
    (*stream)->fd = fd;
    (*stream)->buffered = buffered;
    if (buffered) {
        (*stream)->cap = buffer_size;
    }
    return NECO_OK;
}

int neco_stream_make_buffered_size(neco_stream **stream, int fd, 
    size_t buffer_size)
{
    int ret = stream_make_buffered_size(stream, fd, true, buffer_size);
    error_guard(ret);
    return ret;
}

int neco_stream_make_buffered(neco_stream **stream, int fd) {
    int ret = stream_make_buffered_size(stream, fd, true, 0);
    error_guard(ret);
    return ret;
}

int neco_stream_make(neco_stream **stream, int fd) {
    int ret = stream_make_buffered_size(stream, fd, false, 0);
    error_guard(ret);
    return ret;
}

static int stream_release(neco_stream *stream) {
    if (!stream) {
        return NECO_INVAL;
    } else if (!rt || stream->rtid != rt->id) {
        return NECO_PERM;
    }
    if (stream->buffered) {
        if (stream->rd.data && stream->rd.data != stream->data) {
            free0(stream->rd.data);
        }
        if (stream->wr.data && stream->wr.data != stream->data) {
            free0(stream->wr.data);
        }
    }
    free0(stream);
    return NECO_OK;
}

int neco_stream_release(neco_stream *stream) {
    int ret = stream_release(stream);
    error_guard(ret);
    return ret;
}

static bool ensure_rd_data(neco_stream *stream) {
    if (!stream->rd.data) {
        if (!stream->wr.data) {
            stream->rd.data = stream->data;
        } else {
            stream->rd.data = malloc0(stream->cap);
            if (!stream->rd.data) {
                return false;
            }
        }
    }
    return true;
}

static bool ensure_wr_data(neco_stream *stream) {
    if (!stream->wr.data) {
        if (!stream->rd.data) {
            stream->wr.data = stream->data;
        } else {
            stream->wr.data = malloc0(stream->cap);
            if (!stream->wr.data) {
                return false;
            }
        }
    }
    return true;
}

static ssize_t stream_read_dl(neco_stream *stream, void *data, size_t nbytes,
    int64_t deadline)
{
    if (!stream) {
        return NECO_INVAL;
    } else if (!rt || stream->rtid != rt->id) {
        return NECO_PERM;
    }
    if (!stream->buffered) {
        return neco_read_dl(stream->fd, data, nbytes, deadline);
    }
    if (!ensure_rd_data(stream)) {
        return NECO_NOMEM;
    }
    if (stream->rd.len == 0) {
        ssize_t n = neco_read_dl(stream->fd, stream->rd.data, stream->cap, 
            deadline);
        if (n == -1) {
            return neco_errconv_from_sys();
        }
        stream->rd.len = (size_t)n;
        stream->rd.pos = 0;
    }
    nbytes = stream->rd.len < nbytes ? stream->rd.len : nbytes;
    memcpy(data, stream->rd.data + stream->rd.pos, nbytes);
    stream->rd.pos += nbytes;
    stream->rd.len -= nbytes;
    return (ssize_t)nbytes;
}

ssize_t neco_stream_read_dl(neco_stream *stream, void *data, size_t nbytes,
    int64_t deadline)
{
    ssize_t ret = stream_read_dl(stream, data, nbytes, deadline);
    if (ret == 0 && nbytes > 0 && neco_lasterr() == NECO_OK) {
        ret = NECO_EOF;
    }
    error_guard(ret);
    return ret;
}

ssize_t neco_stream_read(neco_stream *stream, void *data, size_t nbytes) {
    return neco_stream_read_dl(stream, data, nbytes, INT64_MAX);
}

static ssize_t stream_readfull_dl(neco_stream *stream, void *data, 
    size_t nbytes, int64_t deadline)
{
    ssize_t nread = 0;
    do {
        ssize_t n = neco_stream_read_dl(stream, data, nbytes, deadline);
        if (n <= 0) {
            if (nread == 0) {
                nread = n;
            }
            return nread;
        } else {
            data = (char*)data + n;
            nbytes -= (size_t)n;
            nread += n;
        }
    } while (nbytes > 0);
    return nread;
}

ssize_t neco_stream_readfull_dl(neco_stream *stream, void *data, size_t nbytes,
    int64_t deadline)
{
    ssize_t ret = stream_readfull_dl(stream, data, nbytes, deadline);
    error_guard(ret);
    return ret;
}

ssize_t neco_stream_readfull(neco_stream *stream, void *data, size_t nbytes) {
    return neco_stream_readfull_dl(stream, data, nbytes, INT64_MAX);
}

static ssize_t stream_buffered_read_size(neco_stream *stream) {
    if (!stream) {
        return NECO_INVAL;
    } else if (!rt || stream->rtid != rt->id) {
        return NECO_PERM;
    } else if (!stream->buffered) {
        return 0;
    }
    return (ssize_t)stream->rd.len;
}

ssize_t neco_stream_buffered_read_size(neco_stream *stream) {
    ssize_t ret = stream_buffered_read_size(stream);
    error_guard(ret);
    return ret;
}

/// Same as neco_stream_read_byte() but with a deadline parameter.
int neco_stream_read_byte_dl(neco_stream *stream, int64_t deadline) {
    unsigned char byte;
    if (rt && stream && stream->rtid == rt->id && stream->buffered && 
        stream->rd.len > 0 && checkdl(coself(), deadline) == NECO_OK)
    {
        // fast-track read byte
        byte = (unsigned char)stream->rd.data[stream->rd.pos];
        stream->rd.pos++;
        stream->rd.len--;
    } else {
        ssize_t ret = neco_stream_read_dl(stream, &byte, 1, deadline);
        if (ret != 1) {
            return ret;
        }
    }
    return byte;
}

/// Read and returns a single byte. If no byte is available, returns an error.
int neco_stream_read_byte(neco_stream *stream) {
    return neco_stream_read_byte_dl(stream, INT64_MAX);
}

static int stream_unread_byte(neco_stream *stream) {
    if (!stream) {
        return NECO_INVAL;
    } else if (!rt || stream->rtid != rt->id) {
        return NECO_PERM;
    } else if (!stream->buffered || stream->rd.pos == 0) {
        return NECO_UNREADFAIL;
    }
    stream->rd.pos--;
    stream->rd.len++;
    return NECO_OK;
}

/// Unread the last byte. Only the most recently read byte can be unread.
int neco_stream_unread_byte(neco_stream *stream) {
    int ret = stream_unread_byte(stream);
    error_guard(ret);
    return ret;
}

static int stream_flush_dl(neco_stream *stream, int64_t deadline) {
    if (!stream) {
        return NECO_INVAL;
    } else if (!rt || stream->rtid != rt->id) {
        return NECO_PERM;
    } else if (!stream->buffered) {
        // Not buffered. Only need to check the deadline.
        return checkdl(coself(), deadline);
    }
    ssize_t n = neco_write_dl(stream->fd, stream->wr.data, stream->wr.len, 
        deadline);
    if (n <= 0) {
        if (n == 0 && stream->wr.len == 0) {
            return NECO_OK;
        } else {
            return neco_errconv_from_sys();
        }
    }
    if ((size_t)n < stream->wr.len) {
        // parital write.
        memmove(stream->wr.data, stream->wr.data+n, stream->wr.len-(size_t)n);
        stream->wr.len -= (size_t)n;
        return NECO_PARTIALWRITE;
    } else {
        stream->wr.len = 0;
        return NECO_OK;
    }
}

/// Same as neco_stream_flush() but with a deadline parameter.
int neco_stream_flush_dl(neco_stream *stream, int64_t deadline) {
    int ret = stream_flush_dl(stream, deadline);
    async_error_guard(ret);
    return ret;
}

/// Flush writes any buffered data to the underlying file descriptor.
int neco_stream_flush(neco_stream *stream) {
    return neco_stream_flush_dl(stream, INT64_MAX);
}

static ssize_t stream_write_dl(neco_stream *stream, const void *data, 
    size_t nbytes, int64_t deadline)
{
    if (!stream) {
        return NECO_INVAL;
    } else if (!rt || stream->rtid != rt->id) {
        return NECO_PERM;
    }
    if (!stream->buffered) {
        return neco_write_dl(stream->fd, data, nbytes, deadline);
    }
    if (!ensure_wr_data(stream)) {
        return NECO_NOMEM;
    }
    ssize_t nwritten = 0;
    while (nbytes > 0) {
        if (stream->wr.len == stream->cap) {
            // Buffer is full.
            int ret = neco_stream_flush_dl(stream, deadline);
            if (ret != NECO_OK) {
                if (nwritten == 0) {
                    nwritten = ret;
                }
                break;
            }
        }
        // Copy data into the buffer
        size_t n = stream->cap - stream->wr.len;
        n = n < nbytes ? n : nbytes;
        memcpy(stream->wr.data+stream->wr.len, data, n);
        stream->wr.len += n;
        data = (char*)data + n;
        nbytes -= n;
        nwritten += n;
    }
    return nwritten;
}

ssize_t neco_stream_write_dl(neco_stream *stream, const void *data, 
    size_t nbytes, int64_t deadline)
{
    ssize_t ret = stream_write_dl(stream, data, nbytes, deadline);
    async_error_guard(ret);
    return ret;
}

ssize_t neco_stream_write(neco_stream *stream, const void *data, size_t nbytes){
    return neco_stream_write_dl(stream, data, nbytes, INT64_MAX);
}

static ssize_t stream_buffered_write_size(neco_stream *stream) {
    if (!stream) {
        return NECO_INVAL;
    } else if (!rt || stream->rtid != rt->id) {
        return NECO_PERM;
    } else if (!stream->buffered) {
        return 0;
    }
    return (ssize_t)stream->wr.len;
}

ssize_t neco_stream_buffered_write_size(neco_stream *stream) {
    ssize_t ret = stream_buffered_write_size(stream);
    error_guard(ret);
    return ret;
}

static int stream_close_dl(neco_stream *stream, int64_t deadline) {
    if (!stream) {
        return NECO_INVAL;
    } else if (!rt || stream->rtid != rt->id) {
        return NECO_PERM;
    }
    int ret = NECO_OK;
    if (stream->buffered && (deadline < INT64_MAX || stream->wr.len > 0)) {
        ret = stream_flush_dl(stream, deadline);
    }
    close(stream->fd);
    stream_release(stream);
    return ret;
}

/// Close a stream with a deadline. 
/// A deadline is provided to accomodate for buffered streams that may need to
/// flush bytes on close
int neco_stream_close_dl(neco_stream *stream, int64_t deadline) {
    int ret = stream_close_dl(stream, deadline);
    error_guard(ret);
    return ret;
}

/// Close a stream
int neco_stream_close(neco_stream *stream) {
    return neco_stream_close_dl(stream, INT64_MAX);
}

// pcg-family random number generator

static int64_t rincr(int64_t seed) {
    return (int64_t)((uint64_t)(seed)*6364136223846793005 + 1);
}

static uint32_t rgen(int64_t seed) {
    uint64_t state = (uint64_t)seed;
    uint32_t xorshifted = (uint32_t)(((state >> 18) ^ state) >> 27);
    uint32_t rot = (uint32_t)(state >> 59);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

static uint32_t rnext(int64_t *seed) {
    *seed = rincr(rincr(*seed)); // twice called intentionally
    uint32_t value = rgen(*seed);
    return value;
}

static void pcgrandom_buf(void *data, size_t nbytes, int64_t *seed) {
    while (nbytes >= 4) {
        uint32_t value = rnext(seed);
        ((char*)data)[0] = ((char*)&value)[0];
        ((char*)data)[1] = ((char*)&value)[1];
        ((char*)data)[2] = ((char*)&value)[2];
        ((char*)data)[3] = ((char*)&value)[3];
        data = (char*)data + 4;
        nbytes -= 4;
    }
    if (nbytes > 0) {
        uint32_t value = rnext(seed);
        for (size_t i = 0; i < nbytes; i++) {
            ((char*)data)[i] = ((char*)&value)[i];
        }
    }
}

static int rand_setseed(int64_t seed, int64_t *oldseed) {
    if (!rt) {
        return NECO_PERM;
    }
    if (oldseed) {
        *oldseed = rt->rand_seed;
    }
    rt->rand_seed = seed;
    return NECO_OK;
}

/// Set the random seed for the Neco pseudorandom number generator.
///
/// The provided seed is only used for the (non-crypto) NECO_PRNG and is
/// ignored for NECO_CPRNG.
///
/// @param seed 
/// @param oldseed[out] The previous seed
/// @return NECO_OK Success
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @see Random
int neco_rand_setseed(int64_t seed, int64_t *oldseed) {
    int ret = rand_setseed(seed, oldseed);
    error_guard(ret);
    return ret;
}

static int rand_dl(void *data, size_t nbytes, int attr, int64_t deadline) {
    if (attr != NECO_CSPRNG && attr != NECO_PRNG) {
        return NECO_INVAL;
    }
    if (!rt) {
        return NECO_PERM;
    }
    void (*arc4random_buf_l)(void *, size_t) = 0;
    if (attr == NECO_CSPRNG) {
#ifdef __linux__
        // arc4random_buf should be available in libbsd for Linux.
        if (!rt->arc4random_buf) {
            if (!rt->libbsd_handle) {
                rt->libbsd_handle = dlopen("libbsd.so.0", RTLD_LAZY);
                must(rt->libbsd_handle);
            }
            rt->arc4random_buf = (void(*)(void*,size_t))
                dlsym(rt->libbsd_handle, "arc4random_buf");
            must(rt->arc4random_buf);
        }
        arc4random_buf_l = rt->arc4random_buf;
#elif defined(__APPLE__) || defined(__FreeBSD__)
        arc4random_buf_l = arc4random_buf;
#endif
    }
    struct coroutine *co = coself();
    while (1) {
        int ret = checkdl(co, deadline);
        if (ret != NECO_OK) {
            return ret;
        }
        if (nbytes == 0) {
            break;
        }
        size_t partsz = nbytes < 256 ? nbytes : 256;
        if (arc4random_buf_l) {
            arc4random_buf_l(data, partsz);
        } else {
            pcgrandom_buf(data, partsz, &rt->rand_seed);
        }
        nbytes -= partsz;
        data = (char*)data + partsz;
        if (nbytes == 0) {
            break;
        }
        coyield();
    }
    return NECO_OK;
}

/// Same as neco_rand() but with a deadline parameter.
int neco_rand_dl(void *data, size_t nbytes, int attr, int64_t deadline) {
    int ret = rand_dl(data, nbytes, attr, deadline);
    async_error_guard(ret);
    return ret;
}

/// Generator random bytes
///
/// This operation can generate cryptographically secure data by
/// providing the NECO_CSPRNG option or non-crypto secure data with 
/// NECO_PRNG. 
/// 
/// Non-crypto secure data use the [pcg-family](https://www.pcg-random.org) 
/// random number generator. 
///
/// @param data buffer for storing random bytes
/// @param nbytes number of bytes to generate
/// @param attr NECO_PRNG or NECO_CSPRNG
/// @return NECO_OK Success
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_CANCELED Operation canceled
int neco_rand(void *data, size_t nbytes, int attr) {
    return neco_rand_dl(data, nbytes, attr, INT64_MAX);
}

/// Terminate the current coroutine.
///
/// Any clean-up handlers established by neco_cleanup_push() that
/// have not yet been popped, are popped (in the reverse of the order
/// in which they were pushed) and executed.
///
/// Calling this from outside of a coroutine context does nothing and will be
/// treated effectivley as a no-op. 
void neco_exit(void) {
    if (rt) {
        coexit(true);
    }
}

struct neco_gen { 
    // A generator is actually a channel in disguise.
    int _;
};

static int gen_start_chk(neco_gen **gen, size_t data_size) {
    if (!gen || data_size > INT_MAX) {
        return NECO_INVAL;
    }
    if (!rt) {
        return NECO_PERM;
    }
    return NECO_OK;
}

/// Start a generator coroutine
///
/// **Example**
///
/// ```
/// void coroutine(int argc, void *argv[]) {
///     // Yield each int to the caller, one at a time.
///     for (int i = 0; i < 10; i++) {
///         neco_gen_yield(&i);
///     }
/// }
/// 
/// int neco_main(int argc, char *argv[]) {
///     
///     // Create a new generator coroutine that is used to send ints.
///     neco_gen *gen;
///     neco_gen_start(&gen, sizeof(int), coroutine, 0);
/// 
///     // Iterate over each int until the generator is closed.
///     int i;
///     while (neco_gen_next(gen, &i) != NECO_CLOSED) {
///         printf("%d\n", i); 
///     }
/// 
///     // This coroutine no longer needs the generator.
///     neco_gen_release(gen);
///     return 0;
/// }
/// ```
///
/// @param[out] gen Generator object
/// @param data_size Data size of messages
/// @param coroutine Generator coroutine
/// @param argc Number of arguments
/// @return NECO_OK Success
/// @return NECO_NOMEM The system lacked the necessary resources
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @note The caller is responsible for freeing the generator object with 
/// with neco_gen_release().
int neco_gen_start(neco_gen **gen, size_t data_size,
    void(*coroutine)(int argc, void *argv[]), int argc, ...)
{
    int ret = gen_start_chk(gen, data_size);
    if (ret == NECO_OK) {
        va_list args;
        va_start(args, argc);
        ret = startv(coroutine, argc, &args, 0, (void*)gen, data_size);
        va_end(args);
    }
    error_guard(ret);
    return ret;
}

/// Same as neco_gen_start() but using an array for arguments.
int neco_gen_startv(neco_gen **gen, size_t data_size,
    void(*coroutine)(int argc, void *argv[]), int argc, void *argv[])
{
    int ret = gen_start_chk(gen, data_size);
    if (ret == NECO_OK) {
        ret = startv(coroutine, argc, 0, argv, (void*)gen, data_size);
    }
    error_guard(ret);
    return ret;
}

/// Retain a reference of the generator so it can be shared with other
/// coroutines.
///
/// This is needed for avoiding use-after-free bugs.
/// 
/// See neco_gen_start() for an example.
///
/// @param gen The generator
/// @return NECO_OK Success
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @note The caller is responsible for releasing the reference with 
///       neco_gen_release()
/// @see Generators
/// @see neco_gen_release()
int neco_gen_retain(neco_gen *gen) {
    return neco_chan_retain((void*)gen);
}

/// Release a reference to a generator
///
/// See neco_gen_start() for an example.
///
/// @param gen The generator
/// @return NECO_OK Success
/// @return NECO_INVAL An invalid parameter was provided
/// @return NECO_PERM Operation called outside of a coroutine
/// @see Generators
/// @see neco_gen_retain()
int neco_gen_release(neco_gen *gen) {
    return neco_chan_release((void*)gen);
}

/// Same as neco_gen_yield() but with a deadline parameter.
int neco_gen_yield_dl(void *data, int64_t deadline) {
    struct coroutine *co = coself();
    if (!co) {
        return NECO_PERM;
    }
    if (!co->gen) {
        return NECO_NOTGENERATOR;
    }
    return neco_chan_send_dl((void*)co->gen, data, deadline);
}

/// Send a value to the generator for the next iteration.
///
/// See neco_gen_start() for an example.
int neco_gen_yield(void *data) {
    return neco_gen_yield_dl(data, INT64_MAX);
}

/// Same as neco_gen_next() but with a deadline parameter.
int neco_gen_next_dl(neco_gen *gen, void *data, int64_t deadline) {
    return neco_chan_recv_dl((void*)gen, data, deadline);
}

/// Receive the next value from a generator.
///
/// See neco_gen_start() for an example.
int neco_gen_next(neco_gen *gen, void *data) {
    return neco_gen_next_dl(gen, data, INT64_MAX);
}

/// Close the generator.
/// @param gen Generator
/// @return NECO_OK Success
int neco_gen_close(neco_gen *gen) {
    return neco_chan_close((void*)gen);
}

/// Test a neco error code. 
/// @return the provided value, unchanged.
int neco_testcode(int errcode) {
    int ret = errcode;
    error_guard(ret);
    return ret;
}

/// Stop normal execution of the current coroutine, print stack trace, and exit
/// the program immediately.
int neco_panic(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    panic("%s", buf);
    return NECO_OK;
}

static int suspend_dl(int64_t deadline) {
    struct coroutine *co = coself();
    if (!co) {
        return NECO_PERM;
    }

    co->suspended = true;
    rt->nsuspended++;
    copause(deadline);
    rt->nsuspended--;
    co->suspended = false;

    return checkdl(co, INT64_MAX);
}

/// Same as neco_suspend() but with a deadline parameter. 
int neco_suspend_dl(int64_t deadline) {
    int ret = suspend_dl(deadline);
    async_error_guard(ret);
    return ret;
}

/// Suspend the current coroutine.
/// @return NECO_OK Success
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_CANCELED Operation canceled
/// @see neco_resume
/// @see neco_suspend_dl
int neco_suspend(void) {
    return neco_suspend_dl(INT64_MAX);
}

static int resume(int64_t id) {
    if (!rt) {
        return NECO_PERM;
    }
    struct coroutine *co = cofind(id);
    if (!co) {
        return NECO_NOTFOUND;
    }
    if (!co->suspended) {
        return NECO_NOTSUSPENDED;
    }
    sco_resume(co->id);
    return NECO_OK;
}

/// Resume a suspended roroutine
/// @return NECO_OK Success
/// @return NECO_PERM Operation called outside of a coroutine
/// @return NECO_NOTFOUND Coroutine not found
/// @return NECO_NOTSUSPENDED Coroutine not suspended
/// @see neco_suspend
int neco_resume(int64_t id) {
    int ret = resume(id);
    error_guard(ret);
    return ret;
}

#ifndef NECO_NOWORKERS
struct iowork {
    void(*work)(void *udata);
    void *udata;
    struct coroutine *co;
    struct runtime *rt;
};

static void iowork(void *udata) {
    struct iowork *info = udata;
    info->work(info->udata);
    pthread_mutex_lock(&info->rt->iomu);
    colist_push_back(&info->rt->iolist, info->co);
    pthread_mutex_unlock(&info->rt->iomu);
}
#endif

static int workfn(int64_t pin, void(*work)(void *udata), void *udata) {
    (void)pin;
    struct coroutine *co = coself();
    if (!work) {
        return NECO_INVAL;
    }
    if (!co) {
        return NECO_PERM;
    }
#ifdef NECO_NOWORKERS
    // Run in foreground
    work(udata);
#else
    // Run in background
    struct iowork info = { .work = work, .udata = udata, .co = co, .rt = rt };
    rt->niowaiters++;
    while (!worker_submit(rt->worker, pin, iowork, &info)) {
        sco_yield();
    }
    sco_pause();
    rt->niowaiters--;
#endif
    return NECO_OK;
}

/// Perform work in a background thread and wait until the work is done.
/// 
/// This operation cannot be canceled and cannot timeout. It's the responibilty
/// of the caller to figure out a mechanism for doing those things from inside
/// of the work function.
/// 
/// The work function will not be inside of a Neco context, thus all `neco_*`
/// functions will fail if called from inside of the work function.
///
/// @param pin pin to a thread, or use -1 for round robin selection.
/// @param work the work, must not be null
/// @param udata any user data
/// @return NECO_OK Success
/// @return NECO_NOMEM The system lacked the necessary resources
/// @return NECO_INVAL An invalid parameter was provided
/// @note There is no way to cancel or timeout this operation
/// @note There is no way to cancel or timeout this operation
int neco_work(int64_t pin, void(*work)(void *udata), void *udata) {
    int ret = workfn(pin, work, udata);
    error_guard(ret);
    return ret;
}
