# Lessons Learned

Use this file to record mistakes, user corrections, and prevention rules.

## Template
### Date
- Context:
- Mistake / issue:
- Root cause:
- Prevention rule:

## Entries
### 2026-04-22
- Context: ESP32-S3 sensor alert flow in `src/main.ino`, specifically `sendAlert()` cooldown handling.
- Mistake / issue: Cooldown logic blocked alert escalation, so a transition like `WARNING -> DANGER` could leave `currentAlertLevel` stuck at `WARNING` and keep the buzzer pattern too slow.
- Root cause: Notification throttling and hardware/system state updates were coupled in the same early-return path.
- Prevention rule: When implementing cooldowns or rate limits, update the internal severity/state first and apply throttling only to external side effects such as Telegram messages or logs.

### 2026-04-22
- Context: ESP32-S3 sensor state machine and Telegram `/stop` behavior in `src/main.ino`.
- Mistake / issue: Generic sensor-error handling could mask a real `DANGER` reading, and `/stop` only cleared the current alert without suppressing immediate re-trigger.
- Root cause: Safety priority order was not explicit in `processSystemLogic()`, and user intent for temporary silence was not modeled as its own state.
- Prevention rule: In alert systems, evaluate hazard severity before degraded-sensor states, and model operator actions like mute/stop as explicit timed state rather than one-shot variable resets.
