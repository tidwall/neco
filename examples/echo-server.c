#include <stdlib.h>
#include <unistd.h>
#include "../neco.h"

void client(int argc, void *argv[]) {
    int conn = *(int*)argv[0];
    printf("client connected\n");
    char buf[64];
    while (1) {
        ssize_t n = neco_read(conn, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        printf("%.*s", (int)n, buf);
    }
    printf("client disconnected\n");
    close(conn);
}

int neco_main(int argc, char *argv[]) {
    int ln = neco_serve("tcp", "localhost:19203");
    if (ln == -1) {
        perror("neco_serve");
        exit(1);
    }
    printf("listening at localhost:19203\n");
    while (1) {
        int conn = neco_accept(ln, 0, 0);
        if (conn > 0) {
            neco_start(client, 1, &conn);
        }
    }
    close(ln);
    return 0;
}