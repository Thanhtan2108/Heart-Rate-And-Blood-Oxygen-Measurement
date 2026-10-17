#include <scanI2C.h>

bool isI2CDevicePresent(TwoWire& wire, uint8_t address) {
  wire.beginTransmission(address);
  return (wire.endTransmission() == 0);
}

I2CScanResult scanI2CBus(TwoWire& wire) {
  I2CScanResult result = {};
  for (uint8_t address = kScanFirstAddress; address <= kScanLastAddress; address++) {
        if (isI2CDevicePresent(wire, address)) {
      result.addresses[result.count] = address;
      result.count++;
    }
  }
  return result;
}

bool readI2CRegister(TwoWire& wire, uint8_t address, uint8_t reg, uint8_t& value) {
  wire.beginTransmission(address);
  wire.write(reg);
  if (wire.endTransmission(false) != 0) {
    return false;
  }
  const uint8_t kBytesToRead = 1;
  if (wire.requestFrom(address, kBytesToRead) != kBytesToRead) {
    return false;
  }
  value = wire.read();
  return true;
}
 