#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make clean
make
out=$(printf 'pwd\nwhoami\necho pipe-test | tr a-z A-Z\necho redirect-test > /tmp/myshell-pa03-test.txt\ncat < /tmp/myshell-pa03-test.txt\nMSG="hello world"\necho $MSG\necho first ; echo second\nfalse\nif false\nthen\necho wrong-branch\nelse\necho right-branch\nfi\n' | ./bin/myshell)
for expected in 'PIPE-TEST' 'redirect-test' 'hello world' 'first' 'second' 'right-branch'; do
  if ! grep -Fq "$expected" <<<"$out"; then echo "FAIL: missing output: $expected" >&2; exit 1; fi
done
if grep -Fxq 'wrong-branch' <<<"$out"; then echo 'FAIL: wrong if branch executed' >&2; exit 1; fi
echo 'PASS: build, external commands, pipeline, redirection, chaining, variables, and if/else'
