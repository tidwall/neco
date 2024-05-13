#include "tests.h"

void work(void *udata) {
    (*(int*)udata)++;
    usleep(1000);
}

void co_work_runner(int argc, void *argv[]) {
    assert(argc == 1);
    int i = *(int*)argv[0];
    int x = i;
    neco_work(-1, work, &x);
    assert(x == i+1);
}

void co_work(int argc, void *argv[]) {
    (void)argc; (void)argv;
    for (int i = 0; i < 100; i++) {
        expect(neco_start(co_work_runner, 1, &i), NECO_OK);
    }
}

void test_work(void) {
    expect(neco_work(0, 0, 0), NECO_INVAL);
    expect(neco_work(0, work, 0), NECO_PERM);
    expect(neco_start(co_work, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_work);
}
