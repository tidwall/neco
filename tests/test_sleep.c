#include <signal.h>
#include "tests.h"

void co_sleep_cancel_child(int argc, void *argv[]) {
    assert(argc == 1);
    int64_t id = *(int64_t*)argv[0];
    expect(neco_cancel(-1111), NECO_NOTFOUND);
    expect(neco_cancel(id), NECO_OK);
    expect(neco_sleep(NECO_SECOND/4), NECO_OK);
    expect(neco_cancel(id), NECO_OK);
}

void co_sleep_cancel(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    int64_t id = neco_getid();
    assert(id > 0);
    expect(neco_start(co_sleep_cancel_child, 1, &id), NECO_OK);
    expect(neco_sleep(NECO_SECOND*10), NECO_CANCELED);
    expect(neco_sleep(NECO_SECOND/10), NECO_OK);
    expect(neco_sleep(NECO_SECOND*10), NECO_CANCELED);
}

void test_sleep_cancel(void) {
    expect(neco_cancel(0), NECO_PERM);
    expect(neco_start(co_sleep_cancel, 0), NECO_OK);
}


void co_sleep_basic_child(int argc, void *argv[]) {
    assert(argc == 1);
    int *x = argv[0];
    (*x)++;
    assert(neco_sleep(1e9/10) == NECO_OK);
}

void co_sleep_basic(int argc, void *argv[]) {
    assert(argc == 2);
    int *x = argv[0];
    int *n = argv[1];
    for (int i = 0; i < *n; i++) {
        assert(neco_start(co_sleep_basic_child, 1, x) == NECO_OK);
    }
}

void test_sleep_basic(void) {
    int x = 0;
    int n = 20;
    double start = now();
    assert(neco_start(co_sleep_basic, 2, &x, &n) == NECO_OK);
    assert(now() - start > 0.10);
    assert(x == n);
}


int main(int argc, char **argv) {
    do_test(test_sleep_basic);
    do_test(test_sleep_cancel);
}
