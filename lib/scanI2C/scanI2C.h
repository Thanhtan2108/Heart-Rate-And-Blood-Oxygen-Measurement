#ifndef SCAN_I2C_H_
#define SCAN_I2C_H_

#include <Arduino.h>
#include <Wire.h>

// Dải địa chỉ 7-bit dùng cho thiết bị thông thường (0x00-0x07 và 0x78-0x7F là địa chỉ dành riêng)
constexpr uint8_t kScanFirstAddress = 0x08;
constexpr uint8_t kScanLastAddress  = 0x77;

// Số địa chỉ tối đa có thể quét: mảng đủ chứa mọi trường hợp nên không bao giờ tràn
constexpr uint8_t kScanMaxDevices = kScanLastAddress - kScanFirstAddress + 1;

// Kết quả một lần quét bus
struct I2CScanResult {
  uint8_t addresses[kScanMaxDevices];  // các địa chỉ có thiết bị phản hồi, tăng dần
  uint8_t count;                       // số phần tử hợp lệ trong mảng
};
// Trả về true nếu thiết bị tại địa chỉ này phản hồi ACK
bool isI2CDevicePresent(TwoWire& wire, uint8_t address);

// Quét toàn bộ dải địa chỉ, trả về danh sách thiết bị phản hồi ACK
I2CScanResult scanI2CBus(TwoWire& wire);

// Đọc 1 byte từ thanh ghi của thiết bị; trả về false nếu giao tiếp thất bại
bool readI2CRegister(TwoWire& wire, uint8_t address, uint8_t reg, uint8_t& value);

#endif
