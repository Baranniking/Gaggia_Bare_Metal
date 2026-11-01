#!/bin/sh
# Build and run the PC test for Core/Src/protocol.c
set -e
cd "$(dirname "$0")"
${CC:-gcc} -std=c11 -Wall -Wextra -include stubs/hal_stubs.h -I../Core/Inc test_protocol.c -lm -o test_protocol
./test_protocol
