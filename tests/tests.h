#ifndef TESTS_H
#define TESTS_H

#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <sched.h>
#include <fcntl.h>
#include <unistd.h>
#include "../neco.h"

#ifdef _WIN32
#include <ntsecapi.h>
#endif

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
// Everything but Windows and use Emscripten can use fail counters
#define IS_FAIL_TARGET
#endif


#ifndef NECO_TESTING
#error NECO_TESTING not defined
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

atomic_int total_allocs = 0;
atomic_int total_mem = 0;

static bool rand_alloc_fail = false;
// 1 in 10 chance malloc or realloc will fail.
static int rand_alloc_fail_odds = 10; 

struct alloc_prefix {
    size_t size;
} __attribute__((aligned(16)));

static void *xmalloc(size_t size) {
    if (rand_alloc_fail && rand()%rand_alloc_fail_odds == 0) {
        return NULL;
    }
    void *mem = malloc(16+size);
    assert(mem);
    *(uint64_t*)mem = size;
    atomic_fetch_add(&total_allocs, 1);
    atomic_fetch_add(&total_mem, size);
    return (char*)mem+16;
}

static void xfree(void *ptr) {
    if (ptr) {
        size_t size = *(uint64_t*)((char*)ptr-16); 
        atomic_fetch_sub(&total_mem, size);
        atomic_fetch_sub(&total_allocs, 1);
        free((char*)ptr-16);
    }
}

static void *xrealloc(void *ptr, size_t size) {
    if (!ptr) {
        return xmalloc(size);
    }
    if (rand_alloc_fail && rand()%rand_alloc_fail_odds == 0) {
        return NULL;
    }
    size_t psize = *(uint64_t*)((char*)ptr-16);
    void *mem = realloc((char*)ptr-16, 16+size);
    assert(mem);
    *(uint64_t*)mem = size;
    total_mem -= psize;
    total_mem += size;
    return (char*)mem+16;
}

void init_test_allocator(bool random_failures) {
    rand_alloc_fail = random_failures;
    neco_env_setallocator(xmalloc, xrealloc, xfree);
}

void cleanup_test_allocator(void) {
    if (total_allocs > 0 || total_mem > 0) {
        fprintf(stderr, "test failed: %d unfreed allocations, %d bytes\n",
            (int)total_allocs, (int)total_mem);
        exit(1);
    }
    neco_env_setallocator(NULL, NULL, NULL);
}

static bool nameless_tests = false;
static void wait_for_threads(void);

uint64_t mkrandseed(void) {
    uint64_t seed = 0;
#ifdef _WIN32
    assert(RtlGenRandom(&seed, sizeof(uint64_t)));
#else
    FILE *urandom = fopen("/dev/urandom", "r");
    assert(urandom);
    assert(fread(&seed, sizeof(uint64_t), 1, urandom));
    fclose(urandom);
#endif
    return seed;
}

void seedrand(void) {
    uint64_t seed = mkrandseed();
    srand(seed);
}

double rand_double(void) {
    return (double)rand()/((double)(RAND_MAX)+1);
}

static int64_t getnow(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        fprintf(stderr, "clock_gettime: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return now.tv_sec * INT64_C(1000000000) + now.tv_nsec;
}

#define do_test_(name,for_neco) { \
    if (argc < 2 || strstr(#name, argv[1])) { \
        if (!nameless_tests) printf("%s\n", #name); \
        seedrand(); \
        if (for_neco) { \
            neco_env_setcanceltype(NECO_CANCEL_INLINE); \
            neco_env_setcancelstate(NECO_CANCEL_ENABLE); \
            init_test_allocator(false); \
        } \
        name(); \
        wait_for_threads(); \
        if (for_neco) { \
            cleanup_test_allocator(); \
            neco_env_setcanceltype(NECO_CANCEL_ASYNC); \
            neco_env_setcancelstate(NECO_CANCEL_ENABLE); \
        } \
    } \
}

#define do_test(name) \
    do_test_(name, true)




double now(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec*1e9 + now.tv_nsec) / 1e9;
}

char *readfile(const char *path, long *len) {
    FILE *f = fopen(path, "rb+");
    assert(f);
    assert(fseek(f, 0, 2) == 0);
    size_t size = ftell(f);
    rewind(f);
    char *mem = (char *)malloc(size+1);
    assert(mem);
    assert(fread(mem, size, 1, f) == 1);
    mem[size] = '\0';
    assert(fclose(f) == 0);
    if (len) *len = size;
    return (char*)mem;
}

char *readtestfile(const char *name, long *len) {
    char path[128];
    snprintf(path, sizeof(path), "testfiles/%s", name);
    return readfile(path, len);
}

char *commaize(unsigned int n) {
    char s1[64];
    char *s2 = (char *)malloc(64);
    assert(s2);
    memset(s2, 0, sizeof(64));
    snprintf(s1, sizeof(s1), "%d", n);
    int i = strlen(s1)-1; 
    int j = 0;
    while (i >= 0) {
        if (j%3 == 0 && j != 0) {
            memmove(s2+1, s2, strlen(s2)+1);
            s2[0] = ',';
        }
        memmove(s2+1, s2, strlen(s2)+1);
        s2[0] = s1[i];
        i--;
        j++;
    }
    return s2;
}

// private or undocumented functions (exported)
const char *neco_shortstrerror(int code);
int neco_errconv_from_sys(void);
void neco_errconv_to_sys(int err);
int neco_errconv_from_gai(int errnum);
int neco_getaddrinfo_nthreads(void);
int neco_mutex_fastlock(neco_mutex *mutex, int64_t deadline);
void neco_setcanceled(void);
int neco_pipe(int pipefd[2]);
int neco_testcode(int errcode);
void *neco_malloc(size_t size);
void *neco_realloc(void *ptr, size_t size);
void neco_free(void *ptr);
int neco_stream_release(neco_stream *stream);
int neco_stream_make_buffered_size(neco_stream **buf, int fd, size_t buffer_size);
int neco_mutex_destroy(neco_mutex *mutex);
int neco_waitgroup_destroy(neco_waitgroup *waitgroup);
int neco_cond_destroy(neco_cond *cond);

#define FAIL_EXTERN(name) \
extern __thread volatile int neco_fail_ ## name ## _counter; \
extern __thread volatile int neco_fail_ ## name ## _error;

FAIL_EXTERN(read)
FAIL_EXTERN(write)
FAIL_EXTERN(accept)
FAIL_EXTERN(connect)
FAIL_EXTERN(socket)
FAIL_EXTERN(bind)
FAIL_EXTERN(listen)
FAIL_EXTERN(setsockopt)
FAIL_EXTERN(nanosleep)
FAIL_EXTERN(fcntl)
FAIL_EXTERN(evqueue)
FAIL_EXTERN(pthread_create)
FAIL_EXTERN(pthread_detach)
FAIL_EXTERN(pipe)
FAIL_EXTERN(neco_malloc)
FAIL_EXTERN(neco_realloc)
FAIL_EXTERN(stack_get)

extern __thread bool neco_fail_cowait;
extern __thread bool neco_last_panic;
extern __thread bool neco_connect_dl_canceled;
extern __thread int neco_partial_write;

#define expect(op, ...) { \
    int args[] = {__VA_ARGS__,0,0}; \
    int nargs = (sizeof((int[]){__VA_ARGS__})/sizeof(int)); \
    int ret = (op); \
    if (nargs > 0 && ret != (args[0])) { \
        fprintf(stderr, "FAIL expected '%s' got '%s', function %s, file %s, line %d.\n", \
            neco_shortstrerror((args[0])), neco_shortstrerror(ret), \
            __func__, __FILE__, __LINE__); \
        _Exit(1); \
    } \
    if (nargs == 1 && args[0] == NECO_ERROR) { \
        assert(!"need extra arg"); \
    } \
    if (nargs > 1 && neco_lasterr() != (args[1])) { \
        fprintf(stderr, "FAIL `neco_lasterr` expected '%s' got '%s', function %s, file %s, line %d.\n", \
            neco_shortstrerror((args[1])), neco_shortstrerror(neco_lasterr()), \
            __func__, __FILE__, __LINE__); \
        _Exit(1); \
    } \
}

#ifdef assert
#undef assert
#endif

#define assert(cond) { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL assertion '%s', function %s, file %s, line %d.\n", \
            #cond, __func__, __FILE__, __LINE__); \
        _Exit(1); \
    } \
}

static bool order_print = false;

struct order {
    int vals[256];
    int count;
};

static void order_add(struct order *order, int val) {
    if (order_print) {
        printf("%d\n", val);
    }
    order->vals[order->count++] = val;
}

#define order_check(order) { \
    for (int i = 1; i < (order)->count; i++) { \
        if ((order)->vals[i-1] != (order)->vals[i]-1) { \
            fprintf(stderr, "Out of order: [ "); \
            for (int j = 0; j < (order)->count; j++) { \
                fprintf(stderr, "%d ", (order)->vals[j]); \
            } \
            fprintf(stderr, "]\n"); \
            assert(!"bad order"); \
        } \
    } \
}

void wait_for_threads(void) {
    double start = now();
    while (neco_getaddrinfo_nthreads() > 0) {
        if (now() - start > 5e9) {
            fprintf(stderr, "All threads did not stop in a reasonable amount of time\n");
            abort();
        }
        sched_yield();
    }
}

void neco_print_stats(void) {
    neco_stats stats;
    int ret = neco_getstats(&stats);
    if (ret != NECO_OK) {
        printf("%s\n", neco_strerror(ret));
        return;
    }
    printf("coroutines:   %zu\n", stats.coroutines);
    printf("sleepers:     %zu\n", stats.sleepers);
    printf("evwaiters:    %zu\n", stats.evwaiters);
    printf("sigwaiters:   %zu\n", stats.sigwaiters);
    printf("senders:      %zu\n", stats.senders);
    printf("receivers:    %zu\n", stats.receivers);
    printf("locked:       %zu\n", stats.locked);
    printf("waitgroupers: %zu\n", stats.waitgroupers);
    printf("condwaiters:  %zu\n", stats.condwaiters);
}

#define DISABLED(test, platform) \
    int main(void) { \
        (void)nameless_tests; \
        fprintf(stderr, test " \e[31mdisabled for " platform "\e[0m\n"); \
        return 0; \
    }

extern __thread int neco_gai_errno;

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif // TESTS_H
