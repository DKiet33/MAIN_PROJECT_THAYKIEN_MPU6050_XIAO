# Nguyen Ly Hoat Dong Cua Code

Tai lieu nay mo ta nguyen ly hoat dong cua firmware ESP32-S3 bang flowchart `mermaid`.

## Mo ta ngan
- He thong doc du lieu tu ENS160, AHT21, LM75 va MQ2.
- Sau moi chu ky, du lieu duoc danh gia thanh `OK`, `WARNING`, `DANGER`, `ERROR`, hoac `WARMUP`.
- State machine uu tien an toan: neu co `DANGER` thi bao dong ngay, neu khong moi xu ly `sensor error`.
- Buzzer/LED phan ung theo `AlertLevel`.
- Telegram chi la kenh thong bao/phat lenh, khong chi phoi logic an toan.

## Flowchart tong the
```mermaid
flowchart TD
    A[Khoi dong ESP32] --> B[setup]
    B --> C[Khoi tao Serial GPIO I2C]
    C --> D[Khoi tao ENS160 AHT21 LM75 MQ2]
    D --> E[Khoi tao WiFi Telegram neu ENABLE_TELEGRAM]
    E --> F[Bat dau loop]

    F --> G[updateBuzzer]
    G --> H[checkWiFi]
    H --> I{Da den chu ky doc sensor?}

    I -- Chua --> T{ENABLE_TELEGRAM?}
    I -- Roi --> J[sensorsReadAll]
    J --> K[Danh gia TVOC eCO2 AQI Temp MQ2]
    K --> L[processSystemLogic]

    L --> M{Co DANGER?}
    M -- Co --> N[STATE_SENSOR_ALERT + sendAlert DANGER]
    M -- Khong --> O{Co sensor error?}
    O -- Co --> P[STATE_SENSOR_ERROR + sendAlert WARNING]
    O -- Khong --> Q{Co WARNING?}
    Q -- Co --> R[STATE_SENSOR_ALERT + sendAlert WARNING]
    Q -- Khong --> S[STATE_NORMAL + clearAlert]

    N --> T
    P --> T
    R --> T
    S --> T

    T -- Khong --> F
    T -- Co --> U[checkTelegramCommand]
    U --> V{Co lenh?}
    V -- Khong --> F
    V -- Co --> W[handleTelegramCommand]
    W --> F
```

## Flowchart chi tiet sendAlert
```mermaid
flowchart TD
    A[sendAlert level message] --> B{level == ALERT_NONE?}
    B -- Yes --> Z[Return]
    B -- No --> C{Dang bi suppress va level < CRITICAL?}
    C -- Yes --> Z
    C -- No --> D[Cap nhat currentAlertLevel ngay]
    D --> E[Bat LED ngay neu >= WARNING]
    E --> F[Bat buzzer lien tuc neu == CRITICAL]
    F --> G{Escalated hoac het cooldown hoac CRITICAL?}
    G -- No --> Z
    G -- Yes --> H[sendTelegram]
    H --> I[Cap nhat lastAlertTime]
    I --> Z
```

## Flowchart `/stop`
```mermaid
flowchart TD
    A[User gui /stop] --> B[handleTelegramCommand stop]
    B --> C[clearAlert]
    C --> D[alertSuppressed = true]
    D --> E[suppressStartedAt = millis]
    E --> F[Gui tin nhan tat canh bao 60 giay]
    F --> G[Loop tiep tuc]
    G --> H{sendAlert duoc goi?}
    H -- WARNING or DANGER --> I[Bi chan trong 60 giay]
    H -- CRITICAL --> J[Van cho phep canh bao]
```

## PHÂN HỆ THIẾT BỊ ĐEO THẮT LƯNG (WEARABLE FALL DETECTION)

Phân hệ này chạy trên Seeed Studio XIAO ESP32-S3 với cảm biến MPU6050, hỗ trợ chế độ kép: Thu thập dữ liệu (Ingestion) và Suy luận thời gian thực (Inference).

### 1. Flowchart tổng thế Thiết bị đeo (Wearable Unified Loop)
```mermaid
flowchart TD
    Start([Khởi động XIAO]) --> Setup[Setup: I2C, MPU6050, WiFi AP/Client, Web Server, LED]
    Setup --> CheckMode{operationMode?}
    
    %% INFERENCE MODE
    CheckMode -- INFERENCE --> LoopInfer[Đọc MPU6050 @ 100Hz]
    LoopInfer --> AxisRotate[Xoay trục: Z->Fwd, X->Left, Y->Up\nTrừ sai số Hiệu chuẩn Offset]
    AxisRotate --> PushBuffer[Đẩy vào bộ đệm trượt inferBuf]
    PushBuffer --> CheckBufFull{inferBuf đầy?}
    CheckBufFull -- Chưa --> ServeWeb[Xử lý Web Server & server.handleClient]
    CheckBufFull -- Rồi --> RunAI[Chạy Edge Impulse run_classifier]
    RunAI --> FilterBlock[Bộ lọc chống báo giả: Debounce & Cooldown]
    FilterBlock --> ServeWeb
    
    %% INGESTION MODE
    CheckMode -- INGESTION --> LoopIngest{Trạng thái Ingestion?}
    LoopIngest -- READY --> IngestReady[Nhấp nháy LED chậm\nChờ lệnh Bắt đầu thu mẫu từ Web UI]
    LoopIngest -- SAMPLING --> IngestSample[Đọc MPU6050 @ 100Hz đủ 300 mẫu\nLưu vào bộ đệm mảng động]
    IngestSample --> IngestUpload[Thực hiện buildJson + Data Augmentation\nUpload HTTP Secure sang Edge Impulse Studio]
    IngestUpload --> IngestReady
    
    ServeWeb --> CheckMode
```

### 2. Chi tiết Bộ lọc chống báo giả (Debounce & Cooldown)
```mermaid
flowchart TD
    AI[Kết quả run_classifier] --> CheckThres{fall >= FALL_ALERT_THRESHOLD 0.85?}
    
    CheckThres -- Có --> IncConfirm[Tăng bộ đếm fallConfirm++]
    IncConfirm --> CheckSlices{fallConfirm >= FALL_CONFIRM_SLICES 2?}
    
    CheckSlices -- Đúng --> CheckCooldown{millis - lastAlertMs >= FALL_COOLDOWN_MS 6000?}
    CheckSlices -- Sai --> FlashLED[Nháy LED cực nhanh cảnh báo nhẹ]
    
    CheckCooldown -- Đúng --> TriggerAlert[Xác nhận té ngã!\nfallConfirm = 0\nlastAlertMs = millis()\nfall_count++\nBật LED sáng luôn\nlogPrintf CẢNH BÁO TÉ NGÃ]
    CheckCooldown -- Sai --> IgnoreAlert[Bỏ qua do đang trong thời gian khóa]
    
    CheckThres -- Không --> CheckWalk{walk > idle?}
    CheckWalk -- Đúng --> WalkAct[fallConfirm = 0\nNháy LED vừa phải]
    CheckWalk -- Sai --> IdleAct[fallConfirm = 0\nTắt LED]
    
    TriggerAlert --> End[Kết thúc chu kỳ lọc]
    FlashLED --> End
    IgnoreAlert --> End
    WalkAct --> End
    IdleAct --> End
```
