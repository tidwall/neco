#include <signal.h>
#include "tests.h"

static int64_t lastid = 0;

void co_basic_startv(int argc, void *argv[]) {
    assert(argc == 1);
    lastid = neco_getid();
    int x = *(int*)argv[0];
    assert(x == 1977);
}

void co_basic_start(int argc, void *argv[]) {
    assert(argc == 1);
    int *x = argv[0];
    *x = 1977;

#ifdef IS_FAIL_TARGET
    neco_fail_neco_malloc_counter = 1;
    expect(neco_start(co_basic_start, 0), NECO_NOMEM);
    neco_fail_stack_get_counter = 1;
    expect(neco_start(co_basic_start, 0), NECO_NOMEM);
    // This will trigger a NOMEM on number of arguments over 4
    neco_fail_neco_malloc_counter = 2;
    expect(neco_start(co_basic_start, 5, 0, 0, 0, 0, 0), NECO_NOMEM);
#endif

    expect(neco_startv(co_basic_startv, 1, &(void*){x}), NECO_OK);
    assert(neco_lastid() == lastid);
    expect(neco_startv(co_basic_startv, 1, &(void*){x}), NECO_OK);
    assert(neco_lastid() == lastid);
    expect(neco_startv(co_basic_startv, 1, &(void*){x}), NECO_OK);
    assert(neco_lastid() == lastid);
}

void test_basic_start(void) {
    int x = 0;
    expect(neco_start(co_basic_start, 1, &x), NECO_OK);
    assert(x == 1977);

    // These will fail
#ifdef IS_FAIL_TARGET
    for (int i = 1; i <= 500; i++) {
        neco_fail_neco_malloc_counter = i;
        int ret = neco_start(co_basic_start, 1, &x);
        if (ret == NECO_OK) {
            break;
        }
        if (ret != NECO_NOMEM) {
            fprintf(stderr, "expected NECO_NOMEM, got %s (iter %d)\n", 
                neco_strerror(ret), i);
            assert(0);
        }
    }

    // neco_fail_neco_malloc_counter = 1;
    // expect(neco_start(co_basic_start, 0), NECO_NOMEM);
    // neco_fail_neco_malloc_counter = 2;
    // expect(neco_start(co_basic_start, 0), NECO_NOMEM);
    // neco_fail_neco_malloc_counter = 3;
    // expect(neco_start(co_basic_start, 0), NECO_NOMEM);
    // neco_fail_neco_malloc_counter = 4;
    // expect(neco_start(co_basic_start, 0), NECO_NOMEM);
    // neco_fail_neco_malloc_counter = 5;
    // expect(neco_start(co_basic_start, 0), NECO_NOMEM);
    // neco_fail_neco_malloc_counter = 6;
    // expect(neco_start(co_basic_start, 0), NECO_NOMEM);
    // neco_fail_neco_malloc_counter = 7;
    // expect(neco_start(co_basic_start, 0), NECO_NOMEM);
#endif

    expect(neco_start(co_basic_start, -1), NECO_INVAL);
    expect(neco_lastid(), NECO_PERM);


}

void co_basic_stats(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    neco_stats stats;
    expect(neco_getstats(&stats), NECO_OK);
    assert(stats.coroutines == 1);
}

void test_basic_stats(void) {
    neco_stats stats;
    expect(neco_getstats(&stats), NECO_PERM);
    expect(neco_getstats(0), NECO_INVAL);
    assert(neco_start(co_basic_stats, 0) == NECO_OK);
}

void co_sched1(int argc, void *argv[]) {
    assert(argc == 2);
    char *a = argv[0];
    int *i = argv[1];
    a[(*i)++] = 'B';
    assert(neco_yield() == NECO_OK);
    a[(*i)++] = 'F';
}

void co_sched2(int argc, void *argv[]) {
    assert(argc == 2);
    char *a = argv[0];
    int *i = argv[1];
    a[(*i)++] = 'D';
    assert(neco_yield() == NECO_OK);
    a[(*i)++] = 'G';
}

void co_sched(int argc, void *argv[]) {
    assert(argc == 2);
    char *a = argv[0];
    int *i = argv[1];    
    a[(*i)++] = 'A';
    assert(neco_start(co_sched1, 2, a, i) == NECO_OK);
    a[(*i)++] = 'C';
    assert(neco_start(co_sched2, 2, a, i) == NECO_OK);
    a[(*i)++] = 'E';
    assert(neco_yield() == NECO_OK);
    a[(*i)++] = 'H';
}

void test_basic_sched(void) {
    expect(neco_yield(), NECO_PERM);

    // Tests the scheduling order.
    char a[10];
    int i = 0;
    assert(neco_start(co_sched, 2, a, &i) == NECO_OK);
    a[i++] = '\0';
    char exp[] = "ABCDEFGH";
    if (strcmp(a, exp) != 0) { 
        printf("expected '%s' got '%s'\n", exp, a);
        abort();
    }
}

void co_sleep(int argc, void *argv[]) {
    (void)argc; (void)argv;
    expect(neco_sleep(-1), NECO_TIMEDOUT);
    expect(neco_cancel(neco_getid()), NECO_OK);
    expect(neco_sleep(INT64_MAX), NECO_CANCELED);
    expect(neco_sleep(INT64_MIN), NECO_TIMEDOUT);
}

void co_sleep_rand_child(int argc, void *argv[]) {
    assert(argc == 1);
    int64_t dl = *(int64_t*)argv[0];
    int nsecs = (int64_t)(rand_double() * (double)(NECO_SECOND/10));
    expect(neco_sleep(nsecs), NECO_OK);
    expect(neco_sleep_dl(dl), NECO_OK);
}

void co_sleep_rand(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int N = 100;
    int64_t dl = neco_now() + NECO_SECOND/2;
    for (int i = 0; i < N; i++) {
        expect(neco_start(co_sleep_rand_child, 1, &dl), NECO_OK);
    }
}

void test_basic_sleep(void) {
    expect(neco_sleep(0), NECO_PERM);
    expect(neco_start(co_sleep, 0), NECO_OK);
    expect(neco_start(co_sleep_rand, 0), NECO_OK);
}

void test_basic_misc(void) {
    assert(neco_starterid() == NECO_PERM);
    expect(neco_is_main_thread(), NECO_PERM);
    expect(neco_now(), NECO_PERM);
    assert(strlen(neco_switch_method()) > 0);
}

static int val = 0;

void rt1(void *arg) {
    (*(int*)arg)++;
}

void co_exit(int argc, void *argv[]) {
    (void)argc; (void)argv;
    val++;
    neco_cleanup_push(rt1, &val);
    neco_exit();
    neco_cleanup_pop(1);
    val++;
}

void test_basic_exit(void) {
    neco_exit(); // Noop
    val = 0;
    expect(neco_start(co_exit, 0), NECO_OK);    
    assert(val == 2);
}

void test_basic_malloc(void) {
    void *ptr = neco_malloc(100);
    assert(ptr);
    ptr = neco_realloc(ptr, 200);
    assert(ptr);
    neco_free(ptr);
}

int main(int argc, char **argv) {
    do_test(test_basic_start);
    do_test(test_basic_stats);
    do_test(test_basic_sched);
    do_test(test_basic_sleep);
    do_test(test_basic_exit);
    do_test_(test_basic_malloc, false);
    do_test(test_basic_misc);

    // The next lines test that neco_free and exit_prog operations can be
    // reached from outside of a neco context
    neco_free(neco_malloc(100));
    __neco_exit_prog(0);

}
