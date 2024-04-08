#include <stdlib.h>
#include <unistd.h>
#include "../neco.h"

int neco_main(int argc, char *argv[]) {

    // Set up this coroutine to wait for the SIGINT (Ctrl-C) or SIGQUIT
    // (Ctrl-\) signals.
    neco_signal_watch(SIGINT);
    neco_signal_watch(SIGQUIT);

    printf("Waiting for Ctrl-C or Ctrl-\\ signals...\n");
    int sig = neco_signal_wait();
    if (sig == SIGINT) {
        printf("\nReceived Ctrl-C\n");
    } else if (sig == SIGQUIT) {
        printf("\nReceived Ctrl-\\\n");
    }

    // The neco_signal_unwatch can be used to when you no longer want to watch
    // for a signal
    neco_signal_unwatch(SIGINT);
    neco_signal_unwatch(SIGQUIT);

    return 0;
}