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

### 2026-05-27
- Context: Thử nghiệm thực tế kết nối không dây ESP-NOW trên bộ đôi XIAO ESP32-S3 và ESP32-S3 N16R8.
- Mistake / issue: 
  1. Bo mạch N16R8 im lặng hoàn toàn trên Serial Monitor khi khởi động, không in bất kỳ dòng log nào (kể cả địa chỉ MAC).
  2. Hàm callback nhận dữ liệu của ESP-NOW (`onDataRecv`) gọi các hàm `delay()` để điều khiển chu kỳ nhấp nháy còi/LED dẫn đến sập kết nối Wi-Fi stack và reset chip (Watchdog Timer Panic) khi nhận gói tin đầu tiên.
  3. Lỗi biên dịch xảy ra trên Arduino ESP32 Core v3.x do chữ ký hàm của callback gửi `esp_now_send_cb_t` thay đổi tham số đầu tiên từ mảng MAC `const uint8_t *` thành con trỏ cấu trúc `const esp_now_send_info_t *`.
- Root cause:
  1. **Tốc độ Boot của USB CDC cực nhanh**: Cổng USB native (CDC) của ESP32-S3 khởi động và chạy qua hàm `setup()` trước khi hệ điều hành máy tính kịp nhận diện cổng COM ảo và mở Serial Monitor.
  2. **Vi phạm quy tắc Blocking trong Callback/ISR**: Hàm callback của ESP-NOW chạy trực tiếp dưới ngữ cảnh Task Wi-Fi hệ thống (Wi-Fi Task Context). Gọi các tác vụ blocking nặng như `delay()` (FreeRTOS yield) trong callback sẽ chặn đứng Wi-Fi stack, kích hoạt Watchdog.
  3. **Thay đổi API phá vỡ tương thích (Breaking API Change) ở Core v3.x**: Bản cập nhật Arduino Core v3.x chuyển đổi giao thức sang lõi ESP-IDF v5.x nâng cấp kiểu dữ liệu callback gửi/nhận để bổ sung thêm siêu dữ liệu (metadata).
- Prevention rule:
  1. **Thêm vòng chờ USB CDC mở cổng khi khởi động**: Trên các dòng ESP32-S3 sử dụng USB CDC on boot, luôn đặt đoạn code dừng chờ mở cổng Serial `while (!Serial && (millis() - startMs < 4000)) delay(10);` ở đầu hàm `setup()` để bảo toàn toàn bộ log khởi động.
  2. **Giữ Callback siêu nhẹ và phi chặn (Non-blocking)**: Hàm callback nhận/gửi chỉ nên copy dữ liệu sang biến toàn cục an toàn (`volatile`) và phất cờ hiệu báo tin nhắn mới. Toàn bộ hành vi xử lý, in log Serial, nháy LED hay còi có độ trễ phải được thực hiện trong hàm `loop()` chính hoặc các Task FreeRTOS riêng biệt.
  3. **Sử dụng cú pháp tương thích chéo phiên bản (Version-guard compile macro)**: Sử dụng các macro kiểm tra phiên bản `#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)` khi định nghĩa các hàm callback gửi/nhận ESP-NOW để code tự động tương thích với cả các hệ máy cũ (Core v2.x) và mới (Core v3.x).

### 2026-05-27
- Context: Khởi tạo và điều khiển Servo dùng thư viện `ESP32Servo` kết hợp FreeRTOS trên ESP32-S3.
- Mistake / issue: ESP32-S3 bị treo (hang) ngay khi bật `ENABLE_SERVO 1` dù đấu nối phần cứng hoàn toàn đúng, cụ thể là TaskActuator bị đứng sau log "Đang khởi tạo Servo...".
- Root cause:
  1. **Xung đột tài nguyên chéo nhân (Cross-core Resource Conflict):** Hàm `servo.attach(...)` được gọi trong `setup()` chạy trên **Core 1**, nhưng tác vụ điều khiển `servo.write(...)` nằm trong `TaskActuator` chạy trên **Core 0**. Thư viện `ESP32Servo` quản lý bộ định thời phần cứng LEDC nhạy cảm, gọi chéo nhân không đồng bộ gây treo xung đột.
  2. **Dòng khởi động tức thời của Servo (Inrush Current Spike):** Khi vừa đính kèm chân (`attach`), Servo lập tức giật mạnh về vị trí mặc định và rút dòng đột ngột. Nếu thực hiện ngay khi vừa cắm USB (hệ thống chưa ổn định), sẽ gây sụt áp đường 5V và sập nguồn hoặc treo vi điều khiển.
  3. **Lỗi tự dò kênh của thư viện ESP32Servo trên ESP32-S3:** Thư viện tự động tìm kiếm kênh LEDC rỗi trên ESP32-S3 nhưng do sự khác biệt kiến trúc và core v3.x mới, hàm tự dò này rơi vào vòng lặp vô hạn (infinite loop) làm treo cứng tác vụ `TaskActuator`.
- Prevention rule:
  1. **Khởi tạo phần cứng trong Task sở hữu (In-task hardware initialization):** Dời toàn bộ logic khởi tạo (`servo.attach`) vào ngay bên trong Task FreeRTOS chịu trách nhiệm điều khiển cơ cấu chấp hành đó, thực hiện ngay trước vòng lặp `for(;;)` của Task thay vì gọi trong `setup()`.
  2. **Đồng bộ nhân xử lý (Core Synchronization):** Chuyển Task cơ cấu chấp hành (`TaskActuator`) sang chạy cùng **Core 1** (giống như các Task Sensor và WiFi) để tránh xung đột tài nguyên chéo lõi cho LEDC/PWM.
  3. **Chờ ổn định nguồn (Stabilization Delay):** Sử dụng `vTaskDelay(500ms)` trước khi khởi tạo Servo để đảm bảo điện áp cấp qua cổng USB đã ổn định hoàn toàn.
  4. **Cấp phát Timer thủ công (Manual Timer Allocation):** Sử dụng lệnh `ESP32PWM::allocateTimer(0/1/2/3)` trước khi đính kèm Servo để định hướng kênh tĩnh, giải quyết triệt để lỗi lặp vô hạn của thư viện.

### 2026-05-27
- Context: Khởi tạo PWM điều khiển Servo bằng LEDC API trên chip ESP32-S3.
- Mistake / issue: Hàm khởi tạo `ledcAttach` (Core v3.x) trả về `THẤT BẠI` (FAIL) khi đặt độ phân giải `SERVO_LEDC_BITS` là 16-bit.
- Root cause: Giới hạn phần cứng của chip ESP32-S3 (cũng như ESP32-C3, ESP32-S2) chỉ hỗ trợ tối đa độ phân giải timer LEDC là **14-bit**. Chỉ có dòng chip ESP32 thế hệ đầu mới hỗ trợ độ phân giải lên đến 20-bit. Khi cố gắng khởi tạo 16-bit, driver API sẽ tự chặn và trả về lỗi khởi tạo thất bại.
- Prevention rule: Khi thiết lập độ phân giải PWM LEDC cho các dòng chip dòng S/C (như ESP32-S3), bắt buộc phải khống chế giá trị tối đa là **14-bit** (nên dùng 14-bit hoặc 12-bit). Không được bê nguyên cấu hình 16-bit từ ESP32 cũ sang. Cần sử dụng công thức tính duty cycle động `max_duty = (1 << SERVO_LEDC_BITS) - 1` để dễ dàng tương thích chéo thiết bị.



