# SInput Mode

SInput is a community protocol for BLE gamepads that provides native recognition by SDL3 and Steam. The ESP32 presents with a fixed VID/PID and a standardized report layout that SDL's `hidapi` driver understands natively, turning it into a full `SDL_GameController` with rumble, player LED, RGB, IMU, and touchpad support.

## Overview

SInput was created by [HandHeldLegend](https://github.com/HandHeldLegend) to solve the problem of BLE gamepads not being recognized by games without per-device driver support. Instead of each device needing its own kernel driver or SDL driver, SInput defines a fixed report layout that a single SDL driver handles.

Use SInput mode when:
- You're building a gamepad for use with SDL3 games or Steam
- You want native `SDL_GameController` recognition without custom drivers
- You need rumble, player LED, RGB, IMU, or touchpad support
- You're targeting Linux, Windows, or macOS with SDL3

### References

- [SInput Protocol Spec](https://docs.handheldlegend.com/s/sinput)
- [HandHeldLegend/SInput-HID](https://github.com/HandHeldLegend/SInput-HID)
- [SINPUT-LIB-HID](https://github.com/HandHeldLegend/SINPUT-LIB-HID) -- reference firmware library
- [SDL3 SInput Driver](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_sinput.c)
- [SDL PR #13343](https://github.com/libsdl-org/SDL/pull/13343) -- initial SInput support

## Protocol

SInput uses three HID Report IDs, all within the standard HID-over-GATT transport:

| Report ID | Direction | Purpose |
|-----------|-----------|---------|
| 0x01 | Device -> Host | Regular gamepad input state |
| 0x02 | Device -> Host | Command/feature response (reuses Input report type) |
| 0x03 | Host -> Device | Output commands (haptic, features, LED, RGB) |

### Fixed VID/PID

SInput mode automatically sets VID to `0x2E8A` and PID to `0x10C6`. The host OS identifies Bluetooth devices by their VID/PID and loads the appropriate driver. SDL's SInput `hidapi` driver hardcodes this exact VID/PID pair in its allowlist — if you change it, SDL won't recognize the device and you'll lose native `SDL_GameController` support.

**Do not call `setVid()`/`setPid()` after `setGamepadMode(GamepadMode::SInput)`** — the VID/PID is intentionally fixed. If you've previously paired with a different VID/PID, remove the old bond first (`bluetoothctl remove <address>`).

## Input Report 0x01 -- Gamepad State

63-byte payload (Report ID stripped by the BLE HID stack). Sent at the configured polling rate.

### Byte Layout

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | uint8 | Plug status | 0=unknown, 1=no battery, 2=charging, 3=charged, 4=on battery |
| 1 | uint8 | Charge level | 0-100% |
| 2 | uint8 | Buttons 0 | South, East, West, North, DUp, DDown, DLeft, DRight |
| 3 | uint8 | Buttons 1 | StickL, StickR, LShoulder, RShoulder, LTrigger, RTrigger, LPaddle1, RPaddle1 |
| 4 | uint8 | Buttons 2 | Start, Back, Guide, Capture, LPaddle2, RPaddle2, TouchpadL, TouchpadR |
| 5 | uint8 | Buttons 3 | Power, Misc1-7, reserved |
| 6-7 | int16 | Left Stick X | -32768..32767 |
| 8-9 | int16 | Left Stick Y | -32768..32767 |
| 10-11 | int16 | Right Stick X | -32768..32767 |
| 12-13 | int16 | Right Stick Y | -32768..32767 |
| 14-15 | int16 | Left Trigger | -32768..32767 |
| 16-17 | int16 | Right Trigger | -32768..32767 |
| 18-21 | uint32 | IMU Timestamp | Microseconds since boot |
| 22-23 | int16 | Accel X | +/-8g (configurable range) |
| 24-25 | int16 | Accel Y | |
| 26-27 | int16 | Accel Z | |
| 28-29 | int16 | Gyro X | +/-2000dps (configurable range) |
| 30-31 | int16 | Gyro Y | |
| 32-33 | int16 | Gyro Z | |
| 34 | uint8 | (reserved) | |
| 35-36 | int16 | Touch 1 X | -32768..32767 |
| 37-38 | int16 | Touch 1 Y | |
| 39-40 | uint16 | Touch 1 Pressure | 0..32767 |
| 41-42 | int16 | Touch 2 X | |
| 43-44 | int16 | Touch 2 Y | |
| 45-46 | uint16 | Touch 2 Pressure | |
| 47-62 | | (reserved/serial) | MAC address bytes 18-23, zeroed by this library |

### Button Bitmasks

**Buttons 0 (byte 2):**

| Bit | Button |
|-----|--------|
| 0 | South (A/Cross) |
| 1 | East (B/Circle) |
| 2 | West (X/Square) |
| 3 | North (Y/Triangle) |
| 4 | D-pad Up |
| 5 | D-pad Down |
| 6 | D-pad Left |
| 7 | D-pad Right |

**Buttons 1 (byte 3):**

| Bit | Button |
|-----|--------|
| 0 | Left Stick Click |
| 1 | Right Stick Click |
| 2 | Left Bumper |
| 3 | Right Bumper |
| 4 | Left Trigger (digital) |
| 5 | Right Trigger (digital) |
| 6 | Left Paddle 1 |
| 7 | Right Paddle 1 |

**Buttons 2 (byte 4):**

| Bit | Button |
|-----|--------|
| 0 | Start |
| 1 | Back |
| 2 | Guide/Home |
| 3 | Capture/Share |
| 4 | Left Paddle 2 |
| 5 | Right Paddle 2 |
| 6 | Touchpad 1 (click) |
| 7 | Touchpad 2 (click) |

## Feature Response Report 0x02

Sent in response to a "get features" command (0x02 on Output Report 0x03). Describes device capabilities.

### Byte Layout

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | uint8 | Command echo | 0x02 (features command ID) |
| 1-2 | uint16 | Protocol version | LE, currently 1 |
| 3 | uint8 | Capabilities 0 | See bitmask below |
| 4 | uint8 | Capabilities 1 | See bitmask below |
| 5 | uint8 | Gamepad type | SDL_GamepadType enum value |
| 6 | uint8 | Face style | Bits 7-5: face style, Bits 4-0: sub-product |
| 7-8 | uint16 | Polling rate (us) | LE, microseconds between reports |
| 9-10 | uint16 | Accel range | LE, +/- g (e.g. 8) |
| 11-12 | uint16 | Gyro range | LE, +/- dps (e.g. 2000) |
| 13 | uint8 | Usage mask 0 | Button usage bits 0-7 |
| 14 | uint8 | Usage mask 1 | Button usage bits 8-15 |
| 15 | uint8 | Usage mask 2 | Button usage bits 16-23 |
| 16 | uint8 | Usage mask 3 | Button usage bits 24-31 |
| 17 | uint8 | Touchpad count | 0, 1, or 2 |
| 18 | uint8 | Touchpad finger count | 1 or 2 per touchpad |

### Capability Bitmask 0 (byte 3)

| Bit | Capability |
|-----|-----------|
| 0 | Rumble |
| 1 | Player LED |
| 2 | Accelerometer |
| 3 | Gyroscope |
| 4 | Left analog stick |
| 5 | Right analog stick |
| 6 | Left analog trigger |
| 7 | Right analog trigger |

### Capability Bitmask 1 (byte 4)

| Bit | Capability |
|-----|-----------|
| 0 | Touchpad |
| 1 | Joystick RGB |
| 2 | Handheld mode |

### Gamepad Type Values

| Value | Type |
|-------|------|
| 0 | Unknown |
| 1 | Standard |
| 2 | Xbox 360 |
| 3 | Xbox One |
| 4 | PS3 |
| 5 | PS4 |
| 6 | PS5 |
| 7 | Nintendo Pro |
| 8 | Joy-Con Left |
| 9 | Joy-Con Right |
| 10 | Joy-Con Pair |
| 11 | GameCube |
| 12 | Steam |

## Output Report 0x03 -- Host Commands

47-byte payload from host to device. Byte 0 is the command ID.

### Command Table

| Command | Value | Payload | Description |
|---------|-------|---------|-------------|
| Haptic | 0x01 | See below | Rumble/vibration |
| Features | 0x02 | (empty) | Request feature response on next input report |
| Player LED | 0x03 | uint8 index | Set player LED (1-based, 0 = off) |
| Joystick RGB | 0x04 | R, G, B (3 bytes) | Set RGB LED color |

### Haptic Command (Type 2 = ERM Simulation)

| Offset | Size | Field |
|--------|------|-------|
| 0 | uint8 | Command ID (0x01) |
| 1 | uint8 | Type (0x02 = ERM) |
| 2 | uint8 | Left motor amplitude (0-255) |
| 3 | uint8 | Left motor brake (0/1) |
| 4 | uint8 | Right motor amplitude (0-255) |
| 5 | uint8 | Right motor brake (0/1) |

## Configuration

### Enabling SInput Mode

```cpp
BleGamepadConfiguration config;
config.setGamepadMode(GamepadMode::SInput);
```

This automatically:
- Sets VID/PID to 0x2E8A/0x10C6
- Sets button count to 6 (face + shoulders)
- Sets hat switch count to 1
- Enables rumble
- Disables output/feature reports (SInput owns those Report IDs)
- Enables touchpad (1 pad, 2 fingers)

### Configuration Options

| Option | Default (SInput) | Description |
|--------|-------------------|-------------|
| `setEnableRumble()` | true | Enable haptic/rumble reception |
| `setEnableSInputIMU()` | false | Enable gyroscope + accelerometer |
| `setEnableSInputRGB()` | false | Enable RGB LED command reception |
| `setEnableTouchpad()` | true | Enable touchpad data in reports |
| `setTouchpadCount()` | 1 | Number of touchpads (0-2) |
| `setTouchpadFingerCount()` | 2 | Fingers per touchpad (1-2) |
| `setSInputGamepadType()` | 1 (Standard) | SDL gamepad type hint |
| `setSInputFaceStyle()` | 1 (ABXY) | Face button layout hint |
| `setButtonCount()` | 6 | Number of face/shoulder buttons |
| `setHatSwitchCount()` | 1 | D-pad |

## API Reference

### Input Methods

All standard `BleGamepad` input methods work in SInput mode. The library maps them to SInput's fixed byte positions:

| Method | SInput Mapping |
|--------|---------------|
| `press(BUTTON_1..6)` | Buttons 0-5 (South/East/West/North/LB/RB) |
| `setLeftThumb(x, y)` | Left stick X/Y |
| `setRightThumb(z, rz)` | Right stick X/Y |
| `setLeftTrigger(rx)` | Left trigger |
| `setRightTrigger(ry)` | Right trigger |
| `setHat1(val)` | D-pad |
| `pressStart()` | Start button |
| `pressBack()` / `pressSelect()` | Back button |
| `pressHome()` | Guide button |
| `setGyroscope(gX, gY, gZ)` | Gyro data (when IMU enabled) |
| `setAccelerometer(aX, aY, aZ)` | Accel data (when IMU enabled) |
| `setTouchpad(pad, x, y, pressure)` | Touchpad data (when touchpad enabled) |

### Output/Feedback Methods

| Method | Description |
|--------|-------------|
| `isRumbleReceived()` | Check if haptic command arrived |
| `getRumbleLeftAmplitude()` | Left motor amplitude (0-255) |
| `getRumbleRightAmplitude()` | Right motor amplitude (0-255) |
| `isPlayerLedReceived()` | Check if player LED command arrived |
| `getPlayerLedIndex()` | Player LED index (1-based, 0 = off) |
| `isRgbReceived()` | Check if RGB command arrived |
| `getRgbRed()` / `getRgbGreen()` / `getRgbBlue()` | RGB color values |

### Touchpad API

```cpp
void setTouchpad(uint8_t pad, int16_t x, int16_t y, uint16_t pressure);
```

- `pad`: 0 = left/touch1, 1 = right/touch2
- `x`, `y`: Signed 16-bit (-32768..32767). SDL normalizes to 0.0-1.0.
- `pressure`: Unsigned (0..32767). SDL normalizes to 0.0-1.0.

## Touchpad

SInput supports up to 2 touchpads, each with 1 finger (or 1 touchpad with 2 fingers).

### Coordinate System

- X: -32768 (left) to 32767 (right)
- Y: -32768 (top) to 32767 (bottom)
- Pressure: 0 (no touch) to 32767 (maximum)

### SDL Normalization

SDL3 converts raw values to normalized coordinates:
- `normalized_x = raw_x / 65536.0 + 0.5`
- `normalized_y = raw_y / 65536.0 + 0.5`
- `normalized_pressure = raw_pressure / 32768.0`

### Configuration

```cpp
config.setEnableTouchpad(true);       // default true in SInput mode
config.setTouchpadCount(1);           // 0, 1, or 2 touchpads
config.setTouchpadFingerCount(2);     // fingers per touchpad (1 or 2)
```

## IMU (Gyroscope + Accelerometer)

When enabled, IMU data is packed into the input report at bytes 18-33.

### Timestamp

A 32-bit microsecond timestamp at bytes 18-21. SDL uses this for sensor fusion. The library auto-generates this from `millis() * 1000`.

### Scaling

| Sensor | Default Range | Scale Factor |
|--------|--------------|-------------|
| Accelerometer | +/-8g | `9.81 / (32768 / range)` m/s^2 |
| Gyroscope | +/-2000 dps | `(pi/180) / (32768 / range)` rad/s |

### Configuration

```cpp
config.setEnableSInputIMU(true);
```

### API

```cpp
bleGamepad.setGyroscope(gX, gY, gZ);       // int16, -32768..32767
bleGamepad.setAccelerometer(aX, aY, aZ);   // int16, -32768..32767
```

## Player LED

The host sends a player index (1-based) via Output Report 0x03, command 0x03.

```cpp
if (bleGamepad.isPlayerLedReceived()) {
    uint8_t playerIndex = bleGamepad.getPlayerLedIndex(); // 1 = Player 1, 0 = off
    digitalWrite(LED_BUILTIN, playerIndex == 1 ? HIGH : LOW);
}
```

## RGB LED

The host sends an RGB color via Output Report 0x03, command 0x04.

```cpp
if (bleGamepad.isRgbReceived()) {
    uint8_t r = bleGamepad.getRgbRed();
    uint8_t g = bleGamepad.getRgbGreen();
    uint8_t b = bleGamepad.getRgbBlue();
    // Drive your RGB LED here
}
```

## Rumble / Haptics

The host sends ERM-style rumble via Output Report 0x03, command 0x01, type 2.

```cpp
if (bleGamepad.isRumbleReceived()) {
    uint8_t weakMotor = bleGamepad.getRumbleLeftAmplitude();    // 0-255
    uint8_t strongMotor = bleGamepad.getRumbleRightAmplitude(); // 0-255
    analogWrite(WEAK_MOTOR_PIN, weakMotor);
    analogWrite(STRONG_MOTOR_PIN, strongMotor);
}
```

## Battery / Power State

SInput has its own power state reporting in the input report (bytes 0-1), independent of the standard BLE Battery Service.

| Plug Status | Value | Meaning |
|-------------|-------|---------|
| Unknown | 0 | Not reported |
| No Battery | 1 | External power, no battery |
| Charging | 2 | Charging |
| Charged | 3 | Charge complete |
| On Battery | 4 | Running on battery |

```cpp
bleGamepad.setBatteryPowerInformation(POWER_STATE_PRESENT);
bleGamepad.setDischargingState(POWER_STATE_DISCHARGING);
bleGamepad.setBatteryLevel(75); // also updates standard Battery Service
```

## SDL3 Integration

### Requirements

- SDL 3.4.x or newer (SInput driver landed via [PR #13343](https://github.com/libsdl-org/SDL/pull/13343))
- `SDL_JOYSTICK_HIDAPI_SINPUT=1` environment variable (should be on by default)

### Building SDL3 from Source (if needed)

```bash
git clone --branch release-3.4.14 --depth 1 https://github.com/libsdl-org/SDL.git ~/src/SDL3
cmake -S ~/src/SDL3 -B ~/src/SDL3/build -DCMAKE_BUILD_TYPE=Release
cmake --build ~/src/SDL3/build -j$(nproc)
sudo cmake --install ~/src/SDL3/build
sudo ldconfig
```

### Testing

See [examples/SInput/SInputPlayerLED/host_test/SDL3Testing.md](../examples/SInput/SInputPlayerLED/host_test/SDL3Testing.md) for a complete testing walkthrough with a ready-to-run SDL3 test program.

### Linux Pairing

```bash
bluetoothctl
scan on
# find the ESP32 (name "ESP32 BLE Gamepad", VID 0x2E8A)
pair <MAC>
trust <MAC>
connect <MAC>
```

If re-pairing after a firmware change, remove the old bond first:
```bash
bluetoothctl remove <MAC>
```

If pairing fails with `AuthenticationFailed`, fully erase the ESP32's flash before reflashing:
```bash
esptool.py --port <port> erase_flash
```

## macOS Compatibility

### How It Works on macOS

macOS recognizes SInput mode as a Bluetooth HID gamepad via HID-over-GATT (HOGP). Since SInput uses standard HID Input/Output/Feature Reports, macOS handles it like any other BLE gamepad. The device appears in System Settings > Bluetooth and is accessible via Apple's `GCController` framework.

### Pairing

1. Open System Settings > Bluetooth
2. Put the ESP32 into pairing mode (call `begin()` with `GamepadMode::SInput`)
3. Click "Connect" next to the device name
4. The device appears as a gamepad in any app that supports controllers

### HID API Access (hidapi)

On macOS, the `hidapi` library uses `IOHIDManager` as its backend. Feature Reports are accessible:

```python
import hid

dev = hid.Device(path="/dev/hidraw0")
# Feature Report 1: capability query
caps = dev.get_feature_report(1, 64)
print("Capabilities:", list(caps))

# Feature Report 5: haptic/rumble (63 bytes)
rumble = bytes([5] + [0x00]*62)
dev.send_feature_report(rumble)
dev.close()
```

**Note**: macOS's `hidapi` backend strips the leading Report ID byte from `get_feature_report()` return values (unlike Linux which includes it). Handle both if writing cross-platform code:

```python
data = list(result)[1:] if list(result)[:1] == [REPORT_ID] else list(result)
```

### GameController Framework (Swift/Objective-C)

The device is accessible via `GCController`:
```swift
import GameController

NotificationCenter.default.addObserver(
    forName: .GCControllerDidConnect, object: nil, queue: nil
) { notification in
    if let controller = notification.object as? GCController {
        print("Connected: \(controller.vendorName ?? "Unknown")")
        // Map buttons, axes, triggers, etc.
    }
}
```

### SDL3 Integration on macOS

SDL3's SInput driver works on macOS. Build SDL3 from source:

```bash
git clone --branch release-3.4.14 --depth 1 https://github.com/libsdl-org/SDL.git ~/src/SDL3
cmake -S ~/src/SDL3 -B ~/src/SDL3/build -DCMAKE_BUILD_TYPE=Release
cmake --build ~/src/SDL3/build -j$(sysctl -n hw.ncpu)
sudo cmake --install ~/src/SDL3/build
```

### Battery Level

Battery level is not automatically surfaced to macOS system tools. You must read it via the Battery Service characteristic using `hidapi` or equivalent.

### Troubleshooting

- **Device doesn't appear in Bluetooth settings**: Confirm sketch is running `begin()` with `GamepadMode::SInput`, wait for BLE scan
- **`hid.enumerate()` doesn't find it**: Use `blueutil --info XX:XX:XX:XX:XX:XX` to confirm pairing
- **Controller detected but buttons/axes wrong**: Check your `BleGamepadConfiguration` matches what the game expects
- **Feature Report reads return wrong size**: macOS strips the leading Report ID byte (see note above)
- **Connection drops intermittently**: Ensure the ESP32 has adequate power; BLE on macOS can be sensitive to signal strength

## Linux Testing (Quick Reference)

For detailed Linux testing see [LinuxHIDTesting.md](../LinuxHIDTesting.md). Key commands:

```bash
# Pair
bluetoothctl
agent NoInputNoOutput
default-agent
scan on
pair XX:XX:XX:XX:XX:XX
trust XX:XX:XX:XX:XX:XX
connect XX:XX:XX:XX:XX:XX

# Test input
jstest --normal /dev/input/js0
evtest /dev/input/event0

# Feature Reports (rumble)
sudo .venv/bin/python -c "
import hid
dev = hid.Device(path='/dev/hidraw0')
rumble = bytes([5] + [0x00]*62)
dev.send_feature_report(rumble)
dev.close()
"

# Monitor
sudo btmon -i hci0

# Battery
upower -e | grep gaming_input
```

## Steam Integration

### How Steam Recognizes SInput

Steam ships with SDL3 and includes the SInput HIDAPI driver. When your ESP32 connects in SInput mode, Steam automatically:

1. Detects the device via its VID/PID (`0x2E8A:0x10C6`)
2. Loads the SInput driver
3. Maps it as a `SDL_GameController`
4. Enables rumble, player LED, IMU, and touchpad passthrough

No configuration is needed. Steam Input handles the mapping for all games.

### Steam Input Configuration

In Steam's controller settings (Settings > Controller):

- The device appears as "Steam Controller" or "ESP32 BLE Gamepad"
- Button mapping is automatic for most games
- Rumble is forwarded to the ESP32's haptic motors
- IMU data is available to games that support gyro aiming
- Touchpad can be used as a mouse or for custom mappings

### Per-Game Configuration

For games that need custom mappings:

1. Right-click the game in Steam Library > Properties > Controller
2. Override the per-game setting to "Enable Steam Input"
3. Use the controller configurator to remap buttons/axes

### Troubleshooting in Steam

- **Device not recognized**: Ensure you're running SInput mode (`GamepadMode::SInput`), not Generic
- **Rumble not working**: Check `setEnableRumble(true)` in your sketch
- **IMU not detected**: Some games need "Enable Gyro" in Steam Input settings
- **Steam shows "Generic Controller"**: Remove the device from Steam's controller list and re-pair

## Examples

See the [examples/SInput/](../examples/SInput/) directory:

- **SInputPlayerLED.ino** -- Rumble + player LED + battery ramp
- **SInputPlayerLED_RGB.ino** -- Rumble + player LED + RGB LED
- **SInputPlayerLED_NeoPixel.ino** -- Rumble + player LED + NeoPixel strip
- **SInputFullGamepad.ino** -- All inputs (6 buttons, 2 sticks, 2 triggers, D-pad, Start/Back/Home)
- **SInputIMU.ino** -- Gyroscope + accelerometer with simulated data
- **SInputTouchpad.ino** -- Dual touchpad with simulated circular touch

## Features & Limitations

### What works
- Native SDL3 `SDL_GameController` recognition
- Buttons, sticks, triggers, D-pad
- Gyroscope and accelerometer
- Dual touchpad (1-2 pads, 1-2 fingers)
- ERM-style rumble/vibration
- Player LED (1-based index)
- RGB LED (24-bit color)
- Battery/power state reporting
- Works on Linux, Windows, macOS with SDL3 3.4+

### Limitations
- Fixed VID/PID (0x2E8A/0x10C6) -- cannot be customized
- Fixed report layout -- not configurable like Generic mode
- Maximum 6 face/shoulder buttons (plus Start/Back/Guide)
- Only 1 hat switch
- No sliders or simulation controls
- No iOS support
- Requires SDL 3.4.x+ -- older SDL versions won't recognize the device
- Rumble is ERM-style only (no HD/rumble2 support)
- No per-gamepad calibration in the protocol

## Comparison with SINPUT-LIB-HID

The reference [SINPUT-LIB-HID](https://github.com/HandHeldLegend/SINPUT-LIB-HID) library provides the same protocol in a portable C library for firmware authors. Key differences:

| Aspect | This Library | SINPUT-LIB-HID |
|--------|-------------|----------------|
| Platform | ESP32 + NimBLE | Any MCU, any RTOS |
| Integration | Arduino library | CMake static library |
| Touchpad | Yes (implemented) | Hook-based (you implement) |
| IMU | Yes (implemented) | Hook-based |
| Rumble | ERM type 2 | Both HD and ERM hooks |
| RGB | Yes (implemented) | Hook-based |
| Player LED | Yes (implemented) | Hook-based |

Both produce identical wire-format reports. The choice depends on your platform and whether you want Arduino convenience or bare-metal portability.
