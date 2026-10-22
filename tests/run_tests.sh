#!/usr/bin/env bash
# usage: run_tests.sh <path-to-urchin>   (run from the tests/ directory)
set -u
urchin="$1"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
failed=0

for src in *.urc; do
  name="${src%.urc}"
  if ! "$urchin" "$src" -o "$tmp/$name" || ! diff -u "$name.out" <("$tmp/$name"); then
    echo "FAIL: $src"; failed=1
  fi
done

for src in errors/*.urc; do
  expected="$(cat "${src%.urc}.err")"
  actual="$(cd errors && "$urchin" "$(basename "$src")" -o "$tmp/err" 2>&1 >/dev/null | head -1)"
  if [ "$actual" != "$expected" ]; then
    echo "FAIL: $src"; echo "  expected: $expected"; echo "  actual:   $actual"; failed=1
  fi
done

[ "$failed" = 0 ] && echo "all tests passed 🦦"
exit "$failed"
