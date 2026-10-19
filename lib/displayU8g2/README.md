# displayU8g2

Module bọc thư viện U8g2 để điều khiển OLED SSD1306 128×64 qua I²C. Viết theo kiểu C: hàm tự do, đối tượng U8g2 là biến `static` riêng của file `.cpp`. Hàm gọi (`main.cpp`) không cần biết U8g2 tồn tại.

## API

```cpp
bool initOledDisplay(uint8_t i2cAddress, uint32_t clockHz);
void showOledText(const char* text);
```

| Hàm | Làm gì | Ghi chú |
| --- | --- | --- |
| `initOledDisplay` | Khởi tạo OLED một lần | `i2cAddress` là địa chỉ **7-bit** (như `scanI2C` in ra). Trả `true` khi OLED ACK trên bus. Cần `Wire.begin()` trước |
| `showOledText` | Xóa màn, hiện một dòng chữ ASCII ở góc trên-trái | **Chạm I2C** (đẩy 1024 byte), cần mutex I2C khi dùng FreeRTOS. `nullptr` bị bỏ qua |

## Quyết định thiết kế

| Quyết định | Lý do |
| --- | --- |
| Địa chỉ vào là 7-bit, đổi `<< 1` bên trong module | `main` nói cùng "ngôn ngữ" với `scanI2C`; chi tiết 8-bit của U8g2 được giấu |
| Kiểm tra ACK bằng `isI2CDevicePresent` | `u8g2.begin()` luôn trả `true` dù không có màn hình |
| `clockHz` truyền từ ngoài vào | U8g2 gọi `Wire.setClock()` ở mỗi lần truyền, nên phải trùng với clock của cả bus |
| Đối tượng `u8g2` là `static` trong `.cpp` | Chỉ có một OLED; tránh lộ kiểu của U8g2 ra header |
| Header không include `U8g2lib.h` | Header chỉ include thứ khai báo của nó cần |

## Số đo (CP1, ESP32 DOIT V1, OLED SSD1306, chỉ tính `showOledText` gồm `sendBuffer`)

| Clock bus | Thời gian |
| --- | --- |
| 100 kHz | 114 497 µs (~114 ms) |
| 400 kHz | 35 572 µs (~35 ms) |

Ở 25 Hz mỗi mẫu cảm biến cách nhau 40 ms, và bộ đệm phần mềm của thư viện SparkFun chỉ chứa 4 mẫu (an toàn tối đa 3 mẫu mỗi lần đọc). Vì vậy 100 kHz không an toàn khi vừa vẽ vừa lấy mẫu, còn 400 kHz thì có dư địa.

## Giới hạn đã biết

- Chỉ một màn hình, không có handle (chủ đích, đủ cho project).
- Chỉ ASCII, một dòng cố định, font `ncenB14`. Vẽ BPM, SpO₂ để CP2/CP3.
- U8g2 không báo lỗi truyền I²C, nên `showOledText` không có giá trị trả về.

Tài liệu U8g2 chi tiết: `docs/U8g2_Reference.md`.
