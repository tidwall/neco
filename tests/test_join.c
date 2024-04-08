#include <pthread.h>
#include "tests.h"

static int value = 0;

void co_join_child(int argc, void *argv[]) {
    (void)argc; (void)argv;
    value = 9918;
    expect(neco_sleep(NECO_SECOND/10), NECO_OK);
    value = 1899;
}

void co_join_canceler(int argc, void *argv[]) {
    (void)argc; (void)argv;
    expect(neco_sleep(NECO_SECOND/20), NECO_OK);
    expect(neco_cancel(neco_starterid()), NECO_OK);
}

void co_join(int argc, void *argv[]) {
    (void)argc; (void)argv;
    assert(neco_starterid() == 0);

    expect(neco_join(neco_getid()), NECO_PERM);

    expect(neco_start(co_join_child, 0), NECO_OK);
    assert(value == 9918);
    expect(neco_join(neco_lastid()), NECO_OK);
    assert(value == 1899);

    value = 0;
    expect(neco_start(co_join_child, 0), NECO_OK);
    assert(value == 9918);
    expect(neco_join_dl(neco_lastid(), neco_now()+NECO_MILLISECOND), NECO_TIMEDOUT);
    assert(value == 9918);

    value = 0;
    expect(neco_start(co_join_child, 0), NECO_OK);
    assert(value == 9918);
    expect(neco_join_dl(neco_lastid(), 1), NECO_TIMEDOUT);
    assert(value == 9918);

    value = 0;
    expect(neco_start(co_join_child, 0), NECO_OK);
    assert(value == 9918);
    expect(neco_join_dl(neco_lastid(), 1), NECO_TIMEDOUT);
    assert(value == 9918);

    value = 0;
    expect(neco_start(co_join_child, 0), NECO_OK);
    assert(value == 9918);
    expect(neco_join_dl(-1, 1), NECO_OK);
    assert(value == 9918);

    value = 0;
    expect(neco_start(co_join_child, 0), NECO_OK);
    assert(value == 9918);
    expect(neco_cancel(neco_getid()), NECO_OK);
    expect(neco_join_dl(neco_lastid(), 1), NECO_CANCELED);
    assert(value == 9918);

    value = 0;
    expect(neco_start(co_join_child, 0), NECO_OK);
    assert(value == 9918);
    expect(neco_start(co_join_canceler, 0), NECO_OK);
    expect(neco_join(neco_lastid()), NECO_CANCELED);
    assert(value == 9918);

}

void test_join(void) {
    expect(neco_join(0), NECO_PERM);
    expect(neco_start(co_join, 0), NECO_OK);
}


int main(int argc, char **argv) {
    do_test(test_join);
}
