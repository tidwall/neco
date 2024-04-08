#include <pthread.h>
#include "tests.h"

void test_errors_short(void) {
    assert(strcmp(neco_shortstrerror(NECO_OK), "NECO_OK") == 0);
    assert(strcmp(neco_shortstrerror(NECO_ERROR), "NECO_ERROR") == 0);
    assert(strcmp(neco_shortstrerror(NECO_INVAL), "NECO_INVAL") == 0);
    assert(strcmp(neco_shortstrerror(NECO_PERM), "NECO_PERM") == 0);
    assert(strcmp(neco_shortstrerror(NECO_NOMEM), "NECO_NOMEM") == 0);
    assert(strcmp(neco_shortstrerror(NECO_NOSIGWATCH), "NECO_NOSIGWATCH") == 0);
    assert(strcmp(neco_shortstrerror(NECO_CLOSED), "NECO_CLOSED") == 0);
    assert(strcmp(neco_shortstrerror(NECO_EMPTY), "NECO_EMPTY") == 0);
    assert(strcmp(neco_shortstrerror(NECO_TIMEDOUT), "NECO_TIMEDOUT") == 0);
    assert(strcmp(neco_shortstrerror(NECO_CANCELED), "NECO_CANCELED") == 0);
    assert(strcmp(neco_shortstrerror(NECO_BUSY), "NECO_BUSY") == 0);
    assert(strcmp(neco_shortstrerror(NECO_NEGWAITGRP), "NECO_NEGWAITGRP") == 0);
    assert(strcmp(neco_shortstrerror(NECO_GAIERROR), "NECO_GAIERROR") == 0);
    assert(strcmp(neco_shortstrerror(NECO_NOTFOUND), "NECO_NOTFOUND") == 0);
    assert(strcmp(neco_shortstrerror(NECO_UNREADFAIL), "NECO_UNREADFAIL") == 0);
    assert(strcmp(neco_shortstrerror(NECO_PARTIALWRITE), "NECO_PARTIALWRITE") == 0);
    assert(strcmp(neco_shortstrerror(NECO_NOTGENERATOR), "NECO_NOTGENERATOR") == 0);
    assert(strcmp(neco_shortstrerror(NECO_NOTSUSPENDED), "NECO_NOTSUSPENDED") == 0);
    assert(strcmp(neco_shortstrerror(1), "UNKNOWN") == 0);
}

void test_errors_string(void) {
    assert(strcmp(neco_strerror(-1909), "Undefined error: -1909") == 0);
    assert(strcmp(neco_strerror(NECO_OK), "Success") == 0);
    assert(strcmp(neco_strerror(NECO_INVAL), strerror(EINVAL)) == 0);
    assert(strcmp(neco_strerror(NECO_PERM), strerror(EPERM)) == 0);
    assert(strcmp(neco_strerror(NECO_NOMEM), strerror(ENOMEM)) == 0);
    assert(strcmp(neco_strerror(NECO_NOSIGWATCH), "Not watching on a signal") == 0);
    assert(strcmp(neco_strerror(NECO_CLOSED), "Channel closed") == 0);
    assert(strcmp(neco_strerror(NECO_EMPTY), "Channel empty") == 0);
    assert(strcmp(neco_strerror(NECO_TIMEDOUT), strerror(ETIMEDOUT)) == 0);
    assert(strcmp(neco_strerror(NECO_CANCELED), strerror(ECANCELED)) == 0);
    assert(strcmp(neco_strerror(NECO_BUSY), strerror(EBUSY)) == 0);
    assert(strcmp(neco_strerror(NECO_NOTFOUND), "No such coroutine") == 0);
    assert(strcmp(neco_strerror(NECO_NEGWAITGRP), "Negative waitgroup counter") == 0);
    assert(strcmp(neco_strerror(NECO_UNREADFAIL), "Failed to unread byte") == 0);
    assert(strcmp(neco_strerror(NECO_PARTIALWRITE), "Failed to write all bytes") == 0);
    assert(strcmp(neco_strerror(NECO_NOTGENERATOR), "Coroutine is not a generator") == 0);
    assert(strcmp(neco_strerror(NECO_NOTSUSPENDED), "Coroutine is not suspended") == 0);
    errno = ENOMEM;
    assert(strcmp(neco_strerror(NECO_ERROR), strerror(ENOMEM)) == 0);
    neco_gai_errno = EAI_SOCKTYPE;
    assert(strcmp(neco_strerror(NECO_GAIERROR), gai_strerror(EAI_SOCKTYPE)) == 0);
    neco_gai_errno = EAI_SYSTEM;
    assert(strcmp(neco_strerror(NECO_GAIERROR), strerror(ENOMEM)) == 0);
}

void test_errors_conv(void) {
    errno = EPERM;
    assert(neco_errconv_from_sys() == NECO_PERM);
    errno = EINVAL;
    assert(neco_errconv_from_sys() == NECO_INVAL);
    errno = ENOMEM;
    assert(neco_errconv_from_sys() == NECO_NOMEM);
    errno = ECANCELED;
    assert(neco_errconv_from_sys() == NECO_CANCELED);
    errno = ETIMEDOUT;
    assert(neco_errconv_from_sys() == NECO_TIMEDOUT);
    errno = EAGAIN;
    assert(neco_errconv_from_sys() == NECO_ERROR);
    assert(neco_errconv_from_gai(EAI_MEMORY) == NECO_NOMEM);
    errno = EAGAIN;
    assert(neco_errconv_from_gai(EAI_SYSTEM) == NECO_ERROR);
    errno = ETIMEDOUT;
    assert(neco_errconv_from_gai(EAI_SYSTEM) == NECO_TIMEDOUT);
    neco_gai_errno = EAI_FAIL;
    assert(neco_errconv_from_gai(EAI_SOCKTYPE) == NECO_GAIERROR);
    assert(neco_gai_errno == EAI_SOCKTYPE);
    assert(neco_gai_lasterr() == neco_gai_errno);


    neco_errconv_to_sys(NECO_OK);
    assert(errno == 0);

    neco_errconv_to_sys(NECO_INVAL);
    assert(errno == EINVAL);
    neco_errconv_to_sys(NECO_PERM);
    assert(errno == EPERM);
    neco_errconv_to_sys(NECO_NOMEM);
    assert(errno == ENOMEM);
    neco_errconv_to_sys(NECO_CANCELED);
    assert(errno == ECANCELED);
    neco_errconv_to_sys(NECO_TIMEDOUT);
    assert(errno == ETIMEDOUT);
}

void test_errors_code(void) {
    errno = ETIMEDOUT;
    assert(neco_testcode(-1) == NECO_ERROR && errno == ETIMEDOUT);
    assert(neco_lasterr() == NECO_TIMEDOUT);


    assert(!neco_last_panic);
    neco_env_setpaniconerror(true);
    errno = EINVAL;
    assert(neco_testcode(-1) == NECO_ERROR && errno == EINVAL);
    assert(neco_lasterr() == NECO_INVAL);
    neco_env_setpaniconerror(false);
    assert(neco_last_panic);
    neco_last_panic = false;
}


int main(int argc, char **argv) {
    do_test(test_errors_short);
    do_test(test_errors_string);
    do_test(test_errors_conv);
    do_test(test_errors_code);
}
