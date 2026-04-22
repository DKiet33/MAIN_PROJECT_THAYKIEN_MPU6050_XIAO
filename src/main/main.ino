#include <Adafruit_AHTX0.h>
#include <Arduino.h>
#include <ScioSense_ENS160.h>
#include <Temperature_LM75_Derived.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

// ════════════════════════════════════════════════════════════════
// File: main.ino
// Purpose: Hệ thống phát hiện khí độc và nhiệt độ
// Board: ESP32-S3 N16R8 (16MB OPI Flash + 8MB OPI PSRAM, 44-pin)
// Author: Senior Dev + Codex
// Date: 2026-04-22
//
// Arduino IDE Settings:
//   Board:            ESP32S3 Dev Module
//   Flash Size:       16MB (128Mb)
//   Flash Mode:       DIO 80MHz
//   PSRAM:            OPI PSRAM
//   Partition:        16M Flash (3MB APP/9.9MB FATFS)
//   CPU Frequency:    240MHz
//
// ⚠️  GPIO bị chiếm bởi OPI PSRAM (N16R8): GPIO33–37 KHÔNG DÙNG ĐƯỢC
// ════════════════════════════════════════════════════════════════

// ╔══════════════════════════════════════════════════════════════╗
// ║                    CẤU HÌNH (CONFIG)                         ║
// ╚══════════════════════════════════════════════════════════════╝

// ══════ WiFi ══════
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// ══════ Telegram Bot ══════
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"

// ══════ GPIO — I2C ══════
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// ══════ GPIO — Analog ══════
// ⚠️  GPIO34–37 bị OPI PSRAM chiếm trên N16R8 — KHÔNG dùng!
// Dùng ADC1 (GPIO1–10) để tránh xung đột WiFi với ADC2
#define MQ2_ANALOG_PIN 1

// ══════ GPIO — Output ══════
#define BUZZER_PIN 2
#define LED_PIN 4

// ══════ I2C Addresses ══════
#define LM75_I2C_ADDR 0x48

// ══════ Air Quality Thresholds ══════
#define TVOC_WARN_PPB 150
#define TVOC_DANGER_PPB 500
#define ECO2_WARN_PPM 800
#define ECO2_DANGER_PPM 1500
#define AQI_WARN 3
#define AQI_DANGER 4

// ══════ Temperature Thresholds ══════
#define TEMP_WARN_C 45.0f
#define TEMP_DANGER_C 60.0f
#define TEMP_ERROR_MIN_C -40.0f
#define TEMP_ERROR_MAX_C 125.0f
#define TEMP_DIFF_ERROR_C 15.0f

// ══════ Humidity Thresholds ══════
#define HUMID_WARN_PCT 80.0f
#define HUMID_DANGER_PCT 90.0f

// ══════ Gas Thresholds (MQ2 ADC) ══════
#define GAS_WARN_ADC 400
#define GAS_DANGER_ADC 700

// ══════ Timing (ms) ══════
#define SENSOR_READ_INTERVAL_MS 1000
#define ALERT_COOLDOWN_MS 30000
#define MQ2_WARMUP_MS 300000
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RECONNECT_INTERVAL 30000
#define TELEGRAM_CHECK_INTERVAL 5000
#define TELEGRAM_MAX_PER_CYCLE 5
#define SUPPRESS_DURATION_MS 60000

// ══════ Feature Flags ══════
#define ENABLE_TELEGRAM 0
#define ENABLE_MQ2 1
#define DEBUG_SERIAL 1

// ╔══════════════════════════════════════════════════════════════╗
// ║                    ENUMS & STRUCTS                           ║
// ╚══════════════════════════════════════════════════════════════╝

enum SensorStatus {
  SENSOR_OK = 0,
  SENSOR_WARNING,
  SENSOR_DANGER,
  SENSOR_ERROR,
  SENSOR_WARMUP
};

enum AlertLevel {
  ALERT_NONE = 0,
  ALERT_INFO,
  ALERT_WARNING,
  ALERT_DANGER,
  ALERT_CRITICAL
};

enum SystemState {
  STATE_INIT,
  STATE_NORMAL,
  STATE_SENSOR_ALERT,
  STATE_SENSOR_ERROR
};

struct SensorData {
  uint16_t tvoc;
  uint16_t eco2;
  uint8_t aqi;
  bool ens160Connected;
  float ahtTemp;
  float ahtHumidity;
  bool aht21Connected;
  float lm75Temp;
  bool lm75Connected;
  float tempAvg;
  bool tempCrossCheckOk;
  int mq2Raw;
  bool mq2Ready;
  bool mq2Connected;
  SensorStatus airQualityStatus;
  SensorStatus temperatureStatus;
  SensorStatus gasStatus;
  bool anySensorError;
  uint8_t errorCount;
};

// ╔══════════════════════════════════════════════════════════════╗
// ║                    GLOBAL OBJECTS                            ║
// ╚══════════════════════════════════════════════════════════════╝

ScioSense_ENS160 ens160(ENS160_I2CADDR_1);
Adafruit_AHTX0 aht21;
Generic_LM75 lm75(LM75_I2C_ADDR);

WiFiClientSecure secureClient;
UniversalTelegramBot *bot = nullptr;

SystemState currentState = STATE_INIT;
AlertLevel currentAlertLevel = ALERT_NONE;
unsigned long lastSensorRead = 0;
unsigned long lastAlertTime = 0;
unsigned long lastBuzzerToggle = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastTelegramCheck = 0;
unsigned long mq2WarmupStart = 0;
int mq2ZeroCount = 0;
int mq2MaxCount = 0;
bool buzzerState = false;
bool wifiConnected = false;
bool wifiConnecting = false;
unsigned long wifiConnectStart = 0;
bool alertSuppressed = false;
unsigned long suppressStartedAt = 0;

// ╔══════════════════════════════════════════════════════════════╗
// ║                    HELPER FUNCTIONS                          ║
// ╚══════════════════════════════════════════════════════════════╝

bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool validateTemp(float t) {
  return !isnan(t) && !isinf(t) && t >= TEMP_ERROR_MIN_C &&
         t <= TEMP_ERROR_MAX_C;
}

bool validateGas(int raw) { return raw >= 0 && raw <= 4095; }

bool isMQ2Ready() { return (millis() - mq2WarmupStart) >= MQ2_WARMUP_MS; }

bool isSuppressionActive() {
  if (!alertSuppressed) {
    return false;
  }
  if ((millis() - suppressStartedAt) >= SUPPRESS_DURATION_MS) {
    alertSuppressed = false;
    return false;
  }
  return true;
}

SensorStatus evalAirQuality(uint16_t tvoc, uint16_t eco2, uint8_t aqi) {
  if (tvoc >= TVOC_DANGER_PPB || eco2 >= ECO2_DANGER_PPM || aqi >= AQI_DANGER) {
    return SENSOR_DANGER;
  }
  if (tvoc >= TVOC_WARN_PPB || eco2 >= ECO2_WARN_PPM || aqi >= AQI_WARN) {
    return SENSOR_WARNING;
  }
  return SENSOR_OK;
}

SensorStatus evalTemperature(float t) {
  if (!validateTemp(t)) {
    return SENSOR_ERROR;
  }
  if (t >= TEMP_DANGER_C) {
    return SENSOR_DANGER;
  }
  if (t >= TEMP_WARN_C) {
    return SENSOR_WARNING;
  }
  return SENSOR_OK;
}

SensorStatus evalGas(int raw) {
  if (!validateGas(raw)) {
    return SENSOR_ERROR;
  }
  if (!isMQ2Ready()) {
    return SENSOR_WARMUP;
  }
  if (mq2ZeroCount >= 5 || mq2MaxCount >= 5) {
    return SENSOR_ERROR;
  }
  if (raw >= GAS_DANGER_ADC) {
    return SENSOR_DANGER;
  }
  if (raw >= GAS_WARN_ADC) {
    return SENSOR_WARNING;
  }
  return SENSOR_OK;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    SENSOR FUNCTIONS                          ║
// ╚══════════════════════════════════════════════════════════════╝

void sensorsBegin() {
  mq2WarmupStart = millis();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  ens160.begin();
  bool ensOk = ens160.available();
  if (ensOk) {
    ens160.setMode(ENS160_OPMODE_STD);
  }

  bool ahtOk = aht21.begin();
  bool lmOk = i2cPresent(LM75_I2C_ADDR);

#if ENABLE_MQ2
  analogSetAttenuation(ADC_11db);
#endif

#if DEBUG_SERIAL
  Serial.printf("[SENSOR] ENS160 %s\n", ensOk ? "OK" : "FAIL");
  Serial.printf("[SENSOR] AHT21 %s\n", ahtOk ? "OK" : "FAIL");
  Serial.printf("[SENSOR] LM75  %s (0x48)\n", lmOk ? "OK" : "MISSING");
  Serial.printf("[SENSOR] MQ2   warming up 300s...\n");
#endif
}

void scanI2C() {
#if DEBUG_SERIAL
  Serial.println("[I2C] Scanning...");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C] Found 0x%02X\n", a);
    }
  }
  Serial.println("[I2C] Done.");
#endif
}

SensorData sensorsReadAll() {
  SensorData d = {};
  d.ahtTemp = NAN;
  d.ahtHumidity = NAN;
  d.lm75Temp = NAN;
  d.tempAvg = NAN;
  d.mq2Raw = -1;

  if (ens160.available()) {
    d.tvoc = ens160.getTVOC();
    d.eco2 = ens160.geteCO2();
    d.aqi = ens160.getAQI();
    d.ens160Connected = true;
  }

  sensors_event_t hum;
  sensors_event_t tmp;
  if (aht21.getEvent(&hum, &tmp)) {
    d.ahtTemp = tmp.temperature;
    d.ahtHumidity = hum.relative_humidity;
    d.aht21Connected = validateTemp(d.ahtTemp);
  }

  if (i2cPresent(LM75_I2C_ADDR)) {
    float t = lm75.readTemperatureC();
    if (validateTemp(t)) {
      d.lm75Temp = t;
      d.lm75Connected = true;
    }
  }

  if (d.lm75Connected && d.aht21Connected) {
    d.tempAvg = (d.lm75Temp + d.ahtTemp) / 2.0f;
    d.tempCrossCheckOk = (fabs(d.lm75Temp - d.ahtTemp) <= TEMP_DIFF_ERROR_C);
    if (!d.tempCrossCheckOk) {
      d.tempAvg = d.lm75Temp;
    }
  } else if (d.lm75Connected) {
    d.tempAvg = d.lm75Temp;
    d.tempCrossCheckOk = true;
  } else if (d.aht21Connected) {
    d.tempAvg = d.ahtTemp;
    d.tempCrossCheckOk = true;
  }

#if ENABLE_MQ2
  d.mq2Raw = analogRead(MQ2_ANALOG_PIN);
  d.mq2Ready = isMQ2Ready();
  d.mq2Connected = true;

  if (d.mq2Raw == 0) {
    mq2ZeroCount++;
  } else {
    mq2ZeroCount = 0;
  }

  if (d.mq2Raw >= 4095) {
    mq2MaxCount++;
  } else {
    mq2MaxCount = 0;
  }

  if (mq2ZeroCount >= 5 || mq2MaxCount >= 5) {
    d.mq2Connected = false;
  }

  if (!d.mq2Ready) {
    d.gasStatus = SENSOR_WARMUP;
#if DEBUG_SERIAL
    Serial.printf("[SENSOR] MQ2 warming %lus/300s\n",
                  (millis() - mq2WarmupStart) / 1000);
#endif
  }
#else
  d.mq2Raw = 0;
  d.mq2Ready = true;
  d.mq2Connected = true;
#endif

  d.airQualityStatus =
      d.ens160Connected ? evalAirQuality(d.tvoc, d.eco2, d.aqi) : SENSOR_ERROR;

  float tEval = !isnan(d.tempAvg)
                    ? d.tempAvg
                    : (d.lm75Connected ? d.lm75Temp
                                       : (d.aht21Connected ? d.ahtTemp : NAN));
  d.temperatureStatus = isnan(tEval) ? SENSOR_ERROR : evalTemperature(tEval);

#if ENABLE_MQ2
  if (d.gasStatus != SENSOR_WARMUP) {
    d.gasStatus = evalGas(d.mq2Raw);
  }
#else
  d.gasStatus = SENSOR_OK;
#endif

  auto flag = [&](bool c) {
    if (c) {
      d.anySensorError = true;
      d.errorCount++;
    }
  };

  flag(!d.ens160Connected);
  flag(!d.aht21Connected);
  flag(!d.lm75Connected);
#if ENABLE_MQ2
  flag(!d.mq2Connected);
#endif

  return d;
}

void sensorsPrint(const SensorData &d) {
#if DEBUG_SERIAL
  Serial.printf("[SENSOR] ENS160: TVOC=%u eCO2=%u AQI=%u conn=%d\n", d.tvoc,
                d.eco2, d.aqi, d.ens160Connected);
  Serial.printf("[SENSOR] AHT21: T=%.1fC H=%.1f%% conn=%d\n", d.ahtTemp,
                d.ahtHumidity, d.aht21Connected);
  Serial.printf("[SENSOR] LM75:  T=%.1fC conn=%d xcheck=%s\n", d.lm75Temp,
                d.lm75Connected, d.tempCrossCheckOk ? "OK" : "MISS");
  Serial.printf("[SENSOR] TempAvg=%.1fC | MQ2=%d ready=%d conn=%d\n", d.tempAvg,
                d.mq2Raw, d.mq2Ready, d.mq2Connected);
#endif
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    ALERT FUNCTIONS                           ║
// ╚══════════════════════════════════════════════════════════════╝

bool connectWiFi() {
#if ENABLE_TELEGRAM
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - t0) < WIFI_CONNECT_TIMEOUT_MS) {
    yield();
  }
  wifiConnected = (WiFi.status() == WL_CONNECTED);
#if DEBUG_SERIAL
  Serial.printf("[ALERT] WiFi %s\n",
                wifiConnected ? WiFi.localIP().toString().c_str() : "FAIL");
#endif
  return wifiConnected;
#else
  return false;
#endif
}

void checkWiFi() {
#if ENABLE_TELEGRAM
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiConnecting = false;
    return;
  }

  if (wifiConnecting) {
    if ((millis() - wifiConnectStart) >= WIFI_CONNECT_TIMEOUT_MS) {
      wifiConnecting = false;
      wifiConnected = false;
      lastWiFiCheck = millis();
#if DEBUG_SERIAL
      Serial.println("[WIFI] Reconnect timeout");
#endif
    }
    return;
  }

  if ((millis() - lastWiFiCheck) < WIFI_RECONNECT_INTERVAL) {
    return;
  }

  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiConnecting = true;
  wifiConnectStart = millis();
#if DEBUG_SERIAL
  Serial.println("[WIFI] Reconnecting (non-blocking)...");
#endif
#endif
}

void sendTelegram(const String &msg) {
#if ENABLE_TELEGRAM
  checkWiFi();
  if (bot && WiFi.status() == WL_CONNECTED) {
    bot->sendMessage(CHAT_ID, msg, "");
  }
#endif
}

void sendAlert(AlertLevel level, const String &message) {
  if (level == ALERT_NONE) {
    return;
  }

  // /stop chỉ tắt WARNING/DANGER tạm thời; CRITICAL vẫn phải xuyên qua.
  if (isSuppressionActive() && level < ALERT_CRITICAL) {
    return;
  }

  // Cập nhật mức cảnh báo ngay để buzzer/LED phản ứng tức thì,
  // kể cả khi Telegram vẫn đang trong thời gian cooldown.
  bool escalated = level > currentAlertLevel;
  bool shouldNotify = (level == ALERT_CRITICAL) || escalated ||
                      ((millis() - lastAlertTime) >= ALERT_COOLDOWN_MS);
  currentAlertLevel = level;

  if (level >= ALERT_WARNING) {
    digitalWrite(LED_PIN, HIGH);
  }
  if (level == ALERT_CRITICAL) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerState = true;
  }

  String prefix;
  switch (level) {
  case ALERT_INFO:
    prefix = "ℹ️ [INFO] ";
    break;
  case ALERT_WARNING:
    prefix = "⚠️ [CẢNH BÁO] ";
    break;
  case ALERT_DANGER:
    prefix = "🔴 [NGUY HIỂM] ";
    break;
  case ALERT_CRITICAL:
    prefix = "🚨 [KHẨN CẤP] ";
    break;
  default:
    break;
  }

  if (!shouldNotify) {
    return;
  }

  sendTelegram(prefix + message);
  lastAlertTime = millis();
}

void updateBuzzer() {
  switch (currentAlertLevel) {
  case ALERT_NONE:
  case ALERT_INFO:
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    buzzerState = false;
    break;
  case ALERT_WARNING:
    digitalWrite(LED_PIN, HIGH);
    if (millis() - lastBuzzerToggle > (buzzerState ? 200 : 800)) {
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState);
      lastBuzzerToggle = millis();
    }
    break;
  case ALERT_DANGER:
    digitalWrite(LED_PIN, HIGH);
    if (millis() - lastBuzzerToggle > (buzzerState ? 100 : 200)) {
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState);
      lastBuzzerToggle = millis();
    }
    break;
  case ALERT_CRITICAL:
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerState = true;
    break;
  }
}

void clearAlert() {
  currentAlertLevel = ALERT_NONE;
  buzzerState = false;
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

String checkTelegramCommand() {
#if !ENABLE_TELEGRAM
  return "";
#else
  if (!bot || (millis() - lastTelegramCheck) < TELEGRAM_CHECK_INTERVAL) {
    return "";
  }

  lastTelegramCheck = millis();
  checkWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    return "";
  }

  int n = bot->getUpdates(bot->last_message_received + 1);
  int totalProcessed = 0;
  while (n && totalProcessed < TELEGRAM_MAX_PER_CYCLE) {
    for (int i = 0; i < n && totalProcessed < TELEGRAM_MAX_PER_CYCLE; i++) {
      totalProcessed++;
      if (bot->messages[i].chat_id != CHAT_ID) {
        continue;
      }

      String t = bot->messages[i].text;
      t.trim();
      if (t == "/status") {
        return "status";
      }
      if (t == "/reset") {
        return "reset";
      }
      if (t == "/stop") {
        return "stop";
      }
    }

    if (totalProcessed >= TELEGRAM_MAX_PER_CYCLE) {
      break;
    }
    n = bot->getUpdates(bot->last_message_received + 1);
  }

  return "";
#endif
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    STATE MACHINE                             ║
// ╚══════════════════════════════════════════════════════════════╝

void processSystemLogic(const SensorData &d) {
  if (d.airQualityStatus == SENSOR_DANGER ||
      d.temperatureStatus == SENSOR_DANGER || d.gasStatus == SENSOR_DANGER) {
    currentState = STATE_SENSOR_ALERT;
    String m = "NGUY HIỂM!\nTVOC:" + String(d.tvoc) +
               " eCO2:" + String(d.eco2) + "\nTemp:" + String(d.tempAvg, 1) +
               "C MQ2:" + String(d.mq2Raw);
    if (d.anySensorError) {
      m += "\nSensor lỗi: " + String(d.errorCount);
    }
    sendAlert(ALERT_DANGER, m);
    return;
  }

  if (d.anySensorError) {
    if (currentState != STATE_SENSOR_ERROR) {
      currentState = STATE_SENSOR_ERROR;
      sendAlert(ALERT_WARNING,
                "Sensor lỗi (" + String(d.errorCount) + "). Kiểm tra phần cứng.");
    }
    return;
  }

  if (d.airQualityStatus == SENSOR_WARNING ||
      d.temperatureStatus == SENSOR_WARNING || d.gasStatus == SENSOR_WARNING) {
    currentState = STATE_SENSOR_ALERT;
    sendAlert(ALERT_WARNING, "Cảnh báo! TVOC:" + String(d.tvoc) +
                                 " Temp:" + String(d.tempAvg, 1) + "C");
    return;
  }

  if (currentState != STATE_NORMAL) {
    bool wasInit = (currentState == STATE_INIT);
    currentState = STATE_NORMAL;
    clearAlert();
    if (!wasInit) {
      sendTelegram("🟢 Tình hình ổn định.");
    }
#if DEBUG_SERIAL
    Serial.printf("[STATE] %s -> STATE_NORMAL\n", wasInit ? "INIT" : "ALERT");
#endif
  }
}

void handleTelegramCommand(const String &cmd) {
  if (cmd.isEmpty()) {
    return;
  }

  if (cmd == "status") {
    SensorData d = sensorsReadAll();
    String s = "📊 TRẠNG THÁI\n━━━━━━━━━━━━━━\n";
    s += "🌡 LM75:  " + String(d.lm75Temp, 1) + " C\n";
    s += "🌡 AHT21: " + String(d.ahtTemp, 1) + " C\n";
    s += "🌡 Avg:   " + String(d.tempAvg, 1) + " C\n";
    s += "💧 Humid: " + String(d.ahtHumidity, 1) + "%\n";
    s += "💨 TVOC:  " + String(d.tvoc) + " ppb\n";
    s += "💨 eCO2:  " + String(d.eco2) + " ppm\n";
    s += "💨 AQI:   " + String(d.aqi) + "/5\n";
    s += "🔥 MQ2:   " + String(d.mq2Raw) + "\n";
    s += "━━━━━━━━━━━━━━\nState: " + String(currentState);
    sendTelegram(s);
  } else if (cmd == "reset") {
    alertSuppressed = false;
    currentState = STATE_NORMAL;
    clearAlert();
    sendTelegram("🔄 Đã reset.");
  } else if (cmd == "stop") {
    clearAlert();
    alertSuppressed = true;
    suppressStartedAt = millis();
    sendTelegram("🔇 Tắt cảnh báo 60 giây.");
  }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    SETUP & LOOP                              ║
// ╚══════════════════════════════════════════════════════════════╝

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[MAIN] ═══ HỆ THỐNG PHÁT HIỆN KHÍ ĐỘC & NHIỆT ĐỘ ═══");
  Serial.println("[MAIN] Board: ESP32-S3 N16R8 | 16MB Flash | 8MB OPI PSRAM");
#endif

  lastAlertTime = millis() - ALERT_COOLDOWN_MS;

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  sensorsBegin();
  scanI2C();

#if ENABLE_TELEGRAM
  secureClient.setInsecure();
  bot = new UniversalTelegramBot(BOT_TOKEN, secureClient);
  connectWiFi();
  sendTelegram("🟢 Hệ thống đã khởi động! (ESP32-S3 N16R8)");
#endif

  currentState = STATE_INIT;
#if DEBUG_SERIAL
  Serial.println("[MAIN] ✅ Khởi tạo xong!");
  Serial.printf("[MAIN] Free SRAM: %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
#endif
}

void loop() {
  updateBuzzer();
  checkWiFi();

  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = millis();
    SensorData d = sensorsReadAll();
    processSystemLogic(d);
#if DEBUG_SERIAL
    sensorsPrint(d);
#endif
  }

#if ENABLE_TELEGRAM
  handleTelegramCommand(checkTelegramCommand());
#endif
}
