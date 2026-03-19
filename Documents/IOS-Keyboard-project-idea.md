# iOS-Keyboard Project

The iOS-Keyboard system is a two-part solution consisting of an ESP32-based USB HID keyboard emulator and an iOS app that communicates with it over BLE  (Nordic UART Service). Together, they allow speech-to-text on an iPhone to be transmitted as real USB keystrokes to any computer.

## Main Function

I want to use the iPhone's text-to-speech function to write text in several languages (selectable). The text should be transferred via BLE to an ESP32-S3 that emulates a keyboard and types it into a computer. Make sure that the text on the iPhone is identical to the text typed by the ESP32. Use Backspace to correct it if the iPhone corrects its text.

The ESP32 should emulate the appropriate keyboard for the language. To begin, I want US, Swiss-German, DE, FR, UK, ES, IT.

Because we use a USB for the project, the ESP32 should use OTA flashing and logging. 

## Implementation Phases

1. Implement all ESP32 functions except the USB HID keyboard. In this phase, we want to debug the infrastructure, such as OTA flashing and debugging.
2. Implement the HID keyboard. As a test, the workbench should emulate the iPhone and type "Hello world", delete it, and write it again.
3. Implement the IOS app (out of scope for today).

## Tools

Use ESP-IDF and Apple native tools for the project.
Use the ESP32-workbench (https://github.com/SensorsIot/Universal-ESP32-Workbench)  with its skills for flashing, testing, and iPhone's BLE simulation.

Create a new GitHub Repository on https://github.com/SensorsIot/YouTube
