#include "tests.h"

#if defined(_WIN32)
DISABLED("test_net", "Windows")
#elif defined(__EMSCRIPTEN__)
DISABLED("test_net", "Emscripten")
#else

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

void co_net_serve_client(int argc, void *argv[]) {
    assert(argc == 2);
    char *proto = argv[0];
    char *addr = argv[1];
    int fd = neco_dial(proto, addr);
    assert(fd > 0);
    int ret = neco_write(fd, "+PING\r\n", 7);
    assert(ret == 7);
    char buf[256];
    ssize_t n = neco_read(fd, buf, sizeof(buf));
    assert(n == 7);
    buf[n] = '\0';
    assert(strcmp(buf, "+PONG\r\n") == 0);


    assert(neco_read_dl(fd, buf, sizeof(buf), 1) == NECO_ERROR && errno == ETIMEDOUT);
    expect(neco_cancel(neco_getid()), NECO_OK);
    assert(neco_read(fd, buf, sizeof(buf)) == NECO_ERROR && errno == ECANCELED);

    close(fd);
}

void co_net_serve_server(int argc, void *argv[]) {
    assert(argc == 1);
    int fd = *(int*)argv[0];
    char buf[256];
    ssize_t n = neco_read(fd, buf, sizeof(buf));
    assert(n == 7);
    buf[n] = '\0';
    assert(strcmp(buf, "+PING\r\n") == 0);
    assert(neco_write(fd, "+PONG\r\n", 7) == 7);
    close(fd);
}

void co_net_serve(int argc, void *argv[]) {
    assert(argc == 4);
    char *proto_sv = argv[0];
    char *addr_sv = argv[1];
    char *proto_cl = argv[2];
    char *addr_cl = argv[3];
    if (strcmp(proto_sv, "unix") == 0) {
        unlink(addr_sv);
    }
    int sockfd = neco_serve(proto_sv, addr_sv);
    assert(sockfd > 0);
    int fd2 = neco_serve(proto_sv, addr_sv);
    assert(fd2 == -1 && errno == EADDRINUSE);
    int N = 10;
    for (int i = 0; i < N; i++) {
        expect(neco_start(co_net_serve_client, 2, proto_cl, addr_cl), NECO_OK);
    }
    int i = 0;
    while (i < N) {
        neco_fail_evqueue_counter = 1;
        int fd = neco_accept(sockfd, 0, 0);
        neco_fail_evqueue_counter = 0;
        assert(fd > 0);
        expect(neco_start(co_net_serve_server, 1, &fd), NECO_OK);
        i++;
    }
    assert(close(sockfd) == 0);
}

void test_net_unix(void) {
    expect(neco_start(co_net_serve, 4, 
        "unix", "socket", "unix", "socket"), NECO_OK);
    expect(neco_start(co_net_serve, 4, 
        "unix", "socket", "unix", "socket"), NECO_OK);
}

void test_net_tcp_auto(void) {
    expect(neco_start(co_net_serve, 4, 
        "tcp", "localhost:19770", "tcp", "localhost:19770"), NECO_OK);
}

void test_net_tcp_mix40(void) {
    expect(neco_start(co_net_serve, 4, 
        "tcp4", "localhost:19770", "tcp", "localhost:19770"), NECO_OK);
}
void test_net_tcp_mix60(void) {
    expect(neco_start(co_net_serve, 4, 
        "tcp6", "[::]:19770", "tcp", "localhost:19770"), NECO_OK);
}
void test_net_tcp_mix44(void) {
    expect(neco_start(co_net_serve, 4, 
        "tcp4", "localhost:19770", "tcp4", "localhost:19770"), NECO_OK);
}
void test_net_tcp_mix66(void) {
    expect(neco_start(co_net_serve, 4, 
        "tcp6", "localhost:19770", "tcp6", "localhost:19770"), NECO_OK);
}

void test_net_tcp_emptyhost(void) {
    expect(neco_start(co_net_serve, 4, 
        "tcp", ":19770", "tcp", ":19770"), NECO_OK);
}

void co_net_cancel_child(int argc, void **argv) {
    assert(argc == 1);
    int64_t id = *(int64_t*)argv[0];
    expect(neco_cancel(id), NECO_OK); // Cancel Accept 1
    expect(neco_cancel(id), NECO_OK); // Cancel Accept 2
}

void co_net_cancel(int argc, void **argv) {
    assert(argc == 0);
    (void)argv;
    unlink("socket");
    int sockfd = neco_serve("unix", "socket");
    assert(sockfd > 0);
    // Test for deadline
    int fd = neco_accept_dl(sockfd, 0, 0, neco_now() + NECO_SECOND / 4);
    assert(fd == -1 && errno == ETIMEDOUT);

    int64_t id = neco_getid();
    assert(id > 0);
    expect(neco_start(co_net_cancel_child, 1, &id), NECO_OK);
    fd = neco_accept(sockfd, 0, 0); // Accept 1
    assert(fd == -1 && errno == ECANCELED);
    fd = neco_accept(sockfd, 0, 0); // Accept 2
    assert(fd == -1 && errno == ECANCELED);
    close(sockfd);
}

void test_net_cancel(void) {
    // Test for starting a socket reader and writer, performing deadlines and 
    // canceling.
    expect(neco_start(co_net_cancel, 0), NECO_OK);
}

void co_net_getaddrinfo(int argc, void **argv) {
    assert(argc == 0);
    (void)argv;

    struct addrinfo hints = { 0 };
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family = AF_INET;
    struct addrinfo *ainfo = NULL;

    int ret;

    // This will succeed
    ret = neco_getaddrinfo_dl("localhost", "19770", &hints, &ainfo, neco_now()+NECO_HOUR);
    assert(ret == 0);
    freeaddrinfo(ainfo);

    // This will timeout
    ret = neco_getaddrinfo_dl("localhost", "19770", &hints, &ainfo, neco_now()-1);
    assert(ret == EAI_SYSTEM && errno == ETIMEDOUT);
    // expect(neco_sleep(NECO_SECOND/4), NECO_OK);
    
    // This will succeed
    ret = neco_getaddrinfo_dl("localhost", "19770", &hints, &ainfo, neco_now()+NECO_HOUR);
    assert(ret == 0);
    freeaddrinfo(ainfo);
    
    // This will timeout
    ret = neco_getaddrinfo_dl("127.0.0.1", "19770", &hints, &ainfo, neco_now()-1);
    assert(ret == EAI_SYSTEM && errno == ETIMEDOUT);
    // expect(neco_sleep(NECO_SECOND/4), NECO_OK);

    // This will timeout
    ret = neco_getaddrinfo_dl("google.com", "19770", &hints, &ainfo, neco_now()-1);
    assert(ret == EAI_SYSTEM && errno == ETIMEDOUT);

    // This will succeed
    ret = neco_getaddrinfo("127.0.0.1", "19770", &hints, &ainfo);
    assert(ret == 0);
    freeaddrinfo(ainfo);

    // This will cancel
    int64_t id = neco_getid();
    expect(neco_cancel(id), NECO_OK);
    ret = neco_getaddrinfo_dl("google.com", "19770", &hints, &ainfo, neco_now()-1);
    assert(ret == EAI_SYSTEM && errno == ECANCELED);

    // This will cancel
    expect(neco_cancel(id), NECO_OK);
    ret = neco_getaddrinfo_dl("localhost", "19770", &hints, &ainfo, neco_now()-1);
    assert(ret == EAI_SYSTEM && errno == ECANCELED);

    // This will succeed
    int fd = neco_serve("tcp6", ":19970");
    assert(fd > 0);
    close(fd);

}

void test_net_getaddrinfo(void) {
    struct addrinfo hints = { 0 };
    struct addrinfo *ainfo = NULL;
    int ret = neco_getaddrinfo("localhost", "9999", &hints, &ainfo);
    assert(ret == EAI_SYSTEM && errno == EPERM);   
    expect(neco_start(co_net_getaddrinfo, 0), NECO_OK);
}

void co_net_dial_client(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    int fd;
    fd = neco_dial_dl("tcp", "127.0.0.1:19760", neco_now()+NECO_SECOND/4);
    assert(fd == -1 && errno == ECONNREFUSED);
    fd = neco_dial_dl("tcp", "127.0.0.1:19770", neco_now()-1);
    assert(fd == NECO_TIMEDOUT);
    fd = neco_dial_dl("tcp", "127.0.0.1:19770", neco_now()+NECO_SECOND);
    assert(fd > 0);
    close(fd);
}

void co_net_dial(int argc, void **argv) {
    assert(argc == 0);
    (void)argv;
    int sockfd, fd;
    expect(neco_dial(0, 0), NECO_INVAL);
    expect(neco_dial("", ""), NECO_INVAL);
    expect(neco_dial("unix", 
        "000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000000000000000000000000"
        ), NECO_INVAL);
    
    fd = neco_dial("unix", "/");
    assert(fd == -1 && (errno == ENOTSOCK || errno == EACCES));
    fd = neco_dial("tcp", "127.0.0.1:19760");
    assert(fd == -1 && errno == ECONNREFUSED);
    fd = neco_dial_dl("tcp", "127.0.0.1:19760", neco_now()+NECO_SECOND/4);
    assert(fd == -1 && errno == ECONNREFUSED);
    fd = neco_dial_dl("tcp", "127.0.0.1:19760", neco_now()-1);
    assert(fd == NECO_TIMEDOUT);
    sockfd = neco_serve("tcp", "127.0.0.1:19770");
    assert(sockfd > 0);
    expect(neco_start(co_net_dial_client, 0), NECO_OK);
    struct sockaddr_in addr = { 0 };
    socklen_t addrlen = sizeof(struct sockaddr_in);
    fd = neco_accept(sockfd, (struct sockaddr*)&addr, &addrlen);
    assert(fd > 0);
    assert(strcmp(inet_ntoa(addr.sin_addr), "127.0.0.1") == 0);
    assert(addr.sin_port > 0);
    close(fd);
    close(sockfd);
}

void test_net_dial(void) {
    expect(neco_start(co_net_dial, 0), NECO_OK);
}

void co_net_dial_fail(int argc, void **argv) {
    assert(argc == 0);
    (void)argv;

    int ret;

    neco_fail_socket_counter = 1;
    ret = neco_dial("unix", "socket");
    assert(ret == NECO_ERROR && errno == EMFILE);

    neco_fail_fcntl_counter = 1;
    ret = neco_dial("unix", "socket");
    assert(ret == NECO_ERROR && errno == EBADF);
    
}

void test_net_dial_fail(void) {
    expect(neco_dial(0, 0), NECO_PERM);
    expect(neco_read(0, 0, 0), NECO_ERROR, NECO_PERM);
    expect(neco_write(0, 0, 0), NECO_ERROR, NECO_PERM);
    expect(neco_start(co_net_dial_fail, 0), NECO_OK);
}

void co_net_getaddrinfo_fail(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    struct addrinfo hints = { 0 };
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_family = AF_INET;

    struct addrinfo *ainfo = NULL;
    int ret;
    ret = neco_getaddrinfo("", "9999", &hints, &ainfo);

#ifdef __APPLE__
    // Apple does not allow empty node strings
    assert(ret == 0);
    freeaddrinfo(ainfo);
#else
    assert(ret != 0);
#endif

    ret = neco_getaddrinfo(0, "9999", &hints, &ainfo);
    assert(ret == 0);
    freeaddrinfo(ainfo);

    ret = neco_getaddrinfo("i1o2293405", "9999", &hints, &ainfo);
    assert(ret != 0);

    // cause NOMEM errors at six different positions
    for (int i = 1; i <= 4; i++) {
        neco_fail_neco_malloc_counter = i;
        ret = neco_getaddrinfo("i1o2293405", "9999", &hints, &ainfo);
        assert(ret == EAI_MEMORY);
    }
    
    neco_fail_pipe_counter = 1;
    ret = neco_getaddrinfo("i1o2293405", "9999", &hints, &ainfo);
    assert(ret == EAI_SYSTEM && errno == EMFILE);

    neco_fail_fcntl_counter = 1;
    ret = neco_getaddrinfo("i1o2293405", "9999", &hints, &ainfo);
    assert(ret == EAI_SYSTEM && errno == EBADF);

    neco_fail_pthread_create_counter = 1;
    ret = neco_getaddrinfo("i1o2293405", "9999", &hints, &ainfo);
    assert(ret == EAI_SYSTEM && errno == EPERM);

    neco_fail_read_counter = 1;
    ret = neco_getaddrinfo("i1o2293405", "9999", &hints, &ainfo);
    assert(ret == EAI_SYSTEM && errno == EIO);


    expect(neco_sleep(NECO_SECOND/4), NECO_OK);
}

void test_net_getaddrinfo_fail(void) {
    expect(neco_start(co_net_getaddrinfo_fail, 0), NECO_OK);
}

void co_net_serve_fail(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    int fd;

    expect(neco_serve("tcp2", "localhost:80"), NECO_INVAL);

    unlink("socket");
    neco_fail_socket_counter = 1;
    fd = neco_serve("unix", "socket");
    assert(fd == -1 && errno == EMFILE);

    unlink("socket");
    neco_fail_listen_counter = 1;
    fd = neco_serve("unix", "socket");
    assert(fd == -1 && errno == EADDRINUSE);

    unlink("socket");
    neco_fail_fcntl_counter = 1;
    fd = neco_serve("unix", "socket");
    assert(fd == -1 && errno == EBADF);

    neco_fail_neco_malloc_counter = 1;
    fd = neco_serve("tcp", "localhost:80");
    assert(fd == NECO_NOMEM); 

    neco_fail_socket_counter = 1;
    fd = neco_serve("tcp", "localhost:80");
    assert(fd == -1 && errno == EMFILE);

    neco_fail_setsockopt_counter = 1;
    fd = neco_serve("tcp", "localhost:80");
    assert(fd == -1 && errno == EBADF);

    neco_fail_bind_counter = 1;
    fd = neco_serve("tcp", "localhost:19970");
    assert(fd == -1 && errno == EADDRINUSE);

    neco_fail_listen_counter = 1;
    fd = neco_serve("tcp", "localhost:19970");
    assert(fd == -1 && errno == EADDRINUSE);

    neco_fail_fcntl_counter = 1;
    fd = neco_serve("tcp", "localhost:19970");
    assert(fd == -1 && errno == EBADF);

    fd = neco_serve("tcp", "");
    assert(fd == NECO_INVAL);

    fd = neco_serve("tcp", "localhost");
    assert(fd == NECO_INVAL);


    expect(neco_cancel(neco_getid()), NECO_OK);
    fd = neco_serve("tcp", "127.0.0.1:19970");
    assert(fd == NECO_CANCELED);


}

void test_net_serve_fail(void) {
    expect(neco_serve(0, "localhost:80"), NECO_INVAL);
    expect(neco_serve("tcp", 0), NECO_INVAL);
    expect(neco_serve("tcp", "localhost:80"), NECO_PERM);
    expect(neco_start(co_net_serve_fail, 0), NECO_OK);
}

void co_net_autoclose_queue(int argc, void *argv[]) {
    (void)argc; (void)argv;
    // Should automatically close the qfd after 100 ms
    int fd = neco_serve("tcp", "localhost:19970");
    assert(fd != -1);
    expect(neco_accept_dl(fd,0,0, neco_now()+NECO_SECOND/4), NECO_ERROR, NECO_TIMEDOUT);
    close(fd);
    expect(neco_sleep(NECO_SECOND/4), NECO_OK);
}

void test_net_autoclose_queue(void) {
    expect(neco_start(co_net_autoclose_queue, 0), NECO_OK);
}

void co_net_write_errors_reader(int argc, void *argv[]) {
    assert(argc == 1);
    neco_waitgroup *wg = argv[0];
    neco_sleep(NECO_MILLISECOND*100);
    errno = 0;
    int fd = neco_dial("unix", "socket");
    // assert(fd == -1);
    // fd = neco_dial("unix", "socket");
    assert(fd > 0);
    // printf("=====================>>>>  !!!!\n");
    char data[50];
    // should be closed
    assert(neco_read(fd, &data, sizeof(data)) == 0);
    close(fd);

    fd = neco_dial("unix", "socket");
    // assert(fd == -1);
    // fd = neco_dial("unix", "socket");
    assert(fd > 0);
    // printf("=====================>>>>  !!!!\n");


    assert(neco_read(fd, &data, sizeof(data)) == 5);
    close(fd);
    neco_waitgroup_done(wg);
}

void co_net_write_errors(int argc, void *argv[]) {
    (void)argc; (void)argv;

    unlink("socket");
    neco_fail_evqueue_counter = 1;
    int sock = neco_serve("unix", "socket");
    assert(sock > 0);
    neco_waitgroup wg = { 0 };
    neco_waitgroup_init(&wg);
    neco_waitgroup_add(&wg, 1);
    neco_start(co_net_write_errors_reader, 1, &wg);

    neco_sleep(NECO_SECOND/10);
    neco_cancel(neco_getid());
    assert(neco_accept(sock, 0, 0) == -1 && errno == ECANCELED);
    assert(neco_accept_dl(sock, 0, 0, 1) == -1 && errno == ETIMEDOUT);

    neco_fail_accept_counter = 1;
    neco_fail_accept_error = EBADF;
    assert(neco_accept(sock, 0, 0) == -1 && errno == EBADF);
    // printf(">>> %d\n", neco_fail_fcntl_counter);

    neco_fail_fcntl_counter = 1;
    neco_fail_fcntl_error = EBADF;
    assert(neco_accept(sock, 0, 0) == -1 && errno == EBADF);

    int fd = neco_accept(sock, 0, 0);
    // printf(">>> %d\n", neco_fail_fcntl_counter);
    assert(fd > 0);
// printf("================\n");
    neco_cancel(neco_getid());
    assert(neco_write(fd, "hello", 5) == -1 && errno == ECANCELED);
    assert(neco_write_dl(fd, "hello", 5, 1) == -1 && errno == ETIMEDOUT);

    neco_fail_write_counter = 1;
    neco_fail_write_error = EBADF;
    assert(neco_write(fd, "hello", 5) == -1 && errno == EBADF);

    // This will succeed even though a various errors occure because neco
    // reschedules reads and writes on some common errors.
    neco_fail_write_counter = 1;
    neco_fail_write_error = EAGAIN;
    neco_fail_cowait = true;
    int ret = neco_write(fd, "hello", 5);

    assert(ret == 5);
    neco_waitgroup_wait(&wg);
    close(fd);
    close(sock);
}

void test_net_write_errors(void) {
    assert(neco_write(0, 0, 0) == -1 && errno == EPERM);
    assert(neco_accept(0, 0, 0) == -1 && errno == EPERM);
    expect(neco_start(co_net_write_errors, 0), NECO_OK);
}

void co_net_connect_errors(int argc, void *argv[]) {
    (void)argc; (void)argv;
    assert(neco_connect(99283, 0, 0) == -1);
    assert(neco_connect_dl(99283, 0, 0, 1) == -1 && errno == ETIMEDOUT);
    expect(neco_cancel(neco_getid()), NECO_OK);
    assert(neco_connect_dl(99283, 0, 0, 1) == -1 && errno == ECANCELED);

    neco_fail_connect_counter = 1;
    neco_fail_connect_error = EAGAIN;
    assert(neco_connect(99283, 0, 0) == -1 && errno == ECONNREFUSED);

    neco_fail_connect_counter = 1;
    neco_fail_connect_error = EISCONN;
    assert(neco_connect(99283, 0, 0) == -1 && errno == EISCONN);

    neco_fail_connect_counter = 1;
    neco_fail_connect_error = EINTR;
    assert(neco_connect(99283, 0, 0) == -1);

}

void test_net_connect_errors(void) {
    assert(neco_connect_dl(0, 0, 0, 0) == -1 && errno == EPERM);
    expect(neco_start(co_net_connect_errors, 0), NECO_OK);
}

void test_net_setnonblock(void) {
    int fds[2];
    assert(pipe(fds) == 0);
    bool prev;
    expect(neco_setnonblock(fds[0], 1, &prev), NECO_OK);
    assert(!prev);
    expect(neco_setnonblock(fds[0], 1, &prev), NECO_OK);
    assert(prev);
    expect(neco_setnonblock(fds[0], 0, &prev), NECO_OK);
    assert(prev);
    expect(neco_setnonblock(fds[0], 0, &prev), NECO_OK);
    assert(!prev);
    close(fds[0]);
    close(fds[1]);
}

int main(int argc, char **argv) {
    do_test(test_net_unix);
    do_test(test_net_tcp_auto);
    do_test(test_net_tcp_mix40);
    do_test(test_net_tcp_mix60);
    do_test(test_net_tcp_mix44);
    do_test(test_net_tcp_mix66);
    do_test(test_net_dial);
    do_test(test_net_dial_fail);
    do_test(test_net_tcp_emptyhost);
    do_test(test_net_cancel);
    do_test(test_net_getaddrinfo);
    do_test(test_net_getaddrinfo_fail);
    do_test(test_net_serve_fail);
    do_test(test_net_autoclose_queue);
    do_test(test_net_write_errors);
    do_test(test_net_connect_errors);
    do_test(test_net_setnonblock);
}
#endif
