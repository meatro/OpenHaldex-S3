#pragma once

#include <Arduino.h>

extern uint8_t MOTOR5_counter;
extern uint8_t BRAKES1_counter;
extern uint8_t BRAKES2_counter;
extern uint8_t BRAKES4_counter;
extern uint8_t BRAKES4_crc;
extern uint8_t BRAKES5_counter;
extern uint8_t BRAKES5_counter2;
extern uint8_t BRAKES9_counter;
extern uint8_t BRAKES9_counter2;
extern uint8_t BRAKES10_counter;
extern uint8_t mLW_1_counter;
extern uint8_t mLW_1_crc;
extern uint8_t mDiagnose_1_counter;
extern const uint8_t lws_2[16][8];

extern uint8_t GETRIEBE_11_counter;
extern uint8_t MOTOR_11_counter;
extern uint8_t MOTOR_12_counter;
extern uint8_t LWI_01_counter;
extern uint8_t ESP_14_counter;
extern uint8_t MOTOR_20_counter;
extern uint8_t ESP_10_counter;
extern uint8_t ESP_05_counter;
extern uint8_t ESP_23_counter;
extern uint8_t ESP_07_counter;
extern uint8_t MOTOR_CODE_01_counter;
extern uint8_t ESP_20_counter;
extern uint8_t MOTOR_14_counter;
extern uint8_t ESP_19_counter;
extern uint8_t ESP_19_counter2;

extern const uint8_t ID_SEQ_0A8[16];
extern const uint8_t ID_SEQ_0AD[16];
extern const uint8_t ID_SEQ_0A7[16];
extern const uint8_t ID_SEQ_08A[16];
extern const uint8_t ID_SEQ_086[16];
extern const uint8_t ID_SEQ_121[16];
extern const uint8_t ID_SEQ_116[16];
extern const uint8_t ID_SEQ_106[16];
extern const uint8_t ID_SEQ_5BE[16];
extern const uint8_t ID_SEQ_3BE[16];
extern const uint8_t ID_SEQ_392[16];
extern const uint8_t ID_SEQ_641[16];
extern const uint8_t ID_SEQ_65D[16];

uint8_t crc8_autosar(uint8_t* data, uint8_t len);
uint8_t calcChecksum(uint8_t* frame, const uint8_t* idSeq);
