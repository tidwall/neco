#!/bin/bash

# Generate API.md file

NAMESPACE=neco_
SRCDIR=..

set -e
cd $(dirname "${BASH_SOURCE[0]}")/..

rm -rf tmp && mkdir tmp && cd tmp

echo "GENERATE_XML = YES" >> .doxygen.cfg
echo "EXTRACT_ALL = YES" >> .doxygen.cfg
echo "ENABLE_PREPROCESSING=YES" >> .doxygen.cfg
echo "GENERATE_HTML = NO" >> .doxygen.cfg
echo "GENERATE_LATEX = NO" >> .doxygen.cfg
echo "FILE_PATTERNS = *.h *.c" >> .doxygen.cfg
echo "OPTIMIZE_OUTPUT_FOR_C = YES" >> .doxygen.cfg
echo "INPUT = $SRCDIR/.." >> .doxygen.cfg

# First pass to xml
doxygen .doxygen.cfg

# Run the doxygen-md tool
cd ../tools/doxygen-md
go build .
mv doxygen-md ../../tmp
cd ../../tmp
echo "<!--- AUTOMATICALLY GENERATED: DO NOT EDIT --->" >> API.md
echo "" >> API.md
cat ../assets/API_head.md >> API.md
./doxygen-md -ns "$NAMESPACE" >> API.md
cat ../assets/API_foot.md >> API.md
mv API.md ../
cd ..
