#pragma once

#include "x86linux/helper.h"

__attribute((const)) const char *_sicode_np(int sig, int si_code) noexcept;
