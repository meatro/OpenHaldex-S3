#ifndef VW_PQ_CHASSIS_DBC_H
#define VW_PQ_CHASSIS_DBC_H

#include <stdint.h>
#include <stddef.h>

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

extern const dbc_signal_t k_vw_pq_chassis_signals[];
extern const uint16_t k_vw_pq_chassis_signal_count;

uint64_t vw_pq_dbc_extract_raw(const uint8_t* data, uint16_t start_bit, uint8_t length, uint8_t is_little_endian);
float vw_pq_dbc_decode_signal(const dbc_signal_t* sig, const uint8_t* data);

#endif
