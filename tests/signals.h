#ifdef NECO_TESTING

#define print_stacktrace(a, b)

// During testing, track the exit but do not actually exit.
__thread int neco_last_sigexitnow = 0;
static void sigexitnow(int signo) {
    neco_last_sigexitnow = signo;
    if (signo == SIGINT || signo == SIGABRT || signo == SIGSEGV || 
        signo == SIGBUS) 
    {
        fprintf(stderr, "%s\n", signo == SIGINT ? "" : strsignal0(signo));
        _Exit(128+signo);
    }
}

#endif
