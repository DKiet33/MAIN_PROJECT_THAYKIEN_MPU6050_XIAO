# Lessons Learned

Use this file to record mistakes, user corrections, and prevention rules.

## Template
### Date
- Context:
- Mistake / issue:
- Root cause:
- Prevention rule:

## Entries

### 2026-04-22
- Context: ESP32-S3 sensor alert flow trong `src/main.ino`, cụ thể hàm `sendAlert()` và cooldown.
- Mistake / issue: Cooldown logic chặn leo thang cảnh báo, khiến `WARNING → DANGER` bị kẹt và buzzer vẫn chạy pattern chậm.
- Root cause: Throttle Telegram và cập nhật state phần cứng bị ghép chung vào cùng một early-return path.
- Prevention rule: Khi dùng cooldown/rate-limit, cập nhật internal severity/state **trước**, chỉ áp throttle lên side-effects bên ngoài (Telegram, log).

### 2026-04-22
- Context: State machine cảm biến và hành vi Telegram `/stop` trong `src/main.ino`.
- Mistake / issue: Xử lý lỗi cảm biến chung có thể che khuất đọc `DANGER` thật, và `/stop` không ngăn re-trigger ngay lập tức.
- Root cause: Thứ tự ưu tiên an toàn không được khai báo rõ trong `processSystemLogic()`, và ý định "tạm tắt cảnh báo" của user chưa được mô hình hoá thành state riêng.
- Prevention rule: Trong hệ thống alert, đánh giá nguy hiểm **trước** degraded-sensor. Model hành động operator (mute/stop) là timed state, không phải one-shot variable reset.

### 2026-05-15
- Context: Tích hợp Fall Detection vào `Rtos_main.ino` — ban đầu code UART2 trước khi dev XIAO hoàn thiện.
- Mistake / issue: Code UART/Serial2 + TaskFallDetect được thêm vào rồi phải xoá ngay vì dev khác chưa xong, giao thức cũng chưa chốt (WiFi hay ESP-NOW).
- Root cause: Implement integration code khi cả 2 phía chưa sẵn sàng và giao thức chưa được quyết định.
- Prevention rule: Không code integration layer cho external system cho đến khi (1) phía kia đã xong API/protocol, và (2) giao thức truyền thông đã được chốt. Chỉ cần để TODO comment + ghi vào `hardware.md` và `todo.md`.
