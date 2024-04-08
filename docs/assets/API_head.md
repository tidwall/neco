# Neco C API

C API for the Neco coroutine library.

This document provides a detailed description of the functions and types in the
[neco.h](../neco.h) and [neco.c](../neco.c) source files for the Neco library.

For a more general overview please see the project 
[README](https://github.com/tidwall/neco). 


## Table of contents

- [Programing notes](#programming-notes)
- [Basic operations](#basic-operations)
- [Channels](#channels)
- [Generators](#generators)
- [Mutexes](#mutexes)
- [WaitGroups](#waitgroups)
- [Condition variables](#condition-variables)
- [Posix wrappers](#posix-wrappers)
- [File descriptor helpers](#file-descriptor-helpers)
- [Networking utilities](#Networking-utilities)
- [Streams and buffered I/O](#streams-and-buffered-io)
- [Random number generator](#random-number-generator)
- [Error handling](#error-handling)
- [Background worker](#background-worker)
- [Time](#time)
- [Signals](#signals)
- [Cancelation](#cancelation)
- [Stats and information](#stats-and-information)
- [Global environment](#global-environment)

## Programming notes

### neco_main

The `neco_main()` function may be used instead of the standard C `main()`,
which effectively runs the entire program in a Neco coroutine context.

```c
#include "neco.h"

int neco_main(int argc, char *argv[]) {
    // Running inside of a Neco coroutine
    return 0;
}
```

Doing so adjusts Neco to behave as follows:

- `neco_env_setpaniconerror(true)`
- `neco_env_setcanceltype(NECO_CANCEL_ASYNC)`
- The program will exit after the the main coroutine returns.


### Neco errors

Neco functions return Neco errors.

```c
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
#define NECO_TIMEDOUT      -10  ///< Deadline has elapsed (by neco_*_dl)
#define NECO_CANCELED      -11  ///< Operation canceled (by neco_cancel)
#define NECO_BUSY          -12  ///< Resource busy (by mutex_trylock)
#define NECO_NEGWAITGRP    -13  ///< Negative waitgroup counter
#define NECO_GAIERROR      -14  ///< Error with getaddrinfo (check neco_gai_error)
#define NECO_UNREADFAIL    -15  ///< Failed to unread byte (neco_stream_unread_byte)
#define NECO_PARTIALWRITE  -16  ///< Failed to write all data (neco_stream_flush)
#define NECO_NOTGENERATOR  -17  ///< Coroutine is not a generator (neco_gen_yield)
#define NECO_NOTSUSPENDED  -18  ///< Coroutine is not suspended (neco_resume)
```

Three of those errors will panic when `neco_env_setpaniconerror(true)`:
`NECO_INVAL`, `NECO_PERM`, and `NECO_NOMEM`.

Some Neco functions are Posix wrappers, which return the `NECO_ERROR` (-1) and 
it's the programmers responsibilty to check the errno.
Those functions include [neco_read()](#neco_read), [neco_write()](#neco_write), 
[neco_accept()](#neco_accept), and [neco_connect()](#neco_connect).


At any point the programmer may check the `neco_lasterr()` function to get the
Neco error from the last `neco_*` call. This function will ensure that system
errors will be automatically converted to its Neco equivalent. For example,
let's say that a `neco_read()` (which is Posix wrapper for `read()`) returns
`-1` and the `errno` is `EINVAL`, then `neco_lasterr()` will return
`NECO_INVAL`.

### Deadlines and cancelation

All operations that may block a coroutine will have an extended function with
a `_dl` suffix that provides an additional deadline argument. 
A deadline is a timestamp in nanoseconds. NECO_TIMEDOUT will be returned if
the operation does not complete in the provide time.

All operations that may block can also be canceled using the [`neco_cancel()`](docs/API.md#neco_cancel)
function from a different coroutine.
Calling `neco_cancel()` will not cancel an operation that does not block.
A cancel takes higher priority than a deadline. Such that if an operation was
canceled and also has timedout at the same time the cancel wins and
NECO_CANCELED will be returned.

```c

// Try to connect, timing out after one second.
int fd = neco_dial_dl("tcp", "google.com:80", neco_now() + NECO_SECOND);
if (fd < 0) {
    // Connection failed
    if (fd == NECO_CANCELED) {
        // Operation canceled
    } else if (fd == NECO_TIMEDOUT) {
        // Operation timedout
    } else {
        // Some other error
    }
    return;
}
// Success
```

#### Async cancelation

By default, when a coroutine is canceled it's the responsibility of the 
programmer to check the return value and perform any clean up. But it's also
possible to have Neco handle all that by enabling the async cancelation with
`neco_setcanceltype(NECO_CANCEL_ASYNC, 0)` at the coroutine level or 
`neco_env_setcanceltype(NECO_CANCEL_ASYNC)` globally from the main function.

When async cancelation is enabled, the coroutine that is canceled is terminated
right away, and any cleanup handled using the `neco_cleanup_push()` and 
`neco_cleanup_pop()` functions.

Async cancelation is automatically enabled when using `neco_main()`, instead of 
`main()`, as the program startup function.

#### Disabling cancelation

Finally, coroutine cancelation can be totally disabled by calling 
`neco_setcancelstate(NECO_CANCEL_DISABLE, 0)` at the coroutine level or 
`neco_env_setcancelstate(NECO_CANCEL_DISABLE)` globally from the main function.
