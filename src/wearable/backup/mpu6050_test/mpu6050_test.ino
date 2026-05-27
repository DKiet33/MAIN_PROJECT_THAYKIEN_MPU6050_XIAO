#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

// ════════════════════════════════════════════════════════════════
// File: mpu6050_test.ino
// Purpose: Test MPU6050 + XIAO ESP32S3 tích hợp WiFi Live Console
// Board: Seeed XIAO ESP32S3
// Sensor: MPU6050 / GY-521
// Author: Senior Dev + Codex
// Date: 2026-05-12
//
// Kiến trúc Dual-Core (FreeRTOS):
//   Core 1 — loop(): Đọc IMU liên tục, không bị WiFi block
//   Core 0 — wifiTask(): Xử lý HTTP request từ trình duyệt
// ════════════════════════════════════════════════════════════════

// ╔══════════════════════════════════════════════════════════════╗
// ║                    CẤU HÌNH (CONFIG)                         ║
// ╚══════════════════════════════════════════════════════════════╝

// ══════ Cấu hình WiFi ══════
#define WIFI_SSID "Noss Bigger"
#define WIFI_PASSWORD "heistheboss"

// AP Mode (fallback khi không kết nối được mạng nhà)
#define WIFI_AP_SSID "XIAO_MPU_TEST"
#define WIFI_AP_PASSWORD "12345678"

// ══════ I2C — XIAO ESP32S3 ══════
// D4 = GPIO5 (SDA), D5 = GPIO6 (SCL)
#define I2C_SDA_PIN 5
#define I2C_SCL_PIN 6
#define MPU6050_I2C_ADDR 0x68

// ══════ Hệ số chuyển đổi (đồng nhất với wearable.ino / ei_config.h) ══════
// Accel: Full Scale ±8g  → LSB/g = 4096
// Gyro:  Full Scale ±500°/s → LSB/(°/s) = 65.5
#define ACCEL_SCALE 4096.0f
#define GYRO_SCALE 65.5f

// ══════ Cấu hình Ngưỡng ══════
#define AXIS_Z_MIN 0.85f
#define AXIS_Z_MAX 1.15f
#define SHAKE_THRESHOLD_G 2.0f

// ══════ Timing & Serial ══════
#define PRINT_INTERVAL_MS 200
#define SERIAL_BAUD 115200

// ╔══════════════════════════════════════════════════════════════╗
// ║                    STRUCTS (giống wearable.ino)              ║
// ╚══════════════════════════════════════════════════════════════╝

// Đồng nhất với wearable.ino — dùng ImuData struct để dễ bảo trì
enum SensorStatus { SENSOR_OK = 0, SENSOR_ERROR, SENSOR_AXIS_WARN };

struct ImuData {
  float aX, aY, aZ; // Body frame: Forward, Left, Up (đã xoay)
  float gX, gY, gZ; // Body frame gyro tương ứng
  float tempC;
  SensorStatus status;
};

// ╔══════════════════════════════════════════════════════════════╗
// ║                    GLOBAL OBJECTS & SHARED STATE             ║
// ╚══════════════════════════════════════════════════════════════╝

bool mpuConnected = false;
unsigned long lastPrint = 0;
uint32_t sampleCount = 0;

// Shared state giữa Core 1 (IMU) và Core 0 (HTTP)
// Trên ESP32, ghi float 32-bit aligned là atomic ở phần cứng — volatile là đủ
volatile float shared_aX = 0, shared_aY = 0, shared_aZ = 0;
volatile float shared_gX = 0, shared_gY = 0, shared_gZ = 0;
volatile float shared_tempC = 0, shared_totalG = 0;
volatile bool shared_shake = false;
volatile uint32_t shared_sampleCount = 0;
// Debug: IMU Hz thực tế và thời gian từ lần cập nhật cuối (ms)
volatile uint32_t shared_imuHz = 0;
volatile uint32_t shared_dataAgeMs = 0;

// WebServer trên Core 0
WebServer server(80);

// ── Bộ đệm Log Vòng tròn (Circular FIFO Buffer) ──
// Dùng char array cố định — không cấp phát heap
#define MAX_LOG_LINES 40
#define MAX_LOG_LINE_LEN 128
char logBuffer[MAX_LOG_LINES][MAX_LOG_LINE_LEN];
int logHead = 0;
int logTail = 0;
bool logFull = false;

// Buffer JSON tĩnh cho /data endpoint
#define JSON_BUF_SIZE 6144
static char jsonBuf[JSON_BUF_SIZE];

// ── Log helpers ──────────────────────────────────────────────────

void addLog(const char *line) {
  strncpy(logBuffer[logHead], line, MAX_LOG_LINE_LEN - 1);
  logBuffer[logHead][MAX_LOG_LINE_LEN - 1] = '\0';
  logHead = (logHead + 1) % MAX_LOG_LINES;
  if (logFull) {
    logTail = logHead;
  } else if (logHead == logTail) {
    logFull = true;
  }
}

void logPrintf(const char *format, ...) {
  char buf[MAX_LOG_LINE_LEN];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  Serial.print(buf);
  addLog(buf);
}

void logPrintln(const __FlashStringHelper *s) {
  Serial.println(s);
  char buf[MAX_LOG_LINE_LEN];
  snprintf(buf, sizeof(buf), "%S", s);
  addLog(buf);
}

void logPrintln(const char *s) {
  Serial.println(s);
  addLog(s);
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    WEB DASHBOARD                             ║
// ╚══════════════════════════════════════════════════════════════╝

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>MPU6050 Live Console</title>
  <style>
    body {
      background-color: #0b0f19;
      color: #e2e8f0;
      font-family: Consolas, Monaco, 'Andale Mono', 'Ubuntu Mono', monospace;
      margin: 0;
      padding: 16px;
      font-size: 14px;
      line-height: 1.4;
    }
    .header {
      border-bottom: 1px solid #1e293b;
      padding-bottom: 12px;
      margin-bottom: 16px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    h1 { margin: 0; font-size: 16px; color: #38bdf8; font-weight: normal; }
    .status { font-size: 12px; color: #94a3b8; }
    .metrics {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 10px;
      margin-bottom: 16px;
    }
    .box { background: #111827; padding: 10px; border: 1px solid #1f2937; }
    .lbl { font-size: 11px; color: #6b7280; }
    .val { font-size: 16px; font-weight: bold; margin-top: 4px; color: #f3f4f6; }
    .box.shake { background: #7f1d1d; border-color: #ef4444; }
    .box.shake .val { color: #fca5a5; }
    .box.warn  { background: #713f12; border-color: #f59e0b; }
    .box.warn  .val { color: #fcd34d; }
    .box.good  { background: #064e3b; border-color: #10b981; }
    .box.good  .val { color: #6ee7b7; }
    .console-lbl { font-size: 11px; color: #6b7280; margin-bottom: 6px; }
    .console {
      background: #030712;
      padding: 10px;
      border: 1px solid #1f2937;
      height: 400px;
      overflow-y: auto;
      white-space: pre-wrap;
      word-break: break-all;
      color: #10b981;
      font-size: 13px;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>[MPU6050 Live Terminal]</h1>
    <div class="status" id="status">Đang kết nối...</div>
  </div>

  <div class="metrics">
    <div class="box">
      <div class="lbl">ACCEL (g) — Fwd/Left/Up</div>
      <div class="val" id="acc">0.00 / 0.00 / 0.00</div>
    </div>
    <div class="box">
      <div class="lbl">GYRO (°/s)</div>
      <div class="val" id="gyr">0.0 / 0.0 / 0.0</div>
    </div>
    <div class="box">
      <div class="lbl">TỔNG |a|</div>
      <div class="val" id="tot">0.00 g</div>
    </div>
    <div class="box" id="box-shk">
      <div class="lbl">TRẠNG THÁI</div>
      <div class="val" id="shk">OK</div>
    </div>
    <div class="box" id="box-dbg">
      <div class="lbl">DEBUG (lần cập nhật)</div>
      <div class="val" id="dbg">--</div>
    </div>
  </div>

  <div class="console-lbl">LOG OUTPUT:</div>
  <div class="console" id="console"></div>

  <script>
    const term = document.getElementById('console');
    async function update() {
      try {
        const r = await fetch('/data');
        if (!r.ok) return;
        const d = await r.json();

        document.getElementById('status').textContent = `Core1 OK | Mẫu #${d.sampleCount}`;
        document.getElementById('status').style.color = '#10b981';

        document.getElementById('acc').textContent = `${d.aX.toFixed(2)} / ${d.aY.toFixed(2)} / ${d.aZ.toFixed(2)}`;
        document.getElementById('gyr').textContent = `${d.gX.toFixed(1)} / ${d.gY.toFixed(1)} / ${d.gZ.toFixed(1)}`;
        document.getElementById('tot').textContent = `${d.totalG.toFixed(2)} g`;

        const bShk = document.getElementById('box-shk');
        if (d.shake) {
          bShk.className = 'box shake';
          document.getElementById('shk').textContent = '⚡SHAKE!';
        } else {
          bShk.className = 'box';
          document.getElementById('shk').textContent = 'OK';
        }

        if (d.logs && d.logs.length > 0) {
          term.innerHTML = d.logs.join('\n');
          term.scrollTop = term.scrollHeight;
        }

        // Debug box: IMU Hz + data age
        const bDbg = document.getElementById('box-dbg');
        const age  = d.dataAgeMs;
        const hz   = d.imuHz;
        document.getElementById('dbg').textContent = `${hz} Hz | ${age} ms ago`;
        if (age > 500 || hz < 50) {
          bDbg.className = 'box shake'; // Đỏ nặng: hỏ thống bị chặn
        } else if (age > 150 || hz < 80) {
          bDbg.className = 'box warn';  // Cảnh báo nhẹ
        } else {
          bDbg.className = 'box good';  // Bình thường
        }
      } catch (e) {
        document.getElementById('status').textContent = 'Mất kết nối...';
        document.getElementById('status').style.color = '#ef4444';
      }
    }
    setInterval(update, 200);
    update();
  </script>
</body>
</html>
)rawliteral";

// ╔══════════════════════════════════════════════════════════════╗
// ║                    HTTP HANDLERS                             ║
// ╚══════════════════════════════════════════════════════════════╝

void handleRoot() { server.send(200, "text/html", index_html); }

// Đọc snapshot từ shared state trong critical section rồi mới build JSON
void handleData() {
  // Snapshot từ shared state — float 32-bit aligned là atomic trên ESP32
  float aX = shared_aX, aY = shared_aY, aZ = shared_aZ;
  float gX = shared_gX, gY = shared_gY, gZ = shared_gZ;
  float tempC = shared_tempC, totalG = shared_totalG;
  bool shake = shared_shake;
  uint32_t sc = shared_sampleCount;

  // Build JSON vào static buffer — không cấp phát heap
  int pos = snprintf(jsonBuf, JSON_BUF_SIZE,
                     "{\"sampleCount\":%lu,"
                     "\"aX\":%.3f,\"aY\":%.3f,\"aZ\":%.3f,"
                     "\"gX\":%.2f,\"gY\":%.2f,\"gZ\":%.2f,"
                     "\"tempC\":%.1f,\"totalG\":%.3f,"
                     "\"shake\":%s,\"imuHz\":%lu,\"dataAgeMs\":%lu,\"logs\":[",
                     (unsigned long)sc, aX, aY, aZ, gX, gY, gZ, tempC, totalG,
                     shake ? "true" : "false", (unsigned long)shared_imuHz,
                     (unsigned long)shared_dataAgeMs);

  int count = logFull ? MAX_LOG_LINES : logHead;
  int curr = logTail;
  bool first = true;
  for (int i = 0; i < count && pos < JSON_BUF_SIZE - 4; i++) {
    if (!first)
      jsonBuf[pos++] = ',';
    first = false;
    jsonBuf[pos++] = '"';
    const char *src = logBuffer[curr];
    while (*src && pos < JSON_BUF_SIZE - 4) {
      char c = *src++;
      if (c == '"' || c == '\\')
        jsonBuf[pos++] = '\\';
      if (c != '\n' && c != '\r')
        jsonBuf[pos++] = c;
    }
    jsonBuf[pos++] = '"';
    curr = (curr + 1) % MAX_LOG_LINES;
  }

  if (pos < JSON_BUF_SIZE - 2) {
    jsonBuf[pos++] = ']';
    jsonBuf[pos++] = '}';
    jsonBuf[pos] = '\0';
  }

  server.send(200, "application/json", jsonBuf);
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    MPU6050 FUNCTIONS (giống wearable.ino)    ║
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
  writeMPU6050Register(0x1A, 0x03); // DLPF 44Hz
  writeMPU6050Register(0x1B, 0x08); // Gyro ±500°/s
  writeMPU6050Register(0x1C, 0x10); // Accel ±8g
  return true;
}

// Axis convention (Đã map lại bằng phần mềm):
// Mạch NẰM PHẲNG trên bụng, chân cắm (header) hướng xuống đất.
// Áp dụng ma trận xoay để mạch nằm phẳng trên bụng, chân cắm hướng vào người:
// - Trục Z vật lý (hướng ra ngoài) → Body Forward (aX)
// - Trục X vật lý (hướng sang trái của người dùng) → Body Left (aY)
// - Trục Y vật lý (hướng lên đầu) → Body Up (aZ)
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
  int16_t tp = (Wire.read() << 8) | Wire.read();
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();

  // Chuyển đổi raw → đơn vị vật lý (dùng scale từ ei_config.h)
  float phys_aX = ax / ACCEL_SCALE;
  float phys_aY = ay / ACCEL_SCALE;
  float phys_aZ = az / ACCEL_SCALE;
  float phys_gX = gx / GYRO_SCALE;
  float phys_gY = gy / GYRO_SCALE;
  float phys_gZ = gz / GYRO_SCALE;

  // Ma trận xoay (Rotation Matrix) — đồng nhất với wearable.ino
  d.aX = phys_aZ; // Z vật lý → Forward
  d.aY = phys_aX; // X vật lý → Left
  d.aZ = phys_aY; // Y vật lý → Up

  d.gX = phys_gZ;
  d.gY = phys_gX;
  d.gZ = phys_gY;

  d.tempC = (tp / 340.0f) + 36.53f;
  d.status = SENSOR_OK;
  return d;
}

bool verifyAxisOrientation(float aZ) {
  return (aZ >= AXIS_Z_MIN && aZ <= AXIS_Z_MAX);
}

float totalAccelG(float aX, float aY, float aZ) {
  return sqrtf(aX * aX + aY * aY + aZ * aZ);
}

void scanI2C() {
  logPrintln("\n[I2C] -- Dang quet bus I2C... -----------------");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      logPrintf("[I2C] Thiet bi tai 0x%02X%s\n", addr,
                (addr == 0x68 || addr == 0x69) ? "  <- MPU6050" : "");
      found++;
    }
  }
  logPrintf("[I2C] Tong cong %d thiet bi.\n", found);
}

void printSection(const char *title) { logPrintf("\n== %s ==\n", title); }

// ╔══════════════════════════════════════════════════════════════╗
// ║                    FREERTOS WIFI TASK (Core 0)               ║
// ╚══════════════════════════════════════════════════════════════╝

// Task chạy hoàn toàn trên Core 0 — Core 1 (loop) không bao giờ bị block HTTP
void wifiTask(void *pvParameters) {
  for (;;) {
    server.handleClient();
    vTaskDelay(1); // yield 1ms — đủ thời gian cho Core 0 WiFi stack thở
  }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    SETUP                                     ║
// ╚══════════════════════════════════════════════════════════════╝

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  // ── 1. WiFi (STA + AP Mode) ───────────────────────────────────
  WiFi.mode(WIFI_AP_STA);

  if (String(WIFI_SSID).length() > 0) {
    Serial.printf("[WIFI] Dang ket noi STA: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WIFI] STA OK! IP: %s\n",
                    WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WIFI] STA that bai. Dung AP Mode.");
    }
  }

  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  Serial.printf("[WIFI] AP: %s | http://%s\n", WIFI_AP_SSID,
                WiFi.softAPIP().toString().c_str());

  // ── 2. WebServer ──────────────────────────────────────────────
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.begin();

  // ── 3. Khởi động WiFi Task trên Core 0 ───────────────────────
  // Stack 4KB đủ cho HTTP handler; priority 1 thấp hơn loop (1)
  xTaskCreatePinnedToCore(wifiTask, "wifiTask", 4096, NULL, 1, NULL, 0);

  // ── 4. Khởi tạo I2C ──────────────────────────────────────────
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  // ── 5. Log khởi động ─────────────────────────────────────────
  printSection("MPU6050 TEST + WIFI CONSOLE (Dual-Core)");
  logPrintf("[MAIN] Free SRAM: %u bytes\n",
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  if (WiFi.status() == WL_CONNECTED)
    logPrintf("[WIFI] LAN: http://%s\n", WiFi.localIP().toString().c_str());
  logPrintf("[WIFI] AP : http://%s\n", WiFi.softAPIP().toString().c_str());
  logPrintf("[I2C] SDA=GPIO%d  SCL=GPIO%d  400kHz\n", I2C_SDA_PIN, I2C_SCL_PIN);

  // ── 6. Quét & khởi tạo MPU6050 ───────────────────────────────
  scanI2C();
  logPrintln("[MPU] Khoi tao MPU6050...");
  mpuConnected = initMPU6050();

  if (!mpuConnected) {
    logPrintln(
        "[MPU] FAIL - Kiem tra day VCC(3V3), SDA(D4), SCL(D5), AD0(GND)");
    return;
  }
  logPrintln("[MPU] OK - MPU6050 ket noi thanh cong!");
  logPrintln("[MPU] Accel: +/-8g | Gyro: +/-500 deg/s | DLPF: 44Hz");

  // ── 7. Axis orientation check ─────────────────────────────────
  printSection("KIEM TRA DINH HUONG TRUC");
  logPrintln("[AXIS] Dat mach NAM PHANG vao bung, chan cam huong vao nguoi...");
  delay(1000);

  ImuData boot = imuRead();
  logPrintf("[AXIS] aX=%+.3fg  aY=%+.3fg  aZ=%+.3fg  (ky vong aZ~+1g)\n",
            boot.aX, boot.aY, boot.aZ);

  if (verifyAxisOrientation(boot.aZ)) {
    logPrintln("[AXIS] PASS - Dinh huong truc DUNG (X=Forward, Y=Left, Z=Up)");
  } else {
    logPrintf(
        "[AXIS] WARN - aZ=%.3fg ngoai [%.2f, %.2f] - Kiem tra lai huong gap\n",
        boot.aZ, AXIS_Z_MIN, AXIS_Z_MAX);
  }

  printSection("BAT DAU STREAM DU LIEU (Dual-Core)");
  logPrintln("[MAIN] Core1: IMU | Core0: HTTP Server");
  logPrintln("--------------------------------------------------------");

  lastPrint = millis();
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    LOOP (Core 1 — IMU only)                  ║
// ╚══════════════════════════════════════════════════════════════╝

void loop() {
  if (!mpuConnected) {
    static unsigned long lastErr = 0;
    if (millis() - lastErr > 3000) {
      lastErr = millis();
      logPrintln("[ERROR] MPU6050 chua ket noi.");
    }
    delay(100);
    return;
  }

  // Rate limit 100 Hz — tránh chiếm I2C bus liên tục, nhường CPU cho Core 0
  // WiFi
  static unsigned long lastRead = 0;
  unsigned long now = millis();
  if (now - lastRead < 10)
    return; // 10ms = 100Hz
  lastRead = now;

  ImuData d = imuRead();
  if (d.status == SENSOR_ERROR)
    return;

  float tg = totalAccelG(d.aX, d.aY, d.aZ);
  bool shake = tg > SHAKE_THRESHOLD_G;

  // Đo Hz thực tế mỗi giây
  static uint32_t hzCount = 0;
  static unsigned long hzWindow = 0;
  hzCount++;
  if (now - hzWindow >= 1000) {
    shared_imuHz = hzCount;
    hzCount = 0;
    hzWindow = now;
  }

  // Ghi vào shared state — float 32-bit aligned là atomic trên ESP32
  // Không cần critical section: tệ nhất Core 0 đọc một frame cũ hơn 10ms
  shared_aX = d.aX;
  shared_aY = d.aY;
  shared_aZ = d.aZ;
  shared_gX = d.gX;
  shared_gY = d.gY;
  shared_gZ = d.gZ;
  shared_tempC = d.tempC;
  shared_totalG = tg;
  shared_shake = shake;
  shared_sampleCount = ++sampleCount;
  shared_dataAgeMs = 0; // vừa cập nhật — Core 0 đọc giá trị này khi gọi /data

  // In log theo chu kỳ
  if ((now - lastPrint) >= PRINT_INTERVAL_MS) {
    lastPrint = now;
    logPrintf(
        "#%05lu | aX:%+5.2f aY:%+5.2f aZ:%+5.2f g | |a|:%.2fg | %s | %luHz\n",
        sampleCount, d.aX, d.aY, d.aZ, tg, shake ? "SHAKE!" : "OK",
        shared_imuHz);
    if (sampleCount % 40 == 0)
      logPrintln("--------------------------------------------------------");
  }
}
