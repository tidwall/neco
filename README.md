

<p align="center">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo-dark.png">
  <source media="(prefers-color-scheme: light)" srcset="docs/assets/logo-light.png">
  <img alt="Neco" src="docs/assets/logo-light.png" width="260">
</picture>
</p>
<p align="center">
<a href="docs/API.md"><img src="https://img.shields.io/badge/api-reference-blue.svg?style=flat-square" alt="API Reference"></a>
</p>

Neco is a C library that provides concurrency using coroutines.
It's small & fast, and intended to make concurrent I/O & network programming 
easy.

## Features

- [Coroutines](docs/API.md#basic-operations): starting, sleeping, suspending, resuming, yielding, and joining.
- [Synchronization](docs/API.md#channels): channels, generators, mutexes, condition variables, and waitgroups.
- Support for [deadlines and cancelation](docs/API.md#deadlines-and-cancelation).
- [Posix friendly](docs/API.md#posix-wrappers) interface using file descriptors.
- Additional APIs for [networking](docs/API.md#networking-utilities),
[signals](docs/API.md#signals), [random data](docs/API.md#random-number-generator), [streams](docs/API.md#streams-and-buffered-io), and [buffered I/O](docs/API.md#streams-and-buffered-io).
- Lightweight runtime with a fair and deterministic [scheduler](#the-scheduler).
- [Fast](#fast-context-switching) user-space context switching. Uses assembly in most cases.
- Stackful coroutines that are nestable, with their life times fully managed by the scheduler.
- Cross-platform. Linux, Mac, FreeBSD. _(Also WebAssembly and Windows with [some limitations](#platform-notes))_.
- Single file amalgamation. No dependencies.
- [Test suite](tests/README.md) with 100% coverage using sanitizers and [Valgrind](https://valgrind.org).

For a deeper dive, check out the [API reference](docs/API.md).

It may also be worthwhile to see the [Bluebox](https://github.com/tidwall/bluebox) project for a
more complete example of using Neco, including benchmarks.

## Goals

- Give C programs fast single-threaded concurrency.
- To use a concurrency model that resembles the simplicity of pthreads or Go.
- Provide an API for concurrent networking and I/O.
- Make it easy to interop with existing Posix functions.

It's a non-goal for Neco to provide a scalable multithreaded runtime, where the
coroutine scheduler is shared among multiple cpu cores. Or to use other 
concurrency models like async/await.

## Using

Just drop the "neco.c" and "neco.h" files into your project. Uses standard C11 so most modern C compilers should work.

```sh
cc -c neco.c
```

## Example 1 (Start a coroutine)

A coroutine is started with the [`neco_start()`](docs/API.md#neco_start) function.

When `neco_start()` is called for the first time it will initialize a Neco runtime and scheduler for the current thread, and then blocks until the coroutine and all child coroutines have terminated.

```c
#include <stdio.h>
#include "neco.h"

void coroutine(int argc, void *argv[]) {
    printf("main coroutine started\n");
}

int main(int argc, char *argv[]) {
    neco_start(coroutine, 0);
    return 0;
}
```

## Example 2 (Use neco_main instead of main)

Optionally, [`neco_main()`](docs/API.md#neco_main) can be used in place of the standard `main()`.
This is for when the entirety of your program is intended to be run from only coroutines.
It [adjusts the behavior](docs/API.md#neco_main) of the program slightly to make development and error checking easier.

```c
#include <stdio.h>
#include "neco.h"

int neco_main(int argc, char *argv[]) {
    printf("main coroutine started\n");
    return 0;
}
```

## Example 3 (Multiple coroutines)

Here we'll start two coroutines that continuously prints "tick" every one second and "tock" every two.

```c
#include <stdio.h>
#include "neco.h"

void ticker(int argc, void *argv[]) {
    while (1) {
        neco_sleep(NECO_SECOND);
        printf("tick\n");
    }
}

void tocker(int argc, void *argv[]) {
    while (1) {
        neco_sleep(NECO_SECOND*2);
        printf("tock\n");
    }
}

int neco_main(int argc, char *argv[]) {
    neco_start(ticker, 0);
    neco_start(tocker, 0);
    
    // Keep the program alive for an hour.
    neco_sleep(NECO_HOUR);
    return 0;
}
```

## Example 4 (Coroutine arguments)

A coroutine is like its own little program that accepts any number of arguments.

```c
void coroutine(int argc, void *argv[])
```

The arguments are a series of pointers passed to the coroutine. 
All arguments are guaranteed to be in scope when the coroutine starts and until the first `neco_` function is called. This allows you an opportunity to validate and/or copy them.

```c
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "neco.h"

void coroutine(int argc, void *argv[]) {

    // All arguments are currently in scope and should be copied before first
    // neco_*() function is called in this coroutine.

    int arg0 = *(int*)argv[0];
    int arg1 = *(int*)argv[1];
    int arg2 = *(int*)argv[2];
    char *arg3 = argv[3];
    char *arg4 = argv[4];

    printf("arg0=%d, arg1=%d, arg2=%d, arg3=%s, arg4=%s\n", 
        arg0, arg1, arg2, arg3, arg4);

    neco_sleep(NECO_SECOND/2);

    // The arguments are no longer in scope and it's unsafe to use the argv
    // variable any further.

    printf("second done\n");
    
}

int neco_main(int argc, char *argv[]) {

    int arg0 = 0;
    int *arg1 = malloc(sizeof(int));
    *arg1 = 1;

    neco_start(coroutine, 5, &arg0, arg1, &(int){2}, NULL, "hello world");
    free(arg1);

    neco_sleep(NECO_SECOND);
    printf("first done\n");

    return 0;
}
```

## Example 5 (Channels)

A [channel](docs/API.md#channels) is a mechanism for communicating between two or more coroutines.

Here we'll create a second coroutine that sends the message 'ping' to the first coroutine.

```c
#include <stdlib.h>
#include <unistd.h>
#include "neco.h"

void coroutine(int argc, void *argv[]) {
    neco_chan *messages = argv[0];
    
    // Send a message of the 'messages' channel. 
    char *msg = "ping";
    neco_chan_send(messages, &msg);

    // This coroutine no longer needs the channel.
    neco_chan_release(messages);
}

int neco_main(int argc, char *argv[]) {

    // Create a new channel that is used to send 'char*' string messages.
    neco_chan *messages;
    neco_chan_make(&messages, sizeof(char*), 0);

    // Start a coroutine that sends messages over the channel. 
    // It's a good idea to use neco_chan_retain on a channel before using it
    // in a new coroutine. This will avoid potential use-after-free bugs.
    neco_chan_retain(messages);
    neco_start(coroutine, 1, messages);

    // Receive the next incoming message. Here we’ll receive the "ping"
    // message we sent above and print it out.
    char *msg = NULL;
    neco_chan_recv(messages, &msg);
    printf("%s\n", msg);
    
    // This coroutine no longer needs the channel.
    neco_chan_release(messages);

    return 0;
}
```

## Example 6 (Generators)

A [generator](docs/API.md#generators) is like channel but is stricly bound to a coroutine and is intended to treat the coroutine like an iterator.

```c
#include <stdio.h>
#include <unistd.h>
#include "neco.h"

void coroutine(int argc, void *argv[]) {
    // Yield each int to the caller, one at a time.
    for (int i = 0; i < 10; i++) {
        neco_gen_yield(&i);
    }
}

int neco_main(int argc, char *argv[]) {
    
    // Create a new generator coroutine that is used to send ints.
    neco_gen *gen;
    neco_gen_start(&gen, sizeof(int), coroutine, 0);

    // Iterate over each int until the generator is closed.
    int i;
    while (neco_gen_next(gen, &i) != NECO_CLOSED) {
        printf("%d\n", i); 
    }

    // This coroutine no longer needs the generator.
    neco_gen_release(gen);
    return 0;
}
```

## Example 7 (Connect to server)

Neco provides [`neco_dial()`](docs/API.md#neco_dial) for easily connecting
to server.

Here we'll performing a (very simple) HTTP request which prints the homepage of
the http://example.com website.

```c
#include <stdio.h>
#include <unistd.h>
#include "neco.h"

int neco_main(int argc, char *argv[]) {
    int fd = neco_dial("tcp", "example.com:80");
    if (fd < 0) {
        printf("neco_dial: %s\n", neco_strerror(fd));
        return 0;
    }
    char req[] = "GET / HTTP/1.1\r\n"
                 "Host: example.com\r\n"
                 "Connection: close\r\n"
                 "\r\n";
    neco_write(fd, req, sizeof(req));
    while (1) {
        char buf[256];
        int n = neco_read(fd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        printf("%.*s", n, buf);
    }
    close(fd);
    return 0;
}
```

## Example 8 (Create a server)

Use [`neco_serve()`](docs/API.md) to quickly bind and listen on an address. 

Here we'll run a tiny webserver at http://127.0.0.1:8080

```c
#include <stdio.h>
#include <unistd.h>
#include "../neco.h"

void request(int argc, void *argv[]) {
    int fd = *(int*)argv[0];
    char req[256];
    int n = neco_read(fd, req, sizeof(req));
    if (n > 0) {
        char res[] = "HTTP/1.0 200 OK\r\n"
                     "Content-Type: text/html\r\n"
                     "Content-Length: 21\r\n"
                     "\r\n"
                     "<h1>Hello Neco!</h1>\n";
        neco_write(fd, res, sizeof(res));
    }
    close(fd);
}

int neco_main(int argc, char *argv[]) {
    int servfd = neco_serve("tcp", "127.0.0.1:8080");
    if (servfd < 0) {
        printf("neco_serve: %s\n", neco_strerror(servfd));
        return 0;
    }
    printf("Serving at http://127.0.0.1:8080\n");
    while (1) {
        int fd = neco_accept(servfd, 0, 0);
        if (servfd < 0) {
            printf("neco_accept: %s\n", neco_strerror(fd));
            continue;
        }
        neco_start(request, 1, &fd);
    }
    return 0;
}
```

## Example 9 (Echo server and client)

Run server with:

```sh
cc neco.c echo-server.c && ./a.out
```

Run client with:

```sh
cc neco.c echo-client.c && ./a.out
```

**echo-server.c**

```c
#include <stdlib.h>
#include <unistd.h>
#include "neco.h"

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
```

**echo-client.c**

```c
#include <stdlib.h>
#include <unistd.h>
#include "neco.h"

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
```

## Example 10 (Suspending and resuming a coroutine)

Any coroutines can suspended itself indefinetly and then be resumed by other
coroutines by using [`neco_suspend()`](docs/API.md#neco_suspend) and 
[`neco_resume()`](docs/API.md#neco_resume).

```c
#include <stdio.h>
#include <unistd.h>
#include "neco.h"

void coroutine(int argc, void *argv[]) {
    printf("Suspending coroutine\n");
    neco_suspend();
    printf("Coroutine resumed\n");
}

int neco_main(int argc, char *argv[]) {
    neco_start(coroutine, 0);
    
    for (int i = 0; i < 3; i++) {
        printf("%d\n", i+1);
        neco_sleep(NECO_SECOND);
    }

    // Resume the suspended. The neco_lastid() returns the identifier for the
    // last coroutine started by the current coroutine.
    neco_resume(neco_lastid());
    return 0;
}
// Output:
// Suspending coroutine
// 1
// 2
// 3
// Coroutine resumed
```

### More examples

You can find more [examples here](examples).

## Platform notes

Linux, Mac, and FreeBSD supports all features.

Windows and WebAssembly support the core coroutine features, but have some key
limitiations, mostly with working with file descriptors and networking.
This is primarly because the Neco event queue works with epoll and kqueue,
which are only available on Linux and Mac/BSD respectively. This means that the
`neco_wait()` (which allows for a coroutine to wait for a file descriptor to be
readable or writeable) is not currently available on those platforms.

Other limitations include:

- Windows only supports amd64.
- Windows and WebAssembly use smaller default stacks of 1MB.
- Windows and WebAssembly do not support guards or gaps.
- Windows and WebAssembly do not support NECO_CSPRNG (Cryptographically secure
  pseudorandom number generator)
- Windows does not support stack unwinding.

Other than that, Neco works great on those platforms.

Any contributions towards making Windows and WebAssembly feature complete are
welcome. 

## The scheduler

Neco uses [sco](https://github.com/tidwall/sco), which is a fair and
deterministic scheduler. This means that no coroutine takes priority over
another and that all concurrent operations will reproduce in an expected order.

### Fast context switching

The coroutine context switching is powered by 
[llco](https://github.com/tidwall/llco) and uses assembly code in most
cases. On my lab machine (AMD Ryzen 9 5950X) a context switch takes about 11
nanoseconds.

### Thread local runtime

There can be no more than one scheduler per thread.

When the first coroutine is started using `neco_start()`, a new Neco
runtime is initialized in the current thread, and each runtime has its own
scheduler. 

Communicating between coroutines that are running in different threads will
require I/O mechanisms that do not block the current schedulers, such as
`pipe()`, `eventfd()` or atomics. 

_Pthread utilties such as `pthread_mutex_t` and `pthread_cond_t` do not work very well in coroutines._

For example, here we'll create two threads, running their own Neco schedulers.
Each using pipes to communicate with the other.

```c
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "neco.h"

void coro1(int argc, void *argv[]) {
    // This coroutine is running in a different scheduler than coro2.
    int rd = *(int*)argv[0];
    int wr = *(int*)argv[1];
    int val;
    neco_read(rd, &val, sizeof(int));
    printf("coro1: %d\n", val);
    neco_write(wr, &(int){ 2 }, sizeof(int));
}

void coro2(int argc, void *argv[]) {
    // This coroutine is running in a different scheduler than coro1.
    int rd = *(int*)argv[0];
    int wr = *(int*)argv[1];
    int val;
    neco_write(wr, &(int){ 1 }, sizeof(int));
    neco_read(rd, &val, sizeof(int));
    printf("coro2: %d\n", val);
}

void *runtime1(void *arg) {
    int *pipefds = arg;
    neco_start(coro1, 2, &pipefds[0], &pipefds[3]);
    return 0;
}

void *runtime2(void *arg) {
    int *pipefds = arg;
    neco_start(coro2, 2, &pipefds[2], &pipefds[1]);
    return 0;
}

int main() {
    int pipefds[4];
    pipe(&pipefds[0]);
    pipe(&pipefds[2]);
    pthread_t th1, th2;
    pthread_create(&th1, 0, runtime1, pipefds);
    pthread_create(&th2, 0, runtime2, pipefds);
    pthread_join(th1, 0);
    pthread_join(th2, 0);
    return 0;
}
```

## License

Source code is available under the MIT [License](LICENSE).
