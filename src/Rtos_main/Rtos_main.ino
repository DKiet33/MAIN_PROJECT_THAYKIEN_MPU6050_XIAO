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
#include <driver/ledc.h>  // LEDC API trực tiếp — thay thế ESP32Servo (tránh lỗi hang attach trên Core v3.x)
#include <ScioSense_ENS160.h>
#include <Temperature_LM75_Derived.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

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
#define LM75_I2C_ADDR 0x49

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

// ══════ Feature Flags ══════
#define ENABLE_SERVO 1  // 0=skip servo (tránh hang khi chưa nối), 1=bật

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
  ALERT_CRITICAL,
  ALERT_FALL = 5
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

ScioSense_ENS160 ens160(ENS160_I2CADDR_0); // ADD=GND → 0x52
Adafruit_AHTX0 aht21;
Generic_LM75 lm75(LM75_I2C_ADDR);
// Servo: dùng Arduino Native LEDC API (Hỗ trợ đa phiên bản Core)
#define SERVO_LEDC_FREQ_HZ  50
// LƯU Ý PHẦN CỨNG: Bộ điều khiển LEDC của ESP32-S3 chỉ hỗ trợ độ phân giải tối đa 14-bit.
#define SERVO_LEDC_BITS     14

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  #define SERVO_LEDC_INIT()    (pinMode(SERVO_PIN, OUTPUT), ledcAttach(SERVO_PIN, SERVO_LEDC_FREQ_HZ, SERVO_LEDC_BITS))
  #define SERVO_LEDC_WRITE(d)  ledcWrite(SERVO_PIN, d)
#else
  #define SERVO_LEDC_CHANNEL   0
  #define SERVO_LEDC_INIT()    (pinMode(SERVO_PIN, OUTPUT), ledcSetup(SERVO_LEDC_CHANNEL, SERVO_LEDC_FREQ_HZ, SERVO_LEDC_BITS), ledcAttachPin(SERVO_PIN, SERVO_LEDC_CHANNEL), true)
  #define SERVO_LEDC_WRITE(d)  ledcWrite(SERVO_LEDC_CHANNEL, d)
#endif

// Chuyển đổi góc -> duty (50Hz): duty = pulse_us/20000.0 * max_duty
static inline uint32_t angleToDuty(int deg) {
  if (deg < 0) deg = 0;
  if (deg > 180) deg = 180;
  int pulse_us = 500 + (int)((float)deg * (2500.0f - 500.0f) / 180.0f);
  uint32_t max_duty = (1 << SERVO_LEDC_BITS) - 1;
  return (uint32_t)((float)pulse_us / 20000.0f * (float)max_duty);
}
static bool g_servoAttached = false;

// FreeRTOS primitives
static QueueHandle_t sensorQueue = nullptr;
static SemaphoreHandle_t alertMutex = nullptr;

// Shared state (protected by alertMutex)
static volatile AlertLevel    g_alertLevel       = ALERT_NONE;
static volatile bool          g_buzzerState      = false;
static volatile unsigned long g_lastBuzzerToggle = 0;
static volatile unsigned long g_lastFallTime     = 0;

struct __attribute__((packed)) FallAlertPacket {
  uint8_t alertType;      // 0xFA — cảnh báo té ngã
  uint32_t fallCount;     // Số lần phát hiện té ngã lũy kế từ thiết bị đeo
  float confidence;       // Độ tin cậy/xác suất nhận diện từ mô hình AI (0.00 - 1.00)
  uint32_t timestamp;     // Thời gian phát hiện (ms) trên thiết bị đeo
};

// Gói tin telemetry định kỳ từ XIAO — mỗi ~370ms trong chế độ INFERENCE
struct __attribute__((packed)) ImuTelemetryPacket {
  uint8_t  alertType;        // 0xFB
  float    aX, aY, aZ;       // Gia tốc (g) sau khi trừ offset hiệu chuẩn
  float    gX, gY, gZ;       // Vận tốc góc (deg/s)
  float    fall, idle, walk; // Điểm suy luận từ Edge Impulse (0.00-1.00)
  uint32_t timestamp;        // millis() tại XIAO
}; // 1 + 6×4 + 3×4 + 4 = 41 byte

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
  const uint8_t *mac_addr = recvInfo->src_addr;
#else
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
#endif
  (void)mac_addr; // tránh warning unused
  if (len < 1) return;
  uint8_t type = incomingData[0];

  // ── 0xFA: Cảnh báo té ngã ────────────────────────────────────────
  if (type == 0xFA && len == (int)sizeof(FallAlertPacket)) {
    FallAlertPacket packet;
    memcpy(&packet, incomingData, sizeof(packet));
    Serial.printf("[ESP-NOW] *** CANH BAO TE NGA! *** Luy ke: %u | Confidence: %.2f\n",
                  packet.fallCount, packet.confidence);
    if (xSemaphoreTake(alertMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      g_alertLevel   = ALERT_FALL;
      g_lastFallTime = millis();
      xSemaphoreGive(alertMutex);
    }
    return;
  }

  // ── 0xFB: Telemetry suy luận định kỳ (INFERENCE mode) ───────────
  if (type == 0xFB && len == (int)sizeof(ImuTelemetryPacket)) {
    ImuTelemetryPacket pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    const char *top = (pkt.fall > pkt.idle && pkt.fall > pkt.walk) ? "FALL"
                    : (pkt.walk > pkt.idle)                         ? "WALK" : "IDLE";
    Serial.printf("[WEARABLE] aX:%6.3f aY:%6.3f aZ:%6.3f | "
                  "fall:%.2f idle:%.2f walk:%.2f -> %s\n",
                  pkt.aX, pkt.aY, pkt.aZ,
                  pkt.fall, pkt.idle, pkt.walk, top);
  }
}

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

  // ENS160 — measure(true) bắt buộc để trigger đọc data mới
  if (ens160.available()) {
    ens160.measure(true);
    d.tvoc = ens160.getTVOC();
    d.eco2 = ens160.geteCO2();
    d.aqi  = ens160.getAQI();
    d.ens160Connected = true;
  }

  // AHT21
  sensors_event_t hum, tmp;
  if (aht21.getEvent(&hum, &tmp)) {
    d.ahtTemp     = tmp.temperature;
    d.ahtHumidity = hum.relative_humidity;
    d.aht21Connected = validateTemp(d.ahtTemp);

    // Bù nhiệt độ & độ ẩm vào ENS160 để tăng độ chính xác đo TVOC/eCO2
    if (d.ens160Connected)
      ens160.set_envdata(d.ahtTemp, d.ahtHumidity);
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
      // 1. Cảnh báo nguy hiểm từ cảm biến (DANGER/CRITICAL) luôn được quyền đè lên ALERT_FALL để bảo vệ tối thượng
      if (newLevel == ALERT_DANGER || newLevel == ALERT_CRITICAL) {
        g_alertLevel = newLevel;
      } else {
        // 2. Nếu không có DANGER/CRITICAL, chỉ cập nhật trạng thái nhẹ hơn nếu không ở ALERT_FALL hoặc cảnh báo ngã đã hết hạn (12 giây)
        if (g_alertLevel != ALERT_FALL || (millis() - g_lastFallTime >= 12000)) {
          g_alertLevel = newLevel;
        }
      }
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

#if ENABLE_SERVO
  // Khởi tạo Servo bằng Arduino Native LEDC Wrapper
  vTaskDelay(pdMS_TO_TICKS(500)); // chờ nguồn ổn định
#if DEBUG_SERIAL
  Serial.println("[ACTUATOR] Đang khởi tạo Servo (LEDC)..."); Serial.flush();
#endif
  g_servoAttached = SERVO_LEDC_INIT();
#if DEBUG_SERIAL
  Serial.printf("[ACTUATOR] Servo LEDC init: %s\n", g_servoAttached ? "OK" : "FAIL");
  Serial.flush();
#endif
  if (g_servoAttached) {
    // Đưa servo về góc mặc định ban đầu
    SERVO_LEDC_WRITE(angleToDuty(SERVO_ANGLE_NORMAL));
    lastAngle = SERVO_ANGLE_NORMAL;
  }
#endif

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
    case ALERT_FALL: // Khóa cứng Servo ở 0 độ và Quạt tắt khi có cảnh báo ngã
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

    // Chỉ ghi servo khi góc thay đổi và servo đã init
    if (g_servoAttached && targetAngle != lastAngle) {
      SERVO_LEDC_WRITE(angleToDuty(targetAngle));
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

    case ALERT_FALL:
      // Còi kêu liên tục, LED chớp tắt liên tục cực nhanh (100ms ON / 100ms OFF)
      digitalWrite(BUZZER_PIN, HIGH);
      g_buzzerState = true;
      if (now - g_lastBuzzerToggle >= 100) {
        static bool fallLedState = false;
        fallLedState = !fallLedState;
        digitalWrite(LED_PIN, fallLedState ? HIGH : LOW);
        g_lastBuzzerToggle = now;
      }
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
  
  // Đợi cổng Serial ảo (USB CDC) kết nối (tối đa 4s) để tránh trôi mất log boot
  uint32_t startMs = millis();
  while (!Serial && (millis() - startMs < 4000)) {
    delay(10);
  }

  Serial.println(
      "\n[MAIN] ═══ HỆ THỐNG PHÁT HIỆN KHÍ ĐỘC & NHIỆT ĐỘ (FreeRTOS) ═══");
  Serial.println("[MAIN] Board: ESP32-S3 N16R8 | 16MB Flash | 8MB OPI PSRAM");
  Serial.flush();  // đảm bảo in ra trước khi có thể crash
#endif

  // GPIO
#if DEBUG_SERIAL
  Serial.println("[INIT] GPIO pins..."); Serial.flush();
#endif
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW); // Fan OFF mặc định

  // I2C — init TRƯỚC servo để debug sensor trước
#if DEBUG_SERIAL
  Serial.println("[INIT] Wire.begin..."); Serial.flush();
#endif
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  Wire.setTimeOut(50); // 50ms timeout — tránh I2C hang khi bus bị stuck

#if DEBUG_SERIAL
  Serial.println("[INIT] I2C scan...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[INIT]   Found device at 0x%02X\n", addr);
    }
  }
#endif

#if DEBUG_SERIAL
  Serial.println("[INIT] ENS160.begin...");
#endif
  if (i2cPresent(ENS160_I2CADDR_0)) { // ADD=GND → 0x52
    ens160.begin();
    if (ens160.available())
      ens160.setMode(ENS160_OPMODE_STD);
  }

#if DEBUG_SERIAL
  Serial.println("[INIT] AHT21.begin...");
#endif
  aht21.begin();

#if DEBUG_SERIAL
  Serial.printf("[SENSOR] ENS160 %s\n", ens160.available() ? "OK" : "FAIL");
  Serial.printf("[SENSOR] AHT21  %s\n", aht21.begin() ? "OK" : "FAIL");
  Serial.printf("[SENSOR] LM75   %s\n",
                i2cPresent(LM75_I2C_ADDR) ? "OK" : "MISSING");
  Serial.printf("[MAIN] Free SRAM: %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.flush();
#endif

#if ENABLE_SERVO
  // Servo được chuyển sang khởi tạo trong TaskActuator để tránh treo lúc khởi động
#if DEBUG_SERIAL
  Serial.println("[INIT] Servo sẽ được khởi tạo trong TaskActuator..."); Serial.flush();
#endif
#else
#if DEBUG_SERIAL
  Serial.println("[INIT] Servo DISABLED (ENABLE_SERVO=0)"); Serial.flush();
#endif
#endif

  // ── ESP-NOW & Wi-Fi Setup ─────────────────────────────────────
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Ép phần cứng chạy Kênh 1 đồng bộ với Thiết bị đeo
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  Serial.printf("[WIFI] Địa chỉ MAC STA của Trạm chính: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (esp_now_init() == ESP_OK) {
    Serial.println("[ESP-NOW] Khởi tạo thành công!");
    esp_now_register_recv_cb(OnDataRecv);
  } else {
    Serial.println("[ESP-NOW] Khởi tạo THẤT BẠI!");
  }

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
                          nullptr, 3, nullptr, 1);

#if DEBUG_SERIAL
  Serial.println("[MAIN] ✅ FreeRTOS tasks started!");
#endif
}

// loop() nhường hết cho FreeRTOS scheduler
void loop() { vTaskDelay(portMAX_DELAY); }
