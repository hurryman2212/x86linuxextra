# x86linuxextra

This is a general-purpose library (and kernel object) for developing on x86 Linux, providing things like:

* Logger
* SPSC of a non-power-of-2 size
* Userspace scheduler
* i386/BMI extension wrappers
* Helpers for kernel development (via header, kernel module, and static library)

## How to Install

```sh
mkdir -p build
cd build
cmake .. # cmake -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install -j$(nproc)
```
