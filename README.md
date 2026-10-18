# Smart Home Model: ESP32 Door Access Module

Course project at Thu Duc College of Technology (Industrial Electronics), team of 2.
The full model uses two ESP32 boards. **This repository contains the firmware of ESP32 #1** (door access, temperature/humidity display, RGB LED control), which is my part of the project.

## Features

- Unlock the door with an **RFID card** (MFRC522) or a **4x4 keypad password**
- **Change the password** from the keypad; the new password is stored in flash (`Preferences`)
- **20x4 I2C LCD** shows status messages; typed password characters are masked with `*`
- **DHT11** temperature and humidity read every 2 s and shown on the LCD
- **RGB LED control from a smartphone** over Bluetooth Classic (SPP)
- Servo door lock: opens to 90 degrees, closes back to 180 degrees after 5 s

## How to use

| Action | Steps |
|---|---|
| Open with card | Scan the registered card |
| Open with password | Type the password, press `#` |
| Clear input / cancel | Press `*` |
| Change password | Press `A`, enter the current password, `#`, enter the new password, `#` |
| Control the RGB LED | Pair with the Bluetooth device `ESP32_CuaThongMinh`, open a serial Bluetooth terminal app and send a command |

Bluetooth commands: `r` red, `g` green, `b` blue, `p` purple, `w` white, `off` turn off. Each command ends with a newline.

## Hardware

- ESP32 DevKit (board: ESP32 Dev Module in Arduino IDE)
- RFID module MFRC522, keypad 4x4, LCD 20x4 with I2C backpack (address 0x27)
- Servo SG90, DHT11 module, RGB LED module

| Device | ESP32 GPIO |
|---|---|
| MFRC522 SS (chip select) | 5 (SPI: SCK 18, MISO 19, MOSI 23) |
| LCD I2C | SDA 21, SCL 22 (default I2C pins) |
| Servo signal | 13 |
| DHT11 data | 27 |
| RGB LED (R / G / B) | 26 / 25 / 12 |
| Keypad rows | 32, 33, 14, 16 |
| Keypad columns | 17, 15, 2, 0 |

Wiring diagram and photos: see the `docs/` folder.

## Software

- Arduino IDE with the ESP32 board package
- Libraries (install from Library Manager): `MFRC522v2`, `ESP32Servo`, `Keypad`, `LiquidCrystal_I2C`, `DHT sensor library`
- Built in with the ESP32 core: `BluetoothSerial`, `Preferences`, `Wire`, `SPI`

### Build and flash

1. Install the libraries above.
2. Open `firmware/esp32_door_access/esp32_door_access.ino`.
3. Select board **ESP32 Dev Module** and the correct COM port.
4. Click Upload.

Before using your own card, set the card UID in `VALID_UID` (the UID is printed in hexadecimal; read it with the RFID example sketch of the library).

## Test results

- RFID card reading: 10 successful reads out of 10 attempts
- Door unlock time matches the 5 s value set in firmware

## Known limitations and next steps

- `delay()` is used while the door is unlocked or access is denied, so the keypad and Bluetooth do not respond during that time. Planned improvement: rewrite with a non-blocking state machine (`millis()`) or FreeRTOS tasks.
- The card UID and the default password are hard-coded; they should be moved to a configuration or enrolled at runtime.
- GPIO 0, 2, 12 and 15 are ESP32 strapping pins; they are used here for the keypad and LED, so the wiring must not hold them at a wrong level during boot.

## Team

- Door access, DHT11 and Bluetooth RGB LED (this repo): Nguyen Thi Kim Tien
- Rain-sensing drying rack and flame alarm (ESP32 #2): teammate
