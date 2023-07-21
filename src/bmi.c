#include "x86linux/helper.h"

int x86_test_bit(const bitset_t *restrict bitset, uint32_t idx) {
  uint32_t r32_bitset = *(((uint32_t *)bitset) + (idx >> 5));

  int cf;
  asm volatile("bt %2, %1" : "=@ccc"(cf) : "r"(r32_bitset), "Ir"(idx));

  return cf;
}

int x86_set_bit_nonatomic(bitset_t *restrict bitset, uint32_t idx) {
  uint32_t *bitset32 = (void *)bitset;
  bitset32 += (idx >> 5);

  int cf;
  asm volatile("bts %2, %1" : "=@ccc"(cf), "+r"(*bitset32) : "Ir"(idx));

  return cf;
}
int x86_unset_bit_nonatomic(bitset_t *restrict bitset, uint32_t idx) {
  uint32_t *bitset32 = (void *)bitset;
  bitset32 += (idx >> 5);

  int cf;
  asm volatile("btr %2, %1" : "=@ccc"(cf), "+r"(*bitset32) : "Ir"(idx));

  return cf;
}
int x86_set_bit_atomic(bitset_t *restrict bitset, uint32_t idx) {
  int cf;
  asm volatile("lock bts %2, %1"
               : "=@ccc"(cf), "+m"(*bitset)
               : "Ir"(idx)
               : "memory");

  return cf;
}
int x86_unset_bit_atomic(bitset_t *restrict bitset, uint32_t idx) {
  int cf;
  asm volatile("lock btr %2, %1"
               : "=@ccc"(cf), "+m"(*bitset)
               : "Ir"(idx)
               : "memory");

  return cf;
}

int64_t x86_search_lowest_bit(const bitset_t *restrict bitset,
                              uint32_t start_idx, uint32_t last_idx) {
  bitset_t tmp = start_idx & 0x3F;
  if (tmp) {
    bitset_t res = *(bitset + (start_idx >> 6)) & (UINT64_MAX << tmp);
    if (res)
      return ((start_idx & -0x40) + __bsfq(res) > last_idx)
                 ? -1
                 : (int64_t)((start_idx & -0x40) + __bsfq(res));
  }

  tmp = ((start_idx >> 6) + !!tmp);
  while (likely((last_idx >> 6) >= tmp)) {
    bitset_t res = *(bitset + tmp);
    if (res) {
      res = __bsfq(res);
      return ((tmp << 6) + res > last_idx) ? -1 : (int64_t)((tmp << 6) + res);
    }
    ++tmp;
  }
  return -1;
}
int64_t x86_consume_lowest_bit_nonatomic(bitset_t *restrict bitset,
                                         uint32_t start_idx,
                                         uint32_t last_idx) {
  int64_t res = x86_search_lowest_bit(bitset, start_idx, last_idx);
  if (res >= 0)
    *(bitset + (res >> 6)) = __blsr_u64(*(bitset + (res >> 6)));
  return res;
}
int64_t x86_search_lowest_common_bit(const bitset_t *restrict bitset,
                                     const bitset_t *restrict bitset2,
                                     uint32_t start_idx, uint32_t last_idx) {
  bitset_t tmp = start_idx & 0x3F;
  if (tmp) {
    bitset_t res = *(bitset + (start_idx >> 6)) &
                   *(bitset2 + (start_idx >> 6)) & (UINT64_MAX << tmp);
    if (res)
      return ((start_idx & -0x40) + __bsfq(res) > last_idx)
                 ? -1
                 : (int64_t)((start_idx & -0x40) + __bsfq(res));
  }

  tmp = ((start_idx >> 6) + !!tmp);
  while (likely((last_idx >> 6) >= tmp)) {
    bitset_t res = *(bitset + tmp) & *(bitset2 + tmp);
    if (res) {
      res = __bsfq(res);
      return ((tmp << 6) + res > last_idx) ? -1 : (int64_t)((tmp << 6) + res);
    }
    ++tmp;
  }
  return -1;
}
