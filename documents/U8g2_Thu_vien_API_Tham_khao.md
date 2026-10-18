# Tài liệu tra cứu — U8g2 / U8x8 Monochrome Graphics Library

> **Phiên bản đã đối chiếu:** U8g2 2.36.18, theo `library.properties` trong PlatformIO.
> **Ngữ cảnh project:** `platformio.ini` khai báo `olikraus/U8g2@^2.36.18`. `src/main.cpp` hiện scan I²C và tìm địa chỉ OLED 0x3C/0x3D nhưng chưa khởi tạo U8g2. Chưa có đủ thông tin để xác định controller, kích thước, mapping panel hoặc constructor; phải xác minh module thực tế trước khi chọn.

Tài liệu này hướng dẫn chọn API, bus và buffer. Nguồn chữ ký C++ chính là `src/U8g2lib.h`, `src/U8x8lib.h`, `src/MUIU8g2.h`; triển khai lõi nằm dưới `src/clib/`. Ưu tiên source cài trong `.pio/libdeps/esp32doit-devkit-v1/U8g2/` nếu tài liệu web khác phiên bản.

## 1. Chọn U8g2 hay U8x8

- **U8g2:** vẽ đồ họa theo pixel, hình học, bitmap, font và text. Tùy constructor, dùng full buffer hoặc buffer theo page.
- **U8x8:** text theo ô ký tự/tile, không cần framebuffer pixel toàn màn hình; phù hợp UI chữ đơn giản và ít RAM. Không có bộ primitive đồ họa pixel tổng quát của U8g2.

Tên constructor thường theo mẫu:

```text
U8G2_<CONTROLLER>_<PANEL/RESOLUTION>_<BUFFER>_<BUS>
```

Ví dụ minh họa, chỉ dùng sau khi xác nhận module đúng SSD1306 128x64 noname:

```cpp
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
```

Controller phải khớp module (ví dụ SSD1306 khác SH1106); độ phân giải/panel mapping và giao thức cũng phải khớp. I²C scanner chỉ cho biết địa chỉ phản hồi, không nhận diện controller hay layout panel.

Bus phổ biến: `HW_I2C`, `SW_I2C`, `4W_HW_SPI`, `4W_SW_SPI`, các biến thể 3-wire và parallel. Pin/đối số phụ thuộc chữ ký constructor cụ thể; kiểm tra định nghĩa class thay vì suy đoán thứ tự tham số.

### Địa chỉ I²C

```cpp
display.setI2CAddress(0x78);  // địa chỉ 7-bit 0x3C được nhân 2
display.begin();
```

`setI2CAddress()` nhận địa chỉ bus byte (địa chỉ 7-bit dịch trái 1), không nhận trực tiếp địa chỉ 7-bit scanner in ra. Gọi trước `begin()`. `begin()` khởi tạo giao diện và gửi lệnh theo constructor, nhưng **giá trị trả về không xác nhận OLED đã ACK**: trong build tĩnh source trả `true` sau chuỗi khởi tạo; source có nhánh `U8G2_USE_DYNAMIC_ALLOC` trả `false`. Dùng scan/kiểm tra riêng để xác nhận thiết bị, và không nhầm việc scan thấy địa chỉ với xác định đúng controller.

## 2. Chọn buffer và render đúng

| Hậu tố | RAM MCU | Cách render |
| --- | --- | --- |
| `_F_` | Full framebuffer; 128x64 monochrome cơ bản khoảng 1024 byte. | `clearBuffer()` → vẽ toàn frame → `sendBuffer()`. |
| `_1_` | Một page/band, thường 8 pixel cao. | `firstPage()` → vẽ toàn nội dung trong `do/while(nextPage())`. |
| `_2_` | Hai page/band. | Cùng mô hình page loop, RAM/hiệu năng ở giữa. |

Full buffer:

```cpp
display.clearBuffer();
display.setFont(u8g2_font_ncenB14_tr);
display.drawStr(0, 16, "Hello");
display.sendBuffer();
```

Page buffer:

```cpp
display.firstPage();
do {
  display.setFont(u8g2_font_ncenB14_tr);
  display.drawStr(0, 16, "Hello");
} while (display.nextPage());
```

Trong page loop, mọi nội dung phải vẽ lại trên mỗi lượt. Không đọc sensor, cập nhật state/animation hoặc gọi thao tác có side-effect trong block này; chụp snapshot trước khi render để tất cả page hiển thị cùng dữ liệu. `nextPage()` gửi page hiện tại, chuyển sang page tiếp theo và trả 0 khi hoàn tất.

### Buffer API

- `clearBuffer()` xóa framebuffer MCU; không tự cập nhật display.
- `sendBuffer()` gửi framebuffer và gọi refresh message.
- `firstPage()`/`nextPage()` quản lý page drawing.
- `updateDisplay()` gửi buffer nhưng không gửi ePaper refresh message.
- `refreshDisplay()` gửi refresh message, có ý nghĩa với một số controller ePaper; không thay thế tùy tiện cho send/update.
- `updateDisplayArea(tx,ty,tw,th)` chỉ dành cho full-buffer; đối số là tile 8x8, không phải pixel. Source không kiểm tra biên, bỏ qua rotation và không hỗ trợ page mode/ePaper refresh.
- `getBufferPtr()`/tile dimensions hỗ trợ xử lý thấp tầng; layout phụ thuộc cấu hình buffer/controller.

`clearDisplay()` không phải clear-buffer frame thường dùng: source chạy picture loop để gửi các page trắng lên panel rồi đặt lại vị trí page. Với full-buffer, pattern cập nhật thông thường là `clearBuffer()`/vẽ/`sendBuffer()`.

## 3. Khởi tạo và trạng thái màn hình

- `begin()`: gọi `initDisplay()`, `clearDisplay()`, `setPowerSave(0)`, sau đó trả kết quả theo cấu hình build; không xác nhận ACK thiết bị.
- `beginSimple()`: chỉ gọi `initDisplay()`; caller tự xóa và đánh thức màn hình nếu cần.
- `initDisplay()`: gửi sequence khởi tạo display/controller.
- `initInterface()`: khởi tạo interface bus; hữu ích trong một số quy trình deep sleep khi panel vẫn chạy. Không gọi thêm `initDisplay()` nếu quy trình chỉ yêu cầu init interface.
- `setPowerSave(1/0)`: bật chế độ tiết kiệm/tắt hiển thị hoặc đánh thức.
- `setContrast(value)`, `setFlipMode(mode)`: hiệu quả phụ thuộc controller/module.
- `getDisplayWidth()`/`getDisplayHeight()`: kích thước logic pixel; `getCols()`/`getRows()` là lưới ký tự U8x8.
- `setBusClock(hz)` cấu hình trường clock nơi transport hỗ trợ; hành vi phụ thuộc bus/board.

Rotation thông dụng: `U8G2_R0`, `R1`, `R2`, `R3`. Chọn trong constructor; hệ tọa độ logic và width/height thay đổi theo rotation.

## 4. Tọa độ, hình học và màu vẽ

Tọa độ đồ họa U8g2 là pixel, gốc ở góc trái-trên của hệ tọa độ logic đã xoay. Các hàm shape phổ biến:

- `drawPixel(x,y)`, `drawLine(x0,y0,x1,y1)`, `drawHLine()`, `drawVLine()`.
- `drawFrame(x,y,w,h)`, `drawBox(x,y,w,h)`, `drawRFrame()`/`drawRBox()`.
- `drawCircle()`/`drawDisc()`, `drawEllipse()`/`drawFilledEllipse()`, `drawArc()`.
- `drawTriangle()` vẽ tam giác đặc; tọa độ ba đỉnh có dấu.

`setDrawColor(1)` bật pixel, `0` xóa, `2` XOR/invert ở primitive hỗ trợ. Trả lại màu bình thường sau khi dùng 0/2 để tránh ảnh hưởng các bước vẽ tiếp.

`drawArc()` nhận góc 0–255 cho một vòng theo implementation; kiểm tra orientation/mong muốn trực quan bằng ví dụ hoặc source khi dùng cung. `drawCircle()`/`drawDisc()` nhận tâm, bán kính và tùy chọn cung/phần tư.

## 5. Bitmap, font và text

### Bitmap

- `drawXBM(x,y,w,h,bitmap)` vẽ bitmap định dạng XBM.
- `drawXBMP(...)` thường dùng cho bitmap lưu `PROGMEM` trên AVR.
- `drawBitmap(x,y,cnt,h,bitmap)` là API cũ; `cnt` tính theo byte ngang. Ưu tiên XBM/XBMP.
- `setBitmapMode(1)` cho bitmap trong suốt; mode khác vẽ opaque theo hành vi thư viện.

### Font và baseline

`setFont(font)` chọn font sinh sẵn, ví dụ `u8g2_font_ncenB14_tr`. Font quyết định glyph có sẵn, cỡ chữ và footprint ROM; không phải font nào cũng có tiếng Việt.

Mặc định tọa độ Y của hàm text là **baseline**, không phải mép trên. Ví dụ `drawStr(0,16,"Hello")` đặt baseline tại y=16; đặt y=0 thường khiến chữ bị cắt trên đầu. Có thể đổi quy tắc vị trí bằng `setFontPosBaseline()`, `setFontPosTop()`, `setFontPosBottom()`, `setFontPosCenter()`.

API thường dùng:

- `drawStr(x,y,text)` cho text byte/Latin cơ bản.
- `drawUTF8(x,y,text)` cho chuỗi UTF-8; source phải UTF-8 và font cần có glyph.
- `getStrWidth()`/`getUTF8Width()` để căn chỉnh; dùng width UTF-8 cho chuỗi UTF-8.
- `getAscent()`/`getDescent()` để bố trí baseline.
- `enableUTF8Print()` bật decoder UTF-8 cho `print()`; mặc định `Print` không bật UTF-8. `disableUTF8Print()` quay về ASCII/byte.

UTF-8 chỉ là encoding, không bổ sung glyph. Nếu ký tự tiếng Việt bị thiếu, chọn font có coverage hoặc tạo font phù hợp. `strlen()` đếm byte chứ không phải số code point/glyph UTF-8.

## 6. U8x8 text-only

U8x8 viết ký tự theo cột/hàng tile, không phải tọa độ pixel và không cần framebuffer pixel toàn màn hình:

```cpp
#include <U8x8lib.h>
U8X8_SSD1306_128X64_NONAME_HW_I2C display;

void setup() {
  display.begin();
  display.setFont(u8x8_font_chroma48medium8_r);
  display.drawString(0, 0, "Sensor ready");
}
```

API chính: `begin()`, `setFont()`, `drawString()`, `drawUTF8()`, `drawGlyph()`, `draw1x2String()`/`draw2x2String()`, `getCols()`/`getRows()`, `getUTF8Len()`, `setPowerSave()`, `setContrast()`, `clear()` và `home()`. `drawString()`/`drawUTF8()` nhận tọa độ cell. U8x8 cũng có `setI2CAddress()` với quy tắc địa chỉ nhân 2 như U8g2.

## 7. API UI/log và compile options

- `U8G2LOG`/`U8X8LOG`: vùng log ký tự dùng buffer do ứng dụng cấp; bảo đảm vùng nhớ đủ kích thước và tránh ghi log dày trong critical sampling loop.
- `MUIU8G2`: lớp form/focus; ứng dụng vẫn phải cung cấp descriptor và xử lý input/event.
- `userInterfaceSelectionList()`, `userInterfaceMessage()`, `userInterfaceInputValue()`: UI tiện ích tương tác; cần mô hình event/pin phù hợp.
- Một số API chỉ có khi compile guard được bật, ví dụ menu pins `U8X8_USE_PINS`, clip window `U8G2_WITH_CLIP_WINDOW_SUPPORT`, dynamic buffer `U8G2_USE_DYNAMIC_ALLOC`, user pointer `U8X8_WITH_USER_PTR`. Xác minh macro/cấu hình build và chữ ký của class cụ thể trước khi sử dụng.

## 8. Quy trình ghép với MAX30102 trong project

1. Xác định model module, controller, resolution, mapping panel, giao tiếp và địa chỉ; I²C scan một mình không đủ để chọn constructor.
2. Chọn U8G2 full-buffer nếu SRAM/hiệu năng phù hợp và UI vẽ linh hoạt; chọn page-buffer khi cần tiết kiệm RAM; chọn U8x8 nếu chỉ cần text.
3. Khởi tạo display một lần; không coi `begin()` trả `true` là phát hiện phần cứng.
4. Thu thập sensor và drain FIFO riêng; tạo snapshot trạng thái (BPM, SpO₂, validity, lỗi sensor).
5. Render từ snapshot; không gọi sensor hoặc thay đổi state trong page loop.
6. Dùng full-buffer `clearBuffer()`/draw/`sendBuffer()` hoặc page loop đúng mẫu; tránh gửi nguyên màn hình nhanh hơn nhu cầu nếu tối ưu thời gian bus là quan trọng.

Ở thời điểm tài liệu này được lập, `src/main.cpp` mới scan I²C và nhận diện 0x3C/0x3D; chưa gọi API U8g2 và chưa xác định panel OLED cụ thể.

## 9. Source, ví dụ và tài liệu

- `src/U8g2lib.h`, `src/U8g2lib.cpp`: wrapper U8G2, constructor, API vẽ/buffer/Print.
- `src/U8x8lib.h`, `src/U8x8lib.cpp`: API ký tự U8x8 và transport.
- `src/MUIU8g2.h`: wrapper MUI.
- `src/clib/u8g2_buffer.c`: page/full-buffer/send/update; `src/clib/u8g2_cleardisplay.c`: clear display.
- `examples/full_buffer/HelloWorld`, `examples/page_buffer/HelloWorld`, `examples/full_buffer/FontUsage`, `examples/u8x8/HelloWorld`, `examples/full_buffer/UpdateArea`: ví dụ khởi đầu nên tra.
- Wiki/API chính thức: <https://github.com/olikraus/u8g2/wiki>
- U8g2 reference: <https://github.com/olikraus/u8g2/wiki/u8g2reference>
- Setup/buffer tutorial: <https://github.com/olikraus/u8g2/wiki/setup_tutorial>
