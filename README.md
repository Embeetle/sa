# SA — Source Analyzer

SA is the source-analysis engine powering the [Embeetle IDE](https://embeetle.com). It parses C and C++ source code (and assembly/linker scripts) using a custom build of Clang/LLVM and exposes the results through a shared library (`libsource_analyzer`) and a Python wrapper.

## What it does

SA gives Embeetle the ability to understand your firmware project's source code:

- **Symbol resolution** — find definitions, declarations, and every occurrence of a symbol across the project.
- **Code completion** — provide completion candidates at a given source location.
- **Diagnostics** — surface compiler errors and warnings without running a full build.
- **Incremental analysis** — re-analyze only the files that changed (edit/reload cycle, with a disk cache).
- **Assembly analysis** — understand `.S` / `.s` assembly files.
- **Binary analysis** — inspect `.elf` / `.bin` artifacts.
- **Linker script analysis** — parse memory-map information from `.ld` files.
- **Make / link-command analysis** — extract compiler flags and include paths from the project's makefile.

The public API is a plain C interface (see `sa.version` for the exported symbols, e.g. `ce_create_project`, `ce_find_symbols`, `ce_get_completions`, …). A Python convenience wrapper is shipped alongside the library at `esa/source_analyzer.py`.

## Repository layout

```
sa/
├── Makefile                  Main build file (shadow-build, GNU Make)
├── sa.version                Linker version script — controls exported symbols
├── source_analyzer.py        Python wrapper (copied to sys output)
├── sys/
│   ├── esa/                  Clang CUDA/OpenMP wrapper headers + module map
│   └── linux/                Pre-built Linux helper binaries (7za, rsync, ssh, …)
├── test/                     Regression tests (C, C++, assembly, PIC32, STM32, …)
└── docker/                   Docker image used to produce portable Linux builds
    ├── Dockerfile
    ├── build-vm              Build the Docker image
    ├── run-vm                Run a command inside the image
    └── bin/
        ├── make-sa           Build SA inside the container
        ├── update-sa         Clone / update LLVM + SA repos inside the container
        └── upload-sa         Build and upload a Linux release
```

## Build output

A successful build produces a `sys-<os>-<arch>/` directory (e.g. `sys-linux-x86_64` or `sys-windows-x86_64`) containing:

```
sys-<os>-<arch>/
├── esa/
│   └── source_analyzer.py   Python API wrapper
├── lib/
│   ├── libsource_analyzer.so  (or .dll on Windows)
│   ├── clang                  Clang executable
│   └── lld                    LLD linker executable
├── bin/                       Platform utilities (diff, rsync, ssh, …)  [Windows only]
└── version.txt                Git hashes of SA + LLVM (release builds only)
```

This directory is installed into the `sys/` folder of an Embeetle installation.

## Prerequisites

### Common
- **Git**
- **GCC / G++** (C++17)
- **CMake** and **Ninja** (for the LLVM build)
- **7-Zip** (`7za`)
- **Python 3** (for running tests)
- The **LLVM repo** cloned and built at `~/bld/llvm` (see below)

### Windows
- [MSYS2](https://www.msys2.org/) installed at `C:/msys64`
- Run everything from the **MSYS2 UCRT64** shell (`ucrt64.exe`)
- Install the required UCRT64 packages:
  ```
  pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
            mingw-w64-ucrt-x86_64-gcc p7zip rsync
  ```

### Linux
- Standard build tools (`build-essential`, `ninja-build`, `cmake`, `p7zip`, `rsync`)
- For **release builds**: Docker (the Makefile uses a manylinux_2_28 container to ensure broad glibc compatibility)

## Building

SA uses **shadow building** — you must build in a directory separate from the source tree.

### 1. Build LLVM

LLVM must be built before SA. From the SA source tree:

```sh
mkdir -p ~/bld/llvm
cd ~/bld/sa          # your chosen build directory
make -f ~/sa/Makefile llvm
```

This runs CMake + Ninja to build `clang` and `lld` into `~/bld/llvm`.

### 2. Build SA

```sh
cd ~/bld/sa
make -f ~/sa/Makefile
```

The default target runs the self-tests and regression tests, then produces the `sys-<os>-<arch>/` output directory.

Useful individual targets:

| Target | Description |
|---|---|
| `make llvm` | Build Clang + LLD from the LLVM repo |
| `make sys` | Build `libsource_analyzer` and assemble the sys tree |
| `make selftest` | Run unit-level self-tests |
| `make test` | Run Python-based regression tests |
| `make install` | Sync the sys tree into `~/embeetle/sys` |
| `make upload` | Package and upload a release to the Embeetle download server |
| `make release` | `version-stamp` + `selftest` + `test` + `install` + `upload` |
| `make clean` | Remove all build artifacts |

### Automated build (Windows)

The `automate_builds.py` script (located in the `automate_builds/` sibling repo) automates cloning, installing packages, and running the full build pipeline including SA:

```
> python automate_builds.py --build-sa
> python automate_builds.py --install-sa
```

See that script's `--help` output for the full option reference.

### Linux release builds (Docker)

On Linux, portable release builds are produced inside a Docker container based on `quay.io/pypa/manylinux_2_28_x86_64` to ensure compatibility with a wide range of Linux distributions:

```sh
cd ~/bld/sa
make -f ~/sa/Makefile release
```

This automatically builds the Docker image (if needed), runs the build inside the container, and uploads the result.

## Testing

Tests run automatically as part of the default `make` target. To run them individually:

```sh
# Self-tests (unit-level, no toolchain required)
make -f ~/sa/Makefile selftest

# Regression tests (require ARM and MIPS toolchains, downloaded automatically)
make -f ~/sa/Makefile test
```

Toolchains are downloaded from the Embeetle server into `../beetle_tools/` on first use.

## Integration with Embeetle

After a successful build, run `make install` to copy the sys tree into `~/embeetle/sys`. Embeetle will then use the newly built SA automatically when launched from source.

For a production release, `make upload` packages the sys tree as `sys.7z` and uploads it to the Embeetle download server, from where Embeetle installations fetch it on first launch.

## License

Copyright © 2018–2026 Johan Cockx, Matic Kukovec & Kristof Mulier.
Licensed under the [GNU General Public License v3.0 or later](https://www.gnu.org/licenses/gpl-3.0.html).
