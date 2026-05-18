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

### Đã Hoàn Thành (Wearable Fall Detection)
- **Fall Detection** (`src/wearable/wearable_unified`): Hệ thống thiết bị đeo thắt lưng sử dụng Seeed Studio XIAO ESP32-S3 + cảm biến MPU6050 + mô hình suy luận Edge Impulse. 
  - Tích hợp Web UI điều khiển trực quan tiếng Việt, hỗ trợ chế độ kép: Thu thập mẫu (Ingestion) trực tiếp gửi lên Edge Impulse Studio và Suy luận liên tục (Inference) phát hiện té ngã thời gian thực.
  - Tích hợp bộ lọc chống báo giả cao cấp: Xác nhận liên tiếp (`FALL_CONFIRM_SLICES`) và khóa thời gian chờ (`FALL_COOLDOWN_MS`).
  - Bản địa hóa 100% tiếng Việt có dấu cho toàn bộ hệ thống logs Serial Monitor và giao diện web.

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

1. **[Tích hợp truyền thông]** Kết nối giao tiếp không dây (WiFi hoặc ESP-NOW) giữa thiết bị đeo XIAO ESP32-S3 và board chính ESP32-S3 N16R8 để truyền cảnh báo ngã trực tiếp.
2. **[Tích hợp firmware chính]** Bổ sung Task nhận tín hiệu ngã từ thiết bị đeo vào `Rtos_main.ino` để kích hoạt còi/đèn cảnh báo đồng bộ trên board chính.
3. **[Tùy chọn]** Xây dựng Web UI (`src/index.html`) tập trung giám sát toàn bộ hệ thống (cả chất lượng không khí của board chính và trạng thái thiết bị đeo).


---
*Bao cao tien do cap nhat dinh ky.*
