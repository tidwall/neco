#include <signal.h>
#include <pthread.h>
#include "tests.h"

void co_chan_order_send(int argc, void *argv[]) {
    assert(argc == 2);
    struct neco_chan *ch = argv[0];
    int i = (int)((intptr_t)argv[1]);
    assert(neco_chan_send(ch, &i) == NECO_OK);
}

void co_chan_order(int argc, void *argv[]) {
    assert(argc == 2);
    int i = *(int*)argv[0];
    int j = *(int*)argv[1];
    struct neco_chan *ch;
    assert(neco_chan_make(&ch, sizeof(int), j) == NECO_OK);
    for (int k = 0; k < i; k++) {
        assert(neco_start(co_chan_order_send, 2, ch, k+1) == NECO_OK);
    }
    int x;
    for (int k = 0; k < i; k++) {
        assert(neco_chan_recv(ch, &x) == NECO_OK);
        assert(x == k+1);
    }
    assert(neco_chan_release(ch) == NECO_OK);
}

void test_chan_order(void) {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            assert(neco_start(co_chan_order, 2, &i, &j) == NECO_OK);
        }
    }
}

void co_chan_select_sender(int argc, void *argv[]) {
    assert(argc == 5);
    int i = (int)(intptr_t)argv[0];
    int x = (int)(intptr_t)argv[1];
    int N = (int)(intptr_t)argv[2];
    int *incr = argv[3];
    struct neco_chan *ch = argv[4];
    for (int j = 0; j < N; j++) {
        x += incr[j];
        assert(neco_chan_send(ch, &x) == NECO_OK);
        if ((i+j)%3 == 0) {
            neco_sleep(1e9/10);
        }
    }
}

void co_chan_select(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int incr[] = { 8987, 8701, 1923, 4451, 8192, 4421, 1010, 3304, 6104 };
    int N = (int)(sizeof(incr)/sizeof(int));
    int M = 3;
    struct neco_chan *ch0, *ch1, *ch2;
    assert(neco_chan_make(&ch0, sizeof(int), 0) == NECO_OK);
    assert(neco_chan_make(&ch1, sizeof(int), 0) == NECO_OK);
    assert(neco_chan_make(&ch2, sizeof(int), 0) == NECO_OK);
    assert(neco_start(co_chan_select_sender, 5, 0, 10000, N, incr, ch0) == NECO_OK);
    assert(neco_start(co_chan_select_sender, 5, 1, 20000, N, incr, ch1) == NECO_OK);
    assert(neco_start(co_chan_select_sender, 5, 2, 30000, N, incr, ch2) == NECO_OK);
    int idx0 = 0;
    int idx1 = 0;
    int idx2 = 0;
    int exp0 = 10000;
    int exp1 = 20000;
    int exp2 = 30000;
    int data0, data1, data2;
    bool ok0, ok1, ok2;

    for (int i = 0; i < N * M; i++) {
        int idx = neco_chan_select(3, ch0, ch1, ch2);
        switch (idx) {
        case 0:
            ok0 = neco_chan_case(ch0, &data0) == NECO_OK;
            exp0 += incr[idx0++];
            assert(ok0 && data0 == exp0);
            break;
        case 1:
            ok1 = neco_chan_case(ch1, &data1) == NECO_OK;
            exp1 += incr[idx1++];
            assert(ok1 && data1 == exp1);
            break;
        case 2:
            ok2 = neco_chan_case(ch2, &data2) == NECO_OK;
            exp2 += incr[idx2++];
            assert(ok2 && data2 == exp2);
            break;
        }
    }
    assert(neco_chan_release(ch0) == NECO_OK);
    assert(neco_chan_release(ch1) == NECO_OK);
    assert(neco_chan_release(ch2) == NECO_OK);
}

void co_chan_select_big_child(int argc, void *argv[]) {
    assert(argc == 1);
    struct neco_chan *ch = argv[0];
    expect(neco_chan_send(ch, &(int){8876}), NECO_OK);
    expect(neco_chan_release(ch), NECO_OK);
}

void co_chan_select_big_child_cancel(int argc, void *argv[]) {
    assert(argc == 1);
    int64_t id = *(int64_t*)argv[0];
    expect(neco_sleep(NECO_SECOND/4), NECO_OK);
    expect(neco_cancel(id), NECO_OK);
}


void co_chan_select_big(int argc, void *argv[]) {
    (void)argc; (void)argv;
    struct neco_chan *chs[20];
    for (int i = 0; i < 20; i++) {
        expect(neco_chan_make(&chs[i], sizeof(int), 0), NECO_OK);    
    }

    neco_fail_neco_malloc_counter = 1;
    expect(neco_chan_selectv_dl(20, chs, neco_now()+NECO_SECOND/4), NECO_NOMEM);
    expect(neco_chan_selectv_dl(20, chs, neco_now()+NECO_SECOND/4), NECO_TIMEDOUT);

    expect(neco_chan_retain(chs[12]), NECO_OK);
    expect(neco_start(co_chan_select_big_child, 1, chs[12]), NECO_OK);

    assert(neco_chan_selectv(20, chs) == 12);
    int x;
    expect(neco_chan_case(chs[12], &x), NECO_OK);
    assert(x == 8876);

    int64_t id = neco_getid();
    expect(neco_cancel(id), NECO_OK);
    expect(neco_chan_selectv(20, chs), NECO_CANCELED);

    expect(neco_start(co_chan_select_big_child_cancel, 1, &id), NECO_OK);
    expect(neco_chan_selectv(20, chs), NECO_CANCELED);

    for (int i = 0; i < 20; i++) {
        expect(neco_chan_release(chs[i]), NECO_OK);
    }
}

void co_chan_select_fail_perm_0(int argc, void *argv[]) {
    assert(argc == 1);
    neco_chan **ch1 = argv[0];
    expect(neco_chan_make(ch1, 0, 0), NECO_OK);
    expect(neco_sleep(NECO_SECOND/2), NECO_OK);
    expect(neco_chan_release(*ch1), NECO_OK);
}

void co_chan_select_fail_perm_1(int argc, void *argv[]) {
    assert(argc == 1);
    neco_chan **ch1 = argv[0];
    expect(neco_sleep(NECO_SECOND/4), NECO_OK);
    expect(neco_chan_select(1, *ch1), NECO_PERM);
    expect(neco_chan_select(1, NULL), NECO_INVAL);
    return;
}

void *th_chan_select_fail_perm_0(void *arg) {
    assert(neco_start(co_chan_select_fail_perm_0, 1, arg) == NECO_OK);
    return 0;
}

void *th_chan_select_fail_perm_1(void *arg) {
    assert(neco_start(co_chan_select_fail_perm_1, 1, arg) == NECO_OK);
    return 0;
}

void test_chan_select(void) {
    assert(neco_start(co_chan_select, 0) == NECO_OK);
    assert(neco_start(co_chan_select_big, 0) == NECO_OK);
#ifdef __EMSCRIPTEN__
    return;
#endif
    neco_chan *ch1;
    pthread_t th1, th2;
    assert(pthread_create(&th1, 0, th_chan_select_fail_perm_0, &ch1) == 0);
    assert(pthread_create(&th2, 0, th_chan_select_fail_perm_1, &ch1) == 0);
    assert(pthread_join(th1, 0) == 0);
    assert(pthread_join(th2, 0) == 0);
    
}

void co_chan_close_sender(int argc, void *argv[]) {
    assert(argc == 1);
    struct neco_chan *ch = argv[0];
    assert(neco_chan_send(ch, 0) == NECO_OK);
    assert(neco_chan_close(ch) == NECO_OK);
    assert(neco_chan_close(ch) == NECO_CLOSED);
    assert(neco_chan_send(ch, 0) == NECO_CLOSED);
}

void co_chan_close_sender_send(int argc, void *argv[]) {
    assert(argc == 2);
    struct neco_chan *ch = argv[0];
    int x = *(int*)argv[1];
    expect(neco_chan_send(ch, &x), NECO_OK);
    expect(neco_chan_release(ch), NECO_OK);
}

void co_chan_close_sender_recv(int argc, void *argv[]) {
    assert(argc == 1);
    struct neco_chan *ch = argv[0];
    int x;
    expect(neco_chan_recv(ch, &x), NECO_OK);
    assert(x == 1289);
    expect(neco_chan_recv(ch, &x), NECO_OK);
    assert(x == 2938);
    expect(neco_chan_recv(ch, &x), NECO_CLOSED);
    expect(neco_chan_release(ch), NECO_OK);
}

void co_chan_close_send(int argc, void *argv[]) {
    (void)argc; (void)argv;
    struct neco_chan *ch;
    assert(neco_chan_make(&ch, 0, 0) == NECO_OK);
    assert(neco_start(co_chan_close_sender, 1, ch) == NECO_OK);
    assert(neco_chan_recv(ch, 0) == NECO_OK);
    assert(neco_chan_recv(ch, 0) == NECO_CLOSED);
    assert(neco_chan_recv(ch, 0) == NECO_CLOSED);
    assert(neco_chan_release(ch) == NECO_OK);

    /// 

    assert(neco_chan_make(&ch, sizeof(int), 0) == NECO_OK);
    assert(neco_chan_retain(ch) == NECO_OK);
    assert(neco_start(co_chan_close_sender_send, 2, ch, &(int){ 1289 }) == NECO_OK);
    assert(neco_chan_retain(ch) == NECO_OK);
    assert(neco_start(co_chan_close_sender_send, 2, ch, &(int){ 2938 }) == NECO_OK);
    assert(neco_chan_close(ch) == NECO_OK);
    assert(neco_chan_retain(ch) == NECO_OK);
    assert(neco_start(co_chan_close_sender_recv, 1, ch) == NECO_OK);
    assert(neco_chan_release(ch) == NECO_OK);
}

void test_chan_close_send(void) {
    expect(neco_start(co_chan_close_send, 0), NECO_OK);
}

void co_chan_close_recv_waiting(int argc, void *argv[]) {
    assert(argc == 1);
    struct neco_chan *ch = argv[0];
    expect(neco_sleep(NECO_SECOND/10), NECO_OK);
    expect(neco_chan_close(ch), NECO_OK);
    expect(neco_chan_release(ch), NECO_OK);
}

void co_chan_close_recv(int argc, void *argv[]) {
    (void)argc; (void)argv;
    struct neco_chan *ch;
    expect(neco_chan_make(&ch, sizeof(int), 0), NECO_OK);
    expect(neco_chan_retain(ch), NECO_OK);
    expect(neco_start(co_chan_close_recv_waiting, 1, ch), NECO_OK);
    expect(neco_chan_recv(ch, &(int){1}), NECO_CLOSED);
    expect(neco_chan_release(ch), NECO_OK);
}

void test_chan_close_recv(void) {
    expect(neco_start(co_chan_close_recv, 0), NECO_OK);
}

void co_chan_close_select_waiting(int argc, void *argv[]) {
    assert(argc == 1);
    struct neco_chan *ch = argv[0];
    expect(neco_sleep(NECO_SECOND/10), NECO_OK);
    expect(neco_chan_close(ch), NECO_OK);
    expect(neco_chan_release(ch), NECO_OK);
}

void co_chan_close_select(int argc, void *argv[]) {
    (void)argc; (void)argv;
    struct neco_chan *ch;
    expect(neco_chan_make(&ch, sizeof(int), 0), NECO_OK);
    expect(neco_chan_retain(ch), NECO_OK);
    expect(neco_start(co_chan_close_select_waiting, 1, ch), NECO_OK);
    // select-case with the same channel twice is intentional.
    assert(neco_chan_select(2, ch, ch) == 0);
    expect(neco_chan_case(ch, &(int){1}), NECO_CLOSED);
    expect(neco_chan_release(ch), NECO_OK);
}

void test_chan_close_select(void) {
    expect(neco_start(co_chan_close_select, 0), NECO_OK);
}

void co_chan_broadcast_recv(int argc, void *argv[]) {
    assert(argc == 1);
    struct neco_chan *ch = argv[0];
    int x;
    assert(neco_chan_recv(ch, &x) == NECO_OK);
    assert(x == 9182);
    assert(neco_chan_send(ch, &(int){5555}) == NECO_OK);
    assert(neco_chan_release(ch) == NECO_OK);
}

void co_chan_broadcast(int argc, void *argv[]) {
    (void)argc; (void)argv;
    struct neco_chan *ch;
    assert(neco_chan_make(&ch, sizeof(int), 0) == NECO_OK);
    int N = 10;
    for (int i = 0; i < N; i++) {
        assert(neco_chan_retain(ch) == NECO_OK);
        assert(neco_start(co_chan_broadcast_recv, 1, ch) == NECO_OK);
    }
    int sent = neco_chan_broadcast(ch, &(int){9182});
    assert(sent == 10);
    for (int i = 0; i < N; i++) {
        int x;
        assert(neco_chan_recv(ch, &x) == NECO_OK);
        assert(x == 5555);
    }
    assert(neco_chan_release(ch) == NECO_OK);
}

void test_chan_broadcast(void) {
    assert(neco_start(co_chan_broadcast, 0) == NECO_OK);
}

void co_chan_deadline1(int argc, void *argv[]) {
    assert(argc == 2);
    struct neco_chan *ch = argv[0];
    int other_id = *(int*)argv[1];
    int id = neco_getid();

    int x = 0;
    expect(neco_chan_recv_dl(ch, &x, neco_now() + NECO_SECOND/8), NECO_OK);
    assert(x == 9183);
    expect(neco_cancel(other_id), NECO_OK);
    expect(neco_chan_send(ch, &id), NECO_OK);
    expect(neco_chan_send(ch, &id), NECO_CANCELED);
    expect(neco_chan_release(ch), NECO_OK);
}

void co_chan_deadline(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int id = neco_getid();
    struct neco_chan *ch;
    expect(neco_chan_make(&ch, sizeof(int), 0), NECO_OK);
    expect(neco_chan_send_dl(ch, &(int){9182}, neco_now() + NECO_SECOND/8), NECO_TIMEDOUT);
    int x = 0;
    expect(neco_chan_recv_dl(ch, &x, neco_now() + NECO_SECOND/8), NECO_TIMEDOUT);
    expect(neco_chan_select_dl(neco_now() + NECO_SECOND/8, 1, ch), NECO_TIMEDOUT);
    expect(neco_chan_select_dl(neco_now() + NECO_SECOND/8, 0), NECO_TIMEDOUT);

    expect(neco_chan_retain(ch), NECO_OK);
    expect(neco_start(co_chan_deadline1, 2, ch, &id), NECO_OK);
    expect(neco_chan_send_dl(ch, &(int){9183}, neco_now() + NECO_SECOND/8), NECO_OK);
    expect(neco_chan_recv(ch, &x), NECO_CANCELED);
    int other_id;
    expect(neco_chan_recv(ch, &other_id), NECO_OK);
    expect(neco_cancel(other_id), NECO_OK);
    expect(neco_chan_release(ch), NECO_OK);
}

void test_chan_deadline(void) {
    expect(neco_start(co_chan_deadline, 0), NECO_OK);
}

void co_chan_cancel1(int argc, void *argv[]) {
    assert(argc == 2);
    struct neco_chan *ch = argv[0];
    int other_id = *(int*)argv[1];
    expect(neco_sleep(NECO_SECOND/10), NECO_OK);
    expect(neco_cancel(other_id), NECO_OK);
    expect(neco_chan_release(ch), NECO_OK);
}

void co_chan_cancel(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int id = neco_getid();
    struct neco_chan *ch;
    expect(neco_chan_make(&ch, sizeof(int), 0), NECO_OK);
    expect(neco_chan_retain(ch), NECO_OK);
    expect(neco_start(co_chan_cancel1, 2, ch, &id), NECO_OK);
    expect(neco_chan_send(ch, &(int){1}), NECO_CANCELED);
    expect(neco_chan_retain(ch), NECO_OK);
    expect(neco_start(co_chan_cancel1, 2, ch, &id), NECO_OK);
    expect(neco_chan_recv(ch, &(int){1}), NECO_CANCELED);
    expect(neco_chan_release(ch), NECO_OK);
}

void test_chan_cancel(void) {
    expect(neco_start(co_chan_cancel, 0), NECO_OK);
}

void co_chan_fail(int argc, void *argv[]) {
    (void)argc; (void)argv;
    expect(neco_chan_make(0, 0, 0), NECO_INVAL);
    neco_chan *ch;
    expect(neco_chan_make(&ch, SIZE_MAX, 0), NECO_INVAL);
    neco_fail_neco_malloc_counter = 1;
    expect(neco_chan_make(&ch, 10, 0), NECO_NOMEM);
    expect(neco_chan_release(0), NECO_INVAL);

    neco_fail_neco_malloc_counter = 1;
    expect(neco_chan_make(&ch, 0, 0), NECO_NOMEM);

    expect(neco_chan_make(&ch, 0, 0), NECO_OK);
    neco_fail_neco_realloc_counter = 1;

    expect(neco_chan_close(ch), NECO_OK);
    expect(neco_chan_case(ch, 0), NECO_CLOSED);
    expect(neco_chan_release(ch), NECO_OK);
    expect(neco_chan_select(-1), NECO_INVAL);

    expect(neco_chan_make(&ch, (size_t)INT_MAX+1, 0), NECO_INVAL);
    expect(neco_chan_make(&ch, 0, (size_t)INT_MAX+1), NECO_INVAL);
}


void test_chan_fail(void) {
    neco_chan *ch;
    expect(neco_chan_make(&ch, 0, 0), NECO_PERM);
    expect(neco_chan_retain(0), NECO_INVAL);
    expect(neco_chan_retain((neco_chan*)1), NECO_PERM);
    expect(neco_chan_release((neco_chan*)1), NECO_PERM);
    expect(neco_chan_send(0, 0), NECO_INVAL);
    expect(neco_chan_send((neco_chan*)1, 0), NECO_PERM);
    expect(neco_chan_recv(0, 0), NECO_INVAL);
    expect(neco_chan_recv((neco_chan*)1, 0), NECO_PERM);
    expect(neco_chan_close(0), NECO_INVAL);
    expect(neco_chan_close((neco_chan*)1), NECO_PERM);
    expect(neco_chan_case(0, 0), NECO_INVAL);
    expect(neco_chan_case((neco_chan*)1, 0), NECO_PERM);
    expect(neco_start(co_chan_fail, 0), NECO_OK);
    expect(neco_chan_select(1), NECO_PERM);
}

void co_chan_zchanpool(int argc, void *argv[]) {
    (void)argc; (void)argv;
    int N = 500;
    neco_chan **chs = malloc(sizeof(neco_chan*)*N);
    assert(chs);
    for (int i = 0; i < N/2; i++) {
        expect(neco_chan_make(&chs[i], 0, 0), NECO_OK);
    }
    for (int i = 0; i < N/4; i++) {
        expect(neco_chan_release(chs[i]), NECO_OK);
    }
    for (int i = N/2; i < N; i++) {
        expect(neco_chan_make(&chs[i], 0, 0), NECO_OK);
    }
    for (int i = N/4; i < N; i++) {
        expect(neco_chan_release(chs[i]), NECO_OK);
    }
    free(chs);
}

void test_chan_zchanpool(void) {
    expect(neco_start(co_chan_zchanpool, 0), NECO_OK);
}

void co_chan_tryrecv(int argc, void *argv[]) {
    (void)argc; (void)argv;
    neco_chan *ch;
    int x;
    expect(neco_chan_make(&ch, sizeof(int), 1), NECO_OK);
    expect(neco_chan_tryrecv(ch, &x), NECO_EMPTY);
    expect(neco_chan_send(ch, &(int){1}), NECO_OK);
    expect(neco_chan_close(ch), NECO_OK);
    expect(neco_chan_tryrecv(ch, &x), NECO_OK);
    assert(x == 1);
    expect(neco_chan_tryrecv(ch, &x), NECO_CLOSED);
    expect(neco_chan_release(ch), NECO_OK);


    expect(neco_chan_make(&ch, sizeof(int), 1), NECO_OK);
    expect(neco_chan_tryrecv(ch, &x), NECO_EMPTY);
    expect(neco_chan_send(ch, &(int){1}), NECO_OK);
    expect(neco_chan_tryrecv(ch, &x), NECO_OK);
    assert(x == 1);
    expect(neco_chan_close(ch), NECO_OK);
    expect(neco_chan_tryrecv(ch, &x), NECO_CLOSED);
    expect(neco_chan_release(ch), NECO_OK);
}

void test_chan_tryrecv(void) {
    expect(neco_start(co_chan_tryrecv, 0), NECO_OK);
}

void co_chan_tryselect(int argc, void *argv[]) {
    (void)argc; (void)argv;
    neco_chan *ch;
    expect(neco_chan_make(&ch, sizeof(int), 1), NECO_OK);
    expect(neco_chan_tryselect(1, ch), NECO_EMPTY);
    expect(neco_chan_tryselectv(1, &(neco_chan*){ch}), NECO_EMPTY);
    expect(neco_chan_release(ch), NECO_OK);
}


void test_chan_tryselect(void) {
    expect(neco_start(co_chan_tryselect, 0), NECO_OK);
}

int main(int argc, char **argv) {
    do_test(test_chan_order);
    do_test(test_chan_select);
    do_test(test_chan_close_send);
    do_test(test_chan_close_recv);
    do_test(test_chan_close_select);
    do_test(test_chan_tryrecv);
    do_test(test_chan_tryselect);
    do_test(test_chan_broadcast);
    do_test(test_chan_deadline);
    do_test(test_chan_cancel);
    do_test(test_chan_zchanpool);
    do_test(test_chan_fail);
}
