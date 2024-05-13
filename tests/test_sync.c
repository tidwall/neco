#include <signal.h>
#include "tests.h"

int _neco_mutex_stats(neco_mutex *mutex);

void co_sync_mutex_child(int argc, void *argv[]) {
    assert(argc == 4);
    int i = *(int*)argv[0];
    neco_mutex *mu = argv[1];
    neco_chan *ch = argv[2];
    assert(neco_chan_retain(ch) == NECO_OK);
    int *x = argv[3];
    switch (i) {
    case 0:
        neco_sleep(NECO_MILLISECOND * 500);
        break;
    case 1:
        neco_sleep(NECO_MILLISECOND * 200);
        break;
    case 2:
        neco_sleep(NECO_MILLISECOND * 300);
        break;
    case 3:
        neco_sleep(NECO_MILLISECOND * 100);
        break;
    case 4:
        neco_sleep(NECO_MILLISECOND * 400);
        break;
    }
    assert(neco_mutex_lock(mu) == NECO_OK);
    int px;
    switch (i) {
    case 0:
        assert(*x == 455069);
        px = 233912;
        break;
    case 1:
        assert(*x == 817263);
        px = 712934;
        break;
    case 2:
        assert(*x == 712934);
        px = 102993;
        break;
    case 3:
        assert(*x == 918273);
        px = 817263;
        break;
    case 4:
        assert(*x == 102993);
        px = 455069;
        break;
    }
    *x = 0;
    neco_sleep(NECO_MILLISECOND);
    assert(*x == 0);
    *x = px;
    assert(neco_mutex_unlock(mu) == NECO_OK);
    assert(neco_chan_send(ch, 0) == NECO_OK);
    assert(neco_chan_release(ch) == NECO_OK);
}

void co_sync_mutex(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    int x = 881772;
    neco_mutex mu;
    assert(neco_mutex_init(&mu) == NECO_OK);
    
    neco_chan *ch;
    assert(neco_chan_make(&ch, 0, 0) == NECO_OK);
    for (int i = 0; i < 5; i++) {
        assert(neco_start(co_sync_mutex_child, 4, &i, &mu, ch, &x) == NECO_OK);
    }
    assert(neco_mutex_lock(&mu) == NECO_OK);
    assert(x == 881772);
    neco_sleep(NECO_MILLISECOND * 600);
    x = 918273;
    assert(neco_mutex_unlock(&mu) == NECO_OK);
    // joiner
    for (int i = 0; i < 5; i++) {
        assert(neco_chan_recv(ch, 0) == NECO_OK);
    }
    assert(neco_mutex_lock(&mu) == NECO_OK);
    assert(x == 233912);
    expect(neco_mutex_unlock(&mu), NECO_OK);
    expect(neco_mutex_unlock(&mu), NECO_OK);
    expect(neco_chan_release(ch), NECO_OK);
}

void test_sync_mutex(void) {
    assert(neco_start(co_sync_mutex, 0) == NECO_OK);
}

void co_sync_waitgroup_child(int argc, void *argv[]) {
    assert(argc == 2);
    neco_waitgroup *wg = argv[0];
    int *x = argv[1];
    *x = *x + 1;
    neco_sleep(NECO_MILLISECOND*(*x));
    expect(neco_waitgroup_done(wg), NECO_OK);
}

void co_sync_waitgroup_multi_waiter(int argc, void *argv[]) {
    assert(argc == 1);
    neco_waitgroup *wg = argv[0];
    expect(neco_waitgroup_wait(wg), NECO_OK);
}

void co_sync_waitgroup_multi_done(int argc, void *argv[]) {
    assert(argc == 1);
    neco_waitgroup *wg = argv[0];
    expect(neco_sleep(NECO_SECOND), NECO_OK);
    expect(neco_waitgroup_done(wg), NECO_OK);
}

void co_sync_waitgroup(int argc, void *argv[]) {
    assert(argc == 1);
    bool multi = *(bool*)argv[0];
    neco_waitgroup wg;
    int N = 5;
    expect(neco_waitgroup_init(&wg), NECO_OK);
    expect(neco_waitgroup_add(&wg, N), NECO_OK);
    int x = 0;
    for (int i = 0; i < N; i++) {
        expect(neco_start(co_sync_waitgroup_child, 2, &wg, &x), NECO_OK);
    }
    expect(neco_waitgroup_wait(&wg), NECO_OK);
    expect(neco_waitgroup_add(&wg, -1), NECO_NEGWAITGRP);
    expect(neco_waitgroup_done(&wg), NECO_NEGWAITGRP);
    expect(neco_waitgroup_destroy(&wg), NECO_OK);
    assert(x == N);

    // Should work after destroy because it's automatically reinitialized upon
    // neco_waitgroup_add() or neco_waitgroup_wait().
    if (multi) {
        expect(neco_waitgroup_add(&wg, 1), NECO_OK);
        expect(neco_start(co_sync_waitgroup_multi_done, 1, &wg), NECO_OK);
        expect(neco_start(co_sync_waitgroup_multi_waiter, 1, &wg), NECO_OK);
    }
    expect(neco_waitgroup_wait(&wg), NECO_OK);
}

void test_sync_waitgroup_one(void) {
    expect(neco_start(co_sync_waitgroup, 1, &(bool){false}), NECO_OK);
}

void test_sync_waitgroup_multi(void) {
    expect(neco_start(co_sync_waitgroup, 1, &(bool){true}), NECO_OK);
}

void co_sync_waitgroup_cancel_child(int argc, void **argv) {
    assert(argc == 1);
    int64_t id = *(int64_t*)argv[0];
    expect(neco_sleep(NECO_SECOND/8), NECO_OK);
    expect(neco_cancel(id), NECO_OK);
    expect(neco_sleep(NECO_SECOND/8), NECO_OK);
}

void co_sync_waitgroup_cancel(int argc, void **argv) {
    assert(argc == 0);
    (void)argv;
    neco_waitgroup wg;
    expect(neco_waitgroup_init(&wg), NECO_OK);
    int64_t id = neco_getid();
    expect(neco_cancel(id), NECO_OK);
    expect(neco_waitgroup_wait(&wg), NECO_CANCELED);
    expect(neco_waitgroup_add(&wg, 1), NECO_OK);
    expect(neco_start(co_sync_waitgroup_cancel_child, 1, &id), NECO_OK);
    expect(neco_waitgroup_wait(&wg), NECO_CANCELED);
    expect(neco_waitgroup_wait_dl(&wg, neco_now()+1), NECO_TIMEDOUT);
    expect(neco_waitgroup_done(&wg), NECO_OK);
    expect(neco_waitgroup_wait_dl(&wg, neco_now()-1), NECO_TIMEDOUT);
}

void test_sync_waitgroup_cancel(void) {
    expect(neco_start(co_sync_waitgroup_cancel, 0), NECO_OK);
}

void co_sync_waitgroup_fail(int argc, void **argv) {
    assert(argc == 0);
    (void)argv;
    neco_waitgroup wg;
    expect(neco_waitgroup_init(&wg), NECO_OK);
    expect(neco_waitgroup_add(&wg, 1), NECO_OK);

    // poison the waitgroup
    memset(&wg, 63, sizeof(neco_waitgroup));
    expect(neco_waitgroup_add(&wg, 1), NECO_PERM);
    memset(&wg, 0, sizeof(neco_waitgroup));

}

void test_sync_waitgroup_fail(void) {
    expect(neco_waitgroup_init(0), NECO_INVAL);
    expect(neco_waitgroup_destroy(0), NECO_INVAL);
    expect(neco_waitgroup_done(0), NECO_INVAL);
    expect(neco_waitgroup_add(0, 0), NECO_INVAL);
    expect(neco_waitgroup_wait(0), NECO_INVAL);

    neco_waitgroup wg;
    expect(neco_waitgroup_init(&wg), NECO_PERM);
    expect(neco_waitgroup_wait(&wg), NECO_PERM);
    
    expect(neco_start(co_sync_waitgroup_fail, 0), NECO_OK);
}

void co_sync_cond_signal_child(int argc, void *argv[]) {
    assert(argc == 2);
    neco_cond *cond = argv[0];
    neco_mutex *mutex = argv[1];
    assert(!!mutex);
    expect(neco_sleep(NECO_SECOND/4), NECO_OK);
    expect(neco_cond_signal(cond), NECO_OK);
}

void co_sync_cond_signal(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    neco_cond cond;
    expect(neco_cond_init(&cond), NECO_OK);
    neco_mutex mutex;
    expect(neco_mutex_init(&mutex), NECO_OK);
    expect(neco_start(co_sync_cond_signal_child, 2, &cond, &mutex), NECO_OK);
    expect(neco_mutex_lock(&mutex), NECO_OK);
    expect(neco_cond_wait(&cond, &mutex), NECO_OK);
    expect(neco_mutex_unlock(&mutex), NECO_OK);
    expect(neco_cond_destroy(&cond), NECO_OK);
    expect(neco_mutex_destroy(&mutex), NECO_OK);
}

void test_sync_cond_signal(void) {
    expect(neco_start(co_sync_cond_signal, 0), NECO_OK);
}

void co_sync_cond_signal_cancel_child(int argc, void **argv) {
    assert(argc == 2);
    (void)argv;
    int64_t id = *(int64_t*)argv[0];
    bool delay = *(bool*)argv[1];

    if (delay) {
        expect(neco_sleep(NECO_SECOND/8), NECO_OK);
    }
    expect(neco_cancel(id), NECO_OK);
}

void co_sync_cond_signal_cancel(int argc, void **argv) {
    assert(argc == 0)
    (void)argv;

    neco_cond cond = { 0 };
    neco_mutex mutex = { 0 };
    int64_t id = neco_getid();
    bool delay;

    expect(neco_mutex_lock(&mutex), NECO_OK);
    delay = false;
    expect(neco_start(co_sync_cond_signal_cancel_child, 2, &id, &delay), NECO_OK);
    // This gets canceled right away
    expect(neco_cond_wait(&cond, &mutex), NECO_CANCELED);
    expect(neco_mutex_unlock(&mutex), NECO_OK);

    expect(neco_mutex_lock(&mutex), NECO_OK);
    delay = true;
    expect(neco_start(co_sync_cond_signal_cancel_child, 2, &id, &delay), NECO_OK);
    // This gets canceled after slight delay
    expect(neco_cond_wait(&cond, &mutex), NECO_CANCELED);
    expect(neco_mutex_unlock(&mutex), NECO_OK);
}

void test_sync_cond_signal_cancel(void) {
    expect(neco_start(co_sync_cond_signal_cancel, 0), NECO_OK);
}

void co_sync_cond_broadcast_child(int argc, void *argv[]) {
    assert(argc == 3);
    neco_cond *cond = argv[0];
    neco_mutex *mutex = argv[1];
    int *x = argv[2];
    expect(neco_mutex_lock(mutex), NECO_OK);
    expect(neco_cond_wait(cond, mutex), NECO_OK);
    (*x)++;
    expect(neco_mutex_unlock(mutex), NECO_OK);
}

void co_sync_cond_broadcast(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    neco_cond cond;
    expect(neco_cond_init(&cond), NECO_OK);
    neco_mutex mutex;
    expect(neco_mutex_init(&mutex), NECO_OK);
    int N = 10;
    int x = 0;
    for (int i = 0; i < N; i++) {
        expect(neco_start(co_sync_cond_broadcast_child, 3, &cond, &mutex, &x), NECO_OK);
    }
    expect(neco_sleep(NECO_SECOND/4), NECO_OK);
    expect(neco_cond_broadcast(&cond), NECO_OK);
    while (1) {
        expect(neco_mutex_lock(&mutex), NECO_OK);
        bool done = x == N;
        expect(neco_mutex_unlock(&mutex), NECO_OK);
        if (done) {
            break;
        }
    }
    expect(neco_cond_destroy(&cond), NECO_OK);
    expect(neco_mutex_destroy(&mutex), NECO_OK);
}

void test_sync_cond_broadcast(void) {
    expect(neco_start(co_sync_cond_broadcast, 0), NECO_OK);
}

void co_sync_cond_deadline_child(int argc, void *argv[]) {
    assert(argc == 2);
    neco_cond *cond = argv[0];
    neco_mutex *mutex = argv[1];
    assert(!!mutex);
    neco_sleep(NECO_SECOND/2);
    expect(neco_cond_signal(cond), NECO_OK);
}

void co_sync_cond_deadline(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    neco_cond cond;
    expect(neco_cond_init(&cond), NECO_OK);
    neco_mutex mutex;
    expect(neco_mutex_init(&mutex), NECO_OK);
    expect(neco_start(co_sync_cond_deadline_child, 2, &cond, &mutex), NECO_OK);
    expect(neco_mutex_lock(&mutex), NECO_OK);
    expect(neco_cond_wait_dl(&cond, &mutex, neco_now()+NECO_SECOND/8), NECO_TIMEDOUT);
    expect(neco_cond_wait_dl(&cond, &mutex, neco_now()+NECO_SECOND), NECO_OK);
    expect(neco_mutex_unlock(&mutex), NECO_OK);
    expect(neco_cond_destroy(&cond), NECO_OK);
    expect(neco_mutex_destroy(&mutex), NECO_OK);
}

void test_sync_cond_deadline(void) {
    expect(neco_start(co_sync_cond_deadline, 0), NECO_OK);
}

void co_sync_mutex_rw_order_child(int argc, void *argv[]) {
    assert(argc == 2);
    neco_mutex *mu = argv[0];
    struct order *order = argv[1];
    order_add(order, 6);
    expect(neco_mutex_trylock(mu), NECO_BUSY);
    order_add(order, 7);
    expect(neco_mutex_tryrdlock(mu), NECO_OK);
    order_add(order, 8);
    expect(neco_mutex_unlock(mu), NECO_OK);
    order_add(order, 10);
    expect(neco_mutex_lock(mu), NECO_OK);
    order_add(order, 13);
    expect(neco_yield(), NECO_OK);
    order_add(order, 14);
}

void co_sync_mutex_rw_order(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    // order_print = true;
    struct order order = { 0 };
    neco_mutex _mutex;
    neco_mutex *mu = &_mutex;
    order_add(&order, 1);
    expect(neco_mutex_init(mu), NECO_OK);
    order_add(&order, 2);
    expect(neco_mutex_rdlock(mu), NECO_OK);
    order_add(&order, 3);
    expect(neco_mutex_rdlock(mu), NECO_OK);
    order_add(&order, 4);
    expect(neco_mutex_rdlock(mu), NECO_OK);
    order_add(&order, 5);
    expect(neco_start(co_sync_mutex_rw_order_child, 2, mu, &order), NECO_OK);
    order_add(&order, 9);
    expect(neco_mutex_unlock(mu), NECO_OK);
    order_add(&order, 11);
    expect(neco_mutex_unlock(mu), NECO_OK);
    order_add(&order, 12);
    expect(neco_mutex_unlock(mu), NECO_OK);
    order_add(&order, 15);
    order_check(&order);
}

void test_sync_mutex_rw_order(void) {
    expect(neco_start(co_sync_mutex_rw_order, 0), NECO_OK);
}

void co_sync_mutex_deadline_child(int argc, void *argv[]) {
    assert(argc == 2);
    int64_t id = *(int64_t*)argv[0];
    neco_mutex *mutex = argv[1];
    assert(!!mutex);
    expect(neco_sleep(NECO_SECOND/4), NECO_OK);
    expect(neco_cancel(id), NECO_OK);
}

void co_sync_mutex_deadline(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    int64_t id = neco_getid();
    neco_mutex mutex;
    expect(neco_mutex_init(&mutex), NECO_OK);
    expect(neco_mutex_lock(&mutex), NECO_OK);
    expect(neco_start(co_sync_mutex_deadline_child, 2, &id, &mutex), NECO_OK);
    expect(neco_mutex_lock_dl(&mutex, neco_now()+NECO_SECOND/8), NECO_TIMEDOUT);
    expect(neco_cancel(id), NECO_OK);
    expect(neco_mutex_lock_dl(&mutex, neco_now()+NECO_HOUR), NECO_CANCELED);
    expect(neco_mutex_lock_dl(&mutex, neco_now()+NECO_HOUR), NECO_CANCELED);
    expect(neco_cancel(id), NECO_OK);
    expect(neco_mutex_rdlock_dl(&mutex, neco_now()-1), NECO_CANCELED);
    expect(neco_mutex_rdlock_dl(&mutex, neco_now()-1), NECO_TIMEDOUT);
}

void test_sync_mutex_deadline(void) {
    expect(neco_start(co_sync_mutex_deadline, 0), NECO_OK);
}

void co_sync_mutex_fail(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    neco_mutex mutex;
    expect(neco_mutex_init(0), NECO_INVAL);
    expect(neco_mutex_init(&mutex), NECO_OK);
    expect(neco_mutex_lock(&mutex), NECO_OK);
    expect(neco_mutex_destroy(&mutex), NECO_BUSY);
    expect(neco_mutex_fastlock(&mutex, neco_now() - 1), NECO_TIMEDOUT);
}

void test_sync_mutex_fail(void) {
    expect(neco_mutex_destroy(0), NECO_INVAL);
    neco_mutex mutex;
    expect(neco_mutex_init(&mutex), NECO_PERM);
    expect(neco_mutex_lock(&mutex), NECO_PERM);
    expect(neco_mutex_trylock(&mutex), NECO_PERM);
    expect(neco_mutex_rdlock(&mutex), NECO_PERM);
    expect(neco_mutex_tryrdlock(&mutex), NECO_PERM);
    expect(neco_mutex_unlock(&mutex), NECO_PERM);
    expect(neco_mutex_destroy(&mutex), NECO_PERM);
    expect(neco_start(co_sync_mutex_fail, 0), NECO_OK);
}


void co_sync_mutex_rw_reader(int argc, void *argv[]) {
    assert(argc == 4);
    neco_mutex *mu = argv[0];
    int *x = argv[1];
    int *N = argv[2];
    int *rds = argv[3];
    expect(neco_mutex_trylock(mu), NECO_BUSY);
    expect(neco_mutex_lock(mu), NECO_OK);
    (*x)++;
    expect(neco_sleep(NECO_MILLISECOND), NECO_OK);
    expect(neco_mutex_unlock(mu), NECO_OK);
    expect(neco_sleep(NECO_MILLISECOND), NECO_OK);
    expect(neco_mutex_tryrdlock(mu), NECO_BUSY);
    expect(neco_mutex_rdlock(mu), NECO_OK);
    (*rds)++;
    assert(*x == *N*10);
    expect(neco_sleep(NECO_MILLISECOND*100), NECO_OK);
    expect(neco_mutex_unlock(mu), NECO_OK);
}

void co_sync_mutex_rw(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    // struct order order = { 0 };
    neco_mutex mutex;
    neco_mutex *mu = &mutex;
    expect(neco_mutex_init(mu), NECO_OK);
    int x = 0;
    int N = 20;
    int rds = 0;
    expect(neco_mutex_lock(mu), NECO_OK);
    for (int i = 0 ; i < N; i++) {
        expect(neco_start(co_sync_mutex_rw_reader, 4, &mutex, &x, &N, &rds), NECO_OK);
    }
    assert(x == 0);
    expect(neco_mutex_unlock(mu), NECO_OK);
    expect(neco_mutex_lock(mu), NECO_OK);
    assert(x == N);
    expect(neco_sleep(NECO_MILLISECOND*100), NECO_OK);
    x *= 10;
    expect(neco_mutex_unlock(mu), NECO_OK);
    expect(neco_sleep(NECO_MILLISECOND*200), NECO_OK);
    assert(rds == 20);
    expect(neco_mutex_destroy(mu), NECO_OK);
}

void test_sync_mutex_rw(void) {
    expect(neco_start(co_sync_mutex_rw, 0), NECO_OK);
}

void co_sync_cond_fail(int argc, void *argv[]) {
    assert(argc == 0);
    (void)argv;
    
    neco_cond cond;
    neco_mutex mutex;

    expect(neco_cond_init(&cond), NECO_OK);
    expect(neco_cond_wait(0, 0), NECO_INVAL);
    expect(neco_cond_wait(&cond, 0), NECO_INVAL);

    // poison the mutex
    memset(&mutex, 63, sizeof(neco_mutex));
    expect(neco_cond_wait(&cond, &mutex), NECO_PERM);
    memset(&mutex, 0, sizeof(neco_mutex));

    // poison the cond
    memset(&cond, 63, sizeof(neco_cond));
    expect(neco_cond_wait(&cond, &mutex), NECO_PERM);
    memset(&cond, 0, sizeof(neco_cond));

    expect(neco_cond_signal(0), NECO_INVAL);
    expect(neco_cond_broadcast(0), NECO_INVAL);

}

void test_sync_cond_fail(void) {
    neco_cond cond;
    neco_mutex mutex;
    expect(neco_cond_init(0), NECO_INVAL);
    expect(neco_cond_init(&cond), NECO_PERM);
    expect(neco_cond_wait(&cond, &mutex), NECO_PERM);
    expect(neco_cond_destroy(0), NECO_INVAL);
    expect(neco_start(co_sync_cond_fail, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_sync_mutex);
    do_test(test_sync_mutex_rw);
    do_test(test_sync_mutex_rw_order);
    do_test(test_sync_mutex_deadline);
    do_test(test_sync_mutex_fail);
    do_test(test_sync_waitgroup_one);
    do_test(test_sync_waitgroup_multi);
    do_test(test_sync_waitgroup_cancel);
    do_test(test_sync_waitgroup_fail);
    do_test(test_sync_cond_signal);
    do_test(test_sync_cond_signal_cancel);
    do_test(test_sync_cond_broadcast);
    do_test(test_sync_cond_deadline);
    do_test(test_sync_cond_fail);
}
