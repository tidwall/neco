#include <stdio.h>
#include <unistd.h>
#include "../neco.h"

void coroutine(int argc, void *argv[]) {
    // Yield each int to the caller, one at a time.
    for (int i = 0; i < 10; i++) {
        neco_gen_yield(&i);
    }
}

int neco_main(int argc, char *argv[]) {
    
    // Create a new generator coroutine that is used to send ints.
    neco_gen *gen;
    neco_gen_start(&gen, sizeof(int), coroutine, 0);

    // Iterate over each int until the generator is closed.
    int i;
    while (neco_gen_next(gen, &i) != NECO_CLOSED) {
        printf("%d\n", i); 
    }

    // This coroutine no longer needs the generator.
    neco_gen_release(gen);
    return 0;
}
