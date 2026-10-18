#ifndef CONFIG_H_
#define CONFIG_H_

#include <Arduino.h>

// ===== Bus I2C dùng chung cho MAX30102 và OLED (theo sơ đồ đấu dây Bước 2) =====
constexpr uint8_t  kPinSda = 21;
constexpr uint8_t  kPinScl = 22;

// Tốc độ bus: chọn 400 kHz dựa trên số đo ở CP1.
//   100 kHz: đẩy cả màn hình (sendBuffer) mất ~114 ms, sát giới hạn 3 mẫu của bộ đệm
//            SparkFun ở 25 Hz (mỗi mẫu 40 ms), nguy cơ mất mẫu.
//   400 kHz: mất ~35 ms, MAX30102 vẫn đọc được Part ID ổn định.
constexpr uint32_t kI2cClockHz = 400000;

// ===== MAX30102 =====
constexpr uint8_t kMax30102Address        = 0x57;
constexpr uint8_t kMax30102PartIdRegister = 0xFF;
constexpr uint8_t kMax30102ExpectedPartId = 0x15;

// ===== OLED SSD1306: địa chỉ có thể là 0x3C hoặc 0x3D tùy module =====
constexpr uint8_t kOledAddressPrimary   = 0x3C;
constexpr uint8_t kOledAddressSecondary = 0x3D;

#endif  // CONFIG_H_
