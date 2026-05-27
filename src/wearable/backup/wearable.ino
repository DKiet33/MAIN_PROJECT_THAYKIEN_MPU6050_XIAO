#include <Arduino.h>
#include <Wire.h>

#include "ei_config.h"

// ════════════════════════════════════════════════════════════════
// File: wearable.ino
// Purpose: Thu thập dữ liệu IMU (MPU6050) cho Edge Impulse — Phase 1
// Board: Seeed XIAO ESP32S3
// Sensor: MPU6050 / GY-521
// Author: Senior Dev + Codex
// Date: 2026-04-28
//
// Arduino IDE Settings:
//   Board:         XIAO_ESP32S3
//   Upload Speed:  921600
//   USB Mode:      Hardware CDC and JTAG
//
// Wiring (xem docs\mpu6050_mounting_guide.md § 3):
//   MPU6050 VCC  → 3V3
//   MPU6050 GND  → GND
//   MPU6050 SDA  → D4 (GPIO5)
//   MPU6050 SCL  → D5 (GPIO6)
//   MPU6050 AD0  → GND  (I2C addr = 0x68)
//
// Output format (Edge Impulse Data Forwarder):
//   aX,aY,aZ,gX,gY,gZ  @ 100 Hz
//   aX/aY/aZ: gia tốc (g), gX/gY/gZ: góc quay (°/s)
//
// Axis convention (Đã map lại bằng phần mềm):
// Mạch NẰM PHẲNG trên bụng, chân cắm (header) hướng xuống đất.
// Áp dụng ma trận xoay để mạch nằm phẳng trên bụng, chân cắm hướng vào người:
// - Trục Z vật lý (hướng ra ngoài) → Body Forward (aX)
// - Trục X vật lý (hướng sang trái của người dùng) → Body Left (aY)
// - Trục Y vật lý (hướng lên đầu) → Body Up (aZ)
// ════════════════════════════════════════════════════════════════

// ╔══════════════════════════════════════════════════════════════╗
// ║                    ENUMS & STRUCTS                           ║
// ╚══════════════════════════════════════════════════════════════╝

enum SensorStatus {
  SENSOR_OK = 0,
  SENSOR_ERROR,
  SENSOR_AXIS_WARN // Acc_Z lệch quá xa ±1g → lắp đặt sai hướng
};

struct ImuData {
  float aX, aY, aZ; // Gia tốc (g)
  float gX, gY, gZ; // Góc quay (°/s)
  SensorStatus status;
};

// ╔══════════════════════════════════════════════════════════════╗
// ║                    GLOBAL OBJECTS                            ║
// ╚══════════════════════════════════════════════════════════════╝

bool mpuConnected = false;
unsigned long lastSampleTime = 0;

// Chu kỳ lấy mẫu tính từ tần số (microseconds)
static const unsigned long SAMPLE_INTERVAL_US =
    1000000UL / EI_SAMPLING_FREQ_HZ; // = 10000 µs @ 100 Hz

// ╔══════════════════════════════════════════════════════════════╗
// ║                    SENSOR FUNCTIONS                          ║
// ╚══════════════════════════════════════════════════════════════╝

void writeMPU6050Register(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

bool mpuBegin() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  Wire.beginTransmission(MPU6050_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
#if DEBUG_SERIAL
    Serial.println(
        "[MPU] FAIL — Kiểm tra dây nối và địa chỉ I2C (AD0=GND→0x68)");
#endif
    return false;
  }

  writeMPU6050Register(0x6B, 0x00); // Wake up
  delay(10);
  writeMPU6050Register(0x1A, 0x03); // DLPF 44Hz
  writeMPU6050Register(0x1B, 0x08); // Gyro ±500°/s
  writeMPU6050Register(0x1C, 0x10); // Accel ±8g

#if DEBUG_SERIAL
  Serial.println("[MPU] OK — MPU6050 khởi tạo thành công");
  Serial.printf("[MPU] Accel: ±8g | Gyro: ±500°/s | Filter: 44Hz\n");
#endif
  return true;
}

// Xác minh định hướng trục khi đứng thẳng
// Acc_Z phải ≈ +1.0g. Xem mpu6050_mounting_guide.md § Checklist
SensorStatus verifyAxisOrientation(float aZ) {
  if (aZ >= AXIS_VERIFY_ACCEL_Z_MIN && aZ <= AXIS_VERIFY_ACCEL_Z_MAX) {
    return SENSOR_OK;
  }
  return SENSOR_AXIS_WARN;
}

ImuData imuRead() {
  ImuData d = {};

  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) {
    d.status = SENSOR_ERROR;
    return d;
  }

  Wire.requestFrom((uint8_t)MPU6050_I2C_ADDR, (size_t)14);
  if (Wire.available() != 14) {
    d.status = SENSOR_ERROR;
    return d;
  }

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  Wire.read();
  Wire.read(); // Bỏ qua nhiệt độ
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  d.aX = ax / ACCEL_SCALE;
  d.aY = ay / ACCEL_SCALE;
  d.aZ = az / ACCEL_SCALE;

  d.gX = gx / GYRO_SCALE;
  d.gY = gy / GYRO_SCALE;
  d.gZ = gz / GYRO_SCALE;

  d.status = SENSOR_OK;
  return d;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    OUTPUT FUNCTIONS                          ║
// ╚══════════════════════════════════════════════════════════════╝

// Format CSV tương thích Edge Impulse Data Forwarder:
//   aX,aY,aZ,gX,gY,gZ
// Đơn vị: g và °/s — KHÔNG thêm label hay timestamp (EI tự xử lý)
void printEiCsv(const ImuData &d) {
  Serial.printf("%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\r\n", d.aX, d.aY, d.aZ, d.gX,
                d.gY, d.gZ);
}

#if DEBUG_SERIAL
void printDebug(const ImuData &d) {
  Serial.printf(
      "[IMU] aX=%.3fg aY=%.3fg aZ=%.3fg | gX=%.1f gY=%.1f gZ=%.1f °/s\n", d.aX,
      d.aY, d.aZ, d.gX, d.gY, d.gZ);
}
#endif

// ╔══════════════════════════════════════════════════════════════╗
// ║                    SETUP & LOOP                              ║
// ╚══════════════════════════════════════════════════════════════╝

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

#if DEBUG_SERIAL
  Serial.println(
      "\n[MAIN] ═══ WEARABLE — EDGE IMPULSE DATA COLLECTION (PHASE 1) ═══");
  Serial.println("[MAIN] Board: Seeed XIAO ESP32S3 | Sensor: MPU6050 (GY-521)");
  Serial.printf("[MAIN] Sampling: %d Hz | Channels: %d (aX aY aZ gX gY gZ)\n",
                EI_SAMPLING_FREQ_HZ, EI_NUM_CHANNELS);
#endif

  mpuConnected = mpuBegin();

  if (!mpuConnected) {
    // Không thoát — loop sẽ báo lỗi liên tục để dễ debug
    return;
  }

#if AXIS_VERIFY_ON_BOOT
  // Đọc mẫu đầu để kiểm tra định hướng trục
  delay(100); // cho sensor ổn định
  ImuData boot = imuRead();
  SensorStatus axisCheck = verifyAxisOrientation(boot.aZ);

  if (axisCheck == SENSOR_OK) {
#if DEBUG_SERIAL
    Serial.printf("[AXIS] aZ = %.3fg — Định hướng ĐÚNG (Z hướng lên)\n",
                  boot.aZ);
#endif
  } else {
#if DEBUG_SERIAL
    Serial.printf("[AXIS] aZ = %.3fg — Ngoài khoảng [%.2f, %.2f]\n", boot.aZ,
                  AXIS_VERIFY_ACCEL_Z_MIN, AXIS_VERIFY_ACCEL_Z_MAX);
    Serial.println("[AXIS] Kiểm tra lại hướng gắn sensor theo "
                   "mpu6050_mounting_guide.md § 3");
#endif
  }
#endif

#if DEBUG_SERIAL
  Serial.println("[MAIN] Bắt đầu stream dữ liệu...");
  Serial.println("[MAIN] Format: aX,aY,aZ,gX,gY,gZ  (g, °/s)");
  Serial.println("---");
#endif

  lastSampleTime = micros();
}

void loop() {
  if (!mpuConnected) {
#if DEBUG_SERIAL
    Serial.println("[ERROR] MPU6050 không kết nối được. Kiểm tra dây nối.");
#endif
    delay(2000);
    return;
  }

  // ── Timing chính xác bằng micros() thay vì millis() để đạt đúng 100 Hz ──
  unsigned long now = micros();
  if ((now - lastSampleTime) < SAMPLE_INTERVAL_US) {
    return;
  }
  lastSampleTime = now;

  ImuData d = imuRead();

  if (d.status == SENSOR_ERROR) {
#if DEBUG_SERIAL
    Serial.println("[ERROR] Đọc IMU thất bại — bỏ qua sample này");
#endif
    return;
  }

  // Output CSV — Edge Impulse Data Forwarder đọc dòng này
  printEiCsv(d);

#if DEBUG_SERIAL && 0
  // Bật DEBUG chi tiết: đổi && 0 thành && 1
  // (tắt mặc định vì làm chậm vòng lặp)
  printDebug(d);
#endif
}
