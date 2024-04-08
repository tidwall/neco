#!/usr/bin/env bash

# ./run.sh [<test-name>]

set -e
cd $(dirname "${BASH_SOURCE[0]}")

OK=0
finish() { 
    rm -fr *.o
    rm -fr *.out
    rm -fr *.test
    rm -fr *.profraw
    rm -fr *.dSYM
    rm -fr *.profdata
    rm -fr *.c.worker.js
    rm -fr *.c.wasm
    if [[ "$OK" != "1" ]]; then
        echo "FAIL"
    fi
}
trap finish EXIT

echo_wrapped() {
    # print an arguments list with line wrapping
    line=""
    for i in $(seq 1 "$#"); do
        line2="$line ${!i}"
        if [[ "${#line2}" -gt 70 ]]; then
            echo "$line \\"
            line="    "
        fi
        line="$line${!i} "
    done
    echo "$line"
}

if [[ -f /proc/cpuinfo ]]; then
    cpu="`cat /proc/cpuinfo | grep "model name" | uniq | cut -f3- -d ' ' | xargs`"
elif [[ "`which system_profiler`" != "" ]]; then
    cpu="`system_profiler SPHardwareDataType | grep Chip | uniq | cut -f2- -d ':' | xargs`"
fi
if [[ "$CC" == "" ]]; then
    CC=cc
fi
if [[ "$1" != "bench" ]]; then
    CFLAGS="-O0 -g3 -Wall -Wextra -fstrict-aliasing $CFLAGS"
    CCVERSHEAD="$($CC --version | head -n 1)"
    if [[ "$CCVERSHEAD" == "" ]]; then
        exit 1
    fi

    if [[ "$CCVERSHEAD" == *"clang"* ]]; then
        CLANGVERS="$(echo "$CCVERSHEAD" | awk '{print $4}' | awk -F'[ .]+' '{print $1}')"
    fi

    if [[ "$CC" == *"zig"* ]]; then
        # echo Zig does not support asans
        NOSANS=1
    fi

    # Use address sanitizer if possible
    if [[ "$NOSANS" != "1" && "$CLANGVERS" -gt "13" ]]; then
        CFLAGS="$CFLAGS -fno-omit-frame-pointer"
        CFLAGS="$CFLAGS -fprofile-instr-generate"
        CFLAGS="$CFLAGS -fcoverage-mapping"
        CFLAGS="$CFLAGS -fsanitize=address"
        CFLAGS="$CFLAGS -fsanitize=undefined"
        if [[ "$1" == "fuzz" ]]; then
            CFLAGS="$CFLAGS -fsanitize=fuzzer"
        fi
        CFLAGS="$CFLAGS -fno-inline"
        CFLAGS="$CFLAGS -pedantic"
        WITHSANS=1

        if [[ "$(which llvm-cov-$CLANGVERS)" != "" && "$(which llvm-profdata-$CLANGVERS)" != "" ]]; then
            LLVM_COV="llvm-cov-$CLANGVERS"
            LLVM_PROFDATA="llvm-profdata-$CLANGVERS"
        else
            LLVM_COV="llvm-cov"
            LLVM_PROFDATA="llvm-profdata"
        fi

        if [[ "$(which $LLVM_PROFDATA)" != "" && "$(which $LLVM_COV)" != "" ]]; then
            COV_VERS="$($LLVM_COV --version | awk '{print $4}' | awk -F'[ .]+' '{print $1}')"
            if [[ "$COV_VERS" -gt "15" ]]; then
                WITHCOV=1
            fi
        fi
    fi
    CFLAGS=${CFLAGS:-"-O0 -g3 -Wall -Wextra -fstrict-aliasing"}
else
    CFLAGS=${CFLAGS:-"-O3"}
fi
# CFLAGS="$CFLAGS -pthread"
CFLAGS="$CFLAGS -DNECO_TESTING -DNECO_BT_SOURCE_INFO"
if [[ "$VALGRIND" == "1" ]]; then
    CFLAGS="$CFLAGS -DLLCO_VALGRIND"
fi
if [[ "$CC" == "emcc" ]]; then
    # Running emscripten
    CFLAGS="$CFLAGS -sASYNCIFY -sALLOW_MEMORY_GROWTH"
    CFLAGS="$CFLAGS -Wno-limited-postlink-optimizations"
    CFLAGS="$CFLAGS -Wno-unused-command-line-argument"
    CFLAGS="$CFLAGS -Wno-pthreads-mem-growth"
elif [[ "`uname`" == *"_NT-"* ]]; then
    # Running on Windows (MSYS)
    LFLAGS2="$LFLAGS2 -lws2_32"
fi

if [[ "$CC" == *"zig"* ]]; then
    # Without -O3, 'zig cc' has quirks issues on Mac OS.
    CFLAGS="$CFLAGS -O3"
    # Stack unwinding is not supported yet
    # https://github.com/ziglang/zig/issues/9046
    CFLAGS="$CFLAGS -DLLCO_NOUNWIND"
fi



CC=${CC:-cc}
echo "CC: $CC"
echo "CFLAGS: $CFLAGS"
echo "OS: `uname`"
echo "CPU: $cpu"

which "$CC" | true
cc2="$(readlink -f "`which "$CC" | true`" | true)"
if [[ "$cc2" == "" ]]; then
    cc2="$CC"
fi
echo Compiler: $($cc2 --version | head -n 1)
if [[ "$NOSANS" == "1" ]]; then
    echo "Sanitizers disabled"
fi
echo "Neco Commit: `git rev-parse --short HEAD 2>&1 || true`"

if [[ "$1" == "bench" ]]; then
    echo "BENCHMARKING..."
    if [[ "$MARKDOWN" == "1" ]]; then
        echo_wrapped $CC $CFLAGS ../neco.c bench.c
    else
        echo $CC $CFLAGS ../neco.c bench.c
    fi
    $CC $CFLAGS ../neco.c bench.c
    ./a.out $@
    OK=1
elif [[ "$1" == "fuzz" ]]; then
    echo "FUZZING..."
    echo $CC $CFLAGS ../neco.c fuzz.c
    $CC $CFLAGS ../neco.c fuzz.c
    MallocNanoZone=0 ./a.out corpus/ seeds/ # -jobs=8 # "${@:2}"
else
    if [[ "$WITHCOV" == "1" ]]; then
        echo "Code coverage: on"
    else 
        echo "Code coverage: off"
    fi
    echo "For benchmarks: 'run.sh bench'"
    echo "TESTING..."
    DEPS_SRCS="../deps/sco.c ../deps/stack.c ../deps/worker.c"
    DEPS_OBJS="sco.o stack.o worker.o"
    rm -f neco.o $DEPS_OBJS
    for f in *; do 
        if [[ "$f" != test_*.c ]]; then continue; fi 
        if [[ "$1" == test_* ]]; then 
            p=$1
            if [[ "$1" == test_*_* ]]; then
                # fast track matching prefix with two underscores
                suf=${1:5}
                rest=${suf#*_}
                idx=$((${#suf}-${#rest}-${#_}+4))
                p=${1:0:idx}
            fi 
            if [[ "$f" != $p* ]]; then continue; fi
        fi

        # Compile dependencies and neco.o
        if [[ ! -f "neco.o" ]]; then
            # Compile each dependency individually
            DEPS_SRCS_ARR=($DEPS_SRCS)
            for file in "${DEPS_SRCS_ARR[@]}"; do
                $CC $CFLAGS -Wunused-function -c $file
            done
            if [[ "$AMALGA" == "1" ]]; then
                echo "AMALGA=1"
                $CC $CFLAGS -c ../neco.c 
            else
                $CC $CFLAGS -DNECO_NOAMALGA -c ../neco.c
            fi
        fi

        # Compile the test
        if [[ "$AMALGA" == "1" ]]; then
            $CC $CFLAGS -o $f.test neco.o $LFLAGS2 $f -lm
        else
            $CC $CFLAGS -DNECO_NOAMALGA -o $f.test neco.o $DEPS_OBJS $LFLAGS2 $f -lm
        fi

        # Run the program
        if [[ "$WITHSANS" == "1" ]]; then
            export MallocNanoZone=0
            export ASAN_OPTIONS=detect_stack_use_after_return=1
        fi
        if [[ "$WITHCOV" == "1" ]]; then
            export LLVM_PROFILE_FILE="$f.profraw"
        fi
        if [[ "$VALGRIND" == "1" ]]; then
            valgrind --leak-check=yes ./$f.test $@
        elif [[ "$CC" == "emcc" ]]; then
            node ./$f.test $@
        else
            ./$f.test $@
        fi

    done
    OK=1
    echo "OK"

    if [[ "$COVREGIONS" == "" ]]; then 
        COVREGIONS="false"
    fi

    if [[ "$WITHCOV" == "1" ]]; then
        $LLVM_PROFDATA merge *.profraw -o test.profdata
        $LLVM_COV report *.test ../neco.c -ignore-filename-regex=.test. \
            -j=4 \
            -show-functions=true \
            -instr-profile=test.profdata > /tmp/test.cov.sum.txt
        # echo coverage: $(cat /tmp/test.cov.sum.txt | grep TOTAL | awk '{ print $NF }')
        echo covered: "$(cat /tmp/test.cov.sum.txt | grep TOTAL | awk '{ print $7; }') (lines)"
        $LLVM_COV show *.test ../neco.c -ignore-filename-regex=.test. \
            -j=4 \
            -show-regions=true \
            -show-expansions=$COVREGIONS \
            -show-line-counts-or-regions=true \
            -instr-profile=test.profdata -format=html > /tmp/test.cov.html
        echo "details: file:///tmp/test.cov.html"
        echo "summary: file:///tmp/test.cov.sum.txt"
    elif [[ "$WITHCOV" == "0" ]]; then
        echo "code coverage not a available"
        echo "install llvm-profdata and use clang for coverage"
    fi

fi
