# Reunion port

A native port of *Reunion* (Amnesty Design / Grandslam, 1994) for modern
systems, built by decompiling the DOS release. See [PLAN.md](PLAN.md) for the
approach and status.

The game data isn't part of this repository; put your own copy in `vendor/`.

## Building

Needs a C23 compiler (GCC 14+ or Clang 18+), CMake 3.25+ and Ninja. SDL3 is
used from the system when installed (`dnf install SDL3-devel`), otherwise it
is downloaded and built automatically.

```sh
cmake --preset debug          # or: release, asan
cmake --build --preset debug
build/debug/reunion
```

## Reverse engineering

- [ghidra/README.md](ghidra/README.md): rebuilding the Ghidra project and the
  `symbols.txt` format
- [docs/segments.md](docs/segments.md): what each code segment does
