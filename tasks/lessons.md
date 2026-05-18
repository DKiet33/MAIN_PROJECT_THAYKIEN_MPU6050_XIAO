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

### 2026-05-18
- Context: Bản địa hóa tiếng Việt có dấu cho các log hệ thống và tích hợp với Web UI của thiết bị đeo XIAO MPU6050.
- Mistake / issue: Sau khi đổi log hệ thống sang tiếng Việt có dấu, bộ dò cảnh báo té ngã trên Web UI bị hỏng, không hiển thị nhật ký cảnh báo té ngã.
- Root cause: Web UI cũ dùng hàm JavaScript parse chuỗi log Serial thô để tìm từ khóa "ALERT". Khi đổi log Serial sang tiếng Việt có dấu "CẢNH BÁO TÉ NGÃ", bộ parse không còn tìm thấy chuỗi khớp nữa, đồng thời việc parse log thô dễ gây trùng lặp cảnh báo cũ mỗi lần poll.
- Prevention rule: Tránh phân tích cú pháp chuỗi log thô (brittle string parsing) để xác định trạng thái cảnh báo trên giao diện web. Hãy sử dụng bộ đếm trạng thái cụ thể (`fall_count` trong JSON status API) và theo dõi sự thay đổi của bộ đếm giữa các chu kỳ polling để tạo thông báo cảnh báo đáng tin cậy.

### 2026-05-18
- Context: Bộ suy luận liên tục (continuous inference) phát hiện té ngã sử dụng Edge Impulse trên vi điều khiển.
- Mistake / issue: Board chỉ rung động hoặc nhúc nhích nhẹ đã kích hoạt cảnh báo té ngã (false alarm cực cao).
- Root cause: (1) Mô hình AI dự đoán xác suất té ngã theo cửa sổ trượt (sliding window), bất kỳ nhiễu động nhất thời nào cũng tồn tại trong cửa sổ trượt qua nhiều chu kỳ suy luận liên tiếp; (2) Thiếu bộ lọc Debounce và Cooldown sau khi phân loại.
- Prevention rule: Đối với nhận diện hành vi thời gian thực từ dữ liệu cảm biến động, cần triển khai:
  - **Debouncing:** Chỉ kích hoạt cảnh báo khi xác suất vượt ngưỡng liên tục trong N khung hình (`FALL_CONFIRM_SLICES`).
  - **Cooldown:** Thiết lập thời gian khóa tạm thời (`FALL_COOLDOWN_MS`) sau mỗi lần phát hiện cảnh báo thật để tránh việc một cú va đập bị đếm lặp lại nhiều lần do trượt cửa sổ.

