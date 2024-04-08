#include <stdlib.h>
#include <unistd.h>
#include "../neco.h"

void coroutine1(int argc, void *argv[]) {
    neco_chan *c1 = argv[0];
    neco_sleep(NECO_SECOND / 2);
    char *msg = "one";
    neco_chan_send(c1, &msg);
    neco_chan_release(c1);
}
void coroutine2(int argc, void *argv[]) {
    neco_chan *c2 = argv[0];
    neco_sleep(NECO_SECOND / 2);
    char *msg = "two";
    neco_chan_send(c2, &msg);
    neco_chan_release(c2);
}

int neco_main(int argc, char *argv[]) {

    // For our example we’ll select across two channels.

    neco_chan *c1;
    neco_chan *c2;

    neco_chan_make(&c1, sizeof(char*), 0);
    neco_chan_make(&c2, sizeof(char*), 0);

    // Each channel will receive a value after some amount of time, to
    // simulate e.g. blocking RPC operations executing in concurrent coroutines.
    neco_chan_retain(c1);
    neco_start(coroutine1, 1, c1);

    neco_chan_retain(c2);
    neco_start(coroutine2, 1, c2);

    // We’ll use neco_chan_select() to await both of these values
    // simultaneously, printing each one as it arrives.
    for (int i = 0; i < 2; i++) {
        char *msg = NULL;
        switch (neco_chan_select(2, c1, c2)) {
        case 0:
            neco_chan_case(c1, &msg);
            break;
        case 1:
            neco_chan_case(c2, &msg);
            break;
        }
        printf("received %s\n", msg);
    }

    neco_chan_release(c1);
    neco_chan_release(c2);
    return 0;
}