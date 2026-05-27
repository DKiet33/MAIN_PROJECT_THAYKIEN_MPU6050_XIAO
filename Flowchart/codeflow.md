# Codeflow — Rtos_main.ino & wearable_unified_rtos.ino (FreeRTOS + ESP-NOW)

## Trang Thai Du An: 2026-05-27

| Board | File | Trang thai |
|---|---|---|
| Main Station (ESP32-S3 N16R8) | `src/Rtos_main/Rtos_main.ino` | Code xong, ESP-NOW RX tich hop, **test thành công các thiết bị Quạt / LED / Buzzer / Servo** |
| Wearable (XIAO ESP32-S3) | `src/wearable/wearable_unified_rtos/wearable_unified_rtos.ino` | Code xong, ESP-NOW TX tich hop |
| Ket noi 2 board qua ESP-NOW | — | **DA TEST THUC TE THANH CONG** |
| **Cac thiet bi ngoai vi** | GPIO5, GPIO6, LED, Buzzer | **test thành công các thiết bị Quạt / LED / Buzzer / Servo** |

---

## PHAN 1: TRAM CHINH — Rtos_main.ino

### 1.1. Cau truc tong quan

| Thanh phan | Mo ta |
|---|---|
| **Config** | Khai bao chan GPIO, nguong canh bao, timing, FreeRTOS stack sizes |
| **Enums/Structs** | `SensorStatus`, `AlertLevel` (0-5), `SensorData`, `FallAlertPacket` |
| **Global state** | Queue, Mutex, shared volatile vars (`g_alertLevel`, `g_buzzerState`, `g_lastBuzzerToggle`, `g_lastFallTime`) |
| **ESP-NOW** | `OnDataRecv` callback, WiFi STA + Channel 1 lock |
| **Helper functions** | `i2cPresent()`, `validateTemp()`, `evalAirQuality()`, `evalTemperature()` |
| **Sensor functions** | `readAllSensors()`: Doc ENS160 + AHT21 + LM75, tinh `tempAvg`, danh gia trang thai |
| **FreeRTOS Tasks** | 4 tasks doc lap chay song song |
| **setup()** | Init GPIO/Servo/I2C/Sensor, WiFi STA + esp_now_init(), tao Queue+Mutex, spawn 4 tasks |
| **loop()** | `vTaskDelay(portMAX_DELAY)` — nhuong hoan toan cho scheduler |

### 1.2. GPIO / Phan cung

| Bien | Pin | Chuc nang |
|---|---|---|
| `I2C_SDA_PIN` | GPIO 8 | I2C Data |
| `I2C_SCL_PIN` | GPIO 9 | I2C Clock (400 kHz) |
| `BUZZER_PIN` | GPIO 2 | Coi bao (OUTPUT) |
| `LED_PIN` | GPIO 4 | Den bao (OUTPUT) |
| `SERVO_PIN` | GPIO 5 | Servo RC — PWM 50Hz, pulse 500-2500 us |
| `FAN_PIN` | GPIO 6 | Quat 12V qua NPN 2N2222: HIGH=ON, LOW=OFF |
| `LM75_I2C_ADDR` | 0x48 | Dia chi I2C cua LM75 |

### 1.3. Nguong canh bao (Thresholds)

| Thong so | WARNING | DANGER |
|---|---|---|
| TVOC (ENS160) | >= 150 ppb | >= 500 ppb |
| eCO2 (ENS160) | >= 800 ppm | >= 1500 ppm |
| AQI (ENS160) | >= 3 | >= 4 |
| Nhiet do (tempAvg) | >= 45.0 C | >= 60.0 C |
| Nhiet do hop le | -40 C den 125 C | (ngoai dai -> SENSOR_ERROR) |
| Cross-check LM75 vs AHT21 | Chenh lech > 15 C → dung LM75 lam gia tri chinh | |

### 1.4. AlertLevel Enum (cap nhat co ALERT_FALL)

| Gia tri | Ten | Mo ta |
|---|---|---|
| 0 | `ALERT_NONE` | Binh thuong |
| 1 | `ALERT_INFO` | Thong tin (chua dung) |
| 2 | `ALERT_WARNING` | Canh bao nhe: cam bien loi / nguong nhe |
| 3 | `ALERT_DANGER` | Nguy hiem: khi doc / nhiet cao |
| 4 | `ALERT_CRITICAL` | Nguy hiem toi do |
| 5 | `ALERT_FALL` | **Nhan tin nha tu Wearable qua ESP-NOW** |

### 1.5. FreeRTOS Task Map

| Task | Core | Priority | Chu ky | Chuc nang |
|---|---|---|---|---|
| `TaskSensorRead` | Core 1 | 3 | 1000 ms | Doc ENS160 + AHT21 + LM75, gui vao `sensorQueue` |
| `TaskAlertManager` | Core 1 | 4 | Block cho queue (timeout 2s) | Nhan `SensorData`, tinh `AlertLevel` moi, ghi `g_alertLevel` qua `alertMutex` voi logic Latching+Overwrite |
| `TaskBuzzerLED` | Core 1 | 5 | 50 ms | Doc `g_alertLevel`, dieu khien Buzzer + LED theo pattern |
| `TaskActuator` | Core 0 | 3 | 200 ms | Doc `g_alertLevel`, dieu khien Servo + Quat 12V |

### 1.6. Luong du lieu (Data Flow)

```
[ESP-NOW OnDataRecv — ISR callback]
    Nhan FallAlertPacket tu Wearable
    alertType == 0xFA → g_alertLevel = ALERT_FALL, g_lastFallTime = millis()
         |
         | (alertMutex)
         |
[TaskSensorRead]  Core1 Pri3
    readAllSensors() moi 1000ms
    Neu queue day → xoa item cu nhat → gui item moi
    Writes: sensorQueue (xQueueSend)
         |
         v
[TaskAlertManager]  Core1 Pri4
    Reads: sensorQueue (xQueueReceive, blocking 2s)
    Tinh newLevel tu sensor:
      DANGER/CRITICAL  ← cam bien khi doc / nhiet cao
      WARNING          ← mat cam bien / nguong nhe
      NONE             ← binh thuong
    Logic ghi g_alertLevel (Latching + Danger Overwrite):
      if (newLevel == DANGER || newLevel == CRITICAL) → g_alertLevel = newLevel (de len FALL)
      else if (g_alertLevel != ALERT_FALL || millis()-g_lastFallTime >= 12000) → g_alertLevel = newLevel
    Writes: g_alertLevel (qua alertMutex)
         |
    +----+----+
    |         |
    v         v
[TaskBuzzerLED]    [TaskActuator]
Core1 Pri5 / 50ms   Core0 Pri3 / 200ms
Reads: g_alertLevel  Reads: g_alertLevel
Pattern buzzer/LED   Controls Servo + Fan
(qua alertMutex)     (qua alertMutex)
```

### 1.7. Pattern Buzzer/LED (TaskBuzzerLED — 50ms tick)

| AlertLevel | LED | Buzzer |
|---|---|---|
| NONE / INFO | OFF | OFF |
| FALL | Nhap nhay 5Hz: 100ms ON / 100ms OFF | BAT LIEN TUC (HIGH) |
| WARNING | ON | Nhap nhay cham: 200 ms ON / 800 ms OFF |
| DANGER | ON | Nhap nhay nhanh: 100 ms ON / 200 ms OFF |
| CRITICAL | ON | Bat lien tuc (HIGH) |

### 1.8. Logic Actuator (TaskActuator — 200ms tick)

| AlertLevel | Servo | Quat 12V (GPIO6) |
|---|---|---|
| NONE / INFO / **FALL** | 0° (dong) | OFF — **Khong bat quat khi chi nhan tin nga** |
| WARNING | 90° (mo mot phan) | ON |
| DANGER / CRITICAL | 180° (mo hoan toan) | ON |

> **Ghi chu ky thuat Servo (QUAN TRONG):** Su dung Arduino Native LEDC API (`ledcAttach` / `ledcWrite`) thay vi `ESP32Servo` hoac ESP-IDF `driver/ledc.h` truc tiep.
> - ESP32-S3 chi ho tro LEDC toi da **14-bit** (khong phai 16-bit nhu ESP32 the he dau).
> - Formula: `duty = (pulse_us / 20000.0) * (2^14 - 1)`, pulse range 500-2500us.
> - Macro tuong thich da phien ban Core v2.x / v3.x thong qua `#if ESP_ARDUINO_VERSION`.

> **Danger Overwrite:** Neu DANGER/CRITICAL xuat hien trong luc ALERT_FALL → g_alertLevel bi de len DANGER/CRITICAL ngay → Servo 180°, Quat ON.

### 1.9. Goi tin ESP-NOW (FallAlertPacket)

```cpp
struct __attribute__((packed)) FallAlertPacket {
  uint8_t  alertType;    // 0xFA — ma co dinh cho canh bao nga
  uint32_t fallCount;    // so lan phat hien nga luy ke
  float    confidence;   // xac suat tu AI (0.00-1.00)
  uint32_t timestamp;    // millis() tai thoi diem phat hien
};
// Tong kich thuoc: 1 + 4 + 4 + 4 = 13 byte
```

---

## PHAN 2: THIET BI DEO — wearable_unified_rtos.ino

### 2.1. FreeRTOS Task Map (Wearable)

| Task | Core | Priority | Stack | Chuc nang |
|------|------|----------|-------|----------|
| `TaskIMURead` | Core 1 | 5 (cao nhat) | 4096 B | Doc MPU6050 chinh xac moi 10ms (100Hz). Day `ImuData` vao `imuQueue`. Cap nhat web snapshot. |
| `TaskEdgeAI` | Core 1 | 4 (cao) | 4096 B | Pop tu `imuQueue`, lap day sliding window, chay inference Edge Impulse, Debounce+Cooldown, **phat ESP-NOW** khi xac nhan nga. |
| `TaskNetworkWeb` | Core 0 | 3 (trung binh) | 6144 B | Xu ly `server.handleClient()`, INGESTION state machine, HTTPS Upload len Edge Impulse Studio. |

### 2.2. FreeRTOS Primitives

| Primitive | Loai | Bao ve |
|-----------|-------|--------|
| `imuQueue` | `QueueHandle_t` (30 o, sizeof `ImuData`) | Truyen du lieu cam bien tu `TaskIMURead` sang `TaskEdgeAI` va `TaskNetworkWeb` (khi COLLECTING). |
| `stateMutex` | `SemaphoreHandle_t` (mutex) | Bao ve `operationMode` va `state` khi duoc doc/ghi dong thoi tu Web handler va tasks. |

### 2.3. Cau hinh va Hang so chinh (ei_config.h)

- **Tan so lay mau:** 100 Hz (chu ky 10ms)
- **Kich thuoc mau:** 300 mau (3 giay du lieu lien tuc)
- **Thang do cam bien:** Gia toc +/-8g (ACCEL_SCALE=4096.0f), Van toc goc +/-500 deg/s (GYRO_SCALE=65.5f)
- **Bo loc chong bao gia:**
  - `FALL_ALERT_THRESHOLD = 0.85f`
  - `FALL_CONFIRM_SLICES = 3`
  - `FALL_COOLDOWN_MS = 6000`

### 2.4. Luong ESP-NOW TX trong TaskEdgeAI

```
[TaskEdgeAI — Core1 Pri4]
  Nhan ImuData tu imuQueue (blocking 20ms)
  → Loi → continue
  
  Ghi vao sliding window inferBuf
  Khi du SLICE_SIZE mau:
    memmove dich du lieu cu
    neu inferBufFull:
      run_classifier() → fall / idle / walk scores
      
      [Debounce + Cooldown]
      fall >= FALL_ALERT_THRESHOLD (0.85)?
        fallConfirm++
        fallConfirm >= FALL_CONFIRM_SLICES (2)
        && millis() - lastAlertMs >= FALL_COOLDOWN_MS (6000)?
          → fallConfirm = 0, lastAlertMs = millis()
          → fall_count++
          → In log CANH BAO TE NGA
          
          [PHAT ESP-NOW — 3x]
          FallAlertPacket packet {0xFA, fall_count, confidence, millis()}
          for i in 0..2:
            esp_now_send(MAIN_BOARD_MAC, &packet, sizeof(packet))
            vTaskDelay(5ms)
          → logPrintf [ESP-NOW] Da phat 3x
          
          → ledOn()
```

### 2.5. Wi-Fi va ESP-NOW Setup (Wearable setup())

```
WiFi.mode(WIFI_AP_STA)
WiFi.softAP("Wearable_AP", "12345678", channel=1)   // Khoa kenh 1 cho ESP-NOW
WiFi.begin(EI_WIFI_SSID, EI_WIFI_PASSWORD)           // Ket noi router qua STA
initEspNow():
  esp_now_init()
  esp_now_add_peer(MAIN_BOARD_MAC, channel=1)
```

### 2.6. Wi-Fi va ESP-NOW Setup (Main Station setup())

```
WiFi.mode(WIFI_STA)
WiFi.disconnect()
esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)        // Ep kenh 1 dong bo voi Wearable
esp_wifi_get_mac(WIFI_IF_STA, mac) → In ra Serial     // Hien MAC de nguoi dung copy vao MAIN_BOARD_MAC
esp_now_init()
esp_now_register_recv_cb(OnDataRecv)
```

### 2.7. Danh sach API Web Server cua Thiet bi deo

| Endpoint | Method | Chuc nang |
|---|---|---|
| `/` | GET | Tra ve trang Web dieu khien - nen PROGMEM tu `html_page.h` |
| `/status` | GET | JSON trang thai hoat dong, thong so IMU, so lan nga (`fall_count`) |
| `/infer-status` | GET | JSON ket qua suy luan hien tai (fall/idle/walk + XYZ) |
| `/setmode?mode=...` | GET | Thay doi che do (`inference` hoac `ingestion`) |
| `/calibrate` | GET | Hieu chuan lay offset tinh MPU6050 |
| `/trigger` | GET | Bat dau thu mau mot bat thu cong |
| `/autorun?count=N` | GET | Bat dau N bat thu tu dong |
| `/stoprun` | GET | Dung auto-run |
| `/setlabel?label=...` | GET | Doi nhan du lieu (fall/idle/walk) |
| `/setaugment` | GET | Bat/tat Scale 3x augmentation |
| `/setjitter` | GET | Bat/tat Jitter noise augmentation |
| `/upload-manual` | POST | Upload thu cong CSV data len Edge Impulse |
| `/log` | GET | 30 dong log gan nhat (JSON) |
| `/reset-alerts` | GET | Reset bo dem fall_count |

---

## PHAN 3: DONG BO HOA TONG THE (System Synchronization)

| Primitive | Loai | Bao ve |
|---|---|---|
| `sensorQueue` (Main) | QueueHandle_t (len=5) | Truyen SensorData tu TaskSensorRead sang TaskAlertManager |
| `alertMutex` (Main) | SemaphoreHandle_t (mutex) | Bao ve g_alertLevel, g_lastFallTime khi doc/ghi tu ESP-NOW callback / TaskAlertManager / TaskBuzzerLED / TaskActuator |
| `imuQueue` (Wearable) | QueueHandle_t (len=30) | Truyen ImuData tu TaskIMURead sang TaskEdgeAI / TaskNetworkWeb (COLLECTING) |
| `stateMutex` (Wearable) | SemaphoreHandle_t (mutex) | Bao ve operationMode va state |

---

## PHAN 4: TICH HOP LIEN THIET BI (Inter-device Integration)

| Tham so | Gia tri |
|---|---|
| Giao thuc | ESP-NOW Unicast |
| Kenh Wi-Fi | Kenh 1 (ca hai board) |
| Wearable Wi-Fi mode | WIFI_AP_STA — SoftAP "Wearable_AP" khoa kenh 1 |
| Main Station Wi-Fi mode | WIFI_STA — esp_wifi_set_channel(1) |
| Do tre truyen nhan | < 10ms (ly thuyet) |
| Co che chong mat goi | Phat 3x lien tiep, gian cach 5ms |
| Latching ALERT_FALL | 12 giay tu goi tin cuoi cung |
| Danger Overwrite | DANGER/CRITICAL de len FALL ngay lap tuc |

> **TRANG THAI:** Code da tich hop hoan chinh. **DA TEST THUC TE THANH CONG TREN CA 2 THIET BI.**

### Ket qua test thuc te (Verification Results)
- **Ket qua ESP-NOW:** Da kiem thu thuc te thanh cong tren 2 bo mach (XIAO ESP32-S3 va ESP32-S3 N16R8).
- **Phan hoi tu thiet bi:** Khi gia lap te nga, Wearable phat 3x ESP-NOW, Trạm chinh nhan tuc thi (<10ms), coi keu va LED nhay 5Hz chinh xac.
- **Kiem thu latching va overwrite:** Latching 12 giay hoat dong tot, va khi thoi khi doc (ENS160) len muc DANGER, coi va LED da ngay lap tuc kich hoat Danger Overwrite bat servo/quat bat ky luc nao.
- **Ket qua kiem thu (2026-05-27):** test thành công các thiết bị Quạt / LED / Buzzer / Servo.

### Huong dan test (Verification Steps)
1. Nap `Rtos_main.ino` cho Tram chinh → mo Serial Monitor → copy dia chi MAC STA in ra.
2. Dan MAC vao `MAIN_BOARD_MAC[]` trong `wearable_unified_rtos.ino` (dong ~60).
3. Nap `wearable_unified_rtos.ino` cho XIAO ESP32-S3.
4. Kich hoat che do INFERENCE → gia lap nga → kiem tra log va coi/LED cua Tram chinh.

### TODO tiep theo
- [ ] Xay dung Web UI cho Tram chinh (ESP32-S3 N16R8) de giam sat truc quan cac thong so moi truong.
- [ ] Hop nhat 2 Web UI cua Tram chinh va Thiet bi deo (Wearable) lai thanh mot giao dien Web UI tap trung duy nhat.
