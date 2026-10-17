# Module `scanI2C`

Tài liệu lý thuyết + code của module quét và xác thực thiết bị trên bus I2C.
Cấu trúc xuyên suốt: **kiến thức → yêu cầu rút ra → dòng code**.

- Vị trí: `lib/scanI2C/` (gồm `scanI2C.h`, `scanI2C.cpp`, file này)
- Framework: Arduino Core trên ESP32 (PlatformIO), dùng thư viện `Wire`
- Thuộc checkpoint: CP1 (bring-up: tìm thấy 0x57 và 0x3C/0x3D, đọc Part ID 0x15)
- Phụ thuộc Arduino (`Arduino.h`, `Wire.h`), nên khi thêm env `native` ở CP6 phải đưa module này vào `lib_ignore`

## Module làm gì và không làm gì

**Làm:**

1. Quét dải địa chỉ I2C thông thường, trả về danh sách thiết bị có phản hồi.
2. Kiểm tra một địa chỉ cụ thể có thiết bị hay không.
3. Đọc 1 byte từ một thanh ghi của thiết bị (dùng để đọc Part ID).

**Không làm (cố ý):**

- Không gọi `Wire.begin()`. Chân SDA/SCL và tốc độ bus do nơi gọi quyết định.
- Không biết gì về MAX30102 hay OLED. Việc so Part ID với 0x15, hoặc kiểm tra OLED ở 0x3C hay 0x3D, nằm ở `main.cpp` (về sau là driver).

---

## Phần 1. Kiến thức nền về I2C

## 1.1. Địa chỉ 7-bit và dải địa chỉ được dùng

Trên bus I2C, master gọi thiết bị bằng địa chỉ 7-bit, nên về lý thuyết có 128 giá trị (0x00–0x7F). Theo đặc tả I2C, nhiều giá trị đã được dành riêng:

| Địa chỉ 7-bit | Dành cho |
| --- | --- |
| 0x00 | General call (gọi chung tất cả thiết bị) |
| 0x01 | Địa chỉ CBUS (chuẩn bus cũ) |
| 0x02, 0x03 | Dành riêng cho định dạng bus khác / tương lai |
| 0x04–0x07 | Mã master của chế độ Hs (high-speed) |
| 0x78–0x7B | Tiền tố địa chỉ 10-bit |
| 0x7C–0x7F | Dành riêng (device ID) |

Thiết bị thông thường chỉ được gán địa chỉ trong **0x08–0x77**. Quét ngoài dải này là vô nghĩa.

## 1.2. Địa chỉ 7-bit và địa chỉ 8-bit trong datasheet

Trên bus, master gửi một byte gồm 7 bit địa chỉ cộng 1 bit đọc/ghi (R/W). Nhiều datasheet ghi địa chỉ dạng 8-bit:

- MAX30102: ghi 0xAE, đọc 0xAF. Dịch bỏ bit cuối: 0xAE >> 1 = **0x57**.
- OLED SSD1306: 0x78 (8-bit) tương ứng 0x78 >> 1 = **0x3C** (7-bit).

Thư viện `Wire` của Arduino nhận địa chỉ **7-bit** và tự ghép bit R/W. Vì vậy trong project dùng 0x57 và 0x3C, không dùng 0xAE hay 0x78.

## 1.3. ACK và NACK

Sau byte địa chỉ, ở xung nhịp thứ 9, thiết bị nào mang địa chỉ đó sẽ kéo SDA xuống thấp. Đó là **ACK**. Không ai kéo SDA thì gọi là **NACK**. Quét bus chính là gọi lần lượt từng địa chỉ và ghi lại địa chỉ nào có ACK.

ACK chỉ nói "có thiết bị nào đó ở địa chỉ này". Muốn chắc đó là đúng chip thì cần đọc **Part ID**, một thanh ghi chỉ-đọc chứa số nhận dạng chip. OLED thường không đọc ngược được ID qua I2C, nên với OLED chỉ xác nhận được "có ACK".

## 1.4. Giao thức đọc một thanh ghi

Chip dạng thanh ghi hoạt động theo hai pha:

1. Master gọi thiết bị ở chế độ ghi và gửi **địa chỉ thanh ghi** cần đọc. Chip ghi nhớ vào một con trỏ nội bộ.
2. Master gọi lại thiết bị ở chế độ đọc, chip gửi về byte tại thanh ghi mà con trỏ chỉ đến.

Giữa hai pha có thể gửi STOP rồi START lại, hoặc gửi **repeated START** (START mới mà không STOP trước). Repeated START giữ quyền kiểm soát bus liên tục giữa hai pha, là cách chuẩn để đọc thanh ghi.

---

## Phần 2. `scanI2C.h`: từ lý thuyết ra từng dòng code

## 2.1. Chống nạp header nhiều lần

`#include` chép nguyên văn nội dung file vào chỗ gọi. Mỗi file `.cpp` là một **translation unit** biên dịch riêng. Lỗi redefinition chỉ xảy ra khi **trong cùng một translation unit** header bị nạp hai lần, ví dụ `main.cpp` include `a.h` và `b.h`, mà cả hai cùng include `scanI2C.h`.

Cơ chế của bộ ba `#ifndef / #define / #endif`: lần đầu macro chưa tồn tại, nên nó được định nghĩa và nội dung bên dưới được giữ lại. Lần sau macro đã tồn tại nên preprocessor bỏ qua mọi thứ từ `#ifndef` đến `#endif`.

Lưu ý đặt tên macro: tên bắt đầu bằng dấu gạch dưới theo sau là chữ hoa (như `_SCAN_I2C_H_`) được dành riêng cho compiler và thư viện chuẩn. Dùng `SCAN_I2C_H_` (không gạch dưới ở đầu), mỗi header một tên riêng theo tên file.

| | `#ifndef / #define / #endif` | `#pragma once` |
| --- | --- | --- |
| Thuộc chuẩn C++ | Có, chạy mọi compiler | Không, nhưng GCC/Clang/MSVC hỗ trợ |
| Cách nhận biết "đã nạp" | Theo tên macro (đặt trùng tên sẽ bỏ qua nhau) | Theo chính file |

Dự án này chọn `#ifndef`.

```cpp
#ifndef SCAN_I2C_H_
#define SCAN_I2C_H_
```

## 2.2. Include `Arduino.h`

Trong sketch `.ino`, Arduino IDE tự thêm `Arduino.h`. Trong PlatformIO, file `.h` và `.cpp` **không** được thêm tự động, nên mỗi file dùng API của framework phải tự include.

`Arduino.h` cung cấp các hàm, macro và kiểu của framework (`pinMode`, `millis`, `HIGH`/`LOW`, `Serial`, `Print`, `uint8_t`...). Nó **không thêm từ khóa** vào C++ (`constexpr`, `struct` vẫn là C++ chuẩn) và **không kéo theo `Wire`**.

```cpp
#include <Arduino.h>
```

## 2.3. Include `Wire.h`

Các hàm của module làm việc với bus I2C. Bus I2C trong Arduino core là một đối tượng thuộc lớp `TwoWire`, và biến `Wire` quen thuộc là một đối tượng như vậy. Lớp này nằm trong `Wire.h`. Header nào khai báo hàm có tham số kiểu `TwoWire` phải include `Wire.h`.

```cpp
#include <Wire.h>
```

Nguyên tắc: header chỉ include những gì **phần khai báo của chính nó** cần. Phần cài đặt dùng thêm gì thì include ở `.cpp`.

## 2.4. Dải địa chỉ cần quét

Từ mục 1.1, ta cần hai mốc để vòng quét biết điểm đầu và điểm cuối. Địa chỉ 7-bit nằm trong 0–127 nên vừa 1 byte, dùng `uint8_t`. Giá trị biết trước lúc biên dịch và không đổi, nên dùng `constexpr`: compiler tự điền số, không chiếm RAM, và có kiểu rõ ràng (khác `#define`).

```cpp
// Dải địa chỉ 7-bit dùng cho thiết bị thông thường (0x00-0x07 và 0x78-0x7F là địa chỉ dành riêng)
constexpr uint8_t kScanFirstAddress = 0x08;
constexpr uint8_t kScanLastAddress  = 0x77;
```

## 2.5. Kích thước mảng chứa kết quả

Cần nhớ những địa chỉ có thiết bị trả lời. Project cấm `vector` và `new`, nên dùng mảng cố định. Số địa chỉ quét là 0x77 − 0x08 + 1 = **112** (cộng 1 vì cả hai đầu mút đều được quét). Ngay cả khi bus lỗi khiến mọi địa chỉ đều trả lời, kết quả cũng không vượt quá 112. Mảng đúng 112 ô thì không bao giờ tràn, và chỉ tốn 112 byte RAM. Vì vậy không cần cờ báo tràn. Tính từ hai hằng số ở 2.4 để đổi dải quét thì kích thước tự cập nhật.

```cpp
// Số địa chỉ tối đa có thể quét: mảng đủ chứa mọi trường hợp nên không bao giờ tràn
constexpr uint8_t kScanMaxDevices = kScanLastAddress - kScanFirstAddress + 1;
```

## 2.6. Cấu trúc kết quả quét

Mảng 112 ô thường chỉ có 2 ô hợp lệ. Mảng C/C++ không tự biết bao nhiêu ô đang hợp lệ, nên phải đi kèm biến đếm. Hàm C++ không trả về trực tiếp một mảng thô, nên ta gói mảng và biến đếm vào `struct`. Vòng quét đi từ địa chỉ thấp lên cao nên mảng tự có thứ tự tăng dần. `count` tối đa 112 nên `uint8_t` đủ.

```cpp
// Kết quả một lần quét bus
struct I2CScanResult {
  uint8_t addresses[kScanMaxDevices];  // các địa chỉ có thiết bị phản hồi, tăng dần
  uint8_t count;                       // số phần tử hợp lệ trong mảng
};
```

## 2.7. Prototype `isI2CDevicePresent`

Cơ chế ở mục 1.3. Hàm trả `bool` vì câu hỏi chỉ là "có ACK hay không". Tham số `TwoWire& wire`: ESP32 có hai bộ I2C (`Wire` và `Wire1`), nên module không gắn cứng vào `Wire`. Dấu `&` là tham chiếu: dùng chính đối tượng của bên gọi, không sao chép.

```cpp
// Trả về true nếu thiết bị tại địa chỉ này phản hồi ACK
bool isI2CDevicePresent(TwoWire& wire, uint8_t address);
```

## 2.8. Prototype `scanI2CBus`

Quét là gọi `isI2CDevicePresent` cho từng địa chỉ trong dải, rồi lưu các địa chỉ có ACK. Hàm cần biết bus nào để quét, và trả về `I2CScanResult`. Kết quả khoảng 113 byte, nằm trong stack của nơi gọi. Ở CP4, mỗi task FreeRTOS có stack riêng, cần nhớ con số này khi tính kích thước stack.

```cpp
// Quét toàn bộ dải địa chỉ, trả về danh sách thiết bị phản hồi ACK
I2CScanResult scanI2CBus(TwoWire& wire);
```

## 2.9. Prototype `readI2CRegister`

Từ mục 1.4, hàm cần bốn thứ: bus, địa chỉ thiết bị, địa chỉ thanh ghi, và nơi nhận giá trị. Hàm trả `bool` để báo thành công hay thất bại. Nếu trả thẳng giá trị đọc được thì không phân biệt được "đọc lỗi" với "đọc được 0". Vì vậy giá trị đi ra qua tham số tham chiếu `uint8_t& value`.

```cpp
// Đọc 1 byte từ thanh ghi của thiết bị; trả về false nếu giao tiếp thất bại
bool readI2CRegister(TwoWire& wire, uint8_t address, uint8_t reg, uint8_t& value);
```

## 2.10. Đóng header

`#endif` nằm cuối file. Mọi thứ khác phải nằm giữa `#ifndef` và `#endif`.

```cpp
#endif  // SCAN_I2C_H_
```

## 2.11. File `scanI2C.h` hoàn chỉnh

```cpp
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

#endif  // SCAN_I2C_H_
```

---

## Phần 3. `scanI2C.cpp`: từ lý thuyết ra từng dòng code

## 3.1. Nối `.cpp` với `.h`

Compiler biên dịch mỗi `.cpp` riêng lẻ và chỉ biết những gì đã khai báo trong file đó. Để `.cpp` thấy `I2CScanResult`, các hằng số và prototype, phải include header của module. Dùng ngoặc kép `" "` vì nó tìm trong thư mục của file hiện tại trước. Header đã kéo theo `Arduino.h` và `Wire.h` nên không cần lặp lại.

```cpp
#include "scanI2C.h"
```

## 3.2. Cài `isI2CDevicePresent`

Trong Arduino, việc kiểm tra ACK chia thành hai lời gọi:

- `beginTransmission(address)` chỉ chuẩn bị gói trong bộ đệm, chưa có gì chạy trên dây.
- `endTransmission()` mới thực sự phát START, địa chỉ, rồi STOP. Nó trả về một mã: **0 là thành công (có ACK)**, khác 0 là lỗi (theo tài liệu Arduino: 1 dữ liệu quá dài, 2 NACK ở địa chỉ, 3 NACK ở dữ liệu, 4 lỗi khác; core ESP32 còn có mã timeout, xem mục "Cần tự kiểm chứng").

Ta không gửi byte dữ liệu nào, chỉ gửi địa chỉ, nên so mã trả về với 0 là đủ. Hàm này không gọi `wire.begin()`: bên gọi phải `begin()` trước khi quét.

```cpp
// Trả về true nếu thiết bị tại địa chỉ này phản hồi ACK
bool isI2CDevicePresent(TwoWire& wire, uint8_t address) {
  wire.beginTransmission(address);
  return (wire.endTransmission() == 0);
}
```

## 3.3. Cài `scanI2CBus`

**Khởi tạo kết quả.** Biến cục bộ trong C++ không tự về 0, nó chứa rác của vùng stack. Nếu `count` mang giá trị rác thì kết quả sai ngay từ đầu. Cú pháp `= {}` đặt mọi trường về 0, kể cả các ô chưa dùng của mảng.

**Vòng quét.** Đi từ `kScanFirstAddress` đến `kScanLastAddress`, biến lặp `uint8_t` cho khớp kiểu địa chỉ. Điều kiện là `<=` vì địa chỉ cuối cũng được quét. Cạm bẫy cần biết: nếu mốc cuối là 0xFF thì vòng `uint8_t` chạy mãi (255 + 1 quay về 0). Ở đây mốc cuối là 0x77 nên an toàn.

**Ghi kết quả.** Nếu có ACK, lưu địa chỉ vào ô trống đầu tiên. `result.count` vừa là số phần tử đã có, vừa là chỉ số của ô trống kế tiếp, nên dùng nó làm chỉ số rồi tăng lên. Không cần kiểm tra tràn vì mục 2.5 đã chứng minh mảng luôn đủ chỗ.

**Trả về.** Struct trả về theo giá trị (khoảng 113 byte), compiler thường tối ưu để tránh sao chép thật.

```cpp
// Quét toàn bộ dải địa chỉ, trả về danh sách thiết bị phản hồi ACK
I2CScanResult scanI2CBus(TwoWire& wire) {
  I2CScanResult result = {};  // khởi tạo toàn bộ về 0

  for (uint8_t address = kScanFirstAddress; address <= kScanLastAddress; address++) {
    if (isI2CDevicePresent(wire, address)) {
      result.addresses[result.count] = address;
      result.count++;
    }
  }
  return result;
}
```

## 3.4. Cài `readI2CRegister`

Theo giao thức ở mục 1.4, gồm hai pha.

**Pha 1: ghi địa chỉ thanh ghi.** `wire.write(reg)` đặt byte địa chỉ thanh ghi vào bộ đệm. `endTransmission(false)` phát đi **mà không gửi STOP** (để pha 2 dùng repeated START). Nếu mã trả về khác 0 thì dừng ngay và báo thất bại.

**Pha 2: yêu cầu đọc.** `requestFrom(address, số_byte)` phát repeated START ở chế độ đọc rồi nhận về số byte yêu cầu. Nó trả về **số byte thực sự nhận được**, nên so với số byte đã yêu cầu để biết có đủ không. Số byte đặt thành hằng có tên kiểu `uint8_t` để compiler chọn đúng bản nạp chồng của `requestFrom`.

**Lấy byte.** `wire.read()` lấy byte trong bộ đệm nhận, ghi vào `value` (tham chiếu tới biến của bên gọi) rồi trả `true`. Vì vậy `false` nghĩa là giao tiếp lỗi, còn `true` kèm `value = 0` là chip thật sự trả về 0.

```cpp
// Đọc 1 byte từ thanh ghi: ghi địa chỉ thanh ghi (repeated START), rồi đọc 1 byte
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
```

## 3.5. File `scanI2C.cpp` hoàn chỉnh

```cpp
#include "scanI2C.h"

// Trả về true nếu thiết bị tại địa chỉ này phản hồi ACK
bool isI2CDevicePresent(TwoWire& wire, uint8_t address) {
  wire.beginTransmission(address);
  return (wire.endTransmission() == 0);
}

// Quét toàn bộ dải địa chỉ, trả về danh sách thiết bị phản hồi ACK
I2CScanResult scanI2CBus(TwoWire& wire) {
  I2CScanResult result = {};  // khởi tạo toàn bộ về 0

  for (uint8_t address = kScanFirstAddress; address <= kScanLastAddress; address++) {
    if (isI2CDevicePresent(wire, address)) {
      result.addresses[result.count] = address;
      result.count++;
    }
  }
  return result;
}

// Đọc 1 byte từ thanh ghi: ghi địa chỉ thanh ghi (repeated START), rồi đọc 1 byte
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
```

---

## Phần 4. Các quyết định thiết kế và lý do

| Quyết định | Lý do |
| --- | --- |
| Mảng cố định 112 ô, không `vector`/`new` | Ràng buộc dự án; 112 byte RAM không đáng kể |
| Kích thước mảng tính từ hai mốc địa chỉ | Không bao giờ tràn nên không cần cờ báo tràn; đổi dải quét thì tự cập nhật |
| `struct` chứa mảng + `count` | Mảng C không tự biết độ dài hợp lệ; hàm không trả mảng thô được |
| Mọi hàm nhận `TwoWire&` | Không gắn cứng vào `Wire`; về sau mutex FreeRTOS bọc từ ngoài mà không sửa module |
| Không gọi `Wire.begin()` trong module | Nơi gọi sở hữu cấu hình bus (chân, tốc độ) |
| `readI2CRegister` trả `bool` + `uint8_t& value` | Phân biệt "đọc lỗi" với "đọc được 0" |
| Pha 1 dùng `endTransmission(false)` | Giữ bus để pha 2 dùng repeated START |
| Module không biết MAX30102/OLED | Dùng lại được cho chip khác; dữ liệu riêng dự án nằm ở nơi gọi |
| Chọn `#ifndef` thay `#pragma once` | Chuẩn C++, chạy mọi compiler |

---

## Phần 5. Cách dùng dự kiến (sẽ hoàn thiện ở `main.cpp`, CP1)

Thứ tự gọi:

1. `Wire.begin(21, 22)` rồi đặt tốc độ bus (khởi đầu 100 kHz). Bên gọi làm, không phải module.
2. `scanI2CBus(Wire)` rồi in danh sách địa chỉ ra Serial.
3. Kiểm tra danh sách có 0x57 (MAX30102) và 0x3C hoặc 0x3D (OLED) hay không.
4. `readI2CRegister(Wire, 0x57, 0xFF, id)` rồi so `id` với 0x15.

Tiêu chí pass CP1 liên quan module này: thấy 0x57 và 0x3C/0x3D, `readI2CRegister` trả `true` và Part ID đọc được là 0x15.

---

## Phần 6. Giới hạn đã biết và hướng mở rộng

**Giới hạn hiện tại:**

- Chỉ quét địa chỉ 7-bit trong 0x08–0x77, không hỗ trợ địa chỉ 10-bit.
- `readI2CRegister` chỉ đọc 1 byte. Đọc nhiều byte liên tiếp (ví dụ FIFO của MAX30102) cần hàm khác.
- Chưa có hàm ghi thanh ghi.
- Không tự bảo vệ bus khi nhiều task cùng dùng (xem ý tưởng mutex dưới đây).

**Ý tưởng mở rộng (chưa làm, đến khi cần mới bàn):**

- **Phiếu mô tả thiết bị `I2CDeviceSpec`**: gói tên, địa chỉ, có Part ID hay không, địa chỉ thanh ghi ID, giá trị kỳ vọng, để một hàm xác thực dùng chung cho nhiều chip.
- **Kết quả xác thực có 5 trạng thái** thay cho true/false, mỗi trạng thái dẫn tới một hướng xử lý:

  | Trạng thái | Ý nghĩa | Hướng xử lý |
  | --- | --- | --- |
  | `kNotFound` | Không ACK | Kiểm tra dây, nguồn, địa chỉ |
  | `kFoundNoId` | Có ACK nhưng thiết bị không có ID (OLED) | Coi là có mặt, không kiểm thêm được |
  | `kIdMatch` | Có ACK, ID khớp | Đạt |
  | `kIdMismatch` | Có ACK, ID khác | Sai chip hoặc hàng nhái |
  | `kReadFailed` | Có ACK nhưng đọc thanh ghi lỗi | Nghi nhiễu bus hoặc timing |

- **Hàm ghi thanh ghi** và **đọc nhiều byte**: cần khi viết `Max30102Driver` (CP2).
- **Bọc mutex I2C** (CP4): đặt ở nơi gọi (task), không sửa bên trong module. Đây là lợi ích của việc mọi hàm nhận `TwoWire&` và module không tự quản lý bus.
- **Kích thước stack task** (CP4): `I2CScanResult` khoảng 113 byte nằm trên stack của task gọi hàm quét.
- **Test native** (CP6): module phụ thuộc Arduino nên phải nằm trong `lib_ignore` của env `native`.

---

## Phần 7. Cần tự kiểm chứng

Những điểm sau tôi (người hướng dẫn) chưa đối chiếu với nguồn gốc. Hãy tự xác nhận trước khi dựa vào:

1. **Part ID của MAX30102**: mở `datasheets/MAX30102.PDF`, xác nhận thanh ghi Part ID ở 0xFF và giá trị 0x15. Đồng thời xác nhận 0xAE/0xAF dịch ra 0x57.
2. **Mã trả về của `endTransmission()`** trên core ESP32: đối chiếu tài liệu `Wire` của Arduino và core ESP32 (các mã lỗi, đặc biệt mã timeout).
3. **Chữ ký `requestFrom`**: nếu compiler báo "ambiguous overload" trên phiên bản core đang dùng, ghi lại nguyên văn lỗi để xử lý.
4. **Địa chỉ OLED**: 0x3C hay 0x3D tuỳ module, xác nhận bằng kết quả quét thực tế.

---

## Phần 8. Lỗi thường gặp

| Triệu chứng | Nguyên nhân có thể |
| --- | --- |
| Quét ra 0 thiết bị | Chưa gọi `Wire.begin()` trước khi quét; sai chân SDA/SCL; dây lỏng; thiếu nguồn |
| `undefined reference to scanI2CBus` | Chữ ký hàm trong `.cpp` lệch với prototype trong `.h` (khác kiểu hoặc thứ tự tham số) |
| `multiple definition` lúc link | Đặt thân hàm trong file `.h` |
| Struct bị báo định nghĩa hai lần | Thiếu hoặc sai bộ `#ifndef / #define / #endif` (quên `#endif`, trùng tên macro) |
| Dùng 0xAE hoặc 0x78 mà không thấy thiết bị | Dùng địa chỉ 8-bit trong datasheet; `Wire` cần địa chỉ 7-bit |
| `count` ra giá trị lạ | Quên khởi tạo `I2CScanResult result = {};` |
| Không thấy lỗi biên dịch của module | PlatformIO chỉ biên dịch thư viện trong `lib/` khi `main.cpp` include header của nó |
