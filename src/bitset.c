#include "x86linux/helper.h"

unsigned char bitset_test(const bitset_t *restrict bitset32, uint32_t idx) {
  const uint32_t r32_bitset = *(((uint32_t *)bitset32) + (idx >> 5));

  register unsigned char __cf asm("al");
  asm volatile("btl %2, %1\n\t"
               "setc %0"
               : "=a"(__cf)
               : "r"(r32_bitset), "Ir"(idx)
               : "cc");
  return __cf;
}

unsigned char bitset_set(bitset_t *restrict bitset32, uint32_t idx) {
  uint32_t *const restrict __bitset32 = (uint32_t *)bitset32 + (idx >> 5);

  register unsigned char __cf asm("al");
  asm volatile("btsl %2, %1\n\t"
               "setc %0"
               : "=a"(__cf), "+r"(*__bitset32)
               : "Ir"(idx)
               : "cc");
  return __cf;
}
unsigned char bitset_unset(bitset_t *restrict bitset32, uint32_t idx) {
  uint32_t *const restrict __bitset32 = (uint32_t *)bitset32 + (idx >> 5);

  register unsigned char __cf asm("al");
  asm volatile("btrl %2, %1\n\t"
               "setc %0"
               : "=a"(__cf), "+r"(*__bitset32)
               : "Ir"(idx)
               : "cc");
  return __cf;
}

int32_t bitset_search_lowest(const bitset_t *restrict bitset,
                             uint32_t start_idx, uint32_t last_idx) {
  bitset_t __tmp = start_idx & 0x3F;
  if (__tmp) {
    bitset_t __res = *(bitset + (start_idx >> 6)) & (UINT64_MAX << __tmp);
    if (__res)
      return ((start_idx & -0x40) + __builtin_ctzll(__res) > last_idx)
                 ? -1
                 : (int64_t)((start_idx & -0x40) + __builtin_ctzll(__res));
  }

  __tmp = ((start_idx >> 6) + !!__tmp);
  while (likely((last_idx >> 6) >= __tmp)) {
    bitset_t __res = *(bitset + __tmp);
    if (__res) {
      __res = __builtin_ctzll(__res);
      return ((__tmp << 6) + __res > last_idx)
                 ? -1
                 : (int64_t)((__tmp << 6) + __res);
    }
    ++__tmp;
  }
  return -1;
}
int32_t bitset_search_lowest_common(const bitset_t *restrict bitset,
                                    const bitset_t *restrict bitset2,
                                    uint32_t start_idx, uint32_t last_idx) {
  bitset_t __tmp = start_idx & 0x3F;
  if (__tmp) {
    bitset_t __res = *(bitset + (start_idx >> 6)) &
                     *(bitset2 + (start_idx >> 6)) & (UINT64_MAX << __tmp);
    if (__res)
      return ((start_idx & -0x40) + __builtin_ctzll(__res) > last_idx)
                 ? -1
                 : (int64_t)((start_idx & -0x40) + __builtin_ctzll(__res));
  }

  __tmp = ((start_idx >> 6) + !!__tmp);
  while (likely((last_idx >> 6) >= __tmp)) {
    bitset_t __res = *(bitset + __tmp) & *(bitset2 + __tmp);
    if (__res) {
      __res = __builtin_ctzll(__res);
      return ((__tmp << 6) + __res > last_idx)
                 ? -1
                 : (int64_t)((__tmp << 6) + __res);
    }
    ++__tmp;
  }
  return -1;
}
