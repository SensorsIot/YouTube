# iOS-Keyboard — Functional Specification Document (FSD)

## 1. System Overview

The iOS-Keyboard system enables an iPhone's speech-to-text engine to drive real USB keystrokes on any computer. It consists of two components: an ESP32-S3 microcontroller that acts as a BLE-to-USB HID bridge, and an iOS app that captures dictated text and transmits it over BLE. The ESP32-S3 receives text via the Nordic UART Service (NUS), maps each character to the correct USB HID scancode for the selected keyboard layout, and types it on the connected host computer.

Because the ESP32-S3's USB port is dedicated to HID keyboard emulation, all firmware updates and debug logging use WiFi (OTA flashing, UDP logging).

**Primary goals:**
- Accurate speech-to-text relay: text on the iPhone must be identical to text typed on the host computer.
- Support for multiple keyboard layouts: US, Swiss-German (CH-DE), German (DE), French (FR), UK, Spanish (ES), Italian (IT).
- Backspace correction: when the iPhone revises its speech-to-text output, the ESP32 sends Backspace keystrokes and retypes the corrected text.
- OTA firmware updates via WiFi (USB port is occupied by HID).
- UDP debug logging for remote diagnostics.

**Non-goals:**
- The iOS app is not part of ESP32 firmware phases (Phase 3, separate project).
- No media keys, mouse, or gamepad HID support.
- No Bluetooth Classic — BLE only.

**Users / stakeholders:**
- End users dictating text on their iPhone for entry into any computer.
- Developers flashing and provisioning the ESP32-S3 device.

**High-level system flow:**
1. User speaks into iPhone → iOS speech-to-text engine produces text.
2. iOS app sends text (and corrections) to ESP32-S3 via BLE NUS.
3. ESP32-S3 maps characters to scancodes for the active keyboard layout.
4. ESP32-S3 sends USB HID keyboard reports to the connected host computer.
5. Host computer receives keystrokes — the result matches the iPhone's text.

## 2. System Architecture

### 2.1 Logical Architecture

The system has three logical subsystems:

- **iOS App** — Captures speech-to-text, tracks text changes (insertions, deletions, corrections), and transmits delta commands to the ESP32-S3 via BLE NUS.
- **ESP32-S3 Firmware** — Receives BLE NUS data, maintains a text buffer mirroring the iPhone's state, computes the minimal edit (backspaces + new characters), maps characters to layout-specific scancodes, and emits USB HID reports.
- **Host Computer** — Any computer with a USB port; receives standard HID keyboard input. No driver or software installation required.

**Data flow:**
```
iPhone (speech-to-text) --(BLE NUS)--> ESP32-S3 --(USB HID)--> Host Computer
                                         |
                                    WiFi (OTA + UDP logging)
```

### 2.2 Hardware / Platform Architecture

| Component | Platform | Connectivity |
|-----------|----------|-------------|
| iPhone | iOS 16+ | BLE 5.0 (NUS) |
| ESP32-S3 DevKit | ESP-IDF v5.x | BLE (NimBLE) + USB OTG (TinyUSB) + WiFi (STA) |
| Host Computer | Windows / macOS / Linux | USB HID (no driver needed) |

**ESP32-S3 selection rationale:** The ESP32-S3 has native USB OTG support required for USB HID device emulation. The original ESP32 and ESP32-C3 lack USB OTG.

**Power:** The ESP32-S3 is powered via the USB connection to the host computer (bus-powered, 500 mA).

### 2.3 Software Architecture

**ESP32-S3 firmware modules:**

| Module | Responsibility |
|--------|---------------|
| `ble_nus` | BLE advertising, NUS service, receive text commands from iPhone |
| `text_engine` | Text buffer management, diff computation, backspace/retype logic |
| `hid_keyboard` | USB HID descriptor, scancode lookup tables, HID report generation |
| `layout_manager` | Keyboard layout definitions (US, CH-DE, DE, FR, UK, ES, IT), layout switching |
| `wifi_prov` | WiFi STA connection, captive portal for provisioning |
| `ota_update` | HTTP-based OTA firmware download, A/B partition management |
| `udp_log` | UDP logging to configurable host:port, parallel to serial |
| `nvs_store` | NVS-based configuration persistence (WiFi creds, layout, UDP target) |
| `http_server` | HTTP endpoints for status, OTA trigger, layout selection |
| `watchdog` | Software watchdog, memory monitoring, automatic recovery |

**Boot sequence:**
1. Initialize NVS → load configuration (WiFi creds, keyboard layout, UDP target).
2. If no WiFi credentials → start captive portal (AP mode).
3. Connect WiFi (STA mode) → start UDP logging.
4. Initialize BLE → start NUS advertising.
5. Initialize USB HID → enumerate as keyboard on host.
6. Start HTTP server (status, OTA trigger).
7. Start watchdog task.
8. Enter main loop — wait for BLE data, process, emit HID reports.

**Persistence / storage:**
- NVS: WiFi credentials, active keyboard layout, UDP log target IP/port, device name.
- OTA: Dual A/B partitions for safe firmware updates with rollback.

**Update model:** OTA via WiFi. Firmware binary hosted on workbench HTTP server. Triggered via HTTP endpoint or BLE command. Automatic rollback on boot failure.

## 3. Implementation Phases

### 3.1 Phase 1 — Infrastructure Foundation

**Scope:** All ESP32-S3 firmware except USB HID keyboard. Focus on BLE, WiFi, OTA, logging, and provisioning infrastructure.

**Deliverables:**
- ESP-IDF project with NimBLE stack, WiFi STA, captive portal provisioning.
- BLE NUS service accepting text data from workbench (simulating iPhone).
- OTA update mechanism via WiFi (HTTP download, A/B partitions, rollback).
- UDP logging to workbench.
- NVS-based configuration storage (WiFi creds, keyboard layout selection, UDP target).
- HTTP server with `/status` and `/ota` endpoints.
- Watchdog and memory monitoring.
- Serial and UDP logging of all state transitions.

**Exit criteria:**
- Device provisions via captive portal successfully.
- BLE NUS receives text strings from workbench BLE emulation.
- OTA firmware update completes successfully via WiFi.
- UDP logs are received on the workbench.
- Factory reset via BLE command restores provisioning mode.
- All Phase 1 test cases pass.

**Dependencies:** ESP-IDF v5.x, NimBLE stack, ESP32-S3 DevKit with USB OTG.

### 3.2 Phase 2 — USB HID Keyboard

**Scope:** Add USB HID keyboard emulation. Integrate the full BLE-to-USB pipeline. Implement keyboard layout mapping for all target languages. Test with workbench emulating iPhone.

**Deliverables:**
- TinyUSB HID keyboard descriptor and report generation.
- Keyboard layout scancode tables for US, CH-DE, DE, FR, UK, ES, IT.
- Text engine: buffer management, diff computation, backspace correction.
- Layout switching via BLE command.
- Integration: BLE NUS text → text engine → HID scancode → USB report.
- Workbench-driven integration test: type "Hello world", delete it, retype it.

**Exit criteria:**
- ESP32-S3 enumerates as HID keyboard on Windows, macOS, and Linux without custom drivers.
- Workbench sends "Hello world" via BLE NUS → appears in host text editor.
- Workbench sends backspace + correction → host text is corrected accurately.
- All seven keyboard layouts produce correct characters for their language.
- Typing throughput ≥ 60 characters/second with no dropped or duplicate keystrokes.
- All Phase 2 test cases pass.

**Dependencies:** Phase 1 complete. TinyUSB stack (included in ESP-IDF). USB OTG port on ESP32-S3.

### 3.3 Phase 3 — iOS App (Out of Scope)

**Scope:** Native iOS app with speech-to-text capture, BLE NUS client, text diff tracking, and layout selection UI.

**Deliverables:** iOS app (separate repository / project).

**Exit criteria:** End-to-end speech-to-text on iPhone → USB keystrokes on host computer.

**Dependencies:** Phase 2 complete. Apple Developer Account. iPhone with BLE 5.0.

## 4. Functional Requirements

### 4.1 Functional Requirements

#### 4.1.1 BLE Communication

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-1.1 | The device shall implement BLE Nordic UART Service (NUS) with UUID 6E400001-B5A3-F393-E0A9-E50E24DCCA9E | Must |
| FR-1.2 | The device shall accept text data via NUS RX characteristic (writes up to MTU size) | Must |
| FR-1.3 | The device shall send status/acknowledgements via NUS TX characteristic (notifications) | Must |
| FR-1.4 | The device shall handle fragmented BLE messages across multiple packets | Should |
| FR-1.5 | The device shall advertise with a discoverable name (configurable, default: `iOS-KB-{MAC_LAST_4}`) | Must |
| FR-1.6 | The device shall accept at least one simultaneous BLE connection | Must |
| FR-1.7 | The device shall resume advertising after disconnection | Must |
| FR-1.8 | The device shall log BLE connection and disconnection events | Should |

#### 4.1.2 USB HID Keyboard

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-2.1 | The device shall enumerate as a standard USB HID keyboard on the host computer | Must |
| FR-2.2 | The device shall be recognized without custom drivers on Windows, macOS, and Linux | Must |
| FR-2.3 | The device shall send standard USB HID keyboard reports (key press + release) | Must |
| FR-2.4 | The device shall support modifier keys (Shift, Ctrl, Alt, AltGr, GUI) | Must |
| FR-2.5 | The device shall send key release reports to prevent stuck keys | Must |
| FR-2.6 | The device shall handle USB suspend and resume | Should |
| FR-2.7 | The device shall re-enumerate after reboot without host intervention | Should |

#### 4.1.3 Keyboard Layouts

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-3.1 | The device shall support keyboard layout selection via BLE command | Must |
| FR-3.2 | The device shall support the following layouts: US, Swiss-German (CH-DE), German (DE), French (FR), UK, Spanish (ES), Italian (IT) | Must |
| FR-3.3 | Key-to-scancode mapping shall be correct for the selected layout | Must |
| FR-3.4 | The active keyboard layout shall persist across reboots (stored in NVS) | Must |
| FR-3.5 | The device shall report the active layout via BLE or HTTP status | Should |

#### 4.1.4 Text Engine (Backspace Correction)

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-4.1 | The device shall maintain a text buffer mirroring the iPhone's current text state | Must |
| FR-4.2 | When the iPhone corrects previously dictated text, the device shall send Backspace keystrokes to delete the incorrect portion and retype the corrected text | Must |
| FR-4.3 | The text typed on the host computer shall be identical to the text displayed on the iPhone | Must |
| FR-4.4 | The device shall handle rapid text changes without losing characters | Must |
| FR-4.5 | The device shall handle Unicode characters by mapping them to the appropriate key combination for the active layout | Should |

#### 4.1.5 WiFi & Provisioning

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-5.1 | The device shall connect to a configured WiFi network in STA mode | Must |
| FR-5.2 | WiFi credentials shall be stored in NVS | Must |
| FR-5.3 | The device shall automatically reconnect on WiFi disconnect | Must |
| FR-5.4 | The device shall start a captive portal (AP mode) when no valid WiFi config exists | Must |
| FR-5.5 | The captive portal shall allow WiFi network selection and credential entry | Must |
| FR-5.6 | The captive portal shall allow keyboard layout selection | Should |
| FR-5.7 | Configuration shall be saved to NVS before reboot | Must |

#### 4.1.6 OTA Updates

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-6.1 | The device shall support firmware update via HTTP download over WiFi | Must |
| FR-6.2 | The device shall verify firmware integrity before applying (checksum) | Must |
| FR-6.3 | The device shall use dual OTA partitions (A/B scheme) | Must |
| FR-6.4 | The device shall rollback to previous firmware on boot failure | Must |
| FR-6.5 | The device shall report current firmware version via HTTP `/status` | Should |
| FR-6.6 | The device shall log OTA progress (download %, verification, apply) | Should |
| FR-6.7 | OTA update shall be triggerable via HTTP endpoint or BLE command | Must |

#### 4.1.7 Logging

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-7.1 | The device shall output structured log messages via serial (USB CDC or UART) | Must |
| FR-7.2 | The device shall send log messages via UDP to a configurable host:port | Should |
| FR-7.3 | UDP logging shall work in parallel with serial logging | Must |
| FR-7.4 | The device shall log boot sequence with firmware version | Must |
| FR-7.5 | The device shall log all state transitions (WiFi, BLE, USB connect/disconnect) | Should |
| FR-7.6 | The device shall continue operating normally if UDP target is unreachable | Must |

#### 4.1.8 Configuration & NVS

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-8.1 | The device shall store configuration parameters in NVS (WiFi creds, layout, UDP target, device name) | Must |
| FR-8.2 | Configuration shall persist across reboots and power cycles | Must |
| FR-8.3 | The device shall use default values when NVS is empty or corrupt | Must |
| FR-8.4 | The device shall support factory reset (erase all NVS config) via BLE command | Must |
| FR-8.5 | After factory reset, the device shall enter captive portal (AP mode) | Must |

#### 4.1.9 HTTP Server

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-9.1 | The device shall expose an HTTP `/status` endpoint returning firmware version, active layout, WiFi RSSI, free heap, and BLE connection state | Should |
| FR-9.2 | The device shall expose an HTTP `/ota` endpoint for triggering firmware update | Must |
| FR-9.3 | The device shall expose an HTTP `/layout` endpoint for querying and setting the active keyboard layout | Should |

### 4.2 Non-Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| NFR-1.1 | BLE-to-USB HID latency shall not exceed 50 ms under normal operating conditions | Must |
| NFR-1.2 | The device shall sustain ≥ 60 characters per second USB HID throughput | Should |
| NFR-1.3 | No dropped or duplicate keystrokes under normal conditions | Must |
| NFR-2.1 | The device shall recover from OTA failure by rolling back to the previous firmware partition | Must |
| NFR-2.2 | OTA download shall not block BLE text processing (background download) | Should |
| NFR-3.1 | The device shall boot to operational state within 5 seconds (excluding WiFi association) | Should |
| NFR-3.2 | Free heap shall remain above 30 KB during sustained operation | Should |
| NFR-4.1 | BLE communication shall use NimBLE (lightweight stack, suitable for coexistence with WiFi) | Must |
| NFR-4.2 | USB HID shall use TinyUSB (included in ESP-IDF) | Must |

### 4.3 Constraints

- The ESP32-S3 USB OTG port is dedicated to HID — serial console for debugging must use UART or USB CDC on a secondary port, or be replaced by UDP logging.
- The host computer's keyboard layout setting must match the ESP32's active layout. The ESP32 sends scancodes, not characters — the host OS interprets them according to its own keyboard layout setting.
- BLE NUS MTU limits individual packet size (typically 244 bytes after negotiation). Longer messages require fragmentation.
- WiFi and BLE share the ESP32-S3's radio — coexistence is supported but may introduce minor latency increases during WiFi-heavy operations (OTA).

## 5. Risks, Assumptions & Dependencies

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| USB HID descriptor rejected by host OS | Medium | High | Test with Windows, macOS, Linux early in Phase 2 |
| Keyboard layout mismatch between ESP32 and host OS causes wrong characters | High | High | Document host-side layout requirement; add layout reporting to status endpoint |
| BLE connection drops during text entry | Low | Medium | Buffer pending text, send on reconnect; send key release on disconnect |
| WiFi + BLE coexistence causes increased latency | Medium | Low | Profile latency during OTA; defer non-critical WiFi operations during active typing |
| Swiss-German (CH-DE) layout complexity (dead keys, accents) | Medium | Medium | Test all special characters; implement dead key sequences |
| iPhone speech-to-text correction timing race | Medium | Medium | Text engine must handle rapid correction bursts atomically |

**Assumptions:**
- The host computer provides USB bus power (500 mA) (assumed).
- iOS is the primary mobile platform; Android support is deferred (assumed).
- The host computer's OS keyboard layout is set to match the ESP32's active layout (assumed).
- The ESP32-S3 DevKit has both USB OTG and UART available simultaneously (assumed).
- The workbench BLE emulation can simulate iPhone NUS client behavior for Phase 1–2 testing (assumed).

**Dependencies:**
- ESP-IDF v5.x with TinyUSB and NimBLE support.
- ESP32-S3 DevKit with USB OTG capability.
- Universal ESP32 Workbench (https://github.com/SensorsIot/Universal-ESP32-Workbench) for flashing, testing, and BLE simulation.
- New GitHub repository on https://github.com/SensorsIot/YouTube for project source code.

## 6. Interface Specifications

### 6.1 External Interfaces

#### 6.1.1 BLE Nordic UART Service (NUS)

| Property | Value |
|----------|-------|
| Service UUID | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX Characteristic UUID | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX Characteristic UUID | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |
| Direction | iPhone → ESP32 (RX write), ESP32 → iPhone (TX notify) |
| MTU | Negotiated, typically 244 bytes |

#### 6.1.2 USB HID Keyboard

| Property | Value |
|----------|-------|
| USB Class | HID (0x03) |
| USB Subclass | Boot Interface (0x01) |
| USB Protocol | Keyboard (0x01) |
| Report Descriptor | Standard 6KRO keyboard |
| Report Format | 8 bytes: [modifier, reserved, key1, key2, key3, key4, key5, key6] |

#### 6.1.3 HTTP API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/status` | GET | Returns JSON: firmware version, active layout, WiFi RSSI, free heap, BLE state, uptime |
| `/ota` | POST | Triggers OTA download from provided URL |
| `/layout` | GET | Returns current keyboard layout |
| `/layout` | POST | Sets keyboard layout (body: `{"layout": "US"}`) |

### 6.2 Internal Interfaces

| Interface | From → To | Protocol | Data |
|-----------|-----------|----------|------|
| BLE text receive | `ble_nus` → `text_engine` | Function call | Raw text string, command type |
| Text-to-scancode | `text_engine` → `hid_keyboard` | Function call | Sequence of (scancode, modifier) pairs |
| Layout lookup | `hid_keyboard` → `layout_manager` | Function call | Character → (scancode, modifier) mapping |
| Config read/write | All modules → `nvs_store` | Function call | Key-value pairs |
| Log output | All modules → `udp_log` | Function call | Log level, tag, message |

### 6.3 Data Models / Schemas

#### BLE NUS Command Format

Commands sent from iPhone (or workbench) to ESP32 via NUS RX:

| Command | Format | Description |
|---------|--------|-------------|
| Text input | `T:<text>` | Type the given text |
| Replace | `R:<start>:<count>:<text>` | Delete `count` chars at `start`, insert `text` |
| Backspace | `B:<count>` | Send `count` backspace keystrokes |
| Set layout | `L:<layout_code>` | Switch keyboard layout (US, CH-DE, DE, FR, UK, ES, IT) |
| Clear | `C` | Clear text buffer and release all keys |
| Status | `S` | Request current status via TX notify |
| Factory reset | `F` | Erase NVS, reboot into provisioning mode |
| OTA | `O:<url>` | Trigger OTA update from URL |

#### HTTP `/status` Response

```json
{
  "firmware": "1.0.0",
  "layout": "US",
  "wifi_rssi": -55,
  "free_heap": 125000,
  "ble_connected": true,
  "usb_mounted": true,
  "uptime_s": 3600
}
```

### 6.4 Keyboard Layout Scancode Tables

Each layout maps Unicode characters to (HID scancode, modifier bitmask) pairs:

| Layout Code | Language | Notable Characters |
|-------------|----------|-------------------|
| `US` | English (US) | Standard QWERTY, `@` = Shift+2 |
| `CH-DE` | Swiss German | QWERTZ, `@` = AltGr+2, `ü` `ö` `ä` native, dead keys for `^` `~` `` ` `` |
| `DE` | German | QWERTZ, `@` = AltGr+Q, `ß` `ü` `ö` `ä` native |
| `FR` | French | AZERTY, `@` = AltGr+0, `é` `è` `ê` `à` native |
| `UK` | English (UK) | QWERTY, `@` = Shift+', `£` = Shift+3 |
| `ES` | Spanish | QWERTY, `@` = AltGr+2, `ñ` native, dead keys for accents |
| `IT` | Italian | QWERTY, `@` = AltGr+Q, `è` `é` `à` `ò` `ù` native |

## 7. Operational Procedures

### 7.1 First-Time Setup / Flashing

1. Clone the project repository from GitHub.
2. Set up ESP-IDF v5.x development environment.
3. Connect ESP32-S3 DevKit via USB to development machine.
4. Build and flash firmware: `idf.py build && idf.py flash`.
5. After first flash, the device enters captive portal mode (AP: `iOS-KB-XXXX`).
6. Connect to the AP, open the portal, configure WiFi credentials and keyboard layout.
7. Device reboots, connects to WiFi, and is ready for BLE + USB HID operation.
8. All subsequent firmware updates use OTA via WiFi.

### 7.2 OTA Firmware Update

1. Build new firmware: `idf.py build`.
2. Upload firmware binary to workbench: `curl -X POST http://esp32-workbench.local:8080/api/firmware/upload -F "project=ios-keyboard" -F "file=@build/ios-keyboard.bin"`.
3. Trigger OTA via HTTP: `curl -X POST http://<device-ip>/ota -d '{"url":"http://esp32-workbench.local:8080/firmware/ios-keyboard/latest"}'`.
4. Monitor progress via UDP logs.
5. Device reboots into new firmware. If boot fails, automatic rollback to previous version.

### 7.3 Normal Operation

1. Connect ESP32-S3 to host computer via USB cable → enumerates as HID keyboard.
2. Power on → WiFi connects → BLE advertises.
3. Open iOS app → connect to `iOS-KB-XXXX` via BLE.
4. Select keyboard layout in app (must match host OS setting).
5. Dictate text → appears on host computer in real time.
6. Speech corrections on iPhone → ESP32 backspaces and retypes automatically.

### 7.4 Factory Reset

1. Send factory reset command via BLE: `F` command on NUS RX.
2. Or send via HTTP: `curl -X POST http://<device-ip>/reset`.
3. Device erases NVS configuration and reboots.
4. Device enters captive portal mode for reconfiguration.

### 7.5 Recovery Procedures

| Scenario | Recovery |
|----------|----------|
| WiFi credentials changed | Factory reset → reconfigure via captive portal |
| OTA firmware is broken | Automatic A/B rollback on boot failure |
| Device unresponsive | Power cycle; if persistent, factory reset via button hold (5s) |
| BLE won't connect | Check BLE advertising via workbench scan; power cycle device |
| Stuck keys on host | Disconnect USB cable; device sends all-keys-up on reconnect |

## 8. Verification & Validation

### 8.1 Phase 1 Verification — Infrastructure

#### BLE Tests

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| TC-BLE-100 | BLE Discovery and Connection | Power on → scan → connect → enumerate services → disconnect → verify re-advertising | Device discoverable, connection succeeds, NUS service listed, advertising resumes |
| TC-BLE-101 | NUS Data Transfer | Connect → subscribe TX → write test string to RX → verify response → send 200-byte payload → verify reassembly | Bidirectional transfer works, fragmentation handled |
| TC-BLE-103 | BLE Notification Latency | Connect → subscribe → trigger data change → measure latency → trigger 10 rapid changes | Latency < 100 ms, no lost or reordered notifications |

#### WiFi & Provisioning Tests

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| TC-CP-100 | Captive Portal First Boot | Boot with empty NVS → connect to AP → open portal → enter WiFi creds → save → reboot → verify STA connection | Configuration persists, WiFi connects |
| TC-CP-101 | Captive Portal via Button | Running in STA → hold button 5s → AP activates → reconfigure → reboot | Button triggers AP, reconfiguration works |
| EC-100 | Network Disconnect | Active operation → disconnect WiFi → wait → reconnect | Automatic recovery, no crash |
| EC-110 | WiFi Signal Degradation | Move away from AP → monitor RSSI → return | Graceful degradation, automatic recovery |

#### OTA Tests

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| TC-OTA-100 | Successful OTA Update | Check version → upload new firmware → trigger OTA → monitor progress → verify new version | Update completes, correct version running |
| TC-OTA-101 | OTA Rollback | Flash broken firmware via OTA → boot fails → rollback | Automatic rollback, device recoverable |
| TC-OTA-102 | OTA Version Reporting | Query `/status` → update → query again | Version reflects running firmware |
| EC-OTA-200 | Network Loss During OTA | Start OTA → disconnect WiFi at 50% → verify no crash → retry | Previous firmware intact, retry succeeds |
| EC-OTA-201 | Power Loss During OTA | Start OTA → cut power during write → restore → verify | A/B scheme prevents brick |

#### Logging Tests

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| TC-LOG-100 | Serial Boot Log | Power on → monitor serial | Firmware version logged, correct format |
| TC-LOG-101 | UDP Log Delivery | Configure UDP target → boot → check workbench logs | Boot messages received, format matches serial |
| TC-LOG-103 | State Transition Logging | Boot → WiFi connect → BLE connect → BLE disconnect | Every transition produces a log entry |
| EC-LOG-200 | UDP Target Unreachable | Configure unreachable target → boot | Device operates normally, serial works |

#### NVS Tests

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| TC-NVS-100 | Config Persistence | Configure → reboot → read config | Values identical after reboot |
| TC-NVS-101 | Default Values on First Boot | Erase NVS → boot → check defaults → verify AP mode | Clean first boot, no crash |
| TC-NVS-103 | Factory Reset via BLE Command | Send factory reset BLE command → verify NVS cleared → AP mode | Remote reset works |
| EC-NVS-200 | NVS Corruption Recovery | Corrupt NVS → reboot → verify defaults | Graceful fallback, no crash |

#### Watchdog Tests

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| TC-WDT-100 | Software Watchdog | Simulate task hang → wait for timeout → verify reboot | Hung task detected, automatic recovery |
| TC-WDT-102 | Memory Watchdog | Simulate memory pressure → verify warning → verify reboot at critical | Preventive reboot before crash |
| EC-WDT-200 | Watchdog During WiFi Disconnect | Disconnect WiFi → wait 5 min → verify no false trigger | Watchdog independent of WiFi state |
| EC-WDT-201 | Watchdog During OTA | Start OTA (>60s) → verify no timeout | OTA completes without WDT interference |

#### BLE Edge Cases

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| EC-BLE-200 | Disconnect During Transfer | Send large payload → force disconnect → verify recovery | No crash, advertising resumes |
| EC-BLE-201 | Rapid Connect/Disconnect | Connect/disconnect 20 times → check heap → test data transfer | No memory leak, device functional |
| EC-BLE-203 | Concurrent BLE and WiFi | Both active → transfer on both → heavy WiFi (OTA) → verify BLE | Both radios functional, no crash |

### 8.2 Phase 2 Verification — USB HID Keyboard

#### USB HID Tests

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| TC-HID-100 | USB Enumeration | Connect to Windows → check Device Manager → repeat macOS → repeat Linux | Recognized as HID keyboard on all OSes, no driver |
| TC-HID-101 | Keyboard Layout Accuracy | Select US → send "Hello, World! 123" → verify → send special chars → switch DE → send "Über straße" → switch FR → send "café résumé" | All characters correct per layout |
| TC-HID-102 | Modifier Keys | Send Ctrl+C → Ctrl+V → Shift+A → verify | Modifier combinations produce correct actions |
| TC-HID-103 | Typing Throughput | Send 100-char string → measure time → send 500-char paragraph → verify no drops | ≥ 60 chars/s, zero character loss |

#### USB HID Edge Cases

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| EC-HID-200 | USB Disconnect/Reconnect | Unplug USB → replug → verify re-enumeration → type | Clean re-enumeration, no stuck keys |
| EC-HID-201 | USB Suspend/Resume | Host sleep → wake → type | Resumes without re-enumeration |
| EC-HID-203 | Stuck Key Prevention | Send key press → disconnect BLE → verify key release → reconnect | No stuck keys under any failure |
| EC-HID-204 | Concurrent BLE + USB Pipeline | Send text via BLE NUS → verify USB HID output → rapid commands → disconnect BLE mid-string | Clean pipeline, no corruption |

#### Integration Tests

| Test ID | Feature | Procedure | Success Criteria |
|---------|---------|-----------|-----------------|
| TC-INT-001 | Hello World E2E | Workbench sends "Hello world" via BLE NUS → verify on host text editor | Exact text match |
| TC-INT-002 | Backspace Correction | Workbench sends "Helo" → then correction to "Hello" → verify host shows "Hello" | Backspace + retype works |
| TC-INT-003 | Layout Switch | Select US → type → switch to DE → type → verify both correct | Layout switching works mid-session |
| TC-INT-004 | All Layouts | For each layout (US, CH-DE, DE, FR, UK, ES, IT): send layout-specific test string → verify | All 7 layouts produce correct output |
| TC-INT-005 | Long Dictation | Send 1000-character text via BLE → verify complete on host | No character loss over extended input |

### 8.3 Acceptance Tests

| Test ID | Scenario | Success Criteria |
|---------|----------|-----------------|
| AT-001 | End-to-end dictation (Phase 3) | User speaks on iPhone → text appears correctly on host computer |
| AT-002 | Speech correction | iPhone auto-corrects a word → host text is corrected via backspace + retype |
| AT-003 | Multi-language | User switches between US and DE layouts → both produce correct characters |
| AT-004 | Sustained operation | Device operates for 8 hours without crash, memory leak, or stuck keys |
| AT-005 | OTA in the field | OTA update while device is in normal use → update applies, device resumes |

### 8.4 Traceability Matrix

| Requirement | Priority | Test Case(s) | Status |
|------------|----------|-------------|--------|
| FR-1.1 | Must | TC-BLE-100 | Covered |
| FR-1.2 | Must | TC-BLE-101 | Covered |
| FR-1.3 | Must | TC-BLE-101 | Covered |
| FR-1.4 | Should | TC-BLE-101 | Covered |
| FR-1.5 | Must | TC-BLE-100 | Covered |
| FR-1.6 | Must | TC-BLE-100 | Covered |
| FR-1.7 | Must | TC-BLE-100, EC-BLE-200 | Covered |
| FR-1.8 | Should | TC-LOG-103 | Covered |
| FR-2.1 | Must | TC-HID-100 | Covered |
| FR-2.2 | Must | TC-HID-100 | Covered |
| FR-2.3 | Must | TC-HID-101, TC-HID-103 | Covered |
| FR-2.4 | Must | TC-HID-102 | Covered |
| FR-2.5 | Must | EC-HID-203 | Covered |
| FR-2.6 | Should | EC-HID-201 | Covered |
| FR-2.7 | Should | EC-HID-200 | Covered |
| FR-3.1 | Must | TC-INT-003 | Covered |
| FR-3.2 | Must | TC-INT-004 | Covered |
| FR-3.3 | Must | TC-HID-101, TC-INT-004 | Covered |
| FR-3.4 | Must | TC-NVS-100 | Covered |
| FR-3.5 | Should | TC-OTA-102 | Covered |
| FR-4.1 | Must | TC-INT-001, TC-INT-002 | Covered |
| FR-4.2 | Must | TC-INT-002 | Covered |
| FR-4.3 | Must | TC-INT-001, TC-INT-005 | Covered |
| FR-4.4 | Must | TC-HID-103, TC-INT-005 | Covered |
| FR-4.5 | Should | TC-INT-004 | Covered |
| FR-5.1 | Must | TC-CP-100 | Covered |
| FR-5.2 | Must | TC-NVS-100 | Covered |
| FR-5.3 | Must | EC-100 | Covered |
| FR-5.4 | Must | TC-CP-100 | Covered |
| FR-5.5 | Must | TC-CP-100 | Covered |
| FR-5.6 | Should | TC-CP-100 | Covered |
| FR-5.7 | Must | TC-NVS-100 | Covered |
| FR-6.1 | Must | TC-OTA-100 | Covered |
| FR-6.2 | Must | TC-OTA-100 | Covered |
| FR-6.3 | Must | TC-OTA-101, EC-OTA-201 | Covered |
| FR-6.4 | Must | TC-OTA-101 | Covered |
| FR-6.5 | Should | TC-OTA-102 | Covered |
| FR-6.6 | Should | TC-OTA-100 | Covered |
| FR-6.7 | Must | TC-OTA-100 | Covered |
| FR-7.1 | Must | TC-LOG-100 | Covered |
| FR-7.2 | Should | TC-LOG-101 | Covered |
| FR-7.3 | Must | TC-LOG-101 | Covered |
| FR-7.4 | Must | TC-LOG-100 | Covered |
| FR-7.5 | Should | TC-LOG-103 | Covered |
| FR-7.6 | Must | EC-LOG-200 | Covered |
| FR-8.1 | Must | TC-NVS-100 | Covered |
| FR-8.2 | Must | TC-NVS-100 | Covered |
| FR-8.3 | Must | TC-NVS-101 | Covered |
| FR-8.4 | Must | TC-NVS-103 | Covered |
| FR-8.5 | Must | TC-NVS-103 | Covered |
| FR-9.1 | Should | TC-OTA-102 | Covered |
| FR-9.2 | Must | TC-OTA-100 | Covered |
| FR-9.3 | Should | TC-INT-003 | Covered |
| NFR-1.1 | Must | TC-HID-103, TC-BLE-103 | Covered |
| NFR-1.2 | Should | TC-HID-103 | Covered |
| NFR-1.3 | Must | TC-HID-103, TC-INT-005 | Covered |
| NFR-2.1 | Must | TC-OTA-101 | Covered |
| NFR-2.2 | Should | EC-OTA-203 | Covered |
| NFR-3.1 | Should | TC-LOG-100 | Covered |
| NFR-3.2 | Should | TC-WDT-102 | Covered |
| NFR-4.1 | Must | TC-BLE-100 | Covered |
| NFR-4.2 | Must | TC-HID-100 | Covered |

## 9. Troubleshooting Guide

| Symptom | Likely Cause | Diagnostic Steps | Corrective Action |
|---------|-------------|-----------------|-------------------|
| Device not visible in BLE scan | BLE not advertising, or already connected to another client | Check serial/UDP log for BLE init errors; verify only one client connected | Power cycle; disconnect other BLE clients |
| Wrong characters on host | Keyboard layout mismatch between ESP32 and host OS | Check active layout via `/status`; compare with host OS keyboard setting | Set host OS layout to match ESP32 layout, or change ESP32 layout via BLE `L:` command |
| Stuck keys on host | BLE disconnected without key release | Check for stuck modifiers in host OS | Unplug/replug USB; device sends all-keys-up on USB re-enumeration |
| USB not recognized by host | TinyUSB descriptor issue, or USB cable is charge-only | Check `dmesg` (Linux) or Device Manager (Windows) for enumeration errors | Try different USB cable (data-capable); check USB descriptor in firmware |
| OTA fails mid-download | WiFi dropout, server unreachable, or firmware too large | Check UDP logs for download progress and error codes | Verify WiFi stability; check firmware binary size vs partition size; retry |
| Device keeps rebooting | Watchdog timeout, crash loop, or OTA rollback loop | Monitor serial output for panic backtrace or WDT reset reason | Flash known-good firmware via USB (one-time); investigate crash cause |
| Captive portal not appearing | AP mode not started, or phone not redirecting | Check serial log for AP start message; verify phone is connected to device AP | Manually navigate to `192.168.4.1` in browser; factory reset if AP won't start |
| Characters dropped during fast dictation | BLE throughput limit, or HID queue overflow | Monitor BLE RX rate and HID send rate in logs | Reduce dictation speed; check for BLE MTU negotiation; increase HID queue depth |
| Special characters missing (ä, ñ, é) | Layout table incomplete or wrong layout selected | Send known test string; compare output | Verify layout selection; report missing character for layout table update |

## 10. Appendix

### 10.1 Constants & Configuration Defaults

| Parameter | Default Value | Storage |
|-----------|--------------|---------|
| BLE device name | `iOS-KB-{MAC_LAST_4}` | NVS |
| Keyboard layout | `US` | NVS |
| UDP log target IP | `192.168.4.1` (workbench) | NVS |
| UDP log target port | `5555` | NVS |
| WiFi reconnect interval | `5 s` | Compile-time |
| Watchdog timeout | `60 s` | Compile-time |
| Heap warning threshold | `50 KB` | Compile-time |
| Heap critical threshold | `30 KB` | Compile-time |
| HID report interval | `10 ms` | Compile-time |
| BLE advertising interval | `100 ms` | Compile-time |
| OTA partition size | `1.5 MB` (per slot) | Partition table |
| NVS partition size | `24 KB` | Partition table |

### 10.2 BLE UUIDs

| Service/Characteristic | UUID |
|----------------------|------|
| NUS Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| NUS RX (write) | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| NUS TX (notify) | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

### 10.3 USB HID Report Format

Standard 8-byte keyboard report:

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Modifier | Bitmask: bit 0=LCtrl, 1=LShift, 2=LAlt, 3=LGUI, 4=RCtrl, 5=RShift, 6=RAlt (AltGr), 7=RGUI |
| 1 | Reserved | Always 0x00 |
| 2-7 | Keys | Up to 6 simultaneous key scancodes (6KRO) |

### 10.4 Partition Table Layout

| Name | Type | Offset | Size | Description |
|------|------|--------|------|-------------|
| nvs | data | 0x9000 | 24 KB | Non-volatile storage |
| otadata | data | 0xF000 | 8 KB | OTA state tracking |
| ota_0 | app | 0x10000 | 1.5 MB | OTA partition A |
| ota_1 | app | 0x190000 | 1.5 MB | OTA partition B |

### 10.5 Supported Keyboard Layouts Summary

| Code | QWERTY/QWERTZ/AZERTY | AltGr Required | Dead Keys | Special Characters |
|------|----------------------|----------------|-----------|-------------------|
| US | QWERTY | No | No | Standard ASCII |
| CH-DE | QWERTZ | Yes (`@` `#` `\` `{` `}` `[` `]`) | Yes (`^` `~` `` ` ``) | ü ö ä |
| DE | QWERTZ | Yes (`@` `{` `}` `[` `]` `\`) | Yes (`^` `` ` ``) | ü ö ä ß |
| FR | AZERTY | Yes (`@` `#` `{` `}` `[` `]` `\`) | Yes (`^` `¨` `` ` ``) | é è ê ë à â ù û ç |
| UK | QWERTY | No | No | £ (Shift+3) |
| ES | QWERTY | Yes (`@` `#` `{` `}` `[` `]`) | Yes (`^` `` ` `` `¨` `~`) | ñ ¿ ¡ |
| IT | QWERTY | Yes (`@` `#` `{` `}` `[` `]`) | No | è é à ò ù |
