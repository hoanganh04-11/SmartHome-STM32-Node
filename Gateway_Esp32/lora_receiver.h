#pragma once

#include <Arduino.h>
#include "sensor_data.h"

void initLoraReceiver();
bool readLoraSensorData(SensorData_t& outData);
bool sendLoraSensorData(const SensorData_t& data);
bool sendLoraTextCommand(const char* command);
