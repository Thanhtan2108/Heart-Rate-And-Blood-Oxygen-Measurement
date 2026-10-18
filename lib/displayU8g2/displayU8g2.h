#ifndef DISPLAY_U8G2_H_
#define DISPLAY_U8G2_H_

// Header chỉ include thứ mà các khai báo bên dưới cần (uint8_t, uint32_t, bool).
// U8g2lib.h được include trong displayU8g2.cpp để file gọi (main.cpp) không biết U8g2 tồn tại.
#include <Arduino.h>

// Khởi tạo OLED SSD1306 128x64 (I2C) một lần.
//   i2cAddress: địa chỉ 7-bit, đúng như scanI2C in ra (ví dụ 0x3C). Hàm tự đổi sang
//               dạng 8-bit mà U8g2 yêu cầu, nơi gọi không cần quan tâm.
//   clockHz   : tốc độ bus I2C tính bằng Hz (ví dụ 100000). Phải bằng tốc độ bus đang
//               dùng, vì U8g2 đặt lại clock của Wire ở mỗi lần truyền.
// Trả về: true  = OLED trả lời trên bus và đã khởi tạo xong.
//         false = không thấy OLED ở địa chỉ này, nơi gọi nên bỏ qua việc vẽ.
// Điều kiện tiên quyết: Wire.begin() đã được gọi.
bool initOledDisplay(uint8_t i2cAddress, uint32_t clockHz);

// Xóa màn hình rồi hiển thị một dòng chữ (ASCII) ở góc trên-trái.
//   text: chuỗi kết thúc bằng '\0'. Hàm chỉ đọc, không sửa chuỗi. nullptr bị bỏ qua.
// Lưu ý: hàm này chạm I2C (đẩy cả bộ đệm 1024 byte sang chip), sẽ chiếm bus một
// khoảng thời gian, cần bảo vệ bằng mutex I2C khi chuyển sang FreeRTOS.
// Điều kiện tiên quyết: initOledDisplay() đã trả về true.
void showOledText(const char* text);

#endif  // DISPLAY_U8G2_H_
