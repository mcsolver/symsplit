# SymSplit
New symmetry aware algorithm for maximum common subgraph problem.

# Prerequisites
- C++ compiler
- Make (optional)

# Build and Test
## Linux
Use the following command to build this program in Linux operating system.
```shell
$ mkdir bin
$ make build
```

## MacOS
Use the following command to build this program in macOS.
```shell
$ mkdir bin
$ make macbuild
```

## Testing the Program
```shell
$ make test
```

Expected output.
```shell
$ make test
./bin/run.o min_max ./data/tests/pattern ./data/tests/target -l -q -t 100
7, 1, 2.1875e-05, 5.5167e-05, 244, 7, 159, 0, 18, 0
```
