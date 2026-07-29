#!/bin/sh
# Golden tests: transpile each .sic file, compile it, run it (feeding
# <name>.in if present), and compare stdout against <name>.out.
set -u
cd "$(dirname "$0")/.."

mkdir -p tests/out
pass=0
fail=0

for src in examples/*.sic tests/cases/*.sic; do
  [ -e "$src" ] || continue
  name=$(basename "$src" .sic)
  expected="${src%.sic}.out"
  input="${src%.sic}.in"
  cfile="tests/out/$name.c"
  bin="tests/out/$name"

  if ! ./sicc "$src" "$cfile"; then
    echo "FAIL $name (transpile)"
    fail=$((fail + 1))
    continue
  fi

  if ! ${CC:-cc} -Wall -o "$bin" "$cfile" 2>"tests/out/$name.cc.log"; then
    echo "FAIL $name (compile, see tests/out/$name.cc.log)"
    fail=$((fail + 1))
    continue
  fi

  if [ -f "$input" ]; then
    actual=$("$bin" <"$input")
  else
    actual=$("$bin")
  fi

  if [ "$actual" = "$(cat "$expected")" ]; then
    pass=$((pass + 1))
  else
    echo "FAIL $name (output)"
    echo "$actual" >"tests/out/$name.actual"
    diff "$expected" "tests/out/$name.actual" | head -20
    fail=$((fail + 1))
  fi
done

# Error tests: transpilation of tests/errors/*.sic must fail, and stderr
# must contain the sibling .err file.
for src in tests/errors/*.sic; do
  [ -e "$src" ] || continue
  name=$(basename "$src" .sic)

  if ./sicc "$src" "tests/out/err-$name.c" 2>"tests/out/err-$name.log"; then
    echo "FAIL $name (expected a transpile error)"
    fail=$((fail + 1))
    continue
  fi

  if grep -qF "$(cat "${src%.sic}.err")" "tests/out/err-$name.log"; then
    pass=$((pass + 1))
  else
    echo "FAIL $name (wrong error)"
    echo "  expected: $(cat "${src%.sic}.err")"
    echo "  got:      $(cat "tests/out/err-$name.log")"
    fail=$((fail + 1))
  fi
done

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
