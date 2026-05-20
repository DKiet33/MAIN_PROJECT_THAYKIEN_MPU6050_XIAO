# Nguyen Ly Hoat Dong — He Thong Giam Sat Moi Truong & Phat Hien Te Nga

Cap nhat: 2026-05-20 | Trang thai: ESP-NOW code xong, chua test thuc te

---

## TONG QUAN HE THONG

He thong gom 2 board giao tiep voi nhau qua ESP-NOW Unicast kenh Wi-Fi 1:

```mermaid
flowchart LR
    subgraph W["XIAO ESP32-S3 (Wearable)"]
        W1[TaskIMURead\nCore1 Pri5\n100Hz] -->|imuQueue| W2[TaskEdgeAI\nCore1 Pri4\n370ms]
        W2 -->|"WIFI_AP_STA\nKenh 1\nESP-NOW TX"| ESPNOW
        W3[TaskNetworkWeb\nCore0 Pri3] 
        W1 -->|imuQueue| W3
    end

    ESPNOW{{"ESP-NOW\n< 10ms\n[CHUA TEST]"}}

    subgraph M["ESP32-S3 N16R8 (Main Station)"]
        ESPNOW -->|"WIFI_STA\nKenh 1\nOnDataRecv"| M1[alertMutex\ng_alertLevel=ALERT_FALL]
        M2[TaskSensorRead\nCore1 Pri3\n1s] -->|sensorQueue| M3[TaskAlertManager\nCore1 Pri4]
        M3 --> M1
        M1 -->|alertMutex| M4[TaskBuzzerLED\nCore1 Pri5\n50ms]
        M1 -->|alertMutex| M5[TaskActuator\nCore0 Pri3\n200ms]
    end
```

---

## PHAN 1: FLOWCHART KHOI DONG — TRAM CHINH (Rtos_main.ino)

```mermaid
flowchart TD
    A[Khoi dong ESP32-S3 N16R8] --> B[setup]
    B --> C[Init GPIO: Buzzer LOW, LED LOW, Fan LOW]
    C --> D[Init I2C SDA=8 SCL=9 400kHz\nTimeout 50ms]
    D --> E[I2C Scan & Init: ENS160, AHT21, LM75]
    E --> F{ENABLE_SERVO?}
    F -- Co --> G[Servo attach GPIO5\n50Hz, 500-2500us\nGoc ban dau 0 do]
    F -- Khong --> H[Skip servo]
    G --> I
    H --> I[WiFi.mode WIFI_STA\nWiFi.disconnect\nesp_wifi_set_channel 1]
    I --> J[In MAC address STA ra Serial\nde nguoi dung copy vao MAIN_BOARD_MAC]
    J --> K[esp_now_init\nesp_now_register_recv_cb OnDataRecv]
    K --> L[Tao sensorQueue len=5\nTao alertMutex]
    L --> M[Spawn 4 FreeRTOS Tasks]
    M --> N[loop: vTaskDelay portMAX_DELAY]

    subgraph Core1["Core 1"]
        T1["TaskSensorRead Pri3\nDoc sensor moi 1s\ngui vao sensorQueue"]
        T2["TaskAlertManager Pri4\nxQueueReceive block 2s\nTinh AlertLevel moi\nLatching 12s + Danger Overwrite\nGhi g_alertLevel qua alertMutex"]
        T3["TaskBuzzerLED Pri5\nDoc g_alertLevel moi 50ms\nDieu khien Buzzer + LED"]
    end

    subgraph Core0["Core 0"]
        T4["TaskActuator Pri3\nDoc g_alertLevel moi 200ms\nDieu khien Servo + Fan"]
    end

    subgraph ISR["ESP-NOW RX Callback"]
        T0["OnDataRecv\nLen == sizeof FallAlertPacket?\nalertType == 0xFA?\n→ g_alertLevel = ALERT_FALL\n→ g_lastFallTime = millis"]
    end

    T1 -- sensorQueue --> T2
    T0 -- alertMutex --> T2
    T2 -- alertMutex / g_alertLevel --> T3
    T2 -- alertMutex / g_alertLevel --> T4
```

---

## PHAN 2: FLOWCHART TINH ALERT LEVEL — TaskAlertManager

```mermaid
flowchart TD
    A[xQueueReceive sensorQueue\nblock 2s] --> B{Nhan duoc\nSensorData?}
    B -- Khong --> A
    B -- Co --> C[Tinh newLevel tu sensor]

    C --> D{airQualityStatus==DANGER\nhoac temperatureStatus==DANGER?}
    D -- Co --> E[newLevel = ALERT_DANGER]
    D -- Khong --> F{anySensorError?}
    F -- Co --> G[newLevel = ALERT_WARNING]
    F -- Khong --> H{airQuality==WARNING\nhoac temp==WARNING?}
    H -- Co --> I[newLevel = ALERT_WARNING]
    H -- Khong --> J[newLevel = ALERT_NONE]

    E --> K[Ghi g_alertLevel voi Latching + Overwrite]
    G --> K
    I --> K
    J --> K

    K --> L{newLevel == DANGER\nhoac CRITICAL?}
    L -- Co --> M[g_alertLevel = newLevel\nDe len ALERT_FALL neu can]
    L -- Khong --> N{g_alertLevel == ALERT_FALL\nva millis-g_lastFallTime < 12000?}
    N -- Co, dang trong 12s --> O[Giu nguyen ALERT_FALL\nbang qua cap nhat sensor]
    N -- Khong, het 12s hoac khong FALL --> P[g_alertLevel = newLevel]

    M --> A
    O --> A
    P --> A
```

---

## PHAN 3: FLOWCHART BUZZER/LED — TaskBuzzerLED

```mermaid
flowchart TD
    Start[Bat dau vong lap 50ms] --> Read[Doc g_alertLevel qua alertMutex]
    Read --> Switch{g_alertLevel?}

    Switch -- NONE/INFO --> NA[Buzzer OFF\nLED OFF]
    Switch -- FALL --> FA[Buzzer HIGH lien tuc\nLED nhap nhay 100ms ON/OFF]
    Switch -- WARNING --> WA[LED ON\nBuzzer nhap nhay cham\n200ms ON / 800ms OFF]
    Switch -- DANGER --> DA[LED ON\nBuzzer nhap nhay nhanh\n100ms ON / 200ms OFF]
    Switch -- CRITICAL --> CA[LED ON\nBuzzer HIGH lien tuc]

    NA --> End[vTaskDelayUntil 50ms]
    FA --> End
    WA --> End
    DA --> End
    CA --> End
    End --> Start
```

---

## PHAN 4: FLOWCHART ACTUATOR — TaskActuator

```mermaid
flowchart TD
    Start[Bat dau vong lap 200ms] --> Read[Doc g_alertLevel qua alertMutex]
    Read --> Switch{g_alertLevel?}

    Switch -- "NONE/INFO\nhoac FALL" --> NA["targetAngle = 0\nfanOn = false\n(Khong bat quat khi chi co canh bao nga)"]
    Switch -- WARNING --> WA[targetAngle = 90\nfanOn = true]
    Switch -- "DANGER/CRITICAL" --> DA[targetAngle = 180\nfanOn = true]

    NA --> Write[Chi ghi Servo neu goc thay doi\nChi ghi Fan neu trang thai thay doi]
    WA --> Write
    DA --> Write
    Write --> End[vTaskDelayUntil 200ms]
    End --> Start
```

---

## PHAN 5: FLOWCHART KHOI DONG — THIET BI DEO (wearable_unified_rtos.ino)

```mermaid
flowchart TD
    A[Khoi dong XIAO ESP32-S3] --> B[setup]
    B --> C[Serial begin, LED pin, BTN pin]
    C --> D[Wire.begin I2C\ninitMPU6050]
    D --> E{MPU ket noi?}
    E -- Khong --> F[In loi, nhap nhay LED 20x]
    E -- Co --> G[Doc thu ImuData, kiem tra truc Z]
    F --> H
    G --> H[WiFi.mode WIFI_AP_STA\nWiFi.softAP Wearable_AP Kenh 1]
    H --> I[WiFi.begin EI_WIFI_SSID\nCho toi da 15s]
    I --> J[initEspNow\nesp_now_add_peer MAIN_BOARD_MAC Kenh 1]
    J --> K[Dang ky tat ca Web routes\nserver.begin]
    K --> L[Tao imuQueue len=30\nTao stateMutex]
    L --> M[Spawn 3 FreeRTOS Tasks]
    M --> N[loop: vTaskDelay portMAX_DELAY]

    subgraph Core1W["Core 1 — Wearable"]
        T1["TaskIMURead Pri5\nDoc MPU6050 moi 10ms\nDay vao imuQueue"]
        T2["TaskEdgeAI Pri4\nNhan tu imuQueue\nInference 370ms\nESP-NOW TX khi nga"]
    end

    subgraph Core0W["Core 0 — Wearable"]
        T3["TaskNetworkWeb Pri3\nserver.handleClient\nINGESTION state machine\nHTTPS upload EI Studio"]
    end

    T1 -- imuQueue --> T2
    T1 -- imuQueue --> T3
```

---

## PHAN 6: FLOWCHART SUY LUAN AI & PHAT ESP-NOW — TaskEdgeAI

```mermaid
flowchart TD
    Start[Bat dau vong lap] --> Lock[Lay stateMutex 5ms]
    Lock --> CheckMode{curMode == INFERENCE?}
    CheckMode -- Khong INGESTION --> Sleep[vTaskDelay 100ms\nnhuong queue cho Ingestion]
    CheckMode -- Co --> Queue[xQueueReceive imuQueue\ntimeout 20ms]
    Queue -- Het gio --> Start
    Queue -- Nhan duoc ImuData --> Write[Ghi vao inferBuf tai vi tri hien tai\ninferBufIdx++]

    Write --> Full{inferBufIdx >= SLICE_SIZE?}
    Full -- Chua --> Start
    Full -- Roi --> Shift[memmove dich du lieu cu\ninferBufIdx = 0]
    Shift --> Warm{inferBufFull?}
    Warm -- Chua du 4 windows --> Warm2[Tang warmup counter\nneu >= 4 set inferBufFull=true]
    Warm2 --> LED[Dieu khien LED theo nhan]
    Warm -- Da du --> Classify[numpy signal_from_buffer\nrun_classifier &res false]

    Classify --> Extract[Trich xuat fall idle walk\nTim nhan cao nhat top_label]
    Extract --> Thresh{fall >= 0.85?}

    Thresh -- Co --> Incr[fallConfirm++]
    Incr --> Slices{fallConfirm >= 3?}
    Slices -- Chua --> FastLED[Nhap nhay LED nhanh 80ms]
    Slices -- Roi --> Cooldown{millis - lastAlertMs >= 6000?}
    Cooldown -- Khong --> IgnoreLED[Nhap nhay LED nhanh\nbang qua do trong Cooldown]
    Cooldown -- Co --> Alert[fallConfirm=0\nlastAlertMs=millis\nfall_count++\nIn log CANH BAO TE NGA]

    Alert --> SendNOW[Tao FallAlertPacket\nalertType=0xFA\nfallCount confidence timestamp]
    SendNOW --> Loop3[for i in 0 1 2:\n  esp_now_send MAIN_BOARD_MAC\n  vTaskDelay 5ms]
    Loop3 --> LogNOW[logPrintf ESP-NOW Da phat 3x]
    LogNOW --> LedOn[ledOn sat sang]
    LedOn --> PrintLine[Serial.printf INF fall idle walk XYZ]
    PrintLine --> Start

    Thresh -- Khong --> CheckWalk{walk > idle?}
    CheckWalk -- Co --> WalkLED[fallConfirm=0\nNhap nhay LED 250ms]
    CheckWalk -- Khong --> IdleLED[fallConfirm=0\nTat LED]
    WalkLED --> PrintLine
    IdleLED --> PrintLine
    FastLED --> PrintLine
    IgnoreLED --> PrintLine
    LED --> Start
```

---

## PHAN 7: FLOWCHART BO LOC CHONG BAO GIA (Debounce & Cooldown)

```mermaid
flowchart TD
    AI[Ket qua run_classifier] --> CheckThres{fall >= 0.85?}

    CheckThres -- Co --> IncConfirm[Tang bo dem fallConfirm++]
    IncConfirm --> CheckSlices{fallConfirm >= 3?}

    CheckSlices -- Dung --> CheckCooldown{millis - lastAlertMs >= 6000?}
    CheckSlices -- Sai --> FlashLED[Nhap nhay LED nhanh canh bao nhe]

    CheckCooldown -- Dung --> TriggerAlert[Xac nhan te nga!\nfallConfirm = 0\nlastAlertMs = millis\nfall_count++\nBat LED sang lien tuc\nPhat ESP-NOW 3x]
    CheckCooldown -- Sai --> IgnoreAlert[Bo qua do dang trong thoi gian khoa 6s]

    CheckThres -- Khong --> CheckWalk{walk > idle?}
    CheckWalk -- Dung --> WalkAct[fallConfirm = 0\nNhap nhay LED vua phai 250ms]
    CheckWalk -- Sai --> IdleAct[fallConfirm = 0\nTat LED]

    TriggerAlert --> End[Ket thuc chu ky loc]
    FlashLED --> End
    IgnoreAlert --> End
    WalkAct --> End
    IdleAct --> End
```

---

## PHAN 8: LICH SU CAP NHAT

| Ngay | Noi dung cap nhat |
|---|---|
| 2026-04-22 | Khoi tao he thong, firmware baseline main.ino |
| 2026-05-15 | Tao Rtos_main.ino — FreeRTOS 4 tasks, bo Telegram/MQ2, them Servo+Fan |
| 2026-05-18 | Hoan thien fall detection XIAO (Edge Impulse + Web UI tieng Viet + Debounce/Cooldown) |
| 2026-05-20 | Tich hop FreeRTOS vao Wearable (3 tasks), sua loi I2C co lap Core1 |
| 2026-05-20 | **Tich hop ESP-NOW TX/RX ca 2 board, ALERT_FALL=5, Latching 12s, Danger Overwrite** |
| — | **TODO: Test thuc te ESP-NOW tren 2 thiet bi** |
