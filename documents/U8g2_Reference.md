# Tài liệu tra cứu: U8g2 cho OLED SSD1306 0.96" I2C

> Phạm vi: đọc từ `U8g2lib.h/.cpp`, `U8x8lib.cpp`, `clib/` (u8g2_buffer, u8x8_cad, driver SSD1306 128x64, u8g2_d_setup) và examples.
>
> Quy ước: `[source]` = thấy trong code; `[datasheet]` = kiến thức chip SSD1306, cần đối chiếu; `[suy luận]` = mình suy ra/ước lượng chưa đo.
>
> Cách đọc: **Lý thuyết → API → cạm bẫy → áp dụng vào project**. Cuối file có mục **Cần tự kiểm chứng**.

---

## 0. Bức tranh tổng thể

OLED 0.96" 128×64 là một **màn hình có RAM hiển thị riêng (GDDRAM) nằm trong chip SSD1306**. MCU không "vẽ lên màn hình" trực tiếp; MCU **gửi byte vào RAM của chip**, chip tự quét ra pixel. U8g2 là thư viện lo 3 việc:

1. **Bộ đệm vẽ (framebuffer) trong RAM của ESP32**: bạn vẽ chữ/hình vào đây (nhanh, không I2C).
2. **Gửi bộ đệm sang chip** qua I2C (chậm, chiếm bus).
3. **Chuyển đổi định dạng** (pixel → "page" dọc của SSD1306), phông chữ, xoay màn hình.

Kiến trúc 3 tầng:

```text
C++ class  U8G2 (U8g2lib.h)           ← API bạn gọi: drawStr, sendBuffer...
   └─ C core  u8g2_*   (clib/u8g2_*.c)  ← bộ đệm, vẽ hình, phông
        └─ C core  u8x8_* (clib/u8x8_*.c) ← điều khiển chip: lệnh, "CAD" (cách gửi), byte callback
             └─ callback Arduino: u8x8_byte_arduino_hw_i2c  → Wire
```

Hiểu tầng này giúp trả lời: *"vì sao một số lỗi không báo?"* (callback không trả lỗi I2C), *"vì sao có `static`"* (tầng CAD không reentrant), *"vì sao clock bị đặt lại"* (callback `START_TRANSFER` gọi `Wire.setClock`).

---

## 1. Lý thuyết nền: SSD1306 và I2C

### 1.1 Chip SSD1306 [datasheet]

- GDDRAM 128 cột × 64 hàng = **1024 byte**, tổ chức thành **8 page**, mỗi page cao 8 pixel; **1 byte = 8 pixel dọc** trong 1 cột (bit thấp ở trên). → "tile" 8×8 của U8g2 chính là 1 byte × 8 cột.
- Nhận hai loại byte, phân biệt bằng **control byte** ở đầu mỗi gói I2C: `0x00` = các byte sau là **lệnh**; `0x40` = các byte sau là **dữ liệu hiển thị** `[source: u8x8_cad.c]`.
- Địa chỉ I2C: 0x3C hoặc 0x3D (chân SA0). U8g2 mặc định lưu **0x78 (dạng 8-bit)** = 0x3C << 1 `[source]`.

### 1.2 Vì sao U8g2 dùng "địa chỉ 8-bit"

Thư viện C gốc dùng địa chỉ có sẵn bit R/W ở bit 0 (0x78 = 0b0111 1000). `Wire` của Arduino dùng địa chỉ **7-bit** (0x3C). Callback chuyển bằng `address >> 1` khi gọi `Wire.beginTransmission` `[source]`. Hệ quả:

```cpp
u8g2.setI2CAddress(0x3C << 1);   // ĐÚNG  (0x78)
u8g2.setI2CAddress(0x3C);        // SAI   (thành 0x1E trên dây)
```

Đây chính là lý do trong main.cpp có `oledAddress << 1`.

### 1.3 Thời gian chiếm bus (ước lượng)

Một lần `sendBuffer()` đẩy 1024 byte dữ liệu, cắt thành **khối 24 byte** (kèm control byte + địa chỉ ≈ 26 byte/khối trên dây) `[source: u8x8_cad.c,`while (arg_int > 24)`]`. Mỗi byte tốn ≈ 9 chu kỳ clock (8 bit + ACK).

`[suy luận — chưa đo]` ước lượng:

- 100 kHz: ≈ 1024/24 ≈ 43 khối × 26 byte × 9 / 100 000 ≈ **~100 ms**
- 400 kHz: ≈ **~26 ms**

Trong khoảng đó **không ai khác dùng được bus** (MAX30102 cần được đọc mỗi ~40 ms ở 25 Hz, bộ đệm phần mềm chỉ 4 mẫu — xem file SparkFun). Đây là nút thắt thiết kế quan trọng nhất của project. Hãy **đo thật** bằng `micros()` quanh `sendBuffer()` (mục 10).

---

## 2. Lý thuyết nền: chế độ bộ đệm (buffer mode)

Hậu tố cuối tên constructor chọn chế độ bộ nhớ:

| Hậu tố | Chế độ | RAM (128×64) | Cách vẽ | Đặc điểm |
| --- | --- | --- | --- | --- |
| `_F_` | **Full buffer** | 1024 B | `clearBuffer()` → vẽ → `sendBuffer()` | Đơn giản, vẽ theo thứ tự tùy ý, **gửi cả 1024 byte** mỗi lần |
| `_1_` | **Page buffer 1 page** | 128 B | Vòng `firstPage()`/`do{ vẽ }while(nextPage())` | Tiết kiệm RAM, **phải vẽ lại toàn cảnh 8 lần/frame** |
| `_2_` | Page buffer 2 page | 256 B | như trên | Ít vòng hơn (4 lần) |

ESP32 có ~320 KB RAM → **dùng `_F_`** (project dùng `U8G2_SSD1306_128X64_NONAME_F_HW_I2C`). Chế độ page chỉ hợp khi RAM cực hạn (AVR).

Bộ đệm full nằm trong RAM tĩnh của đối tượng `u8g2` → **không cần `new`**, phù hợp ràng buộc project. Đối tượng nên là biến toàn cục/tĩnh (không phải biến cục bộ stack trong task nhỏ).

**Tile**: đơn vị 8×8 pixel. Màn 128×64 = 16 × 8 tile. Hàm theo tọa độ tile (`updateDisplayArea`, `drawTile`) dùng đơn vị tile, hàm vẽ thông thường dùng đơn vị pixel — dễ nhầm.

---

## 3. Constructor

### 3.1 Quy tắc đặt tên `[source: U8g2lib.h]`

```cpp
U8G2_<controller>_<độ phân giải>_<biến thể>_<buffer>_<bus>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C
```

- `SSD1306` chip điều khiển; `128X64` kích thước.
- `NONAME`: biến thể module "không tên" (đa số module 0.96" giá rẻ) — khác nhau ở chuỗi lệnh khởi tạo/mặc định. Biến thể khác của 128X64: `VCOMH0`, `ALT0` (dùng khi NONAME cho hình lỗi).
- `F` full buffer. `HW_I2C` dùng phần cứng I2C qua `Wire`; `SW_I2C` bit-bang bằng 2 chân GPIO; `2ND_HW_I2C` dùng `Wire1`.

### 3.2 Chữ ký (HW I2C) `[source]`

```cpp
U8G2_SSD1306_128X64_NONAME_F_HW_I2C(
    const u8g2_cb_t* rotation,
    uint8_t reset = U8X8_PIN_NONE,
    uint8_t clock = U8X8_PIN_NONE,
    uint8_t data  = U8X8_PIN_NONE);
```

| Tham số | Ý nghĩa |
| --- | --- |
| `rotation` | `U8G2_R0` (0°), `U8G2_R1` (90°), `U8G2_R2` (180°), `U8G2_R3` (270°), `U8G2_MIRROR`, `U8G2_MIRROR_VERTICAL` |
| `reset` | Chân RESET của OLED. Module 4 chân 0.96" thường **không có** chân reset → `U8X8_PIN_NONE` (=255) |
| `clock`, `data` | Pin SCL, SDA — **chỉ dùng nếu cả hai khác `U8X8_PIN_NONE`** `[source]`; khi đó callback gọi `Wire.begin(data, clock)`; ngược lại gọi `Wire.begin()` (pin mặc định) |

Với ESP32 DOIT: pin mặc định SDA=21, SCL=22 → không cần truyền. Nếu muốn tường minh/chống nhầm: `(U8G2_R0, U8X8_PIN_NONE, 22, 21)` (chú ý thứ tự **clock rồi data**).

Biến thể SH1106 (nếu hình bị dịch 2 pixel hoặc rác ở mép trái/phải): `U8G2_SH1106_128X64_NONAME_F_HW_I2C` `[source: có trong U8g2lib.h]`. Project đã xác nhận SSD1306.

---

## 4. API vòng đời & cấu hình màn hình

| Hàm | Mục đích / Khi dùng | Tham số | Ghi chú & cạm bẫy |
| --- | --- | --- | --- |
| `bool begin()` | Khởi tạo: `initDisplay()` + `clearDisplay()` + `setPowerSave(0)` | – | **Luôn trả `true`** (trừ khi bật `U8G2_USE_DYNAMIC_ALLOC`) → **không thể dùng để biết OLED có nối hay không**; lỗi I2C không bao giờ báo `[source]`. Bên trong gọi `Wire.begin()` (xem mục 8). Dùng `scanI2C` để xác thực trước. |
| `void beginSimple()` | Như `begin()` nhưng không xóa màn, không bật hiển thị | – | Bạn tự `clearDisplay()`, `setPowerSave(0)` |
| `void setI2CAddress(uint8_t adr)` | Đổi địa chỉ I2C | `adr` **8-bit** (7-bit << 1) | Gọi **trước** `begin()` |
| `void setBusClock(uint32_t hz)` / `getBusClock()` | Tốc độ I2C dành riêng cho các lần truyền của U8g2 | Hz | Nếu 0 → lấy mặc định của chip (SSD1306: 400 kHz `[source: i2c_bus_clock_100kHz = 4]`). U8g2 gọi `Wire.setClock(bus_clock)` **trước mỗi lần truyền** → nó **ghi đè** clock mà MAX30102 đặt (xem mục 8). |
| `void setPowerSave(uint8_t on)` | 1 = tắt hiển thị (ngủ), 0 = bật | | RAM giữ nguyên. Hữu ích cho state machine/IDLE. |
| `void setContrast(uint8_t v)` | Độ sáng (0–255) | | Giảm độ sáng → giảm dòng/nóng/lão hóa OLED; ảnh hưởng nhẹ đến brownout. |
| `void setFlipMode(uint8_t m)` | 1 = lật 180° bằng phần cứng chip | | Không đổi buffer; khác `rotation` của constructor. |
| `void clearDisplay()` | Xóa **RAM của chip** | | Thực chất chạy một vòng `firstPage/nextPage` rỗng để gửi các page trắng lên chip, rồi đặt lại tile row về 0 `[source: u8g2_cleardisplay.c]`. Khác `clearBuffer()` (xóa buffer ở ESP32). Tốn một lần truyền đầy đủ. |
| `void initDisplay()` / `initInterface()` | Phần con của `begin()` | | Hiếm khi gọi tay. `initInterface()` dành cho trường hợp MCU thức dậy từ deep sleep mà OLED vẫn đang chạy: **gọi `initDisplay()` HOẶC `initInterface()`, không gọi cả hai** (initInterface đã nằm trong initDisplay) `[source: comment trong U8g2lib.h]` |
| `u8g2_uint_t getDisplayWidth()/getDisplayHeight()` | Kích thước pixel (128×64) | | Dùng để căn giữa, không viết số cứng |
| `getCols()/getRows()` | Kích thước theo tile (16×8) | | |

---

## 5. Bộ đệm & gửi lên màn hình

### 5.1 Chế độ full buffer (dùng trong project)

```cpp
u8g2.clearBuffer();      // xóa buffer trong ESP32 (nhanh, không I2C)
/* ... vẽ ... */
u8g2.sendBuffer();       // đẩy toàn bộ buffer sang chip (chiếm bus ~26–100 ms, xem 1.3)
```

| Hàm | Mục đích | Cạm bẫy |
| --- | --- | --- |
| `clearBuffer()` | Xóa buffer RAM | Quên gọi → chữ cũ chồng lên chữ mới |
| `sendBuffer()` | Gửi cả buffer + `RefreshDisplay` | Không có tham số, không trả lỗi. Chặn tới khi truyền xong. Gọi **mỗi khi giá trị đổi**, không gọi liên tục trong vòng `loop` |
| `updateDisplay()` / `refreshDisplay()` | `updateDisplay()` gửi cả buffer **không** kèm thông điệp refresh (khác `sendBuffer()`); `refreshDisplay()` chỉ gửi thông điệp refresh | Refresh chỉ có ý nghĩa với màn ePaper; với OLED cứ dùng `sendBuffer()` cho nhất quán |
| `updateDisplayArea(tx, ty, tw, th)` | **Chỉ gửi một vùng** (tính theo **tile**) | Ví dụ cập nhật riêng ô BPM: ít byte hơn → **giữ bus ngắn hơn rất nhiều**. Không phụ thuộc `U8G2_WITH_CLIP_WINDOW_SUPPORT` (macro đó chỉ bảo vệ `setClipWindow`/`setMaxClipWindow`) `[source: U8g2lib.h]`. Chỉ dùng được ở chế độ full buffer (ở page mode: **âm thầm không làm gì**). ⚠ **Không kiểm tra biên** (tx+tw > 16 hoặc ty+th > 8 → đọc vượt buffer) và **không tính rotation** `[source: u8g2_buffer.c]` → chỉ nên dùng với `U8G2_R0` và tọa độ đã tính chắc. Công cụ then chốt để giảm xung đột với MAX30102. |
| `getBufferPtr`, `getBufferTileHeight/Width`; `setBufferPtr`, `getBufferSize` | Truy cập buffer thô | Dùng để test/ghi buffer ra ảnh; không cần thường ngày. ⚠ `setBufferPtr` và `getBufferSize` **chỉ tồn tại khi build có `U8G2_USE_DYNAMIC_ALLOC`** `[source: U8g2lib.h]` — build mặc định của project không có hai hàm này |
| `setBufferCurrTileRow` | (page mode) | Không dùng |
| `setAutoPageClear(mode)` | Có tự xóa buffer ở mỗi trang hay không | Chỉ ý nghĩa ở page mode |

### 5.2 Page mode (để hiểu, không dùng)

```cpp
u8g2.firstPage();
do { /* vẽ toàn bộ khung */ } while (u8g2.nextPage());
```

Mỗi lần lặp chỉ vẽ vào một lát; **toàn bộ code vẽ chạy nhiều lần** (lát × lần). Nếu `drawStr` có tác dụng phụ (đếm, `millis()`) sẽ bị lặp. Vì vậy: **không đọc cảm biến hay đổi state trong vòng này**; chụp snapshot (BPM, SpO2, valid) trước vòng rồi vẽ từ snapshot.

### 5.3 Quy tắc vàng khi vẽ

- **Vẽ vào buffer = rẻ, không đụng I2C** → có thể làm ngoài mutex (nếu buffer chỉ một task đụng).
- **`sendBuffer()` = đụng I2C** → **phải** trong mutex I2C.
- Chỉ **một task** nên sở hữu đối tượng `u8g2` (buffer không thread-safe).

---

## 6. Vẽ chữ

### 6.1 Lý thuyết: baseline, phông, kiểu vẽ

- Phông trong U8g2 là mảng `const uint8_t[]` nằm trong **flash** (không tốn RAM khi chưa chọn) `[source]`. Đổi phông = đổi con trỏ.
- Tọa độ `(x, y)` của `drawStr` mặc định là **góc dưới-trái theo đường cơ sở (baseline)** của chuỗi, *không phải góc trên-trái*. Chữ có "đuôi" (g, y) vượt xuống dưới baseline. `[source: setFontPosBaseline mặc định]`. Hậu quả: `drawStr(0, 0, "A")` gần như **không thấy** (chữ nằm trên mép).
- Đổi gốc: `setFontPosTop()` → `y` là đỉnh chữ; `setFontPosCenter()`; `setFontPosBottom()`.

### 6.2 Quy ước tên phông `u8g2_font_<tên><cỡ>_<hậu tố>`

Hậu tố 2 ký tự: ký tự 1 = kiểu khoảng cách chữ, ký tự 2 = tập ký tự `[source — đối chiếu tên trong u8g2.h]`:

| Ký tự 1 | Ý nghĩa | Ký tự 2 | Tập ký tự |
| --- | --- | --- | --- |
| `t` | Transparent (nền trong suốt khi vẽ) | `r` | Reduced: ASCII 32–127 |
| `m` | Monospace (cùng độ rộng) | `f` | Full: 256 ký tự (Latin-1) |
| `h` | "h" = chiều cao đồng nhất (ký tự cùng cao) | `n` | Number: chỉ `0-9` và vài ký hiệu → **rất nhỏ, tốt cho số lớn** |
| `8` | Mỗi ký tự khung 8×8 | `u` | Uppercase |
| | | `e` | Extended — có thêm ký tự ngoài ASCII. Hậu tố `_te` **có thật** (166 font trong `u8g2.h`) nhưng tập ký tự chính xác theo wiki U8g2, **chưa tự kiểm** `[chưa kiểm]` |

→ `u8g2_font_ncenB14_tr` = New Century Bold 14px, trong suốt, ASCII; `u8g2_font_helvB10_tr`; `u8g2_font_6x10_tr`; `u8g2_font_logisoso*_tn` (số rất to, chỉ số).
Chọn `_tn` cho số BPM lớn (nhỏ flash, dễ đọc). **Tiếng Việt có dấu**: `_tr`/`_tf` không có glyph dấu tiếng Việt (nằm ngoài Latin-1). Source có sẵn `u8g2_font_unifont_t_vietnamese1` và `u8g2_font_unifont_t_vietnamese2` `[source: tên font trong u8g2.h; phạm vi glyph từng font chưa kiểm]` → dùng với `drawUTF8`, file `.cpp` phải lưu UTF-8; unifont cao ~16 px nên tốn chỗ trên màn 64 px, và mỗi font chỉ phủ một phần số ký tự có dấu (thử cả hai). Với project, nhãn tiếng Anh ("BPM", "SpO2") vẫn là lựa chọn đơn giản nhất.

### 6.3 API chữ

| Hàm | Mục đích | Tham số | Trả về / lưu ý |
| --- | --- | --- | --- |
| `setFont(const uint8_t* font)` | Chọn phông hiện hành | tên phông | Gọi trước `drawStr`; không đổi các thứ đã vẽ |
| `drawStr(x, y, const char* s)` | Vẽ chuỗi ASCII | pixel, baseline | Trả **chiều rộng** đã vẽ (pixel) → dùng để vẽ nối tiếp |
| `drawUTF8(x, y, s)` | Vẽ chuỗi UTF-8 | | Phông phải chứa glyph tương ứng |
| `drawGlyph(x, y, encoding)` | 1 ký tự theo mã (VD `0x2665`) | | Dùng với phông ký hiệu |
| `drawStrX2/drawUTF8X2/drawGlyphX2` | Vẽ phóng gấp đôi | | Nhanh để có chữ lớn, nhưng răng cưa |
| `getStrWidth(s)` / `getUTF8Width(s)` | Chiều rộng chuỗi (pixel) | | **Căn giữa**: `x = (128 - getStrWidth(s)) / 2` |
| `getAscent()` / `getDescent()` | Độ cao trên/dưới baseline | | Căn dọc, tính vị trí dòng |
| `getMaxCharHeight/Width()` | Kích thước ký tự lớn nhất | | Chia dòng |
| `setFontMode(is_transparent)` | 1 = nền trong suốt; 0 = ghi cả nền | | Chế độ 0 tự xóa vùng dưới chữ (hữu ích khi vẽ đè giá trị mới) |
| `setFontDirection(d)` | Hướng chữ 0,1,2,3 | | |
| `setFontRefHeightText/ExtendedText/All` | Cách tính chiều cao tham chiếu | | Ảnh hưởng `getAscent` và vị trí `Top/Center` |
| *(không có wrapper C++)* `u8g2_IsGlyph(u8g2.getU8g2(), enc)`, `u8g2_GetGlyphWidth(u8g2.getU8g2(), enc)` | Kiểm tra glyph tồn tại / độ rộng | | Chỉ có dạng hàm C trong `clib/u8g2.h`; class `U8G2` **không** có `isGlyph()`/`getGlyphWidth()` `[source]` |

### 6.4 In giống `Serial.print`

Class kế thừa `Print` của Arduino `[source]`: có thể `u8g2.setCursor(x, y); u8g2.print(75); u8g2.print(" BPM");`, kể cả `printf`-kiểu qua `snprintf` rồi `drawStr`.

- `setCursor(x, y)`, `getCursorX/Y()`, `home()`, `clear()`.
- Với số nguyên/float, **ưu tiên `snprintf` vào mảng `char buf[16]` + `drawStr`** để kiểm soát độ rộng và **tránh `String`** (cấp phát heap).
- `print` dùng `drawGlyph` từng ký tự theo con trỏ → nhớ `setFont` trước.
- Mặc định `print()` coi chuỗi là byte/ASCII; muốn in UTF-8 gọi `enableUTF8Print()` (tắt: `disableUTF8Print()`). `strlen()` đếm byte, không phải glyph; dùng `getUTF8Width()` cho chuỗi UTF-8 `[source: U8g2lib.h]`.

---

## 7. Vẽ hình

Hệ tọa độ: gốc (0,0) ở **góc trên-trái**, x sang phải, y xuống dưới. Màn 128×64 → x 0..127, y 0..63. Vẽ ngoài biên bị cắt (clip) an toàn.

| Hàm | Mục đích | Tham số | Ghi chú |
| --- | --- | --- | --- |
| `setDrawColor(c)` | 0 = xóa pixel, 1 = bật, 2 = XOR | | Vẽ chữ trắng trên hộp đen: `setDrawColor(1); drawBox(...); setDrawColor(0); drawStr(...)` (đảo màu) |
| `drawPixel(x,y)` | 1 điểm | | |
| `drawLine(x1,y1,x2,y2)` | Đường thẳng | | **Vẽ sóng PPG** bằng nối các điểm |
| `drawHLine(x,y,w)` / `drawVLine(x,y,h)` | Đường ngang/dọc | | Nhanh hơn `drawLine` |
| `drawFrame(x,y,w,h)` | Khung chữ nhật rỗng | | |
| `drawBox(x,y,w,h)` | Chữ nhật đặc | | Dùng để **xóa vùng cục bộ** (`setDrawColor(0)`) |
| `drawRFrame/drawRBox(x,y,w,h,r)` | Bo góc | r = bán kính | |
| `drawCircle/drawDisc(x0,y0,r,opt)` | Đường tròn/đĩa | `opt` = `U8G2_DRAW_ALL` hoặc từng góc (`UPPER_RIGHT…`) | |
| `drawEllipse/drawFilledEllipse` | Elip | | |
| `drawTriangle(x0,y0,x1,y1,x2,y2)` | Tam giác đặc | | Biểu tượng "mũi tên", cảnh báo |
| `drawArc(x,y,r,start,end)` | Cung | `start`, `end` kiểu `uint8_t`, thang 0–255 ứng với một vòng (64 = 1/4 vòng) | `start == end` → vẽ cả vòng tròn; `start > end` → đảo vùng được vẽ `[source: u8g2_arc.c]`. Chiều/gốc góc nên thử bằng mắt |
| `drawXBM(x,y,w,h,bitmap)` | Bitmap XBM (1 bit/pixel) | mảng trong RAM | `drawXBMP` cho mảng **PROGMEM** (flash) → dùng cho biểu tượng trái tim |
| `drawBitmap(x,y,cnt,h,bitmap)` | Bitmap theo "byte mỗi hàng" | `cnt` = số byte mỗi hàng | |
| `setBitmapMode(transparent)` | Nền bitmap trong suốt | | |
| `setClipWindow(x0,y0,x1,y1)` / `setMaxClipWindow()` | Giới hạn vùng vẽ | | Hữu ích khi vẽ đồ thị trong khung |
| `drawTile(x,y,cnt,ptr)` | Ghi trực tiếp tile lên màn (tọa độ tile) | | Bỏ qua buffer; hiếm dùng |

Mẹo cho đồ thị sóng PPG: lưu ~120 điểm đã chuẩn hóa vào mảng `uint8_t y[128]`, mỗi frame vẽ `drawLine` giữa các điểm kề nhau, rồi chỉ **cập nhật vùng đồ thị** bằng `updateDisplayArea`.

---

## 8. Tương tác U8g2 ↔ Wire ↔ SparkFun (phần quan trọng nhất của project)

Các điểm dưới đây đọc trực tiếp từ `u8x8_byte_arduino_hw_i2c` `[source]`:

1. **`INIT` (trong `begin()`)**: nếu `bus_clock == 0` thì gán mặc định của chip (400 kHz); rồi gọi `Wire.begin()` (có pin chỉ khi bạn truyền cả `clock` và `data`). Gọi `Wire.begin()` lần 2 (SparkFun `begin()` đã gọi một lần) **không gây lỗi trên ESP32** thông thường, nhưng nên gọi `Wire.begin(21, 22)` một lần ở `setup()` rồi **thứ tự khởi tạo cố định**.
2. **`START_TRANSFER` (mỗi lần truyền)**: `Wire.setClock(bus_clock)` rồi `beginTransmission(addr >> 1)`. ⇒ **U8g2 ghi đè clock của bus ở mọi lần truyền**. MAX30102 (SparkFun) chỉ đặt clock **một lần** trong `begin()`. Kết quả: sau lần `sendBuffer()` đầu tiên, bus chạy ở clock của U8g2 cho **cả** MAX30102.
   - Muốn 100 kHz cho cả hai: `u8g2.setBusClock(100000)`.
   - Muốn 400 kHz cho cả hai (OLED nhanh hơn 4×, bus rảnh hơn): `setBusClock(400000)` và `particleSensor.begin(Wire, I2C_SPEED_FAST)`; MAX30102 hỗ trợ 400 kHz `[datasheet — cần xác nhận]`, nhưng dây dài/pull-up yếu có thể lỗi → **kiểm bằng scanI2C + đọc Part ID ổn định**.
3. **`SEND`**: `Wire.write` từng khối; **không kiểm lỗi** của `endTransmission()` → NACK/bus treo **không được phát hiện**.
4. **`static in_transfer`** ở tầng CAD `[source: u8x8_cad.c]`: cờ trạng thái chung → hai luồng gọi U8g2 đồng thời có thể làm hỏng khung. ⇒ mọi lời gọi tới bus của U8g2 (`begin`, `sendBuffer`, `updateDisplayArea`, `setPowerSave`, `setContrast`, `clearDisplay`) **phải nằm trong mutex**.
5. Khối truyền dữ liệu 24 byte + control byte → mỗi `endTransmission` ngắn, nên **mutex không thể giữ ở mức từng khối** (cũng không cần); giữ trọn `sendBuffer()`.

### Kết luận thiết kế (cho CP4)

- Một `i2cMutex` toàn cục (`rtos_handles.h`).
- **Task đọc cảm biến**: `take(mutex)` → `check()` → `give` mỗi ~40 ms.
- **Task hiển thị**: vẽ buffer ngoài mutex → `take` → `sendBuffer()` (hoặc `updateDisplayArea` cho vùng đổi) → `give`; tần suất ~4–10 Hz, **không** 25 Hz.
- Nếu `sendBuffer` giữ bus > 3 chu kỳ mẫu (>~120 ms ở 100 kHz) sẽ mất mẫu. Giải pháp (theo thứ tự rẻ → đắt): `setBusClock(400000)`; `updateDisplayArea` thay vì cả khung; giảm tần suất vẽ; ưu tiên task cảm biến cao hơn task hiển thị.
- Nếu `take` mutex có thời gian chờ → xử lý timeout, **không** chặn vô hạn (watchdog).

---

## 9. Các API khác (nhận biết, ít dùng trong project)

| Nhóm | Hàm | Dùng khi |
| --- | --- | --- |
| Debug | `writeBufferPBM(Print&)`, `writeBufferXBM(Print&)` (+`2`) | Xuất buffer ra Serial để xem ảnh — hữu ích khi không có OLED |
| UI/menu | `userInterfaceSelectionList`, `userInterfaceMessage`, `userInterfaceInputValue`, `getMenuEvent`, `setMenu*Pin` | Giao diện nút bấm; project chưa dùng |
| Log cuộn | `class U8G2LOG` | Màn hình log cuộn chữ |
| Con trỏ người dùng | `setUserPtr/getUserPtr` | Gắn dữ liệu cho callback |
| `u8x8` (U8X8lib) | Thư viện chỉ chữ, không buffer, ghi trực tiếp tile | Rất nhẹ RAM nhưng chỉ phông 8×8; không phù hợp đồ thị. API chính: `begin`, `setFont`, `drawString`, `drawUTF8`, `draw1x2String/draw2x2String`, `clear`, `home`, `setPowerSave`, `setContrast`; tọa độ theo **ô ký tự**; `setI2CAddress` cũng nhận địa chỉ ×2 |
| `U8G2_NULL` | Màn hình giả | Test không cần phần cứng |

---

## 10. Quy trình kiểm tra OLED (phục vụ CP1 Step 5)

1. `scanI2C` thấy `0x3C`.
2. `u8g2.setI2CAddress(0x3C << 1); u8g2.begin();`
3. `clearBuffer → setFont(u8g2_font_ncenB14_tr) → drawStr(0, 20, "Hello") → sendBuffer`.
4. Quan sát: chữ rõ, không nhiễu/lệch, không nhấp nháy; `Hello` ở hàng trên; Serial không có `Brownout`.
5. **Thử nghiệm đo**: bao `sendBuffer()` bằng `micros()` → in thời gian (đối chiếu ước lượng 26/100 ms ở mục 1.3); lặp ở 100 kHz và 400 kHz.
6. **Thử nghiệm xung đột**: nếu dùng chung MAX30102, in số mẫu `check()` trả về sau mỗi `sendBuffer()`; nếu ≥4 mẫu → mất dữ liệu.

---

## 11. Bảng nhanh constants

| Hằng | Giá trị |
| --- | --- |
| `U8G2_R0/R1/R2/R3` | Xoay 0/90/180/270° |
| `U8G2_MIRROR`, `U8G2_MIRROR_VERTICAL` | Lật gương |
| `U8X8_PIN_NONE` | 255 (không dùng chân) |
| `U8G2_DRAW_UPPER_RIGHT/UPPER_LEFT/LOWER_LEFT/LOWER_RIGHT/ALL` | 0x01/0x02/0x04/0x08/0x0F |
| Địa chỉ mặc định SSD1306 trong U8g2 | 0x78 (8-bit) = 0x3C (7-bit) |
| Clock mặc định SSD1306 | 400 kHz |
| Framebuffer `128X64_F` | 1024 byte |
| Kích thước khối dữ liệu I2C | 24 byte + control byte |

---

## 12. Cần tự kiểm chứng

1. **Thời gian `sendBuffer()` thực tế** ở 100 kHz/400 kHz: các số 26 ms / 100 ms là **ước lượng** từ cấu trúc gói, chưa đo.
2. **Chân reset / pull-up** của module OLED 4 chân: thường không có RESET; pull-up thường có sẵn trên module (và cả breakout MAX30102) → hai bộ pull-up **song song** làm điện trở tương đương nhỏ đi — kiểm bằng đồng hồ nếu bus 400 kHz không ổn.
3. **`Wire.begin()` gọi nhiều lần** có đặt lại pin hoặc gây lỗi trên core Arduino-ESP32 của bạn: thử và quét bus sau từng bước.
4. ~~`U8G2_WITH_CLIP_WINDOW_SUPPORT` cho `updateDisplayArea`~~ **đã kiểm xong**: macro này chỉ ảnh hưởng `setClipWindow`/`setMaxClipWindow`, và được `#define` sẵn trong `clib/u8g2.h` `[source]`; `updateDisplayArea` không phụ thuộc nó.
5. **Glyph tiếng Việt**: xác nhận phông bạn chọn có hay không (tìm trong danh sách phông; mặc định coi là không).
6. **Biến thể `NONAME` vs `VCOMH0` vs `ALT0`**: nếu hình có vệt/độ tương phản lạ, thử đổi biến thể.
7. **Dòng điện OLED khi trắng toàn màn** + LED MAX30102 50 mA có gây sụt áp/brownout: đã test ở CP1 ở mức bring-up; test lại khi đồng thời chạy cả hai ở dòng LED thực.
8. **Hành vi I2C của core ESP32 khi lỗi** (timeout `endTransmission`): cần xem phiên bản core cụ thể, ảnh hưởng đến watchdog.
