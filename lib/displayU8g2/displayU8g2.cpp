#include "displayU8g2.h"

#include <Wire.h>
#include <U8g2lib.h>

#include "scanI2C.h"

// Đối tượng U8g2 duy nhất của module, "riêng tư": static ở phạm vi file nên file khác
// không truy cập được. Là biến toàn cục để sống lâu hơn một lời gọi hàm.
// Constructor chỉ lưu cấu hình (xoay 0 độ, không có chân reset), chưa chạm phần cứng.
// Phần cứng chỉ bị chạm từ u8g2.begin() trở đi.
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Vị trí chữ: y là đường baseline (chân chữ), không phải mép trên.
// Với font ncenB14, baseline = 20 thì chữ nằm gọn ở nửa trên của màn hình.
static constexpr uint8_t kTextX         = 0;
static constexpr uint8_t kTextBaselineY = 20;

bool initOledDisplay(uint8_t i2cAddress, uint32_t clockHz) {
  // u8g2.begin() luôn trả true dù không có OLED, nên kiểm tra thật bằng ACK trên bus.
  if (!isI2CDevicePresent(Wire, i2cAddress)) {
    return false;
  }

  // Thứ tự bắt buộc: đặt clock và địa chỉ trước begin().
  u8g2.setBusClock(clockHz);
  // U8g2 dùng địa chỉ 8-bit (7-bit dịch trái 1). Phép dịch cho kết quả int nên ép về uint8_t.
  u8g2.setI2CAddress(static_cast<uint8_t>(i2cAddress << 1));
  u8g2.begin();
  return true;
}

void showOledText(const char* text) {
  if (text == nullptr) {
    return;  // drawStr với con trỏ null sẽ làm chương trình treo
  }

  u8g2.clearBuffer();                    // xóa bộ đệm trong RAM, chưa chạm I2C
  u8g2.setFont(u8g2_font_ncenB14_tr);    // chọn font (nằm trong flash)
  u8g2.drawStr(kTextX, kTextBaselineY, text);  // vẽ vào bộ đệm, màn hình chưa đổi
  u8g2.sendBuffer();                     // đẩy bộ đệm sang chip: lúc này chữ mới hiện
}
