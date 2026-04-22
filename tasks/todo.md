# Task Plan

## Active Plan
- [x] Create project skeleton for agent workflow and source files.
- [x] Seed orchestration rules in `.agents/rules/instructions.md`.
- [x] Add memory files in `tasks/` for planning and lessons learned.
- [x] Create placeholders for ESP32-S3 firmware and web UI.
- [x] Replace hardware placeholders in `.agents/rules/hardware.md`.
- [x] Implement sensor-only firmware baseline in `src/main.ino`.
- [x] Remove camera AI / ESP32-CAM pin usage from the current baseline.
- [x] Fix alert escalation logic so `WARNING -> DANGER/CRITICAL` updates buzzer/LED immediately even during Telegram cooldown.
- [x] Prioritize real `DANGER` readings over sensor-error warnings in the main state machine.
- [x] Add temporary alert suppression for Telegram `/stop` so alerts do not immediately re-trigger.
- [ ] Build the real web control UI in `src/index.html`.
- [ ] Add the next batch of real peripherals and update the pin map.

## Notes
- Keep `src/` as the shared working area for firmware and web assets.
- Update this file before and during any non-trivial task.

## Review
- Initial workspace scaffold created on 2026-04-22.
- Sensor-only ESP32-S3 baseline created on 2026-04-22 from the provided all-in-one sketch.
- Alert logic in `src/main.ino` was corrected on 2026-04-22 so cooldown only rate-limits Telegram notifications, not severity escalation.
- Safety review fixes were applied on 2026-04-22 so `DANGER` takes priority over generic sensor faults and `/stop` suppresses non-critical alerts for 60 seconds.
- Next meaningful step is to map the upcoming real components and assign remaining free GPIOs.
