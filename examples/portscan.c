// Port scanning
//
// Thanks to @sigidagi for recommending this example.
// See https://github.com/tidwall/neco/issues/14

#include <stdio.h>
#include <unistd.h>
#include "../neco.h"

void co_dial_port(int argc, void** argv) {
    const char *host = argv[0];    // host
    int port = *(int*)argv[1];     // port
    neco_waitgroup *wg = argv[2];  // waitgroup

    // Dial the port with a one second deadline
    char addr[256];
    snprintf(addr, sizeof(addr), "%s:%d", host, port);
    int fd = neco_dial_dl("tcp", addr, neco_now() + NECO_SECOND);
    if (fd < 0) {
        printf("%-5d FAIL\t%s\n", port, neco_strerror(neco_lasterr()));
    } else {
        printf("%-5d OK\n", port);
        close(fd);
    }
    // Notify the waitgroup that we are done.
    neco_waitgroup_done(wg);
}

int neco_main(int argc, char** argv) {
    const char *host = "scanme.nmap.org";
    int ports[] = { 22, 80, 8080, 443 };


    // Use a waitgroup to synchronize the coroutines
    neco_waitgroup wg;
    neco_waitgroup_init(&wg);

    // Dial all the ports
    printf("%s\n", host);
    for (int i = 0; i < sizeof(ports)/sizeof(int); i++) {
        neco_waitgroup_add(&wg, 1);
        neco_start(co_dial_port, 3, host, &ports[i], &wg);
    } 

    // Wait for all coroutines to finish
    neco_waitgroup_wait(&wg);
    return 0;
}