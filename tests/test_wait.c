#include "tests.h"

#if defined(_WIN32)
DISABLED("test_net", "Windows")
#elif defined(__EMSCRIPTEN__)
DISABLED("test_net", "Emscripten")
#else

void co_wait_fd(int argc, void *argv[]) {
    (void)argc;
    (void)argv;
    int fds[2];
    assert(pipe(fds) == 0);
    expect(neco_wait_dl(fds[0], NECO_WAIT_READ, neco_now()+NECO_MILLISECOND), NECO_TIMEDOUT);
    expect(neco_wait(fds[1], NECO_WAIT_WRITE), NECO_OK);
    int ret = neco_wait(-10, NECO_WAIT_WRITE);
    assert(ret == NECO_ERROR && errno == EBADF);
    assert(write(fds[1], "hiya", 4) == 4);
    expect(neco_wait(fds[0], NECO_WAIT_READ), NECO_OK);
    char buf[16];
    assert(read(fds[0], buf, sizeof(buf)) == 4 && memcmp(buf, "hiya", 4) == 0);
    close(fds[0]);
    close(fds[1]);
}

void test_wait_fd(void) {
    expect(neco_wait(1, 0), NECO_INVAL);
    expect(neco_wait(1, NECO_WAIT_READ), NECO_PERM);
    expect(neco_start(co_wait_fd, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_wait_fd);
}
#endif
