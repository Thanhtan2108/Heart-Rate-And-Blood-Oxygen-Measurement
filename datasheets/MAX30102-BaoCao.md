# Báo cáo chi tiết cảm biến MAX30102

**Nguồn tài liệu:** Datasheet Maxim Integrated — *MAX30102 High-Sensitivity Pulse Oximeter and Heart-Rate Sensor for Wearable Health*  
**Mã tài liệu:** 19-7740, **Rev 0**, tháng 9/2015 (34 trang)  
**File gốc trong dự án:** `datasheets/MAX30102.PDF`  
**Mục đích:** Tài liệu tham khảo khi thiết kế phần cứng, lập trình ESP32/Arduino và đọc dữ liệu PPG / SpO2 / nhịp tim.

> **Lưu ý y tế:** MAX30102 là module cảm biến quang. Datasheet **không** cung cấp thuật toán lâm sàng đã được chứng nhận. Giá trị SpO2/BPM tính từ ADC chỉ dùng cho đồ án, nghiên cứu, thiết bị wellness — không dùng chẩn đoán y tế trừ khi hệ thống được hiệu chuẩn và chứng nhận riêng.

---

## Mục lục

1. [Tổng quan](#1-tổng-quan)
2. [Đặc điểm nổi bật](#2-đặc-điểm-nổi-bật)
3. [Cấu trúc hệ thống và nguyên lý đo](#3-cấu-trúc-hệ-thống-và-nguyên-lý-đo)
4. [Thông số tuyệt đối (Absolute Maximum Ratings)](#4-thông-số-tuyệt-đối-absolute-maximum-ratings)
5. [Đặc tuyến điện](#5-đặc-tuyến-điện)
6. [Đặc tuyến quang (LED, photodiode, kính)](#6-đặc-tuyến-quang-led-photodiode-kính)
7. [Cảm biến nhiệt độ die](#7-cảm-biến-nhiệt-độ-die)
8. [Giao diện số và thời gian I2C](#8-giao-diện-số-và-thời-gian-i2c)
9. [Đặc tuyến điển hình (TOC)](#9-đặc-tuyến-điển-hình-toc)
10. [Sơ đồ chân (Pinout)](#10-sơ-đồ-chân-pinout)
11. [Mô tả chức năng chi tiết](#11-mô-tả-chức-năng-chi-tiết)
12. [Bản đồ thanh ghi đầy đủ](#12-bản-đồ-thanh-ghi-đầy-đủ)
13. [Ngắt (Interrupt)](#13-ngắt-interrupt)
14. [FIFO — cấu trúc, con trỏ, cách đọc](#14-fifo--cấu-trúc-con-trỏ-cách-đọc)
15. [Cấu hình FIFO (0x08)](#15-cấu-hình-fifo-0x08)
16. [Chế độ hoạt động (0x09)](#16-chế-độ-hoạt-động-0x09)
17. [Cấu hình SpO2 (0x0A)](#17-cấu-hình-spo2-0x0a)
18. [Biên độ xung LED (0x0C–0x10)](#18-biên-độ-xung-led-0x0c0x10)
19. [Chế độ Multi-LED (0x11–0x12)](#19-chế-độ-multi-led-0x110x12)
20. [Nhiệt độ die (0x1F–0x21)](#20-nhiệt-độ-die-0x1f0x21)
21. [Proximity (0x30)](#21-proximity-0x30)
22. [Part ID / Revision](#22-part-id--revision)
23. [Bảng sample rate hợp lệ](#23-bảng-sample-rate-hợp-lệ)
24. [Bù nhiệt bước sóng LED đỏ](#24-bù-nhiệt-bước-sóng-led-đỏ)
25. [Timing đo và thu thập dữ liệu](#25-timing-đo-và-thu-thập-dữ-liệu)
26. [Tuần tự nguồn](#26-tuần-tự-nguồn)
27. [Giao thức I2C chi tiết](#27-giao-thức-i2c-chi-tiết)
28. [Mạch ứng dụng điển hình](#28-mạch-ứng-dụng-điển-hình)
29. [Đóng gói, đặt hàng](#29-đóng-gói-đặt-hàng)
30. [Hướng dẫn thực hành firmware (ESP32 / Arduino)](#30-hướng-dẫn-thực-hành-firmware-esp32--arduino)
31. [Checklist khi làm việc với cảm biến](#31-checklist-khi-làm-việc-với-cảm-biến)
32. [Thuật ngữ và viết tắt](#32-thuật-ngữ-và-viết-tắt)

---

## 1. Tổng quan

MAX30102 là **module tích hợp** đo **oxi máu xung (pulse oximetry, SpO2)** và **nhịp tim (heart rate / PPG)**. Bên trong gồm:

- LED đỏ (~660 nm) và LED hồng ngoại (~880 nm)
- Photodiode
- Thủy tinh/kính phủ (cover glass) tích hợp
- Mạch điện tử nhiễu thấp, **khử ánh sáng môi trường (ALC — Ambient Light Cancellation)**
- ADC sigma-delta 18-bit
- FIFO 32 mẫu
- Giao tiếp **I2C**
- Cảm biến nhiệt độ trên die
- Driver LED lập trình được

Thiết kế nhằm **giảm linh kiện ngoại vi** cho thiết bị đeo (wearable) và điện thoại.

### Nguồn điện

| Rail | Vai trò | Điện áp điển hình | Dải bảo đảm |
| --- | --- | --- | --- |
| **VDD** | Analog / logic IC | 1.8 V | 1.7 – 2.0 V |
| **VLED+** (R_LED+ / IR_LED+) | Anode LED | 3.3 V hoặc 5.0 V (datasheet đo với 5.0 V) | 3.1 – 5.25 V |

Có thể **tắt phần mềm (shutdown)** với dòng chờ rất nhỏ (~0.7 µA typ), nên **không bắt buộc cắt nguồn** khi ngủ.

### Giao tiếp

- I2C tương thích, tối đa **400 kHz**
- Địa chỉ 8-bit: **ghi `0xAE`**, **đọc `0xAF`**
- Địa chỉ 7-bit (Arduino `Wire`): **`0x57`**
- Chân **INT** open-drain, active-low

### Ứng dụng datasheet nêu

- Wearable devices
- Fitness assistant devices

---

## 2. Đặc điểm nổi bật

- Cảm biến nhịp tim **và** pulse oximeter trong giải pháp **phản xạ (LED reflective)**
- Module quang 14 chân rất nhỏ: **5.6 mm × 3.3 mm × 1.55 mm**
- Kính phủ tích hợp, tăng độ bền và hiệu năng quang
- Công suất siêu thấp, phù hợp pin:
  - Sample rate và dòng LED lập trình được để tiết kiệm năng lượng
  - Heart-rate monitor < **1 mW**
  - Dòng shutdown điển hình **0.7 µA** (max 10 µA @ 25 °C)
- Xuất dữ liệu nhanh, sample rate cao (tới 3200 sps, phụ thuộc độ rộng xung)
- Chống artifact chuyển động tốt nhờ SNR cao
- Nhiệt độ hoạt động **-40 °C đến +85 °C**

---

## 3. Cấu trúc hệ thống và nguyên lý đo

### 3.1. Sơ đồ khối (theo datasheet)

Luồng tín hiệu:

1. LED đỏ / IR chiếu vào mô (ngón tay, cổ tay) qua kính phủ
2. Ánh sáng phản xạ + ánh sáng môi trường tới photodiode
3. **ALC** khử thành phần ánh sáng môi trường (Track/Hold)
4. ADC dòng 18-bit
5. Lọc nhiễu số
6. Đưa vào **FIFO**
7. MCU (host) đọc qua I2C; chân INT báo sẵn sàng / FIFO gần đầy

Khối nội: oscillator, ADC nhiệt độ die, LED drivers, I2C, digital filter, data registers.

### 3.2. Nguyên lý PPG / SpO2 (bối cảnh dùng IC)

MAX30102 **không tính BPM hay %SpO2 sẵn**. Nó chỉ trả **chuỗi ADC theo kênh LED**.

- **PPG (photoplethysmogram):** thành phần AC của ánh sáng phản xạ theo nhịp mạch. Nhịp tim lấy từ tần số đỉnh AC (LED đỏ hoặc IR).
- **SpO2:** dựa trên tỷ số điều biến của hai bước sóng (thường R = (ACred/DCred) / (ACir/DCir)), rồi ánh xạ qua đường cong hiệu chuẩn. Bước sóng LED đỏ rất nhạy với nhiệt độ → datasheet khuyến nghị **đọc nhiệt độ die** để bù.

Chế độ phản xạ: LED và photodiode cùng phía mô (khác kiểu truyền qua như kẹp ngón tay y tế).

---

## 4. Thông số tuyệt đối (Absolute Maximum Ratings)

**Vượt các giới hạn này có thể phá hỏng IC vĩnh viễn.**

| Thông số | Giới hạn |
| --- | --- |
| VDD tới GND | −0.3 V đến +2.2 V |
| GND tới PGND | −0.3 V đến +0.3 V |
| X_DRV, VLED+ tới PGND | −0.3 V đến +6.0 V |
| Các chân khác tới GND | −0.3 V đến +6.0 V |
| Thời gian ngắn mạch ngõ ra | Liên tục |
| Dòng vào liên tục bất kỳ chân | ±20 mA |
| ESD HBM | 2.5 kV |
| Latchup immunity | ±250 mA |
| Công suất tiêu tán liên tục (TA = +70 °C), OESIP | 440 mW (derate 5.5 mW/°C trên +70 °C) |
| Nhiệt độ hoạt động | −40 °C đến +85 °C |
| Junction temperature | +90 °C |
| Hàn reflow | +260 °C |
| Lưu kho | −40 °C đến +105 °C |

### Nhiệt trở package OESIP

- θJA = **180 °C/W**
- θJC = **150 °C/W**  
(Đo theo JEDEC JESD51-7, board 4 lớp.)

---

## 5. Đặc tuyến điện

Điều kiện chung trừ khi ghi khác: **VDD = 1.8 V**, **VLED+ = 5.0 V**, **TA = −40 °C đến +85 °C**, typical @ +25 °C.

**Ghi chú kiểm tra (datasheet):**

- Note 2: 100% test @ 25 °C; dải nhiệt bảo đảm bằng characterization / ATE.
- Note 3: RED/IR count bảo đảm bằng characterization + production test.
- Note 4: Quang, I2C timing, cover glass — bảo đảm bằng design/characterization, **không** test 100% lúc xuất xưởng.

### 5.1. Nguồn

| Thông số | Ký hiệu | Min | Typ | Max | Đơn vị | Điều kiện |
| --- | --- | --- | --- | --- | --- | --- |
| Điện áp VDD | VDD | 1.7 | 1.8 | 2.0 | V | Bảo đảm qua dung sai count RED/IR |
| Điện áp LED | VLED+ | 3.1 | 3.3 | 5.25 | V | PSRR của LED driver |
| Dòng VDD (SpO2 + HR) | IDD | | 600 | 1200 | µA | PW = 215 µs, 50 sps |
| Dòng VDD (chỉ IR) | | | 600 | 1200 | µA | PW = 215 µs, 50 sps |
| Dòng shutdown | ISHDN | | 0.7 | 10 | µA | +25 °C, MODE = 0x80 (SHDN) |

> Trên module breakout phổ biến (GY-MAX30102): thường có LDO 1.8 V cho VDD và LED chạy 3.3 V hoặc 5 V. **Không nối VDD trực tiếp 3.3 V** — vượt abs max 2.2 V.

### 5.2. ADC / PPG

| Thông số | Min | Typ | Max | Đơn vị / ghi chú |
| --- | --- | --- | --- | --- |
| Độ phân giải ADC | | 18 | | bit (tối đa; thực tế theo LED_PW) |
| Red ADC count | 55536 | 65536 | 75536 | RED_PA=0x0C, LED_PW=0x01, SPO2_SR=0x05, ADC_RGE=0x00, 25 °C |
| IR ADC count | 55536 | 65536 | 75536 | IR_PA=0x0C, cùng điều kiện |
| Dark current count | | 30 | 128 | RED_PA=IR_PA=0x00, LED_PW=0x03, SR=0x01, RGE=0x02 |
| Dark current | | 0.01 | 0.05 | % full scale |
| ALC dưới nắng trực tiếp 100k lux, ngón tay trên sensor | | 2 counts (Red) / 2 counts (IR) | | ADC_RGE=0x3, LED_PW=0x03, SR=0x01 |
| PSRR theo VDD | | 0.25 | 1 | % FS (1.7–2.0 V) |
| PSRR VDD, 100 mVpp DC–100 kHz | | 10 | | LSB |
| PSRR LED driver 3.6–5.0 V | | 0.05 | 1 | % FS |
| PSRR LED, 100 mVpp | | 10 | | LSB |

ALC có thể khử tới **200 µA** dòng ánh sáng môi trường. Full-scale ADC lập trình **2 µA đến 16 µA** (2048–16384 nA).

### 5.3. Clock và thời gian tích phân ADC

| Thông số | Min | Typ | Max | Đơn vị |
| --- | --- | --- | --- | --- |
| Tần số clock ADC | 10.32 | 10.48 | 10.64 | MHz |

Phần *Detailed Description* ghi sampling ADC **10.24 MHz** — nên hiểu đây là giá trị kiến trúc/nominal; bảng điện dùng 10.48 MHz typical.

**Thời gian tích phân INT theo LED_PW:**

| LED_PW | Thời gian tích phân |
| --- | --- |
| 0x00 | 69 µs |
| 0x01 | 118 µs |
| 0x02 | 215 µs |
| 0x03 | 411 µs |

**Slot timing** (cạnh lên xung Red → cạnh lên xung IR):

| LED_PW | Slot timing |
| --- | --- |
| 0x00 | 427.1 µs |
| 0x01 | 524.7 µs |
| 0x02 | 720.0 µs |
| 0x03 | 1106.6 µs |

---

## 6. Đặc tuyến quang (LED, photodiode, kính)

### 6.1. Cover glass

- Hydrolytic resistance class: **HGB 1** theo DIN ISO 719 (Note 4)

### 6.2. LED hồng ngoại (IR)

Điều kiện: ILED = 20 mA, TA = +25 °C (Note 4)

| Thông số | Min | Typ | Max | Đơn vị |
| --- | --- | --- | --- | --- |
| Bước sóng đỉnh λP | 870 | 880 | 900 | nm |
| FWHM Δλ | | 30 | | nm |
| Điện áp thuận VF | | 1.4 | | V |
| Công suất bức xạ PO | | 6.5 | | mW |

### 6.3. LED đỏ (Red)

Cùng điều kiện 20 mA, +25 °C (Note 4)

| Thông số | Min | Typ | Max | Đơn vị |
| --- | --- | --- | --- | --- |
| Bước sóng đỉnh λP | 650 | 660 | 670 | nm |
| FWHM Δλ | | 20 | | nm |
| Điện áp thuận VF | | 2.1 | | V |
| Công suất bức xạ PO | | 9.8 | | mW |

### 6.4. Photodiode (Note 4)

| Thông số | Giá trị |
| --- | --- |
| Dải phổ (QE > 50%) | 600 – 900 nm |
| Diện tích nhạy quang | 1.36 mm² |
| Kích thước vùng nhạy | 1.38 mm × 0.98 mm |

QE phủ cả Red và IR — phù hợp đo hai kênh bằng một photodiode.

---

## 7. Cảm biến nhiệt độ die

| Thông số | Typ / giới hạn |
| --- | --- |
| Thời gian thu thập ADC nhiệt | 29 ms @ 25 °C |
| Độ chính xác | ±1 °C @ 25 °C |
| TMIN | −40 °C |
| TMAX | +85 °C |
| Độ phân giải nội | **0.0625 °C** |

Dùng để bù **dịch bước sóng LED đỏ** khi tính SpO2. IR ít nhạy bước sóng hơn.

---

## 8. Giao diện số và thời gian I2C

### 8.1. SCL, SDA (input)

| Thông số | Điều kiện | Giá trị |
| --- | --- | --- |
| VIH | VDD = 2 V | 0.7 × VDD |
| VIL | VDD = 2 V | 0.3 × VDD |
| Hysteresis VH | | 0.2 V typ |
| Dòng rò IIN | VIN = GND hoặc VDD, tĩnh | ±0.05 µA typ, ±1 µA max |

### 8.2. SDA, INT (output)

- VOL = 0.2 V max khi ISINK = 6 mA (open-drain)

### 8.3. I2C timing (Note 4)

| Thông số | Ký hiệu | Min | Max | Đơn vị |
| --- | --- | --- | --- | --- |
| Địa chỉ ghi | | | 0xAE | hex |
| Địa chỉ đọc | | | 0xAF | hex |
| fSCL | | 0 | 400 | kHz |
| Bus free STOP→START | tBUF | 1.3 | | µs |
| Hold START (kể cả repeated) | tHD;STA | 0.6 | | µs |
| SCL low | tLOW | 1.3 | | µs |
| SCL high | tHIGH | 0.6 | | µs |
| Setup repeated START | tSU;STA | 0.6 | | µs |
| Data hold | tHD;DAT | 0 | 900 | ns |
| Data setup | tSU;DAT | 100 | | ns |
| Setup STOP | tSU;STO | 0.6 | | µs |
| Spike bị triệt | tSP | 0 | 50 | ns |
| Điện dung bus CB | | | 400 | pF |
| Rise time nhận | tR | 20 + 0.1CB | 300 | ns |
| Fall time nhận | tRF | 20 + 0.1CB | 300 | ns |
| Fall time SDA phát | tTF | | 300 | ns |

Pull-up SDA/SCL: datasheet gợi ý **thường > 500 Ω**. INT: **4.7 kΩ** (không sink dòng lớn).

---

## 9. Đặc tuyến điển hình (TOC)

Các đồ thị (tóm tắt ý nghĩa khi thiết kế):

| Ký hiệu | Nội dung | Ý nghĩa thực tế |
| --- | --- | --- |
| toc01 | Headroom nguồn LED đỏ vs dòng | VLED+ phải đủ cao khi ILED 20–50 mA |
| toc02 | Headroom LED IR | IR VF thấp hơn Red → dễ hơn về headroom |
| toc03 | IDD vs VDD | Shutdown vs normal; dòng analog theo VDD |
| toc04 | DC counts vs khoảng cách (tấm styrene trắng) | Tín hiệu giảm mạnh khi xa sensor (mm) |
| toc05 | ISHDN(VDD) vs nhiệt | Dòng ngủ tăng theo nhiệt và VDD |
| toc06 | Dòng shutdown VLED vs nhiệt | VLED=4.75 V và 5.25 V |
| toc08 / toc09 | Phổ LED đỏ / IR @ +30 °C | Đỉnh ~660 nm / ~880 nm |
| toc10 / toc11 | λP vs nhiệt độ (Red / IR) | Red dịch rõ theo nhiệt — cần bù SpO2 |
| toc12 / toc13 | IF vs VF Red / IR @ 25 °C | Thiết kế nguồn LED |
| toc14 | Passband ALC theo PW | PW dài → lọc ALC khác; nhiễu ánh sáng nhấp nháy |
| toc15 | QE photodiode vs bước sóng | Nhạy 600–900 nm |

**Khoảng cách quang:** đồ thị DC counts giảm rất nhanh theo mm. Ngón tay / da phải **áp sát kính**, giảm khe hở và ánh sáng lọt.

---

## 10. Sơ đồ chân (Pinout)

Package nhìn từ trên (optical window ở giữa):

```text
        N.C. 1                 14 N.C.
         SCL 2                 13 INT
         SDA 3                 12 GND
        PGND 4                 11 VDD
       R_DRV 5                 10 VLED+
      IR_DRV 6                  9 VLED+
        N.C. 7                  8 N.C.
```

### Bảng chân

| Pin | Tên | Chức năng |
| --- | --- | --- |
| 1, 7, 8, 14 | **N.C.** | Không kết nối điện. **Vẫn hàn pad PCB** để cơ khí vững. |
| 2 | **SCL** | Clock I2C (input) |
| 3 | **SDA** | Data I2C hai chiều, open-drain |
| 4 | **PGND** | Ground **driver LED** |
| 5 | **R_DRV** | Cathode / driver LED đỏ |
| 6 | **IR_DRV** | Cathode / driver LED IR |
| 9, 10 | **VLED+** | Nguồn anode LED (hai pin cùng net). Bypass về **PGND**. |
| 11 | **VDD** | Nguồn analog 1.8 V. Bypass về **GND**. |
| 12 | **GND** | Analog ground |
| 13 | **INT** | Ngắt active-low, open-drain. Pull-up ra điện áp I/O MCU (tới +5 V). |

**Quy tắc layout:**

- Tách **GND (analog)** và **PGND (LED)** đúng datasheet; trên breakout thương mại thường đã nối tại một điểm.
- Tụ bypass **sát chân IC**.
- Điện trở + cảm kháng đường nguồn LED **≪ 1 Ω**.
- Tối thiểu **1 µF** trên VLED+ (mạch điển hình dùng **10 µF**).

Trên nhiều breakout, R_DRV / IR_DRV đã nối sẵn trong module — firmware chỉ cần I2C.

---

## 11. Mô tả chức năng chi tiết

### 11.1. Khái quát

- Điều chỉnh gần như toàn bộ qua thanh ghi.
- FIFO sâu **32 mẫu**, MCU không cần đọc từng sample liên tục — phù hợp bus I2C dùng chung.

### 11.2. Khối SpO2

Gồm ALC, ADC sigma-delta continuous-time, và bộ lọc discrete-time độc quyền.

- Full-scale ADC: **2 µA – 16 µA**
- ALC khử tới **200 µA** ambient
- ADC oversampling 18-bit, clock ~10.24–10.48 MHz
- Data rate lập trình **50 – 3200 sps**

### 11.3. LED driver

- Dòng **0 – 50 mA** (nếu VLED+ đủ)
- Độ rộng xung **69 – 411 µs**
- Cùng PW cho Red và IR
- PWM/xung duty thấp → tiết kiệm pin nhưng gây ripple VLED+ → cần bypass tốt, nếu không LED “nhấp” thành nhiễu quang

### 11.4. Proximity (tiết kiệm pin + giảm ánh sáng nhìn thấy)

Khi ghi MODE để vào SpO2/HR:

1. IR chạy **proximity** với dòng **PILOT_PA**
2. Nếu count IR vượt **PROX_INT_THRESH** → tự chuyển mode đo bình thường
3. Host nhận **PROX_INT** → bắt đầu thuật toán
4. Muốn vào lại proximity: **ghi lại MODE** (kể cả cùng giá trị)
5. Tắt proximity: **PROX_INT_EN = 0** → vào SpO2/HR ngay

Vào/ra proximity (khi PROX_INT_EN = 1) **xóa FIFO** (WR_PTR = RD_PTR).

### 11.5. Chế độ HR

- Bảng MODE: HR (`010`) chỉ **LED đỏ**.
- Phần ứng dụng: HR **không bắt buộc** nhiệt độ; có thể chọn Red **hoặc** IR (Multi-LED).
- Figure 5 minh họa xung IR — coi đây là ví dụ timing, không mâu thuẫn việc HR mặc định dùng Red.

---

## 12. Bản đồ thanh ghi đầy đủ

Quy ước: địa chỉ hex, POR = Power-On Reset.

### 12.1. STATUS

| Addr | Tên | Bit7 | Bit6 | Bit5 | Bit4 | Bit3 | Bit2 | Bit1 | Bit0 | POR | R/W |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0x00 | Interrupt Status 1 | A_FULL | PPG_RDY | ALC_OVF | PROX_INT | — | — | — | PWR_RDY | 0x00 | R |
| 0x01 | Interrupt Status 2 | — | — | — | — | — | — | DIE_TEMP_RDY | — | 0x00 | R |
| 0x02 | Interrupt Enable 1 | A_FULL_EN | PPG_RDY_EN | ALC_OVF_EN | PROX_INT_EN | — | — | — | — | 0x00 | R/W |
| 0x03 | Interrupt Enable 2 | — | — | — | — | — | — | DIE_TEMP_RDY_EN | — | 0x00 | R/W |

Bit không dùng: giữ **0**.

### 12.2. FIFO

| Addr | Tên | Bits | POR | R/W |
| --- | --- | --- | --- |
| 0x04 | FIFO Write Pointer | FIFO_WR_PTR[4:0] | 0x00 | R/W |
| 0x05 | Overflow Counter | OVF_COUNTER[4:0] | 0x00 | R/W |
| 0x06 | FIFO Read Pointer | FIFO_RD_PTR[4:0] | 0x00 | R/W |
| 0x07 | FIFO Data | FIFO_DATA[7:0] | 0x00 | R/W |

Thực tế vận hành: **nên chỉ ghi FIFO_RD_PTR** (khi cần đọc lại). WR_PTR và DATA do IC cập nhật.

Ghi WR_PTR được khi MODE = `010`, `011`, hoặc `111`.

### 12.3. CONFIGURATION

| Addr | Tên | Cấu trúc bit | POR | R/W |
| --- | --- | --- | --- | --- |
| 0x08 | FIFO Configuration | SMP_AVE[2:0] \| FIFO_ROLLOVER_EN \| FIFO_A_FULL[3:0] | 0x00 | R/W |
| 0x09 | Mode Configuration | SHDN \| RESET \| — \| — \| — \| MODE[2:0] | 0x00 | R/W |
| 0x0A | SpO2 Configuration | 0 (reserved) \| SPO2_ADC_RGE[1:0] \| SPO2_SR[2:0] \| LED_PW[1:0] | 0x00 | R/W |
| 0x0B | RESERVED | | 0x00 | R/W |
| 0x0C | LED1 Pulse Amplitude | LED1_PA[7:0] (Red) | 0x00 | R/W |
| 0x0D | LED2 Pulse Amplitude | LED2_PA[7:0] (IR) | 0x00 | R/W |
| 0x0E | RESERVED | | 0x00 | R/W |
| 0x0F | RESERVED | | 0x00 | R/W |
| 0x10 | PILOT_PA | PILOT_PA[7:0] | 0x00 | R/W |
| 0x11 | Multi-LED Control 1 | SLOT2[2:0] \| SLOT1[2:0] | 0x00 | R/W |
| 0x12 | Multi-LED Control 2 | SLOT4[2:0] \| SLOT3[2:0] | 0x00 | R/W |

### 12.4. RESERVED / TEMP / PROX / ID

| Addr | Tên | POR | R/W | Ghi chú |
| --- | --- | --- | --- | --- |
| 0x13–0x17 | RESERVED | 0xFF | R/W | Không dùng |
| 0x18–0x1E | RESERVED | 0x00 | R | |
| 0x1F | Die Temp Integer TINT[7:0] | 0x00 | R | 2’s complement, 1 °C/bit |
| 0x20 | Die Temp Fraction TFRAC[3:0] | 0x00 | R | 0.0625 °C/bit |
| 0x21 | Die Temperature Config | TEMP_EN (self-clear) | 0x00 | Datasheet bảng ghi **R**; bit TEMP_EN vẫn **ghi 1** để kích đo |
| 0x22–0x2F | RESERVED | 0x00 | R/W | |
| 0x30 | PROX_INT_THRESH[7:0] | 0x00 | R/W | 8 bit cao của ADC IR |
| 0xFE | Revision ID | 0xXX | R | Liên hệ Maxim theo lô |
| 0xFF | Part ID | **0x15** | R | Dùng verify I2C |

Đọc vượt 0xFF: pointer **không** quay về 0x00; dữ liệu không có nghĩa.

---

## 13. Ngắt (Interrupt)

INT kéo **LOW** đến khi xóa. Open-drain, pull-up ~**4.7 kΩ** tới VDDIO MCU (tới 5 V).

### 13.1. Cách xóa

- Đọc Interrupt Status tương ứng, **hoặc**
- Đọc thanh ghi nguồn gốc ngắt (ví dụ FIFO_DATA xóa PPG_RDY; TFRAC xóa DIE_TEMP_RDY)

Đọc status thường **xóa mọi bit** trong thanh ghi status đó và nhả chân INT về HIGH.

### 13.2. Các cờ — Interrupt Status 1 (0x00)

| Bit | Tên | Ý nghĩa | Cách xóa |
| --- | --- | --- | --- |
| A_FULL | FIFO almost full | Số ô trống FIFO ≤ ngưỡng FIFO_A_FULL | Đọc 0x00 |
| PPG_RDY | Mẫu mới trong FIFO | Mỗi sample mới (SpO2/HR) | Đọc 0x00 hoặc FIFO_DATA |
| ALC_OVF | ALC bão hòa | Ánh sáng môi trường quá mạnh, ADC bị ảnh hưởng | Đọc 0x00 |
| PROX_INT | Đã chạm ngưỡng proximity | Đã chuyển sang đo SpO2/HR | Đọc 0x00 |
| PWR_RDY | Power ready | VDD vượt UVLO sau power-up / brownout | Đọc 0x00 |

**PWR_RDY không tắt được qua Enable.** Brownout reset logic; mặc định mọi interrupt enable = 0, hệ thống **phải biết** dữ liệu đã mất.

**RESET bit không tạo PWR_RDY.**

### 13.3. Interrupt Status 2 (0x01)

| Bit | Tên | Ý nghĩa | Cách xóa |
| --- | --- | --- | --- |
| DIE_TEMP_RDY | Đo nhiệt xong | Đọc 0x1F/0x20 | Đọc 0x01 hoặc **0x20 (TFRAC)** |

### 13.4. Interrupt Enable (0x02–0x03)

Bật từng nguồn (trừ PWR_RDY). Khuyến nghị đồ án:

- Bật **A_FULL_EN**, đọc burst FIFO
- Có thể **mask PPG_RDY** (datasheet timing SpO2/HR cũng mask PPG_RDY) để giảm ISR
- Bật **ALC_OVF_EN** để biết đo không đáng tin (nắng, đèn mạnh)
- SpO2: bật **DIE_TEMP_RDY_EN** khi đo nhiệt định kỳ
- Wearable: **PROX_INT_EN** để ngủ khi không có ngón tay

---

## 14. FIFO — cấu trúc, con trỏ, cách đọc

### 14.1. Thông số

- Sâu **32 sample**
- Mỗi **kênh** = **3 byte** (18-bit left-justified)
- SpO2 (Red + IR): **6 byte/sample** → tối đa 32 × 6 = **192 byte**
- HR một kênh: **3 byte/sample**
- Multi-LED: 3 byte × số slot đang bật (tối đa 4 slot = 12 byte/sample)

### 14.2. Con trỏ

| Thanh ghi | Vai trò |
| --- | --- |
| FIFO_WR_PTR | Vị trí mẫu **tiếp theo sẽ ghi** |
| FIFO_RD_PTR | Vị trí mẫu **host sẽ đọc** |
| OVF_COUNTER | Số mẫu **mất** khi FIFO đầy; bão hòa **0x0F**; reset 0 khi pop một sample hoàn chỉnh |

Số mẫu có sẵn (nhớ **modulo 32**):

```text
NUM_AVAILABLE = (FIFO_WR_PTR - FIFO_RD_PTR) & 0x1F
```

Nếu rollover tắt và FIFO đầy, WR không tăng — cần kết hợp OVF_COUNTER.

Khi **rollover tắt (mặc định)**: FIFO đầy thì **không nhận mẫu mới** (mất dữ liệu, OVF tăng).  
Khi **FIFO_ROLLOVER_EN = 1**: ghi đè mẫu cũ, WR quấn về 0.

### 14.3. Left-justified

MSB luôn ở bit 17, không phụ thuộc độ phân giải.

| Độ phân giải | Bit có nghĩa | Bit thấp bỏ |
| --- | --- | --- |
| 18-bit | [17:0] | — |
| 17-bit | [17:1] | [0] |
| 16-bit | [17:2] | [1:0] |
| 15-bit | [17:3] | [2:0] |

3 byte kênh:

| Byte | Bit ADC |
| --- | --- |
| Byte 1 | [17:16] (2 bit thấp của byte; 6 bit cao không dùng — FIFO_DATA[23:18] unused) |
| Byte 2 | [15:8] |
| Byte 3 | [7:0] |

Firmware:

```text
value_18bit = ((b1 & 0x03) << 16) | (b2 << 8) | b3
```

### 14.4. Thứ tự kênh (SpO2)

Theo Figure 2, mỗi sample:

1. **IR** (3 byte)
2. **RED** (3 byte)

Pseudo-code datasheet (trang 16) lại gán LED1 rồi LED2. LED1 = Red, LED2 = IR theo bảng Multi-LED. **Thứ tự FIFO vs tên LED1/LED2 dễ nhầm.**

Thực hành: in cả hai kênh, đối chiếu — kênh IR thường DC cao hơn trên da; hoặc tắt lần lượt LED_PA. Nhiều thư viện SparkFun/Maxim đọc **Red rồi IR**. Khi port driver, **verify bằng thực nghiệm** (tắt LED1_PA hoặc LED2_PA).

### 14.5. Burst đọc — ngoại lệ địa chỉ 0x07

- Thanh ghi thường: auto-increment địa chỉ
- **0x07 FIFO_DATA: không tăng địa chỉ**, mỗi byte tiếp theo là byte FIFO tiếp; **FIFO_RD_PTR tăng sau byte đầu mỗi sample** (theo datasheet)
- Đọc sample SpO2 = 6 byte trong **một** transaction I2C

### 14.6. Khởi tạo FIFO

Trước khi vào SpO2/HR, ghi **0x00** vào:

- FIFO_WR_PTR (0x04)
- OVF_COUNTER (0x05)
- FIFO_RD_PTR (0x06)

Power-cycle / UVLO cũng xóa pointer.

### 14.7. Pseudo-code đọc FIFO (theo datasheet)

```text
// 1) Đọc WR_PTR
START; ADDR_W; 0x04; RESTART; ADDR_R; read WR_PTR; STOP

NUM_AVAILABLE = (WR_PTR - RD_PTR)  // modulo 32
NUM_TO_READ <= NUM_AVAILABLE

// 2) Burst FIFO_DATA
START; ADDR_W; 0x07; RESTART; ADDR_R
for i in 0 .. NUM_TO_READ-1:
    LED1_b2 = read()   // [23:16]
    LED1_b1 = read()
    LED1_b0 = read()
    LED2_b2 = read()
    LED2_b1 = read()
    LED2_b0 = read()
STOP

// 3) Nếu transaction 2 lỗi: ghi lại FIFO_RD_PTR để đọc lại
```

WR_PTR nên đọc lại / kết hợp RD_PTR để tránh race khi sample mới tới giữa chừng.

---

## 15. Cấu hình FIFO (0x08)

### 15.1. SMP_AVE[2:0] — trung bình mẫu trên chip

Giảm throughput, tăng SNR, giảm data rate hiệu dụng.

| SMP_AVE | Số mẫu trung bình |
| --- | --- |
| 000 | 1 (không TB) |
| 001 | 2 |
| 010 | 4 |
| 011 | 8 |
| 100 | 16 |
| 101, 110, 111 | 32 |

Data rate FIFO hiệu dụng ≈ SPO2_SR / N_avg.

### 15.2. FIFO_ROLLOVER_EN

- 0: đầy thì dừng ghi
- 1: ghi đè vòng

Đo liên tục real-time: thường **rollover = 1** + đọc kịp A_FULL.  
Không được mất mẫu (log): rollover = 0, đọc đủ nhanh.

### 15.3. FIFO_A_FULL[3:0]

Ngắt khi số **ô trống** = giá trị này.

| FIFO_A_FULL | Ô trống khi ngắt | Mẫu chưa đọc |
| --- | --- | --- |
| 0x0 | 0 | 32 (đầy) |
| 0x1 | 1 | 31 |
| … | … | … |
| 0xF | 15 | 17 |

Ví dụ 0xF: ngắt khi còn 17 mẫu chưa đọc — host còn thời gian trước khi tràn.

---

## 16. Chế độ hoạt động (0x09)

```text
Bit7 SHDN | Bit6 RESET | Bit5–3 unused | Bit2–0 MODE
```

### 16.1. SHDN

- 1: power-save
- Giữ giá trị thanh ghi, I2C vẫn đọc/ghi
- Mọi interrupt clear về 0
- Dòng ~0.7 µA typ

### 16.2. RESET

- 1: reset mọi config/threshold/data về POR
- Tự xóa về 0 khi xong
- **Không** phát PWR_RDY

Đợi RESET tự về 0 trước khi cấu hình tiếp.

### 16.3. MODE[2:0]

| MODE | Chế độ | LED |
| --- | --- | --- |
| 000, 001 | Không dùng | — |
| **010** | Heart Rate | **Chỉ Red** |
| **011** | SpO2 | Red + IR |
| 100–110 | Không dùng | — |
| **111** | Multi-LED | Red và/hoặc IR theo SLOT |

Đổi MODE **không** xóa thanh ghi khác, **không** xóa data cũ trong FIFO — firmware nên **tự xóa FIFO**.

---

## 17. Cấu hình SpO2 (0x0A)

```text
Bit7 = 0 (reserved)
SPO2_ADC_RGE[1:0] | SPO2_SR[2:0] | LED_PW[1:0]
```

### 17.1. ADC range (18-bit)

| RGE | LSB | Full scale |
| --- | --- | --- |
| 00 | 7.81 pA | **2048 nA** |
| 01 | 15.63 pA | 4096 nA |
| 10 | 31.25 pA | 8192 nA |
| 11 | 62.5 pA | **16384 nA** |

Da tối / ngón xa: tăng LED hoặc range. Sát da, LED mạnh: range lớn tránh bão hòa. ALC_OVF / count gần 2^18 là dấu hiệu bão hòa.

### 17.2. Sample rate SPO2_SR

| SR | sps |
| --- | --- |
| 000 | 50 |
| 001 | 100 |
| 010 | 200 |
| 011 | 400 |
| 100 | 800 |
| 101 | 1000 |
| 110 | 1600 |
| 111 | 3200 |

Một sample SpO2 = **1 xung/conversion IR + 1 Red**.

Nếu SR quá cao so với LED_PW, IC **tự kẹp** xuống SR cao nhất còn hợp lệ.

Đồ án nhịp tim/SpO2 ngón tay: thường **100 sps**, PW **411 µs** (18-bit) nếu timing cho phép.

### 17.3. LED_PW và độ phân giải ADC

| LED_PW | PW (µs) | ADC bits |
| --- | --- | --- |
| 00 | 69 (68.95) | 15 |
| 01 | 118 (117.78) | 16 |
| 10 | 215 (215.44) | 17 |
| 11 | 411 (410.75) | 18 |

PW dài: SNR tốt hơn, tốn pin, SR max thấp hơn.

---

## 18. Biên độ xung LED (0x0C–0x10)

Dòng typical (trim từng con **lệch khá rộng**):

| Thanh ghi | ~ mA |
| --- | --- |
| 0x00 | 0.0 |
| 0x01 | 0.2 |
| 0x02 | 0.4 |
| 0x0F | 3.1 |
| 0x1F | 6.4 |
| 0x3F | 12.5 |
| 0x7F | 25.4 |
| 0xFF | 50.0 |

Nội suy gần: **~0.2 mA × mã thanh ghi** (0x00–0xFF).

| Thanh ghi | LED |
| --- | --- |
| 0x0C LED1_PA | Red |
| 0x0D LED2_PA | IR |
| 0x10 PILOT_PA | Proximity **và** slot Multi-LED dùng PILOT |

Điều chỉnh thực tế: tăng dần đến khi DC PPG nằm giữa thang (không clip 0 / full-scale), AC còn đủ nhìn thấy đập mạch.

---

## 19. Chế độ Multi-LED (0x11–0x12)

MODE = `111`. Tới **4 time slot**/sample.

Bật slot **theo thứ tự**: không tắt SLOT1 nếu SLOT2 còn bật.

Mỗi slot active → 3 byte FIFO. SLOT1+SLOT2 → 6 byte/sample.

| SLOTx[2:0] | LED | Dòng |
| --- | --- | --- |
| 000 | Tắt | — |
| 001 | LED1 Red | LED1_PA |
| 010 | LED2 IR | LED2_PA |
| 011 | Tắt | — |
| 100 | Tắt | — |
| 101 | LED1 Red | **PILOT_PA** |
| 110 | LED2 IR | **PILOT_PA** |

Dùng để xen kẽ công suất khác nhau, hoặc HR bằng IR thay vì Red mặc định.

---

## 20. Nhiệt độ die (0x1F–0x21)

```text
TMEASURED = TINTEGER + TFRACTION
```

- **TINT (0x1F):** 2’s complement, 1 °C/bit  

| Hex | °C |
| --- | --- |
| 0x00 | 0 |
| 0x01 | +1 |
| 0x7F | +127 |
| 0x80 | −128 |
| 0xFF | −1 |

- **TFRAC (0x20):** 4 bit, bước **0.0625 °C**. Luôn **cộng dương**, kể cả khi integer âm:  
  ví dụ −128 + 0.5 = **−127.5 °C**

- **TEMP_EN (0x21):** ghi 1 → một lần đo; tự xóa khi xong (trong IR/SpO2 mode). Thời gian ~**29 ms**.

SpO2: đo nhiệt **1 lần/giây hoặc vài giây**, không cần mỗi sample.

---

## 21. Proximity (0x30)

`PROX_INT_THRESH` = **8 bit cao** của ADC IR.

Ví dụ:

- `0x01` → ADC 17-bit ≥ **1023** là kích  
- `0xFF` → chỉ **bão hòa ADC** mới kích

Hiệu chỉnh theo module, khe hở, PILOT_PA.

---

## 22. Part ID / Revision

| Addr | Giá trị | Dùng |
| --- | --- | --- |
| 0xFF PART_ID | **0x15** | Probe I2C thành công |
| 0xFE REV_ID | tùy silicon | Phân biệt revision |

Không đọc được 0x15: sai địa chỉ, wiring, VDD chưa 1.8 V, hoặc module hỏng.

---

## 23. Bảng sample rate hợp lệ

PW quyết định SR tối đa.

### Bảng 11 — SpO2 (Red + IR mỗi sample)

| SR (sps) | 69 µs (15b) | 118 µs (16b) | 215 µs (17b) | 411 µs (18b) |
| --- | --- | --- | --- | --- |
| 50 | ✓ | ✓ | ✓ | ✓ |
| 100 | ✓ | ✓ | ✓ | ✓ |
| 200 | ✓ | ✓ | ✓ | ✓ |
| 400 | ✓ | ✓ | ✓ | ✓ |
| 800 | ✓ | ✓ | ✓ | |
| 1000 | ✓ | ✓ | | |
| 1600 | ✓ | | | |
| 3200 | ✓ | | | |

### Bảng 12 — HR (một LED/sample → timing thoáng hơn)

| SR (sps) | 69 µs | 118 µs | 215 µs | 411 µs |
| --- | --- | --- | --- | --- |
| 50–400 | ✓ | ✓ | ✓ | ✓ |
| 800 | ✓ | ✓ | ✓ | ✓ |
| 1000 | ✓ | ✓ | ✓ | ✓ |
| 1600 | ✓ | ✓ | ✓ | |
| 3200 | ✓ | ✓ | | |

SpO2 **18-bit (411 µs)** chỉ tới **400 sps**.  
HR 18-bit tới **1000 sps**.

---

## 24. Bù nhiệt bước sóng LED đỏ

Die LED nóng rất nhanh (chục µs) ngay trong xung ngắn. Ước nhiệt LED:

```text
T_LED ≈ T_die_sensor + ΔT(dòng, duty)
```

### **Bảng 13 — ΔT LED đỏ**

| Dòng (mã) | Duty (% PW / sample time) | ΔT ước lượng |
| --- | --- | --- |
| 0001 (~0.2 mA) | 8% | 0.1 °C |
| 1111 (~50 mA)* | 8% | 2 °C |
| 0.2 mA | 16% | 0.3 °C |
| 50 mA | 16% | 4 °C |
| 0.2 mA | 32% | 0.6 °C |
| 50 mA | 32% | 8 °C |

\*Datasheet ghi `1111 (50 mA)` — 4 bit minh họa; thanh ghi thực 8 bit, **0xFF ≈ 50 mA**.

Cột duty 8/16/32% tương ứng PW so với chu kỳ sample (ví dụ 411 µs @ 100 sps ≈ 4.1% một LED; hai LED SpO2 cao hơn).

Đường cong λ vs T nằm ở TOC (toc10), không tabulate nm/°C trong bảng chữ.

---

## 25. Timing đo và thu thập dữ liệu

### 25.1. Slot Multi-LED / hai kênh (Bảng 14)

| PW (µs) | Khoảng giữa xung (slot) (µs) | Cạnh lên → cạnh lên (µs) |
| --- | --- | --- |
| 69 | 358 | 427 |
| 118 | 407 | 525 |
| 215 | 505 | 720 |
| 411 | 696 | 1107 |

**Figure 3 — ví dụ SpO2 1 kHz, PW 69 µs:** Red on 69 µs, off 931 µs; IR tương tự; lệch pha 358 µs.

1 kHz + 69 µs khớp Bảng 11; **không** dùng 411 µs ở 1 kHz SpO2.

### 25.2. Chuỗi sự kiện SpO2 (Figure 4 / Table 15)

| Event | Việc | Chi tiết |
| --- | --- | --- |
| 1 | Vào SpO2 + đo nhiệt | MODE = `011` (0x03), set TEMP_EN, **mask PPG_RDY** |
| 2 | Nhiệt xong | DIE_TEMP_RDY, ~29 ms |
| 3 | Đọc nhiệt, xóa ngắt | |
| 4 | FIFO almost full | Ngắt A_FULL |
| 5 | Đọc FIFO, xóa ngắt | Cửa sổ datasheet ~15–300 ms tùy SR và ngưỡng |
| 6 | Sample mới ghi vào vị trí RD mới | |

LED luân phiên IR–RED theo từng sample.

### 25.3. Chuỗi HR (Figure 5 / Table 16)

| Event | Việc |
| --- | --- |
| 1 | MODE = `010` (0x02), mask PPG_RDY |
| 2 | A_FULL (ví dụ còn 1 ô trống) |
| 3 | Đọc FIFO |
| 4 | Sample mới |

Không bắt buộc nhiệt. Figure minh họa chuỗi xung IR.

---

## 26. Tuần tự nguồn

### Power-up (khuyến nghị)

1. **Bật VDD (1.8 V) trước**
2. Sau đó **VLED+** (3.3–5 V)
3. INT và I2C **được phép pull-up sẵn** kể cả khi IC chưa có nguồn
4. Khi VDD > UVLO → **PWR_RDY**, INT low
5. Đọc 0x00 để xóa ngắt, bắt đầu cấu hình

### Power-down

IC **chịu mọi thứ tự** tắt nguồn.

Brownout VDD: dữ liệu/config về trạng thái reset; PWR_RDY báo host.

---

## 27. Giao thức I2C chi tiết

- Tương thích I2C / SMBus, 2 dây SDA + SCL
- Tới **400 kHz**
- Master tạo SCL, START/STOP
- Mỗi byte 8 bit + ACK
- SDA open-drain; SCL input (kéo lên nếu master open-drain / đa master)
- Điện trở series SDA/SCL: tùy chọn (chống spike, crosstalk)

### Bit / START / STOP

- SDA **ổn định khi SCL high**; đổi SDA khi SCL high = START/STOP
- START: SDA high→low, SCL high
- STOP: SDA low→high, SCL high
- REPEATED START: START mới, không STOP
- Không STOP trong **cùng xung SCL high** với START

### Slave ID

Cố định `0b1010111` (7-bit **0x57**):

| B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 | Write | Read |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 0 | 1 | 0 | 1 | 1 | 1 | R/W | **0xAE** | **0xAF** |

Sai ID → IC ngắt kết nối SCL ngay (tiết kiệm).

R/W = 0 ghi, 1 đọc. ACK: slave kéo SDA low xung clock 9.

### Ghi (Figure 9)

```text
START | 0xAE | ACK | REG_ADDR | ACK | DATA0 | ACK | [DATA1 | ACK ...] | STOP
```

Pointer tăng dần → ghi liên tiếp cả dải thanh ghi.

### Đọc (Figure 10–11)

```text
START | 0xAE | ACK | REG_ADDR | ACK
RESTART | 0xAF | ACK | DATA | (ACK master nếu còn byte) ... | NACK | STOP
```

Ngoại lệ **FIFO_DATA**: pointer không tăng; byte sau vẫn từ FIFO.

---

## 28. Mạch ứng dụng điển hình

Theo trang 31 datasheet:

| Thành phần | Gợi ý |
| --- | --- |
| VDD | **+1.8 V**, tụ **4.7 µF**, ngân dòng ~20 mA |
| VLED+ | **+5.0 V**, tụ **10 µF**, đỉnh tới ~200 mA (xung LED) |
| SDA/SCL | Pull-up **1 kΩ** tới VDDIO host (hình datasheet) |
| INT | Open-drain về host |
| R_DRV, IR_DRV | Trong module; host **không nối** nếu đã tích hợp |
| Host | MCU I2C + GPIO INT |

Khuyến nghị nguồn LED (mục Power Considerations):

- R, L đường nguồn **≪ 1 Ω**
- ≥ **1 µF** sát chân, ground plane tốt
- Tránh ripple VLED+ biến thành nhiễu quang

**ESP32 3.3 V:**

- VDD IC vẫn **1.8 V** (LDO trên breakout)
- I2C/INT pull-up **3.3 V** — datasheet cho phép INT tới 5 V, input digital theo VDD 1.8 V; **mức I2C phụ thuộc module** (nhiều breakout có MOSFET level-shift). Đo trước khi nối.
- VLED+ thường 3.3 V từ ESP32: nằm trong 3.1–5.25 V nhưng headroom LED đỏ 50 mA kém hơn 5 V — nên giới hạn LED_PA vừa phải.

---

## 29. Đóng gói, đặt hàng

| Mục | Giá trị |
| --- | --- |
| Part | **MAX30102EFD+T** |
| Temp | −40 °C đến +85 °C |
| Package | 14-lead **OESIP**, pitch 0.8 mm |
| RoHS | `+` = Pb-free / RoHS |
| T | Tape and reel |
| Package code | F143A5MK+1 |
| Outline | 21-1048 |
| Land pattern | 90-0602 |

Kích thước module: **5.6 × 3.3 × 1.55 mm**.

Revision history: Rev 0, 9/15 — initial release.

---

## 30. Hướng dẫn thực hành firmware (ESP32 / Arduino)

Áp dụng đồ án PlatformIO `esp32doit-devkit-v1`.

### 30.1. Probe cảm biến

```text
I2C begin
Đọc PART_ID (0xFF) == 0x15
Đọc REV_ID (0xFE) để log
```

### 30.2. Init tối thiểu (SpO2 + FIFO)

```text
1. Ghi RESET (0x09 bit6 = 1), poll đến khi bit = 0
2. Xóa FIFO: 0x04, 0x05, 0x06 = 0
3. FIFO config 0x08:
   - SMP_AVE = 4 (010)  // tùy
   - ROLLOVER_EN = 1
   - FIFO_A_FULL = 0xF  // ngắt khi còn 17 mẫu
4. SpO2 config 0x0A:
   - ADC_RGE = 00 hoặc 01 (2048–4096 nA) lúc bắt đầu
   - SR = 100 sps (001)
   - PW = 411 µs / 18-bit (11)  // hợp lệ với 100 sps SpO2
5. LED1_PA, LED2_PA: bắt đầu ~0x1F–0x3F, chỉnh theo count
6. Enable: A_FULL_EN (+ ALC_OVF, TEMP nếu cần); mask PPG_RDY
7. MODE = 011 (SpO2)
8. (Tùy chọn) TEMP_EN = 1, đọc khi DIE_TEMP_RDY
```

HR only: MODE = `010`, chỉ LED1_PA.

### 30.3. Vòng thu thập

- ISR INT (falling) → cờ software  
- Loop: đọc WR_PTR, tính số mẫu, burst 0x07  
- Ghép 18-bit, tách 2 kênh  
- ALC_OVF → đánh dấu sample không tin cậy  
- Lọc DC (moving average / DC tracker) → AC PPG  
- BPM: peak/zero-crossing hoặc FFT trên AC  
- SpO2: ratio-of-ratios + hệ số hiệu chuẩn (cần đo thực, datasheet không cho bảng %)

### 30.4. Chất lượng tín hiệu

| Hiện tượng | Hướng xử lý |
| --- | --- |
| Count ~0 | Tăng LED, kiểm tra áp ngón, VLED+, wiring |
| Count bão hòa (~262143) | Giảm LED hoặc tăng ADC_RGE |
| ALC_OVF | Che sáng, giảm ambient, kiểm tra áp sát |
| PPG phẳng | Áp lực ngón, giảm cử động, tăng N_avg, PW dài hơn |
| FIFO overflow (OVF>0) | Đọc nhanh hơn, giảm SR, tăng A_FULL sớm, I2C 400 kHz |
| I2C NACK | 0x57, pull-up, VDD 1.8 V, chung GND |

### 30.5. Công suất

- Shutdown SHDN khi không đo  
- Proximity + PILOT_PA nhỏ khi chờ ngón tay  
- SR 50–100 sps + averaging cho pin  
- LED chiếm phần lớn năng lượng VLED+, không phải IDD 600 µA  

---

## 31. Checklist khi làm việc với cảm biến

### **Phần cứng**

- [ ] VDD 1.7–2.0 V (không 3.3 V trực tiếp vào die)
- [ ] VLED+ 3.1–5.25 V, tụ sát chân
- [ ] GND và PGND đúng; ESP32 GND chung module
- [ ] I2C pull-up; INT pull-up
- [ ] N.C. vẫn có pad cơ khí nếu layout chip trần
- [ ] Cửa sổ quang sạch, ngón áp sát, giảm ánh sáng lọt

### **Firmware**

- [ ] PART_ID = 0x15
- [ ] Reset rồi mới config
- [ ] Xóa FIFO trước khi chạy mode
- [ ] SR ↔ PW nằm bảng 11/12
- [ ] Đọc FIFO burst tại 0x07 (không giả định auto-increment)
- [ ] Mask bit [17:0] đúng; không sign-extend nhầm
- [ ] Xử lý wrap 5-bit pointer
- [ ] Bật A_FULL thay vì phụ thuộc mọi PPG_RDY nếu ISR nặng
- [ ] SpO2: nhiệt định kỳ + không tin sample khi ALC_OVF
- [ ] SHDN khi idle

### **Thuật toán**

- [ ] Tách DC/AC từng kênh
- [ ] BPM trong dải sinh lý (ví dụ 40–180 bpm) kèm kiểm tra đỉnh
- [ ] SpO2 cần calibration; không hard-code một công thức “chuẩn y tế” từ datasheet (datasheet không có)

---

## 32. Thuật ngữ và viết tắt

| Thuật ngữ | Nghĩa |
| --- | --- |
| PPG | Photoplethysmogram — sóng mạch quang |
| SpO2 | Độ bão hòa oxi máu xung |
| ALC | Ambient Light Cancellation |
| FIFO | Hàng đợi mẫu ADC |
| PA | Pulse Amplitude — mã dòng LED |
| PW | Pulse Width — độ rộng xung LED |
| SR | Sample Rate |
| RGE | ADC Range / full-scale |
| PILOT | Dòng LED proximity / slot phụ |
| UVLO | Undervoltage lockout |
| OESIP | Package quang 14 chân |
| HR | Heart Rate |
| QE | Quantum Efficiency |
| PSRR | Power Supply Rejection Ratio |
| HBM | Human Body Model (ESD) |

---

## Phụ lục A — Tóm tắt thanh ghi “cần nhớ”

| Addr | Chức năng nhanh |
| --- | --- |
| 0x00 / 0x01 | Đọc/xóa ngắt |
| 0x02 / 0x03 | Bật ngắt |
| 0x04–0x07 | FIFO |
| 0x08 | Averaging, rollover, almost-full |
| 0x09 | SHDN, RESET, MODE |
| 0x0A | ADC range, SR, PW |
| 0x0C / 0x0D | Dòng Red / IR |
| 0x10 | PILOT |
| 0x11 / 0x12 | Multi-LED slots |
| 0x1F–0x21 | Nhiệt độ |
| 0x30 | Ngưỡng proximity |
| 0xFF | Part ID = 0x15 |

## Phụ lục B — Công thức firmware hay dùng

```text
I2C_7BIT          = 0x57
PART_ID_EXPECTED  = 0x15

adc18             = ((b1 & 0x03) << 16) | (b2 << 8) | b3

n_samples         = (wr_ptr - rd_ptr) & 0x1F

T                 = (int8_t)TINT + (TFRAC & 0x0F) * 0.0625
                    // TFRAC luôn cộng dương

I_led_mA_approx   = reg_PA * 0.2        // typical, không phải spec chặt
```

---

*Tài liệu này diễn giải lại datasheet MAX30102 Rev 0 (2015) bằng tiếng Việt, bổ sung ghi chú triển khai ESP32/Arduino. Khi không khớp silicon/module, ưu tiên đo thực và file PDF gốc trong thư mục `datasheets`.*
