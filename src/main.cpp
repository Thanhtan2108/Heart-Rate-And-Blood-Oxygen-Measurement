#include <Arduino.h>
#include <Wire.h>
#include <scanI2C.h>
#include <U8g2lib.h>
#include <displayU8g2.h>

// Cấu hình bus I2C (theo sơ đồ đấu dây Bước 2)
constexpr uint8_t  kPinSda     = 21;
constexpr uint8_t  kPinScl     = 22;
constexpr uint32_t kI2cClockHz = 100000;

// Thiết bị kỳ vọng trên bus
constexpr uint8_t kMax30102Address        = 0x57;
constexpr uint8_t kMax30102PartIdRegister = 0xFF;
constexpr uint8_t kMax30102ExpectedPartId = 0x15;
constexpr uint8_t kOledAddressPrimary     = 0x3C;
constexpr uint8_t kOledAddressSecondary   = 0x3D;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
DisplayU8g2 display(oled);

void drawOledSelfTest() {
  display.clearBuffer();
  display.setFont(u8g2_font_6x10_tf);
  display.drawTextAligned("OLED SSD1306", 11, DisplayTextAlign::kCenter);

  display.useVietnameseFont();
  display.drawTextAligned("Tiếng Việt", 29, DisplayTextAlign::kCenter);
  display.drawTextAligned("ă â ê ô ơ ư", 48, DisplayTextAlign::kCenter);
  display.drawIcon(DisplayIcon::kCheck, 108, 49, 14);
  display.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== CP1: scan I2C + Part ID + OLED U8g2 self-test ===");

  Wire.begin(kPinSda, kPinScl);
  Wire.setClock(kI2cClockHz);

  const I2CScanResult scan = scanI2CBus(Wire);
  Serial.printf("Found %u device(s):", (unsigned)scan.count);
  for (uint8_t i = 0; i < scan.count; i++) {
    Serial.printf(" 0x%02X", (unsigned)scan.addresses[i]);
  }
  Serial.println();

  if (!isI2CDevicePresent(Wire, kMax30102Address)) {
    Serial.printf("MAX30102: NOT FOUND at 0x%02X\n", (unsigned)kMax30102Address);
  } else {
    uint8_t partId = 0;
    if (!readI2CRegister(Wire, kMax30102Address, kMax30102PartIdRegister, partId)) {
      Serial.println("MAX30102: ACK ok but register read FAILED");
    } else if (partId != kMax30102ExpectedPartId) {
      Serial.printf("MAX30102: Part ID MISMATCH, read 0x%02X, expected 0x%02X\n",
                    (unsigned)partId, (unsigned)kMax30102ExpectedPartId);
    } else {
      Serial.printf("MAX30102: Part ID 0x%02X OK\n", (unsigned)partId);
    }
  }

  uint8_t oledAddress = 0;
  if (isI2CDevicePresent(Wire, kOledAddressPrimary)) {
    oledAddress = kOledAddressPrimary;
  } else if (isI2CDevicePresent(Wire, kOledAddressSecondary)) {
    oledAddress = kOledAddressSecondary;
  }

  if (oledAddress == 0) {
    Serial.println("OLED: NOT FOUND at 0x3C or 0x3D");
  } else {
    Serial.printf("OLED: found at 0x%02X\n", (unsigned)oledAddress);
    if (!display.begin(oledAddress)) {
      Serial.println("OLED: U8g2 initialization FAILED");
    } else {
      drawOledSelfTest();
      Serial.println("OLED: U8g2 self-test pattern sent");
    }
  }
}

void loop() {
  delay(1000);
}
