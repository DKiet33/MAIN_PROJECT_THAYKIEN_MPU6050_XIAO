# Task Plan

## Active Plan

- [x] Create project skeleton for agent workflow and source files.
- [x] Seed orchestration rules in `.agents/rules/instructions.md`.
- [x] Add memory files in `tasks/` for planning and lessons learned.
- [x] Create placeholders for ESP32-S3 firmware and web UI.
- [x] Replace hardware placeholders in `.agents/rules/hardware.md`.
- [x] Implement sensor-only firmware baseline in `src/main/main.ino`.
- [x] Remove camera AI / ESP32-CAM pin usage from the current baseline.
- [x] Fix alert escalation logic so `WARNING -> DANGER/CRITICAL` updates buzzer/LED immediately.
- [x] Prioritize real `DANGER` readings over sensor-error warnings in the state machine.
- [x] Add temporary alert suppression for Telegram `/stop` (suppress 60s).
- [x] Tao `src/main/Rtos_main.ino` — FreeRTOS version (4 tasks: SensorRead, AlertManager, BuzzerLED, Actuator).
- [x] Bo Telegram Bot va MQ2 ra khoi `Rtos_main.ino`.
- [x] Them Servo SG90 (GPIO5) va Quat 12V / NPN 2N2222 (GPIO6) vao `Rtos_main.ino`.
- [x] Tạo `src/fall_detection_xiao/fall_detection_xiao.ino` — sketch mẫu cho XIAO ESP32-S3.
- [x] Cập nhật `hardware.md` với pin map đầy đủ (Servo, Fan, XIAO sub-board).
- [x] Hoàn thiện Fall Detection trên XIAO ESP32-S3 (MPU6050 + Edge Impulse) với đầy đủ Web UI điều khiển.
- [x] Tích hợp bộ lọc chống báo giả (Debounce + Cooldown) cho luồng suy luận phát hiện té ngã.
- [x] Nghiên cứu kiến trúc tích hợp FreeRTOS cho Wearable và giải pháp kết nối không dây ESP-NOW đồng kênh Wi-Fi (lưu trữ trong `research_notes.md`).
- [x] Backup `wearable_unified` folder → `src/wearable/backup/wearable_unified_backup_20260520`.
- [x] Tích hợp FreeRTOS vào Wearable Board (`wearable_unified.ino`), phân chia 3 task: `TaskIMURead` (đọc cảm biến 100Hz, Core 1), `TaskEdgeAI` (suy luận 370ms, Core 1), `TaskNetworkWeb` (Web server & upload, Core 0).
- [x] Sửa lỗi giao tiếp I2C MPU6050 trong test_mpu6050_rtos.ino (Di chuyển I2C scan/init sang setup, nâng stack size lên 4096 bytes).
- [x] Sửa đổi đồng bộ luồng I2C trong wearable_unified.ino (CALIBRATING tiêu thụ dữ liệu từ imuQueue thay vì gọi imuRead trực tiếp trên Core 0).
- [x] Tích hợp phát tín hiệu ESP-NOW vào Wearable Board (khóa AP kênh 1, gửi gói tin `FallAlertPacket`).
- [x] Tích hợp nhận tín hiệu ESP-NOW vào Trạm chính (`Rtos_main.ino`), kích hoạt trạng thái `ALERT_FALL` qua `alertMutex` với tự khóa 12s.

## Notes

- `src/main/main.ino` — file goc, **khong sua**, dung lam tham chieu.
- `src/Rtos_main/Rtos_main.ino` — file dang phat trien chinh thuc cho Trạm chính (đã tích hợp ESP-NOW RX).
- `src/wearable/wearable_unified_rtos/wearable_unified_rtos.ino` — file đang phát triển chính thức cho Thiết bị đeo (FreeRTOS + ESP-NOW TX).
- Giao tiếp không dây chính thức thống nhiệm dùng **ESP-NOW** hoạt động đồng kênh **Kênh Wi-Fi 1** để đảm bảo vừa duy trì Web UI vừa truyền tin khẩn cấp độ trễ siêu thấp dưới 10ms.
- Cap nhat file nay truoc va trong khi thuc hien bat ky task khong tam thuong nao.

## Cần Làm Tiếp Theo (Pending)

- [x] test thành công các thiết bị Quạt / LED / Buzzer / Servo.
- [x] Khắc phục lỗi hoạt động thiết bị ngoại vi bằng cách chuyển sang Arduino Native LEDC API (hỗ trợ đa phiên bản Core v2.x / v3.x thông qua Macro compile guard).
- [ ] Xây dựng Web UI cho Trạm chính (ESP32-S3 N16R8) để giám sát các thông số môi trường (Nhiệt độ, Độ ẩm, Khí độc).
- [ ] Hợp nhất 2 Web UI của Trạm chính (N16R8) và Thiết bị đeo (XIAO ESP32-S3) thành một giao diện quản trị tập trung duy nhất.
- [x] Kích hoạt và tích hợp thành công phản hồi vật lý của các thiết bị ngoại vi trên Main Board.

## Review

- 2026-04-22: Khoi tao workspace scaffold.
- 2026-04-22: Tao firmware baseline `main.ino` (ENS160, AHT21, LM75, MQ2, Buzzer, LED, Telegram).
- 2026-04-22: Sua alert escalation logic va `/stop` suppression.
- 2026-05-15: Tao `Rtos_main.ino` voi FreeRTOS (bo Telegram, MQ2; them Servo, Fan).
- 2026-05-15: Tao `fall_detection_xiao.ino` — sketch mau XIAO ESP32-S3.
- 2026-05-18: Hoàn thiện dự án thiết bị đeo phát hiện té ngã với MPU6050 và XIAO ESP32-S3 (Edge Impulse, Dual-tab Ingestion/Inference Web UI, tích hợp bộ lọc chống báo giả liên tục Debounce + Cooldown).
- 2026-05-20: Hoàn thành báo cáo nghiên cứu tích hợp FreeRTOS đa nhiệm và kết nối không dây đồng kênh ESP-NOW giữa hai board (lưu trữ trong `research_notes.md`).
- 2026-05-20: Tích hợp FreeRTOS vào Wearable Sub-Board (`wearable_unified_rtos.ino`) — 3 task (TaskIMURead Core1/Pri5, TaskEdgeAI Core1/Pri4, TaskNetworkWeb Core0/Pri3) bảo vệ bằng `imuQueue` + `stateMutex`. Backup gốc tại `backup/wearable_unified_backup_20260520`.
- 2026-05-20: Sửa đổi test_mpu6050_rtos.ino di chuyển quét I2C và khởi tạo MPU vào setup(), nâng stack size, cô lập ngắt I2C trên Core 1.
- 2026-05-20: Tích hợp hoàn chỉnh ESP-NOW TX/RX trên cả 2 board. Thiết bị đeo phát `FallAlertPacket` (0xFA) 3x qua Kênh 1. Trạm chính nhận qua `OnDataRecv`, kích hoạt `ALERT_FALL=5` với Latching 12s và Danger Overwrite. Cơ chế Buzzer/LED/Actuator đã cập nhật đầy đủ.
- 2026-05-27: **Test thực tế thành công kết nối ESP-NOW giữa bộ đôi XIAO ESP32-S3 và ESP32-S3 N16R8.** Sửa lỗi USB CDC trôi log bằng cách thêm vòng lặp chờ, sửa lỗi sập Wi-Fi do delay() trong ISR bằng cơ chế cờ hiệu volatile non-blocking. Cập nhật chữ ký hàm callback tương thích chéo phiên bản Core v3.x. Đồng bộ toàn bộ nâng cấp vào firmware chính thức.
- 2026-05-27: Cập nhật toàn bộ tài liệu .md (README, Flowchart/) phản ánh tình hình mới nhất dự án.
- 2026-05-27: test thành công các thiết bị Quạt / LED / Buzzer / Servo.


