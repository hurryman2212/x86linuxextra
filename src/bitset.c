#include "x86linux/helper.h"

unsigned char bitset_test(const bitset_t *restrict bitset32,
                          uint32_t idx) noexcept {
  const bitset_t *_bitset = bitset32 + (idx >> 6);

  register unsigned char _cf asm("al");
  asm volatile("btl %2, %1\n\t"
               "setc %0"
               : "=a"(_cf)
               : "m"(*_bitset), "Ir"(idx & 0x3F)
               : "cc");
  return _cf;
}

unsigned char bitset_set(bitset_t *restrict bitset32, uint32_t idx) noexcept {
  bitset_t *_bitset = bitset32 + (idx >> 6);

  register unsigned char _cf asm("al");
  asm volatile("btsl %2, %1\n\t"
               "setc %0"
               : "=a"(_cf), "+m"(*_bitset)
               : "Ir"(idx & 0x3F)
               : "cc");
  return _cf;
}
unsigned char bitset_unset(bitset_t *restrict bitset32, uint32_t idx) noexcept {
  bitset_t *_bitset = bitset32 + (idx >> 6);

  register unsigned char _cf asm("al");
  asm volatile("btrl %2, %1\n\t"
               "setc %0"
               : "=a"(_cf), "+m"(*_bitset)
               : "Ir"(idx & 0x3F)
               : "cc");
  return _cf;
}

int64_t bitset_search_lowest(const bitset_t *restrict bitset,
                             uint32_t start_idx, uint32_t last_idx) noexcept {
  if (start_idx > last_idx)
    return -1;

  bitset_t _tmp = start_idx & 0x3F;
  if (_tmp) {
    const bitset_t _res = *(bitset + (start_idx >> 6)) & (UINT64_MAX << _tmp);
    if (_res)
      return ((start_idx & -0x40) + __builtin_ctzll(_res) > last_idx)
                 ? -1
                 : (int64_t)((start_idx & -0x40) + __builtin_ctzll(_res));
  }

  _tmp = ((start_idx >> 6) + !!_tmp);
  while (likely((last_idx >> 6) >= _tmp)) {
    bitset_t _res = *(bitset + _tmp);
    if (_res) {
      _res = __builtin_ctzll(_res);
      return ((_tmp << 6) + _res > last_idx) ? -1
                                             : (int64_t)((_tmp << 6) + _res);
    }
    ++_tmp;
  }
  return -1;
}
int64_t bitset_search_lowest_common(const bitset_t *restrict bitset,
                                    const bitset_t *restrict bitset2,
                                    uint32_t start_idx,
                                    uint32_t last_idx) noexcept {
  if (start_idx > last_idx)
    return -1;

  bitset_t _tmp = start_idx & 0x3F;
  if (_tmp) {
    const bitset_t _res = *(bitset + (start_idx >> 6)) &
                          *(bitset2 + (start_idx >> 6)) & (UINT64_MAX << _tmp);
    if (_res)
      return ((start_idx & -0x40) + __builtin_ctzll(_res) > last_idx)
                 ? -1
                 : (int64_t)((start_idx & -0x40) + __builtin_ctzll(_res));
  }

  _tmp = ((start_idx >> 6) + !!_tmp);
  while (likely((last_idx >> 6) >= _tmp)) {
    bitset_t _res = *(bitset + _tmp) & *(bitset2 + _tmp);
    if (_res) {
      _res = __builtin_ctzll(_res);
      return ((_tmp << 6) + _res > last_idx) ? -1
                                             : (int64_t)((_tmp << 6) + _res);
    }
    ++_tmp;
  }
  return -1;
}
