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
- The **LLVM repo** cloned from https://github.com/Embeetle/llvm

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

The `automate_builds.py` script (located in the `automate_builds/` sibling repo at https://github.com/Embeetle/automate_builds) automates cloning, installing packages, and running the full build pipeline including SA:

```
> python automate_builds.py --build-sa
> python automate_builds.py --install-sa
```

See that script's `--help` output for the full option reference.

## License

Copyright © 2018–2026 Johan Cockx.
Licensed under the [GNU General Public License v3.0 or later](https://www.gnu.org/licenses/gpl-3.0.html).
