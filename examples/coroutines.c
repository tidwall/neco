#include <stdlib.h>
#include <unistd.h>
#include "../neco.h"

void coroutine(int argc, void *argv[]) {
    const char *name = (char*)argv[0];
    for (int i = 0; i < 5; i++) {
        printf("coroutine: %s (%d)\n", name, i);

        // Yield to another coroutine. 
        neco_yield();
    }
}

int neco_main(int argc, char *argv[]) {

    // Start three coroutines that will execute concurrently.
    neco_start(coroutine, 1, "A");
    neco_start(coroutine, 1, "B");
    neco_start(coroutine, 1, "C");

    // Our three function calls are now running asynchronously in separate
    // coroutines. Wait for them to finish (for a more robust approach, use a
    // neco_waitgroup or neco_join).
    neco_sleep(NECO_MILLISECOND);
    printf("done\n");
    return 0;
}