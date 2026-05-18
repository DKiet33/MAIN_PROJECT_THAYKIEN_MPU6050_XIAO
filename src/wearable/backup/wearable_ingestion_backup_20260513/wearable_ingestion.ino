#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <stdarg.h>

#include "ei_config_ingestion.h"

// ════════════════════════════════════════════════════════════════
// File: wearable_ingestion.ino
// Purpose: Thu thập dữ liệu IMU (100Hz) → Upload trực tiếp lên
//          Edge Impulse Ingestion API qua WiFi, KHÔNG cần cáp USB
// Board: Seeed XIAO ESP32S3
// Sensor: MPU6050 / GY-521
// Author: Senior Dev + Codex
// Date: 2026-05-12
//
// ── Tính năng ────────────────────────────────────────────────────
//   • Trigger bằng nút BOOT vật lý HOẶC nút trên Web Dashboard
//   • Web Dashboard realtime tại http://<IP thiết bị>
//       - Live IMU (aX, aY, aZ, |a|) cập nhật 20Hz khi READY
//       - Progress bar thu mẫu (0 → EI_SAMPLE_COUNT)
//       - Lịch sử upload + trạng thái lần gần nhất
//   • HTTPS Chunked Stream — không cấp phát heap lớn
//   • State machine 5 trạng thái (READY/COLLECTING/UPLOADING/OK/ERR)
//
// ── Cách dùng ────────────────────────────────────────────────────
//   1. Điền EI_API_KEY và EI_LABEL trong ei_config_ingestion.h
//      Labels hiện tại: "idle" | "walk" | "fall"
//   2. Upload firmware qua USB (1 lần duy nhất)
//   3. Cấp nguồn → kết nối WiFi → mở http://<IP> trên trình duyệt
//   4. Nhấn BOOT hoặc nút web để bắt đầu thu 3 giây dữ liệu
//   5. Thiết bị tự upload lên Edge Impulse Studio qua WiFi
//
// ── LED Status ───────────────────────────────────────────────────
//   Nháy chậm 1Hz   → READY (chờ lệnh)
//   Nháy nhanh 10Hz → COLLECTING (đang thu mẫu)
//   Sáng liên tục   → UPLOADING (đang gửi lên EI)
//   5 nháy chậm     → DONE OK
//   10 nháy nhanh   → DONE ERROR (kiểm tra API Key / WiFi)
//
// ── Axis convention (giống wearable.ino) ─────────────────────────
// Mạch NẰM PHẲNG trên bụng, chân cắm hướng xuống đất:
// - Trục Z vật lý → Body Forward (aX)
// - Trục X vật lý → Body Left   (aY)
// - Trục Y vật lý → Body Up     (aZ) ← kỳ vọng +1g khi đứng thẳng
// ════════════════════════════════════════════════════════════════

// ╔══════════════════════════════════════════════════════════════╗
// ║                         STRUCTS                              ║
// ╚══════════════════════════════════════════════════════════════╝

enum SensorStatus { SENSOR_OK = 0, SENSOR_ERROR };

struct ImuData {
  float aX, aY, aZ;
  float gX, gY, gZ;
  SensorStatus status;
};

enum State { READY, CALIBRATING, COLLECTING, UPLOADING, DONE_OK, DONE_ERR };

// ╔══════════════════════════════════════════════════════════════╗
// ║                          GLOBALS                             ║
// ╚══════════════════════════════════════════════════════════════╝

State state = READY;
bool mpuConnected = false;
bool lastUploadOk = false;
char lastUploadTime[32] = "--";
char currentSampleName[32] = "";
// Runtime label — có thể đổi qua Web UI mà không cần reflash
char activeLabel[32] = EI_LABEL;

// Sample buffer — 300 × 6 floats = 7.2 KB
float sampleBuf[EI_SAMPLE_COUNT][EI_NUM_CHANNELS];
int samplesCollected = 0;

unsigned long sampleTimer = 0;
static const unsigned long SAMPLE_INTERVAL_US = 1000000UL / EI_SAMPLING_FREQ_HZ;

// Live IMU snapshot dùng cho web (volatile — safe atomic float trên ESP32)
volatile float web_aX = 0, web_aY = 0, web_aZ = 0;
volatile float web_gX = 0, web_gY = 0, web_gZ = 0;
volatile float web_totalG = 0;

// ── Calibration offsets (reset mỗi boot, đổi qua /calibrate) ────────────
#define CAL_SAMPLES 200
float off_aX = 0, off_aY = 0, off_aZ = 0; // đơn vị g
float off_gX = 0, off_gY = 0, off_gZ = 0; // đơn vị deg/s
bool calibrated = false;
float calSumAX = 0, calSumAY = 0, calSumAZ = 0;
float calSumGX = 0, calSumGY = 0, calSumGZ = 0;
int calCount = 0;

// ── Auto-run: thu nhiều batch liên tiếp tự động ──────────────────────
bool autoRunActive = false;
int autoRunTotal = 0; // số batch cần thu
int autoRunDone = 0;  // số batch đã upload thành công

// ── Augmentation Scale (Phương án 2): upload 3x với hệ số biên độ ──
bool augmentMode = false; // bật/tắt qua nút web
int augUploadIdx = 0;     // 0=gốc, 1=×0.94, 2=×1.06
// Hệ số scale: gốc / ngã nhẹ hơn 6% / ngã mạnh hơn 6%
const float AUG_SCALES[3] = {1.00f, 0.94f, 1.06f};

// ── Augmentation Jitter (Phương án 1): thêm nhiễu trắng ngẫu nhiên ──
bool jitterMode = false; // bật/tắt qua nút web
// Biên độ nhiễu: ±0.02g cho accel, ±1.0 deg/s cho gyro
#define JITTER_ACCEL 0.02f
#define JITTER_GYRO 1.00f

// ── Manual Upload: upload file CSV từ trình duyệt lên EI thủ công ──────
bool manualUploadActive = false; // đang trong chu kì upload thủ công
bool manualSavedAugMode = false; // lưu trạng thái augment cũ
bool manualSavedJitter = false;  // lưu trạng thái jitter cũ

// WebServer HTTP trên port 80
WebServer server(80);

// Ring buffer Serial log cho web (/log endpoint)
#define WEB_LOG_LINES 30
#define WEB_LOG_LEN 96
static char webLog[WEB_LOG_LINES][WEB_LOG_LEN];
static int webLogHead = 0;
static int webLogCount = 0;

void logPrint(const char *msg) {
  Serial.print(msg);
  // Chia theo dấu '\n' vào từng slot
  const char *start = msg;
  while (*start) {
    const char *nl = strchr(start, '\n');
    int len = nl ? (nl - start) : (int)strlen(start);
    if (len > 0) {
      int n = len < WEB_LOG_LEN - 1 ? len : WEB_LOG_LEN - 1;
      memcpy(webLog[webLogHead], start, n);
      webLog[webLogHead][n] = '\0';
      webLogHead = (webLogHead + 1) % WEB_LOG_LINES;
      if (webLogCount < WEB_LOG_LINES)
        webLogCount++;
    }
    if (!nl)
      break;
    start = nl + 1;
  }
}

void logPrintf(const char *fmt, ...) {
  static char _buf[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(_buf, sizeof(_buf), fmt, ap);
  va_end(ap);
  logPrint(_buf);
}

#include "html_page.h" // HTML Dashboard

const char *stateStr() {
  switch (state) {
  case READY:
    return "READY";
  case CALIBRATING:
    return "CALIBRATING";
  case COLLECTING:
    return "COLLECTING";
  case UPLOADING:
    return "UPLOADING";
  case DONE_OK:
    return "DONE_OK";
  case DONE_ERR:
    return "DONE_ERR";
  }
  return "READY";
}

void handleRoot() { server.send_P(200, "text/html", HTML_PAGE); }

void handleLog() {
  // Trả về 30 dòng log gần nhất dưới dạng JSON array
  static char buf[3200];
  int pos = 0;
  pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"lines\":[");
  int start = webLogCount < WEB_LOG_LINES ? 0 : webLogHead; // oldest slot
  int count = webLogCount < WEB_LOG_LINES ? webLogHead : WEB_LOG_LINES;
  bool first = true;
  for (int i = 0; i < count && pos < (int)sizeof(buf) - 8; i++) {
    int idx = (start + i) % WEB_LOG_LINES;
    if (!first)
      buf[pos++] = ',';
    first = false;
    buf[pos++] = '"';
    for (const char *p = webLog[idx]; *p && pos < (int)sizeof(buf) - 6; p++) {
      if (*p == '"' || *p == '\\')
        buf[pos++] = '\\';
      if (*p != '\r')
        buf[pos++] = *p;
    }
    buf[pos++] = '"';
  }
  snprintf(buf + pos, sizeof(buf) - pos, "]}");
  server.send(200, "application/json", buf);
}

void handleStatus() {
  static char buf[900];
  snprintf(buf, sizeof(buf),
           "{\"state\":\"%s\",\"label\":\"%s\","
           "\"calibrated\":%s,\"calPct\":%d,"
           "\"aX\":%.3f,\"aY\":%.3f,\"aZ\":%.3f,\"totalG\":%.3f,"
           "\"samples\":%d,\"total\":%d,"
           "\"autoActive\":%s,\"autoTotal\":%d,\"autoDone\":%d,"
           "\"augment\":%s,\"augIdx\":%d,"
           "\"jitter\":%s,"
           "\"manualUpload\":%s,"
           "\"lastUpload\":\"%s\",\"ip\":\"%s\"}",
           stateStr(), activeLabel, calibrated ? "true" : "false",
           (calCount * 100) / CAL_SAMPLES, (float)web_aX, (float)web_aY,
           (float)web_aZ, (float)web_totalG, samplesCollected, EI_SAMPLE_COUNT,
           autoRunActive ? "true" : "false", autoRunTotal, autoRunDone,
           augmentMode ? "true" : "false", augUploadIdx,
           jitterMode ? "true" : "false", manualUploadActive ? "true" : "false",
           lastUploadTime, WiFi.localIP().toString().c_str());
  server.send(200, "application/json", buf);
}

void handleSetAugment() {
  augmentMode = !augmentMode;
  if (!augmentMode)
    augUploadIdx = 0;
  logPrintf("[AUG] Scale 3x: %s\n", augmentMode ? "ON" : "OFF");
  server.send(200, "text/plain", augmentMode ? "on" : "off");
}

void handleSetJitter() {
  jitterMode = !jitterMode;
  logPrintf("[AUG] Jitter: %s (+/-%.2fg / +/-%.1fdeg/s)\n",
            jitterMode ? "ON" : "OFF", JITTER_ACCEL, JITTER_GYRO);
  server.send(200, "text/plain", jitterMode ? "on" : "off");
}

void handleUploadManual() {
  if (state != READY) {
    server.send(400, "text/plain", "busy");
    return;
  }
  String label =
      server.hasArg("label") ? server.arg("label") : String(activeLabel);
  label.trim();
  if (label.length() > 0 && label.length() < sizeof(activeLabel))
    label.toCharArray(activeLabel, sizeof(activeLabel));
  bool doScale = server.hasArg("scale") && server.arg("scale") == "1";
  bool doJitter = server.hasArg("jitter") && server.arg("jitter") == "1";

  // ── Parse CSV body vao sampleBuf ──────────────────────────────
  // Ho tro ca dinh dang 6-col (accX,aY,aZ,gX,gY,gZ)
  // va 7-col (timestamp,accX,aY,aZ,gX,gY,gZ)
  String body = server.arg("plain");
  samplesCollected = 0;
  int lineStart = 0;
  while (lineStart < (int)body.length() && samplesCollected < EI_SAMPLE_COUNT) {
    int lineEnd = body.indexOf('\n', lineStart);
    if (lineEnd < 0)
      lineEnd = body.length();
    String line = body.substring(lineStart, lineEnd);
    line.trim();
    lineStart = lineEnd + 1;
    if (line.length() == 0 || line[0] == '#')
      continue; // skip header/comment

    // dem dau phay de xac dinh co truong timestamp dau hay khong
    int commas = 0;
    for (int i = 0; i < (int)line.length(); i++)
      if (line[i] == ',')
        commas++;
    int skipCols = (commas >= 6) ? 1 : 0; // bo cot timestamp neu co 7 cot

    float vals[6] = {};
    int col = 0, dataCol = 0, start = 0;
    bool valid = true;
    for (int i = 0; i <= (int)line.length() && dataCol < 6; i++) {
      if (i == (int)line.length() || line[i] == ',') {
        if (col >= skipCols) {
          String tok = line.substring(start, i);
          tok.trim();
          if (tok.length() == 0) {
            valid = false;
            break;
          }
          vals[dataCol++] = tok.toFloat();
        }
        col++;
        start = i + 1;
      }
    }
    if (!valid || dataCol < 6)
      continue;
    for (int c = 0; c < EI_NUM_CHANNELS; c++)
      sampleBuf[samplesCollected][c] = vals[c];
    samplesCollected++;
  }

  if (samplesCollected == 0) {
    server.send(400, "text/plain", "Không có dữ liệu — kiểm tra định dạng CSV");
    return;
  }
  logPrintf("[MAN] %d dòng CSV, nhãn='%s' scale=%d jitter=%d\n",
            samplesCollected, activeLabel, doScale, doJitter);

  // Luu trang thai augment hien tai, ghi de bang cai dat thu cong
  manualSavedAugMode = augmentMode;
  manualSavedJitter = jitterMode;
  augmentMode = doScale;
  jitterMode = doJitter;
  augUploadIdx = 0;
  manualUploadActive = true;
  state = UPLOADING;
  server.send(200, "text/plain", "ok");
}

void handleTrigger() {
  if (state == READY) {
    samplesCollected = 0;
    sampleTimer = micros();
    state = COLLECTING;
    logPrintf("[WEB] Trigger! Thu mẫu label='%s'...\n", activeLabel);
  }
  server.send(200, "text/plain", "ok");
}

void handleSetLabel() {
  // Chỉ cho phép đổi label khi đang READY
  if (state != READY) {
    server.send(400, "text/plain", "busy");
    return;
  }
  if (server.hasArg("label")) {
    String l = server.arg("label");
    l.trim();
    if (l.length() > 0 && l.length() < sizeof(activeLabel)) {
      l.toCharArray(activeLabel, sizeof(activeLabel));
      logPrintf("[WEB] Label đổi sang '%s'\n", activeLabel);
    }
  }
  server.send(200, "text/plain", activeLabel);
}

void handleCalibrate() {
  if (state != READY) {
    server.send(400, "text/plain", "busy");
    return;
  }
  // Reset offsets về 0 để imuRead() trả raw trong quá trình thu
  off_aX = off_aY = off_aZ = 0;
  off_gX = off_gY = off_gZ = 0;
  calibrated = false;
  calSumAX = calSumAY = calSumAZ = 0;
  calSumGX = calSumGY = calSumGZ = 0;
  calCount = 0;
  sampleTimer = micros();
  state = CALIBRATING;
  logPrintf("[CAL] Bắt đầu. Giữ yên tư thế đứng thẳng (%d mẫu)...\n",
            CAL_SAMPLES);
  server.send(200, "text/plain", "ok");
}

void handleAutoRun() {
  if (state != READY) {
    server.send(400, "text/plain", "busy");
    return;
  }
  int count = server.hasArg("count") ? server.arg("count").toInt() : 1;
  if (count < 1)
    count = 1;
  if (count > 500)
    count = 500; // giới hạn an toàn
  autoRunTotal = count;
  autoRunDone = 0;
  autoRunActive = true;
  samplesCollected = 0;
  sampleTimer = micros();
  state = COLLECTING;
  logPrintf("[AUTO] Bắt đầu %d batches, label='%s'\n", autoRunTotal,
            activeLabel);
  server.send(200, "text/plain", "ok");
}

void handleStopRun() {
  autoRunActive = false;
  logPrintf("[AUTO] Dừng auto-run (%d/%d xong).\n", autoRunDone, autoRunTotal);
  server.send(200, "text/plain", "stopped");
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                        LED HELPERS                           ║
// ╚══════════════════════════════════════════════════════════════╝

void ledOn() { digitalWrite(LED_PIN, LOW); } // Active LOW trên XIAO
void ledOff() { digitalWrite(LED_PIN, HIGH); }

void blinkLed(int times, int periodMs) {
  for (int i = 0; i < times; i++) {
    ledOn();
    delay(periodMs / 2);
    ledOff();
    delay(periodMs / 2);
  }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                           MPU6050                            ║
// ╚══════════════════════════════════════════════════════════════╝

void writeMPU6050Register(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

bool initMPU6050() {
  Wire.beginTransmission(MPU6050_I2C_ADDR);
  if (Wire.endTransmission() != 0)
    return false;
  writeMPU6050Register(0x6B, 0x00); // Wake up
  delay(10);
  writeMPU6050Register(0x1A, 0x03); // DLPF 44 Hz
  writeMPU6050Register(0x1B, 0x08); // Gyro ±500°/s
  writeMPU6050Register(0x1C, 0x10); // Accel ±8g
  return true;
}

// Rotation matrix — đồng nhất với wearable.ino
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
  Wire.read(); // skip temperature
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  float pAX = ax / ACCEL_SCALE, pAY = ay / ACCEL_SCALE, pAZ = az / ACCEL_SCALE;
  float pGX = gx / GYRO_SCALE, pGY = gy / GYRO_SCALE, pGZ = gz / GYRO_SCALE;

  d.aX = pAZ - off_aX;
  d.aY = pAX - off_aY;
  d.aZ = pAY - off_aZ; // Z→Fwd, X→Left, Y→Up; trừ offset trọng lực
  d.gX = pGZ - off_gX;
  d.gY = pGX - off_gY;
  d.gZ = pGY - off_gZ;
  d.status = SENSOR_OK;
  return d;
}

// Đọc IMU 1 lần và cập nhật snapshot hiển thị web — gọi khi UPLOADING
// để XYZ/G không bị đơ trên UI.
inline void refreshImuSnapshot() {
  ImuData snap = imuRead();
  if (snap.status == SENSOR_OK) {
    web_aX = snap.aX;
    web_aY = snap.aY;
    web_aZ = snap.aZ;
    web_gX = snap.gX;
    web_gY = snap.gY;
    web_gZ = snap.gZ;
    web_totalG =
        sqrtf(snap.aX * snap.aX + snap.aY * snap.aY + snap.aZ * snap.aZ);
  }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          HTTPS UPLOAD — Edge Impulse Ingestion API           ║
// ╚══════════════════════════════════════════════════════════════╝
// Dùng Content-Length (không chunked) — EI server không hỗ trợ
// Transfer-Encoding: chunked ở phía request body.
// Static buffer 22 KB: 300 mẫu × 6 floats × ~12 chars ≈ 21.6 KB

WiFiClientSecure httpsClient;

// Buffer tĩnh build JSON toàn bộ trước khi gửi
// ESP32S3 có 512 KB SRAM — 22 KB là hoàn toàn ổn
static char jsonPayload[22000];

// Hàm tạo số ngẫu nhiên float trong [-range, +range] dùng LCG đơn giản
static uint32_t _lcgState = 12345;
float randF(float range) {
  _lcgState = _lcgState * 1664525UL + 1013904223UL;
  float r = (float)(_lcgState & 0xFFFF) / 65535.0f; // [0, 1]
  return (r * 2.0f - 1.0f) * range;                 // [-range, +range]
}

// Build JSON payload vào jsonPayload[], scale=hệ số nhân biên độ (1.0=gốc)
// jitter=true → cộng nhiễu trắng ngẫu nhiên vào từng sample
int buildJson(float scale, bool jitter = false) {
  int pos = 0;
  char tmp[20];

  // Header (constant)
  const char *hdr =
      "{\"protected\":{\"ver\":\"v1\",\"alg\":\"none\",\"iat\":0},"
      "\"signature\":\"0\","
      "\"payload\":{"
      "\"device_name\":\"xiao-esp32s3\","
      "\"device_type\":\"XIAO_ESP32S3\","
      "\"interval_ms\":10.0,"
      "\"sensors\":["
      "{\"name\":\"accX\",\"units\":\"g\"},"
      "{\"name\":\"accY\",\"units\":\"g\"},"
      "{\"name\":\"accZ\",\"units\":\"g\"},"
      "{\"name\":\"gyroX\",\"units\":\"deg/s\"},"
      "{\"name\":\"gyroY\",\"units\":\"deg/s\"},"
      "{\"name\":\"gyroZ\",\"units\":\"deg/s\"}"
      "],\"values\":[";
  int hlen = strlen(hdr);
  if (pos + hlen >= (int)sizeof(jsonPayload) - 10)
    return -1;
  memcpy(jsonPayload + pos, hdr, hlen);
  pos += hlen;

  // Values — nhân toàn bộ giá trị với hệ số scale (augmentation)
  for (int i = 0; i < samplesCollected; i++) {
    if (i > 0 && pos < (int)sizeof(jsonPayload) - 80)
      jsonPayload[pos++] = ',';
    jsonPayload[pos++] = '[';
    for (int c = 0; c < EI_NUM_CHANNELS; c++) {
      if (c > 0)
        jsonPayload[pos++] = ',';
      float v = sampleBuf[i][c] * scale;
      if (jitter) {
        // Cộng nhiễu: ±JITTER_ACCEL cho 3 kênh acc, ±JITTER_GYRO cho 3 kênh
        // gyro
        v += (c < 3) ? randF(JITTER_ACCEL) : randF(JITTER_GYRO);
      }
      int n = snprintf(tmp, sizeof(tmp), "%.4f", v);
      memcpy(jsonPayload + pos, tmp, n);
      pos += n;
    }
    jsonPayload[pos++] = ']';
  }

  // Footer
  const char *ftr = "]}}";
  memcpy(jsonPayload + pos, ftr, 3);
  pos += 3;
  jsonPayload[pos] = '\0';
  return pos;
}

// scale: hệ số nhân biên độ (augmentation); augIdx: thêm vào tên file để unique
// jitter=true → áp dụng nhiễu trắng khi build JSON
bool uploadToEdgeImpulse(float scale = 1.0f, int augIdx = 0,
                         bool jitter = false) {
  Serial.printf("[EI] Build JSON (scale=%.2f, jitter=%d, idx=%d)...\n", scale,
                jitter, augIdx);
  int jsonLen = buildJson(scale, jitter);
  if (jsonLen <= 0) {
    Serial.println("[EI] FAIL: Buffer quá nhỏ!");
    return false;
  }
  Serial.printf("[EI] JSON: %d bytes. Kết nối server...\n", jsonLen);

  httpsClient.setInsecure();
  if (!httpsClient.connect("ingestion.edgeimpulse.com", 443)) {
    Serial.println("[EI] FAIL: Không kết nối được server!");
    return false;
  }

  // ── HTTP Request — Content-Length (không chunked) ──────────────
  httpsClient.println("POST /api/training/data HTTP/1.1");
  httpsClient.println("Host: ingestion.edgeimpulse.com");
  unsigned long ts = millis();
  // augIdx != 0 → thêm hậu tố a1/a2 vào tên file để EI không nhận dạng là trùng
  if (augIdx == 0)
    snprintf(currentSampleName, sizeof(currentSampleName), "%s-%lu",
             activeLabel, ts);
  else
    snprintf(currentSampleName, sizeof(currentSampleName), "%s-%lu-a%d",
             activeLabel, ts, augIdx);
  httpsClient.printf("x-api-key: %s\r\n", EI_API_KEY);
  httpsClient.printf("x-label: %s\r\n", activeLabel);
  httpsClient.printf("x-file-name: %s.json\r\n", currentSampleName);
  httpsClient.println("Content-Type: application/json");
  httpsClient.printf("Content-Length: %d\r\n", jsonLen);
  httpsClient.println("Connection: close");
  httpsClient.println();

  // Gửi body theo block 512 bytes để tránh watchdog reset
  int sent = 0;
  while (sent < jsonLen) {
    int block = min(512, jsonLen - sent);
    httpsClient.write((uint8_t *)(jsonPayload + sent), block);
    sent += block;
    delay(1);
    server.handleClient();
    refreshImuSnapshot(); // cập nhật XYZ/G để UI không đơ khi gửi body
  }

  Serial.println("[EI] Đã gửi. Chờ phản hồi...");

  // ── Đọc HTTP Status ────────────────────────────────────────────
  unsigned long t0 = millis();
  while (!httpsClient.available() && millis() - t0 < 10000) {
    delay(5);
    server.handleClient();
    refreshImuSnapshot(); // cập nhật XYZ/G để UI không đơ khi chờ response
  }

  String statusLine = httpsClient.readStringUntil('\n');
  Serial.printf("[EI] Server: %s\n", statusLine.c_str());

  bool ok = (statusLine.indexOf("200") >= 0);
  httpsClient.stop();
  return ok;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                            SETUP                             ║
// ╚══════════════════════════════════════════════════════════════╝

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  ledOff();

  Serial.println("\n[MAIN] === WEARABLE INGESTION v1 ===");
  Serial.printf("[MAIN] Label: %s | %d mẫu @ %d Hz\n", activeLabel,
                EI_SAMPLE_COUNT, EI_SAMPLING_FREQ_HZ);

  // ── I2C & MPU6050 ─────────────────────────────────────────────
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  mpuConnected = initMPU6050();
  if (!mpuConnected) {
    Serial.println("[MPU] FAIL — Kiểm tra dây nối!");
    blinkLed(20, 200); // Nháy báo lỗi liên tục
  } else {
    Serial.println("[MPU] OK — MPU6050 kết nối thành công.");
  }

  // Axis check
  delay(100);
  ImuData boot = imuRead();
  if (boot.status == SENSOR_OK) {
    Serial.printf("[AXIS] aZ = %.3fg — %s\n", boot.aZ,
                  (boot.aZ >= AXIS_Z_MIN && boot.aZ <= AXIS_Z_MAX)
                      ? "PASS"
                      : "WARN: Kiểm tra hướng gắn!");
  }

  // ── WiFi ─────────────────────────────────────────────────────
  Serial.printf("[WIFI] Kết nối %s...\n", EI_WIFI_SSID);
  WiFi.begin(EI_WIFI_SSID, EI_WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    logPrintf("[WIFI] OK! IP: %s\n", WiFi.localIP().toString().c_str());
    blinkLed(3, 200);
  } else {
    logPrintf("[WIFI] FAIL\n");
    blinkLed(10, 100);
  }

  // ── WebServer ─────────────────────────────────────────────────
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/trigger", HTTP_GET, handleTrigger);
  server.on("/log", HTTP_GET, handleLog);
  server.on("/setlabel", HTTP_GET, handleSetLabel);
  server.on("/calibrate", HTTP_GET, handleCalibrate);
  server.on("/autorun", HTTP_GET, handleAutoRun);
  server.on("/stoprun", HTTP_GET, handleStopRun);
  server.on("/setaugment", HTTP_GET, handleSetAugment);
  server.on("/setjitter", HTTP_GET, handleSetJitter);
  server.on("/upload-manual", HTTP_POST, handleUploadManual);
  server.begin();
  logPrintf("[WEB] Dashboard: http://%s\n", WiFi.localIP().toString().c_str());

  logPrintf("[MAIN] Sẵn sàng. Nhấn BOOT hoặc nút web để thu mẫu.\n");
  state = READY;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    LOOP — State Machine                      ║
// ╚══════════════════════════════════════════════════════════════╝

void loop() {
  static unsigned long ledTimer = 0;
  static bool ledState = false;

  switch (state) {

  // ── READY: LED nháy chậm 1Hz, chờ nhấn BOOT ──────────────────
  case READY:
    if (millis() - ledTimer > 500) {
      ledTimer = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    }
    // Debounce button (active LOW)
    if (digitalRead(BTN_PIN) == LOW) {
      delay(50);
      if (digitalRead(BTN_PIN) == LOW) {
        while (digitalRead(BTN_PIN) == LOW)
          delay(10); // chờ nhả
        samplesCollected = 0;
        sampleTimer = micros();
        state = COLLECTING;
        Serial.printf("[REC] BẮT ĐẦU thu mẫu label='%s'...\n", activeLabel);
      }
    }
    break;

  // ── CALIBRATING: LED 5Hz blink, thu CAL_SAMPLES mẫu raw ─────
  case CALIBRATING:
    if (millis() - ledTimer > 100) {
      ledTimer = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    }
    if ((micros() - sampleTimer) >= SAMPLE_INTERVAL_US) {
      sampleTimer = micros();
      ImuData d = imuRead(); // offsets = 0 → đọc raw
      if (d.status == SENSOR_OK) {
        calSumAX += d.aX;
        calSumAY += d.aY;
        calSumAZ += d.aZ;
        calSumGX += d.gX;
        calSumGY += d.gY;
        calSumGZ += d.gZ;
        calCount++;
      }
      if (calCount >= CAL_SAMPLES) {
        off_aX = calSumAX / calCount;
        off_aY = calSumAY / calCount;
        off_aZ = calSumAZ / calCount - 1.0f; // trừ 1g trọng lực
        off_gX = calSumGX / calCount;
        off_gY = calSumGY / calCount;
        off_gZ = calSumGZ / calCount;
        calibrated = true;
        logPrintf("[CAL] Xong! offA: %.3f %.3f %.3f | offG: %.3f %.3f %.3f\n",
                  off_aX, off_aY, off_aZ, off_gX, off_gY, off_gZ);
        blinkLed(3, 150);
        state = READY;
      }
    }
    server.handleClient();
    break;

  // ── COLLECTING: LED nháy nhanh 10Hz, thu 100Hz ──────────────
  case COLLECTING:
    // LED 10Hz blink
    if (millis() - ledTimer > 50) {
      ledTimer = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    }

    // Thu mẫu đúng 100Hz bằng micros()
    if ((micros() - sampleTimer) >= SAMPLE_INTERVAL_US) {
      sampleTimer = micros();
      ImuData d = imuRead();
      if (d.status == SENSOR_OK && samplesCollected < EI_SAMPLE_COUNT) {
        sampleBuf[samplesCollected][0] = d.aX;
        sampleBuf[samplesCollected][1] = d.aY;
        sampleBuf[samplesCollected][2] = d.aZ;
        sampleBuf[samplesCollected][3] = d.gX;
        sampleBuf[samplesCollected][4] = d.gY;
        sampleBuf[samplesCollected][5] = d.gZ;
        samplesCollected++;
        // Cập nhật web snapshot từ dữ liệu đã đọc (không cần đọc thêm)
        web_aX = d.aX;
        web_aY = d.aY;
        web_aZ = d.aZ;
        web_gX = d.gX;
        web_gY = d.gY;
        web_gZ = d.gZ;
        web_totalG = sqrtf(d.aX * d.aX + d.aY * d.aY + d.aZ * d.aZ);
      }
      if (samplesCollected >= EI_SAMPLE_COUNT) {
        Serial.printf("[REC] Đủ %d mẫu — chuyển sang Upload.\n",
                      samplesCollected);
        ledOn();
        state = UPLOADING;
      }
    }
    // Gọi handleClient() giữa 2 mẫu — an toàn vì timing dùng micros()
    server.handleClient();
    break;

  // ── UPLOADING: LED sáng liên tục, POST lên EI ───────────────
  case UPLOADING:
    ledOn();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[EI] Mất WiFi — thử kết nối lại...");
      WiFi.reconnect();
      delay(3000);
    }
    {
      float sc = augmentMode ? AUG_SCALES[augUploadIdx] : 1.0f;
      bool jit = jitterMode; // Phương án 1: jitter độc lập với scale
      bool ok = uploadToEdgeImpulse(sc, augUploadIdx, jit);
      if (!ok) {
        augUploadIdx = 0;
        state = DONE_ERR;
      } else if (augmentMode && augUploadIdx < 2) {
        augUploadIdx++;
        logPrintf("[AUG] Scale upload %d/3 xong — x%.2f%s\n", augUploadIdx,
                  AUG_SCALES[augUploadIdx - 1], jit ? " + jitter" : "");
        // ở lại UPLOADING để upload tiếp bản augment kế
      } else {
        augUploadIdx = 0;
        state = DONE_OK;
      }
    }
    break;

  // ── DONE OK: báo thành công, tiếp batch kế (nếu auto-run) ──────
  case DONE_OK:
    ledOff();
    lastUploadOk = true;
    snprintf(lastUploadTime, sizeof(lastUploadTime), "%s OK",
             currentSampleName);
    Serial.printf("[EI] THÀNH CÔNG! Mẫu '%s' đã lên Edge Impulse Studio.\n",
                  currentSampleName);
    if (autoRunActive) {
      // Auto-run: blink ngắn để không block lâu giữa các batch
      blinkLed(2, 120);
      autoRunDone++;
      if (autoRunDone < autoRunTotal) {
        server.handleClient(); // trả lời web trong lúc chuyển batch
        samplesCollected = 0;
        sampleTimer = micros();
        state = COLLECTING;
        logPrintf("[AUTO] Batch %d/%d...\n", autoRunDone + 1, autoRunTotal);
      } else {
        autoRunActive = false;
        logPrintf("[AUTO] Xong! Đã thu đủ %d/%d batches.\n", autoRunDone,
                  autoRunTotal);
        state = READY;
      }
    } else {
      blinkLed(5, 300);
      if (manualUploadActive) {
        // Khoi phuc cai dat augment truoc khi upload thu cong
        manualUploadActive = false;
        augmentMode = manualSavedAugMode;
        jitterMode = manualSavedJitter;
      }
      state = READY;
    }
    break;

  // ── DONE ERR: nhay 10x nhanh, ve READY ───────────────────────
  case DONE_ERR:
    ledOff();
    lastUploadOk = false;
    snprintf(lastUploadTime, sizeof(lastUploadTime), "%s ERR",
             currentSampleName);
    Serial.printf("[EI] THAT BAI! Mau '%s' upload loi.\n", currentSampleName);
    if (autoRunActive) {
      autoRunActive = false;
      logPrintf("[AUTO] Dung do loi upload (batch %d/%d).\n", autoRunDone + 1,
                autoRunTotal);
    }
    if (manualUploadActive) {
      manualUploadActive = false;
      augmentMode = manualSavedAugMode;
      jitterMode = manualSavedJitter;
    }
    blinkLed(10, 100);
    state = READY;
    break;
  }

  // handleClient: luôn gọi — trong COLLECTING an toàn vì timing dùng micros()
  if (state != UPLOADING) // trong uploadToEdgeImpulse đã gọi bên trong
    server.handleClient();

  // Cập nhật live IMU snapshot cho web khi READY (COLLECTING tự cập nhật trong
  // vòng lấy mẫu)
  if (state == READY && mpuConnected) {
    static unsigned long lastWeb = 0;
    if (millis() - lastWeb >= 50) { // 20Hz refresh
      lastWeb = millis();
      ImuData d = imuRead();
      if (d.status == SENSOR_OK) {
        web_aX = d.aX;
        web_aY = d.aY;
        web_aZ = d.aZ;
        web_gX = d.gX;
        web_gY = d.gY;
        web_gZ = d.gZ;
        web_totalG = sqrtf(d.aX * d.aX + d.aY * d.aY + d.aZ * d.aZ);
      }
    }
  }
}
