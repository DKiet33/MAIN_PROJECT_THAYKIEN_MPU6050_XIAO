# Lessons Learned

Use this file to record mistakes, user corrections, and prevention rules.

## Entries

### 2026-04-22 (Sensor alert flow and cooldown)
- **Context:** ESP32-S3 sensor alerts and Telegram rate-limiting.
- **Issue:** Cooldown logic blocked severity transition (WARNING -> DANGER), causing buzzer pattern to hang.
- **Root cause:** Telegram rate-limiting and hardware state updates shared the same early-return path.
- **Rule:** Update internal states first; apply rate-limit/throttling only to external actions (Telegram/logs).

### 2026-04-22 (Sensor error prioritization)
- **Context:** Priority in sensor state machine and Telegram `/stop` command.
- **Issue:** Degraded sensor error hid real DANGER state, and `/stop` re-triggered immediately.
- **Root cause:** Incorrect check order in logic, and operator suppression was a flag rather than a timed state.
- **Rule:** Prioritize DANGER readings over sensor-error states. Treat operator suppression (mute) as a timed state.

### 2026-05-15 (Premature integration)
- **Context:** Fall detection UART/Serial2 integration before wearable was finished.
- **Issue:** Integration code was written, then removed due to protocol changes.
- **Root cause:** Integrating with unfinished subsystems without finalized protocol.
- **Rule:** Implement integration layers only after communication protocol and API are finalized. Use TODO placeholders.

### 2026-05-18 (Web UI log parsing)
- **Context:** Web UI polling serial logs for fall events.
- **Issue:** Vietnamese localization of logs ("CẢNH BÁO TÉ NGÃ") broke regex parse on Web UI.
- **Root cause:** Parsing raw logs for system states on Web UI is fragile.
- **Rule:** Avoid parsing serial strings. Use structured data APIs (e.g., JSON status API) with count variables.

### 2026-05-18 (AI continuous inference false alarms)
- **Context:** Fall detection classification on microcontroller.
- **Issue:** Light shakes triggered false fall alerts.
- **Root cause:** Sliding window issues and lack of signal filtering.
- **Rule:** Implement Debounce (alert only if probability > threshold for N consecutive windows) and Cooldown (lock alerts for X seconds).

### 2026-05-20 (LaTeX formatting in chat)
- **Context:** Answering questions in chat.
- **Issue:** Using LaTeX formulas (e.g. `$100\text{Hz}$`) broke rendering.
- **Root cause:** UI does not support LaTeX formatting.
- **Rule:** Never use LaTeX. Use plain text or standard symbols (e.g., 100Hz, ~40ms, degC).

### 2026-05-20 (RTOS I2C conflicts & WHO_AM_I checks)
- **Context:** Porting MPU6050 logic to FreeRTOS.
- **Issue:** I2C bus crashed on multi-task access, and WHO_AM_I check failed on variant chip MPU6500.
- **Root cause:** Core-specific interrupts in Arduino Wire, and over-engineering hardware validation.
- **Rule:** Isolate physical I2C calls to a single core/task. Do not add strict register checks if the original code did not require it.

### 2026-05-27 (ESP-NOW callbacks & boot logs)
- **Context:** Real-world testing of ESP-NOW on ESP32-S3.
- **Issue:** CDC boot logs missed, delay() in callback caused WDT panic, compile errors on Core v3.x.
- **Root cause:** Fast boot on native USB, calling blocking code in system Wi-Fi task, and Core API changes.
- **Rule:** Add boot delay for USB CDC serial detection. Keep callbacks non-blocking (only set flags). Use compile guards for v3.x compatibility.

### 2026-05-27 (FreeRTOS Servo hangs)
- **Context:** Controlling Servo with ESP32Servo in FreeRTOS.
- **Issue:** System hung during `servo.write()` or inrush current tripped power.
- **Root cause:** Cross-core resource conflict, current spike, and library timer auto-detection bug.
- **Rule:** Initialize hardware inside the task using it. Run motor tasks on the same core as sensors. Add stabilization delays and allocate timers manually.

### 2026-05-27 (LEDC Resolution limit)
- **Context:** PWM configuration on ESP32-S3.
- **Issue:** `ledcAttach` failed when using 16-bit resolution.
- **Root cause:** ESP32-S3 hardware LEDC timer supports maximum 14-bit resolution.
- **Rule:** Limit LEDC resolution to 14-bit or less on ESP32-S3/C3. Calculate duty cycle dynamically.

### 2026-06-22 (Word in-place document editing)
- **Context:** Modifying reports with user manual edits.
- **Issue:** Regenerating docx from templates lost manual formatting.
- **Root cause:** Overwriting files instead of targeted XML modification.
- **Rule:** Edit docx files in-place by locating specific sections and modifying them without overwriting the entire document structure.
