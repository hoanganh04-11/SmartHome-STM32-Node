#include "lcd_display.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Chan man hinh
#define TFT_CS   5
#define TFT_RST  21
#define TFT_DC   3
#define TFT_MOSI 23
#define TFT_CLK  18
#define TFT_MISO 19
#define TFT_LED  1

static Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

// Mau sac
#define COLOR_BG     0x1082
#define COLOR_CARD   0x2104
#define COLOR_WHITE  0xFFFF
#define COLOR_YELLOW 0xFFE0
#define COLOR_GREEN  0x07E0
#define COLOR_RED    0xF800
#define COLOR_ORANGE 0xFD20
#define COLOR_CYAN   0x07FF
#define COLOR_BLUE   0x001F
#define COLOR_GRAY   0x8410

static void drawFrame() {
  tft.fillScreen(COLOR_BG);

  tft.fillRoundRect(2, 2, 156, 116, 5, COLOR_CARD);
  tft.drawRoundRect(2, 2, 156, 116, 5, COLOR_CYAN);

  tft.fillRoundRect(162, 2, 156, 116, 5, COLOR_CARD);
  tft.drawRoundRect(162, 2, 156, 116, 5, COLOR_BLUE);

  tft.fillRoundRect(2, 122, 156, 116, 5, COLOR_CARD);
  tft.drawRoundRect(2, 122, 156, 116, 5, COLOR_GREEN);

  tft.fillRoundRect(162, 122, 156, 116, 5, COLOR_CARD);
  tft.drawRoundRect(162, 122, 156, 116, 5, COLOR_YELLOW);

  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("NHIET DO");
  tft.drawFastHLine(4, 20, 152, COLOR_GRAY);

  tft.setTextColor(COLOR_BLUE);
  tft.setCursor(170, 10);
  tft.print("DO AM DAT");
  tft.drawFastHLine(164, 20, 152, COLOR_GRAY);

  tft.setTextColor(COLOR_GREEN);
  tft.setCursor(10, 130);
  tft.print("KHI GAS");
  tft.drawFastHLine(4, 140, 152, COLOR_GRAY);

  tft.setTextColor(COLOR_YELLOW);
  tft.setCursor(170, 130);
  tft.print("ANH SANG");
  tft.drawFastHLine(164, 140, 152, COLOR_GRAY);

  tft.setTextColor(COLOR_GRAY);
  tft.setCursor(6, 220);
  tft.print("0");
  tft.setCursor(72, 220);
  tft.print("500");
  tft.setCursor(130, 220);
  tft.print("1000");
  tft.setCursor(166, 220);
  tft.print("0");
  tft.setCursor(232, 220);
  tft.print("500");
  tft.setCursor(290, 220);
  tft.print("1000");
}

static void refreshTemp(const SensorData_t& data) {
  tft.fillRect(4, 21, 152, 98, COLOR_CARD);

  uint16_t threshold = data.tempThreshold > 0 ? data.tempThreshold : 35;
  uint16_t c = data.temperature < 25.0f ? COLOR_CYAN :
               data.temperature < 30.0f ? COLOR_GREEN : COLOR_ORANGE;
  tft.drawRoundRect(2, 2, 156, 116, 5, c);
  tft.setTextColor(c);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("NHIET DO");

  tft.setTextColor(COLOR_YELLOW);
  tft.setTextSize(4);
  tft.setCursor(10, 35);
  tft.print(data.temperature, 1);

  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 82);
  tft.print("Do C");

  tft.setTextSize(1);
  tft.setCursor(10, 98);
  tft.setTextColor(COLOR_WHITE);
  tft.printf("Nguong: %u", threshold);

  if (data.temperature >= threshold) {
    tft.setTextColor(COLOR_RED);
    tft.setTextSize(1);
    tft.setCursor(10, 108);
    tft.print("! QUA NONG !");
  }
}

static void refreshHum(const SensorData_t& data) {
  tft.fillRect(164, 21, 152, 98, COLOR_CARD);

  uint16_t c = data.humidity < 40.0f ? COLOR_ORANGE :
               data.humidity < 70.0f ? COLOR_CYAN : COLOR_BLUE;
  tft.drawRoundRect(162, 2, 156, 116, 5, c);
  tft.setTextColor(c);
  tft.setTextSize(1);
  tft.setCursor(170, 10);
  tft.print("DO AM DAT");

  tft.setTextColor(COLOR_CYAN);
  tft.setTextSize(4);
  tft.setCursor(170, 35);
  tft.print(data.humidity, 1);

  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(2);
  tft.setCursor(170, 82);
  tft.print("%");
}

static void refreshGas(const SensorData_t& data) {
  tft.fillRect(4, 141, 152, 78, COLOR_CARD);

  uint16_t threshold = data.gasThreshold > 0 ? data.gasThreshold : 600;
  bool gasAlert = data.gasValue >= threshold;
  uint16_t borderColor = gasAlert ? COLOR_RED : COLOR_GREEN;
  tft.drawRoundRect(2, 122, 156, 116, 5, borderColor);
  tft.setTextColor(borderColor);
  tft.setTextSize(1);
  tft.setCursor(10, 130);
  tft.print("KHI GAS");

  tft.setTextColor(gasAlert ? COLOR_RED : COLOR_WHITE);
  tft.setTextSize(3);
  tft.setCursor(10, 150);
  tft.print(data.gasValue);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE);
  tft.print(" ppm");

  tft.setTextSize(1);
  tft.setCursor(10, 185);
  if (gasAlert) {
    tft.setTextColor(COLOR_RED);
    tft.print("! CANH BAO !");
  } else {
    tft.setTextColor(COLOR_GREEN);
    tft.print("An toan");
  }

  tft.setTextColor(COLOR_WHITE);
  tft.setCursor(80, 185);
  tft.printf("N:%u", threshold);

  int barMaxW = 144;
  int barVal = map(constrain((int)data.gasValue, 0, 1000), 0, 1000, 0, barMaxW);
  uint16_t barColor = data.gasValue < 300 ? COLOR_GREEN :
                      data.gasValue < 600 ? COLOR_ORANGE : COLOR_RED;
  tft.fillRect(6, 203, barMaxW, 12, COLOR_CARD);
  tft.drawRect(6, 205, barMaxW, 8, COLOR_GRAY);
  tft.fillRect(7, 206, barVal, 6, barColor);
}

static void refreshLight(const SensorData_t& data) {
  tft.fillRect(164, 141, 152, 78, COLOR_CARD);

  uint16_t borderColor = data.lightValue < 200 ? COLOR_BLUE :
                         data.lightValue < 500 ? COLOR_CYAN :
                         data.lightValue < 800 ? COLOR_YELLOW : COLOR_ORANGE;
  tft.drawRoundRect(162, 122, 156, 116, 5, borderColor);
  tft.setTextColor(borderColor);
  tft.setTextSize(1);
  tft.setCursor(170, 130);
  tft.print("ANH SANG");

  tft.setTextColor(COLOR_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(170, 150);
  tft.print(data.lightValue);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE);
  tft.print(" lux");

  tft.setTextSize(1);
  tft.setCursor(170, 185);
  if (data.lightValue < 200) {
    tft.setTextColor(COLOR_BLUE);
    tft.print("Toi");
  } else if (data.lightValue < 500) {
    tft.setTextColor(COLOR_CYAN);
    tft.print("Binh thuong");
  } else if (data.lightValue < 800) {
    tft.setTextColor(COLOR_YELLOW);
    tft.print("Sang");
  } else {
    tft.setTextColor(COLOR_ORANGE);
    tft.print("Rat sang");
  }

  int barMaxW = 144;
  int barVal = map(constrain((int)data.lightValue, 0, 1000), 0, 1000, 0, barMaxW);
  tft.fillRect(166, 203, barMaxW, 12, COLOR_CARD);
  tft.drawRect(166, 205, barMaxW, 8, COLOR_GRAY);
  tft.fillRect(167, 206, barVal, 6, borderColor);
}

static void refreshStatus(const SensorData_t& data) {
  bool relay1 = (data.relayStatus & 0x01) != 0;
  bool relay2 = (data.relayStatus & 0x02) != 0;

  tft.fillRect(40, 100, 110, 16, COLOR_CARD);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE);
  tft.setCursor(42, 104);
  tft.printf("R1:%u R2:%u AL:%u AT:%u", relay1, relay2, data.alertStatus, data.autoMode);
}

void initLcdDisplay() {
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  tft.begin();
  tft.setRotation(1);
  drawFrame();
}

void updateLcdDisplay(const SensorData_t& data) {
  refreshTemp(data);
  refreshHum(data);
  refreshGas(data);
  refreshLight(data);
  refreshStatus(data);
}
