# Urchin

Fun toy programming language built around otters

Urchin compiles to native code through LLVM.

## Building

Requires LLVM (tested with 22), clang, and CMake.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

```sh
build/urchin examples/fib.urc -o fib && ./fib    # compile and run
build/urchin examples/fib.urc --emit-llvm -o -   # print the LLVM IR
```

## A quick swim through the language

Every value is a `pebble`, a 64-bit signed integer.

```
~~ comments float like otters ~~
dive fib(pebble n) -> pebble {
  if n < 2 { surface n; }
  surface fib(n - 1) + fib(n - 2);
}

paddle i from 0 to 10 {
  splash fib(i);
}
```

| Urchin                          | Meaning                                         |
| ------------------------------- | ----------------------------------------------- |
| `~~ ... ~~`                     | comment (may span lines)                        |
| `dive name(pebble a) -> pebble` | define a function                               |
| `surface expr;`                 | return from a dive                              |
| `splash expr;`                  | print a pebble on its own line                  |
| `stash x = expr;`               | declare a variable                              |
| `x = expr;`                     | reassign a variable                             |
| `if c { } else if d { } else { }` | branch (any non-zero pebble is true)          |
| `paddle i from a to b { }`      | loop `i` from `a` up to, but not including, `b` |

Operators: `+ - * / %`, comparisons `== != < > <= >=` (which give `1` or `0`), and unary `-`.

Statements outside of any dive run top to bottom when the program starts. Dives can call each other regardless of the order they're written in, and a dive that ends without surfacing returns `0`.

Shoutouts to Loy

MIT License
