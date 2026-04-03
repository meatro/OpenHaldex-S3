#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t id;
  const char* name;
  uint16_t start_bit;
  uint8_t length;
  uint8_t is_little_endian;
  uint8_t is_signed;
  float factor;
  float offset;
  float min;
  float max;
  const char* unit;
  int16_t mux; // -1 none, -2 multiplexor, >=0 multiplexed value
} dbc_signal_t;

static inline uint64_t dbc_extract_raw(const uint8_t* data, uint16_t start_bit, uint8_t length,
                                       uint8_t is_little_endian) {
  if (length == 0 || length > 64) {
    return 0;
  }
  if (is_little_endian) {
    uint64_t raw = 0;
    for (uint8_t i = 0; i < 8; i++) {
      raw |= ((uint64_t)data[i]) << (8U * i);
    }
    raw >>= start_bit;
    if (length == 64) {
      return raw;
    }
    return raw & ((1ULL << length) - 1ULL);
  }

  uint64_t raw = 0;
  for (uint16_t i = 0; i < length; i++) {
    int bit_index = (int)start_bit - (int)i;
    int byte_index = bit_index / 8;
    int bit_in_byte = 7 - (bit_index % 8);
    uint8_t bit = (data[byte_index] >> bit_in_byte) & 0x1;
    raw = (raw << 1) | bit;
  }
  return raw;
}

static inline float dbc_decode_signal(const dbc_signal_t* sig, const uint8_t* data) {
  uint64_t raw = dbc_extract_raw(data, sig->start_bit, sig->length, sig->is_little_endian);
  if (!sig->is_signed) {
    return ((float)raw * sig->factor) + sig->offset;
  }
  if (sig->length == 64) {
    int64_t sraw = (int64_t)raw;
    return ((float)sraw * sig->factor) + sig->offset;
  }
  uint64_t sign_mask = 1ULL << (sig->length - 1U);
  int64_t sraw = (raw & sign_mask) ? (int64_t)(raw | (~((1ULL << sig->length) - 1ULL))) : (int64_t)raw;
  return ((float)sraw * sig->factor) + sig->offset;
}
