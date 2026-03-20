# ⌨️ iOS Keyboard — Speech-to-USB HID Bridge

Turn your iPhone's speech-to-text into **real USB keystrokes** on any computer via an ESP32-S3. 🎤➡️⌨️

## ✨ What It Does

Your iPhone dictates text via a companion iOS app. The text is sent over **Bluetooth LE** to the ESP32-S3, which converts it into **USB HID keyboard reports** — typing the words on whatever computer is plugged in. Works with Windows, macOS, and Linux. No drivers needed.

## 🏗️ Architecture

```
┌──────────┐    BLE NUS     ┌───────────┐    USB HID    ┌──────────┐
│  iPhone   │ ─────────────▶ │  ESP32-S3  │ ─────────────▶ │ Computer │
│  (iOS App)│   T:Hello      │ iOS-KB-2C4C│  keystrokes    │ (any OS) │
└──────────┘                └───────────┘                └──────────┘
                                  │
                            WiFi + HTTP
                            (config/OTA)
```

## 🚀 Features

### 📡 Phase 1 — Infrastructure
- **WiFi Provisioning** — Captive portal with layout selector (`iOS-KB-XXXX` AP)
- **BLE NUS** — Nordic UART Service for text commands
- **HTTP API** — `/status`, `/ota`, `/layout`, `/reset` endpoints
- **OTA Updates** — Over-the-air firmware updates via HTTP or BLE
- **UDP Logging** — All ESP_LOG output mirrored to UDP for remote debugging
- **NVS Storage** — Persistent config (WiFi, layout, device name, UDP target)
- **Watchdog** — Task WDT (60s) + heap monitor (warn 50KB, reboot 30KB)

### ⌨️ Phase 2 — USB HID Keyboard
- **TinyUSB HID** — Standard 6KRO keyboard, plug-and-play on any OS
- **7 Keyboard Layouts** — US, DE, CH-DE, FR, UK, ES, IT
- **Text Engine** — Intelligent buffer with type, backspace, replace, clear
- **Layout Switching** — Change keyboard layout at runtime via BLE or HTTP
- **Dead Key / AltGr Support** — International characters on European layouts
- **Stuck Key Prevention** — All keys released on BLE disconnect or clear

### 📱 Phase 3 — iOS App *(planned)*
- Speech-to-text dictation
- Real-time text streaming over BLE

## 📦 Project Structure

```
ios-keyboard/
├── CMakeLists.txt              # ESP-IDF project
├── partitions.csv              # 4MB OTA layout (factory + 2 OTA slots)
├── sdkconfig.defaults          # BLE, WiFi, OTA, watchdog config
├── sdkconfig.defaults.esp32s3  # TinyUSB HID for ESP32-S3
├── components/
│   └── dns_server/             # Captive portal DNS redirect
├── main/
│   ├── app_main.c              # Boot sequence orchestrator
│   ├── ble_nus.c/h             # BLE Nordic UART + command dispatcher
│   ├── wifi_prov.c/h           # WiFi STA/AP with captive portal
│   ├── nvs_store.c/h           # NVS persistence layer
│   ├── http_server.c/h         # REST API (status/ota/layout/reset)
│   ├── ota_update.c/h          # HTTP OTA with progress reporting
│   ├── udp_log.c/h             # UDP log forwarder
│   ├── watchdog.c/h            # Task WDT + heap monitor
│   ├── hid_keyboard.c/h        # TinyUSB HID keyboard driver
│   ├── layout_manager.c/h      # 7-layout scancode tables
│   ├── text_engine.c/h         # Text buffer + diff engine
│   └── portal.html             # Captive portal UI
└── docs/
    └── iOS-Keyboard-FSD.md     # Functional Specification
```

## 🔧 Building & Flashing

### Prerequisites
- ESP-IDF v5.4+
- ESP32-S3 development board

### Build
```bash
cd ios-keyboard
idf.py set-target esp32s3
idf.py build
```

### Flash (local USB)
```bash
idf.py -p /dev/ttyUSB0 flash
```

### Flash (via Universal ESP32 Workbench)
```bash
SLOT_URL=$(curl -s http://esp32-workbench.local:8080/api/devices | jq -r '.slots[0].url')
cd build && esptool.py --port "${SLOT_URL}?ign_set_control" \
  --chip esp32s3 -b 921600 write_flash @flash_args
```

### OTA Update
```bash
curl -X POST http://<device-ip>/ota \
  -H 'Content-Type: application/json' \
  -d '{"url":"http://<server>/firmware.bin"}'
```

## 📡 BLE Command Protocol

Connect to the device's NUS service (`6e400001-b5a3-f393-e0a9-e50e24dcca9e`) and write commands to the RX characteristic:

| Command | Description | Example |
|---------|-------------|---------|
| `T:<text>` | Type text | `T:Hello world` |
| `B:<n>` | Backspace N chars | `B:5` |
| `R:<start>:<count>:<text>` | Replace range | `R:0:5:World` |
| `C` | Clear buffer + release keys | `C` |
| `L:<code>` | Set keyboard layout | `L:DE` |
| `S` | Get status JSON | `S` |
| `O:<url>` | Trigger OTA update | `O:http://...` |
| `F` | Factory reset + reboot | `F` |

## 🌐 HTTP API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/status` | Device status (version, layout, WiFi, BLE, USB, heap, uptime) |
| GET | `/layout` | Current keyboard layout |
| POST | `/layout` | Set layout `{"layout":"DE"}` |
| POST | `/ota` | Trigger OTA `{"url":"..."}` |
| POST | `/reset` | Factory reset (erase NVS + reboot) |

## 🗺️ Supported Layouts

| Code | Layout | Notes |
|------|--------|-------|
| `US` | US English (QWERTY) | Default |
| `DE` | German (QWERTZ) | Y/Z swapped, AltGr for `@[]{}` |
| `CH-DE` | Swiss German (QWERTZ) | AltGr for `@#[]{}~` |
| `FR` | French (AZERTY) | Full AZERTY remapping |
| `UK` | British (QWERTY) | `@` on `'`, `#` on dedicated key |
| `ES` | Spanish (QWERTY) | AltGr for `@#[]{}~` |
| `IT` | Italian (QWERTY) | AltGr for `@#[]{}~` |

## 📊 Stats

- **Firmware size**: ~1.16 MB (7% free in 1216KB partition)
- **Free heap**: ~160 KB at runtime
- **Source code**: ~2,950 lines (C + HTML)
- **Key timing**: ~30ms per keystroke (press + release + gap)

## 📄 License

MIT
