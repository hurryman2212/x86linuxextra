# x86linuxextra

This is an optimized general-purpose library (and kernel module) for developing
on x86 Linux, which I "personally" think is the de facto standard development
platform, providing things like:

- Compatibility between C/C++ and user-space/kernel source code
- Conventional macros for alignment, compile-time processing, string handling
  and types
- Functions for 32/64-bit bitset operations (currently using i386 (or BMI if
  build configuration is set to use it) extension)
- Functions for SPSC (Single-Producer Single-Consumer) queue of a non-power-of-2
  size
- Header-only intrusive circular doubly linked lists
- Logger
- User-space scheduler and best-effort `futex` lock/release wrappers
- x86 `CPUID`/`CPUIDEX` and `UMWAIT` wrappers
- ... and the kernel module containing (some of) the above for helping kernel
  development via exported symbols

You need the static archive of
[libbacktrace](https://github.com/ianlancetaylor/libbacktrace) to build this
library. Build it with `--enable-static --with-pic` so it can also be linked
into `libx86linuxextra.so`. Both targets use `libbacktrace.a`; no shared
libbacktrace is required. The static target propagates this CMake link
dependency automatically. For a nonstandard installation prefix, pass
`-DCMAKE_PREFIX_PATH=/path/to/prefix`. Also, the header code assumes compiling
with either `gcc`/`g++` or `clang`/`clang++`, which I also think are the de
facto C/C++ compilers.

This project tries to implement everything in the GNU version of C11 (`gnu11`)
with inline x86 assembly and avoid reimplementing things from the de facto
standard libraries (like GLib or Boost). For the kernel module, the goal is to
offer common instruments available in the user-space library. While this targets
the x86 version of Linux, it would be implemented with the most generic code
possible. (actually, one of the future goals is to support ARM architecture as
well, which is growing as the potential replacement of x86 platforms, and change
the project name to `x86armlinuxextra`)

The code base focuses on providing APIs that are as close to standard styles as
possible and always expects the latest Linux kernel version at that moment.
Regarding the type of C/C++ standard libraries, it currently expects the latest
version of `glibc`/`libstdc++` at that moment.

## How to Install

```sh
mkdir -p build
cd build
cmake .. # -DCMAKE_GENERATOR=Ninja -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install -j$(nproc)
```

Consumers can use the installed CMake package without listing libbacktrace
themselves:

```cmake
find_package(x86linuxextra CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE x86linuxextra::libx86linuxextra.so)
```

Use `x86linuxextra::libx86linuxextra.a` for static linking. Its required
libraries are propagated automatically. `x86linuxextra::headers` provides only
the headers and compile definitions.

In user space, `log_lvl()`, `log_enable()` and `log_disable()` access shared
anonymous memory initialized by the library constructor. Changes are visible to
all threads and descendants created with `fork()`. An `exec()` starts a new
logging state. Kernel builds retain an ordinary integer log-level variable.

`FILE_NAME` provides the current source filename; define `X86LINUX_DIR` to strip
a source directory prefix. These replace the former `__filename__` and `__DIR__`
names. Internal scheduler/SPSC preparation functions use the
`_usersched_spsc_prepare_*` prefix. Rebuild consumers against the updated header
and library, including the new user-space log-level storage.
