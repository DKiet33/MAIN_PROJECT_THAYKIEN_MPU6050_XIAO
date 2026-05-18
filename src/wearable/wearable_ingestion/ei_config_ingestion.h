#pragma once

// ════════════════════════════════════════════════════════════════
// File: ei_config_ingestion.h
// Purpose: Cấu hình tập trung cho wearable_ingestion
//          Thu thập IMU → Upload thẳng lên Edge Impulse qua WiFi
// ════════════════════════════════════════════════════════════════

// ══════ WiFi (mạng nhà) ══════
#define EI_WIFI_SSID "PHAM HIEN"
#define EI_WIFI_PASSWORD "0353950060"

// ══════ Edge Impulse Project ══════
// Lấy API Key tại: Studio → Project → Dashboard → Keys → API Key
#define EI_API_KEY "ei_39e44825f24a58759b1d02ea3535cef846d6c23a8c431fd6"

// Nhãn cho buổi thu thập này — đổi trước mỗi loại hành động:
//   "idle", "walk", "fall"
#define EI_LABEL "idle"

// ══════ Sampling ══════
#define EI_SAMPLING_FREQ_HZ 100
#define EI_SAMPLE_COUNT 300 // 300 mẫu = 3 giây @ 100 Hz
#define EI_NUM_CHANNELS 6   // aX, aY, aZ, gX, gY, gZ

// ══════ MPU6050 ══════
#define MPU6050_I2C_ADDR 0x68
#define ACCEL_SCALE 4096.0f // ±8g
#define GYRO_SCALE 65.5f    // ±500°/s

// ══════ I2C — XIAO ESP32S3 ══════
#define I2C_SDA_PIN 5 // D4
#define I2C_SCL_PIN 6 // D5

// ══════ Hardware ══════
#define LED_PIN 21 // Orange LED trên XIAO ESP32S3
#define BTN_PIN 0  // BOOT button (active LOW)

// ══════ Serial ══════
#define SERIAL_BAUD 115200

// ══════ Axis Verify ══════
#define AXIS_Z_MIN 0.85f
#define AXIS_Z_MAX 1.15f
