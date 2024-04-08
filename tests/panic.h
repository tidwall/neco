#ifdef NECO_TESTING

__thread bool neco_last_panic = false;
#define panic(fmt, ...) \
    neco_last_panic = true;

// print a string, function, file, and line number.
#define pwhere(str) \
    fprintf(stderr, "%s, function %s, file %s, line %d.\n", \
        (str), __func__, __FILE__, __LINE__)

// define an unreachable section of code. 
// This will not signal like __builtin_unreachable()
#define unreachable() \
    pwhere("Unreachable"); \
    abort()

// must is like assert but it cannot be disabled with -DNDEBUG
// This is primarily used for a syscall that, when provided valid paramaters, 
// should never fail. Yet if it does fail we want the 411.
#define must(cond) \
    if (!(cond)) { \
        pwhere("Must failed: " #cond); \
        perror("System error:"); \
        abort(); \
    } \
    (void)0

#endif
