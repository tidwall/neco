#include "tests.h"

void co_gen_yielder(int argc, void *argv[]) {
    (void)argc; (void)argv;
    expect(neco_gen_yield(&(int){9}), NECO_OK);
    expect(neco_gen_yield(&(int){4}), NECO_OK);
    expect(neco_sleep(NECO_SECOND/10), NECO_OK);
    expect(neco_gen_yield(&(int){3}), NECO_OK);
    expect(neco_gen_yield(&(int){7}), NECO_CLOSED);

}

void co_gen_yield(int argc, void *argv[]) {
    assert(argc == 1);
    bool startv = *(bool*)argv[0];

    expect(neco_gen_yield(0), NECO_NOTGENERATOR);

    neco_gen *gen;
    if (startv) {
        expect(neco_gen_startv(&gen, sizeof(int), co_gen_yielder, 0, 0), NECO_OK);
    } else {
        expect(neco_gen_start(&gen, sizeof(int), co_gen_yielder, 0), NECO_OK);
    }
    expect(neco_gen_retain(gen), NECO_OK);
    int x;
    expect(neco_gen_next(gen, &x), NECO_OK);
    assert(x == 9);
    expect(neco_gen_next(gen, &x), NECO_OK);
    assert(x == 4);
    expect(neco_gen_next_dl(gen, &x, neco_now()+NECO_SECOND/20), NECO_TIMEDOUT);
    expect(neco_gen_next(gen, &x), NECO_OK);
    assert(x == 3);
    expect(neco_gen_close(gen), NECO_OK);
    expect(neco_gen_release(gen), NECO_OK); // for gen_start
    expect(neco_gen_release(gen), NECO_OK); // for gen_retain
}

void test_gen_yield(void) {
    expect(neco_start(co_gen_yield, 1, &(bool){true}), NECO_OK);
    expect(neco_start(co_gen_yield, 1, &(bool){false}), NECO_OK);
}

void co_gen_fail_yielder(int argc, void *argv[]) {
    (void)argc; (void)argv;

}

void co_gen_fail(int argc, void *argv[]) {
    (void)argc; (void)argv;
    neco_gen *gen;
    neco_fail_neco_malloc_counter = 2;
    expect(neco_gen_start(&gen, 0, co_gen_fail_yielder, 0), NECO_NOMEM);
    neco_fail_neco_malloc_counter = 3;
    expect(neco_gen_start(&gen, 0, co_gen_fail_yielder, 5, 0, 0, 0, 0, 0), NECO_NOMEM);
}

void test_gen_fail(void) {
    expect(neco_gen_start(0, 0, 0, 0), NECO_INVAL);
    expect(neco_gen_start((void*)1, SIZE_MAX, 0, 0), NECO_INVAL);
    expect(neco_gen_start((void*)1, 0, 0, 0), NECO_PERM);
    expect(neco_gen_yield(0), NECO_PERM);
    expect(neco_start(co_gen_fail, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_gen_fail);
    do_test(test_gen_yield);
}
