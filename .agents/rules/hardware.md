# ESP32-S3 Hardware Notes

## Purpose
Store the current pin map, attached modules, and board constraints for the ESP32-S3 N16R8 build.

## Board
- MCU: ESP32-S3
- Variant: ESP32-S3 N16R8, 44-pin
- Flash / PSRAM: 16MB OPI Flash + 8MB OPI PSRAM
- USB mode: USB CDC on boot enabled
- Power input: 5V USB hoặc nguồn ngoài regulated

---

## Active Pin Map — Main ESP32-S3

| Function         | GPIO   | Notes                                           |
| ---------------- | ------ | ----------------------------------------------- |
| Buzzer           | GPIO2  | Digital output                                  |
| Status LED       | GPIO4  | Digital output                                  |
| Servo SG90       | GPIO5  | PWM 50Hz — thư viện ESP32Servo (pulse 500–2500µs) |
| Fan 12V (NPN)    | GPIO6  | Digital out → Base 2N2222 (HIGH = Fan ON)       |
| I2C SDA          | GPIO8  | Shared bus: ENS160, AHT21, LM75                 |
| I2C SCL          | GPIO9  | Shared bus: ENS160, AHT21, LM75                 |
| GPIO17           | —      | Tự do (dự phòng)                                |
| GPIO18           | —      | Tự do — dành sẵn cho Fall Detection (WiFi/ESP-NOW tích hợp sau) |

> **Đã bỏ:** MQ2 (GPIO1) — loại khỏi `Rtos_main.ino`, vẫn còn trong `main.ino` gốc.

---

## I2C Devices — Main ESP32-S3

| Device | Address | Bus                  |
| ------ | ------- | -------------------- |
| ENS160 | `0x53`  | I2C on GPIO8 / GPIO9 |
| AHT21  | `0x38`  | I2C on GPIO8 / GPIO9 |
| LM75   | `0x48`  | I2C on GPIO8 / GPIO9 |

I2C clock: 400 kHz (`Wire.setClock(400000)`).

---

## Fan Wiring (NPN 2N2222)

```
ESP32 GPIO6 → 1kΩ → Base (2N2222)
              Collector → Fan(-) [diode 1N4007 ngược chiều song song với Fan]
              Emitter   → GND
Fan(+) → 12V
```

> ⚠️ Bắt buộc có diode 1N4007 flyback để chống dòng ngược khi tắt motor.

---

## Sub-board — XIAO ESP32-S3 (Fall Detection)

> **Trạng thái:** Đang được dev khác thực hiện.  
> **Giao thức dự kiến:** WiFi hoặc ESP-NOW (chưa quyết định cuối).

| Function         | Pin XIAO                | Notes                              |
| ---------------- | ----------------------- | ---------------------------------- |
| MPU6050 SDA      | GPIO5                   | I2C                                |
| MPU6050 SCL      | GPIO6                   | I2C                                |
| Giao tiếp Main   | WiFi / ESP-NOW          | Gắn lên người, phát hiện ngã      |
| GND              | GND                     | Chung với Main board nếu có dây   |

---

## Loại bỏ / Không dùng

- MQ2 (GPIO1) — đã bỏ khỏi `Rtos_main.ino`, vẫn còn trong `main.ino` gốc.
- Telegram Bot / WiFi client — đã bỏ, không kết nối được server Telegram.
- Camera AI / ESP32-CAM UART — đã bỏ từ đầu dự án.

---

## Constraints

- **GPIO33–37**: Bị chiếm bởi OPI PSRAM — KHÔNG dùng.
- Ưu tiên ADC1 (GPIO1–10) cho cảm biến analog khi WiFi active (ADC2 bị conflict).
- Kiểm tra boot-strapping và USB-safe pins trước khi gán relay, button, hoặc display.
- Cập nhật file này mỗi khi thêm module mới.

---

## Component Links

### 1. Khối nhận dữ liệu và cảm biến (Sử dụng ESP32-S3)
- [Module WiFi Bluetooth WROOM-1 N16R8 ESP32-S3 DevKitC-1](https://icdayroi.com/module-wifi-bluetooth-wroom-1-n16r8-esp32-s3-devkitc-1)
- [Cảm biến chất lượng nhiệt độ và độ ẩm không khí ENS160+AHT21 eCO2 TVOC](https://icdayroi.com/cam-bien-chat-luong-nhiet-do-va-do-am-khong-khi-ens160-aht21-eco2-tvoc)
- [Cảm biến nhiệt độ CJMCU-75 LM75 hỗ trợ I2C](https://icdayroi.com/cam-bien-nhiet-do-cjmcu-75-lm75-ho-tro-i2c)
- [Cảm biến khí gas MQ-2](https://icdayroi.com/cam-bien-khi-gas-mq-2) *(đã bỏ khỏi Rtos_main)*

### 2. Thiết bị đeo tay (Sử dụng XIAO ESP32S3 + MPU6050)
- [XIAO ESP32S3 Getting Started](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- Cảm biến gia tốc MPU6050
