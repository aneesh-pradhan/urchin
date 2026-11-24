# Urchin

[![CI](https://github.com/aneesh-pradhan/urchin/actions/workflows/ci.yml/badge.svg)](https://github.com/aneesh-pradhan/urchin/actions/workflows/ci.yml)

Urchin is a small programming language that compiles straight to a native executable. The vocabulary is otter-themed — a function is a `dive`, `surface` returns, `splash` prints — and every value is a pebble, a 64-bit signed integer.

The compiler is the project. One C++ program lexes the source, parses it, and generates LLVM IR. Clang then optimizes that IR and links it against libc. It is short enough to read in one sitting, and the programs in `examples/` and `tests/` build and run.

## LLVM

Urchin builds with LLVM **18, 19, 20, 21, 22, and 23**. CMake stops, and prints the install commands, for any other version. CI builds, runs the test suite, and runs `examples/fib.urc` on each of those majors.

Two LLVM changes set the edges of that range. LLVM 21 stores a `llvm::Triple` on the module, so 18–20 pass the triple string instead. LLVM 23's `BasicBlock::getTerminator()` asserts when a block is still open; Urchin checks the last instruction, which is what `getTerminator()` did before that.

## Build and run

Run this in one terminal, from a clean clone, on Ubuntu 24.04. It installs LLVM 22 from [apt.llvm.org](https://apt.llvm.org). For 18, 19, 20, 21, or 23, use that number everywhere `22` appears.

```sh
sudo apt-get install -y build-essential cmake wget ca-certificates lsb-release software-properties-common gnupg
wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh
chmod +x /tmp/llvm.sh
sudo /tmp/llvm.sh 22
sudo apt-get install -y llvm-22-dev

git clone https://github.com/aneesh-pradhan/urchin.git
cd urchin
export PATH="/usr/lib/llvm-22/bin:$PATH"
cmake -S . -B build -DLLVM_DIR="$(llvm-config --cmakedir)"
cmake --build build
ctest --test-dir build --output-on-failure

./build/urchin examples/fib.urc -o fib
./fib
```

`PATH` has to include this LLVM before `cmake` and before `urchin` links a program. Urchin shells out to whichever `clang` it finds on `PATH`.

Ubuntu 24.04's own archive is LLVM 18, which is in range:

```sh
sudo apt-get install -y build-essential cmake llvm-dev clang
git clone https://github.com/aneesh-pradhan/urchin.git
cd urchin
cmake -S . -B build -DLLVM_DIR="$(llvm-config-18 --cmakedir)"
cmake --build build
ctest --test-dir build --output-on-failure
./build/urchin examples/fib.urc -o fib
./fib
```

## Example

`examples/fib.urc`:

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

`./fib` prints:

```
0
1
1
2
3
5
8
13
21
34
```

Print the LLVM IR instead of linking:

```sh
./build/urchin examples/fib.urc --emit-llvm -o -
```

## A quick swim through the language

Every value is a `pebble`, a 64-bit signed integer.

| Urchin                            | Meaning                                         |
| --------------------------------- | ----------------------------------------------- |
| `~~ ... ~~`                       | comment (may span lines)                        |
| `dive name(pebble a) -> pebble`   | define a function                               |
| `surface expr;`                   | return from a dive                              |
| `splash expr;`                    | print a pebble on its own line                  |
| `stash x = expr;`                 | declare a variable                              |
| `x = expr;`                       | reassign a variable                             |
| `if c { } else if d { } else { }` | branch (any non-zero pebble is true)            |
| `paddle i from a to b { }`        | loop `i` from `a` up to, but not including, `b` |

Operators: `+ - * / %`, comparisons `== != < > <= >=` (which give `1` or `0`), and unary `-`.

Statements outside of any dive run top to bottom when the program starts. Dives can call each other regardless of the order they're written in, and a dive that ends without surfacing returns `0`.

Shoutouts to Loy

## License

[MIT](LICENSE). Copyright 2026 Aneesh Pradhan.
