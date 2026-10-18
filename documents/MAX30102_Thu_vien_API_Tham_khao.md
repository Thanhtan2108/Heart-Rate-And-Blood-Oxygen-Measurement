# Tài liệu tra cứu — SparkFun MAX3010x Pulse and Proximity Sensor Library

> **Phiên bản đã đối chiếu:** 1.1.2, theo `library.properties` trong PlatformIO (`SparkFun MAX3010x Pulse and Proximity Sensor Library`).
> **Phạm vi:** API Arduino/C++ của thư viện SparkFun; source xác nhận sử dụng với MAX30102 và MAX30105. Không suy rộng khả năng tương thích sang mọi chip chỉ vì tên thư viện là MAX3010x.
> **Ngữ cảnh project:** `platformio.ini` khai báo dependency `sparkfun/SparkFun MAX3010x Pulse and Proximity Sensor Library@^1.1.2`. `src/main.cpp` hiện dùng `Wire` trên GPIO 21/22 ở 100 kHz để scan I²C và đọc Part ID 0x15 ở mức thấp; chưa khởi tạo lớp `MAX30105` hay thuật toán nhịp tim/SpO₂. Tài liệu này mô tả API tham khảo, không khẳng định các tính năng đó đã được tích hợp.

Khi cần xác nhận chữ ký/hành vi cho đúng phiên bản, ưu tiên source PlatformIO tại `.pio/libdeps/esp32doit-devkit-v1/SparkFun MAX3010x Pulse and Proximity Sensor Library/`, đặc biệt `src/MAX30105.h`, `src/MAX30105.cpp`, `src/heartRate.*`, `src/spo2_algorithm.*` và ví dụ đi kèm.

## 1. Mô hình dữ liệu và nguyên tắc sử dụng

MAX30102 đo ánh sáng phản xạ qua LED đỏ/IR và photodiode. Các giá trị Red/IR trả về là mẫu ADC thô, không phải BPM hay SpO₂ đã hiệu chuẩn.

Luồng FIFO thường dùng:

1. `begin()` khởi tạo giao tiếp và đọc Part ID.
2. `setup()` cấu hình mode, FIFO, ADC, sample rate, pulse width và dòng LED.
3. Gọi `check()` thường xuyên để chuyển mẫu từ FIFO phần cứng sang buffer staging của thư viện.
4. Lặp `while (available())`: đọc các kênh trên cùng record bằng `getFIFORed()`/`getFIFOIR()`, xử lý hoặc lưu lại, rồi gọi `nextSample()` đúng một lần.
5. Đưa dữ liệu đã lưu vào bộ lọc/thuật toán; hiển thị ở một bước riêng.

### Giới hạn FIFO quan trọng

- FIFO phần cứng có 32 record; buffer phần mềm trong source chỉ có `STORAGE_SIZE = 4`.
- `check()` đọc record phần cứng rồi ghi vòng vào buffer phần mềm, không từ chối ghi khi buffer chưa được tiêu thụ. `available()` tính khoảng cách hai con trỏ theo modulo 4 và không có bộ đếm overflow riêng.
- Nếu đúng 4 mẫu được ghi khi `head` bằng `tail`, hai con trỏ lại bằng nhau và `available()` báo 0 dù buffer vừa được ghi đủ một vòng. Nếu ghi nhiều hơn, dữ liệu có thể bị ghi đè.
- Hãy gọi `check()` và tiêu thụ staging buffer thường xuyên. Không in Serial, render OLED, delay dài hoặc chạy thuật toán tốn thời gian trước khi lấy hết mẫu đang chờ. Nếu cần giữ cửa sổ mẫu dài, copy các cặp Red/IR sang buffer riêng của ứng dụng.
- `clearFIFO()` chỉ đặt lại các con trỏ/bộ đếm FIFO phần cứng; không xóa `sense.head`, `sense.tail` hoặc buffer staging phần mềm.

MAX30102 không có LED xanh như MAX30105; dùng Red + IR (`ledMode=2`) cho ứng dụng nhịp tim/SpO₂. Không coi `getGreen()`/`getFIFOGreen()` là kênh dữ liệu hữu ích trên MAX30102.

## 2. Khởi tạo và cấu hình

```cpp
#include <Wire.h>
#include "MAX30105.h"

MAX30105 sensor;  // Khai báo global/static như các ví dụ của thư viện

void setup() {
  if (!sensor.begin(Wire, I2C_SPEED_STANDARD)) {
    // Báo lỗi và xử lý trạng thái sensor không sẵn sàng
  }
  sensor.setup(0x1F, 4, 2, 100, 411, 4096);
}
```

Constructor của lớp rỗng, không khởi tạo buffer/con trỏ. Đối tượng global/static được zero-initialize; tránh tạo đối tượng cục bộ chưa khởi tạo rồi gọi API.

### `begin(TwoWire&, uint32_t i2cSpeed, uint8_t i2caddr)`

- Mặc định dùng `Wire`, 100 kHz (`I2C_SPEED_STANDARD`) và địa chỉ 7-bit `0x57`.
- Gọi `wirePort.begin()` không truyền pin rồi gọi `setClock(i2cSpeed)`. Nếu dùng chân remap hoặc bus đã khởi tạo tùy chỉnh, xác minh hành vi của Arduino core trên board mục tiêu. Project hiện khởi tạo `Wire.begin(21, 22)` trước khi gọi helper scan; nếu dùng driver thư viện cần xác nhận việc `begin()` lặp lại không làm thay đổi cấu hình cần thiết.
- Trả `true` khi Part ID đọc được khớp 0x15; `false` nếu không. Đây là kiểm tra giao tiếp/Part ID, không phải xác minh toàn bộ phần cứng quang học.
- **Không dựa vào địa chỉ tùy chỉnh cho FIFO:** `begin()` lưu `i2caddr` cho nhiều thao tác thanh ghi, nhưng `check()` trong source 1.1.2 truy cập FIFO bằng hằng `MAX30105_ADDRESS` (0x57). Dùng địa chỉ mặc định khi cần đọc FIFO hoặc kiểm tra/sửa driver.

### `setup(powerLevel, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange)`

`setup()` reset mềm, cấu hình FIFO/mode/particle settings, đặt dòng LED/slot, rồi xóa FIFO phần cứng. Nên dùng thay vì tự gọi các setter thanh ghi thấp tầng.

| Tham số | Giá trị/ý nghĩa |
| --- | --- |
| `powerLevel` | 0–255, mã dòng LED: 0 tắt; source comment mốc 0x02 ≈ 0,4 mA, 0x1F ≈ 6,4 mA, 0x7F ≈ 25,4 mA, 0xFF ≈ 50 mA. Dòng cao có thể bão hòa ADC/tăng công suất; phải hiệu chỉnh theo module và tiếp xúc thực tế. |
| `sampleAverage` | 1, 2, 4, 8, 16, 32. Số mẫu cảm biến gộp/trung bình thành một FIFO record. Giá trị khác bị thay bằng 4. |
| `ledMode` | Chỉ dùng 1=Red, 2=Red+IR, 3=Red+IR+Green. Với MAX30102 thường dùng 2. **Source không validate:** giá trị khác có thể làm `activeLEDs` sai và khiến `check()` đọc sai số byte mỗi record. |
| `sampleRate` | Mẫu/s nội bộ: 50, 100, 200, 400, 800, 1000, 1600, 3200. Source ánh xạ theo các ngưỡng; nên truyền đúng giá trị được hỗ trợ. |
| `pulseWidth` | 69, 118, 215, 411 µs; ảnh hưởng độ phân giải ADC theo comment source (15–18 bit) và thời gian phát LED. |
| `adcRange` | 2048, 4096, 8192, 16384 (dải dòng quang danh định nA theo quy ước thư viện). Giá trị ngoài các mức xử lý có thể rơi về cấu hình khác; dùng đúng một trong bốn mức. |

Ví dụ cấu hình 100 sps với average 4 cho record FIFO hiệu dụng xấp xỉ 25 record/s, phù hợp giả định cửa sổ 100 mẫu/4 giây trong ví dụ SpO₂. Đây là điểm bắt đầu, không phải cấu hình y tế đã hiệu chuẩn.

Prototype header mặc định `sampleRate=400`; comment triển khai nói mặc định 50. Khi dùng mặc định, tin chữ ký header thực tế, không suy ra từ comment.

## 3. Đọc dữ liệu

### Getter nhanh

```cpp
uint32_t getRed();
uint32_t getIR();
uint32_t getGreen();
```

Mỗi getter gọi `safeCheck(250)` và chờ tối đa khoảng 250 ms để tìm dữ liệu mới; timeout trả 0. Đây là cách tiện cho thử nghiệm/plotter, nhưng blocking và các lần gọi Red/IR riêng có thể trả về các thời điểm khác nhau. Không diễn giải 0 chắc chắn là mẫu cảm biến bằng 0.

### FIFO — dùng cho mẫu đồng bộ

```cpp
uint16_t check();
uint8_t available();
uint32_t getFIFORed();
uint32_t getFIFOIR();
void nextSample();
```

```cpp
sensor.check();
while (sensor.available()) {
  const uint32_t red = sensor.getFIFORed();
  const uint32_t ir = sensor.getFIFOIR();
  // Lưu/xử lý hai kênh của cùng FIFO record
  sensor.nextSample();
}
```

`getFIFO...()` đọc ở con trỏ `tail`, không tiêu thụ record. `nextSample()` dịch `tail` nếu đang có mẫu. Gọi `check()` thường xuyên; giá trị trả về là số mẫu đọc từ FIFO phần cứng ở lần kiểm tra, không phải đảm bảo buffer phần mềm còn lưu được đủ số đó.

`safeCheck(uint8_t maxTimeToCheck)` polling `check()` và delay 1 ms cho tới timeout hoặc thấy mẫu; tham số bị giới hạn bởi kiểu `uint8_t` (0–255 ms) và hàm chặn luồng gọi.

## 4. Setter thấp tầng và các cảnh báo

- `setLEDMode(uint8_t)` nhận **mã trường thanh ghi**, không phải quy ước `1/2/3` của `setup()`: 0x02 Red-only, 0x03 Red+IR, 0x07 Multi-LED trong source. Hàm chỉ ghi mode register, không cập nhật `activeLEDs` và không cấu hình slot. **Không dùng nó để đổi số kênh đang đọc sau `setup()`**; cấu hình lại đầy đủ và xử lý FIFO cũ trước khi tiếp tục.
- `setADCRange()`, `setSampleRate()`, `setPulseWidth()` nhận mã bit-field thanh ghi, không nhận các số có nghĩa thuận tiện (`4096`, `100`, `411`) mà `setup()` nhận.
- `setFIFOAverage()` nhận mã trường (0x00/20/40/60/80/A0 cho 1/2/4/8/16/32), không phải số mẫu trực tiếp.
- `setPulseAmplitudeRed/IR/Green/Proximity(uint8_t)` đặt mã amplitude 8-bit. Không mặc định dùng mức tối đa.
- `enableFIFORollover()` điều khiển rollover FIFO phần cứng; không giải quyết overflow buffer phần mềm 4 record.
- `setFIFOAlmostFull(uint8_t)` nhận trường 4-bit ngược: 0x00 ≈ 32 record, 0x0F ≈ 17 record theo comment source.
- `enableSlot()`/`disableSlots()` là cấu hình Multi-LED thấp tầng; các mã slot không phải API enum công khai, cần xem source/datasheet trước khi dùng.
- `readRegister8()`/`writeRegister8()` dành cho debug/mở rộng thanh ghi; ưu tiên API có ý nghĩa trước, và đối chiếu datasheet khi thao tác trực tiếp.

## 5. Ngắt, nhiệt độ, ID

`enableAFULL()`, `enableDATARDY()`, `enableALCOVF()`, `enablePROXINT()`, `enableDIETEMPRDY()` chỉ bật bit trong thanh ghi. Chúng không tự gắn ISR Arduino hay cấu hình pin INT. `getINT1()`/`getINT2()` trả bitmask trạng thái; đọc status có thể xóa cờ theo hành vi chip.

### Nhiệt độ die

Sau `begin()` và cấu hình sensor, gọi `enableDIETEMPRDY()` trước `readTemperature()` như ví dụ thư viện. `readTemperature()` kích hoạt phép đo và chờ tối đa khoảng 100 ms; source hiện vẫn đọc thanh ghi nhiệt độ sau timeout, chưa trả sentinel lỗi đáng tin cậy. Giá trị là nhiệt độ die, không phải nhiệt độ cơ thể/môi trường đã hiệu chuẩn. `readTemperatureF()` đổi °C sang °F; sentinel `-999` được kiểm tra nhưng không được bảo đảm bởi timeout hiện tại.

`readPartID()` đọc Part ID trực tiếp (kỳ vọng 0x15); `getRevisionID()` trả revision đã lưu ở `begin()`.

## 6. Thuật toán đi kèm

### `checkForBeat(int32_t sample)`

Nằm trong `heartRate.h`; nhận từng mẫu IR thô và trả `true` khi heuristic phát hiện beat. Không tự đọc sensor, không trả BPM và không tính SpO₂. Bộ lọc có trạng thái tĩnh/toàn cục, không có API reset công khai; cần cân nhắc nếu khởi động lại phép đo hoặc đổi cấu hình lấy mẫu. Chuyển động, tiếp xúc, ánh sáng và perfusion ảnh hưởng mạnh đến độ tin cậy.

### `maxim_heart_rate_and_oxygen_saturation(...)`

Nằm trong `spo2_algorithm.h`; nhận hai buffer IR/Red cùng chiều dài và đồng bộ, ghi SpO₂, BPM cùng cờ valid ra các tham số. Có prototype `uint16_t*` trên AVR nhỏ và `uint32_t*` trên kiến trúc khác. Kiểm tra `validSPO2` và `validHeartRate` trước khi dùng kết quả; source đặt SpO₂ `-999` khi không hợp lệ.

Source đặt `FreqS=25`, `BUFFER_SIZE=FreqS*4=100`, `MA4_SIZE=4` (comment yêu cầu không đổi). Cửa sổ 100 record tương đương khoảng 4 giây chỉ khi tốc độ record hiệu dụng đúng 25 Hz. Nếu thay sample rate/average hoặc mất mẫu, thời lượng cửa sổ và kết quả có thể sai. Đây là thuật toán tham khảo, không phải thiết bị/chỉ số chẩn đoán y tế.

## 7. Quy trình khuyến nghị cho project

1. Khai báo đối tượng `MAX30105` ở global/static.
2. Khởi tạo I²C/pin/clock phù hợp ESP32; gọi `begin()` và xử lý lỗi rõ ràng.
3. Gọi `setup(..., ledMode=2, ...)` cho MAX30102; bắt đầu với dòng LED vừa phải.
4. Trong vòng thu thập, gọi `check()` và đọc hết `available()` ngay; copy cặp Red/IR vào buffer ứng dụng riêng nếu cần thuật toán cửa sổ.
5. Tách thu thập, xử lý tín hiệu và vẽ OLED thành các bước riêng; tránh `Serial`/OLED/blocking trong thời gian phải drain FIFO.
6. Chỉ cập nhật BPM/SpO₂ nếu cờ valid đúng; biểu diễn rõ trạng thái “chưa có kết quả/không hợp lệ”.
7. Kiểm tra chất lượng tín hiệu và cấu hình thực tế; raw ADC không phải đơn vị sinh lý.

## 8. Source và ví dụ cần tra

- `src/MAX30105.h`, `src/MAX30105.cpp`: API, giao tiếp, cấu hình, FIFO và staging buffer.
- `src/heartRate.h`, `src/heartRate.cpp`: phát hiện beat.
- `src/spo2_algorithm.h`, `src/spo2_algorithm.cpp`: ước lượng BPM/SpO₂.
- `examples/Example1_Basic_Readings`, `Example6_FIFO_Readings`, `Example8_SPO2`, `Example3_Temperature_Sense`: luồng đọc cơ bản, FIFO, thuật toán cửa sổ, nhiệt độ.
- Repository: <https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library>
