#include <Arduino.h>

#include "sensor_data.h"
#include "lora_receiver.h"
#include "lcd_display.h"
#include "blynk_bridge.h"

static SensorData_t sensorData = {0.0f, 0.0f, 0, 0, 35, 600, 0, 0, false};

static bool mergeChangedFields(SensorData_t& current, const SensorData_t& incoming) {
  bool changed = false;

  if (current.temperature != incoming.temperature) {
    current.temperature = incoming.temperature;
    changed = true;
  }
  if (current.humidity != incoming.humidity) {
    current.humidity = incoming.humidity;
    changed = true;
  }
  if (current.gasValue != incoming.gasValue) {
    current.gasValue = incoming.gasValue;
    changed = true;
  }
  if (current.lightValue != incoming.lightValue) {
    current.lightValue = incoming.lightValue;
    changed = true;
  }
  if (current.tempThreshold != incoming.tempThreshold) {
    current.tempThreshold = incoming.tempThreshold;
    changed = true;
  }
  if (current.gasThreshold != incoming.gasThreshold) {
    current.gasThreshold = incoming.gasThreshold;
    changed = true;
  }
  if (current.relayStatus != incoming.relayStatus) {
    current.relayStatus = incoming.relayStatus;
    changed = true;
  }
  if (current.alertStatus != incoming.alertStatus) {
    current.alertStatus = incoming.alertStatus;
    changed = true;
  }
  if (current.autoMode != incoming.autoMode) {
    current.autoMode = incoming.autoMode;
    changed = true;
  }

  return changed;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  initLoraReceiver();
  initLcdDisplay();
  updateLcdDisplay(sensorData);

  initBlynkBridge();
}

void loop() {
  blynkBridgeLoop();

  SensorData_t loraFrame;
  if (readLoraSensorData(loraFrame)) {
    SensorData_t oldSensorData = sensorData;
    bool changed = mergeChangedFields(sensorData, loraFrame);
    if (!changed) {
      Serial.println("LoRa frame khong doi du lieu, bo qua cap nhat");
      return;
    }

    Serial.println("-------- NHAN FRAME --------");
    Serial.printf("Fixed sensor IDs: light=1 temp=2 gas=3 humi=4\n");
    Serial.printf("Temperature : %.2f C\n", sensorData.temperature);
    Serial.printf("Humidity    : %.2f %%\n", sensorData.humidity);
    Serial.printf("Gas         : %u\n", sensorData.gasValue);
    Serial.printf("Light       : %u\n", sensorData.lightValue);
    Serial.printf("Temp thres  : %u\n", sensorData.tempThreshold);
    Serial.printf("Gas thres   : %u\n", sensorData.gasThreshold);
    Serial.printf("Relay 1(dev=1): %d\n", (sensorData.relayStatus & 0x01) != 0);
    Serial.printf("Relay 2(dev=2): %d\n", (sensorData.relayStatus & 0x02) != 0);
    Serial.printf("Alert       : %u\n", sensorData.alertStatus);
    Serial.printf("AutoMode    : %u\n", sensorData.autoMode);
    Serial.println("----------------------------");
    updateLcdDisplay(sensorData);
    blynkBridgePublishChangedFromLora(oldSensorData, sensorData);
  }
}
