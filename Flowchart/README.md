# Dự Án Hệ Thống Nhận Diện Và Cảnh Báo 

Đây là kho lưu trữ mã nguồn chính cho dự án hệ thống cảnh báo và điều khiển tự động.

## Trạng Thái Hiện Tại
Dự án đã hoàn thiện bộ khung cơ bản, thiết lập được cấu trúc mã nguồn, định tuyến luồng hoạt động (flowchart) và các chương trình nền tảng.

## Kế Hoạch Tiếp Theo (Sắp Tới)
Dự án đang chuẩn bị bước sang giai đoạn tích hợp thêm các thiết bị phần cứng để mở rộng tính năng:
- **Tích hợp Servo và Quạt (Fan)**: Thêm các cơ cấu chấp hành để phục vụ cho các hoạt động điều khiển cơ học và điều hòa/tản nhiệt.
- **Tích hợp XIAO ESP + MPU6050**: Triển khai tính năng **Nhận diện ngã (Fall Detection)**. Cảm biến MPU6050 (gia tốc kế và con quay hồi chuyển) sẽ kết hợp với vi điều khiển XIAO ESP để phát hiện các chuyển động bất thường, nghiêng đổ đột ngột và tự động gửi thông tin để xử lý cảnh báo ngã.

---
*Báo cáo tiến độ được cập nhật định kỳ để theo dõi các thay đổi tiếp theo.*
