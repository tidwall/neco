#include <signal.h>
#include "tests.h"

void *neco_malloc(size_t nbytes);
void *neco_realloc(void *ptr, size_t nbytes);
void neco_free(void *ptr);


int64_t i64_add_clamp(int64_t a, int64_t b);

void test_misc_units(void) {
    assert(i64_add_clamp(0, INT64_MIN) == INT64_MIN);
    assert(i64_add_clamp(0, INT64_MAX) == INT64_MAX);
    assert(i64_add_clamp(1, INT64_MIN) == INT64_MIN+1);
    assert(i64_add_clamp(1, INT64_MAX) == INT64_MAX);
    assert(i64_add_clamp(-1, INT64_MIN) == INT64_MIN);
    assert(i64_add_clamp(-1, INT64_MAX) == INT64_MAX-1);
}

int main(int argc, char **argv) {
    // test outside of the "do"
    
    neco_env_setallocator(0,0,0);
    void *ptr = neco_malloc(100);
    assert(ptr);
    ptr = neco_realloc(ptr, 200);
    assert(ptr);
    neco_free(ptr);
    neco_free(0);


    do_test(test_misc_units);
}
