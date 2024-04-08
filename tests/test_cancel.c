#include "tests.h"

static int total = 0;

void cl0(void *arg) {
    total += (size_t)arg;
}

void co_basic_cleanup(int argc, void *argv[]) {
    (void)argc; (void)argv;
    neco_cleanup_push(cl0, (void*)1);
    neco_cleanup_push(cl0, (void*)2);
    neco_cleanup_push(cl0, (void*)4);
    neco_cleanup_push(cl0, (void*)8);
    // ...
    neco_cleanup_pop(1);
    neco_cleanup_pop(1);    
    neco_cleanup_pop(0);
    neco_cleanup_pop(1);
}

void test_cancel_cleanup(void) {
    expect(neco_start(co_basic_cleanup, 0), NECO_OK);
    assert(total == 13);
}

void co_cancel(int argc, void *argv[]) {
    assert(argc == 1);
    bool sleep = *(bool*)argv[0];
    int oldtype;
    expect(neco_setcanceltype(NECO_CANCEL_ASYNC, &oldtype), NECO_OK);
    assert(oldtype == NECO_CANCEL_INLINE);
    neco_cleanup_push(cl0, (void*)1);
    neco_cleanup_push(cl0, (void*)2);
    neco_cleanup_push(cl0, (void*)4);
    neco_cleanup_push(cl0, (void*)8);
    if (sleep) {
        expect(neco_sleep(NECO_SECOND/10), NECO_OK);
    } else {
        expect(neco_yield(), NECO_OK);
    }
    // 
    neco_cleanup_pop(1);
    neco_cleanup_pop(1);    
    neco_cleanup_pop(0);
    neco_cleanup_pop(1);
}

void co_cancel_starter(int argc, void *argv[]) {
    (void)argc; (void)argv;
    total = 0;
    expect(neco_start(co_cancel, 1, argv[0]), NECO_OK);
    expect(neco_cancel(neco_lastid()), NECO_OK);
}

void test_cancel_early(void) {
    expect(neco_start(co_cancel_starter, 1, &(bool){true}), NECO_OK);
    expect(neco_start(co_cancel_starter, 1, &(bool){false}), NECO_OK);
    assert(total == 15);
}

void test_cancel_errors(void) {
    expect(neco_cancel(-1), NECO_PERM);
    expect(neco_setcanceltype(-1, 0), NECO_INVAL);
    expect(neco_setcanceltype(NECO_CANCEL_ASYNC, 0), NECO_PERM);
    expect(neco_setcanceltype(NECO_CANCEL_INLINE, 0), NECO_PERM);

    expect(neco_setcancelstate(-1, 0), NECO_INVAL);
    expect(neco_setcancelstate(NECO_CANCEL_ENABLE, 0), NECO_PERM);
    expect(neco_setcancelstate(NECO_CANCEL_DISABLE, 0), NECO_PERM);
}

void co_cancel_block_child(int argc, void *argv[]) {
    (void)argc; (void)argv;
    expect(neco_setcancelstate(NECO_CANCEL_DISABLE, 0), NECO_OK);
    neco_sleep(NECO_SECOND/4);
    expect(neco_setcancelstate(NECO_CANCEL_ENABLE, 0), NECO_OK);
}

void co_cancel_block(int argc, void *argv[]) {
    (void)argc; (void)argv;
    expect(neco_start(co_cancel_block_child, 0), NECO_OK);
    int64_t start = neco_now();
    expect(neco_cancel(neco_lastid()), NECO_NOTFOUND);
    assert(neco_now() - start >= NECO_SECOND/4);

    expect(neco_start(co_cancel_block_child, 0), NECO_OK);
    start = neco_now();
    expect(neco_cancel_dl(neco_lastid(), 0), NECO_TIMEDOUT);
    assert(neco_now() - start < NECO_SECOND/4);

    expect(neco_start(co_cancel_block_child, 0), NECO_OK);
    start = neco_now();
    expect(neco_cancel(neco_getid()), NECO_OK);
    expect(neco_cancel_dl(neco_lastid(), 0), NECO_CANCELED);
    assert(neco_now() - start < NECO_SECOND/4);

}

void test_cancel_block(void) {
    expect(neco_start(co_cancel_block, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_cancel_cleanup);
    do_test(test_cancel_early);
    do_test(test_cancel_errors);
    do_test(test_cancel_block);
}
