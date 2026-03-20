# 📋 iOS Keyboard — Functional Specification Document

**Version:** 1.0
**Date:** 2026-03-20
**Status:** Phase 1 ✅ Complete | Phase 2 ✅ Complete | Phase 3 🔲 Planned

---

## 1. 🎯 Overview

The iOS Keyboard project converts iPhone speech-to-text input into USB HID keyboard keystrokes via an ESP32-S3 microcontroller. The device acts as a BLE-to-USB bridge: an iOS app sends dictated text over Bluetooth LE, and the ESP32-S3 types it on any connected computer as a standard USB keyboard.

### 1.1 Goals
- 🎤 Enable hands-free typing on any computer via iPhone dictation
- ⌨️ Support 7 international keyboard layouts (US, DE, CH-DE, FR, UK, ES, IT)
- 🔌 Plug-and-play USB HID — no drivers required
- 📡 Remote configuration and OTA updates via WiFi/BLE
- 🛡️ Production-quality reliability (watchdog, heap monitoring, OTA rollback)

### 1.2 Non-Goals
- iOS app development (Phase 3, out of scope for this document)
- USB host mode (device is always a USB peripheral)
- Bluetooth Classic (BLE only)

---

## 2. 🏗️ System Architecture

```
┌─────────────┐         ┌──────────────┐         ┌──────────────┐
│   iPhone     │   BLE   │   ESP32-S3    │   USB   │   Computer   │
│              │ ──NUS──▶│              │ ──HID──▶│              │
│  iOS App     │         │  ios-keyboard │         │  Any OS      │
│  (Phase 3)   │ ◀──TX── │  firmware     │         │              │
└─────────────┘         └──────────────┘         └──────────────┘
                              │ WiFi
                              ▼
                        ┌──────────────┐
                        │  Config/OTA   │
                        │  HTTP Server  │
                        └──────────────┘
```

### 2.1 Hardware
- **MCU**: ESP32-S3 (dual-core Xtensa, WiFi + BLE 5.0, USB-OTG)
- **Flash**: 4MB (3 app partitions: factory + 2 OTA)
- **USB**: USB-OTG port for HID keyboard output
- **Connectivity**: WiFi 802.11 b/g/n + BLE 5.0

### 2.2 Software Stack
- **Framework**: ESP-IDF v5.4
- **BLE**: NimBLE (Nordic UART Service)
- **USB**: TinyUSB (HID keyboard class)
- **Storage**: NVS Flash

---

## 3. 📡 Phase 1 — Infrastructure Foundation

### 3.1 Boot Sequence

```
1. NVS init → load config
2. Network stack init (esp_netif + event loop)
3. UDP logging init (target from NVS or default)
4. WiFi provisioning (STA if creds stored, AP if not)
5. Wait for WiFi before BLE (coexistence, up to 15s)
6. BLE NUS init (device name from NVS)
7. HTTP server start
8. Watchdog start
9. Phase 2 init (layout manager, text engine, HID keyboard)
10. Heartbeat task (status log every 10s)
```

### 3.2 WiFi Provisioning (`wifi_prov.c`)

| Feature | Details |
|---------|---------|
| AP SSID | `iOS-KB-XXXX` (last 4 hex of MAC, or NVS device name) |
| AP Security | Open |
| Captive Portal | HTML form with WiFi + keyboard layout selector |
| Supported Layouts | US, CH-DE, DE, FR, UK, ES, IT |
| STA Retries | 20 attempts before giving up |

### 3.3 BLE Nordic UART Service (`ble_nus.c`)

| Property | Value |
|----------|-------|
| Service UUID | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| RX (write) UUID | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| TX (notify) UUID | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |
| Device Name | `iOS-KB-XXXX` (configurable via NVS) |
| Max Connections | 1 (peripheral mode) |

#### Command Protocol

| Command | Response | Action |
|---------|----------|--------|
| `L:<code>` | `OK:L:<code>` | Set keyboard layout + persist to NVS |
| `S` | JSON status | Return firmware version, layout, WiFi, heap |
| `F` | `OK:F:resetting` | Factory reset (erase NVS + reboot) |
| `O:<url>` | `OK:O:starting` | Start OTA from URL, progress via TX |
| `T:<text>` | `OK` | Type text via HID |
| `B:<n>` | `OK` | Backspace N characters |
| `R:<s>:<c>:<text>` | `OK` | Replace range (start, count, new text) |
| `C` | `OK` | Clear buffer + release all keys |

### 3.4 HTTP Server (`http_server.c`)

| Endpoint | Method | Port | Description |
|----------|--------|------|-------------|
| `/status` | GET | 80 (STA) / 8080 (AP) | JSON device status |
| `/layout` | GET | 80 / 8080 | Current layout |
| `/layout` | POST | 80 / 8080 | Set layout `{"layout":"DE"}` |
| `/ota` | POST | 80 / 8080 | Trigger OTA `{"url":"..."}` |
| `/reset` | POST | 80 / 8080 | Factory reset |

#### `/status` Response
```json
{
  "project": "ios-keyboard",
  "version": "9d36ef7",
  "layout": "US",
  "wifi_connected": true,
  "wifi_rssi": -25,
  "ble_connected": false,
  "usb_mounted": true,
  "free_heap": 160492,
  "uptime_s": 14
}
```

### 3.5 NVS Store (`nvs_store.c`)

| Key | Default | Description |
|-----|---------|-------------|
| `wifi_ssid` | — | WiFi SSID |
| `wifi_pass` | — | WiFi password |
| `kb_layout` | `"US"` | Active keyboard layout |
| `udp_target` | `"192.168.4.1:5555"` | UDP log destination |
| `dev_name` | `"iOS-KB-{MAC4}"` | BLE/AP device name |

Namespace: `ios_kb`

### 3.6 OTA Update (`ota_update.c`)
- Configurable URL via BLE `O:` command or HTTP POST
- Progress reported at 10% increments via BLE TX and ESP_LOG
- Auto-reboot on success
- ESP-IDF A/B OTA with rollback support

### 3.7 Watchdog (`watchdog.c`)
- Task WDT: 60s timeout, heartbeat every 30s
- Heap monitor: warn at 50KB, reboot at 30KB
- Logs events via UDP

### 3.8 UDP Logging (`udp_log.c`)
- Hooks `esp_log_set_vprintf()` to capture all ESP_LOG output
- Async FreeRTOS message buffer (4KB) → UDP sender task
- Target configurable via NVS, default `192.168.4.1:5555`
- Always also prints to UART serial

---

## 4. ⌨️ Phase 2 — USB HID Keyboard

### 4.1 HID Keyboard (`hid_keyboard.c`)

| Property | Value |
|----------|-------|
| USB Class | HID Keyboard (6KRO) |
| Report Descriptor | Standard keyboard (TUD_HID_REPORT_DESC_KEYBOARD) |
| Polling Interval | 10ms |
| Key Timing | 20ms hold + 10ms release gap |
| Manufacturer | `iOS-Keyboard` |
| Product | `iOS-KB HID Keyboard` |

### 4.2 Layout Manager (`layout_manager.c`)

7 complete ASCII scancode tables mapping printable characters (0x20–0x7E) to HID scancodes + modifiers.

| Layout | Type | Special |
|--------|------|---------|
| US | QWERTY | Standard baseline |
| DE | QWERTZ | Y↔Z swap, AltGr for `@[]{}` |
| CH-DE | QWERTZ | Y↔Z swap, AltGr for `@#[]{}~` |
| FR | AZERTY | Full A↔Q, W↔Z, M→`;` remapping |
| UK | QWERTY | `@` on `'`, `#` on dedicated key, `\` on 102nd |
| ES | QWERTY | AltGr for `@#[]{}~\` |
| IT | QWERTY | AltGr for `@#[]{}~` |

Special characters: `\n` → Enter, `\t` → Tab, `\b` → Backspace

### 4.3 Text Engine (`text_engine.c`)

Internal text buffer (4KB max) mirroring the state of what's been typed on the host.

| Function | Description |
|----------|-------------|
| `text_engine_type(text)` | Append text, emit HID keystrokes |
| `text_engine_backspace(n)` | Emit N backspaces, shrink buffer |
| `text_engine_replace(start, count, text)` | Minimal backspace + retype diff |
| `text_engine_clear()` | Backspace entire buffer + release all keys |

#### Replace Algorithm
1. Calculate tail length (chars after replaced region)
2. Backspace: tail_length + count (get to replacement start)
3. Type: new text + old tail
4. Update buffer in-place

---

## 5. ✅ Test Results

### 5.1 Phase 1 Tests

| # | Test | Status |
|---|------|--------|
| 1 | `idf.py build` succeeds targeting ESP32-S3 | ✅ |
| 2 | Flash via workbench RFC2217 serial | ✅ |
| 3 | Captive portal `iOS-KB-XXXX` → configure WiFi + layout → STA reboot | ✅ |
| 4 | WiFi STA connects, gets IP, RSSI -25 dBm | ✅ |
| 5 | UDP boot logs arrive at workbench | ✅ |
| 6 | HTTP `/status` returns complete JSON | ✅ |
| 7 | HTTP `/layout` GET/POST (US → DE → US) | ✅ |
| 8 | BLE scan finds `iOS-KB-2C4C` with NUS service | ✅ |
| 9 | BLE `S` command → status JSON | ✅ |
| 10 | BLE `L:DE` → layout change verified via HTTP | ✅ |
| 11 | BLE `F` → factory reset → device reboots to AP mode | ✅ |
| 12 | Heartbeat logs every 10s, heap stable ~167KB | ✅ |

### 5.2 Phase 2 Tests

| # | Test | Status |
|---|------|--------|
| 1 | USB enumeration — host sees `iOS-KB HID Keyboard` | ✅ |
| 2 | `/status` reports `usb_mounted: true` | ✅ |
| 3 | BLE `T:Hello world` → correct keystrokes on host | ✅ |
| 4 | BLE `B:5` + `T:World` → backspace + retype verified | ✅ |
| 5 | BLE `C` → exact backspace count matches buffer length | ✅ |
| 6 | `L:DE` + `T:yz` → outputs `zy` (QWERTZ Y↔Z swap) | ✅ |
| 7 | Every key press has matching release — no stuck keys | ✅ |
| 8 | OTA via BLE `O:<url>` → download + reboot + new firmware | ✅ |
| 9 | BLE reconnect after OTA reboot | ✅ |

---

## 6. 📊 Resource Usage

| Resource | Value |
|----------|-------|
| Firmware binary | 1.16 MB |
| Partition free space | 7% (84KB) |
| Runtime free heap | ~160 KB |
| Source lines of code | ~2,950 |
| Flash layout | factory (1216K) + ota_0 (1216K) + ota_1 (1216K) |
| NVS partition | 24 KB |

---

## 7. 🔮 Phase 3 — iOS App (Planned)

- 📱 SwiftUI app with speech-to-text engine
- 📡 CoreBluetooth NUS client
- 🎤 Real-time dictation streaming via `T:` commands
- ✏️ Text correction via `R:` replace commands
- ⚙️ Settings: layout selection, device pairing, OTA triggers
