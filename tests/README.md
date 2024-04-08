# Testing

Tests can be run from the project's root directory.

```bash
tests/run.sh
```

This will run all tests using the system's default compiler.

If [Clang](https://clang.llvm.org) is your compiler then you will also be 
provided with memory address sanitizing and code coverage.

If you need Valgrind you can provide `VALGRIND=1`.

### Examples

```bash
tests/run.sh                   # defaults
CC=clang-17 tests/run.sh       # use alternative compiler
CC=emcc tests/run.sh           # test WebAssembly using Emscripten
CC="zig cc" tests/run.sh       # test with the Zig C compiler
CFLAGS="-O3" tests/run.sh      # use custom cflags
NOSANS=1 tests/run.sh          # do not use sanitizers
VALGRIND=1 tests/run.sh        # use valgrind on all tests
```
