#!/bin/bash

# Statically embeds the dependencies into the root source file
# Each source file must have a // BEGIN file.c and // END file.c

SOURCE_FILE=../neco.c
EMBEDDED_FILES=("sco.c" "stack.c" "aat.h" "worker.c")

set -e
cd $(dirname "${BASH_SOURCE[0]}")

cp $SOURCE_FILE tmp.1

embed() {
    BEGIN=$(cat tmp.1 | grep -n -w $"// BEGIN $1" | cut -d : -f 1)
    END=$(cat tmp.1 | grep -n -w $"// END $1" | cut -d : -f 1)
    if [[ "$BEGIN" == "" ]]; then
        echo "missing // BEGIN $1"
        exit 1
    elif [[ "$END" == "" ]]; then
        echo "missing // END $1"
        exit 1
    elif [[ $BEGIN -gt $END ]]; then
        echo "missing // BEGIN $1 must be after // END $1"
        exit 1
    fi
    sed -n 1,${BEGIN}p tmp.1 > tmp.2
    cat $1 >> tmp.2
    sed -n ${END},999999999999p tmp.1 >> tmp.2
    mv tmp.2 tmp.1
}

for file in "${EMBEDDED_FILES[@]}"; do
    embed "$file"
done

# overwrite the original source file
mv tmp.1 $SOURCE_FILE