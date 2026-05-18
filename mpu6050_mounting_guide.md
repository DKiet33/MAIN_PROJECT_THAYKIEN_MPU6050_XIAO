# Hướng dẫn Đặt và Lắp MPU6050 vào Wearable Phát hiện Té ngã

Tài liệu này là hướng dẫn chuyên sâu dành cho dự án phát hiện té ngã (Fall Detection) sử dụng MPU6050 trên thiết bị đeo (wearable), kết hợp huấn luyện mô hình trên **Edge Impulse**.

---

## 1. Hệ Trục Tọa Độ (Axes of Sensitivity) của MPU6050

Dựa trên Product Specification của InvenSense, nếu đặt chip MPU6050 nằm phẳng trên mặt bàn, mặt in chữ hướng lên, **dấu chấm (Pin 1) ở góc trên-trái**:

| Trục | Chiều dương |
|------|-------------|
| **X** | Sang phải |
| **Y** | Lên trên (về phía Pin 1) |
| **Z** | Vuông góc với mặt chip, hướng lên trên (ra ngoài) |

> [!IMPORTANT]
> Chiều quay của Gyroscope tuân theo **Quy tắc bàn tay phải** (Right-Hand Rule). Khi ngón cái thuận chiều trục, các ngón còn lại cuộn theo chiều quay dương.

---

## 2. Vị trí Lắp Đặt Tối Ưu trên Cơ Thể

Nghiên cứu khoa học so sánh nhiều vị trí (NIH, MDPI) cho kết quả nhất quán:

| Vị trí | Độ chính xác | Ghi chú |
|--------|------------|---------|
| **Eo / Lưng dưới** ⭐ | **Cao nhất** (~98%) | Gần trọng tâm cơ thể, ít nhiễu nhất từ chuyển động tay chân |
| **Ngực / Xương ức** | Cao | Hiệu quả tương đương eo, thiết kế tích hợp áo dễ hơn |
| **Đùi** | Trung bình-Cao | Tốt cho phân tích dáng đi, phức tạp hơn khi gắn |
| **Cổ tay** ❌ | **Thấp nhất** | Nhiều chuyển động tay không liên quan → false positive cao |
| **Cổ chân** | Trung bình | Hữu ích cho gait analysis, không lý tưởng cho fall detection tổng quát |

> [!IMPORTANT]
> **Khuyến nghị:** Dùng vị trí **eo (waist)** hoặc **lưng dưới** cho nghiên cứu này. Đây là tiêu chuẩn vàng (gold standard) trong hầu hết các bài báo khoa học về fall detection.

---

## 3. Định Hướng Trục Cảm biến theo Body Frame

Để dữ liệu nhất quán và không gây vướng víu (cồng kềnh), phần mềm đã được nâng cấp để áp dụng **Ma trận xoay (Rotation Matrix)**. Bạn chỉ cần đeo theo quy tắc sau:

### Quy ước gắn NẰM PHẲNG ở eo:

1. Dán/đặt mạch **nằm phẳng (áp sát) vào bụng/dây nịt**. Bề mặt linh kiện hướng ra ngoài.
2. Hướng các chân cắm (header) chỉ thẳng **vào người**.

Lúc này, các trục vật lý của chip sẽ tương ứng với cơ thể như sau:
- Trục **Z vật lý** hướng ra ngoài → Trở thành **Body X (Forward)**
- Trục **X vật lý** hướng sang trái → Trở thành **Body Y (Left)**
- Trục **Y vật lý** hướng lên đầu → Trở thành **Body Z (Up)**

> [!NOTE]
> Code trong `wearable.ino` và `mpu6050_test.ino` **đã tự động xoay các trục này**. 
> Do đó, khi bạn đứng thẳng, phần mềm vẫn sẽ in ra:
> - `aX ≈ 0g` (Forward)
> - `aY ≈ 0g` (Left)
> - `aZ ≈ +1g` (Up)
> 
> Bạn vẫn dùng mức `aZ ≈ +1g` trên màn hình Serial để xác minh mình đã gắn đúng chiều hay chưa.

---

## 4. Nguyên Tắc Lắp Đặt Cơ Học vào Wearable

### 4.1. Độ cứng vững (Rigid Coupling) — QUAN TRỌNG NHẤT
- Cảm biến **phải dính chặt** vào cơ thể hoặc quần áo. Bất kỳ sự trượt/rung lắc vi mô nào giữa chip và da/vải đều tạo ra "motion artifact" — nhiễu không thể lọc bỏ sau này.
- **Cách gắn được khuyên dùng (thứ tự ưu tiên):**
  1. Tích hợp vào đai lưng (belt) hoặc túi quần có dây buộc chắc
  2. Băng elastic có ma sát cao (non-slip elastic band)
  3. Keo dán y tế (medical-grade adhesive patch) cho thử nghiệm ngắn hạn
  4. Túi vải may vào quần áo

### 4.2. Vật liệu tiếp xúc da
- Ưu tiên vật liệu **thoáng khí, không gây dị ứng** (hypoallergenic)
- Tránh kim loại tiếp xúc trực tiếp da → ngứa, dị ứng
- Nếu module GY-521 có chân nhô ra, bọc foam/silicon để tránh tổn thương da

### 4.3. Trọng lượng và hình dạng
- Wearable phải **nhẹ và mỏng**, không gây cảm giác vướng víu
- Trọng tâm cân bằng để thiết bị không tự nghiêng hay xoay trong quá trình đeo

---

## 5. Giao Thức Thu Thập Dữ Liệu cho Edge Impulse

### 5.1. Cấu hình tần số lấy mẫu

| Thông số | Giá trị khuyên dùng | Lý do |
|---------|--------|-------|
| **Sampling Rate** | **100 Hz** | Đủ để bắt cú té ngã nhanh (~300ms), tiêu chuẩn nghiên cứu |
| **Window Size** | 2000 – 4000 ms | Bao phủ đủ chu kỳ: nghiêng → va chạm → bất động |
| **Window Stride** | 500 ms | Tăng dữ liệu mà không cần thu thêm |

> [!WARNING]
> **Tần số trên Arduino PHẢI khớp tần số cấu hình trên Edge Impulse.** Nếu Arduino ghi 100Hz nhưng Edge Impulse đặt 50Hz, toàn bộ features tần số sẽ bị sai lệch nghiêm trọng.

### 5.2. Nhãn dữ liệu (Labels) cần thu thập

**Lớp FALL (Té ngã):**
- `fall_forward` — Té ngã về phía trước
- `fall_backward` — Té ngã về phía sau  
- `fall_left` — Té ngã sang trái
- `fall_right` — Té ngã sang phải

**Lớp ADL — Activities of Daily Living (Không té ngã):**
- `walk` — Đi bộ bình thường
- `run` — Chạy bộ
- `sit_to_stand` — Đứng dậy từ ghế
- `stand_to_sit` — Ngồi xuống ghế
- `bend_pickup` — Cúi nhặt đồ vật
- `jump` — Nhảy (hay gây false positive nhất!)
- `lie_down` — Nằm xuống từ từ (không phải té)

> [!NOTE]
> **Lie down** và **Fall** có biên dạng gia tốc khá giống nhau. Thu thập nhiều mẫu `lie_down` giúp model phân biệt được "nằm xuống chủ ý" vs "té ngã".

### 5.3. Quy trình thu thập từng nhãn

1. Đeo wearable đúng vị trí và hướng đã chuẩn
2. Đứng yên 2-3 giây (trạng thái chuẩn)
3. Thực hiện hành động (té ngã / ADL)
4. Giữ nguyên tư thế sau khi té ít nhất 2-3 giây trước khi dừng ghi
5. Mỗi nhãn cần tối thiểu **20-30 mẫu** (nhiều người thực hiện nếu có thể)

> [!TIP]
> **Dùng nhiều người thực hiện thu thập** sẽ giúp model tổng quát hơn (robust), không overfit vào cách di chuyển của một người duy nhất.

---

## 6. Checklist Trước khi Record Data

- [ ] Mạch dán NẰM PHẲNG áp sát vào bụng, mặt linh kiện hướng ra ngoài.
- [ ] Chân cắm (Header pins) của mạch hướng thẳng XUỐNG ĐẤT.
- [ ] Mở Serial Monitor check: `aZ ≈ +1.0g` khi đứng thẳng, không cử động.
- [ ] Wearable được gắn chắc ở eo, lắc nhẹ không xê dịch.
- [ ] Tần số đọc dữ liệu trên Arduino = Tần số cài trên Edge Impulse (100Hz)
- [ ] Đã có ít nhất 4 nhãn fall + ít nhất 5 nhãn ADL để cân bằng dataset
- [ ] Góc và vị trí lắp đặt đã chụp hình lưu lại để tái lắp nhất quán

---

## 7. Tóm Tắt Kiến Trúc Hệ Thống

```
[MPU6050 tại eo]
      │ I2C (100 Hz)
      ▼
[ESP32-S3]
      │ Serial / WiFi / BLE
      ▼
[Edge Impulse Data Forwarder]
      │
      ▼
[Edge Impulse Studio]
  - Spectral Analysis features
  - Keras Classifier
  - Window: 2000ms @ 100Hz
      │
      ▼
[Deployed Model trên ESP32-S3]
  - Inference tại chỗ
  - Gửi cảnh báo Telegram khi phát hiện té ngã
```
