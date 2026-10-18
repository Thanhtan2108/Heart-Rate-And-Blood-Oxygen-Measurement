# Tài liệu tra cứu: SparkFun MAX3010x Library cho MAX30102

> Phạm vi: đọc từ source `MAX30105.h/.cpp`, `heartRate.h/.cpp`, `spo2_algorithm.h/.cpp` và 9 example.
>
> Cách đọc: **Lý thuyết → API → cạm bẫy → áp dụng vào project**. Mục "Cần tự kiểm chứng" ở cuối liệt kê những gì KHÔNG suy ra được từ source (phải xem datasheet hoặc đo thật).
>
> Quy ước: `[source]` = đã thấy trực tiếp trong code; `[datasheet]` = kiến thức chip, cần đối chiếu PDF; `[suy luận]` = mình suy ra, chưa đo.

---

## 0. Bức tranh tổng thể

Thư viện có **3 tầng độc lập**, hiểu tầng nào dùng cho việc gì là quan trọng nhất:

| Tầng | File | Việc làm | Chạm I2C? |
| --- | --- | --- | --- |
| 1. Driver chip | `MAX30105.h/.cpp` (class `MAX30105`) | Cấu hình thanh ghi, đọc FIFO, trả mẫu thô IR/RED | Có |
| 2. Phát hiện nhịp | `heartRate.h/.cpp` (`checkForBeat`) | Từ **từng mẫu IR** → báo "vừa có 1 nhịp" (thuật toán Maxim PBA) | Không (thuần toán) |
| 3. HR + SpO2 theo lô | `spo2_algorithm.h/.cpp` | Từ **buffer 100 mẫu IR+RED** → HR, SpO2, cờ hợp lệ | Không (thuần toán) |

Class tên `MAX30105` vì thư viện dùng chung họ MAX30105/MAX30102. MAX30102 **không có LED xanh** → chỉ dùng RED + IR.

Điều này khớp ràng buộc của project: *"SparkFun là driver, mình chỉ viết wrapper"* → `Max30102Driver` bọc tầng 1; `HeartRateEstimator` / `Spo2Estimator` là code của bạn, có thể **học** từ tầng 2, 3 nhưng không bắt buộc gọi chúng (đề bài yêu cầu peak 3 điểm + ngưỡng thích nghi, R-ratio).

---

## 1. Lý thuyết nền: cảm biến PPG hoạt động thế nào

### 1.1 PPG (photoplethysmography)

Chiếu ánh sáng vào mô (đầu ngón tay), đo lượng ánh sáng phản xạ/truyền qua. Mỗi nhịp tim bơm máu → thể tích máu động mạch ở đầu ngón tăng → hấp thụ nhiều ánh sáng hơn → tín hiệu thu được **dao động nhỏ** theo nhịp tim.

Tín hiệu = **DC lớn** (mô, xương, máu tĩnh mạch, ánh sáng nền; hàng chục nghìn đến hàng trăm nghìn count) + **AC nhỏ** (thành phần theo nhịp, thường ~0.1–2% của DC). Hầu hết thuật toán đều quy về: *tách DC, nhìn AC*.

### 1.2 Vì sao có 2 LED (RED + IR) → SpO2

Hemoglobin oxy hóa (HbO2) và khử oxy (Hb) hấp thụ **khác nhau** ở bước sóng đỏ (~660 nm) và hồng ngoại (~880 nm) `[datasheet]`:

- Hb hấp thụ đỏ mạnh hơn HbO2.
- HbO2 hấp thụ IR mạnh hơn một chút.

Đo AC/DC của từng bước sóng rồi lấy tỉ số:

```cpp
R = (AC_red / DC_red) / (AC_ir / DC_ir)
```

Chia cho DC để **khử ảnh hưởng độ dày mô, lực ép, cường độ LED** (cùng một đường quang học cho cả hai màu). SpO2 sau đó là hàm **đã hiệu chuẩn thực nghiệm** của R (xem mục 6). Không có "công thức vật lý chính xác": đó là lý do sai số ±3–5% là chấp nhận được với module rẻ tiền.

### 1.3 Chuỗi đo bên trong chip (để hiểu các tham số cấu hình)

```text
LED xung (độ rộng = pulse width, dòng = LED current)
   → photodiode → ADC (full-scale = ADC range, độ phân giải phụ thuộc pulse width)
   → (tùy chọn) trung bình N mẫu (sample averaging)
   → ghi vào FIFO 32 mẫu
   → MCU đọc qua I2C
```

Mỗi tham số trong chuỗi này tương ứng một hàm `set...()` của thư viện (mục 3).

---

## 2. Lý thuyết nền: thanh ghi, FIFO, I2C của MAX30102

### 2.1 Địa chỉ và định danh

- Địa chỉ I2C 7-bit: **0x57** `[source: MAX30105_ADDRESS]`.
- Part ID (thanh ghi 0xFF) = **0x15**, Revision ID ở 0xFE `[source]`.
- `begin()` dùng Part ID để xác thực chip: sai → trả `false`.

### 2.2 Đọc/ghi thanh ghi và `bitMask`

Cấu hình chip là tập các thanh ghi 8-bit, mỗi thanh ghi nhồi **nhiều trường bit** (VD `PARTICLECONFIG 0x0A` chứa ADC range + sample rate + pulse width). Muốn đổi 1 trường mà không phá trường khác → **read–modify–write**:

```text
giá trị mới = (giá trị cũ & mask) | giá trị_trường_mới
```

Thư viện làm đúng việc này trong `bitMask(reg, mask, thing)` `[source]`. Hệ quả:

- Mỗi lần gọi một hàm `setXxx` = **1 lần đọc + 1 lần ghi** I2C (2 transaction) → cấu hình chỉ nên làm lúc khởi động, không làm trong vòng lặp thời gian thực.
- **Không an toàn nếu hai task cùng gọi** (đọc–sửa–ghi bị xen nhau). Mutex I2C của bạn phải bao quanh.

### 2.3 Đọc 1 thanh ghi cần 2 pha

`readRegister8` `[source]`: `beginTransmission(addr)` → `write(reg)` → `endTransmission(false)` (**repeated START**, không nhả bus) → `requestFrom(addr, 1)`.
Lý do: pha 1 là *ghi con trỏ thanh ghi* (chip dùng byte đầu tiên làm địa chỉ thanh ghi); pha 2 là *đọc dữ liệu từ con trỏ đó*. Nếu dùng `endTransmission()` (STOP) giữa hai pha, một số chip vẫn chạy, nhưng repeated START giữ bus và đảm bảo không ai chen vào giữa.

> Đây cũng là câu hỏi còn treo từ module `scanI2C`: *tại sao `endTransmission(false)`?* — câu trả lời là đoạn trên.

⚠ `readRegister8` **trả 0 khi lỗi** (không có cách phân biệt lỗi với giá trị 0 thật) `[source]`. Hệ quả: các hàm thư viện không báo lỗi bus. Vì vậy module `scanI2C` của bạn có `readI2CRegister` trả `bool` riêng.

### 2.4 FIFO của chip

- Sâu **32 mẫu** `[source: wrap +32; datasheet]`. Mỗi mẫu = số LED hoạt động × **3 byte** (mỗi kênh 18-bit nằm trong 24-bit, dùng 18 bit thấp).
- 3 thanh ghi con trỏ: **FIFO_WR_PTR (0x04)**, **FIFO_RD_PTR (0x06)**, **OVERFLOW counter (0x05)**; dữ liệu lấy qua **FIFO_DATA (0x07)**: đọc liên tục từ đúng địa chỉ này, chip tự tăng con trỏ đọc `[datasheet]`.
- Số mẫu chưa đọc = `(WR − RD) mod 32` `[source]`.
- **Rollover** (`enableFIFORollover`): khi đầy, mẫu mới ghi đè mẫu cũ thay vì bị bỏ.

> `[suy luận]` Khi FIFO **đầy tuyệt đối** (32 mẫu chưa đọc), WR_PTR == RD_PTR → `check()` thấy `readPointer == writePointer` và coi như **không có dữ liệu**. Nghĩa là nếu task đọc bị treo quá thời gian lấp đầy FIFO (ở 25 Hz: 32/25 ≈ 1.28 s), bạn có thể mất cả lô mà thư viện không báo. → Phải đảm bảo `check()` được gọi đều đặn (watchdog/đo thời gian giữa 2 lần gọi).

### 2.5 Sample averaging (điểm dễ nhầm nhất)

Chip lấy mẫu ở `sampleRate`, nhưng nếu `sampleAverage = N` thì **gộp N mẫu thành 1 mẫu FIFO**:

```text
tần số mẫu ra FIFO = sampleRate / sampleAverage
```

Ví dụ project: `sampleRate=100`, `sampleAverage=4` → **25 Hz**, khớp `FreqS = 25` trong `spo2_algorithm.h` `[source]`. Nếu bạn đổi một trong hai mà không đổi `FreqS`, **HR tính sai tỉ lệ** (vì `HR = FreqS×60 / khoảng_cách_mẫu`).

Lợi ích của averaging: giảm nhiễu √N lần và giảm tải I2C, đổi lại băng thông tín hiệu thấp đi (25 Hz đủ cho PPG: nhịp tim < 4 Hz).

### 2.6 Độ phân giải ADC vs pulse width

Pulse width càng rộng → tích phân ánh sáng lâu hơn → nhiều bit hơn `[source: comment trong setup()]`:

| pulseWidth (µs) | Độ phân giải |
| --- | --- |
| 69 | 15 bit |
| 118 | 16 bit |
| 215 | 17 bit |
| 411 | 18 bit |

Pulse width cũng **giới hạn sample rate tối đa** (xung rộng thì không nhét được nhiều xung/giây) `[datasheet — bảng trong datasheet, cần tự tra]`. Chọn tổ hợp không hợp lệ → chip có thể tự hạ rate. Đây là mục "Cần tự kiểm chứng" #3.

### 2.7 ADC range & LED current

- **ADC range** (2048/4096/8192/16384 nA full-scale): range lớn → ít bị bão hòa nhưng mỗi count thô hơn (`[source]` comment: 7.81 / 15.63 / 31.25 / 62.5 pA mỗi LSB).
- **LED current**: thanh ghi 8-bit. `[source]`: `0x00 = 0 mA`, `0x7F ≈ 25.4 mA`, `0xFF ≈ 50 mA`; mỗi bước ≈ 0.2 mA. Dòng lớn → tín hiệu lớn hơn nhưng nóng hơn, tốn điện, dễ **bão hòa ADC** (giá trị kẹt ở trần 262143 = 2¹⁸−1).
- Quy tắc chỉnh thực tế: **tăng dòng LED cho tới khi DC IR nằm khoảng giữa thang ADC (không chạm trần)**, rồi mới chỉnh ADC range.

---

## 3. API class `MAX30105` — tra cứu chi tiết

Giả sử `MAX30105 particleSensor;`

### 3.1 Khởi động

| Hàm | Mục đích / Khi dùng | Tham số | Trả về | Ghi chú & cạm bẫy |
| --- | --- | --- | --- | --- |
| `bool begin(TwoWire& wire=Wire, uint32_t speed=100000, uint8_t addr=0x57)` | Gắn thư viện với 1 bus I2C, gọi `wire.begin()`, `setClock(speed)`, đọc Part ID để xác thực. Gọi **1 lần** ở `setup()`. | `wire`: cổng I2C; `speed`: `I2C_SPEED_STANDARD`(100k)/`I2C_SPEED_FAST`(400k); `addr`: 7-bit | `true` nếu Part ID == 0x15 | ① Gọi `Wire.begin()` bên trong. Nếu bạn đã `Wire.begin(21,22)` trước đó, gọi lại không có tham số trên ESP32 **thường vẫn giữ pin mặc định 21/22** (`[suy luận]`, cần thử). ② Đặt `setClock` của **cả bus**, và U8g2 cũng đặt clock riêng (xem file U8g2 mục lưu ý). ③ Chỉ kiểm tra Part ID, **không** kiểm tra revision. ④ Tham số `addr` chỉ lưu vào `_i2caddr`; xem cạm bẫy `check()` hard-code bên dưới. |
| `void setup(powerLevel=0x1F, sampleAverage=4, ledMode=3, sampleRate=400, pulseWidth=411, adcRange=4096)` | "Cấu hình nhanh" một lệnh: soft reset, set averaging, rollover, LED mode, ADC, rate, pulse width, dòng LED, slot, `clearFIFO`. | xem bảng map bên dưới | – | ① **Mặc định `ledMode=3` là LED xanh → MAX30102 không có** → luôn truyền `ledMode=2`. ② Comment trong source ghi "sample rate = 50 / adc 16384" nhưng **giá trị mặc định thật là 400 / 4096** — đừng tin comment. ③ `setup()` là nơi **duy nhất** gán `activeLEDs`; nếu bạn tự gọi từng `setXxx` mà không gọi `setup()`, `check()` đọc sai số byte. ④ Gọi `softReset()` đầu tiên nên cấu hình cũ bị xóa. |

**Map giá trị `setup()` → thanh ghi** `[source]` (làm tròn xuống theo ngưỡng):

| Tham số | Giá trị nhận | Quy tắc |
| --- | --- | --- |
| `sampleAverage` | 1,2,4,8,16,32 | giá trị khác → 4 |
| `ledMode` | 1 = chỉ RED, 2 = RED+IR, 3 = RED+IR+Green | khác 1,2,3 → thanh ghi mode = chỉ RED, **nhưng `activeLEDs = ledMode` vẫn giữ số sai** → `check()` đọc sai số byte/mẫu, dữ liệu lệch. Không có validate `[source]`: chỉ truyền 1, 2 hoặc 3 |
| `adcRange` | `<4096`→2048; `<8192`→4096; `<16384`→8192; `==16384`→16384 | |
| `sampleRate` | `<100`→50; `<200`→100; `<400`→200; `<800`→400; `<1000`→800; `<1600`→1000; `<3200`→1600; `==3200`→3200 | |
| `pulseWidth` | `<118`→69; `<215`→118; `<411`→215; `==411`→411 | |
| `powerLevel` | 0x00–0xFF, ghi vào 4 kênh (RED, IR, Green, Proximity) | |

Cấu hình đề xuất cho project (đã tính ở mục 2.5): `setup(powerLevel, 4, 2, 100, 411, 4096)`; `powerLevel` chỉnh theo quan sát DC (khoảng giữa thang). Đây là **điểm xuất phát**, phải kiểm lại trên tín hiệu thật.

### 3.2 Cấu hình riêng lẻ (dùng khi cần tinh chỉnh sau `setup()`)

| Hàm | Mục đích | Tham số hợp lệ | Ghi chú |
| --- | --- | --- | --- |
| `setLEDMode(mode)` | Chọn kênh LED hoạt động | `MAX30105_MODE_REDONLY(0x02)`, `MODE_REDIRONLY(0x03)`, `MODE_MULTILED(0x07)` | ⚠ Tên hằng nhìn "ngược": REDONLY = 0x02 là mã của chip, không phải số LED. Không cập nhật `activeLEDs`. |
| `setADCRange(r)` | Full-scale ADC | `MAX30105_ADCRANGE_2048/4096/8192/16384` | |
| `setSampleRate(r)` | Tốc độ lấy mẫu thô | `MAX30105_SAMPLERATE_50…3200` | Nhớ chia cho averaging |
| `setPulseWidth(w)` | Độ rộng xung LED ↔ số bit | `MAX30105_PULSEWIDTH_69/118/215/411` | Có sai chính tả comment `_188` (thực tế là 118) |
| `setPulseAmplitudeRed/IR/Green/Proximity(v)` | Dòng LED từng kênh (0–255) | 0x00…0xFF | RED và IR **độc lập**; SpO2 cần cân bằng DC của 2 kênh tương đương để R có nghĩa |
| `setFIFOAverage(x)` | Số mẫu gộp | `MAX30105_SAMPLEAVG_1…32` | Hằng số khác với số nguyên của `setup()` |
| `enableFIFORollover()` / `disableFIFORollover()` | Ghi đè khi đầy / dừng khi đầy | – | `setup()` bật sẵn |
| `setFIFOAlmostFull(n)` | Ngưỡng A_FULL (n = số "chỗ trống còn lại", đếm ngược) | 0x00=32 mẫu … 0x0F=17 mẫu | Ngược trực giác `[datasheet]` — header/cpp của thư viện **không** ghi ý nghĩa này, chỉ thấy hàm ghi `n` vào 4 bit thấp thanh ghi FIFOCONFIG (mask 0xF0) `[source]` |
| `enableSlot(slot, device)` / `disableSlots()` | Multi-LED mode: gán LED vào khe thời gian 1–4 | `SLOT_RED_LED(1)`, `SLOT_IR_LED(2)`, `SLOT_GREEN_LED(3)`… | Chỉ cần khi dùng chế độ multi-LED; `setup()` đã gọi |
| `setProximityThreshold(msb)` / `setPROXINTTHRESH(v)` | Ngưỡng báo "có vật" (chỉ cho MAX30105) | 8 bit cao của ADC | Không dùng trong project |

### 3.3 Điều khiển nguồn

| Hàm | Mục đích | Chi tiết |
| --- | --- | --- |
| `softReset()` | Về giá trị mặc định chip | Ghi bit RESET (0x40) ở `MODECONFIG` rồi **poll** tới khi bit tự xóa, timeout 100 ms. Sau reset phải cấu hình lại hết. |
| `shutDown()` / `wakeUp()` | Tiết kiệm điện: tắt/bật | Chip vẫn trả lời I2C khi shutdown, thanh ghi giữ nguyên. Hữu ích cho state machine (trạng thái IDLE → tắt LED). |

### 3.4 Đọc dữ liệu (quan trọng nhất)

Thư viện có **2 kiểu đọc**, không trộn lẫn:

**Kiểu A – "lấy mẫu mới nhất" (đơn giản, chặn):**

| Hàm | Mục đích | Cạm bẫy |
| --- | --- | --- |
| `uint32_t getIR()` / `getRed()` / `getGreen()` | Gọi `safeCheck(250)` rồi trả mẫu **mới nhất** | **Chặn tới 250 ms** nếu không có dữ liệu mới (trả 0). Bỏ qua mẫu cũ (mất mẫu nếu gọi chậm). Ổn cho demo, **không dùng** cho thuật toán nhịp tim (cần chuỗi mẫu liên tục). |
| `bool safeCheck(uint8_t maxMs)` | `check()` lặp với `delay(1)` tới hạn `maxMs` | Điều kiện thoát là `check() == true`, tức **đúng 1 mẫu**, không phải "≥1 mẫu" `[source]`. Nếu lô có ≥2 mẫu, vòng lặp tiếp tục chờ cho tới hết hạn. Quirk của thư viện, không dùng trong code thời gian thực của bạn. |

**Kiểu B – "đọc theo hàng đợi" (dùng cho project):**

```cpp
loop:
    particleSensor.check();                     // 1) gom mẫu từ FIFO chip vào bộ đệm phần mềm
    while (particleSensor.available()) {        // 2) còn mẫu chưa lấy?
        uint32_t red = particleSensor.getFIFORed();
        uint32_t ir  = particleSensor.getFIFOIR();
        particleSensor.nextSample();            // 3) bắt buộc để tiến con trỏ đuôi
        // xử lý (red, ir)
    }
```

| Hàm | Mục đích | Chi tiết |
| --- | --- | --- |
| `uint16_t check()` | Hỏi chip có mẫu mới không; nếu có, đọc **burst** từ FIFO_DATA vào bộ đệm vòng phần mềm. Trả số mẫu chip đã có. | Xem phân tích bên dưới |
| `uint8_t available()` | Số mẫu trong bộ đệm phần mềm chưa được `nextSample()` | `(head − tail) mod 4` |
| `getFIFORed/IR/Green()` | Mẫu tại **đuôi** (cũ nhất chưa lấy) | Không chạm I2C |
| `nextSample()` | Dịch đuôi, bỏ qua mẫu vừa đọc | Chỉ tiến nếu `available()>0` |
| `clearFIFO()` | Đặt WR/RD/OVF ptr = 0 **của chip** | Gọi khi bắt đầu phiên đo mới/sau khi đặt ngón tay để xả mẫu cũ trong FIFO chip. ⚠ **Không** xóa bộ đệm phần mềm (`head`/`tail`, mảng `red[]`/`IR[]`) `[source]`: trước đó hãy `while (available()) nextSample();` để bỏ mẫu còn sót. |
| `getWritePointer()` / `getReadPointer()` | Đọc con trỏ FIFO chip | Dùng chẩn đoán (chip đã sinh bao nhiêu mẫu) |

**Phân tích `check()` [source]:**

1. Đọc `RD_PTR` và `WR_PTR` (2 transaction). Bằng nhau → trả 0.
2. `n = WR − RD (+32 nếu âm)`, số byte = `n × activeLEDs × 3`.
3. Ghi con trỏ về thanh ghi FIFO_DATA, rồi `requestFrom` theo **khối ≤ `I2C_BUFFER_LENGTH`** (thư viện định nghĩa = 32 cho nền tảng không phải AVR/SAMD) và cắt về bội số của `activeLEDs×3` → với RED+IR là **30 byte/khối** (5 mẫu).
4. Với mỗi mẫu: ghép 3 byte big-endian, **mask `0x3FFFF` (18 bit)** vì 6 bit cao không mang dữ liệu.
5. Ghi vào bộ đệm vòng `STORAGE_SIZE = 4` mẫu.

Hệ quả thiết kế (quan trọng cho FreeRTOS):

- Bộ đệm phần mềm chỉ **4 mẫu**. Nếu một lần `check()` đọc >4 mẫu, **mẫu cũ bị ghi đè** mà `available()` chỉ thấy tối đa 3. Tệ hơn: nếu `check()` ghi **đúng 4 mẫu** khi bộ đệm đang rỗng, `head` quay về đúng chỗ `tail` → `available()` báo **0** (mất cả 4 mẫu mà không có dấu hiệu); ghi 5 mẫu thì báo 1 `[source: head = (head+1)%4; available = (head−tail) mod 4]`. **Quy tắc an toàn: mỗi lần `check()` chỉ được đọc ≤ 3 mẫu mới**, và phải `drain` hết `available()` ngay sau đó. → `check()` phải được gọi **đủ thường**: ở 25 Hz (40 ms/mẫu) nên gọi ≤ 40–80 ms một lần, tuyệt đối không để task đọc bị OLED chặn lâu (xem mục 8).
- `check()` **hard-code `MAX30105_ADDRESS`** trong `beginTransmission/requestFrom` thay vì `_i2caddr` `[source]` → tham số `addr` của `begin()` chỉ có tác dụng cho các hàm đọc thanh ghi, không cho burst FIFO. Không ảnh hưởng vì chip cố định 0x57.
- `check()` **không phải thread-safe** và gồm nhiều transaction → mutex phải bao **cả hàm `check()`**, không phải từng transaction.
- Không phát hiện lỗi bus: `requestFrom` thất bại → `read()` trả −1/0xFF → mẫu rác, thư viện vẫn trả bình thường.

### 3.5 Nhiệt độ die

| Hàm | Mục đích | Chi tiết |
| --- | --- | --- |
| `float readTemperature()` | Nhiệt độ **chip** (không phải da) để bù trôi nhiệt/chẩn đoán | Ghi `DIETEMPCONFIG=1`, poll `INTSTAT2` bit DIE_TEMP_RDY tới 100 ms, đọc int (có dấu) + frac × 0.0625. **Chặn tới 100 ms**; dùng `enableDIETEMPRDY()` trước (issue #19 trong comment). Hết 100 ms mà cờ chưa lên thì **vẫn đọc thanh ghi và trả giá trị như bình thường, không báo lỗi** (dòng `return -999` đang bị comment trong source `[source]`) → kết quả có thể là số cũ/sai mà không biết. |
| `readTemperatureF()` | Như trên, °F | Có kiểm tra `-999` nhưng `readTemperature` không bao giờ trả −999 → vô tác dụng. |

Không đưa vào vòng đo chính (chặn 100 ms phá nhịp 40 ms). Nếu dùng: gọi hiếm, trong khoảng lặng.

### 3.6 Ngắt (INT)

Chân INT của breakout (nối GPIO4 trong project) là **active-low, open-drain** `[datasheet]` — cần pull-up (thường có sẵn trên module `[cần kiểm]`).

| Nhóm | Hàm bật/tắt | Ý nghĩa | Đọc/xóa |
| --- | --- | --- | --- |
| INT1 (thanh ghi 0x02/0x00) | `enable/disableAFULL()` | FIFO còn ≤ N chỗ (A_FULL) | `getINT1()` — **đọc thanh ghi trạng thái sẽ xóa cờ** `[datasheet]` |
| | `enable/disableDATARDY()` | Có mẫu mới (mỗi mẫu) | |
| | `enable/disableALCOVF()` | Ánh sáng nền làm ADC tràn | |
| | `enable/disablePROXINT()` | Chỉ MAX30105 | |
| INT2 (0x03/0x01) | `enable/disableDIETEMPRDY()` | Đo nhiệt xong | `getINT2()` |

Hai cách đọc dữ liệu:

- **Polling** (Example 5, 6, 8): gọi `check()` theo chu kỳ. Đơn giản, đủ cho 25 Hz.
- **Ngắt** (Example 7): `enableDATARDY/AFULL` + ISR trên GPIO4 chỉ **đặt cờ / `xTaskNotifyFromISR`**; việc đọc I2C vẫn làm trong task, **không đọc I2C trong ISR** (I2C chặn, Wire không an toàn trong ISR).
Với project 25 Hz, polling bằng `vTaskDelayUntil` rẻ và đủ; INT là phương án nâng cao (không cắt được thì bỏ).

### 3.7 ID

`readPartID()` (0xFF → 0x15), `getRevisionID()` (cache từ `begin()`).

### 3.8 Low-level (public nhưng dùng cẩn thận)

`readRegister8(addr, reg)` (trả 0 khi lỗi), `writeRegister8(addr, reg, val)` (không báo lỗi). Dùng để đọc thanh ghi chẩn đoán; không dựa vào chúng để phát hiện lỗi bus.

---

## 4. `heartRate.h` — `checkForBeat()` (Maxim PBA)

```cpp
bool checkForBeat(int32_t sample);   // gọi 1 lần cho mỗi mẫu IR; true = vừa có đỉnh nhịp
```

### 4.1 Nó làm gì (lý thuyết)

Chuỗi xử lý mỗi mẫu `[source]`:

1. **Ước lượng DC** (`averageDCEstimator`): bộ lọc IIR bậc 1 `p += ((x<<15) − p) >> 4` → trung bình trượt hàm mũ, hệ số 1/16, tính bằng số cố định Q15. Trả `p>>15`.
2. **AC = mẫu − DC**, rồi **lọc thông thấp FIR** 32-tap đối xứng (hệ số `FIRCoeffs`, tổng quy đổi >>15) để bỏ nhiễu tần số cao.
3. **Tìm điểm cắt không dương** (zero-crossing đi lên): khi AC trước <0 và hiện tại ≥0 → chốt biên độ `Max − Min` của chu kỳ vừa rồi; nếu nằm trong **(20, 1000)** thì coi là **một nhịp**.
4. Cập nhật max/min trong các nửa chu kỳ dương/âm.

Cách dùng điển hình (Example 5): ghi mốc `millis()` mỗi lần `true`, `bpm = 60000/Δt`, rồi **trung bình 4 giá trị** (comment trong source khuyến nghị).

### 4.2 Cạm bẫy

- **Trạng thái toàn cục** (`cbuf`, `ir_avg_reg`, `IR_AC_*`…) → chỉ dùng cho **một luồng tín hiệu**; không có hàm reset. Đổi ngón tay/khởi động lại phiên đo → bộ lọc còn trạng thái cũ. Không viết unit test native dễ dàng vì không reset được.
- **Cắt 16 bit**: `averageDCEstimator(int32_t*, uint16_t x)` nhận `uint16_t` → mẫu IR 18 bit bị **cắt mất 2 bit cao** khi truyền `x`; trong `checkForBeat` mẫu `int32_t` bị ép xuống `uint16_t`. Với IR ~ 50 000–200 000 count, DC ước lượng sai/bẻ gập; vẫn "chạy được" vì thuật toán chủ yếu nhìn AC sau khi trừ DC, nhưng đây là nguồn lỗi tinh vi `[suy luận]`, cần kiểm chứng bằng cách in `IR_Average_Estimated`.
- Ngưỡng (20, 1000) **cố định**, không thích nghi theo biên độ tín hiệu → ngón chạm yếu/mạnh là đổi kết quả. Đây là lý do project dùng **adaptive threshold tự viết**.
- Không có đánh giá tin cậy; không có timeout "mất nhịp".

### 4.3 Khi nào dùng / không dùng

- Dùng để **so sánh** với thuật toán tự viết (đối chứng khi debug CP3) hoặc làm demo nhanh.
- **Không dùng làm HeartRateEstimator của project** (đề bài yêu cầu tự làm 3 điểm + ngưỡng thích nghi). Nhưng *ý tưởng* (loại DC, lọc thông thấp, cắt zero, lọc theo biên độ) đáng học.

---

## 5. `spo2_algorithm.h` — `maxim_heart_rate_and_oxygen_saturation()`

```cpp
void maxim_heart_rate_and_oxygen_saturation(
    uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length,
    uint32_t *pun_red_buffer,
    int32_t *pn_spo2, int8_t *pch_spo2_valid,
    int32_t *pn_heart_rate, int8_t *pch_hr_valid);
```

(Tên tham số ở trên là tên **thật trong source** `[source: spo2_algorithm.h]`; trong example bạn sẽ thấy biến gọi là `validSPO2`, `validHeartRate`… chỉ là tên biến của example. Kiểu dữ liệu: chỉ riêng **ATmega328P/168 (Uno)** dùng `uint16_t` cho cả hai buffer — thư viện cắt 16 bit cao để tiết kiệm RAM; mọi board khác kể cả ESP32 dùng `uint32_t`.)

### 5.1 Tham số

| Tham số | Ý nghĩa |
| --- | --- |
| `pun_ir_buffer`, `pun_red_buffer` | Mảng mẫu thô, **cùng độ dài, cùng thời điểm** |
| `n_ir_buffer_length` | Phải bằng `BUFFER_SIZE = FreqS × 4 = 100` (4 giây ở 25 Hz). Code dùng `BUFFER_SIZE` cứng ở nhiều chỗ → **không được đưa length khác** |
| `pn_spo2`, `pch_spo2_valid` | Kết quả SpO2 %, hoặc `−999` và `valid=0` |
| `pn_heart_rate`, `pch_hr_valid` | BPM, hoặc `−999`, `valid=0` |

Các biến `FreqS=25`, `BUFFER_SIZE=100`, `MA4_SIZE=4` là macro trong header; `an_x[]`, `an_y[]` là mảng `static` **trong header** (mỗi translation unit include sẽ có bản riêng, tốn RAM nếu include ở nhiều file `.cpp`).

### 5.2 Thuật toán (lý thuyết, theo thứ tự thực hiện)

1. **Bỏ DC & đảo dấu** IR: `x = −(IR − mean)` → đáy sóng thành đỉnh để dùng bộ dò đỉnh.
2. **Trung bình trượt 4 điểm** (làm mượt).
3. **Ngưỡng** `th = mean(x)` bị kẹp vào **[30, 60]**.
4. **Dò đỉnh** (`maxim_find_peaks`): đỉnh > ngưỡng, cách nhau ≥ **4 mẫu** (≈160 ms → trần ~375 bpm), tối đa 15 đỉnh.
5. **HR** = `FreqS × 60 / (khoảng cách đỉnh trung bình, đơn vị mẫu)`. Cần ≥2 đỉnh, nếu không `−999`.
6. Với mỗi cặp đáy liên tiếp: tìm cực đại RED và IR trong khoảng, trừ đường DC nội suy tuyến tính giữa hai đáy → **AC_red, AC_ir**; lấy `DC_max` làm DC.
7. `ratio_k = (AC_red·DC_ir) / (AC_ir·DC_red) × 100` (số nguyên). Lưu tối đa 5 giá trị.
8. **Trung vị** các ratio (chống ngoại lai từng nhịp).
9. `SpO2 = uch_spo2_table[ratio]` nếu `2 < ratio < 184` — bảng xấp xỉ **SpO2 ≈ −45.060·R² + 30.354·R + 94.845** (R = ratio/100).

### 5.3 Độ phân giải & giới hạn (để bảo vệ trước hội đồng)

- Ở 25 Hz, một mẫu = 40 ms. Với HR 75 bpm (chu kỳ ≈ 20 mẫu), **sai số 1 mẫu ≈ ±5%** (≈ ±4 bpm). Trung bình nhiều khoảng giảm sai số này; đó là lý do Maxim lấy 4 giây.
- Công thức SpO2 là **thực nghiệm cho module của Maxim**; module rẻ tiền (LED/quang học khác) lệch hệ thống. ±3–5% là mục tiêu thực tế; **không phải thiết bị y tế**.
- Giá trị `R` hợp lý của người khỏe mạnh thường ~0.4–1.0 → SpO2 tính ra 95–100% `[datasheet/ứng dụng — cần kiểm]`.

### 5.4 Quirk trong source (không phải lỗi của bạn)

- Trong tính `n_x_ac` dùng chỉ số `n_y_dc_max_idx` thay vì `n_x_dc_max_idx` → IR dùng vị trí đỉnh của RED. Thường sai khác nhỏ vì hai đỉnh gần nhau, nhưng là sai lệch thật.
- Thuật toán **khối (batch)**: phải gom đủ 100 mẫu rồi gọi. Ví dụ chuẩn (Example 8): thu 100 mẫu ban đầu, rồi **trượt**: bỏ 25 mẫu cũ nhất, thêm 25 mẫu mới, tính lại mỗi giây.
- Chi phí tính toán nhỏ (vài trăm µs trên ESP32 `[suy luận]`, nên đo bằng `micros()`).
- Không có thông tin chất lượng tín hiệu; "valid" chỉ nói đã tính được hay chưa, không nói có tin được hay không.

### 5.5 Hàm phụ (public trong header)

`maxim_find_peaks`, `maxim_peaks_above_min_height`, `maxim_remove_close_peaks`, `maxim_sort_ascend`, `maxim_sort_indices_descend` — tiện ích dò đỉnh/sắp xếp; dùng được độc lập, hữu ích làm **test oracle** cho `HeartRateEstimator` tự viết (cùng input, so kết quả).

### 5.6 Áp dụng vào project

Tự viết `Spo2Estimator` bằng R-ratio (đã chọn). Dùng hàm Maxim làm **đối chứng khi debug** (CP5): cho cả hai cùng nhận một buffer ghi lại từ Serial, so SpO2. Sai khác lớn → nghi ngờ code mình hoặc dữ liệu.

---

## 6. Hướng dẫn chọn cấu hình & quy trình xác nhận

1. `begin()` trả `true` (Part ID khớp).
2. `setup(power, 4, 2, 100, 411, 4096)`; in `getIR()` khi **chưa đặt ngón** → giá trị thấp (vài nghìn); đặt ngón → tăng mạnh (≥ vài chục nghìn).
3. Nếu IR bị kẹt ở **262143** → bão hòa: giảm `power` hoặc tăng `adcRange`.
4. Nếu IR quá thấp, nhiễu: tăng `power`.
5. Chỉnh RED cùng dải với IR (DC tương đương).
6. Xác nhận chu kỳ mẫu: đo `millis()` giữa 2 mẫu liên tiếp ≈ 40 ms.
7. Chỉ khi tín hiệu AC nhìn thấy trên Serial Plotter mới làm thuật toán.

---

## 7. Hàm ↔ Class `Max30102Driver` (gợi ý wrapper)

| Việc của wrapper | Gọi |
| --- | --- |
| `bool init(TwoWire&, uint32_t clk)` | `begin(...)` + `setup(...)` + log Part ID |
| `bool readSamples(Sample* out, uint8_t max, uint8_t& n)` | `check()` → vòng `available()` → `getFIFOIR/Red` → `nextSample()` |
| `void flush()` | `clearFIFO()` |
| `void sleep()/wake()` | `shutDown()/wakeUp()` |
| Báo lỗi | Tự kiểm lỗi bus bằng `scanI2C` (`isI2CDevicePresent`) vì thư viện không báo |

Ràng buộc đã có: không `new`, không `std::vector` → dùng `struct Sample{ uint32_t ir, red; }` và mảng tĩnh.

---

## 8. Liên hệ FreeRTOS (CP4)

- **Một mutex I2C** bao **nguyên** `check()` (nhiều transaction) và nguyên `sendBuffer()` của U8g2. Không bao từng transaction riêng.
- `check()` và OLED **cùng bus**: nếu OLED giữ bus lâu hơn ~3 chu kỳ lấy mẫu, bộ đệm 4 mẫu của thư viện sẽ mất dữ liệu (xem file U8g2 mục thời gian truyền).
- Task đọc: `vTaskDelayUntil` mỗi ~40 ms (khớp 25 Hz) hoặc nhanh hơn một chút; đẩy mẫu vào `sampleQueue` (struct nhỏ, copy theo giá trị).
- ISR (nếu dùng INT/GPIO4): chỉ notify, không đọc I2C.
- Watchdog: task đọc phải "feed" khi `check()` có tiến triển; nếu bus treo thì `requestFrom` có thể chờ lâu (tùy timeout Wire).

---

## 9. Bảng nhanh hằng số quan trọng `[source]`

| Hằng | Giá trị |
| --- | --- |
| `MAX30105_ADDRESS` | 0x57 |
| Part ID | 0x15 (thanh ghi 0xFF) |
| Thanh ghi chính | FIFO_WR 0x04, OVF 0x05, FIFO_RD 0x06, FIFO_DATA 0x07, FIFO_CFG 0x08, MODE 0x09, PARTICLE(SPO2) 0x0A, LED1(RED) 0x0C, LED2(IR) 0x0D, DIE_TEMP 0x1F/0x20/0x21 |
| `SAMPLEAVG_*` | 0x00, 0x20, 0x40, 0x60, 0x80, 0xA0 |
| `SAMPLERATE_*` | 50→0x00, 100→0x04, 200→0x08, 400→0x0C, 800→0x10, 1000→0x14, 1600→0x18, 3200→0x1C |
| `PULSEWIDTH_*` | 69→0, 118→1, 215→2, 411→3 |
| `ADCRANGE_*` | 2048→0x00, 4096→0x20, 8192→0x40, 16384→0x60 |
| `MODE_*` | REDONLY 0x02, REDIRONLY 0x03, MULTILED 0x07 |
| `STORAGE_SIZE` | 4 mẫu (bộ đệm phần mềm) |
| `I2C_BUFFER_LENGTH` | 32 (nền tảng không phải AVR/SAMD) |
| `FreqS`, `BUFFER_SIZE` | 25 Hz, 100 mẫu |

---

## 10. Cần tự kiểm chứng (không suy ra được từ source)

1. **Bước sóng LED** (RED ~660 nm, IR ~880 nm) và nguyên lý hấp thụ → đối chiếu datasheet MAX30102 (có trong `datasheets/MAX30102.PDF`).
2. **Bảng sample rate tối đa theo pulse width** → tra datasheet. Tổ hợp (100 Hz, 411 µs) cần xác nhận hợp lệ.
3. **Hành vi chân INT** (active-low, xóa cờ khi đọc thanh ghi, có pull-up trên breakout chưa) → datasheet + đo bằng đồng hồ/serial.
4. **FIFO đầy: WR_PTR == RD_PTR** và `check()` coi như không có dữ liệu → tự thử: tạm dừng `check()` > 1.4 s rồi quan sát `getWritePointer/getReadPointer`.
5. **`I2C_BUFFER_LENGTH` thực tế trên Arduino-ESP32**: thư viện định nghĩa 32; core ESP32 có thể có macro cùng tên (có thể cảnh báo redefinition). Kiểm bằng log lúc biên dịch.
6. **`Wire.begin()` gọi lại không tham số có giữ pin 21/22?** Thử bằng cách in `Wire.getBufferSize()` và quét I2C sau `begin`.
7. **Cắt 16 bit trong `checkForBeat`** có ảnh hưởng thật hay không: in `IR_Average_Estimated` với IR > 65 535.
8. **Thời gian chạy** `maxim_heart_rate_and_oxygen_saturation` trên ESP32: đo `micros()` (con số mình đưa là phỏng đoán).
9. **Cấu hình khởi điểm** (power level, adcRange) tùy ngón tay và module: phải thử.
