#include "tests.h"

#if defined(_WIN32)
DISABLED("test_signal", "Windows")
#elif defined(__EMSCRIPTEN__)
DISABLED("test_signal", "Emscripten")
#else

#include <pthread.h>
#include <signal.h>

extern __thread int neco_last_sigexitnow;
void neco_reset_sigcrashed(void);
const char *strsignal0(int signo);

void co_signal_basic_child(int argc, void *argv[]) {
    assert(argc == 2);
    int sigexpect = *(int*)(argv[0]);
    struct neco_chan *ch = argv[1];
    // printf("co_signal: %d (%p)\n", sigexpect, neco_stack_bottom());
    neco_signal_watch(SIGUSR1);
    neco_signal_watch(SIGUSR2);
    while (1) {
        int signo = neco_signal_wait();
        // printf("    SIGNAL %d (%p)\n", signo, neco_stack_bottom());
        if (signo == sigexpect) {
            // printf("    ACCEPT (%p)\n", neco_stack_bottom());
            assert(neco_chan_send(ch, &signo) == NECO_OK);
            break;
        } else {
            printf("???\n");
            assert(0);
        }
    }
    neco_signal_unwatch(SIGUSR1);
    neco_signal_unwatch(SIGUSR2);
    // printf("co_signal: %d DONE\n", sigexpect);
}

void co_signal_basic(int argc, void *argv[]) {
    (void)argc; (void)argv;
    // printf("co_signal\n");
    struct neco_chan *chsig;
    assert(neco_chan_make(&chsig, sizeof(int), 0) == NECO_OK);
    assert(chsig);
    int sigex0 = SIGUSR1;
    assert(neco_start(co_signal_basic_child, 2, &sigex0, chsig) == NECO_OK);
    neco_yield();
    int sigex1 = SIGUSR2;
    assert(neco_start(co_signal_basic_child, 2, &sigex1, chsig) == NECO_OK);
    neco_yield();
    // printf("waiting on signal... (pid: %d)\n", getpid());
    raise(SIGUSR1);
    raise(SIGUSR2);
    raise(SIGUSR2);
    int i = 0;
    int j = 0;
    while (1) {
        int signo;
        assert(neco_chan_recv(chsig, &signo) == NECO_OK);
        i++;
        j += signo;
        if (j == SIGUSR1+SIGUSR2) {
            assert(i == 2);
            break;
        }
    }
    assert(neco_chan_close(chsig) == NECO_OK);
    assert(neco_chan_release(chsig) == NECO_OK);

    // Test out of range signal. 
    // neco_last_sigexitnow = 0;
    // raise(33);
    // neco_yield();
    // assert(neco_last_sigexitnow == 0);

    expect(neco_signal_watch(0), NECO_INVAL);
    expect(neco_signal_unwatch(0), NECO_INVAL);
    expect(neco_signal_watch(32), NECO_INVAL);
    expect(neco_signal_unwatch(32), NECO_INVAL);
    expect(neco_signal_wait(), NECO_NOSIGWATCH);

    expect(neco_signal_watch(SIGINT), NECO_OK);
    expect(neco_cancel(neco_getid()), NECO_OK);
    expect(neco_signal_wait(), NECO_CANCELED);
    expect(neco_signal_unwatch(SIGINT), NECO_OK);


}

void test_signal_basic(void) {
    assert(strcmp(strsignal0(-1), "Unknown signal: -1") == 0);
    assert(strcmp(strsignal0(33), "Unknown signal: 33") == 0);
    assert(strlen(strsignal0(1)) > 0);
    assert(neco_start(co_signal_basic, 0) == NECO_OK);
    expect(neco_signal_watch(SIGINT), NECO_PERM);
    expect(neco_signal_unwatch(SIGINT), NECO_PERM);
    expect(neco_signal_wait(), NECO_PERM);
}

void co_signal_deadline_canceler(int argc, void *argv[]) {
    assert(argc == 1);
    int64_t id = *(int64_t*)argv[0];
    expect(neco_yield(), NECO_OK);
    raise(SIGUSR1);
    raise(SIGUSR1);
    expect(neco_sleep(NECO_SECOND/2), NECO_OK);
    expect(neco_cancel(id), NECO_OK);
}

void co_signal_deadline(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int64_t id = neco_getid();
    expect(neco_start(co_signal_deadline_canceler, 1, &id), NECO_OK);
    expect(neco_signal_watch(SIGUSR1), NECO_OK);
    expect(neco_signal_wait_dl(neco_now() + NECO_SECOND/8), SIGUSR1);
    expect(neco_signal_wait(), SIGUSR1);
    expect(neco_signal_wait_dl(neco_now() + NECO_SECOND/8), NECO_TIMEDOUT);
    expect(neco_signal_wait_dl(neco_now() + NECO_SECOND*2), NECO_CANCELED);
    expect(neco_signal_unwatch(SIGUSR1), NECO_OK);
}

void test_signal_deadline(void) {
    expect(neco_start(co_signal_deadline, 0), NECO_OK);
}

void co_signal_exit0(int argc, void *argv[]) {
    (void)argc; (void)argv;
    neco_sleep(NECO_SECOND/4);
}

void co_signal_watch_no_wait(int argc, void *argv[]) {
    (void)argc; (void)argv;
    expect(neco_signal_watch(SIGUSR2), NECO_OK);
    neco_sleep(NECO_SECOND/10);
    expect(neco_signal_unwatch(SIGUSR2), NECO_OK);
}


void co_signal_exit(int argc, void *argv[]) {
    (void)argc; (void)argv;
    assert(neco_is_main_thread() == true);
    // Simulates an exit on SIGINT and SIGUSR1

    neco_last_sigexitnow = 0;
    raise(SIGILL);
    neco_yield();
    assert(neco_last_sigexitnow == SIGILL);
    neco_reset_sigcrashed();

    neco_last_sigexitnow = 0;
    raise(SIGTERM);
    neco_yield();
    assert(neco_last_sigexitnow == SIGTERM);
    neco_reset_sigcrashed();
    

    neco_last_sigexitnow = 0;
    raise(SIGUSR1);
    neco_yield();
    assert(neco_last_sigexitnow == SIGUSR1);
    neco_reset_sigcrashed();


    neco_last_sigexitnow = 0;
    raise(SIGFPE);
    neco_yield();
    assert(neco_last_sigexitnow == SIGFPE);

    // Without a reset, the last crash (SIGILL) will continue forward.

    neco_last_sigexitnow = 0;
    raise(SIGPIPE);
    neco_yield();
    assert(neco_last_sigexitnow == SIGFPE);
    neco_reset_sigcrashed();

    // Finally ensure that a watched signal eventually crashes when a child
    // unwatches without waiting.
    expect(neco_start(co_signal_watch_no_wait, 0), NECO_OK);
    // printf("===============================\n");
    neco_last_sigexitnow = 0;
    // printf("E\n");
    raise(SIGUSR2);
    neco_sleep(NECO_SECOND/5);
    assert(neco_last_sigexitnow == SIGUSR2);
    // printf("===============================\n");

    // printf("2\n");
}

void test_signal_exit(void) {
    expect(neco_start(co_signal_exit, 0), NECO_OK);
}

void co_signal_exit_thread(int argc, void *argv[]) {
    (void)argc; (void)argv;
    assert(neco_is_main_thread() == false);
    expect(neco_signal_watch(SIGINT), NECO_PERM);
    expect(neco_signal_unwatch(SIGINT), NECO_PERM);
}

void *th_signal(void *arg) {
    (void)arg;
    expect(neco_start(co_signal_exit_thread, 0), NECO_OK);
    return NULL;
}

void test_signal_thread(void) {
    pthread_t th;
    assert(pthread_create(&th, 0, th_signal, 0) == 0);
    assert(pthread_join(th, 0) == 0);
}

int main(int argc, char **argv) {
    do_test(test_signal_basic);
    do_test(test_signal_deadline);
    do_test(test_signal_exit);
    do_test(test_signal_thread);
}

#endif
