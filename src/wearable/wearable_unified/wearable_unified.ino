// ════════════════════════════════════════════════════════════════
// File: wearable_unified.ino
// Purpose: Thu thập dữ liệu IMU (Ingestion) + Suy luận AI real-time
//          (Inference) trong 1 firmware duy nhất — chuyển chế độ
//          qua Web UI 2 tab, không cần reflash.
// Board: Seeed XIAO ESP32S3
// Sensor: MPU6050 / GY-521
// Date: 2026-05-13
//
// ── Chế độ hoạt động ─────────────────────────────────────────────
//   INGESTION: Thu mẫu IMU → Upload thẳng lên Edge Impulse qua WiFi
//   INFERENCE: Suy luận AI real-time (fall/idle/walk) mỗi ~370ms
//
// ── LED Status (INGESTION) ────────────────────────────────────────
//   Nháy chậm 1Hz   → READY (chờ lệnh)
//   Nháy nhanh 10Hz → COLLECTING (đang thu mẫu)
//   Sáng liên tục   → UPLOADING (đang gửi lên EI)
//   5 nháy chậm     → DONE OK
//   10 nháy nhanh   → DONE ERROR
//
// ── LED Status (INFERENCE) ────────────────────────────────────────
//   Tắt             → idle
//   Nháy 2Hz        → walk
//   Sáng liên tục   → fall (alert)
//
// ── Axis convention ───────────────────────────────────────────────
//   Mạch NẰM PHẲNG trên bụng, chân cắm hướng xuống đất:
//   - Trục Z vật lý → Body Forward (aX)
//   - Trục X vật lý → Body Left   (aY)
//   - Trục Y vật lý → Body Up     (aZ) ← kỳ vọng +1g khi đứng thẳng
//
// ── Lưu ý thư viện EI ────────────────────────────────────────────
//   MAIN_PROJECT_THAYKIEN_inferencing.h phải là #include đầu tiên
//   để tránh xung đột macro min/max/round/DEFAULT với Arduino core.
// ════════════════════════════════════════════════════════════════

// ── Includes — thứ tự quan trọng: EI trước Arduino headers ───────
#include <MAIN_PROJECT_THAYKIEN_inferencing.h>
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <stdarg.h>

#include "ei_config.h"

// ╔══════════════════════════════════════════════════════════════╗
// ║                         STRUCTS                              ║
// ╚══════════════════════════════════════════════════════════════╝

enum SensorStatus { SENSOR_OK = 0, SENSOR_ERROR };

struct ImuData {
  float aX, aY, aZ;
  float gX, gY, gZ;
  SensorStatus status;
};

// ── Chế độ hoạt động ─────────────────────────────────────────────
enum OperationMode { INGESTION, INFERENCE };

// ── State machine (chỉ dùng khi INGESTION) ───────────────────────
enum State { READY, CALIBRATING, COLLECTING, UPLOADING, DONE_OK, DONE_ERR };

// ── Kết quả suy luận (dùng khi INFERENCE) ────────────────────────
struct InferResult {
  float fall, idle, walk;
  int timing_dsp, timing_cls;
  int fall_count;     // số lần phát hiện té ngã trong phiên
  char top_label[16]; // nhãn chiếm ưu thế hiện tại
};

// ╔══════════════════════════════════════════════════════════════╗
// ║                          GLOBALS                             ║
// ╚══════════════════════════════════════════════════════════════╝

OperationMode operationMode = INFERENCE;
State state = READY;
InferResult inferResult = {};

bool mpuConnected = false;
bool lastUploadOk = false;
char lastUploadTime[32] = "--";
char currentSampleName[32] = "";
char activeLabel[32] = EI_LABEL; // đổi qua Web UI, không cần reflash

// ── Sample buffer (Ingestion) — 300 × 6 floats = 7.2 KB ──────────
float sampleBuf[EI_SAMPLE_COUNT][EI_NUM_CHANNELS];
int samplesCollected = 0;

unsigned long sampleTimer = 0;
static const unsigned long SAMPLE_INTERVAL_US = 1000000UL / EI_SAMPLING_FREQ_HZ;

// ── Live IMU snapshot cho web (volatile — safe atomic float ESP32) ─
volatile float web_aX = 0, web_aY = 0, web_aZ = 0;
volatile float web_gX = 0, web_gY = 0, web_gZ = 0;
volatile float web_totalG = 0;

// ── Calibration offsets ───────────────────────────────────────────
#define CAL_SAMPLES 200
float off_aX = 0, off_aY = 0, off_aZ = 0;
float off_gX = 0, off_gY = 0, off_gZ = 0;
bool calibrated = false;
float calSumAX = 0, calSumAY = 0, calSumAZ = 0;
float calSumGX = 0, calSumGY = 0, calSumGZ = 0;
int calCount = 0;

// ── Auto-run ──────────────────────────────────────────────────────
bool autoRunActive = false;
int autoRunTotal = 0;
int autoRunDone = 0;

// ── Augmentation Scale ────────────────────────────────────────────
bool augmentMode = false;
int augUploadIdx = 0;
const float AUG_SCALES[3] = {1.00f, 0.94f, 1.06f};

// ── Augmentation Jitter ───────────────────────────────────────────
bool jitterMode = false;
#define JITTER_ACCEL 0.02f
#define JITTER_GYRO 1.00f

// ── Manual Upload ─────────────────────────────────────────────────
bool manualUploadActive = false;
bool manualSavedAugMode = false;
bool manualSavedJitter = false;

// ── Inference: Sliding Window Continuous ─────────────────────────
// run_classifier_continuous() cần EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW = 4
// slice = 37 mẫu → mỗi 370ms chạy 1 lần inference
static float inferBuf[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE]; // 900 floats
static int inferBufIdx = 0;
static bool inferBufFull = false; // đợi đủ 1 window trước lần inference đầu

// ── WebServer ─────────────────────────────────────────────────────
WebServer server(80);

// ╔══════════════════════════════════════════════════════════════╗
// ║                        WEB LOG (Ring Buffer)                 ║
// ╚══════════════════════════════════════════════════════════════╝

#define WEB_LOG_LINES 30
#define WEB_LOG_LEN 96
static char webLog[WEB_LOG_LINES][WEB_LOG_LEN];
static int webLogHead = 0;
static int webLogCount = 0;

void logPrint(const char *msg) {
  Serial.print(msg);
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

// EI yêu cầu hàm này được định nghĩa trong sketch
void ei_printf(const char *format, ...) {
  static char _buf[192];
  va_list ap;
  va_start(ap, format);
  vsnprintf(_buf, sizeof(_buf), format, ap);
  va_end(ap);
  Serial.print(_buf);
}

// ── Forward declarations ──────────────────────────────────────────
ImuData imuRead();
inline void refreshImuSnapshot();
bool uploadToEdgeImpulse(float scale, int augIdx, bool jitter);

// ── HTML page (khai báo ở html_page.h, định nghĩa sau) ───────────
#include "html_page.h"

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
  writeMPU6050Register(0x1B, 0x08); // Gyro ±500 deg/s
  writeMPU6050Register(0x1C, 0x10); // Accel ±8g
  return true;
}

// Rotation matrix — mạch nằm phẳng trên bụng, chân cắm xuống đất
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

  // Z→Fwd, X→Left, Y→Up — trừ offset
  d.aX = pAZ - off_aX;
  d.aY = pAX - off_aY;
  d.aZ = pAY - off_aZ;
  d.gX = pGZ - off_gX;
  d.gY = pGX - off_gY;
  d.gZ = pGY - off_gZ;
  d.status = SENSOR_OK;
  return d;
}

// Cập nhật snapshot web khi UPLOADING để XYZ không đơ trên UI
inline void refreshImuSnapshot() {
  ImuData s = imuRead();
  if (s.status == SENSOR_OK) {
    web_aX = s.aX;
    web_aY = s.aY;
    web_aZ = s.aZ;
    web_gX = s.gX;
    web_gY = s.gY;
    web_gZ = s.gZ;
    web_totalG = sqrtf(s.aX * s.aX + s.aY * s.aY + s.aZ * s.aZ);
  }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          HTTPS UPLOAD — Edge Impulse Ingestion API           ║
// ╚══════════════════════════════════════════════════════════════╝
// Dùng Content-Length — EI server không hỗ trợ chunked request body.

WiFiClientSecure httpsClient;
static char jsonPayload[22000]; // 300 mẫu × 6 floats × ~12 chars ≈ 21.6 KB

static uint32_t _lcgState = 12345;
float randF(float range) {
  _lcgState = _lcgState * 1664525UL + 1013904223UL;
  float r = (float)(_lcgState & 0xFFFF) / 65535.0f;
  return (r * 2.0f - 1.0f) * range;
}

int buildJson(float scale, bool jitter = false) {
  int pos = 0;
  char tmp[20];
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

  for (int i = 0; i < samplesCollected; i++) {
    if (i > 0 && pos < (int)sizeof(jsonPayload) - 80)
      jsonPayload[pos++] = ',';
    jsonPayload[pos++] = '[';
    for (int c = 0; c < EI_NUM_CHANNELS; c++) {
      if (c > 0)
        jsonPayload[pos++] = ',';
      float v = sampleBuf[i][c] * scale;
      if (jitter)
        v += (c < 3) ? randF(JITTER_ACCEL) : randF(JITTER_GYRO);
      int n = snprintf(tmp, sizeof(tmp), "%.4f", v);
      memcpy(jsonPayload + pos, tmp, n);
      pos += n;
    }
    jsonPayload[pos++] = ']';
  }
  const char *ftr = "]}}";
  memcpy(jsonPayload + pos, ftr, 3);
  pos += 3;
  jsonPayload[pos] = '\0';
  return pos;
}

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
  httpsClient.println("POST /api/training/data HTTP/1.1");
  httpsClient.println("Host: ingestion.edgeimpulse.com");
  unsigned long ts = millis();
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

  int sent = 0;
  while (sent < jsonLen) {
    int block = min(512, jsonLen - sent);
    httpsClient.write((uint8_t *)(jsonPayload + sent), block);
    sent += block;
    delay(1);
    server.handleClient();
    refreshImuSnapshot();
  }
  Serial.println("[EI] Đã gửi. Chờ phản hồi...");
  unsigned long t0 = millis();
  while (!httpsClient.available() && millis() - t0 < 10000) {
    delay(5);
    server.handleClient();
    refreshImuSnapshot();
  }
  String statusLine = httpsClient.readStringUntil('\n');
  Serial.printf("[EI] Server: %s\n", statusLine.c_str());
  bool ok = (statusLine.indexOf("200") >= 0);
  httpsClient.stop();
  return ok;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                     HTTP ROUTE HANDLERS                      ║
// ╚══════════════════════════════════════════════════════════════╝

// ── Helper: chuỗi trạng thái cho JSON ────────────────────────────
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

// ── /log — 30 dòng log gần nhất dạng JSON ────────────────────────
void handleLog() {
  static char buf[3200];
  int pos = 0;
  pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"lines\":[");
  int start = webLogCount < WEB_LOG_LINES ? 0 : webLogHead;
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

// ── /status — snapshot toàn bộ trạng thái ingestion ─────────────
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
           "\"mode\":\"%s\","
           "\"lastUpload\":\"%s\",\"ip\":\"%s\"}",
           stateStr(), activeLabel, calibrated ? "true" : "false",
           (calCount * 100) / CAL_SAMPLES, (float)web_aX, (float)web_aY,
           (float)web_aZ, (float)web_totalG, samplesCollected, EI_SAMPLE_COUNT,
           autoRunActive ? "true" : "false", autoRunTotal, autoRunDone,
           augmentMode ? "true" : "false", augUploadIdx,
           jitterMode ? "true" : "false", manualUploadActive ? "true" : "false",
           operationMode == INGESTION ? "ingestion" : "inference",
           lastUploadTime, WiFi.localIP().toString().c_str());
  server.send(200, "application/json", buf);
}

// ── /infer-status — snapshot kết quả suy luận ────────────────────
void handleInferStatus() {
  static char buf[512];
  snprintf(buf, sizeof(buf),
           "{\"mode\":\"inference\","
           "\"label\":\"%s\","
           "\"fall\":%.3f,\"idle\":%.3f,\"walk\":%.3f,"
           "\"timing_dsp\":%d,\"timing_cls\":%d,"
           "\"fall_count\":%d,"
           "\"ip\":\"%s\"}",
           inferResult.top_label, inferResult.fall, inferResult.idle,
           inferResult.walk, inferResult.timing_dsp, inferResult.timing_cls,
           inferResult.fall_count, WiFi.localIP().toString().c_str());
  server.send(200, "application/json", buf);
}

// ── /setmode — chuyển chế độ INGESTION ↔ INFERENCE ───────────────
void handleSetMode() {
  // Chỉ cho phép đổi mode khi ingestion đang READY
  if (operationMode == INGESTION && state != READY) {
    server.send(400, "text/plain", "busy");
    return;
  }
  String m = server.hasArg("mode") ? server.arg("mode") : "";
  if (m == "inference") {
    operationMode = INFERENCE;
    // Reset inference buffer để bắt đầu mới
    inferBufIdx = 0;
    inferBufFull = false;
    logPrintf("[MODE] Chuyển sang INFERENCE\n");
  } else {
    operationMode = INGESTION;
    logPrintf("[MODE] Chuyển sang INGESTION\n");
  }
  server.send(200, "text/plain", m == "inference" ? "inference" : "ingestion");
}

// ── /reset-alerts — xóa bộ đếm fall ─────────────────────────────
void handleResetAlerts() {
  inferResult.fall_count = 0;
  logPrintf("[INF] Đã đặt lại bộ đếm cảnh báo té ngã\n");
  server.send(200, "text/plain", "ok");
}

// ── /setaugment, /setjitter, /trigger, /setlabel ─────────────────
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
void handleTrigger() {
  if (operationMode == INGESTION && state == READY) {
    samplesCollected = 0;
    sampleTimer = micros();
    state = COLLECTING;
    logPrintf("[WEB] Trigger! Thu mẫu label='%s'...\n", activeLabel);
  }
  server.send(200, "text/plain", "ok");
}
void handleSetLabel() {
  if (operationMode == INGESTION && state != READY) {
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
  count = constrain(count, 1, 500);
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
      continue;
    int commas = 0;
    for (int i = 0; i < (int)line.length(); i++)
      if (line[i] == ',')
        commas++;
    int skipCols = (commas >= 6) ? 1 : 0;
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
  manualSavedAugMode = augmentMode;
  manualSavedJitter = jitterMode;
  augmentMode = doScale;
  jitterMode = doJitter;
  augUploadIdx = 0;
  manualUploadActive = true;
  state = UPLOADING;
  server.send(200, "text/plain", "ok");
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
  Serial.println("\n[MAIN] === WEARABLE UNIFIED v1 ===");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  mpuConnected = initMPU6050();
  if (!mpuConnected) {
    Serial.println("[MPU] FAIL — Kiểm tra dây nối!");
    blinkLed(20, 200);
  } else {
    Serial.println("[MPU] OK");
    delay(100);
    ImuData boot = imuRead();
    if (boot.status == SENSOR_OK)
      Serial.printf("[AXIS] aZ=%.3fg — %s\n", boot.aZ,
                    (boot.aZ >= AXIS_Z_MIN && boot.aZ <= AXIS_Z_MAX) ? "PASS"
                                                                     : "WARN");
  }

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

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/infer-status", HTTP_GET, handleInferStatus);
  server.on("/setmode", HTTP_GET, handleSetMode);
  server.on("/reset-alerts", HTTP_GET, handleResetAlerts);
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
  logPrintf("[MAIN] Sẵn sàng.\n");
  state = READY;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                   LOOP — State Machine                       ║
// ╚══════════════════════════════════════════════════════════════╝

void loop() {
  static unsigned long ledTimer = 0;
  static bool ledState = false;

  // ── INFERENCE mode ──────────────────────────────────────────
  if (operationMode == INFERENCE) {
    if ((micros() - sampleTimer) >= SAMPLE_INTERVAL_US) {
      sampleTimer = micros();
      ImuData d = imuRead();
      if (d.status == SENSOR_OK) {
        web_aX = d.aX;
        web_aY = d.aY;
        web_aZ = d.aZ;
        web_gX = d.gX;
        web_gY = d.gY;
        web_gZ = d.gZ;
        web_totalG = sqrtf(d.aX * d.aX + d.aY * d.aY + d.aZ * d.aZ);

        int wp = (EI_CLASSIFIER_RAW_SAMPLE_COUNT - EI_CLASSIFIER_SLICE_SIZE) *
                     EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME +
                 inferBufIdx * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
        inferBuf[wp + 0] = d.aX;
        inferBuf[wp + 1] = d.aY;
        inferBuf[wp + 2] = d.aZ;
        inferBuf[wp + 3] = d.gX;
        inferBuf[wp + 4] = d.gY;
        inferBuf[wp + 5] = d.gZ;
        inferBufIdx++;

        if (inferBufIdx >= EI_CLASSIFIER_SLICE_SIZE) {
          inferBufIdx = 0;
          memmove(inferBuf,
                  inferBuf + EI_CLASSIFIER_SLICE_SIZE *
                                 EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME,
                  (EI_CLASSIFIER_RAW_SAMPLE_COUNT - EI_CLASSIFIER_SLICE_SIZE) *
                      EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME * sizeof(float));
          if (!inferBufFull) {
            static int wu = 0;
            if (++wu >= EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)
              inferBufFull = true;
          }
          if (inferBufFull) {
            signal_t sig;
            if (numpy::signal_from_buffer(
                    inferBuf, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &sig) == 0) {
              ei_impulse_result_t res = {};
              if (run_classifier(&sig, &res, false) == EI_IMPULSE_OK) {
                inferResult.timing_dsp = res.timing.dsp;
                inferResult.timing_cls = res.timing.classification;
                inferResult.fall = inferResult.idle = inferResult.walk = 0;
                float best = -1;
                for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
                  float v = res.classification[i].value;
                  const char *l = res.classification[i].label;
                  if (strcmp(l, "fall") == 0)
                    inferResult.fall = v;
                  else if (strcmp(l, "idle") == 0)
                    inferResult.idle = v;
                  else if (strcmp(l, "walk") == 0)
                    inferResult.walk = v;
                  if (v > best) {
                    best = v;
                    strncpy(inferResult.top_label, l, 15);
                    inferResult.top_label[15] = 0;
                  }
                }
                // ── Debouncing + Cooldown ─────────────────────────────
                static int fallConfirm = 0;
                static unsigned long lastAlertMs = 0;

                if (inferResult.fall >= FALL_ALERT_THRESHOLD) {
                  fallConfirm++;
                  if (fallConfirm >= FALL_CONFIRM_SLICES &&
                      millis() - lastAlertMs >= FALL_COOLDOWN_MS) {
                    // Xác nhận đủ số slice và qua cooldown → cảnh báo thật
                    fallConfirm = 0;
                    lastAlertMs = millis();
                    inferResult.fall_count++;
                    logPrintf(
                        "[INF] CẢNH BÁO TÉ NGÃ #%d (conf=%.2f, x%d slices)\n",
                        inferResult.fall_count, inferResult.fall,
                        FALL_CONFIRM_SLICES);
                    ledOn();
                  } else {
                    // Đang tích lũy xác nhận — nháy LED nhanh báo hiệu chưa
                    // chắc
                    if (millis() - ledTimer > 80) {
                      ledTimer = millis();
                      ledState = !ledState;
                      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
                    }
                  }
                } else if (inferResult.walk > inferResult.idle) {
                  fallConfirm = 0; // reset bộ đếm khi không còn fall
                  if (millis() - ledTimer > 250) {
                    ledTimer = millis();
                    ledState = !ledState;
                    digitalWrite(LED_PIN, ledState ? LOW : HIGH);
                  }
                } else {
                  fallConfirm = 0; // reset bộ đếm khi không còn fall
                  ledOff();
                }
                Serial.printf("[INF] fall:%.2f idle:%.2f walk:%.2f -> %s%s "
                              "[DSP:%dms CLS:%dms]\n",
                              inferResult.fall, inferResult.idle,
                              inferResult.walk, inferResult.top_label,
                              (inferResult.fall >= FALL_ALERT_THRESHOLD)
                                  ? " *** ALERT ***"
                                  : "",
                              inferResult.timing_dsp, inferResult.timing_cls);
              }
            }
          }
        }
      }
    }
    server.handleClient();
    return;
  }

  // ── INGESTION state machine ──────────────────────────────────
  switch (state) {
  case READY:
    if (millis() - ledTimer > 500) {
      ledTimer = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    }
    if (digitalRead(BTN_PIN) == LOW) {
      delay(50);
      if (digitalRead(BTN_PIN) == LOW) {
        while (digitalRead(BTN_PIN) == LOW)
          delay(10);
        samplesCollected = 0;
        sampleTimer = micros();
        state = COLLECTING;
        Serial.printf("[REC] BẮT ĐẦU label='%s'...\n", activeLabel);
      }
    }
    break;

  case CALIBRATING:
    if (millis() - ledTimer > 100) {
      ledTimer = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    }
    if ((micros() - sampleTimer) >= SAMPLE_INTERVAL_US) {
      sampleTimer = micros();
      ImuData d = imuRead();
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
        off_aZ = calSumAZ / calCount - 1.0f;
        off_gX = calSumGX / calCount;
        off_gY = calSumGY / calCount;
        off_gZ = calSumGZ / calCount;
        calibrated = true;
        logPrintf("[CAL] Xong! offA:%.3f %.3f %.3f offG:%.3f %.3f %.3f\n",
                  off_aX, off_aY, off_aZ, off_gX, off_gY, off_gZ);
        blinkLed(3, 150);
        state = READY;
      }
    }
    server.handleClient();
    break;

  case COLLECTING:
    if (millis() - ledTimer > 50) {
      ledTimer = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? LOW : HIGH);
    }
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
        web_aX = d.aX;
        web_aY = d.aY;
        web_aZ = d.aZ;
        web_gX = d.gX;
        web_gY = d.gY;
        web_gZ = d.gZ;
        web_totalG = sqrtf(d.aX * d.aX + d.aY * d.aY + d.aZ * d.aZ);
      }
      if (samplesCollected >= EI_SAMPLE_COUNT) {
        Serial.printf("[REC] Đủ %d mẫu.\n", samplesCollected);
        ledOn();
        state = UPLOADING;
      }
    }
    server.handleClient();
    break;

  case UPLOADING:
    ledOn();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      delay(3000);
    }
    {
      float sc = augmentMode ? AUG_SCALES[augUploadIdx] : 1.0f;
      bool ok = uploadToEdgeImpulse(sc, augUploadIdx, jitterMode);
      if (!ok) {
        augUploadIdx = 0;
        state = DONE_ERR;
      } else if (augmentMode && augUploadIdx < 2) {
        augUploadIdx++;
        logPrintf("[AUG] %d/3 xong.\n", augUploadIdx);
      } else {
        augUploadIdx = 0;
        state = DONE_OK;
      }
    }
    break;

  case DONE_OK:
    ledOff();
    lastUploadOk = true;
    snprintf(lastUploadTime, sizeof(lastUploadTime), "%s OK",
             currentSampleName);
    logPrintf("[EI] THÀNH CÔNG! '%s'\n", currentSampleName);
    if (autoRunActive) {
      blinkLed(2, 120);
      autoRunDone++;
      if (autoRunDone < autoRunTotal) {
        server.handleClient();
        samplesCollected = 0;
        sampleTimer = micros();
        state = COLLECTING;
        logPrintf("[AUTO] Batch %d/%d...\n", autoRunDone + 1, autoRunTotal);
      } else {
        autoRunActive = false;
        logPrintf("[AUTO] Xong! %d batches.\n", autoRunDone);
        state = READY;
      }
    } else {
      blinkLed(5, 300);
      if (manualUploadActive) {
        manualUploadActive = false;
        augmentMode = manualSavedAugMode;
        jitterMode = manualSavedJitter;
      }
      state = READY;
    }
    break;

  case DONE_ERR:
    ledOff();
    lastUploadOk = false;
    snprintf(lastUploadTime, sizeof(lastUploadTime), "%s ERR",
             currentSampleName);
    logPrintf("[EI] THẤT BẠI! '%s'\n", currentSampleName);
    if (autoRunActive)
      autoRunActive = false;
    if (manualUploadActive) {
      manualUploadActive = false;
      augmentMode = manualSavedAugMode;
      jitterMode = manualSavedJitter;
    }
    blinkLed(10, 100);
    state = READY;
    break;
  }

  if (state != UPLOADING)
    server.handleClient();

  if (state == READY && mpuConnected) {
    static unsigned long lastWeb = 0;
    if (millis() - lastWeb >= 50) {
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
