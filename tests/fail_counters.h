#ifdef NECO_TESTING

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#undef read0
#undef recv0
#undef write0
#undef send0
#undef accept0
#undef connect0
#undef socket0
#undef bind0
#undef listen0
#undef setsockopt0
#undef nanosleep0
#undef fcntl0
#undef evqueue0
#undef pthread_create0
#undef pthread_detach0
#undef pipe0
#undef malloc0
#undef realloc0
#undef stack_get0

#define FAIL_CALL(type, name0, name, ret, err, params, args) \
__thread int neco_fail_ ## name ## _counter = 0; \
__thread int neco_fail_ ## name ## _error = err; \
static type name0 params { \
    type retout; \
    if (neco_fail_ ## name ## _counter == 1) { \
        errno =  neco_fail_ ## name ## _error; \
        neco_fail_ ## name ## _error = err; \
        retout = ret; \
    } else { \
        retout = name args; \
    } \
    if (neco_fail_ ## name ## _counter > 0) { \
        neco_fail_ ## name ## _counter--; \
    } \
    return retout; \
}
typedef void *voidp;
FAIL_CALL(ssize_t, recv0, recv, -1, EIO,
    (int fd, void *data, size_t nbytes, int flags), 
    (fd, data, nbytes, flags))
FAIL_CALL(ssize_t, read0, read, -1, EIO,
    (int fd, void *data, size_t nbytes), 
    (fd, data, nbytes))
FAIL_CALL(ssize_t, send0, send, -1, EIO,
    (int fd, const void *data, size_t nbytes, int flags), 
    (fd, data, nbytes, flags))
FAIL_CALL(ssize_t, write0, write, -1, EIO,
    (int fd, const void *data, size_t nbytes), 
    (fd, data, nbytes))
FAIL_CALL(int, accept0, accept, -1, EMFILE,
    (int sockfd, struct sockaddr *addr, socklen_t *addrlen), 
    (sockfd, addr, addrlen))
FAIL_CALL(int, connect0, connect, -1, ECONNREFUSED,
    (int sockfd, const struct sockaddr *addr, socklen_t addrlen), 
    (sockfd, addr, addrlen))
FAIL_CALL(int, socket0, socket, -1, EMFILE,
    (int domain, int type, int protocol), 
    (domain, type, protocol))
FAIL_CALL(int, bind0, bind, -1, EADDRINUSE,
    (int sockfd, const struct sockaddr *addr, socklen_t addrlen), 
    (sockfd, addr, addrlen))
FAIL_CALL(int, listen0, listen, -1, EADDRINUSE,
    (int sockfd, int backlog), 
    (sockfd, backlog))
FAIL_CALL(int, setsockopt0, setsockopt, -1, EBADF,
    (int sockfd, int level, int optname, const void *optval, socklen_t optlen), 
    (sockfd, level, optname, optval, optlen))
FAIL_CALL(int, nanosleep0, nanosleep, -1, EINTR,
    (const struct timespec *req, struct timespec *rem), 
    (req, rem))
FAIL_CALL(int, evqueue0, evqueue, -1, EBADF,
    (void), 
    ())
FAIL_CALL(int, pthread_create0, pthread_create, EPERM, EPERM,
    (pthread_t *thread, const pthread_attr_t *attr, 
        void *(*start_routine)(void *arg), void *arg), 
    (thread, attr, start_routine, arg))
FAIL_CALL(int, pthread_detach0, pthread_detach, EINVAL, EINVAL,
    (pthread_t thread), 
    (thread))
FAIL_CALL(voidp, malloc0, neco_malloc, NULL, ENOMEM,
    (size_t nbytes), 
    (nbytes))
FAIL_CALL(voidp, realloc0, neco_realloc, NULL, ENOMEM,
    (void *ptr, size_t nbytes), 
    (ptr, nbytes))
FAIL_CALL(int, stack_get0, stack_get, -1, ENOMEM,
    (struct stack_mgr *mgr, struct stack *stack), 
    (mgr, stack))

#ifndef _WIN32
FAIL_CALL(int, fcntl0, fcntl, -1, EBADF,
    (int fd, int cmd, int arg), 
    (fd, cmd, arg))
FAIL_CALL(int, pipe0, pipe, -1, EMFILE,
    (int fds[2]), 
    (fds))
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
