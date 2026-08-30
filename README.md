# ESP32-BLE-Gamepad

![Build](https://github.com/lemmingDev/ESP32-BLE-Gamepad/actions/workflows/main.yml/badge.svg)
![PlatformIO](https://github.com/lemmingDev/ESP32-BLE-Gamepad/actions/workflows/platformio.yml/badge.svg)

Bluetooth LE Gamepad library for the ESP32. Supports three operating modes: Generic HID, SInput (SDL3 native), and XInput (Xbox emulation).

## Supported Boards

This library depends only on [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) and has no chip-specific code, so it should work on any Espressif MCU with a BLE radio. The following are currently built and compile-tested in CI (see [main.yml](.github/workflows/main.yml)):

- ESP32
- ESP32-S3
- ESP32-C3
- ESP32-C6

Other BLE-capable variants (e.g. ESP32-C2, ESP32-H2) are likely to work too, but aren't currently covered by CI. Note that plain ESP32-S2 has no Bluetooth radio at all, so it can't run this library.

## Mode Comparison

| Feature | Generic | SInput | XInput |
|---------|:-------:|:------:|:------:|
| Buttons | 1-128 | Up to 25 (face, shoulders, stick clicks, triggers, paddles, capture, touchpad clicks, power, misc) | 11 (A/B/X/Y/LB/RB/LS/RS/Select/Start/Home) |
| Thumbsticks | 2 (configurable axes) | 2 (left/right) | 2 (left/right) |
| Triggers | 2 (analog) | 2 (analog) | 2 (analog) |
| D-pad | Up to 4 hat switches | 1 hat switch | 1 hat switch |
| Gyroscope/Accelerometer | Via motion API | Via SInput IMU | No |
| Touchpad | No | 1-2 touchpads | No |
| Rumble | Via Output Report | ERM simulation | Strong/weak motors + trigger vibration |
| Player LED | No | Yes (1-based index) | No |
| RGB LED | No | Yes (24-bit color) | No |
| Battery reporting | Yes (standard BLE) | Yes (SInput power state) | No |
| SDL3 native recognition | No | Yes | No |
| Windows XInput support | No | No | Yes |
| Linux compatibility | Yes | Yes (SDL3 3.4+) | Yes (xpad driver) |
| macOS compatibility | Yes (GCController) | Yes (GCController + SDL3) | Yes (GCController, native Xbox support since Big Sur) |
| Android compatibility | Yes (different mapping) | No | No |
| Configurable VID/PID | Yes | Fixed (0x2E8A/0x10C6) | Fixed (0x045E/0x02FD or 0x0B13) |
| Use case | Custom apps, any OS | SDL3 games, Steam | Xbox-compatible games |

### Which mode should I use?

- **Generic** -- Default. Works everywhere as a standard HID gamepad. Use this if you're building a custom app or need maximum configurability.
- **SInput** -- Use this for SDL3 games, Steam, or any `SDL_GameController`-aware app. Gets you native recognition, rumble, player LED, RGB, IMU, and touchpad without per-VID/PID driver support.
- **XInput** -- Use this for Windows games that expect an Xbox controller. Broad compatibility with games that use XInput/DirectInput. Also works natively on macOS (Xbox controllers are supported since macOS Big Sur) and Linux (via `xpad` driver).

> **Note on VID/PID**: SInput and XInput modes automatically set a specific USB Vendor ID and Product ID that host drivers expect. **Do not override `setVid()`/`setPid()` in these modes** — the host OS identifies the device by VID/PID and loads the matching driver (SDL's SInput driver for `0x2E8A:0x10C6`, Xbox drivers for `0x045E:*`). Changing the VID/PID silently breaks driver recognition. `setVid()`/`setPid()` are only intended for Generic mode, where any VID/PID is fine.

## Quick Start

### Generic Mode (default)

```cpp
#include <BleGamepad.h>

BleGamepad bleGamepad;

void setup() {
  bleGamepad.begin(); // 16 buttons, all axes, 1 hat
}

void loop() {
  if (bleGamepad.isConnected()) {
    bleGamepad.press(BUTTON_1);
    bleGamepad.sendReport();
    delay(500);
    bleGamepad.release(BUTTON_1);
    bleGamepad.sendReport();
    delay(500);
  }
}
```

### SInput Mode

```cpp
#include <BleGamepad.h>

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup() {
  config.setGamepadMode(GamepadMode::SInput);
  config.setEnableRumble(true);
  bleGamepad.begin(&config);
}

void loop() {
  if (bleGamepad.isConnected()) {
    bleGamepad.press(BUTTON_1);
    bleGamepad.sendReport();

    if (bleGamepad.isRumbleReceived()) {
      Serial.printf("Rumble: weak=%d strong=%d\n",
                    bleGamepad.getRumbleLeftAmplitude(),
                    bleGamepad.getRumbleRightAmplitude());
    }
    delay(10);
  }
}
```

### XInput Mode

```cpp
#include <BleGamepad.h>

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup() {
  config.setGamepadMode(GamepadMode::XInput);
  bleGamepad.begin(&config);
}

void loop() {
  if (bleGamepad.isConnected()) {
    bleGamepad.press(BUTTON_1); // A
    bleGamepad.sendReport();

    if (bleGamepad.isXInputRumbleReceived()) {
      Serial.printf("Rumble: strong=%d weak=%d\n",
                    bleGamepad.getXInputStrongMotor(),
                    bleGamepad.getXInputWeakMotor());
    }
    delay(10);
  }
}
```

## Installation

1. Make sure you can use the ESP32 with the Arduino IDE. [Instructions can be found here.](https://github.com/espressif/arduino-esp32#installation-instructions)
2. [Download the latest release of this library from the release page.](https://github.com/lemmingDev/ESP32-BLE-Gamepad/releases)
3. In the Arduino IDE go to "Sketch" -> "Include Library" -> "Add .ZIP Library..." and select the file you just downloaded.
4. In the Arduino IDE go to "Tools" -> "Manage Libraries..." -> Filter for "NimBLE-Arduino" by h2zero and install.
5. You can now go to "File" -> "Examples" -> "ESP32 BLE Gamepad" and select an example to get started.

PlatformIO: add `h2zero/NimBLE-Arduino` to your `lib_deps` and `esp32:esp32` to your platform.

## Examples

### Generic Examples
| Example | Description |
|---------|-------------|
| [Gamepad](examples/Generic/Gamepad/Gamepad.ino) | Basic button presses and axis movement |
| [IndividualAxes](examples/Generic/IndividualAxes/IndividualAxes.ino) | Set each axis independently |
| [TestAll](examples/Generic/TestAll/TestAll.ino) | Exercise all features |
| [FlightControllerTest](examples/Generic/FlightControllerTest/FlightControllerTest.ino) | Flight controller with simulation controls |
| [DrivingControllerTest](examples/Generic/DrivingControllerTest/DrivingControllerTest.ino) | Driving controller with steering/brake/accelerator |
| [MotionController](examples/Generic/MotionController/MotionController.ino) | Gyroscope and accelerometer |
| [PotAsAxis](examples/Generic/PotAsAxis/PotAsAxis.ino) | Map analog pot to axis |
| [SpecialButtons](examples/Generic/SpecialButtons/SpecialButtons.ino) | Start, Select, Home, etc. |
| [TestFeatureReports](examples/Generic/TestFeatureReports/TestFeatureReports.ino) | HID Feature Report exchange |
| [TestReceivingOutputReport](examples/Generic/TestReceivingOutputReport/TestReceivingOutputReport.ino) | Receive HID Output Reports |
| [MultipleButtons](examples/Generic/MultipleButtons/MultipleButtons.ino) | Multiple simultaneous buttons |
| [MultipleButtonsAndHats](examples/Generic/MultipleButtonsAndHats/MultipleButtonsAndHats.ino) | Multiple buttons and hat switches |
| [CharacteristicsConfiguration](examples/Generic/CharacteristicsConfiguration/CharacteristicsConfiguration.ino) | Custom BLE characteristics |
| [Keypad4x4](examples/Generic/Keypad4x4/Keypad4x4.ino) | 4x4 keypad as buttons |
| [ForcePairingMode](examples/Generic/ForcePairingMode/ForcePairingMode.ino) | Force re-pairing |
| [GetPeerInfo](examples/Generic/GetPeerInfo/GetPeerInfo.ino) | Query connected peer |
| [SetBatteryLevel](examples/Generic/SetBatteryLevel/SetBatteryLevel.ino) | Set battery percentage |
| [SetBatteryPowerState](examples/Generic/SetBatteryPowerState/SetBatteryPowerState.ino) | Set battery power state |
| [Diagnostics](examples/Generic/Diagnostics/Diagnostics.ino) | Connection diagnostics |
| [SingleButton](examples/Generic/SingleButton/SingleButton.ino) | Single button debounce |
| [SingleButtonDebounce](examples/Generic/SingleButtonDebounce/SingleButtonDebounce.ino) | Debounced single button |
| [MultipleButtonsDebounce](examples/Generic/MultipleButtonsDebounce/MultipleButtonsDebounce.ino) | Debounced multiple buttons |
| [Fightstick](examples/Generic/Fightstick/Fightstick.ino) | Fightstick layout |

### SInput Examples
| Example | Description |
|---------|-------------|
| [SInputRumble](examples/SInput/SInputRumble/SInputRumble.ino) | Rumble/vibration reception |
| [SInputPlayerLED](examples/SInput/SInputPlayerLED/SInputPlayerLED.ino) | Player LED assignment |
| [SInputRGB](examples/SInput/SInputRGB/SInputRGB.ino) | RGB LED via discrete PWM pins |
| [SInputRGB_NeoPixel](examples/SInput/SInputRGB_NeoPixel/SInputRGB_NeoPixel.ino) | RGB LED via WS2812/NeoPixel strip |
| [SInputIMU](examples/SInput/SInputIMU/SInputIMU.ino) | Gyroscope and accelerometer |
| [SInputTouchpad](examples/SInput/SInputTouchpad/SInputTouchpad.ino) | Dual touchpad input |
| [SInputBattery](examples/SInput/SInputBattery/SInputBattery.ino) | Battery level reporting |
| [SInputFullGamepad](examples/SInput/SInputFullGamepad/SInputFullGamepad.ino) | All features combined |

### XInput Examples
| Example | Description |
|---------|-------------|
| [XInputGamepad](examples/XInput/XInputGamepad/XInputGamepad.ino) | Xbox One S mode with rumble |
| [XInputSeriesX](examples/XInput/XInputSeriesX/XInputSeriesX.ino) | Xbox Series X mode with Share button |

## OS Compatibility

### Windows

- **Generic mode**: Recognized as a standard HID gamepad via `hid-generic`. Works in DirectInput-compatible games and any app using `hidapi`.
- **XInput mode**: Recognized natively as an Xbox controller. Works in all XInput-compatible games (virtually every modern Windows game with controller support). Shows as "Xbox Wireless Controller" in Settings > Bluetooth & devices > Controllers.
- **SInput mode**: Recognized as a HID gamepad. Works in Steam via SDL3.

### Linux

- **Generic mode**: BlueZ bridges the device into the kernel via `uhid`, creating `/dev/hidraw*`, `/dev/input/js*`, and `/dev/input/event*` nodes. Recognized by `jstest`, `evtest`, SDL, and any game using the Linux joystick or evdev subsystems.
- **XInput mode**: Works via the `xpad` kernel driver, included in most distributions. The device appears as a standard Xbox controller. Share button requires Linux 6.5+ (Series X PID).
- **SInput mode**: Recognized via SDL3's HIDAPI SInput driver (SDL 3.4+). Steam uses SDL3 and recognizes the device natively.

For detailed Linux testing (pairing, udev rules, hidapi, monitoring), see [LinuxHIDTesting.md](LinuxHIDTesting.md).

### macOS

- **Generic mode**: Recognized as a Bluetooth HID gamepad. Works via Apple's `GCController` (GameController framework) and `IOHIDManager`. Any game or emulator supporting GCController will detect it.
- **XInput mode**: macOS natively supports Xbox Wireless Controllers with Bluetooth (since macOS Big Sur 11.0). The device appears as "Xbox Wireless Controller" and works via `GCController`. Rumble is supported via GCController haptics. No driver needed.
- **SInput mode**: Recognized as a Bluetooth HID gamepad. Works via `GCController` and SDL3 (3.4+) on macOS.

| macOS Version | Xbox Controller Support |
|---|---|
| Big Sur (11.0)+ | Xbox One S via Bluetooth |
| Monterey (12.0)+ | GCController framework |
| Ventura (13.0)+ | Improved mapping |
| Sonoma (14.0)+ | Rumble via GCController haptics |
| Sequoia (15.0)+ | Wired Xbox support (USB-C) |
| Tahoe (26.0)+ | Current |

### Android

- **Generic mode only**: Works as a HID gamepad. Triggers are mapped to GAS/BRAKE instead of standard trigger axes. Right thumbstick may use z/rx instead of z/rz. See [GenericMode.md](docs/GenericMode.md#android-axis-mapping) for details.
- **SInput and XInput modes**: Not supported on Android.

### Steam (All Platforms)

Steam has built-in SDL3 support and recognizes gamepads automatically. Each mode works differently in Steam:

| Mode | Steam Recognition | What You Get |
|---|---|---|
| **Generic** | Detected as generic gamepad | Basic input; may need manual button mapping in Steam Input |
| **SInput** | Native `SDL_GameController` | Automatic mapping, rumble, player LED, IMU, touchpad -- no configuration needed |
| **XInput** | Recognized as Xbox controller | Automatic mapping on Windows/Linux; macOS via GCController |

**Recommendation**: Use **SInput mode** for Steam. It gets you native recognition with full feature support (rumble, IMU, touchpad) without per-game configuration. Steam ships with SDL3 and handles the SInput driver automatically.

If you prefer XInput mode, Steam Input maps Xbox controllers by default -- it will work, but you won't get IMU/touchpad/rumble via the SInput protocol (you'll get Xbox-style rumble instead).

## Deep Dives

- **[Generic Mode](docs/GenericMode.md)** -- Full protocol reference, HID descriptor, configuration, API
- **[SInput Mode](docs/SInputMode.md)** -- SInput protocol, SDL3 integration, touchpad, IMU, haptics, RGB
- **[XInput Mode](docs/XInputMode.md)** -- Xbox emulation protocol, rumble, PID differences
- **[GATT vs HID-over-GATT](GattVsHid.md)** -- Architecture, how SDL/game engines reach each service
- **[Linux HID Testing](LinuxHIDTesting.md)** -- Testing with hidraw/hidapi on Linux
- **[Troubleshooting Guide](TroubleshootingGuide.md)** -- Common issues and fixes

## POSSIBLE BREAKING CHANGES - PLEASE READ

A large code rebase (configuration class) along with some extra features (start, select, menu, home, back, volume up, volume down and volume mute buttons) has been committed thanks to @dexterdy

Since version 5 of this library, the axes and simulation controls have configurable min and max values. The defaults were changed from -32767 to 0 in version 5, and restored to -32768 to 32767 in version 0.8.0. Existing sketches that relied on the 0-32767 range should explicitly set `config.setAxesMin(0)` / `config.setAxesMax(32767)` if needed.

`setAxes` accepts axes in the order (x, y, z, rx, ry, rz, slider1, slider2)
`setHIDAxes` accepts them in the order (x, y, z, rz, rx, ry, slider1, slider2)

## NimBLE

Since version 3 of this library, the more efficient NimBLE library is used instead of the default BLE implementation. Please use the library manager to install it, or get it from here: https://github.com/h2zero/NimBLE-Arduino

Since version 3, this library also supports a configurable HID descriptor, which allows users to customise how the device presents itself to the OS (number of buttons, hats, axes, sliders, simulation controls etc). See the examples for guidance.

This version endeavors to be compatible with the latest released version of NimBLE-Arduino through the Arduino Library Manager.

## License

Published under the MIT license. Please see license.txt.

It would be great however if any improvements are fed back into this version.

## Troubleshooting Guide

Troubleshooting guide and suggestions can be found in [TroubleshootingGuide](TroubleshootingGuide.md)

## Testing Your Gamepad

### Cross-Platform Gamepad Testers

These free tools visualize all gamepad inputs (buttons, axes, triggers, D-pad) in real time and work on Windows, macOS, and Linux:

| Tool | Platform | Source | Notes |
|------|----------|--------|-------|
| [HIDTester](https://github.com/rhunecke/HIDTester) | Windows, macOS, Linux | [Source](https://github.com/rhunecke/HIDTester) | Lightweight, no install needed. Shows buttons, axes, D-pad, deadzone analysis, signal curves, polling rate. Built on SDL3. |
| [Gamepad_Tester](https://github.com/zoltcode/Gamepad_Tester) | Windows, macOS, Linux | [Source](https://github.com/zoltcode/Gamepad_Tester) | C++23/SDL3/ImGui. Latency measurement, polling rate analysis, rumble testing, input visualization. Pre-built binaries available. |
| [gamepad-tester.net](https://gamepad-tester.net) | Browser (all OS) | N/A | Browser-based, no install. Works in Chrome/Edge/Firefox. Shows all buttons/axes/triggers. |

Both HIDTester and Gamepad_Tester are built on SDL3, so they recognize SInput controllers natively (VID `0x2E8A`/PID `0x10C6`) and map buttons/axes correctly. The browser-based tester works with any mode but shows raw button indices instead of named buttons.

### Platform-Specific Testing

- **Linux**: See [LinuxHIDTesting.md](LinuxHIDTesting.md) for testing Input/Output/Feature Reports via hidraw/hidapi, `jstest`, and `evtest`.
- **macOS**: Gamepad appears in System Settings > Bluetooth. Use Chrome or Firefox for browser-based testing (Safari has partial Gamepad API support).
- **Windows**: Gamepad appears in Settings > Bluetooth & devices > Controllers. Use [HIDTester](https://github.com/rhunecke/HIDTester) or [Gamepad_Tester](https://github.com/zoltcode/Gamepad_Tester) for detailed input visualization.

### Hardware-in-the-Loop Testing (HIL)

[ESP32-BLE-Gamepad-HIL](https://github.com/LeeNX/ESP32-BLE-Gamepad-HIL) is a hardware-in-the-loop test rig that automates end-to-end validation of the library. An ESP32 runs test firmware; the harness drives it over USB serial and asserts the resulting BLE HID behavior on a Linux host (typically a Raspberry Pi). It tests:

- **Buttons** — every configured button produces one distinct evdev key event
- **Axes** — each axis maps to the correct ABS code with exact min/centre/max endpoints
- **Hats** — 8 directions + centre
- **HID descriptor** — golden file comparison per profile
- **Device Information / PnP / Battery** — GATT service validation
- **Feature / Output reports** — bidirectional round-trip
- **Latency / throughput** — ~18.6ms median button press, burst testing

The rig uses a builder/tester split (builder needs PlatformIO; tester needs only esptool + pytest) and runs in CI via GitHub Actions + Tailscale SSH. See the [HIL README](https://github.com/LeeNX/ESP32-BLE-Gamepad-HIL) for setup instructions.

## GATT vs HID-over-GATT

For an explanation of how this library's HID Service and NUS service differ, how SDL/game engines and the Linux input stack actually reach each one, and how to extend it with features like rumble or RGB/player LEDs, see [GattVsHid](GattVsHid.md)

## Notes

This library allows you to make the ESP32 act as a Bluetooth Gamepad and control what it does. Relies on [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)

For Windows testing, use [HIDTester](https://github.com/rhunecke/HIDTester) or [Gamepad_Tester](https://github.com/zoltcode/Gamepad_Tester) — both are cross-platform and don't require DirectX.

Gamepads designed for Android use a different button mapping. This affects analog triggers, where the standard left and right trigger axes are not detected. Android calls the HID report for right trigger `"GAS"` and left trigger `"BRAKE"`. Enabling the `"Accelerator"` and `"Brake"` simulation controls allows them to be used instead of right and left trigger.

Right thumbstick on Windows is usually z, rz, whereas on Android, this may be z, rx, so you may want to set them separately with setZ and setRX, instead of using setRightThumb(z, rz), or use setRightThumbAndroid(z, rx)

For the most consistent behavior across Windows, macOS, and Linux, use **XInput mode** (emulates an Xbox controller — universally recognized by games and OSes) or **SInput mode** (native SDL3 support with full button/axis/rumble/RGB mapping). Generic mode relies on each OS's HID stack to interpret the descriptor, so axis ordering, trigger behavior, and button naming can vary between platforms and drivers. See [XInputMode.md](docs/XInputMode.md) and [SInputMode.md](docs/SInputMode.md) for setup details.

You might also be interested in:
- [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse)
- [ESP32-BLE-Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard)
- [Composite Gamepad/Mouse/Keyboard and Xinput capable fork of this library](https://github.com/Mystfit/ESP32-BLE-CompositeHID)

or the NimBLE versions at

- [ESP32-NimBLE-Mouse](https://github.com/wakwak-koba/ESP32-NimBLE-Mouse)
- [ESP32-NimBLE-Keyboard](https://github.com/wakwak-koba/ESP32-NimBLE-Keyboard)

## Credits

Credits to [T-vK](https://github.com/T-vK) as this library is based on his ESP32-BLE-Mouse library (https://github.com/T-vK/ESP32-BLE-Mouse) that he provided.

Credits to [chegewara](https://github.com/chegewara) as the ESP32-BLE-Mouse library is based on [this piece of code](https://github.com/nkolban/esp32-snippets/issues/230#issuecomment-473135679) that he provided.

Credits to [wakwak-koba](https://github.com/wakwak-koba) for the NimBLE [code](https://github.com/wakwak-koba/ESP32-NimBLE-Gamepad) that he provided.

Credits to [LeeNX](https://github.com/LeeNX) for the initial SInput research, pull requests, and extensive help with GitHub issues. Their contributions were instrumental in driving the SInput implementation forward and keeping the project moving.

Credits to [Mystfit](https://github.com/Mystfit) for the [ESP32-BLE-CompositeHID](https://github.com/Mystfit/ESP32-BLE-CompositeHID) library, which served as a reference for the XInput implementation and Xbox HID descriptors.
