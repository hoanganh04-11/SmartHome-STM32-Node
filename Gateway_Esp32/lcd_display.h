#pragma once

#include <Arduino.h>
#include "sensor_data.h"

void initLcdDisplay();
void updateLcdDisplay(const SensorData_t& data);

