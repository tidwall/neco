#include <math.h>
#include "tests.h"

double entropy_calc(const char *data, size_t nbytes) {
    uint64_t possible_bytes[256] = { 0 };
    for (size_t i = 0; i < nbytes; i++) {
        unsigned char byte_idx = data[i];
        possible_bytes[byte_idx]++; 
    }
    double entropy = 0;
    for (size_t i = 0; i < 256; i++) {
        uint64_t count = possible_bytes[i];
        if (count) {
            double prob = (double)count / (double)nbytes;
            entropy -= prob * log2(prob);
        }
    }
    return entropy;
}

void co_rand(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int bufsz = 1003;
    char *buf = malloc(bufsz);
    assert(buf);
    memset(buf, 0x11, bufsz);
    assert(entropy_calc(buf, bufsz) < 0.0001);    
    expect(neco_rand(buf, bufsz, NECO_CSPRNG), NECO_OK);
    assert(entropy_calc(buf, bufsz) > 7.5);
    expect(neco_rand(buf, bufsz, NECO_CSPRNG), NECO_OK);
    assert(entropy_calc(buf, bufsz) > 7.5);

    expect(neco_rand_setseed(9876, 0), NECO_OK);
    expect(neco_rand(buf, bufsz, NECO_PRNG), NECO_OK);
    double e1 = entropy_calc(buf, bufsz);
    assert(e1 > 7.5);

    int64_t oldseed;
    expect(neco_rand_setseed(4123, &oldseed), NECO_OK);
    expect(neco_rand_setseed(oldseed, 0), NECO_OK);
    expect(neco_rand(buf, bufsz, NECO_PRNG), NECO_OK);
    double e2 = entropy_calc(buf, bufsz);
    assert(e2 > 7.5 && e2 != e1);





    expect(neco_rand_setseed(oldseed, 0), NECO_OK);
    expect(neco_rand(buf, bufsz, NECO_PRNG), NECO_OK);
    double e3 = entropy_calc(buf, bufsz);
    assert(e3 == e2);
    


    expect(neco_rand(0, 0, NECO_PRNG), NECO_OK);
    expect(neco_cancel(neco_getid()), NECO_OK);
    expect(neco_rand(0, 0, NECO_PRNG), NECO_CANCELED);
    expect(neco_rand_dl(0, 0, NECO_PRNG, 1), NECO_TIMEDOUT);
    
    free(buf);
}

void test_rand(void) {
    expect(neco_rand(0, 0, -1), NECO_INVAL);
    expect(neco_rand(0, 0, 0), NECO_PERM);
    expect(neco_rand_setseed(0,0), NECO_PERM);
    expect(neco_start(co_rand, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_rand);
}
