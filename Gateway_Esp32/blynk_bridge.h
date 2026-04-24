#pragma once

#include <Arduino.h>
#include "sensor_data.h"

void initBlynkBridge();
void blynkBridgeLoop();
void blynkBridgePublishChangedFromLora(const SensorData_t& oldData, const SensorData_t& newData);
void blynkBridgeRequestNodeDataByInterval();
