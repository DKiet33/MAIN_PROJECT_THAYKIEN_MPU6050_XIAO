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
- [x] Hoàn thiện Fall Detection trên XIAO ESP32-S3 (MPU6050 + Edge Impulse) với đầy đủ Web UI điều khiển tiếng Việt có dấu.
- [x] Tích hợp bộ lọc chống báo giả (Debounce + Cooldown) cho luồng suy luận phát hiện té ngã.
- [ ] **[PENDING]** Tích hợp kết quả Fall Detection vào `Rtos_main.ino` qua WiFi hoặc ESP-NOW.
- [ ] Build web control UI trong `src/index.html`.


## Notes

- `src/main/main.ino` — file goc, **khong sua**, dung lam tham chieu.
- `src/main/Rtos_main.ino` — file dang phat trien chinh thuc.
- `src/fall_detection_xiao/fall_detection_xiao.ino` — sketch cho XIAO ESP32-S3, cho dev khac hoan thien.
- Giao tiep giua XIAO va Main ESP32-S3 du kien dung **WiFi** hoac **ESP-NOW** (chua quyet dinh).
- Cap nhat file nay truoc va trong khi thuc hien bat ky task khong tam thuong nao.

## Review

- 2026-04-22: Khoi tao workspace scaffold.
- 2026-04-22: Tao firmware baseline `main.ino` (ENS160, AHT21, LM75, MQ2, Buzzer, LED, Telegram).
- 2026-04-22: Sua alert escalation logic va `/stop` suppression.
- 2026-05-15: Tao `Rtos_main.ino` voi FreeRTOS (bo Telegram, MQ2; them Servo, Fan).
- 2026-05-15: Tao `fall_detection_xiao.ino` — sketch mau XIAO ESP32-S3.
- 2026-05-18: Hoàn thiện dự án thiết bị đeo phát hiện té ngã với MPU6050 và XIAO ESP32-S3 (Edge Impulse, Dual-tab Ingestion/Inference Web UI, 100% tiếng Việt có dấu, tích hợp bộ lọc chống báo giả liên tục Debounce + Cooldown).

