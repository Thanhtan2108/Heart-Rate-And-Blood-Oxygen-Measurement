# Máy đo nhịp tim và SpO₂ (ESP32 + MAX30102 + OLED)

## Tiến độ

| Mốc | Nội dung | Trạng thái |
| --- | --- | --- |
| **CP1** | scanI2C thấy MAX30102 + OLED, đọc Part ID 0x15, OLED hiện "Hello" | **Đạt** |
| CP2 | Max30102Driver + đọc raw IR/RED ổn định 10 phút | Chưa làm |
| CP3 | HeartRateEstimator (BPM 60–100) hiển thị OLED | Chưa làm |
| CP4 | FreeRTOS: 3 task, 2 queue, mutex I²C, chạy 10 phút không treo | Chưa làm |
| CP5 | Spo2Estimator, moving average, state machine, watchdog | Chưa làm |
| CP6 | Unit test native, README đầy đủ, video demo | Chưa làm |

## Phần cứng và đấu dây

Chỉ dùng nhãn in trên board.

| Nhãn trên module | Chức năng | Nối tới nhãn trên ESP32 |
| --- | --- | --- |
| MAX30102 `VIN` | Nguồn | `3V3` |
| MAX30102 `GND` | Mass | `GND` |
| MAX30102 `SDA` | I²C dữ liệu | `21` |
| MAX30102 `SCL` | I²C xung nhịp | `22` |
| MAX30102 `INT` | Ngắt (chưa dùng ở CP1) | `4` |
| MAX30102 `IRD`, `RD` | Không nối | – |
| OLED `VCC` | Nguồn | `3V3` |
| OLED `GND` | Mass | `GND` |
| OLED `SDA` | I²C dữ liệu | `21` |
| OLED `SCL` | I²C xung nhịp | `22` |

Địa chỉ I²C 7-bit: MAX30102 = `0x57`, OLED = `0x3C`.

## Cấu trúc thư mục

```text
platformio.ini
include/config.h             chân, tốc độ bus, địa chỉ thiết bị (nguồn duy nhất)
src/main.cpp                 bring-up CP1: quét bus, Part ID, OLED Hello
lib/scanI2C/                 quét bus, kiểm tra thiết bị, đọc thanh ghi (có README riêng)
lib/displayU8g2/             bọc U8g2: khởi tạo và hiện chữ lên OLED (có README riêng)
docs/                        tài liệu tra cứu SparkFun MAX3010x và U8g2
images/                      ảnh đấu dây
```

## Build và chạy

```bash
pio run                      biên dịch
pio run -t upload            nạp
pio device monitor           xem Serial (115200)
```

Kết quả Serial mong đợi:

```text
=== CP1: scan I2C + Part ID + OLED Hello ===
Found 2 device(s): 0x3C 0x57
MAX30102: Part ID 0x15 OK
OLED: found at 0x3C
OLED: Hello shown, showOledText took 35572 us (~35 ms)
Re-check after OLED traffic:
MAX30102: Part ID 0x15 OK
```

## Kết quả đo và quyết định (CP1)

`showOledText` (gồm `sendBuffer`, đẩy cả bộ đệm 1024 byte) đo trực tiếp trên phần cứng:

| Clock bus | Thời gian | So với chu kỳ mẫu 40 ms (25 Hz) |
| --- | --- | --- |
| 100 kHz | 114,5 ms | ~2,9 chu kỳ |
| 400 kHz | 35,6 ms | ~0,9 chu kỳ |

Thư viện SparkFun chỉ có bộ đệm phần mềm 4 mẫu (an toàn tối đa 3 mẫu mỗi lần `check()`). Với 100 kHz bus bị OLED chiếm gần hết giới hạn đó nên dễ mất mẫu. **Quyết định: dùng 400 kHz** (`kI2cClockHz` trong `include/config.h`), kèm điều kiện phải kiểm chứng đọc FIFO ổn định 10 phút ở CP2. Ở 400 kHz, Part ID MAX30102 vẫn đọc đúng trước và sau khi vẽ OLED.

## Quy ước

- Comment tiếng Việt, tên biến/hàm tiếng Anh.
- Không exception, không `std::vector`, không `new`/`delete` trong vòng lặp.
- Module viết kiểu C: hàm tự do, trạng thái là biến `static` ở phạm vi file.
- Thông số phần cứng chỉ khai báo một lần trong `include/config.h`.

## Việc tiếp theo (CP2)

`Max30102Driver`: khởi tạo SparkFun với `ledMode=2` (MAX30102 không có LED xanh), đọc mẫu theo `check → available → getFIFO → nextSample`, mỗi lần `check()` không để quá 3 mẫu dồn lại, rồi in raw IR/RED ra Serial và chạy 10 phút.

## Tài liệu tham khảo

- `docs/SparkFun_MAX3010x_Reference.md`: API, thanh ghi, FIFO, thuật toán Maxim.
- `docs/U8g2_Reference.md`: API, buffer, tương tác U8g2 với Wire và MAX30102.
- `lib/scanI2C/README.md`: lý thuyết I²C và từng dòng code của module quét bus.
