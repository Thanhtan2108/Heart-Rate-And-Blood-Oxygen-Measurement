#include <Arduino.h>
#include <Wire.h>

#include "displayU8g2.h"
#include "scanI2C.h"

// Cấu hình bus I2C (theo sơ đồ đấu dây Bước 2)
constexpr uint8_t  kPinSda     = 21;
constexpr uint8_t  kPinScl     = 22;
constexpr uint32_t kI2cClockHz = 400000;

// Thiết bị kỳ vọng trên bus
constexpr uint8_t kMax30102Address        = 0x57;
constexpr uint8_t kMax30102PartIdRegister = 0xFF;
constexpr uint8_t kMax30102ExpectedPartId = 0x15;
constexpr uint8_t kOledAddressPrimary     = 0x3C;
constexpr uint8_t kOledAddressSecondary   = 0x3D;

// Bước 3: quét và in các địa chỉ có trên bus
static void printI2CScan() {
  const I2CScanResult scan = scanI2CBus(Wire);
  Serial.printf("Found %u device(s):", (unsigned)scan.count);
  for (uint8_t i = 0; i < scan.count; i++) {
    Serial.printf(" 0x%02X", (unsigned)scan.addresses[i]);
  }
  Serial.println();
}

// Bước 4: xác nhận đích danh MAX30102 bằng Part ID. Trả về true nếu đúng chip.
static bool verifyMax30102() {
  if (!isI2CDevicePresent(Wire, kMax30102Address)) {
    Serial.printf("MAX30102: NOT FOUND at 0x%02X\n", (unsigned)kMax30102Address);
    return false;
  }

  uint8_t partId = 0;
  if (!readI2CRegister(Wire, kMax30102Address, kMax30102PartIdRegister, partId)) {
    Serial.println("MAX30102: ACK ok but register read FAILED");
    return false;
  }
  if (partId != kMax30102ExpectedPartId) {
    Serial.printf("MAX30102: Part ID MISMATCH, read 0x%02X, expected 0x%02X\n",
                  (unsigned)partId, (unsigned)kMax30102ExpectedPartId);
    return false;
  }

  Serial.printf("MAX30102: Part ID 0x%02X OK\n", (unsigned)partId);
  return true;
}

// Tìm địa chỉ OLED. Trả về 0 nếu không thấy (địa chỉ hợp lệ luôn >= 0x08).
static uint8_t findOledAddress() {
  if (isI2CDevicePresent(Wire, kOledAddressPrimary)) {
    return kOledAddressPrimary;
  }
  if (isI2CDevicePresent(Wire, kOledAddressSecondary)) {
    return kOledAddressSecondary;
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== CP1: scan I2C + Part ID + OLED Hello ===");

  // Bus phải được cấu hình trước mọi lời gọi module
  Wire.begin(kPinSda, kPinScl);
  Wire.setClock(kI2cClockHz);

  printI2CScan();
  verifyMax30102();

  const uint8_t oledAddress = findOledAddress();
  if (oledAddress == 0) {
    Serial.println("OLED: NOT FOUND at 0x3C or 0x3D");
    return;
  }
  Serial.printf("OLED: found at 0x%02X\n", (unsigned)oledAddress);

  // Bước 5: khởi tạo OLED và hiện chữ
  if (!initOledDisplay(oledAddress, kI2cClockHz)) {
    Serial.println("OLED: init FAILED");
    return;
  }

  // Đo thời gian hiện chữ (gồm cả sendBuffer). Phép trừ uint32_t vẫn đúng khi micros() tràn số.
  const uint32_t startUs = micros();
  showOledText("Hello");
  const uint32_t elapsedUs = micros() - startUs;
  Serial.printf("OLED: Hello shown, showOledText took %lu us (~%lu ms)\n",
                (unsigned long)elapsedUs, (unsigned long)(elapsedUs / 1000));

  // Sau khi OLED truyền dữ liệu, MAX30102 trên cùng bus vẫn phải đọc được
  Serial.println("Re-check after OLED traffic:");
  verifyMax30102();
}

void loop() {
  delay(1000);
}
