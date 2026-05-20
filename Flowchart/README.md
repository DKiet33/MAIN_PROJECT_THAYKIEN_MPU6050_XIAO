# Flowchart — Tài liệu Thiết kế Hệ thống

Thư mục này chứa tài liệu kỹ thuật mô tả kiến trúc và nguyên lý hoạt động của firmware.

**Cập nhật lần cuối:** 2026-05-20

---

## Trạng Thái Dự Án (2026-05-20)

### Đã Hoàn Thành ✅
- **Firmware gốc** (`src/main/main.ino`): Baseline đầy đủ — ENS160, AHT21, LM75, Buzzer, LED, Telegram Bot. File này được giữ nguyên làm tham chiếu.
- **Firmware FreeRTOS Trạm chính** (`src/Rtos_main/Rtos_main.ino`): 4 FreeRTOS tasks:
  - `TaskSensorRead` (Core1/Pri3): Đọc ENS160, AHT21, LM75 mỗi 1 giây
  - `TaskAlertManager` (Core1/Pri4): Tính mức cảnh báo (NONE/WARNING/DANGER/CRITICAL/**FALL**) với Latching 12s và Danger Overwrite
  - `TaskBuzzerLED` (Core1/Pri5): Điều khiển buzzer + LED theo pattern từng mức cảnh báo
  - `TaskActuator` (Core0/Pri3): Điều khiển Servo SG90 và Quạt 12V (NPN 2N2222)
- **Thiết bị đeo FreeRTOS** (`src/wearable/wearable_unified_rtos/wearable_unified_rtos.ino`): 3 FreeRTOS tasks:
  - `TaskIMURead` (Core1/Pri5): Đọc MPU6050 @ 100Hz → `imuQueue`
  - `TaskEdgeAI` (Core1/Pri4): Inference Edge Impulse + Debounce/Cooldown + **phát ESP-NOW**
  - `TaskNetworkWeb` (Core0/Pri3): Web server + INGESTION state machine + HTTPS upload
- **Bộ lọc chống báo giả**: Confirm Slices (x2) + Cooldown (6s)
- **ESP-NOW TX (Wearable)**: Code xong — `WIFI_AP_STA`, SoftAP Kênh 1, gói tin `FallAlertPacket` 3x
- **ESP-NOW RX (Main Station)**: Code xong — `WIFI_STA`, Kênh 1 lock, callback `OnDataRecv`, `ALERT_FALL=5`

### Cần Làm Tiếp Theo 🔴
1. **[KHẨN CẤP] Test tích hợp ESP-NOW trên 2 thiết bị thực tế:**
   - Nạp `Rtos_main.ino` → copy địa chỉ MAC STA in ra Serial
   - Dán MAC vào `MAIN_BOARD_MAC[]` trong `wearable_unified_rtos.ino` (~dòng 60)
   - Nạp `wearable_unified_rtos.ino` → kich hoạt chế độ INFERENCE → giả lập ngã
   - Xác minh: truyền nhận < 10ms, Latching 12s, Danger Overwrite
2. **Hoàn thiện Actuator vật lý**: Bật `ENABLE_SERVO=1`, kiểm thử servo vật lý
3. **Web UI tập trung** (tùy chọn): Giám sát cả môi trường + wearable trên cùng dashboard

---

## Phần Cứng

| Thành phần | Mô tả |
|---|---|
| ESP32-S3 N16R8 | Board trạm chính, 16MB Flash + 8MB PSRAM |
| ENS160 | Cảm biến chất lượng không khí (TVOC, eCO2, AQI) |
| AHT21 | Cảm biến nhiệt độ + độ ẩm |
| LM75 | Cảm biến nhiệt độ I2C dự phòng (0x48) |
| Buzzer | Cảnh báo âm thanh (GPIO2) |
| LED | Chỉ thị trạng thái (GPIO4) |
| Servo SG90 | Mở/đóng cơ cấu chấp hành (GPIO5) |
| Quạt 12V + 2N2222 | Tản nhiệt tự động (GPIO6) |
| XIAO ESP32-S3 | Sub-board nhận diện ngã (MPU6050 + Edge Impulse) |
| MPU6050 (GY-521) | Cảm biến gia tốc + con quay hồi chuyển 6 trục |

---

## Danh Sách Tài Liệu

| File | Nội dung |
|---|---|
| [`codeflow.md`](./codeflow.md) | Chi tiết kỹ thuật: cấu trúc code, data flow, logic từng task, API web server, giao thức ESP-NOW |
| [`nguyen-ly-hoat-dong-flowchart.md`](./nguyen-ly-hoat-dong-flowchart.md) | Flowchart Mermaid toàn hệ thống: khởi động, TaskAlertManager, TaskBuzzerLED, TaskActuator, TaskEdgeAI + ESP-NOW, bộ lọc chống báo giả |
| [`README.md`](./README.md) | File này — trạng thái dự án và chỉ mục tài liệu |

---
*Báo cáo tiến độ — cập nhật định kỳ.*
