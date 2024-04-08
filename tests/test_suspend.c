#include "tests.h"

int nvals = 0;
static char vals[100] = { 0 };

void co_suspend_child(int argc, void *argv[]) {
    (void)argc; (void)argv;
    vals[nvals++] = 'B';
    expect(neco_suspend(), NECO_OK);
    vals[nvals++] = 'D';
    expect(neco_yield(), NECO_OK);
    vals[nvals++] = 'F';
    expect(neco_resume(neco_starterid()), NECO_OK);
    vals[nvals++] = 'H';
}


void co_suspend(int argc, void *argv[]) {
    (void)argc; (void)argv;
    vals[nvals++] = 'A';
    expect(neco_start(co_suspend_child, 0), NECO_OK);
    vals[nvals++] = 'C';
    expect(neco_resume(neco_lastid()), NECO_OK);
    vals[nvals++] = 'E';
    expect(neco_suspend(), NECO_OK);
    vals[nvals++] = 'G';

    expect(neco_resume(neco_lastid()), NECO_NOTSUSPENDED);
    expect(neco_resume(-1), NECO_NOTFOUND);
}

void test_suspend(void) {
    expect(neco_suspend(), NECO_PERM);
    expect(neco_resume(0), NECO_PERM);
    expect(neco_start(co_suspend, 0), NECO_OK);
    bool ok = true;
    char c = 'A';
    for (int i = 0 ; i < nvals; i++, c++) {
        if (vals[i] != c) {
            ok = false;
            break;
        }
    }
    if (!ok) {
        printf("Out of order: [");
        for (int i = 0 ; i < nvals; i++) {
            printf("%c ", vals[i]);
        }
        printf("]\n");
        assert(ok);
    }
}


int main(int argc, char **argv) {
    do_test(test_suspend);
}
