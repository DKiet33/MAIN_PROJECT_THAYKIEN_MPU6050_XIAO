#pragma once

// ════════════════════════════════════════════════════════════════
// File: ei_config.h
// Purpose: Cấu hình tập trung cho wearable_unified
//          Thu thập IMU (Ingestion) + Suy luận AI real-time (Inference)
// Board: Seeed XIAO ESP32S3
// Sensor: MPU6050 / GY-521
// ════════════════════════════════════════════════════════════════

// ══════ WiFi ══════
#define EI_WIFI_SSID "PHAM HIEN"
#define EI_WIFI_PASSWORD "0353950060"

// ══════ Edge Impulse — Ingestion API ══════
// Lấy API Key tại: Studio → Project → Dashboard → Keys → API Key
#define EI_API_KEY "ei_39e44825f24a58759b1d02ea3535cef846d6c23a8c431fd6"

// Nhãn mặc định khi khởi động — có thể đổi qua Web UI:
//   "idle" | "walk" | "fall"
#define EI_LABEL "idle"

// ══════ Sampling (Ingestion) ══════
#define EI_SAMPLING_FREQ_HZ 100
#define EI_SAMPLE_COUNT 300 // 300 mẫu = 3 giây @ 100 Hz
#define EI_NUM_CHANNELS 6   // aX, aY, aZ, gX, gY, gZ

// ══════ MPU6050 ══════
#define MPU6050_I2C_ADDR 0x68
#define ACCEL_SCALE 4096.0f // ±8g
#define GYRO_SCALE 65.5f    // ±500 deg/s

// ══════ I2C — XIAO ESP32S3 ══════
#define I2C_SDA_PIN 5 // D4
#define I2C_SCL_PIN 6 // D5

// ══════ Hardware ══════
#define LED_PIN 21 // Orange LED trên XIAO ESP32S3 (active LOW)
#define BTN_PIN 0  // BOOT button (active LOW)

// ══════ Serial ══════
#define SERIAL_BAUD 115200

// ══════ Axis Verify ══════
#define AXIS_Z_MIN 0.85f
#define AXIS_Z_MAX 1.15f

// ══════ Inference ══════
// Ngưỡng xác suất để kích hoạt cảnh báo té ngã (0.0 – 1.0)
#define FALL_ALERT_THRESHOLD 0.85f
// Số slice liên tiếp phải vượt ngưỡng mới kích hoạt cảnh báo (chống rốc / giật
// thờ)
#define FALL_CONFIRM_SLICES 2
// Thời gian chờ sau mỗi cảnh báo trước khi cho phép cảnh báo tiếp theo (ms)
#define FALL_COOLDOWN_MS 6000
