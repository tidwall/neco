#include <stdlib.h>
#include <unistd.h>
#include "../neco.h"

int neco_main(int argc, char *argv[]) {
    int fd = neco_dial("tcp", "localhost:19203");
    if (fd == -1) {
        perror("neco_listen");
        exit(1);
    }
    printf("connected\n");
    char buf[64];
    while (1) {
        printf("> ");
        fflush(stdout);
        ssize_t nbytes = neco_read(STDIN_FILENO, buf, sizeof(buf));
        if (nbytes < 0) {
            break;
        }
        ssize_t ret = neco_write(fd, buf, nbytes);
        if (ret < 0) {
            break;
        }
    }
    printf("disconnected\n");
    close(fd);
    return 0;
}