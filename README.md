# x86linuxextra

This is an optimized general-purpose library (and kernel module) for developing on x86 Linux, which I "personally" think is the de facto standard development platform, providing things like:

* Compatibility between C/C++ and user-space/kernel source code
* Conventional macros for alignment, compile-time processing, string handling and types
* Functions for 32/64-bit bitset operations (currently using i386 (or BMI if build configuration is set to use it) extension)
* Functions for SPSC (Single-Producer Single-Consumer) queue of a non-power-of-2 size
* Logger
* User-space scheduler and best-effort `futex` lock/release wrappers
* x86 `CPUID`/`CPUIDEX` and `UMWAIT` wrappers
* ... and the kernel module containing (some of) the above for helping kernel development via exported symbols

You need [libbacktrace](https://github.com/ianlancetaylor/libbacktrace) to build this library. Also, the header code assumes compiling with either `gcc`/`g++` or `clang`/`clang++`, which I also think are the de facto C/C++ compilers.

This project tries to implement everything in the GNU version of C11 (`gnu11`) with inline x86 assembly and avoid reimplementing things from the de facto standard libraries (like GLib or Boost). For the kernel module, the goal is to offer common instruments available in the user-space library. While this targets the x86 version of Linux, it would be implemented with the most generic code possible. (actually, one of the future goals is to support ARM architecture as well, which is growing as the potential replacement of x86 platforms, and change the project name to `x86armlinuxextra`)

The code base focuses on providing APIs that are as close to standard styles as possible and always expects the latest Linux kernel version at that moment. Regarding the type of C/C++ standard libraries, it currently expects the latest version of `glibc`/`libstdc++` at that moment.

## How to Install

```sh
mkdir -p build
cd build
cmake .. # -DCMAKE_GENERATOR=Ninja -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install -j$(nproc)
```
