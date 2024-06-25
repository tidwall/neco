// https://github.com/tidwall/neco
//
// Copyright 2024 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
//
// Neco -- Coroutine library for C

#ifndef NECO_H
#define NECO_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

////////////////////////////////////////////////////////////////////////////////
// basic operations
////////////////////////////////////////////////////////////////////////////////

/// @defgroup BasicOperations Basic operations
/// Neco provides standard operations for starting a coroutine, sleeping, 
/// suspending, resuming, yielding to another coroutine, joining/waiting for
/// child coroutines, and exiting a running coroutine.
/// @{
int neco_start(void(*coroutine)(int argc, void *argv[]), int argc, ...);
int neco_startv(void(*coroutine)(int argc, void *argv[]), int argc, void *argv[]);
int neco_yield(void);
int neco_sleep(int64_t nanosecs);
int neco_sleep_dl(int64_t deadline);
int neco_join(int64_t id);
int neco_join_dl(int64_t id, int64_t deadline);
int neco_suspend(void);
int neco_suspend_dl(int64_t deadline);
int neco_resume(int64_t id);
void neco_exit(void);
int64_t neco_getid(void);
int64_t neco_lastid(void);
int64_t neco_starterid(void);
/// @}

////////////////////////////////////////////////////////////////////////////////
// channels
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Channels Channels
/// Channels allow for sending and receiving values between coroutines.
/// By default, sends and receives will block until the other side is ready.
/// This allows the coroutines to synchronize without using locks or condition
/// variables.
/// @{
typedef struct neco_chan neco_chan;

int neco_chan_make(neco_chan **chan, size_t data_size, size_t capacity);
int neco_chan_retain(neco_chan *chan);
int neco_chan_release(neco_chan *chan);
int neco_chan_send(neco_chan *chan, void *data);
int neco_chan_send_dl(neco_chan *chan, void *data, int64_t deadline);
int neco_chan_broadcast(neco_chan *chan, void *data);
int neco_chan_recv(neco_chan *chan, void *data);
int neco_chan_recv_dl(neco_chan *chan, void *data, int64_t deadline);
int neco_chan_tryrecv(neco_chan *chan, void *data);
int neco_chan_close(neco_chan *chan);
int neco_chan_select(int nchans, ...);
int neco_chan_select_dl(int64_t deadline, int nchans, ...);
int neco_chan_selectv(int nchans, neco_chan *chans[]);
int neco_chan_selectv_dl(int nchans, neco_chan *chans[], int64_t deadline);
int neco_chan_tryselect(int nchans, ...);
int neco_chan_tryselectv(int nchans, neco_chan *chans[]);
int neco_chan_case(neco_chan *chan, void *data);
/// @}

////////////////////////////////////////////////////////////////////////////////
// generators
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Generators Generators
/// A generator is a specialized iterator-bound coroutine that can produce a
/// sequence of values to be iterated over.
/// @{
typedef struct neco_gen neco_gen;

int neco_gen_start(neco_gen **gen, size_t data_size, void(*coroutine)(int argc, void *argv[]), int argc, ...);
int neco_gen_startv(neco_gen **gen, size_t data_size, void(*coroutine)(int argc, void *argv[]), int argc, void *argv[]);
int neco_gen_retain(neco_gen *gen);
int neco_gen_release(neco_gen *gen);
int neco_gen_yield(void *data);
int neco_gen_yield_dl(void *data, int64_t deadline);
int neco_gen_next(neco_gen *gen, void *data);
int neco_gen_next_dl(neco_gen *gen, void *data, int64_t deadline);
int neco_gen_close(neco_gen *gen);
/// @}

////////////////////////////////////////////////////////////////////////////////
// synchronization mechanisms
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Mutexes Mutexes
/// A mutex is synchronization mechanism that blocks access to variables by
/// multiple coroutines at once. This enforces exclusive access by a coroutine
/// to a variable or set of variables and helps to avoid data inconsistencies
/// due to race conditions.
/// @{
typedef struct { _Alignas(16) char _[48]; } neco_mutex;

#define NECO_MUTEX_INITIALIZER { 0 }

int neco_mutex_init(neco_mutex *mutex);
int neco_mutex_lock(neco_mutex *mutex);
int neco_mutex_lock_dl(neco_mutex *mutex, int64_t deadline);
int neco_mutex_trylock(neco_mutex *mutex);
int neco_mutex_unlock(neco_mutex *mutex);
int neco_mutex_rdlock(neco_mutex *mutex);
int neco_mutex_rdlock_dl(neco_mutex *mutex, int64_t deadline);
int neco_mutex_tryrdlock(neco_mutex *mutex);
/// @}

/// @defgroup WaitGroups WaitGroups
/// A WaitGroup waits for a multiple coroutines to finish. 
/// The main coroutine calls neco_waitgroup_add() to set the number of
/// coroutines to wait for. Then each of the coroutines runs and calls
/// neco_waitgroup_done() when complete.
/// At the same time, neco_waitgroup_wait() can be used to block until all
/// coroutines are completed.
/// @{
typedef struct { _Alignas(16) char _[48]; } neco_waitgroup;

#define NECO_WAITGROUP_INITIALIZER { 0 }

int neco_waitgroup_init(neco_waitgroup *waitgroup);
int neco_waitgroup_add(neco_waitgroup *waitgroup, int delta);
int neco_waitgroup_done(neco_waitgroup *waitgroup);
int neco_waitgroup_wait(neco_waitgroup *waitgroup);
int neco_waitgroup_wait_dl(neco_waitgroup *waitgroup, int64_t deadline);
/// @}

/// @defgroup CondVar Condition variables
/// A condition variable is a synchronization mechanism that allows coroutines
/// to suspend execution until some condition is true. 
/// @{
typedef struct { _Alignas(16) char _[48]; } neco_cond;
#define NECO_COND_INITIALIZER { 0 }

int neco_cond_init(neco_cond *cond);
int neco_cond_signal(neco_cond *cond);
int neco_cond_broadcast(neco_cond *cond);
int neco_cond_wait(neco_cond *cond, neco_mutex *mutex);
int neco_cond_wait_dl(neco_cond *cond, neco_mutex *mutex, int64_t deadline);
/// @}

////////////////////////////////////////////////////////////////////////////////
// file descriptors
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Posix Posix wrappers
/// Functions that work like their Posix counterpart but do not block, allowing
/// for usage in a Neco coroutine.
/// @{

// wrappers for various posix operations.
ssize_t neco_read(int fd, void *data, size_t nbytes);
ssize_t neco_read_dl(int fd, void *data, size_t nbytes, int64_t deadline);
ssize_t neco_write(int fd, const void *data, size_t nbytes);
ssize_t neco_write_dl(int fd, const void *data, size_t nbytes, int64_t deadline);
int neco_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int neco_accept_dl(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int64_t deadline);
int neco_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int neco_connect_dl(int sockfd, const struct sockaddr *addr, socklen_t addrlen, int64_t deadline);
int neco_getaddrinfo(const char *node, const char *service,
    const struct addrinfo *hints, struct addrinfo **res);
int neco_getaddrinfo_dl(const char *node, const char *service,
    const struct addrinfo *hints, struct addrinfo **res, int64_t deadline);
/// @}

/// @defgroup Posix2 File descriptor helpers
/// Functions for working with file descriptors.
/// @{

// utility for enabling non-blocking on existing file descriptors
int neco_setnonblock(int fd, bool nonblock, bool *oldnonblock);

// wait for a file descriptor to be readable or writeable.
#define NECO_WAIT_READ  1
#define NECO_WAIT_WRITE 2

int neco_wait(int fd, int mode);
int neco_wait_dl(int fd, int mode, int64_t deadline);

/// @}

////////////////////////////////////////////////////////////////////////////////
// networking
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Networking Networking utilities
/// @{

int neco_serve(const char *network, const char *address);
int neco_serve_dl(const char *network, const char *address, int64_t deadline);
int neco_dial(const char *network, const char *address);
int neco_dial_dl(const char *network, const char *address, int64_t deadline);

/// @}

////////////////////////////////////////////////////////////////////////////////
// cancelation
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Cancelation Cancelation
/// @{

int neco_cancel(int64_t id);
int neco_cancel_dl(int64_t id, int64_t deadline);

#define NECO_CANCEL_ASYNC      1
#define NECO_CANCEL_INLINE     2
#define NECO_CANCEL_ENABLE     3
#define NECO_CANCEL_DISABLE    4

int neco_setcanceltype(int type, int *oldtype);
int neco_setcancelstate(int state, int *oldstate);

#define neco_cleanup_push(routine, arg) {__neco_c0(&(char[32]){0},routine,arg);
#define neco_cleanup_pop(execute)        __neco_c1(execute);}

/// @}

////////////////////////////////////////////////////////////////////////////////
// random number generator
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Random Random number generator
/// @{

#define NECO_CSPRNG 0 // Cryptographically secure pseudorandom number generator
#define NECO_PRNG   1 // Pseudorandom number generator (non-crypto, faster)

int neco_rand_setseed(int64_t seed, int64_t *oldseed);
int neco_rand(void *data, size_t nbytes, int attr);
int neco_rand_dl(void *data, size_t nbytes, int attr, int64_t deadline);

/// @}

////////////////////////////////////////////////////////////////////////////////
// signal handling
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Signals Signals
/// Allows for signals, such as SIGINT (Ctrl-C), to be intercepted or ignored.
/// @{

int neco_signal_watch(int signo);
int neco_signal_wait(void);
int neco_signal_wait_dl(int64_t deadline);
int neco_signal_unwatch(int signo);

/// @}


////////////////////////////////////////////////////////////////////////////////
// background worker
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Worker Background worker
/// Run arbitrary code in a background worker thread
/// @{

int neco_work(int64_t pin, void(*work)(void *udata), void *udata);

/// @}

////////////////////////////////////////////////////////////////////////////////
// Stats and information
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Stats Stats and information
/// @{

typedef struct neco_stats {
    size_t coroutines;   ///< Number of active coroutines
    size_t sleepers;     ///< Number of sleeping coroutines
    size_t evwaiters;    ///< Number of coroutines waiting on I/O events
    size_t sigwaiters;   ///<
    size_t senders;      ///<
    size_t receivers;    ///<
    size_t locked;       ///<
    size_t waitgroupers; ///<
    size_t condwaiters;  ///<
    size_t suspended;    ///<
    size_t workers;      ///< Number of background worker threads
} neco_stats;

int neco_getstats(neco_stats *stats);
int neco_is_main_thread(void);
const char *neco_switch_method(void);

/// @}

////////////////////////////////////////////////////////////////////////////////
// global behaviors
////////////////////////////////////////////////////////////////////////////////

/// @defgroup GlobalFuncs Global environment
/// @{

void neco_env_setallocator(void *(*malloc)(size_t), void *(*realloc)(void*, size_t), void (*free)(void*));
void neco_env_setpaniconerror(bool paniconerror);
void neco_env_setcanceltype(int type);
void neco_env_setcancelstate(int state);

/// @}

////////////////////////////////////////////////////////////////////////////////
// time and duration
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Time Time
/// Functions for working with time.
///
/// The following defines are available for convenience.
///
/// ```c
/// #define NECO_NANOSECOND  INT64_C(1)
/// #define NECO_MICROSECOND INT64_C(1000)
/// #define NECO_MILLISECOND INT64_C(1000000)
/// #define NECO_SECOND      INT64_C(1000000000)
/// #define NECO_MINUTE      INT64_C(60000000000)
/// #define NECO_HOUR        INT64_C(3600000000000)
/// ```
///
/// @{

#define NECO_NANOSECOND  INT64_C(1)
#define NECO_MICROSECOND INT64_C(1000)
#define NECO_MILLISECOND INT64_C(1000000)
#define NECO_SECOND      INT64_C(1000000000)
#define NECO_MINUTE      INT64_C(60000000000)
#define NECO_HOUR        INT64_C(3600000000000)

int64_t neco_now(void);

///@}

////////////////////////////////////////////////////////////////////////////////
// errors
////////////////////////////////////////////////////////////////////////////////

/// @defgroup ErrorFuncs Error handling
/// Functions for working with [Neco errors](./API.md#neco-errors). 
/// @{

#define NECO_OK              0  ///< Successful result (no error)
#define NECO_ERROR          -1  ///< System error (check errno)
#define NECO_INVAL          -2  ///< Invalid argument
#define NECO_PERM           -3  ///< Operation not permitted
#define NECO_NOMEM          -4  ///< Cannot allocate memory
#define NECO_EOF            -5  ///< End of file or stream (neco_stream_*)
#define NECO_NOTFOUND       -6  ///< No such coroutine (neco_cancel)
#define NECO_NOSIGWATCH     -7  ///< Not watching on a signal
#define NECO_CLOSED         -8  ///< Channel is closed
#define NECO_EMPTY          -9  ///< Channel is empty (neco_chan_tryrecv)
#define NECO_TIMEDOUT      -10  ///< Deadline has elapsed (neco_*_dl)
#define NECO_CANCELED      -11  ///< Operation canceled (by neco_cancel)
#define NECO_BUSY          -12  ///< Resource busy (mutex_trylock)
#define NECO_NEGWAITGRP    -13  ///< Negative waitgroup counter
#define NECO_GAIERROR      -14  ///< Error with getaddrinfo (check neco_gai_error)
#define NECO_UNREADFAIL    -15  ///< Failed to unread byte (neco_stream_unread_byte)
#define NECO_PARTIALWRITE  -16  ///< Failed to write all data (neco_stream_flush)
#define NECO_NOTGENERATOR  -17  ///< Coroutine is not a generator (neco_gen_yield)
#define NECO_NOTSUSPENDED  -18  ///< Coroutine is not suspended (neco_resume)

const char *neco_strerror(ssize_t errcode);
int neco_lasterr(void);
int neco_gai_lasterr(void);
int neco_panic(const char *fmt, ...);

/// @}

////////////////////////////////////////////////////////////////////////////////
// streama
////////////////////////////////////////////////////////////////////////////////

/// @defgroup Streams Streams and Buffered I/O
/// Create a Neco stream from a file descriptor using neco_stream_make() or
/// a buffered stream using neco_stream_make_buffered().
/// @{

typedef struct neco_stream neco_stream;

int neco_stream_make(neco_stream **stream, int fd);
int neco_stream_make_buffered(neco_stream **stream, int fd);
int neco_stream_close(neco_stream *stream);
int neco_stream_close_dl(neco_stream *stream, int64_t deadline);
ssize_t neco_stream_read(neco_stream *stream, void *data, size_t nbytes);
ssize_t neco_stream_read_dl(neco_stream *stream, void *data, size_t nbytes, int64_t deadline);
ssize_t neco_stream_write(neco_stream *stream, const void *data, size_t nbytes);
ssize_t neco_stream_write_dl(neco_stream *stream, const void *data, size_t nbytes, int64_t deadline);
ssize_t neco_stream_readfull(neco_stream *stream, void *data, size_t nbytes);
ssize_t neco_stream_readfull_dl(neco_stream *stream, void *data, size_t nbytes, int64_t deadline);
int neco_stream_read_byte(neco_stream *stream);
int neco_stream_read_byte_dl(neco_stream *stream, int64_t deadline);
int neco_stream_unread_byte(neco_stream *stream);
int neco_stream_flush(neco_stream *stream);
int neco_stream_flush_dl(neco_stream *stream, int64_t deadline);
ssize_t neco_stream_buffered_read_size(neco_stream *stream);
ssize_t neco_stream_buffered_write_size(neco_stream *stream);

/// @}

////////////////////////////////////////////////////////////////////////////////
// happy convenience macro
////////////////////////////////////////////////////////////////////////////////

// int neco_main(int argc, char *argv[]);

#include <stdio.h>
#include <stdlib.h>

#define neco_main \
__neco_main(int argc, char *argv[]); \
static void _neco_main(int argc, void *argv[]) { \
    (void)argc; \
    __neco_exit_prog(__neco_main(*(int*)argv[0], *(char***)argv[1])); \
} \
int main(int argc, char *argv[]) { \
    neco_env_setpaniconerror(true); \
    neco_env_setcanceltype(NECO_CANCEL_ASYNC); \
    int ret = neco_start(_neco_main, 2, &argc, &argv); \
    fprintf(stderr, "neco_start: %s (code %d)\n", neco_strerror(ret), ret); \
    return -1; \
}; \
int __neco_main

////////////////////////////////////////////////////////////////////////////////
// private functions, not to be call directly
////////////////////////////////////////////////////////////////////////////////

void __neco_c0(void*,void(*)(void*),void*); 
void __neco_c1(int);
void __neco_exit_prog(int);

////////////////////////////////////////////////////////////////////////////////

#ifndef EAI_SYSTEM
#define EAI_SYSTEM 11
#endif

#endif // NECO_H
