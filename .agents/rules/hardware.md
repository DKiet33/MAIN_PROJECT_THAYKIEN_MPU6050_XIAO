# ESP32-S3 Hardware Notes

## Purpose
Store the current pin map, attached modules, and board constraints for the ESP32-S3 N16R8 build.

## Board
- MCU: ESP32-S3
- Variant: ESP32-S3 N16R8, 44-pin
- Flash / PSRAM: 16MB OPI Flash + 8MB OPI PSRAM
- USB mode: USB CDC on boot enabled
- Power input: TODO confirm 5V USB or external regulated input

## Active Pin Map
| Function | GPIO | Notes |
| --- | --- | --- |
| MQ2 analog out | GPIO1 | ADC1_CH0, chosen to avoid ADC2/WiFi conflicts |
| Buzzer | GPIO2 | Digital output |
| Status LED | GPIO4 | Digital output |
| I2C SDA | GPIO8 | Shared bus for ENS160, AHT21, LM75 |
| I2C SCL | GPIO9 | Shared bus for ENS160, AHT21, LM75 |

## I2C Devices
| Device | Address | Bus |
| --- | --- | --- |
| ENS160 | `0x53` | I2C on GPIO8 / GPIO9 |
| AHT21 | `0x38` | I2C on GPIO8 / GPIO9 |
| LM75 | `0x48` | I2C on GPIO8 / GPIO9 |

## Reserved / Removed
- Camera AI and ESP32-CAM UART link have been removed from the current design.
- UART1 camera pins `GPIO17` and `GPIO18` are currently free again unless reassigned later.

## Constraints
- GPIO33 to GPIO37 are occupied by OPI PSRAM on N16R8 and must not be used.
- Prefer ADC1 pins for analog sensors when WiFi is active.
- Confirm boot-strapping and USB-safe pins before assigning extra relays, buttons, or displays.
- Revisit this file whenever new modules are added.

## Component Links

### 1. Khối nhận dữ liệu và cảm biến (Sử dụng ESP32-S3)
- [Module WiFi Bluetooth WROOM-1 N16R8 ESP32-S3 DevKitC-1](https://icdayroi.com/module-wifi-bluetooth-wroom-1-n16r8-esp32-s3-devkitc-1)
- [Cảm biến chất lượng nhiệt độ và độ ẩm không khí ENS160+AHT21 eCO2 TVOC](https://icdayroi.com/cam-bien-chat-luong-nhiet-do-va-do-am-khong-khi-ens160-aht21-eco2-tvoc)
- [Cảm biến nhiệt độ CJMCU-75 LM75 hỗ trợ I2C](https://icdayroi.com/cam-bien-nhiet-do-cjmcu-75-lm75-ho-tro-i2c)
- [Cảm biến khí gas MQ-2](https://icdayroi.com/cam-bien-khi-gas-mq-2)

### 2. Thiết bị đeo tay (Sử dụng XIAO ESP32S3 + MPU6050)
- [XIAO ESP32S3 Getting Started](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- Cảm biến gia tốc MPU6050
