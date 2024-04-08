<!--- AUTOMATICALLY GENERATED: DO NOT EDIT --->

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


<a name='group___basic_operations'></a>
## Basic operations

Neco provides standard operations for starting a coroutine, sleeping, suspending, resuming, yielding to another coroutine, joining/waiting for child coroutines, and exiting a running coroutine. 



- [neco_start()](#group___basic_operations_1gacfbcf6055ddaa921e490d1126b098c6b)
- [neco_startv()](#group___basic_operations_1ga5b0f6236cd9a5474a64d4bf255e78cc3)
- [neco_yield()](#group___basic_operations_1ga452ce9548546546212f543d520e80d56)
- [neco_sleep()](#group___basic_operations_1gae87a3246f84cf2db5740657b7d154be8)
- [neco_sleep_dl()](#group___basic_operations_1ga59a2c087c1f101f2ae5a4a1453c1ca01)
- [neco_join()](#group___basic_operations_1ga64637448098a3c511e0c0c048c40103e)
- [neco_join_dl()](#group___basic_operations_1gaccb867c094a5e8b4e831c8a27f181773)
- [neco_suspend()](#group___basic_operations_1ga552dc7e3c3f0ee58d4904dac8a4f7321)
- [neco_suspend_dl()](#group___basic_operations_1ga32850f9848273874245218b6b3392471)
- [neco_resume()](#group___basic_operations_1gafd0eb7bf4f11111d42375dd9bf8ede79)
- [neco_exit()](#group___basic_operations_1ga0164114b476cd4d66ec80755c384d4c3)
- [neco_getid()](#group___basic_operations_1gabf1388fdcd0eaf2d299ae3699fa1a69a)
- [neco_lastid()](#group___basic_operations_1ga099df89d4ddd5816d896f4d74131dcbe)
- [neco_starterid()](#group___basic_operations_1ga56c02a14469157b84af23887f084db73)


<a name='group___channels'></a>
## Channels

Channels allow for sending and receiving values between coroutines. By default, sends and receives will block until the other side is ready. This allows the coroutines to synchronize without using locks or condition variables. 



- [neco_chan_make()](#group___channels_1gad7f6dc81bcb94cd4a144daebabfc8c52)
- [neco_chan_retain()](#group___channels_1gab843bd5df882ec0aaa6cbd5083b9b721)
- [neco_chan_release()](#group___channels_1ga2808f4ad91d72f8ce57fa3c0a2d3fafb)
- [neco_chan_send()](#group___channels_1gaddc9839d4aba79e1d7a19c9bdc10a9a5)
- [neco_chan_send_dl()](#group___channels_1ga18583ad54f7e44bf8838ece1501e89be)
- [neco_chan_broadcast()](#group___channels_1ga4c5abf01d047840d3e637e2424fab06d)
- [neco_chan_recv()](#group___channels_1ga2f6bb94175df1adcb02fcff0ba607714)
- [neco_chan_recv_dl()](#group___channels_1ga5f0b70f0982b2e3e32278e158121a565)
- [neco_chan_tryrecv()](#group___channels_1gac6e63d29f8ab1a2ab2fa26e2b5ef4b0e)
- [neco_chan_close()](#group___channels_1ga0bcf858aef2c63b06262ce9613e8d436)
- [neco_chan_select()](#group___channels_1ga4422a483d68b1426026ba7c432647046)
- [neco_chan_select_dl()](#group___channels_1gab3afdfa2446b1a698a251ff4bfe05d07)
- [neco_chan_selectv()](#group___channels_1ga47f4559dca0426dce566a070ce77a324)
- [neco_chan_selectv_dl()](#group___channels_1gad55a1acfaa4035d68047b716a4cc22b4)
- [neco_chan_tryselect()](#group___channels_1ga3eef207727751d43f1b00258ff50003b)
- [neco_chan_tryselectv()](#group___channels_1ga9f29bd7474bbe176ae51a3c6959981c6)
- [neco_chan_case()](#group___channels_1ga9b94a8a90ecd46807add2ee9d45c2325)


<a name='group___generators'></a>
## Generators

A generator is a specialized iterator-bound coroutine that can produce a sequence of values to be iterated over. 



- [neco_gen_start()](#group___generators_1ga76747762182e1e4e09b875c050a78b78)
- [neco_gen_startv()](#group___generators_1ga007f921308ba7e35becc465cc756cae1)
- [neco_gen_retain()](#group___generators_1ga20fc0ad0de7d0665c399e628dd61ae8c)
- [neco_gen_release()](#group___generators_1ga23c2af17111c63566cb2b07a1a34ee84)
- [neco_gen_yield()](#group___generators_1gad7c3391c16f5552560484087cf75d7bc)
- [neco_gen_yield_dl()](#group___generators_1gafcbfd40e4aea60194aa64d33f892da65)
- [neco_gen_next()](#group___generators_1ga34283229abf0393bc05b720239f2c29b)
- [neco_gen_next_dl()](#group___generators_1gaaf4f60adae59dc293bc6ad48b525baf9)
- [neco_gen_close()](#group___generators_1gaf783371ccc65f5f50665a6aa3be60da4)


<a name='group___mutexes'></a>
## Mutexes

A mutex is synchronization mechanism that blocks access to variables by multiple coroutines at once. This enforces exclusive access by a coroutine to a variable or set of variables and helps to avoid data inconsistencies due to race conditions. 



- [neco_mutex_init()](#group___mutexes_1gae779cdf3a6496968d1739478fb942a2f)
- [neco_mutex_lock()](#group___mutexes_1ga5f9a1499741979555215c0d32df28036)
- [neco_mutex_lock_dl()](#group___mutexes_1ga7078809a05ef4865105dc2badcba16c8)
- [neco_mutex_trylock()](#group___mutexes_1gab87dcdc89e9bee98acbb526a0a241470)
- [neco_mutex_unlock()](#group___mutexes_1ga51f3a1467736c2569e3d784c679d906f)
- [neco_mutex_rdlock()](#group___mutexes_1gaf0557f23118f870f726af5ec2be2338c)
- [neco_mutex_rdlock_dl()](#group___mutexes_1gaab33cb2e6ac8a339be65f45d7560e6ea)
- [neco_mutex_tryrdlock()](#group___mutexes_1ga6a19e3ee7407825f7eec426e0b2b07ab)


<a name='group___wait_groups'></a>
## WaitGroups

A WaitGroup waits for a multiple coroutines to finish. The main coroutine calls [neco_waitgroup_add()](#group___wait_groups_1ga6e9fbd0b59edd0e043a3f5513c89e355) to set the number of coroutines to wait for. Then each of the coroutines runs and calls [neco_waitgroup_done()](#group___wait_groups_1gaec7cebb64cc152a3e5d08473b817aa8d) when complete. At the same time, [neco_waitgroup_wait()](#group___wait_groups_1ga50a892457d7275ef71414dc3444d8ca3) can be used to block until all coroutines are completed. 



- [neco_waitgroup_init()](#group___wait_groups_1ga70bb6ff8f8e15578ec43d9a02c463a55)
- [neco_waitgroup_add()](#group___wait_groups_1ga6e9fbd0b59edd0e043a3f5513c89e355)
- [neco_waitgroup_done()](#group___wait_groups_1gaec7cebb64cc152a3e5d08473b817aa8d)
- [neco_waitgroup_wait()](#group___wait_groups_1ga50a892457d7275ef71414dc3444d8ca3)
- [neco_waitgroup_wait_dl()](#group___wait_groups_1gae8bf9aac9f44942100aabe108774e7cf)


<a name='group___cond_var'></a>
## Condition variables

A condition variable is a synchronization mechanism that allows coroutines to suspend execution until some condition is true. 



- [neco_cond_init()](#group___cond_var_1ga7ba9fc6b2cc9da91baa449a88fde1f16)
- [neco_cond_signal()](#group___cond_var_1ga0e0a94263080390d3fed8f3def0bbf25)
- [neco_cond_broadcast()](#group___cond_var_1ga8dacdb4efb8a82d69b43dec8a1eec199)
- [neco_cond_wait()](#group___cond_var_1gaa6c18058d4bbb2274415bfc6a821d04f)
- [neco_cond_wait_dl()](#group___cond_var_1ga6b374b1be4ba2e3d60f5462728196205)


<a name='group___posix'></a>
## Posix wrappers

Functions that work like their Posix counterpart but do not block, allowing for usage in a Neco coroutine. 



- [neco_read()](#group___posix_1ga1869cc07ba78a167b8462a9cef09c0ba)
- [neco_read_dl()](#group___posix_1ga9077362d45d7e4f71dbb9b24a6134e4d)
- [neco_write()](#group___posix_1ga5c8ea872889626c86c57cc3180a673be)
- [neco_write_dl()](#group___posix_1gad1fe0b6ed23aadaca0dbf2031ffbebab)
- [neco_accept()](#group___posix_1ga49ed3b4837ffcd995b054cfb4bb178b1)
- [neco_accept_dl()](#group___posix_1ga963e70ab9a894c49624dc6b03d7494a4)
- [neco_connect()](#group___posix_1ga16815a8a7a51d95281f1d70555168383)
- [neco_connect_dl()](#group___posix_1ga50ccd758a7423fa97b717d595efe9a86)
- [neco_getaddrinfo()](#group___posix_1gae80194a21ffdc035a7054276b537a8c3)
- [neco_getaddrinfo_dl()](#group___posix_1ga8a828e8a4e27f41aa5dfd966a3e6702d)


<a name='group___posix2'></a>
## File descriptor helpers

Functions for working with file descriptors. 



- [neco_setnonblock()](#group___posix2_1ga6f612b7e64d5a9ec7ff607315cc8ec3a)
- [neco_wait()](#group___posix2_1ga99dc783673fbf7e3e12479d59ad7ee4c)
- [neco_wait_dl()](#group___posix2_1ga8392f97cbef774c6d044c75b393fbc03)


<a name='group___networking'></a>
## Networking utilities

- [neco_serve()](#group___networking_1ga62b996c86bb80c1cab2092d82c6ea53c)
- [neco_serve_dl()](#group___networking_1gae0fe390fce4ecf3f80cbb9668020a59c)
- [neco_dial()](#group___networking_1ga327b79d78328e50dd1a98fbf444c9d0c)
- [neco_dial_dl()](#group___networking_1ga02c848d20aeee63790bb2def69fea4c9)


<a name='group___cancelation'></a>
## Cancelation

- [neco_cancel()](#group___cancelation_1gae6426af020deb390047106686d5da031)
- [neco_cancel_dl()](#group___cancelation_1ga85143c030d2055da855a9451980585af)
- [neco_setcanceltype()](#group___cancelation_1gad6dfa0851bbb7317821b27f16ef0553d)
- [neco_setcancelstate()](#group___cancelation_1ga3cdc3d0b6f48a805fafa94247122cb75)


<a name='group___random'></a>
## Random number generator

- [neco_rand_setseed()](#group___random_1gaefd0991a1cdf430f22899fd34bdba892)
- [neco_rand()](#group___random_1ga4567eff3299e3b00e829b82ca657d8cd)
- [neco_rand_dl()](#group___random_1gaaad47a533a78ff549be6d41339846493)


<a name='group___signals'></a>
## Signals

Allows for signals, such as SIGINT (Ctrl-C), to be intercepted or ignored. 



- [neco_signal_watch()](#group___signals_1ga4848017f351283eed5e432a9af9ddd7b)
- [neco_signal_wait()](#group___signals_1gaa34972437b6b9e85ba0efec6cd388516)
- [neco_signal_wait_dl()](#group___signals_1gacd8061a747344fbde13cbdfcc99efd8f)
- [neco_signal_unwatch()](#group___signals_1ga58c262e5ded1ec30c64618959453517a)


<a name='group___worker'></a>
## Background worker

Run arbritary code in a background worker thread 



- [neco_work()](#group___worker_1ga6a8c55b440330f4a1f7b654787d96946)


<a name='group___stats'></a>
## Stats and information

- [neco_getstats()](#group___stats_1ga9728fa5ef28c4d88a951faaf7d26a506)
- [neco_is_main_thread()](#group___stats_1ga44d9b10110f57fb7491b236d3ec7e731)
- [neco_switch_method()](#group___stats_1ga60ee198bb5309531aac43b8276ac7a64)


<a name='group___global_funcs'></a>
## Global environment

- [neco_env_setallocator()](#group___global_funcs_1gae6a60529f176d23f51f10a3906beb568)
- [neco_env_setpaniconerror()](#group___global_funcs_1ga10db728074cebc8d35cfcec01bb1dc97)
- [neco_env_setcanceltype()](#group___global_funcs_1gacab37dcac0bef3c4a29f523ea32b8df3)
- [neco_env_setcancelstate()](#group___global_funcs_1ga09e93872d1245a3bcba13b3b45b43618)


<a name='group___time'></a>
## Time

Functions for working with time.

The following defines are available for convenience.

```c
#define NECO_NANOSECOND  INT64_C(1)
#define NECO_MICROSECOND INT64_C(1000)
#define NECO_MILLISECOND INT64_C(1000000)
#define NECO_SECOND      INT64_C(1000000000)
#define NECO_MINUTE      INT64_C(60000000000)
#define NECO_HOUR        INT64_C(3600000000000)
```
 



- [neco_now()](#group___time_1gad969c647e4b3b661eab3f7f5b5372d38)


<a name='group___error_funcs'></a>
## Error handling

Functions for working with [Neco errors](./API.md#neco-errors). 



- [neco_strerror()](#group___error_funcs_1ga053c32e9432aef83bda474e41fd7b2d4)
- [neco_lasterr()](#group___error_funcs_1ga82ade5f26218caa8e0b4883575a683bd)
- [neco_gai_lasterr()](#group___error_funcs_1ga6b2d592c3044f9756073daabfe25b470)
- [neco_panic()](#group___error_funcs_1gae611410883c5d2cfa24def14f3a2cc38)


<a name='group___streams'></a>
## Streams and Buffered I/O

Create a Neco stream from a file descriptor using [neco_stream_make()](#group___streams_1ga74885d1c39c7c8442fc9e82b8411243e) or a buffered stream using [neco_stream_make_buffered()](#group___streams_1ga70276b8dd3bfe1a3e715ffd4c8486620). 



- [neco_stream_make()](#group___streams_1ga74885d1c39c7c8442fc9e82b8411243e)
- [neco_stream_make_buffered()](#group___streams_1ga70276b8dd3bfe1a3e715ffd4c8486620)
- [neco_stream_close()](#group___streams_1ga526b51cbef51b1e36723ca718368f215)
- [neco_stream_close_dl()](#group___streams_1gaaec08f5c1c1d3d630fa2faba75b6a6fb)
- [neco_stream_read()](#group___streams_1gae6c836fd60b4a2225b5ccdecbe529c38)
- [neco_stream_read_dl()](#group___streams_1gad8f1e0ac6eae64fc4d8cb995b17a0bb6)
- [neco_stream_write()](#group___streams_1gaa3f1c676605a7441c527f1a798cedacf)
- [neco_stream_write_dl()](#group___streams_1ga09f9bdd323f2ba29669d71968c7fd3cd)
- [neco_stream_readfull()](#group___streams_1ga1b3b8a4e10113768f22c6cb5b7992ce4)
- [neco_stream_readfull_dl()](#group___streams_1ga1f08421b4cac63e930046c328c6f3c6a)
- [neco_stream_read_byte()](#group___streams_1gaf8dc61bc7e0b9c6c105c8ab51693ee3d)
- [neco_stream_read_byte_dl()](#group___streams_1ga3096f4f082cee9cddcfaad4ca3694f7a)
- [neco_stream_unread_byte()](#group___streams_1ga1ec67ae2dfa4cae9a26c536e3c44c4f6)
- [neco_stream_flush()](#group___streams_1ga1c847c02240767c06daf832eb85022e2)
- [neco_stream_flush_dl()](#group___streams_1gac4818a4bc2497ca46db221a1fea96746)
- [neco_stream_buffered_read_size()](#group___streams_1gaf8d0174f3c7627ae4b0635d493528616)
- [neco_stream_buffered_write_size()](#group___streams_1gae870778b3faddfcd71de62880adb6172)

<a name='structneco__stats'></a>
## neco_stats
```c
struct neco_stats {
    size_t coroutines;   // Number of active coroutines. 
    size_t sleepers;     // Number of sleeping coroutines. 
    size_t evwaiters;    // Number of coroutines waiting on I/O events. 
    size_t sigwaiters;  
    size_t senders;     
    size_t receivers;   
    size_t locked;      
    size_t waitgroupers;
    size_t condwaiters; 
    size_t suspended;   
    size_t workers;      // Number of background worker threads. 
};
```

<a name='structneco__mutex'></a>
## neco_mutex
```c
struct neco_mutex {
    int64_t rtid;       
    bool locked;        
    int rlocked;        
    struct colist queue;
    char _;             
};
```

<a name='structneco__waitgroup'></a>
## neco_waitgroup
```c
struct neco_waitgroup {
    int64_t rtid;       
    int count;          
    struct colist queue;
    char _;             
};
```

<a name='structneco__cond'></a>
## neco_cond
```c
struct neco_cond {
    int64_t rtid;       
    struct colist queue;
    char _;             
};
```

<a name='structneco__chan'></a>
## neco_chan
```c
struct neco_chan;
```

<a name='structneco__stream'></a>
## neco_stream
```c
struct neco_stream;
```

<a name='structneco__gen'></a>
## neco_gen
```c
struct neco_gen;
```

<a name='group___basic_operations_1gacfbcf6055ddaa921e490d1126b098c6b'></a>
## neco_start()
```c
int neco_start(void(*coroutine)(int argc, void *argv[]), int argc,...);
```
Starts a new coroutine.

If this is the first coroutine started for the program (or thread) then this will also create a neco runtime scheduler which blocks until the provided coroutine and all of its subsequent child coroutines finish.

**Example**

```c
// Here we'll start a coroutine that prints "hello world".

void coroutine(int argc, void *argv[]) {
    char *msg = argv[0];
    printf("%s\n", msg);
}

neco_start(coroutine, 1, "hello world");
```




**Parameters**

- **coroutine**: The coroutine that will soon run
- **argc**: Number of arguments
- **...**: Arguments passed to the coroutine



**Return**

- NECO_OK Success
- NECO_NOMEM The system lacked the necessary resources
- NECO_INVAL An invalid parameter was provided



<a name='group___basic_operations_1ga5b0f6236cd9a5474a64d4bf255e78cc3'></a>
## neco_startv()
```c
int neco_startv(void(*coroutine)(int argc, void *argv[]), int argc, void *argv[]);
```
Starts a new coroutine using an array for arguments. 

**See also**

- [neco_start](#group___basic_operations_1gacfbcf6055ddaa921e490d1126b098c6b)



<a name='group___basic_operations_1ga452ce9548546546212f543d520e80d56'></a>
## neco_yield()
```c
int neco_yield();
```
Cause the calling coroutine to relinquish the CPU. The coroutine is moved to the end of the queue. 

**Return**

- NECO_OK Success
- NECO_PERM Operation called outside of a coroutine



<a name='group___basic_operations_1gae87a3246f84cf2db5740657b7d154be8'></a>
## neco_sleep()
```c
int neco_sleep(int64_t nanosecs);
```
Causes the calling coroutine to sleep until the number of specified nanoseconds have elapsed. 

**Parameters**

- **nanosecs**: duration nanoseconds



**Return**

- NECO_OK Coroutine slept until nanosecs elapsed
- NECO_TIMEDOUT nanosecs is a negative number
- NECO_CANCELED Operation canceled
- NECO_PERM Operation called outside of a coroutine


**See also**

- [neco_sleep_dl()](#group___basic_operations_1ga59a2c087c1f101f2ae5a4a1453c1ca01)



<a name='group___basic_operations_1ga59a2c087c1f101f2ae5a4a1453c1ca01'></a>
## neco_sleep_dl()
```c
int neco_sleep_dl(int64_t deadline);
```
Same as [neco_sleep()](#group___basic_operations_1gae87a3246f84cf2db5740657b7d154be8) but with a deadline parameter. 


<a name='group___basic_operations_1ga64637448098a3c511e0c0c048c40103e'></a>
## neco_join()
```c
int neco_join(int64_t id);
```
Wait for a coroutine to terminate. If that coroutine has already terminated or is not found, then this operation returns immediately.

**Example**

```c
// Start a new coroutine
neco_start(coroutine, 0);

// Get the identifier of the new coroutine.
int64_t id = neco_lastid();

// Wait until the coroutine has terminated.
neco_join(id);
```




**Parameters**

- **id**: Coroutine identifier



**Return**

- NECO_OK Success
- NECO_CANCELED Operation canceled
- NECO_PERM Operation called outside of a coroutine


**See also**

- [neco_join_dl()](#group___basic_operations_1gaccb867c094a5e8b4e831c8a27f181773)



<a name='group___basic_operations_1gaccb867c094a5e8b4e831c8a27f181773'></a>
## neco_join_dl()
```c
int neco_join_dl(int64_t id, int64_t deadline);
```
Same as [neco_join()](#group___basic_operations_1ga64637448098a3c511e0c0c048c40103e) but with a deadline parameter. 


<a name='group___basic_operations_1ga552dc7e3c3f0ee58d4904dac8a4f7321'></a>
## neco_suspend()
```c
int neco_suspend();
```
Suspend the current coroutine. 

**Return**

- NECO_OK Success
- NECO_PERM Operation called outside of a coroutine
- NECO_CANCELED Operation canceled


**See also**

- [neco_resume](#group___basic_operations_1gafd0eb7bf4f11111d42375dd9bf8ede79)
- [neco_suspend_dl](#group___basic_operations_1ga32850f9848273874245218b6b3392471)



<a name='group___basic_operations_1ga32850f9848273874245218b6b3392471'></a>
## neco_suspend_dl()
```c
int neco_suspend_dl(int64_t deadline);
```
Same as [neco_suspend()](#group___basic_operations_1ga552dc7e3c3f0ee58d4904dac8a4f7321) but with a deadline parameter. 


<a name='group___basic_operations_1gafd0eb7bf4f11111d42375dd9bf8ede79'></a>
## neco_resume()
```c
int neco_resume(int64_t id);
```
Resume a suspended roroutine 

**Return**

- NECO_OK Success
- NECO_PERM Operation called outside of a coroutine
- NECO_NOTFOUND Coroutine not found
- NECO_NOTSUSPENDED Coroutine not suspended


**See also**

- [neco_suspend](#group___basic_operations_1ga552dc7e3c3f0ee58d4904dac8a4f7321)



<a name='group___basic_operations_1ga0164114b476cd4d66ec80755c384d4c3'></a>
## neco_exit()
```c
void neco_exit();
```
Terminate the current coroutine.

Any clean-up handlers established by [neco_cleanup_push()](#group___cancelation_1gae48d7d3dd6218995e4d058b98aad5c9f) that have not yet been popped, are popped (in the reverse of the order in which they were pushed) and executed.

Calling this from outside of a coroutine context does nothing and will be treated effectivley as a no-op. 


<a name='group___basic_operations_1gabf1388fdcd0eaf2d299ae3699fa1a69a'></a>
## neco_getid()
```c
int64_t neco_getid();
```
Returns the identifier for the currently running coroutine.

This value is guaranteed to be unique for the duration of the program. 

**Return**

- The coroutine identifier
- NECO_PERM Operation called outside of a coroutine



<a name='group___basic_operations_1ga099df89d4ddd5816d896f4d74131dcbe'></a>
## neco_lastid()
```c
int64_t neco_lastid();
```
Returns the identifier for the coroutine started by the current coroutine.

For example, here a coroutine is started and its identifer is then retreived.

```c
neco_start(coroutine, 0);
int64_t id = neco_lastid();
```




**Return**

- A coroutine identifier, or zero if the current coroutine has not yet started any coroutines.
- NECO_PERM Operation called outside of a coroutine



<a name='group___basic_operations_1ga56c02a14469157b84af23887f084db73'></a>
## neco_starterid()
```c
int64_t neco_starterid();
```
Get the identifier for the coroutine that started the current coroutine.

```c
void child_coroutine(int argc, void *argv[]) {
    int parent_id = neco_starterid();
    // The parent_id is equal as the neco_getid() from the parent_coroutine
    // below. 
}

void parent_coroutine(int argc, void *argv[]) {
   int id = neco_getid();
   neco_start(child_coroutine, 0);
}
```
 

**Return**

- A coroutine identifier, or zero if the coroutine is the first coroutine started.



<a name='group___channels_1gad7f6dc81bcb94cd4a144daebabfc8c52'></a>
## neco_chan_make()
```c
int neco_chan_make(neco_chan **chan, size_t data_size, size_t capacity);
```
Creates a new channel for sharing messages with other coroutines.

**Example**

```c
void coroutine(int argc, void *argv[]) {
    neco_chan *ch = argv[0];

    // Send a message
    neco_chan_send(ch, &(int){ 1 });

    // Release the channel
    neco_chan_release(ch);
}

int neco_start(int argc, char *argv[]) {
    neco_chan *ch;
    neco_chan_make(&ch, sizeof(int), 0);
    
    // Retain a reference of the channel and provide it to a newly started
    // coroutine. 
    neco_chan_retain(ch);
    neco_start(coroutine, 1, ch);
    
    // Receive a message
    int msg;
    neco_chan_recv(ch, &msg);
    printf("%d\n", msg);      // prints '1'
    
    // Always release the channel when you are done
    neco_chan_release(ch);
}
```




**Parameters**

- **chan**: Channel
- **data_size**: Data size of messages
- **capacity**: Buffer capacity



**Return**

- NECO_OK Success
- NECO_NOMEM The system lacked the necessary resources
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine


**Note**

- The caller is responsible for freeing with [neco_chan_release()](#group___channels_1ga2808f4ad91d72f8ce57fa3c0a2d3fafb)
- data_size and capacity cannot be greater than INT_MAX



<a name='group___channels_1gab843bd5df882ec0aaa6cbd5083b9b721'></a>
## neco_chan_retain()
```c
int neco_chan_retain(neco_chan *chan);
```
Retain a reference of the channel so it can be shared with other coroutines.

This is needed for avoiding use-after-free bugs.

See [neco_chan_make()](#group___channels_1gad7f6dc81bcb94cd4a144daebabfc8c52) for an example.



**Parameters**

- **chan**: The channel



**Return**

- NECO_OK Success
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine


**Note**

- The caller is responsible for releasing the reference with [neco_chan_release()](#group___channels_1ga2808f4ad91d72f8ce57fa3c0a2d3fafb)


**See also**

- [Channels](#group___channels)
- [neco_chan_release()](#group___channels_1ga2808f4ad91d72f8ce57fa3c0a2d3fafb)



<a name='group___channels_1ga2808f4ad91d72f8ce57fa3c0a2d3fafb'></a>
## neco_chan_release()
```c
int neco_chan_release(neco_chan *chan);
```
Release a reference to a channel

See [neco_chan_make()](#group___channels_1gad7f6dc81bcb94cd4a144daebabfc8c52) for an example.



**Parameters**

- **chan**: The channel



**Return**

- NECO_OK Success
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine


**See also**

- [Channels](#group___channels)
- [neco_chan_retain()](#group___channels_1gab843bd5df882ec0aaa6cbd5083b9b721)



<a name='group___channels_1gaddc9839d4aba79e1d7a19c9bdc10a9a5'></a>
## neco_chan_send()
```c
int neco_chan_send(neco_chan *chan, void *data);
```
Send a message

See [neco_chan_make()](#group___channels_1gad7f6dc81bcb94cd4a144daebabfc8c52) for an example.



**Return**

- NECO_OK Success
- NECO_PERM Operation called outside of a coroutine
- NECO_INVAL An invalid parameter was provided
- NECO_CANCELED Operation canceled
- NECO_CLOSED Channel closed


**See also**

- [Channels](#group___channels)
- [neco_chan_recv()](#group___channels_1ga2f6bb94175df1adcb02fcff0ba607714)



<a name='group___channels_1ga18583ad54f7e44bf8838ece1501e89be'></a>
## neco_chan_send_dl()
```c
int neco_chan_send_dl(neco_chan *chan, void *data, int64_t deadline);
```
Same as [neco_chan_send()](#group___channels_1gaddc9839d4aba79e1d7a19c9bdc10a9a5) but with a deadline parameter. 


<a name='group___channels_1ga4c5abf01d047840d3e637e2424fab06d'></a>
## neco_chan_broadcast()
```c
int neco_chan_broadcast(neco_chan *chan, void *data);
```
Sends message to all receiving channels. 

**Return**

- The number of channels that received the message
- NECO_PERM Operation called outside of a coroutine
- NECO_INVAL An invalid parameter was provided
- NECO_CLOSED Channel closed


**Note**

- This operation cannot be canceled and does not timeout


**See also**

- [Channels](#group___channels)
- [neco_chan_recv()](#group___channels_1ga2f6bb94175df1adcb02fcff0ba607714)



<a name='group___channels_1ga2f6bb94175df1adcb02fcff0ba607714'></a>
## neco_chan_recv()
```c
int neco_chan_recv(neco_chan *chan, void *data);
```
Receive a message

See [neco_chan_make()](#group___channels_1gad7f6dc81bcb94cd4a144daebabfc8c52) for an example.



**Parameters**

- **chan**: channel
- **data**: data pointer



**Return**

- NECO_OK Success
- NECO_PERM Operation called outside of a coroutine
- NECO_INVAL An invalid parameter was provided
- NECO_CANCELED Operation canceled
- NECO_CLOSED Channel closed


**See also**

- [Channels](#group___channels)
- [neco_chan_send()](#group___channels_1gaddc9839d4aba79e1d7a19c9bdc10a9a5)



<a name='group___channels_1ga5f0b70f0982b2e3e32278e158121a565'></a>
## neco_chan_recv_dl()
```c
int neco_chan_recv_dl(neco_chan *chan, void *data, int64_t deadline);
```
Same as [neco_chan_recv()](#group___channels_1ga2f6bb94175df1adcb02fcff0ba607714) but with a deadline parameter. 


<a name='group___channels_1gac6e63d29f8ab1a2ab2fa26e2b5ef4b0e'></a>
## neco_chan_tryrecv()
```c
int neco_chan_tryrecv(neco_chan *chan, void *data);
```
Receive a message, but do not wait if the message is not available. 

**Parameters**

- **chan**: channel
- **data**: data pointer



**Return**

- NECO_OK Success
- NECO_EMPTY No message available
- NECO_CLOSED Channel closed
- NECO_PERM Operation called outside of a coroutine
- NECO_INVAL An invalid parameter was provided


**See also**

- [Channels](#group___channels)
- [neco_chan_recv()](#group___channels_1ga2f6bb94175df1adcb02fcff0ba607714)



<a name='group___channels_1ga0bcf858aef2c63b06262ce9613e8d436'></a>
## neco_chan_close()
```c
int neco_chan_close(neco_chan *chan);
```
Close a channel for sending. 

**Parameters**

- **chan**: channel



**Return**

- NECO_OK Success
- NECO_PERM Operation called outside of a coroutine
- NECO_INVAL An invalid parameter was provided
- NECO_CLOSED Channel already closed



<a name='group___channels_1ga4422a483d68b1426026ba7c432647046'></a>
## neco_chan_select()
```c
int neco_chan_select(int nchans,...);
```
Wait on multiple channel operations at the same time.

**Example**

```c
// Let's say we have two channels 'c1' and 'c2' that both transmit 'char *'
// messages.

// Use neco_chan_select() to wait on both channels.

char *msg;
int idx = neco_chan_select(2, c1, c2);
switch (idx) {
case 0:
    neco_chan_case(c1, &msg);
    break;
case 1:
    neco_chan_case(c2, &msg);
    break;
default:
    // Error occured. The return value 'idx' is the error
}

printf("%s\n", msg);
```




**Parameters**

- **nchans**: Number of channels
- **...**: The channels



**Return**

- The index of channel with an available message
- NECO_PERM Operation called outside of a coroutine
- NECO_INVAL An invalid parameter was provided
- NECO_NOMEM The system lacked the necessary resources
- NECO_CANCELED Operation canceled


**See also**

- [Channels](#group___channels)



<a name='group___channels_1gab3afdfa2446b1a698a251ff4bfe05d07'></a>
## neco_chan_select_dl()
```c
int neco_chan_select_dl(int64_t deadline, int nchans,...);
```
Same as [neco_chan_select()](#group___channels_1ga4422a483d68b1426026ba7c432647046) but with a deadline parameter. 


<a name='group___channels_1ga47f4559dca0426dce566a070ce77a324'></a>
## neco_chan_selectv()
```c
int neco_chan_selectv(int nchans, neco_chan *chans[]);
```
Same as [neco_chan_select()](#group___channels_1ga4422a483d68b1426026ba7c432647046) but using an array for arguments. 


<a name='group___channels_1gad55a1acfaa4035d68047b716a4cc22b4'></a>
## neco_chan_selectv_dl()
```c
int neco_chan_selectv_dl(int nchans, neco_chan *chans[], int64_t deadline);
```
Same as [neco_chan_selectv()](#group___channels_1ga47f4559dca0426dce566a070ce77a324) but with a deadline parameter. 


<a name='group___channels_1ga3eef207727751d43f1b00258ff50003b'></a>
## neco_chan_tryselect()
```c
int neco_chan_tryselect(int nchans,...);
```
Same as [neco_chan_select()](#group___channels_1ga4422a483d68b1426026ba7c432647046) but does not wait if a message is not available 

**Return**

- NECO_EMPTY No message available


**See also**

- [Channels](#group___channels)
- [neco_chan_select()](#group___channels_1ga4422a483d68b1426026ba7c432647046)



<a name='group___channels_1ga9f29bd7474bbe176ae51a3c6959981c6'></a>
## neco_chan_tryselectv()
```c
int neco_chan_tryselectv(int nchans, neco_chan *chans[]);
```
Same as [neco_chan_tryselect()](#group___channels_1ga3eef207727751d43f1b00258ff50003b) but uses an array for arguments. 


<a name='group___channels_1ga9b94a8a90ecd46807add2ee9d45c2325'></a>
## neco_chan_case()
```c
int neco_chan_case(neco_chan *chan, void *data);
```
Receive the message after a successful [neco_chan_select()](#group___channels_1ga4422a483d68b1426026ba7c432647046). See [neco_chan_select()](#group___channels_1ga4422a483d68b1426026ba7c432647046) for an example. 

**Parameters**

- **chan**: The channel
- **data**: The data



**Return**

- NECO_OK Success
- NECO_PERM Operation called outside of a coroutine
- NECO_INVAL An invalid parameter was provided
- NECO_CLOSED Channel closed



<a name='group___generators_1ga76747762182e1e4e09b875c050a78b78'></a>
## neco_gen_start()
```c
int neco_gen_start(neco_gen **gen, size_t data_size, void(*coroutine)(int argc, void *argv[]), int argc,...);
```
Start a generator coroutine

**Example**

```c
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




**Parameters**

- **gen**: Generator object
- **data_size**: Data size of messages
- **coroutine**: Generator coroutine
- **argc**: Number of arguments



**Return**

- NECO_OK Success
- NECO_NOMEM The system lacked the necessary resources
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine


**Note**

- The caller is responsible for freeing the generator object with with [neco_gen_release()](#group___generators_1ga23c2af17111c63566cb2b07a1a34ee84).



<a name='group___generators_1ga007f921308ba7e35becc465cc756cae1'></a>
## neco_gen_startv()
```c
int neco_gen_startv(neco_gen **gen, size_t data_size, void(*coroutine)(int argc, void *argv[]), int argc, void *argv[]);
```
Same as [neco_gen_start()](#group___generators_1ga76747762182e1e4e09b875c050a78b78) but using an array for arguments. 


<a name='group___generators_1ga20fc0ad0de7d0665c399e628dd61ae8c'></a>
## neco_gen_retain()
```c
int neco_gen_retain(neco_gen *gen);
```
Retain a reference of the generator so it can be shared with other coroutines.

This is needed for avoiding use-after-free bugs.

See [neco_gen_start()](#group___generators_1ga76747762182e1e4e09b875c050a78b78) for an example.



**Parameters**

- **gen**: The generator



**Return**

- NECO_OK Success
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine


**Note**

- The caller is responsible for releasing the reference with [neco_gen_release()](#group___generators_1ga23c2af17111c63566cb2b07a1a34ee84)


**See also**

- [Generators](#group___generators)
- [neco_gen_release()](#group___generators_1ga23c2af17111c63566cb2b07a1a34ee84)



<a name='group___generators_1ga23c2af17111c63566cb2b07a1a34ee84'></a>
## neco_gen_release()
```c
int neco_gen_release(neco_gen *gen);
```
Release a reference to a generator

See [neco_gen_start()](#group___generators_1ga76747762182e1e4e09b875c050a78b78) for an example.



**Parameters**

- **gen**: The generator



**Return**

- NECO_OK Success
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine


**See also**

- [Generators](#group___generators)
- [neco_gen_retain()](#group___generators_1ga20fc0ad0de7d0665c399e628dd61ae8c)



<a name='group___generators_1gad7c3391c16f5552560484087cf75d7bc'></a>
## neco_gen_yield()
```c
int neco_gen_yield(void *data);
```
Send a value to the generator for the next iteration.

See [neco_gen_start()](#group___generators_1ga76747762182e1e4e09b875c050a78b78) for an example. 


<a name='group___generators_1gafcbfd40e4aea60194aa64d33f892da65'></a>
## neco_gen_yield_dl()
```c
int neco_gen_yield_dl(void *data, int64_t deadline);
```
Same as [neco_gen_yield()](#group___generators_1gad7c3391c16f5552560484087cf75d7bc) but with a deadline parameter. 


<a name='group___generators_1ga34283229abf0393bc05b720239f2c29b'></a>
## neco_gen_next()
```c
int neco_gen_next(neco_gen *gen, void *data);
```
Receive the next value from a generator.

See [neco_gen_start()](#group___generators_1ga76747762182e1e4e09b875c050a78b78) for an example. 


<a name='group___generators_1gaaf4f60adae59dc293bc6ad48b525baf9'></a>
## neco_gen_next_dl()
```c
int neco_gen_next_dl(neco_gen *gen, void *data, int64_t deadline);
```
Same as [neco_gen_next()](#group___generators_1ga34283229abf0393bc05b720239f2c29b) but with a deadline parameter. 


<a name='group___generators_1gaf783371ccc65f5f50665a6aa3be60da4'></a>
## neco_gen_close()
```c
int neco_gen_close(neco_gen *gen);
```
Close the generator. 

**Parameters**

- **gen**: Generator



**Return**

- NECO_OK Success



<a name='group___mutexes_1gae779cdf3a6496968d1739478fb942a2f'></a>
## neco_mutex_init()
```c
int neco_mutex_init(neco_mutex *mutex);
```

<a name='group___mutexes_1ga5f9a1499741979555215c0d32df28036'></a>
## neco_mutex_lock()
```c
int neco_mutex_lock(neco_mutex *mutex);
```

<a name='group___mutexes_1ga7078809a05ef4865105dc2badcba16c8'></a>
## neco_mutex_lock_dl()
```c
int neco_mutex_lock_dl(neco_mutex *mutex, int64_t deadline);
```

<a name='group___mutexes_1gab87dcdc89e9bee98acbb526a0a241470'></a>
## neco_mutex_trylock()
```c
int neco_mutex_trylock(neco_mutex *mutex);
```

<a name='group___mutexes_1ga51f3a1467736c2569e3d784c679d906f'></a>
## neco_mutex_unlock()
```c
int neco_mutex_unlock(neco_mutex *mutex);
```

<a name='group___mutexes_1gaf0557f23118f870f726af5ec2be2338c'></a>
## neco_mutex_rdlock()
```c
int neco_mutex_rdlock(neco_mutex *mutex);
```

<a name='group___mutexes_1gaab33cb2e6ac8a339be65f45d7560e6ea'></a>
## neco_mutex_rdlock_dl()
```c
int neco_mutex_rdlock_dl(neco_mutex *mutex, int64_t deadline);
```

<a name='group___mutexes_1ga6a19e3ee7407825f7eec426e0b2b07ab'></a>
## neco_mutex_tryrdlock()
```c
int neco_mutex_tryrdlock(neco_mutex *mutex);
```

<a name='group___wait_groups_1ga70bb6ff8f8e15578ec43d9a02c463a55'></a>
## neco_waitgroup_init()
```c
int neco_waitgroup_init(neco_waitgroup *waitgroup);
```

<a name='group___wait_groups_1ga6e9fbd0b59edd0e043a3f5513c89e355'></a>
## neco_waitgroup_add()
```c
int neco_waitgroup_add(neco_waitgroup *waitgroup, int delta);
```

<a name='group___wait_groups_1gaec7cebb64cc152a3e5d08473b817aa8d'></a>
## neco_waitgroup_done()
```c
int neco_waitgroup_done(neco_waitgroup *waitgroup);
```

<a name='group___wait_groups_1ga50a892457d7275ef71414dc3444d8ca3'></a>
## neco_waitgroup_wait()
```c
int neco_waitgroup_wait(neco_waitgroup *waitgroup);
```

<a name='group___wait_groups_1gae8bf9aac9f44942100aabe108774e7cf'></a>
## neco_waitgroup_wait_dl()
```c
int neco_waitgroup_wait_dl(neco_waitgroup *waitgroup, int64_t deadline);
```

<a name='group___cond_var_1ga7ba9fc6b2cc9da91baa449a88fde1f16'></a>
## neco_cond_init()
```c
int neco_cond_init(neco_cond *cond);
```

<a name='group___cond_var_1ga0e0a94263080390d3fed8f3def0bbf25'></a>
## neco_cond_signal()
```c
int neco_cond_signal(neco_cond *cond);
```

<a name='group___cond_var_1ga8dacdb4efb8a82d69b43dec8a1eec199'></a>
## neco_cond_broadcast()
```c
int neco_cond_broadcast(neco_cond *cond);
```

<a name='group___cond_var_1gaa6c18058d4bbb2274415bfc6a821d04f'></a>
## neco_cond_wait()
```c
int neco_cond_wait(neco_cond *cond, neco_mutex *mutex);
```

<a name='group___cond_var_1ga6b374b1be4ba2e3d60f5462728196205'></a>
## neco_cond_wait_dl()
```c
int neco_cond_wait_dl(neco_cond *cond, neco_mutex *mutex, int64_t deadline);
```

<a name='group___posix_1ga1869cc07ba78a167b8462a9cef09c0ba'></a>
## neco_read()
```c
ssize_t neco_read(int fd, void *data, size_t nbytes);
```
Read from a file descriptor.

This operation attempts to read up to count from file descriptor fd into the buffer starting at buf.

This is a Posix wrapper function for the purpose of running in a Neco coroutine. It's expected that the provided file descriptor is in non-blocking state.



**Return**

- On success, the number of bytes read is returned (zero indicates end of file)
- On error, value -1 (NECO_ERROR) is returned, and errno is set to indicate the error.


**See also**

- [Posix wrappers](#group___posix)
- [neco_setnonblock()](#group___posix2_1ga6f612b7e64d5a9ec7ff607315cc8ec3a)
- [https://www.man7.org/linux/man-pages/man2/read.2.html](https://www.man7.org/linux/man-pages/man2/read.2.html)



<a name='group___posix_1ga9077362d45d7e4f71dbb9b24a6134e4d'></a>
## neco_read_dl()
```c
ssize_t neco_read_dl(int fd, void *data, size_t nbytes, int64_t deadline);
```
Same as [neco_read()](#group___posix_1ga1869cc07ba78a167b8462a9cef09c0ba) but with a deadline parameter. 


<a name='group___posix_1ga5c8ea872889626c86c57cc3180a673be'></a>
## neco_write()
```c
ssize_t neco_write(int fd, const void *data, size_t nbytes);
```
Write to a file descriptor.

This operation attempts to write all bytes in the buffer starting at buf to the file referred to by the file descriptor fd.

This is a Posix wrapper function for the purpose of running in a Neco coroutine. It's expected that the provided file descriptor is in non-blocking state.

One difference from the Posix version is that this function will attempt to write *all* bytes in buffer. The programmer, at their discretion, may considered it as an error when fewer than count is returned. If so, the [neco_lasterr()](#group___error_funcs_1ga82ade5f26218caa8e0b4883575a683bd) will return the NECO_PARTIALWRITE.



**Return**

- On success, the number of bytes written is returned.
- On error, value -1 (NECO_ERROR) is returned, and errno is set to indicate the error.


**See also**

- [Posix wrappers](#group___posix)
- [neco_setnonblock()](#group___posix2_1ga6f612b7e64d5a9ec7ff607315cc8ec3a)
- [https://www.man7.org/linux/man-pages/man2/write.2.html](https://www.man7.org/linux/man-pages/man2/write.2.html)



<a name='group___posix_1gad1fe0b6ed23aadaca0dbf2031ffbebab'></a>
## neco_write_dl()
```c
ssize_t neco_write_dl(int fd, const void *data, size_t nbytes, int64_t deadline);
```
Same as [neco_write()](#group___posix_1ga5c8ea872889626c86c57cc3180a673be) but with a deadline parameter. 


<a name='group___posix_1ga49ed3b4837ffcd995b054cfb4bb178b1'></a>
## neco_accept()
```c
int neco_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```
Accept a connection on a socket.

While in a coroutine, this function should be used instead of the standard accept() to avoid blocking other coroutines from running concurrently.

The the accepted file descriptor is returned in non-blocking mode.



**Parameters**

- **sockfd**: Socket file descriptor
- **addr**: Socket address out
- **addrlen**: Socket address length out



**Return**

- On success, file descriptor (non-blocking)
- On error, value -1 (NECO_ERROR) is returned, and errno is set to indicate the error.


**See also**

- [Posix wrappers](#group___posix)



<a name='group___posix_1ga963e70ab9a894c49624dc6b03d7494a4'></a>
## neco_accept_dl()
```c
int neco_accept_dl(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int64_t deadline);
```
Same as [neco_accept()](#group___posix_1ga49ed3b4837ffcd995b054cfb4bb178b1) but with a deadline parameter. 


<a name='group___posix_1ga16815a8a7a51d95281f1d70555168383'></a>
## neco_connect()
```c
int neco_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```
Connects the socket referred to by the file descriptor sockfd to the address specified by addr.

While in a coroutine, this function should be used instead of the standard connect() to avoid blocking other coroutines from running concurrently.



**Parameters**

- **sockfd**: Socket file descriptor
- **addr**: Socket address out
- **addrlen**: Socket address length out



**Return**

- NECO_OK Success
- On error, value -1 (NECO_ERROR) is returned, and errno is set to indicate the error.



<a name='group___posix_1ga50ccd758a7423fa97b717d595efe9a86'></a>
## neco_connect_dl()
```c
int neco_connect_dl(int sockfd, const struct sockaddr *addr, socklen_t addrlen, int64_t deadline);
```
Same as [neco_connect()](#group___posix_1ga16815a8a7a51d95281f1d70555168383) but with a deadline parameter. 


<a name='group___posix_1gae80194a21ffdc035a7054276b537a8c3'></a>
## neco_getaddrinfo()
```c
int neco_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
```
The getaddrinfo() function is used to get a list of addresses and port numbers for node (hostname) and service.

This is functionally identical to the Posix getaddrinfo function with the exception that it does not block, allowing for usage in a Neco coroutine.



**Return**

- On success, 0 is returned
- On error, a nonzero error code defined by the system. See the link below for a list.


**See also**

- [Posix wrappers](#group___posix)
- [https://www.man7.org/linux/man-pages/man3/getaddrinfo.3.html](https://www.man7.org/linux/man-pages/man3/getaddrinfo.3.html)



<a name='group___posix_1ga8a828e8a4e27f41aa5dfd966a3e6702d'></a>
## neco_getaddrinfo_dl()
```c
int neco_getaddrinfo_dl(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res, int64_t deadline);
```
Same as [neco_getaddrinfo()](#group___posix_1gae80194a21ffdc035a7054276b537a8c3) but with a deadline parameter. 


<a name='group___posix2_1ga6f612b7e64d5a9ec7ff607315cc8ec3a'></a>
## neco_setnonblock()
```c
int neco_setnonblock(int fd, bool nonblock, bool *oldnonblock);
```
Change the non-blocking state for a file descriptor. 

**See also**

- [File descriptor helpers](#group___posix2)



<a name='group___posix2_1ga99dc783673fbf7e3e12479d59ad7ee4c'></a>
## neco_wait()
```c
int neco_wait(int fd, int mode);
```
Wait for a file descriptor to be ready for reading or writing.

Normally one should use [neco_read()](#group___posix_1ga1869cc07ba78a167b8462a9cef09c0ba) and [neco_write()](#group___posix_1ga5c8ea872889626c86c57cc3180a673be) to read and write data. But there may be times when you need more involved logic or to use alternative functions such as `recvmsg()` or `sendmsg()`.

```c
while (1) {
    int n = recvmsg(sockfd, msg, MSG_DONTWAIT);
    if (n == -1) {
        if (errno == EAGAIN) {
            // The socket is not ready for reading.
            neco_wait(sockfd, NECO_WAIT_READ);
            continue;
        }
        // Socket error.
        return;
    }
    // Message received.
    break;
}
```




**Parameters**

- **fd**: The file descriptor
- **mode**: NECO_WAIT_READ or NECO_WAIT_WRITE



**Return**

- NECO_OK Success
- NECO_NOMEM The system lacked the necessary resources
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine
- NECO_ERROR Check errno for more info


**See also**

- [File descriptor helpers](#group___posix2)



<a name='group___posix2_1ga8392f97cbef774c6d044c75b393fbc03'></a>
## neco_wait_dl()
```c
int neco_wait_dl(int fd, int mode, int64_t deadline);
```
Same as [neco_wait()](#group___posix2_1ga99dc783673fbf7e3e12479d59ad7ee4c) but with a deadline parameter. 


<a name='group___networking_1ga62b996c86bb80c1cab2092d82c6ea53c'></a>
## neco_serve()
```c
int neco_serve(const char *network, const char *address);
```
Listen on a local network address.

**Example**

```c
int servefd = neco_serve("tcp", "127.0.0.1:8080");
if (servefd < 0) {
   // .. error, do something with it.
}
while (1) {
    int fd = neco_accept(servefd, 0, 0);
    // client accepted
}

close(servefd);
```
 

**Parameters**

- **network**: must be "tcp", "tcp4", "tcp6", or "unix".
- **address**: the address to serve on



**Return**

- On success, file descriptor (non-blocking)
- On error, Neco error


**See also**

- [Networking utilities](#group___networking)
- [neco_dial()](#group___networking_1ga327b79d78328e50dd1a98fbf444c9d0c)



<a name='group___networking_1gae0fe390fce4ecf3f80cbb9668020a59c'></a>
## neco_serve_dl()
```c
int neco_serve_dl(const char *network, const char *address, int64_t deadline);
```
Same as [neco_serve()](#group___networking_1ga62b996c86bb80c1cab2092d82c6ea53c) but with a deadline parameter. 


<a name='group___networking_1ga327b79d78328e50dd1a98fbf444c9d0c'></a>
## neco_dial()
```c
int neco_dial(const char *network, const char *address);
```
Connect to a remote server.

**Example**

```c
int fd = neco_dial("tcp", "google.com:80");
if (fd < 0) {
   // .. error, do something with it.
}
// Connected to google.com. Use neco_read(), neco_write(), or create a 
// stream using neco_stream_make(fd).
close(fd);
```
 

**Parameters**

- **network**: must be "tcp", "tcp4", "tcp6", or "unix".
- **address**: the address to dial



**Return**

- On success, file descriptor (non-blocking)
- On error, Neco error


**See also**

- [Networking utilities](#group___networking)
- [neco_serve()](#group___networking_1ga62b996c86bb80c1cab2092d82c6ea53c)



<a name='group___networking_1ga02c848d20aeee63790bb2def69fea4c9'></a>
## neco_dial_dl()
```c
int neco_dial_dl(const char *network, const char *address, int64_t deadline);
```
Same as [neco_dial()](#group___networking_1ga327b79d78328e50dd1a98fbf444c9d0c) but with a deadline parameter. 


<a name='group___cancelation_1gae6426af020deb390047106686d5da031'></a>
## neco_cancel()
```c
int neco_cancel(int64_t id);
```

<a name='group___cancelation_1ga85143c030d2055da855a9451980585af'></a>
## neco_cancel_dl()
```c
int neco_cancel_dl(int64_t id, int64_t deadline);
```

<a name='group___cancelation_1gad6dfa0851bbb7317821b27f16ef0553d'></a>
## neco_setcanceltype()
```c
int neco_setcanceltype(int type, int *oldtype);
```

<a name='group___cancelation_1ga3cdc3d0b6f48a805fafa94247122cb75'></a>
## neco_setcancelstate()
```c
int neco_setcancelstate(int state, int *oldstate);
```

<a name='group___random_1gaefd0991a1cdf430f22899fd34bdba892'></a>
## neco_rand_setseed()
```c
int neco_rand_setseed(int64_t seed, int64_t *oldseed);
```
Set the random seed for the Neco pseudorandom number generator.

The provided seed is only used for the (non-crypto) NECO_PRNG and is ignored for NECO_CPRNG.



**Parameters**

- **seed**: 
- **oldseed[out]**: The previous seed



**Return**

- NECO_OK Success
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine


**See also**

- [Random number generator](#group___random)



<a name='group___random_1ga4567eff3299e3b00e829b82ca657d8cd'></a>
## neco_rand()
```c
int neco_rand(void *data, size_t nbytes, int attr);
```
Generator random bytes

This operation can generate cryptographically secure data by providing the NECO_CSPRNG option or non-crypto secure data with NECO_PRNG.

Non-crypto secure data use the [pcg-family](https://www.pcg-random.org) random number generator.



**Parameters**

- **data**: buffer for storing random bytes
- **nbytes**: number of bytes to generate
- **attr**: NECO_PRNG or NECO_CSPRNG



**Return**

- NECO_OK Success
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine
- NECO_CANCELED Operation canceled



<a name='group___random_1gaaad47a533a78ff549be6d41339846493'></a>
## neco_rand_dl()
```c
int neco_rand_dl(void *data, size_t nbytes, int attr, int64_t deadline);
```
Same as [neco_rand()](#group___random_1ga4567eff3299e3b00e829b82ca657d8cd) but with a deadline parameter. 


<a name='group___signals_1ga4848017f351283eed5e432a9af9ddd7b'></a>
## neco_signal_watch()
```c
int neco_signal_watch(int signo);
```
Have the current coroutine watch for a signal.

This can be used to intercept or ignore signals.

Signals that can be watched: SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGPIPE, SIGUSR1, SIGUSR2, SIGALRM

Signals that *can not* be watched: SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP

**Example**

```c
// Set up this coroutine to watich for the SIGINT (Ctrl-C) and SIGQUIT
// (Ctrl-\) signals.
neco_signal_watch(SIGINT);
neco_signal_watch(SIGQUIT);

printf("Waiting for Ctrl-C or Ctrl-\\ signals...\n");
int sig = neco_signal_wait();
if (sig == SIGINT) {
    printf("\nReceived Ctrl-C\n");
} else if (sig == SIGQUIT) {
    printf("\nReceived Ctrl-\\\n");
}

// The neco_signal_unwatch is used to stop watching.
neco_signal_unwatch(SIGINT);
neco_signal_unwatch(SIGQUIT);
```




**Parameters**

- **signo**: The signal number



**Return**

- NECO_OK Success
- NECO_INVAL An invalid parameter was provided
- NECO_PERM Operation called outside of a coroutine


**See also**

- [Signals](#group___signals)



<a name='group___signals_1gaa34972437b6b9e85ba0efec6cd388516'></a>
## neco_signal_wait()
```c
int neco_signal_wait();
```
Wait for a signal to arrive. 

**Return**

- A signal number or an error.
- NECO_PERM
- NECO_NOSIGWATCH if not currently watching for signals.
- NECO_CANCELED


**See also**

- [Signals](#group___signals)
- [neco_signal_wait_dl()](#group___signals_1gacd8061a747344fbde13cbdfcc99efd8f)



<a name='group___signals_1gacd8061a747344fbde13cbdfcc99efd8f'></a>
## neco_signal_wait_dl()
```c
int neco_signal_wait_dl(int64_t deadline);
```
Same as [neco_signal_wait()](#group___signals_1gaa34972437b6b9e85ba0efec6cd388516) but with a deadline parameter. 


<a name='group___signals_1ga58c262e5ded1ec30c64618959453517a'></a>
## neco_signal_unwatch()
```c
int neco_signal_unwatch(int signo);
```
Stop watching for a siganl to arrive 

**Parameters**

- **signo**: The signal number



**Return**

- NECO_OK on success or an error



<a name='group___worker_1ga6a8c55b440330f4a1f7b654787d96946'></a>
## neco_work()
```c
int neco_work(int64_t pin, void(*work)(void *udata), void *udata);
```
Perform work in a background thread and wait until the work is done.

This operation cannot be canceled and cannot timeout. It's the responibilty of the caller to figure out a mechanism for doing those things from inside of the work function.

The work function will not be inside of a Neco context, thus all `neco_*` functions will fail if called from inside of the work function.



**Parameters**

- **pin**: pin to a thread, or use -1 for round robin selection.
- **work**: the work, must not be null
- **udata**: any user data



**Return**

- NECO_OK Success
- NECO_NOMEM The system lacked the necessary resources
- NECO_INVAL An invalid parameter was provided


**Note**

- There is no way to cancel or timeout this operation
- There is no way to cancel or timeout this operation



<a name='group___stats_1ga9728fa5ef28c4d88a951faaf7d26a506'></a>
## neco_getstats()
```c
int neco_getstats(neco_stats *stats);
```
Returns various stats for the current Neco runtime.

```c
// Print the number of active coroutines
neco_stats stats;
neco_getstats(&stats);
printf("%zu\n", stats.coroutines);
```


Other stats include:

```c
coroutines
sleepers
evwaiters
sigwaiters
senders
receivers
locked
waitgroupers
condwaiters
suspended
```
 


<a name='group___stats_1ga44d9b10110f57fb7491b236d3ec7e731'></a>
## neco_is_main_thread()
```c
int neco_is_main_thread();
```
Test if coroutine is running on main thread. 

**Return**

- 1 for true, 0 for false, or a negative value for error.



<a name='group___stats_1ga60ee198bb5309531aac43b8276ac7a64'></a>
## neco_switch_method()
```c
const char *neco_switch_method();
```

<a name='group___global_funcs_1gae6a60529f176d23f51f10a3906beb568'></a>
## neco_env_setallocator()
```c
void neco_env_setallocator(void *(*malloc)(size_t), void *(*realloc)(void *, size_t), void(*free)(void *));
```
Globally set the allocators for all Neco functions.

*This should only be run once at program startup and before the first neco_start function is called*. 

**See also**

- [Global environment](#group___global_funcs)



<a name='group___global_funcs_1ga10db728074cebc8d35cfcec01bb1dc97'></a>
## neco_env_setpaniconerror()
```c
void neco_env_setpaniconerror(bool paniconerror);
```
Globally set the panic-on-error state for all coroutines.

This will cause panics (instead of returning the error) for three errors: `NECO_INVAL`, `NECO_PERM`, and `NECO_NOMEM`.

*This should only be run once at program startup and before the first neco_start function is called*. 

**See also**

- [Global environment](#group___global_funcs)



<a name='group___global_funcs_1gacab37dcac0bef3c4a29f523ea32b8df3'></a>
## neco_env_setcanceltype()
```c
void neco_env_setcanceltype(int type);
```
Globally set the canceltype for all coroutines.

*This should only be run once at program startup and before the first neco_start function is called*. 

**See also**

- [Global environment](#group___global_funcs)



<a name='group___global_funcs_1ga09e93872d1245a3bcba13b3b45b43618'></a>
## neco_env_setcancelstate()
```c
void neco_env_setcancelstate(int state);
```
Globally set the cancelstate for all coroutines.

*This should only be run once at program startup and before the first neco_start function is called*. 

**See also**

- [Global environment](#group___global_funcs)



<a name='group___time_1gad969c647e4b3b661eab3f7f5b5372d38'></a>
## neco_now()
```c
int64_t neco_now();
```
Get the current time.

This operation calls gettime(CLOCK_MONOTONIC) to retreive a monotonically increasing value that is not affected by discontinuous jumps in the system time.

This value IS NOT the same as the local system time; for that the user should call gettime(CLOCK_REALTIME).

The main purpose of this function to work with operations that use deadlines, i.e. functions with the `*_dl()` suffix.

**Example**

```c
// Receive a message from a channel using a deadline of one second from now.
int ret = neco_chan_recv_dl(ch, &msg, neco_now() + NECO_SECOND);
if (ret == NECO_TIMEDOUT) {
    // The operation timed out
}
```




**Return**

- On success, the current time as nanoseconds.
- NECO_PERM Operation called outside of a coroutine


**See also**

- [Time](#group___time)



<a name='group___error_funcs_1ga053c32e9432aef83bda474e41fd7b2d4'></a>
## neco_strerror()
```c
const char *neco_strerror(ssize_t errcode);
```
Returns a string representation of an error code. 

**See also**

- Errors



<a name='group___error_funcs_1ga82ade5f26218caa8e0b4883575a683bd'></a>
## neco_lasterr()
```c
int neco_lasterr();
```
Returns last known error from a Neco operation

See [Neco errors](./API.md#errors) for a list. 


<a name='group___error_funcs_1ga6b2d592c3044f9756073daabfe25b470'></a>
## neco_gai_lasterr()
```c
int neco_gai_lasterr();
```
Get the last error from a [neco_getaddrinfo()](#group___posix_1gae80194a21ffdc035a7054276b537a8c3) call.

See the [man page](https://man.freebsd.org/cgi/man.cgi?query=gai_strerror) for a list of errors. 


<a name='group___error_funcs_1gae611410883c5d2cfa24def14f3a2cc38'></a>
## neco_panic()
```c
int neco_panic(const char *fmt,...);
```
Stop normal execution of the current coroutine, print stack trace, and exit the program immediately. 


<a name='group___streams_1ga74885d1c39c7c8442fc9e82b8411243e'></a>
## neco_stream_make()
```c
int neco_stream_make(neco_stream **stream, int fd);
```

<a name='group___streams_1ga70276b8dd3bfe1a3e715ffd4c8486620'></a>
## neco_stream_make_buffered()
```c
int neco_stream_make_buffered(neco_stream **stream, int fd);
```

<a name='group___streams_1ga526b51cbef51b1e36723ca718368f215'></a>
## neco_stream_close()
```c
int neco_stream_close(neco_stream *stream);
```
Close a stream. 


<a name='group___streams_1gaaec08f5c1c1d3d630fa2faba75b6a6fb'></a>
## neco_stream_close_dl()
```c
int neco_stream_close_dl(neco_stream *stream, int64_t deadline);
```
Close a stream with a deadline. A deadline is provided to accomodate for buffered streams that may need to flush bytes on close 


<a name='group___streams_1gae6c836fd60b4a2225b5ccdecbe529c38'></a>
## neco_stream_read()
```c
ssize_t neco_stream_read(neco_stream *stream, void *data, size_t nbytes);
```

<a name='group___streams_1gad8f1e0ac6eae64fc4d8cb995b17a0bb6'></a>
## neco_stream_read_dl()
```c
ssize_t neco_stream_read_dl(neco_stream *stream, void *data, size_t nbytes, int64_t deadline);
```

<a name='group___streams_1gaa3f1c676605a7441c527f1a798cedacf'></a>
## neco_stream_write()
```c
ssize_t neco_stream_write(neco_stream *stream, const void *data, size_t nbytes);
```

<a name='group___streams_1ga09f9bdd323f2ba29669d71968c7fd3cd'></a>
## neco_stream_write_dl()
```c
ssize_t neco_stream_write_dl(neco_stream *stream, const void *data, size_t nbytes, int64_t deadline);
```

<a name='group___streams_1ga1b3b8a4e10113768f22c6cb5b7992ce4'></a>
## neco_stream_readfull()
```c
ssize_t neco_stream_readfull(neco_stream *stream, void *data, size_t nbytes);
```

<a name='group___streams_1ga1f08421b4cac63e930046c328c6f3c6a'></a>
## neco_stream_readfull_dl()
```c
ssize_t neco_stream_readfull_dl(neco_stream *stream, void *data, size_t nbytes, int64_t deadline);
```

<a name='group___streams_1gaf8dc61bc7e0b9c6c105c8ab51693ee3d'></a>
## neco_stream_read_byte()
```c
int neco_stream_read_byte(neco_stream *stream);
```
Read and returns a single byte. If no byte is available, returns an error. 


<a name='group___streams_1ga3096f4f082cee9cddcfaad4ca3694f7a'></a>
## neco_stream_read_byte_dl()
```c
int neco_stream_read_byte_dl(neco_stream *stream, int64_t deadline);
```
Same as [neco_stream_read_byte()](#group___streams_1gaf8dc61bc7e0b9c6c105c8ab51693ee3d) but with a deadline parameter. 


<a name='group___streams_1ga1ec67ae2dfa4cae9a26c536e3c44c4f6'></a>
## neco_stream_unread_byte()
```c
int neco_stream_unread_byte(neco_stream *stream);
```
Unread the last byte. Only the most recently read byte can be unread. 


<a name='group___streams_1ga1c847c02240767c06daf832eb85022e2'></a>
## neco_stream_flush()
```c
int neco_stream_flush(neco_stream *stream);
```
Flush writes any buffered data to the underlying file descriptor. 


<a name='group___streams_1gac4818a4bc2497ca46db221a1fea96746'></a>
## neco_stream_flush_dl()
```c
int neco_stream_flush_dl(neco_stream *stream, int64_t deadline);
```
Same as [neco_stream_flush()](#group___streams_1ga1c847c02240767c06daf832eb85022e2) but with a deadline parameter. 


<a name='group___streams_1gaf8d0174f3c7627ae4b0635d493528616'></a>
## neco_stream_buffered_read_size()
```c
ssize_t neco_stream_buffered_read_size(neco_stream *stream);
```

<a name='group___streams_1gae870778b3faddfcd71de62880adb6172'></a>
## neco_stream_buffered_write_size()
```c
ssize_t neco_stream_buffered_write_size(neco_stream *stream);
```

***

Generated with the help of [doxygen](https://www.doxygen.nl/index.html)
