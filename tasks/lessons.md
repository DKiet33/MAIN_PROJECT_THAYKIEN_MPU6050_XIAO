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

### 2026-05-20
- Context: Soạn thảo câu trả lời và báo cáo nghiên cứu trong chat.
- Mistake / issue: Sử dụng cú pháp toán học LaTeX dạng `$ ... $` (ví dụ: `$100\text{Hz}$`, `$\approx 40\text{ms}$`) khiến giao diện chat hiển thị lỗi, xuất hiện các chuỗi ký tự thô mất thẩm mỹ.
- Root cause: Dùng ký hiệu LaTeX theo thói quen khi biểu diễn các đại lượng vật lý/thời gian, không kiểm tra khả năng hỗ trợ LaTeX của giao diện chat UI.
- Prevention rule: Tuyệt đối không sử dụng định dạng LaTeX (`$ ... $`, `\approx`, `\text{...}`) trong câu trả lời chat hoặc tài liệu trừ khi có yêu cầu đặc biệt. Thay vào đó, hãy biểu diễn bằng văn bản thuần hoặc ký hiệu tiêu chuẩn dễ đọc (ví dụ: `100Hz`, `~40ms`, `~370ms`, `khoảng`, `xấp xỉ`, `độ C`).

### 2026-05-20
- Context: Chuyển đổi mã nguồn đơn luồng bare-metal sang FreeRTOS đa nhiệm cho board Wearable MPU6050 (XIAO ESP32S3).
- Mistake / issue: 
  1. Giao tiếp I2C bị sập/FAIL khi chạy đa nhiệm FreeRTOS, đặc biệt là khi task hiệu chuẩn (`CALIBRATING`) hoặc các tác vụ giám sát trên Core 0 gọi I2C trực tiếp.
  2. Khi viết mã chẩn đoán `test_mpu6050_rtos.ino` và nâng cấp `wearable_unified.ino`, đã tự ý thêm bước kiểm tra chữ ký thanh ghi `WHO_AM_I` khắt khe, dẫn đến việc thiết bị báo lỗi khởi tạo thất bại vì chip vật lý thực tế trên board là MPU6500 (trả về chữ ký `0x70` thay vì `0x68` của MPU6050 gốc), trong khi mã nguồn bare-metal ban đầu không hề kiểm tra chữ ký này và vẫn chạy tốt.
- Root cause:
  1. **Xung đột Interrupt Core-Specific trên ESP32**: Hàm `Wire.begin()` được gọi trong `setup()` đăng ký ngắt I2C trên Core 1. Khi các task chạy trên Core 0 cố tương tác I2C trực tiếp thông qua `Wire`, phần cứng bị timeout và sập ngắt.
  2. **Over-engineering (Tự ý thêm logic kiểm tra phần cứng nghiêm ngặt)**: Thói quen tự thêm các chốt chặn chẩn đoán/safeguard (`WHO_AM_I`) khi porting code cũ mà không để ý rằng phần cứng thực tế có thể sử dụng các biến thể/clone tương thích thanh ghi nhưng khác ID chữ ký, phá vỡ tính tương thích ngược vốn có của mã gốc.
- Prevention rule:
  1. **Cô lập hoạt động I2C vật lý trên Core 1**: Trong các ứng dụng FreeRTOS trên ESP32, toàn bộ hoạt động giao tiếp trực tiếp qua I2C (`Wire`) bắt buộc phải được cô lập duy nhất trong một task chạy trên Core đăng ký ngắt (thường là Core 1). Các task khác trên các core khác chỉ được phép tiêu thụ dữ liệu gián tiếp thông qua hàng đợi (`Queue`) hoặc biến toàn cục an toàn luồng.
  2. **Tôn trọng tính tối giản và logic của mã gốc (Simplicity & Least Surprise)**: Khi porting hoặc tối ưu hóa mã nguồn hiện có, tuyệt đối không tự ý thêm các bước kiểm tra phần cứng nghiêm ngặt hơn (như đọc ID, check signature) nếu mã nguồn gốc không yêu cầu. Luôn giữ nguyên logic khởi tạo thô (ping địa chỉ I2C rồi ghi thanh ghi trực tiếp) để duy trì khả năng tương thích tối đa với các cảm biến biến thể/clone mà không tạo ra lỗi chặn đột ngột.


