#include "tests.h"

extern void *neco_malloc(size_t nbytes);
extern void *neco_realloc(void *ptr, size_t nbytes);

void co_test_panic(int argc, void *argv[]) {
    (void)argc; (void)argv;
    neco_last_panic = false;
    neco_env_setpaniconerror(true);

    void *ptr = neco_malloc(SIZE_MAX);
    assert(!ptr && neco_last_panic);
    neco_last_panic = false;
    neco_env_setpaniconerror(true);
    ptr = neco_realloc(0, SIZE_MAX);
    assert(!ptr && neco_last_panic);

    neco_env_setpaniconerror(false);
    neco_last_panic = false;
    
    neco_panic("hello");
    assert(neco_last_panic);
    neco_last_panic = false;
}

void test_panic(void) {
    expect(neco_start(co_test_panic, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_panic);
}
