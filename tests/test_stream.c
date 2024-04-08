#include <unistd.h>
#include "tests.h"

#if defined(_WIN32)
DISABLED("test_stream", "Windows")
#elif defined(__EMSCRIPTEN__)
DISABLED("test_stream", "Emscripten")
#else

void co_stream_other(int argc, void *argv[]) {
    assert(argc == 2);
    int fd = *(int*)argv[0];
    size_t bufsz = *(size_t*)argv[1];

    neco_stream *buf;
    expect(neco_stream_make_buffered_size(&buf, fd, bufsz), NECO_OK);

    assert(neco_stream_write(buf, "hello1", 6) == 6);
    expect(neco_stream_flush(buf), NECO_OK);
    expect(neco_stream_flush(buf), NECO_OK);

    char data[6];
    assert(neco_stream_read(buf, data, sizeof(data)) == 6);
    assert(memcmp(data, "jello1", 6) == 0);

    assert(neco_stream_write(buf, "jello2", 6) == 6);
    expect(neco_stream_flush(buf), NECO_OK);


    neco_stream_release(buf);

    close(fd);
}

void co_stream(int argc, void *argv[]) {
    assert(argc == 1);
    size_t bufsz = *(size_t*)argv[0];

    int fds[2];
    expect(neco_pipe(fds), NECO_OK);
    int fd = fds[0];
    expect(neco_start(co_stream_other, 2, &fds[1], &bufsz), NECO_OK);


    neco_stream *buf;
    neco_fail_neco_malloc_counter = 1;
    expect(neco_stream_make_buffered(&buf, fd), NECO_NOMEM);

    expect(neco_stream_make_buffered_size(&buf, fd, bufsz), NECO_OK);

    char data[6];

    assert(neco_stream_read(buf, data, 0) == 0);

    assert(neco_stream_read(buf, data, sizeof(data)) == 6);
    assert(memcmp(data, "hello1", 6) == 0);

    assert(neco_stream_buffered_read_size(buf) == 0);
    expect(neco_stream_unread_byte(buf), NECO_OK); // 1
    assert(neco_stream_buffered_read_size(buf) == 1);
    expect(neco_stream_unread_byte(buf), NECO_OK); // o
    assert(neco_stream_buffered_read_size(buf) == 2);
    expect(neco_stream_unread_byte(buf), NECO_OK); // l
    assert(neco_stream_buffered_read_size(buf) == 3);
    expect(neco_stream_unread_byte(buf), NECO_OK); // l
    assert(neco_stream_buffered_read_size(buf) == 4);
    expect(neco_stream_unread_byte(buf), NECO_OK); // e
    assert(neco_stream_buffered_read_size(buf) == 5);
    expect(neco_stream_unread_byte(buf), NECO_OK); // h
    assert(neco_stream_buffered_read_size(buf) == 6);
    expect(neco_stream_unread_byte(buf), NECO_UNREADFAIL);

    // close(fd);

    assert(neco_stream_read_byte(buf) == 'h');
    assert(neco_stream_read_byte(buf) == 'e');
    assert(neco_stream_read_byte(buf) == 'l');
    assert(neco_stream_read_byte(buf) == 'l');
    assert(neco_stream_read_byte(buf) == 'o');
    assert(neco_stream_read_byte(buf) == '1');

    assert(neco_stream_write(buf, "", 0) == 0);
    assert(neco_stream_write(buf, "jello1", 6) == 6);
    assert(neco_stream_flush(buf) == NECO_OK);

    assert(neco_stream_read_byte(buf) == 'j');
    assert(neco_stream_read_byte(buf) == 'e');
    assert(neco_stream_read_byte(buf) == 'l');
    assert(neco_stream_read_byte(buf) == 'l');
    assert(neco_stream_read_byte(buf) == 'o');
    assert(neco_stream_read_byte(buf) == '2');

    close(fd);

    int ret = neco_stream_read_byte(buf);
    assert(ret == NECO_ERROR && errno == EBADF);
    
    ret = neco_stream_read_byte(buf);
    assert(ret == NECO_ERROR && errno == EBADF);

    ret = neco_stream_read_byte(buf);
    assert(ret == NECO_ERROR && errno == EBADF);

    ret = neco_stream_read(buf, data, 0);
    assert(ret == NECO_ERROR && errno == EBADF);


    ret = neco_stream_write(buf, "bad", 3);
    assert(ret == 3);
    assert(neco_stream_buffered_write_size(buf) == 3);
    ret = neco_stream_flush(buf);
    assert(ret == NECO_ERROR && errno == EBADF);
    ret = neco_stream_flush(buf);
    assert(ret == NECO_ERROR && errno == EBADF);


    
    


    // printf("%d\n", neco_stream_unread_byte(buf));

    // assert(neco_stream_unread_byte(buf) == '1');
    // assert(neco_stream_unread_byte(buf) == 'o');
    // assert(neco_stream_unread_byte(buf) == 'l');
    // assert(neco_stream_unread_byte(buf) == 'l');
    // assert(neco_stream_unread_byte(buf) == 'e');
    // assert(neco_stream_unread_byte(buf) == 'h');



    neco_stream_release(buf);


}

void test_stream_basic(void) {

    // Test for basic errors
    expect(neco_stream_make_buffered_size(0, 1, 16), NECO_INVAL);
    expect(neco_stream_make_buffered_size((void*)16, 1, 16), NECO_PERM);
    expect(neco_stream_make_buffered_size((void*)16, -1, 16), NECO_INVAL);
    expect(neco_stream_make_buffered_size(0, 1, 16), NECO_INVAL);
    expect(neco_stream_make_buffered_size((void*)16, 1, 16), NECO_PERM);
    expect(neco_stream_make_buffered_size((void*)16, -1, 16), NECO_INVAL);
    expect(neco_stream_flush(0), NECO_INVAL);
    expect(neco_stream_flush((void*)1), NECO_PERM);
    expect(neco_stream_read_byte(0), NECO_INVAL);
    expect(neco_stream_read_byte((void*)1), NECO_PERM);
    expect(neco_stream_unread_byte(0), NECO_INVAL);
    expect(neco_stream_unread_byte((void*)1), NECO_PERM);
    expect(neco_stream_buffered_read_size(0), NECO_INVAL);
    expect(neco_stream_buffered_read_size((void*)1), NECO_PERM);
    expect(neco_stream_release(0), NECO_INVAL);
    expect(neco_stream_release((void*)1), NECO_PERM);
    expect(neco_stream_buffered_write_size(0), NECO_INVAL);
    expect(neco_stream_buffered_write_size((void*)1), NECO_PERM);
    expect(neco_stream_read(0, 0, 0), NECO_INVAL);
    expect(neco_stream_read((void*)1, 0, 0), NECO_PERM);
    expect(neco_stream_write(0, 0, 0), NECO_INVAL);
    expect(neco_stream_write((void*)1, 0, 0), NECO_PERM);

    // Coroutine tests
    expect(neco_start(co_stream, 1, &(size_t){0}), NECO_OK);
    expect(neco_start(co_stream, 1, &(size_t){16}), NECO_OK);
}

void co_stream_big_other(int argc, void *argv[]) {
    assert(argc == 3);
    int fd = *(int*)argv[0];
    int N = *(int*)argv[1];
    char *rdata0 = argv[2];
    char *rdata = xmalloc(N);
    assert(rdata);
    neco_stream *buf;
    expect(neco_stream_make_buffered(&buf, fd), NECO_OK);
    ssize_t n = neco_stream_readfull(buf, rdata, N);
    assert(n == N);
    assert(memcmp(rdata, rdata0, N) == 0);
    ssize_t t = neco_stream_buffered_read_size(buf);
    neco_stream_release(buf);
    while (1) {
        n = neco_read(fd, rdata, N);
        if (n <= 0) {
            break;
        }
        t += n;
    }
    assert(t == N);
    close(fd);
    xfree(rdata);
}

void co_stream_big(int argc, void *argv[]) {
    (void)argc; (void)argv;

    int N = 123456;
    char *rdata = xmalloc(N);
    assert(rdata);
    expect(neco_rand(rdata, N, 0), NECO_OK);

    int fds[2];
    expect(neco_pipe(fds), NECO_OK);
    int fd = fds[0];
    neco_stream *buf;
    expect(neco_stream_make_buffered(&buf, fd), NECO_OK);
    expect(neco_start(co_stream_big_other, 3, &fds[1], &N, rdata), NECO_OK);
    int64_t other_id = neco_lastid();
    ssize_t n = neco_stream_write(buf, rdata, N);
    assert(n == N);
    expect(neco_stream_flush(buf), NECO_OK);
    neco_stream_release(buf);
    n = neco_write(fd, rdata, N);
    assert(n == N);
    close(fd);
    neco_join(other_id);
    xfree(rdata);
}

void test_stream_big(void) {
    expect(neco_start(co_stream_big, 0), NECO_OK);
}

void co_stream_partial_other(int argc, void *argv[]) {
    assert(argc == 3);
    int fd = *(int*)argv[0];
    int N = *(int*)argv[1];
    // char *rdata0 = argv[2];
    char *rdata = xmalloc(N);
    assert(rdata);
    // neco_stream *buf;
    // expect(neco_stream_make_buffered(&buf, fd), NECO_OK);
    // ssize_t n = neco_stream_readfull(buf, rdata, N);
    // assert(n == N);
    // assert(memcmp(rdata, rdata0, N) == 0);
    // ssize_t t = neco_stream_buffered_read_size(buf);
    while (1) {
        ssize_t n = neco_read(fd, rdata, N);
        if (n <= 0) {
            break;
        }
    }


    neco_stream *buf;
    expect(neco_stream_make_buffered(&buf, fd), NECO_OK);
    ssize_t n = neco_stream_readfull(buf, rdata, N);
    assert(n == NECO_EOF); // buffer closed
    neco_stream_release(buf);



    // assert(t == N);
    close(fd);
    xfree(rdata);
}

void co_stream_partial_write(int argc, void *argv[]) {
    (void)argc; (void)argv;

    int N = 123456;
    char *rdata = xmalloc(N);
    assert(rdata);
    expect(neco_rand(rdata, N, 0), NECO_OK);


    int fds[2];
    expect(neco_pipe(fds), NECO_OK);
    int fd = fds[0];
    expect(neco_start(co_stream_partial_other, 3, &fds[1], &N, rdata), NECO_OK);
    int64_t other_id = neco_lastid();

    neco_partial_write = 2;
    ssize_t n = neco_write(fd, rdata, N);
    assert(n < N);
    assert(neco_lasterr() == NECO_PARTIALWRITE);
    
    // assert(n == N);

    neco_stream *buf;
    expect(neco_stream_make_buffered(&buf, fd), NECO_OK);
    expect(neco_stream_write(buf, "hello", 5), 5);
    neco_fail_neco_malloc_counter = 1;
    expect(neco_stream_read(buf, rdata, N), NECO_NOMEM);
    neco_stream_release(buf);


    expect(neco_stream_make_buffered(&buf, fd), NECO_OK);
    expect(neco_stream_read_dl(buf, rdata, N, 1), NECO_TIMEDOUT);
    neco_fail_neco_malloc_counter = 1;
    expect(neco_stream_write(buf, rdata, N), NECO_NOMEM);
    neco_stream_release(buf);




    expect(neco_stream_make_buffered(&buf, fd), NECO_OK);
    expect(neco_stream_write(buf, rdata, 500), 500);
    neco_partial_write = 2;
    int ret = neco_stream_flush(buf);
    assert(ret == NECO_PARTIALWRITE);
    neco_stream_release(buf);


    expect(neco_stream_make_buffered(&buf, fd), NECO_OK);
    expect(neco_stream_write(buf, rdata, 4096), 4096);
    neco_partial_write = 1;
    ret = neco_stream_write(buf, "1", 1); // this forces an error
    assert(ret == -1 && errno == EIO);
    neco_stream_release(buf);



    // neco_partial_write = 2;
    // n = neco_stream_write(buf, rdata, N);
    // assert(n < N);
    // int ret = neco_stream_flush(buf);
    // printf("%d\n", ret);
    // assert(neco_lasterr() == NECO_PARTIALWRITE);
    

    close(fd);
    neco_join(other_id);
    xfree(rdata);
}

void test_stream_partial_write(void) {
    expect(neco_start(co_stream_partial_write, 0), NECO_OK);
}

void co_stream_nonbuffered_client(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int fd = neco_dial("tcp", "127.0.0.1:9182");
    assert(fd > 0);
    neco_stream *stream;
    expect(neco_stream_make(&stream, fd), NECO_OK);

    assert(neco_stream_write(stream, "hello\n", 6) == 6);
    assert(neco_stream_buffered_write_size(stream) == 0);

    expect(neco_stream_close(stream), NECO_OK);
}

void co_stream_nonbuffered(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int servefd = neco_serve("tcp", "127.0.0.1:9182");
    assert(servefd > 0);
    expect(neco_start(co_stream_nonbuffered_client, 0), NECO_OK);
    int fd = neco_accept(servefd, 0, 0);
    assert(fd > 0);
    neco_stream *stream;
    expect(neco_stream_make(&stream, fd), NECO_OK);

    char buf[100];
    assert(neco_stream_read(stream, buf, sizeof(buf)) == 6);
    assert(memcmp(buf, "hello\n", 6) == 0);
    assert(neco_stream_buffered_read_size(stream) == 0);

    expect(neco_stream_flush(stream), NECO_OK);
    expect(neco_stream_close(stream), NECO_OK);
    close(servefd);
}

void test_stream_nonbuffered(void) {
    expect(neco_stream_close(0), NECO_INVAL);
    expect(neco_stream_close((void*)1), NECO_PERM);
    expect(neco_start(co_stream_nonbuffered, 0), NECO_OK);
}

void co_stream_buffered_client(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int fd = neco_dial("tcp", "127.0.0.1:9182");
    assert(fd > 0);
    neco_stream *stream;
    expect(neco_stream_make_buffered(&stream, fd), NECO_OK);

    assert(neco_stream_write(stream, "hello\n", 6) == 6);
    assert(neco_stream_buffered_write_size(stream) == 6);

    expect(neco_stream_close(stream), NECO_OK);
}

void co_stream_buffered(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int servefd = neco_serve("tcp", "127.0.0.1:9182");
    assert(servefd > 0);
    expect(neco_start(co_stream_buffered_client, 0), NECO_OK);
    int fd = neco_accept(servefd, 0, 0);
    assert(fd > 0);
    neco_stream *stream;
    expect(neco_stream_make_buffered(&stream, fd), NECO_OK);

    char buf[100];
    assert(neco_stream_read(stream, buf, sizeof(buf)) == 6);
    assert(memcmp(buf, "hello\n", 6) == 0);
    assert(neco_stream_buffered_read_size(stream) == 0);

    expect(neco_stream_close(stream), NECO_OK);
    close(servefd);
}


void test_stream_buffered(void) {
    expect(neco_start(co_stream_buffered, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_stream_basic);
    do_test(test_stream_big);
    do_test(test_stream_partial_write);
    do_test(test_stream_buffered);
    do_test(test_stream_nonbuffered);
}
#endif
