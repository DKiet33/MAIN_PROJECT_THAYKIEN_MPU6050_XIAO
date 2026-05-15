// ════════════════════════════════════════════════════════════════
// File: Rtos_main.ino
// Purpose: Hệ thống phát hiện khí độc và nhiệt độ — phiên bản FreeRTOS
// Board: ESP32-S3 N16R8 (16MB OPI Flash + 8MB OPI PSRAM, 44-pin)
// Loại bỏ: Telegram Bot, MQ2
// Giữ lại: ENS160, AHT21, LM75, Buzzer, LED, Servo, Fan 12V
//
// Arduino IDE Settings:
//   Board:            ESP32S3 Dev Module
//   Flash Size:       16MB (128Mb)
//   Flash Mode:       DIO 80MHz
//   PSRAM:            OPI PSRAM
//   Partition:        16M Flash (3MB APP/9.9MB FATFS)
//   CPU Frequency:    240MHz
//
// FreeRTOS Task Map:
//   TaskSensorRead   Core1 Pri3  — Đọc cảm biến mỗi 1s → sensorQueue
//   TaskAlertManager Core1 Pri4  — Nhận data → tính AlertLevel
//   TaskBuzzerLED    Core1 Pri5  — Điều khiển Buzzer & LED theo AlertLevel
//   TaskActuator     Core0 Pri3  — Điều khiển Servo + Quạt 12V (NPN 2N2222)
//
// Actuator Logic:
//   ALERT_NONE/INFO     → Servo 0°   | Fan OFF
//   ALERT_WARNING       → Servo 90°  | Fan ON
//   ALERT_DANGER/CRITICAL → Servo 180° | Fan ON
//
// Fan wiring (NPN 2N2222):
//   ESP32 GPIO6 → 1kΩ → Base (2N2222)
//   Collector   → Fan (-) [+ flyback diode 1N4007 song song]
//   Emitter     → GND
//   Fan (+)     → 12V
//   GPIO HIGH = Fan ON (NPN saturation)
//
// NOTE: Fall Detection (XIAO ESP32-S3 + MPU6050 + Edge Impulse)
//   → Đang được dev khác thực hiện, sẽ tich hợp qua WiFi hoặc ESP-NOW.
// ════════════════════════════════════════════════════════════════

#include <Adafruit_AHTX0.h>
#include <Arduino.h>
#include <ESP32Servo.h>
#include <ScioSense_ENS160.h>
#include <Temperature_LM75_Derived.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// ╔══════════════════════════════════════════════════════════════╗
// ║                    CẤU HÌNH (CONFIG)                         ║
// ╚══════════════════════════════════════════════════════════════╝

// ══════ GPIO — I2C ══════
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// ══════ GPIO — Output ══════
#define BUZZER_PIN 2
#define LED_PIN 4
#define SERVO_PIN 5 // PWM 50Hz — ESP32Servo
#define FAN_PIN 6   // NPN 2N2222 base: HIGH=ON, LOW=OFF

// ══════ Servo Angles ══════
#define SERVO_ANGLE_NORMAL 0   // Bình thường: đóng
#define SERVO_ANGLE_WARN 90    // Cảnh báo: mở một phần
#define SERVO_ANGLE_DANGER 180 // Nguy hiểm: mở hoàn toàn

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

// ══════ Timing ══════
#define SENSOR_READ_INTERVAL_MS 1000

// ══════ Debug ══════
#define DEBUG_SERIAL 1

// ══════ FreeRTOS Config ══════
#define SENSOR_QUEUE_LEN 5 // số SensorData tối đa trong queue
#define TASK_STACK_SENSOR 4096
#define TASK_STACK_ALERT 4096
#define TASK_STACK_BUZZ 2048
#define TASK_STACK_ACTUATOR 3072

// ╔══════════════════════════════════════════════════════════════╗
// ║                    ENUMS & STRUCTS                           ║
// ╚══════════════════════════════════════════════════════════════╝

enum SensorStatus {
  SENSOR_OK = 0,
  SENSOR_WARNING,
  SENSOR_DANGER,
  SENSOR_ERROR
};

enum AlertLevel {
  ALERT_NONE = 0,
  ALERT_INFO,
  ALERT_WARNING,
  ALERT_DANGER,
  ALERT_CRITICAL
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

  SensorStatus airQualityStatus;
  SensorStatus temperatureStatus;

  bool anySensorError;
  uint8_t errorCount;
};

// ╔══════════════════════════════════════════════════════════════╗
// ║                    GLOBAL OBJECTS                            ║
// ╚══════════════════════════════════════════════════════════════╝

ScioSense_ENS160 ens160(ENS160_I2CADDR_1);
Adafruit_AHTX0 aht21;
Generic_LM75 lm75(LM75_I2C_ADDR);
Servo servo;

// FreeRTOS primitives
static QueueHandle_t sensorQueue = nullptr;
static SemaphoreHandle_t alertMutex = nullptr;

// Shared state (protected by alertMutex)
static volatile AlertLevel    g_alertLevel       = ALERT_NONE;
static volatile bool          g_buzzerState      = false;
static volatile unsigned long g_lastBuzzerToggle = 0;

// ╔══════════════════════════════════════════════════════════════╗
// ║                    HELPER FUNCTIONS                          ║
// ╚══════════════════════════════════════════════════════════════╝

static bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool validateTemp(float t) {
  return !isnan(t) && !isinf(t) && t >= TEMP_ERROR_MIN_C &&
         t <= TEMP_ERROR_MAX_C;
}

static SensorStatus evalAirQuality(uint16_t tvoc, uint16_t eco2, uint8_t aqi) {
  if (tvoc >= TVOC_DANGER_PPB || eco2 >= ECO2_DANGER_PPM || aqi >= AQI_DANGER)
    return SENSOR_DANGER;
  if (tvoc >= TVOC_WARN_PPB || eco2 >= ECO2_WARN_PPM || aqi >= AQI_WARN)
    return SENSOR_WARNING;
  return SENSOR_OK;
}

static SensorStatus evalTemperature(float t) {
  if (!validateTemp(t))
    return SENSOR_ERROR;
  if (t >= TEMP_DANGER_C)
    return SENSOR_DANGER;
  if (t >= TEMP_WARN_C)
    return SENSOR_WARNING;
  return SENSOR_OK;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    SENSOR READ                               ║
// ╚══════════════════════════════════════════════════════════════╝

static SensorData readAllSensors() {
  SensorData d = {};
  d.ahtTemp = NAN;
  d.ahtHumidity = NAN;
  d.lm75Temp = NAN;
  d.tempAvg = NAN;

  // ENS160
  if (ens160.available()) {
    d.tvoc = ens160.getTVOC();
    d.eco2 = ens160.geteCO2();
    d.aqi = ens160.getAQI();
    d.ens160Connected = true;
  }

  // AHT21
  sensors_event_t hum, tmp;
  if (aht21.getEvent(&hum, &tmp)) {
    d.ahtTemp = tmp.temperature;
    d.ahtHumidity = hum.relative_humidity;
    d.aht21Connected = validateTemp(d.ahtTemp);
  }

  // LM75
  if (i2cPresent(LM75_I2C_ADDR)) {
    float t = lm75.readTemperatureC();
    if (validateTemp(t)) {
      d.lm75Temp = t;
      d.lm75Connected = true;
    }
  }

  // Temperature fusion
  if (d.lm75Connected && d.aht21Connected) {
    d.tempAvg = (d.lm75Temp + d.ahtTemp) / 2.0f;
    d.tempCrossCheckOk = (fabsf(d.lm75Temp - d.ahtTemp) <= TEMP_DIFF_ERROR_C);
    if (!d.tempCrossCheckOk)
      d.tempAvg = d.lm75Temp;
  } else if (d.lm75Connected) {
    d.tempAvg = d.lm75Temp;
    d.tempCrossCheckOk = true;
  } else if (d.aht21Connected) {
    d.tempAvg = d.ahtTemp;
    d.tempCrossCheckOk = true;
  }

  // Evaluate statuses
  d.airQualityStatus =
      d.ens160Connected ? evalAirQuality(d.tvoc, d.eco2, d.aqi) : SENSOR_ERROR;

  float tEval = !isnan(d.tempAvg)
                    ? d.tempAvg
                    : (d.lm75Connected ? d.lm75Temp
                                       : (d.aht21Connected ? d.ahtTemp : NAN));
  d.temperatureStatus = isnan(tEval) ? SENSOR_ERROR : evalTemperature(tEval);

  // Error summary
  auto flag = [&](bool cond) {
    if (cond) {
      d.anySensorError = true;
      d.errorCount++;
    }
  };
  flag(!d.ens160Connected);
  flag(!d.aht21Connected);
  flag(!d.lm75Connected);

  return d;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    FREERTOS TASKS                            ║
// ╚══════════════════════════════════════════════════════════════╝

// ─── Task 1: Sensor Read ───────────────────────────────────────
// Core1 | Priority 3
// Đọc tất cả cảm biến mỗi SENSOR_READ_INTERVAL_MS và đẩy vào queue.
void TaskSensorRead(void * /*pvParam*/) {
  TickType_t xLastWake = xTaskGetTickCount();
  const TickType_t xInterval = pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS);

  for (;;) {
    SensorData d = readAllSensors();

    // Nếu queue đầy, xóa item cũ nhất rồi gửi item mới
    if (uxQueueSpacesAvailable(sensorQueue) == 0) {
      SensorData discard;
      xQueueReceive(sensorQueue, &discard, 0);
    }
    xQueueSend(sensorQueue, &d, 0);

#if DEBUG_SERIAL
    Serial.printf("[SENSOR] ENS160: TVOC=%u eCO2=%u AQI=%u conn=%d\n", d.tvoc,
                  d.eco2, d.aqi, d.ens160Connected);
    Serial.printf("[SENSOR] AHT21:  T=%.1fC H=%.1f%% conn=%d\n", d.ahtTemp,
                  d.ahtHumidity, d.aht21Connected);
    Serial.printf("[SENSOR] LM75:   T=%.1fC conn=%d xcheck=%s\n", d.lm75Temp,
                  d.lm75Connected, d.tempCrossCheckOk ? "OK" : "MISS");
    Serial.printf("[SENSOR] TempAvg=%.1fC\n", d.tempAvg);
#endif

    vTaskDelayUntil(&xLastWake, xInterval);
  }
}

// ─── Task 2: Alert Manager ─────────────────────────────────────
// Core1 | Priority 4
// Nhận SensorData từ queue, tính AlertLevel mới, ghi vào g_alertLevel.
void TaskAlertManager(void * /*pvParam*/) {
  SensorData d;

  for (;;) {
    // Chặn tối đa 2s chờ data từ sensor task
    if (xQueueReceive(sensorQueue, &d, pdMS_TO_TICKS(2000)) != pdTRUE) {
      continue;
    }

    AlertLevel newLevel = ALERT_NONE;

    // DANGER trumps everything
    if (d.airQualityStatus == SENSOR_DANGER ||
        d.temperatureStatus == SENSOR_DANGER) {
      newLevel = ALERT_DANGER;
    } else if (d.anySensorError) {
      newLevel = ALERT_WARNING;
    } else if (d.airQualityStatus == SENSOR_WARNING ||
               d.temperatureStatus == SENSOR_WARNING) {
      newLevel = ALERT_WARNING;
    } else {
      newLevel = ALERT_NONE;
    }

    // Ghi AlertLevel mới (mutex bảo vệ)
    if (xSemaphoreTake(alertMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      g_alertLevel = newLevel;
      xSemaphoreGive(alertMutex);
    }

#if DEBUG_SERIAL
    const char *lvlStr[] = {"NONE", "INFO", "WARNING", "DANGER", "CRITICAL"};
    Serial.printf("[ALERT] Level -> %s\n", lvlStr[newLevel]);
#endif
  }
}

// ─── Task 3: Actuator (Servo + Fan) ───────────────────────────
// Core0 | Priority 3
// Đọc g_alertLevel, điều khiển servo và quạt 12V (NPN 2N2222).
// Servo dùng ESP32Servo (PWM 50Hz). Fan: GPIO6 HIGH=ON.
void TaskActuator(void * /*pvParam*/) {
  const TickType_t xInterval = pdMS_TO_TICKS(200); // cập nhật mỗi 200ms
  TickType_t xLastWake = xTaskGetTickCount();

  int lastAngle = -1; // theo dõi thay đổi để tránh ghi servo liên tục
  bool lastFan = false;

  for (;;) {
    AlertLevel lvl = ALERT_NONE;
    if (xSemaphoreTake(alertMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      lvl = g_alertLevel;
      xSemaphoreGive(alertMutex);
    }

    int targetAngle;
    bool fanOn;

    switch (lvl) {
    case ALERT_NONE:
    case ALERT_INFO:
      targetAngle = SERVO_ANGLE_NORMAL;
      fanOn = false;
      break;
    case ALERT_WARNING:
      targetAngle = SERVO_ANGLE_WARN;
      fanOn = true;
      break;
    case ALERT_DANGER:
    case ALERT_CRITICAL:
    default:
      targetAngle = SERVO_ANGLE_DANGER;
      fanOn = true;
      break;
    }

    // Chỉ ghi servo khi góc thay đổi để tránh jitter
    if (targetAngle != lastAngle) {
      servo.write(targetAngle);
      lastAngle = targetAngle;
#if DEBUG_SERIAL
      Serial.printf("[ACTUATOR] Servo → %d°\n", targetAngle);
#endif
    }

    // Quạt: NPN 2N2222 — HIGH = ON
    if (fanOn != lastFan) {
      digitalWrite(FAN_PIN, fanOn ? HIGH : LOW);
      lastFan = fanOn;
#if DEBUG_SERIAL
      Serial.printf("[ACTUATOR] Fan → %s\n", fanOn ? "ON" : "OFF");
#endif
    }

    vTaskDelayUntil(&xLastWake, xInterval);
  }
}

// ─── Task 4: Buzzer & LED ──────────────────────────────────────
// Core1 | Priority 5 (ưu tiên cao nhất để phản hồi phần cứng kịp thời)
// Đọc g_alertLevel và điều khiển pattern Buzzer + LED.
void TaskBuzzerLED(void * /*pvParam*/) {
  const TickType_t xInterval = pdMS_TO_TICKS(50); // kiểm tra mỗi 50ms
  TickType_t xLastWake = xTaskGetTickCount();

  for (;;) {
    AlertLevel lvl = ALERT_NONE;
    if (xSemaphoreTake(alertMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      lvl = g_alertLevel;
      xSemaphoreGive(alertMutex);
    }

    unsigned long now = millis();

    switch (lvl) {
    case ALERT_NONE:
    case ALERT_INFO:
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
      g_buzzerState = false;
      break;

    case ALERT_WARNING:
      // LED sáng, buzzer nhấp nháy chậm: 200ms ON / 800ms OFF
      digitalWrite(LED_PIN, HIGH);
      {
        unsigned long onTime = g_buzzerState ? 200 : 800;
        if ((now - g_lastBuzzerToggle) >= onTime) {
          g_buzzerState = !g_buzzerState;
          digitalWrite(BUZZER_PIN, g_buzzerState ? HIGH : LOW);
          g_lastBuzzerToggle = now;
        }
      }
      break;

    case ALERT_DANGER:
      // LED sáng, buzzer nhấp nháy nhanh: 100ms ON / 200ms OFF
      digitalWrite(LED_PIN, HIGH);
      {
        unsigned long onTime = g_buzzerState ? 100 : 200;
        if ((now - g_lastBuzzerToggle) >= onTime) {
          g_buzzerState = !g_buzzerState;
          digitalWrite(BUZZER_PIN, g_buzzerState ? HIGH : LOW);
          g_lastBuzzerToggle = now;
        }
      }
      break;

    case ALERT_CRITICAL:
      // LED + Buzzer bật liên tục
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
      g_buzzerState = true;
      break;
    }

    vTaskDelayUntil(&xLastWake, xInterval);
  }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║                    SETUP & LOOP                              ║
// ╚══════════════════════════════════════════════════════════════╝

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
  delay(200);
  Serial.println(
      "\n[MAIN] ═══ HỆ THỐNG PHÁT HIỆN KHÍ ĐỘC & NHIỆT ĐỘ (FreeRTOS) ═══");
  Serial.println("[MAIN] Board: ESP32-S3 N16R8 | 16MB Flash | 8MB OPI PSRAM");
#endif

  // GPIO
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW); // Fan OFF mặc định

  // Servo
  servo.setPeriodHertz(50);           // 50Hz — tiêu chuẩn servo RC
  servo.attach(SERVO_PIN, 500, 2500); // pulse: 500μs (0°) đến 2500μs (180°)
  servo.write(SERVO_ANGLE_NORMAL);    // vị trí khởi đầu: 0°

  // I2C + Sensors
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  ens160.begin();
  if (ens160.available())
    ens160.setMode(ENS160_OPMODE_STD);
  aht21.begin();

#if DEBUG_SERIAL
  Serial.printf("[SENSOR] ENS160 %s\n", ens160.available() ? "OK" : "FAIL");
  Serial.printf("[SENSOR] AHT21  %s\n", aht21.begin() ? "OK" : "FAIL");
  Serial.printf("[SENSOR] LM75   %s (0x48)\n",
                i2cPresent(LM75_I2C_ADDR) ? "OK" : "MISSING");
  Serial.printf("[MAIN] Free SRAM: %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
#endif

  // FreeRTOS primitives
  sensorQueue = xQueueCreate(SENSOR_QUEUE_LEN, sizeof(SensorData));
  alertMutex = xSemaphoreCreateMutex();

  configASSERT(sensorQueue != nullptr);
  configASSERT(alertMutex != nullptr);

  // Tạo tasks
  xTaskCreatePinnedToCore(TaskSensorRead, "SensorRead", TASK_STACK_SENSOR,
                          nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(TaskAlertManager, "AlertManager", TASK_STACK_ALERT,
                          nullptr, 4, nullptr, 1);
  xTaskCreatePinnedToCore(TaskBuzzerLED, "BuzzerLED", TASK_STACK_BUZZ, nullptr,
                          5, nullptr, 1);
  xTaskCreatePinnedToCore(TaskActuator, "Actuator", TASK_STACK_ACTUATOR,
                          nullptr, 3, nullptr, 0);

#if DEBUG_SERIAL
  Serial.println("[MAIN] ✅ FreeRTOS tasks started!");
#endif
}

// loop() nhường hết cho FreeRTOS scheduler
void loop() { vTaskDelay(portMAX_DELAY); }
