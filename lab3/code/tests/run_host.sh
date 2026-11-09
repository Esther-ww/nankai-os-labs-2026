#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/prepare_host.py
test_bin=$(mktemp "${TMPDIR:-/tmp}/lab3-c-host.XXXXXX")
trap 'rm -f "$test_bin"' EXIT HUP INT TERM
cc -std=gnu99 -Wall -Wextra -Werror -fno-builtin -Icode/libs -Icode/kern/trap tests/host_exception.c -o "$test_bin"
"$test_bin"
