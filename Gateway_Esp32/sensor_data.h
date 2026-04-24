#pragma once

#include <Arduino.h>

// LoRa payload: 19 bytes
typedef struct __attribute__((packed)) {
  float    temperature;
  float    humidity;  
  uint16_t gasValue;
  uint16_t lightValue;
  uint16_t tempThreshold;
  uint16_t gasThreshold;
  uint8_t  relayStatus;   // bit0: relay1, bit1: relay2
  uint8_t  alertStatus;   // 0/1
  bool     autoMode;      // true: tu dong, false: dieu khien tay
} SensorData_t;
