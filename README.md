# x86linuxextra

This is an optimized general-purpose library (and kernel module) for developing on x86 Linux, providing things like:

* Compatibility between C/C++ and userspace/kernel source code
* Conventional macros for alignment, compile-time processing, string handling, and types
* Functions for 32/64-bit bitset operations (currently using i386 (or BMI if build configuration is set to use it) extension)
* Functions for SPSC (Single-Producer Single-Consumer) queue of a non-power-of-2 size
* Logger
* Userspace scheduler and futex lock/release wrappers
* x86 CPUID/CPUIDEX and UMWAIT wrappers
* ... and the kernel module containing (some of) the above for helping kernel development via exported symbols

You need libbacktrace to build this library. Also, the header code assumes compiling with either gcc/g++ or clang/clang++ at this moment.

This project will continue to try to implement everything in the GNU version of C11 (gnu11) and inline x86 assembly, and avoid reimplementing things from the de-facto standard libraries (like GLib or Boost). For the kernel module, the goal is to offer common instruments available in the userspace library. While this targets the x86 version of Linux, it would be implemented with the most generic code possible.

## How to Install

```sh
mkdir -p build
cd build
cmake .. # -DCMAKE_GENERATOR=Ninja -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install -j$(nproc)
```
