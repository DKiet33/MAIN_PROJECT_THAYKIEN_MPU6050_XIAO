#pragma once

// ════════════════════════════════════════════════════════════════
// File: ei_config.h
// Purpose: Cấu hình tập trung cho firmware wearable (XIAO ESP32S3 + MPU6050)
//          Phase 1 — Thu thập dữ liệu cho Edge Impulse
// Board: Seeed XIAO ESP32S3
// ════════════════════════════════════════════════════════════════

// ══════ Edge Impulse Data Forwarder ══════
// Tần số lấy mẫu PHẢI khớp với cấu hình trên Edge Impulse Studio
#define EI_SAMPLING_FREQ_HZ 100
#define EI_NUM_CHANNELS 6 // aX, aY, aZ, gX, gY, gZ

// ══════ MPU6050 ══════
#define MPU6050_I2C_ADDR 0x68 // AD0 nối GND → 0x68; AD0 nối 3V3 → 0x69

// Hệ số chuyển đổi raw → đơn vị vật lý
// Accel: Full Scale ±8g  → LSB/g = 4096
// Gyro:  Full Scale ±500°/s → LSB/(°/s) = 65.5
#define ACCEL_SCALE 4096.0f // raw / ACCEL_SCALE = gia tốc (g)
#define GYRO_SCALE 65.5f    // raw / GYRO_SCALE  = góc quay (°/s)

// Ngưỡng xác minh lắp đặt khi đứng thẳng (Acc_Z ≈ +1.0g)
// Xem mpu6050_mounting_guide.md § 3
#define AXIS_VERIFY_ACCEL_Z_MIN 0.85f
#define AXIS_VERIFY_ACCEL_Z_MAX 1.15f

// ══════ I2C — XIAO ESP32S3 ══════
// D4 = GPIO5 (SDA), D5 = GPIO6 (SCL)
// Tham chiếu: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
#define I2C_SDA_PIN 5
#define I2C_SCL_PIN 6

// ══════ Serial ══════
#define SERIAL_BAUD 115200

// ══════ Debug & Verification ══════
// Đặt = 1 để bật log debug qua Serial (tắt khi train EI để giảm latency)
#define DEBUG_SERIAL        0
// Đặt = 1 để kiểm tra hướng trục Acc_Z ≈ +1g ngay khi boot
#define AXIS_VERIFY_ON_BOOT 1
