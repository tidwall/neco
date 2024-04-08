#include "tests.h"

#if defined(_WIN32)
DISABLED("test_pipe", "Windows")
#elif defined(__EMSCRIPTEN__)
DISABLED("test_pipe", "Emscripten")
#else

#include <pthread.h>

void *th0_entry(void *arg) {
    usleep(10000);
    int fd = *(int*)arg;
    assert(fd > 0);
    assert(neco_setnonblock(fd, false, 0) == 0);
    char data[64];
    assert(write(fd, "+PING\r\n", 7) == 7);
    assert(read(fd, data, sizeof(data)) == 7);
    assert(memcmp(data, "+PONG\r\n", 7) == 0);
    close(fd);
    return 0;
}

void *th1_entry(void *arg) {
    int fd = *(int*)arg;
    assert(fd > 0);
    assert(neco_setnonblock(fd, false, 0) == 0);
    char data[64];
    assert(read(fd, data, sizeof(data)) == 7);
    assert(memcmp(data, "+PING\r\n", 7) == 0);
    assert(write(fd, "+PONG\r\n", 7) == 7);
    close(fd);
    return 0;
}

void co_pipe(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int fds[2];
    int r = neco_pipe(fds);
    if (r == -1) {
        perror("neco_pipe");
    }
    expect(neco_pipe(fds), NECO_OK);
    pthread_t th0, th1;
    assert(pthread_create(&th0, 0, th0_entry, &fds[0]) == 0);
    assert(pthread_create(&th1, 0, th1_entry, &fds[1]) == 0);
    assert(pthread_join(th0, 0) == 0);
    assert(pthread_join(th1, 0) == 0);
}

void test_pipe(void) {
    assert(neco_pipe(0) == -1 && errno == EINVAL);
    int fds[2];
    assert(neco_pipe(fds) == -1 && errno == EPERM);
    expect(neco_start(co_pipe, 0), NECO_OK);
}


int main(int argc, char **argv) {
    do_test(test_pipe);
}

#endif
