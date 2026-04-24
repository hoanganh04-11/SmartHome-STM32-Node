#include "lora_receiver.h"

static const uint8_t FRAME_START = 0xAA;
static const uint8_t FRAME_END   = 0x55;

// Chan LoRa
#define LORA_RX 16
#define LORA_TX 17
#define LORA_M0 2
#define LORA_M1 4

typedef struct __attribute__((packed)) {
  uint8_t startByte;
  SensorData_t payload;
  uint8_t checksum;
  uint8_t endByte;
} SensorFrame_t;

static uint8_t calcXor(const uint8_t* data, size_t len) {
  uint8_t c = 0;
  for (size_t i = 0; i < len; i++) {
    c ^= data[i];
  }
  return c;
}

void initLoraReceiver() {
  pinMode(LORA_M0, OUTPUT);
  pinMode(LORA_M1, OUTPUT);

  // Mode 0: normal transmission
  digitalWrite(LORA_M0, LOW);
  digitalWrite(LORA_M1, LOW);

  Serial2.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  delay(200);

  Serial.println("ESP32 LoRa Receiver Ready");
  Serial.println("Serial2: RX=16, TX=17");
  Serial.println("M0=GPIO2 -> LOW, M1=GPIO4 -> LOW");
}

bool readLoraSensorData(SensorData_t& outData) {
  static uint8_t buf[sizeof(SensorFrame_t)];
  static size_t idx = 0;

  while (Serial2.available()) {
    uint8_t b = (uint8_t)Serial2.read();

    if (idx == 0 && b != FRAME_START) {
      continue;
    }

    buf[idx++] = b;

    if (idx >= sizeof(SensorFrame_t)) {
      idx = 0;

      SensorFrame_t frame;
      memcpy(&frame, buf, sizeof(frame));

      if (frame.startByte != FRAME_START) {
        Serial.println("Loi: sai start byte");
        return false;
      }

      if (frame.endByte != FRAME_END) {
        Serial.println("Loi: sai end byte");
        return false;
      }

      uint8_t cs = calcXor((uint8_t*)&frame.payload, sizeof(frame.payload));
      if (cs != frame.checksum) {
        Serial.printf("Loi checksum: tinh=%02X, nhan=%02X\n", cs, frame.checksum);
        return false;
      }

      outData = frame.payload;
      return true;
    }
  }

  return false;
}

bool sendLoraSensorData(const SensorData_t& data) {
  SensorFrame_t frame;
  frame.startByte = FRAME_START;
  frame.payload = data;
  frame.checksum = calcXor((const uint8_t*)&frame.payload, sizeof(frame.payload));
  frame.endByte = FRAME_END;

  size_t written = Serial2.write((const uint8_t*)&frame, sizeof(frame));
  return written == sizeof(frame);
}

bool sendLoraTextCommand(const char* command) {
  if (command == nullptr || command[0] == '\0') {
    return false;
  }

  size_t commandLen = strlen(command);
  size_t written = Serial2.write((const uint8_t*)command, commandLen);
  written += Serial2.write('\n');
  return written == (commandLen + 1);
}
