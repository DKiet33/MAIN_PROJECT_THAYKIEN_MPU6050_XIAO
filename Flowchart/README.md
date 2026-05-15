# Du An He Thong Nhan Dien Va Canh Bao

He thong canh bao da chuc nang chay tren ESP32-S3 N16R8, tich hop cam bien chat luong khong khi, nhiet do, va nhan dien nga bang AI.

## Trang Thai Hien Tai (2026-05-15)

### Da Hoan Thanh
- **Firmware goc** (`src/main/main.ino`): Baseline day du — ENS160, AHT21, LM75, MQ2, Buzzer, LED, Telegram Bot. File nay duoc giu nguyen lam tham chieu.
- **Firmware FreeRTOS** (`src/main/Rtos_main.ino`): Phien ban dang phat trien chinh thuc voi 4 FreeRTOS tasks:
  - `TaskSensorRead` (Core1/Pri3): Doc ENS160, AHT21, LM75 moi 1 giay
  - `TaskAlertManager` (Core1/Pri4): Tinh muc canh bao (NONE/WARNING/DANGER/CRITICAL)
  - `TaskBuzzerLED` (Core1/Pri5): Dieu khien buzzer + LED theo pattern muc canh bao
  - `TaskActuator` (Core0/Pri3): Dieu khien Servo SG90 va Quat 12V (NPN 2N2222)
- **Pin Map day du**: ENS160/AHT21/LM75 qua I2C, Buzzer GPIO2, LED GPIO4, Servo GPIO5, Fan GPIO6.

### Dang Thuc Hien (Dev Khac)
- **Fall Detection** (`src/fall_detection_xiao/fall_detection_xiao.ino`): Board XIAO ESP32-S3 + MPU6050 + Edge Impulse model train san. Gac len nguoi, phat hien nga.
- Giao thuc truyen thong giua XIAO va Main ESP32-S3: **WiFi hoac ESP-NOW** (chua chot).

## Phan Cung

| Thanh phan | Mo ta |
|---|---|
| ESP32-S3 N16R8 | Board chinh, 16MB Flash + 8MB PSRAM |
| ENS160 | Cam bien chat luong khong khi (TVOC, eCO2, AQI) |
| AHT21 | Cam bien nhiet do + do am |
| LM75 | Cam bien nhiet do I2C du phong |
| Buzzer | Canh bao am thanh |
| LED | Chi thi trang thai |
| Servo SG90 | Mo/dong co cau chap hanh |
| Quat 12V + 2N2222 | Tan nhiet tu dong |
| XIAO ESP32-S3 | Sub-board nhan dien nga (MPU6050 + Edge Impulse) |

## Ke Hoach Tiep Theo

1. **[Dev khac]** Hoan thien firmware XIAO ESP32-S3 (Edge Impulse inference + truyen tin hieu nga).
2. **[Sau khi chot giao thuc]** Tich hop Fall Detection vao `Rtos_main.ino` (them 1 task WiFi/ESP-NOW).
3. **[Tuy chon]** Xay dung Web UI (`src/index.html`) de giam sat trang thai he thong.

---
*Bao cao tien do cap nhat dinh ky.*
