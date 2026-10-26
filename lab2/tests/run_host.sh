#!/bin/sh
# Linux 宿主机测试；不替代 RISC-V 内核验证。
set -eu
cd "$(dirname "$0")/.."
cc=${CC:-gcc}
test_dir=$(mktemp -d)
trap 'rm -f "$test_dir/host_buddy"; rmdir "$test_dir"' EXIT HUP INT TERM
"$cc" -std=gnu99 -O1 -Wall -Wextra -Werror -Wno-unused-function -fno-builtin \
  -I kern/mm -I kern/debug -I libs \
  tests/host_buddy.c -o "$test_dir/host_buddy"
"$test_dir/host_buddy"
for case_name in n z w i d r o u a x; do
  status=0
  "$test_dir/host_buddy" "$case_name" || status=$?
  if [ "$status" -ne 77 ]; then
    echo "FAIL: invalid-operation case $case_name, exit $status"
    exit 1
  fi
done
echo 'ALL HOST TESTS PASSED'
