# 🎬 YouTube — Companion Repository

Code and resources for the YouTube video: [**Building an iOS Speech-to-USB Keyboard with ESP32-S3**](https://youtu.be/nmGEedloQ6E)

---

## 📂 Projects

### ⌨️ [ios-keyboard/](ios-keyboard/)
The main project — an ESP32-S3 firmware that turns iPhone speech-to-text into USB HID keystrokes. BLE receives dictated text, TinyUSB types it on any computer. Supports 7 keyboard layouts (US, DE, CH-DE, FR, UK, ES, IT), OTA updates, WiFi captive portal provisioning, and more.

👉 **[Full README](ios-keyboard/README.md)** · **[Functional Spec](ios-keyboard/docs/iOS-Keyboard-FSD.md)** · **[Complete project repo](https://github.com/SensorsIot/IOS-Keyboard)**

### 🔧 [Universal-ESP32-Workbench/](Universal-ESP32-Workbench/)
The automated test and flash infrastructure used throughout the video. Provides RFC2217 remote flashing, BLE/WiFi proxies, GPIO control, and serial monitoring — all from a Raspberry Pi.

👉 **[Dedicated repo](https://github.com/SensorsIot/Universal-ESP32-Workbench)**

### 👋 [hello-world/](hello-world/)
Minimal ESP-IDF hello-world used to verify the toolchain and workbench setup.

---

## 🛠️ Prerequisites

- 🧰 [ESP-IDF v5.4+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)
- 📟 ESP32-S3 development board
- 🍓 *(optional)* [Universal ESP32 Workbench](Universal-ESP32-Workbench/) for remote flash/test

## 🚀 Quick Start

```bash
cd ios-keyboard
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

## 📄 License

MIT
