# Generic Mode

Generic HID gamepad mode. The ESP32 presents as a standard Bluetooth LE gamepad with a fully configurable HID descriptor. Works on all major operating systems without special drivers.

## Overview

This is the default mode. The library builds a HID Report Descriptor at runtime based on your `BleGamepadConfiguration` settings, producing a device that any OS recognizes as a standard gamepad/joystick via HID-over-GATT (HOGP).

Use Generic mode when:
- You need maximum configurability (buttons, axes, hats, sliders, simulation controls)
- You're targeting multiple OS platforms including Android
- You're building a custom app that reads the device via `hidraw`/`hidapi`/`evdev`
- You don't need SDL3 native recognition or Xbox compatibility

## Protocol

### HID Report Descriptor

The descriptor is built dynamically in `BleGamepad::buildGenericDescriptor()`. It uses standard USB HID usage pages:

| Usage Page | Usage | Purpose |
|-----------|-------|---------|
| 0x01 (Generic Desktop) | 0x05 (Game Pad) | Top-level collection |
| 0x01 (Generic Desktop) | 0x30 (X), 0x31 (Y), etc. | Axes |
| 0x01 (Generic Desktop) | 0x39 (Hat switch) | D-pad |
| 0x09 (Button) | 0x01-0x80 | Buttons |
| 0x01 (Generic Desktop) | 0x36 (Slider) | Sliders |
| 0x01 (Generic Desktop) | 0xB6 (Rudder), etc. | Simulation controls |

### Report IDs

All data is sent on a single Report ID (default: 3, configurable via `setHidReportId()`). The report structure is:

```
[Report ID] [Buttons...] [Axes...] [Hats...] [Sliders...] [Simulation...]
```

## Configuration

All configuration is done via `BleGamepadConfiguration` before calling `begin()`.

### Buttons

| Option | Default | Range | Description |
|--------|---------|-------|-------------|
| `setButtonCount()` | 16 | 1-128 | Number of digital buttons |
| `setWhichSpecialButtons()` | start/select enabled | 8 booleans | Start, Select, Menu, Home, Back, Volume Inc, Volume Dec, Volume Mute |

Buttons are reported as a bitmask. With 16 buttons, 2 bytes are used; with 128 buttons, 16 bytes.

### Axes

| Option | Default | Description |
|--------|---------|-------------|
| `setWhichAxes(x,y,z,rx,ry,rz,s1,s2)` | all true | Enable/disable individual axes |
| `setAxesMin()` | 0x0000 | Minimum axis value |
| `setAxesMax()` | 0x7FFF (32767) | Maximum axis value |

Default range is 0..32767 (unsigned-like). Set both to -32767..32767 for a full signed range (see `setAxesMin(-32767)`).

**Axis mapping by OS:**

| Library Axis | Windows | Linux | Android |
|-------------|---------|-------|---------|
| X | Left Thumb X | ABS_X | Left Thumb X |
| Y | Left Thumb Y | ABS_Y | Left Thumb Y |
| Z | Right Thumb X | ABS_Z | Right Thumb X |
| Rx | Left Trigger | ABS_RX | BRAKE |
| Ry | Right Trigger | ABS_RY | GAS |
| Rz | Right Thumb Y | ABS_RZ | Right Thumb Y |

Android maps triggers differently -- see [Notes](#android-axis-mapping) below.

### Hat Switches

| Option | Default | Description |
|--------|---------|-------------|
| `setHatSwitchCount()` | 1 | Number of hat switches (0-4) |

Each hat switch uses 4 bits. Values: `HAT_CENTERED`, `HAT_UP`, `HAT_UP_RIGHT`, `HAT_RIGHT`, `HAT_DOWN_RIGHT`, `HAT_DOWN`, `HAT_DOWN_LEFT`, `HAT_LEFT`, `HAT_UP_LEFT`.

### Sliders

| Option | Default | Description |
|--------|---------|-------------|
| `setWhichAxes()` | slider1=false, slider2=false | Enable via axis flags |
| `setSliderMin()`/`setSliderMax()` | 0/32767 | Slider range |

### Simulation Controls

| Option | Default | Description |
|--------|---------|-------------|
| `setWhichSimulationControls(rudder,throttle,accel,brake,steering)` | all false | Enable individual controls |

| Control | Windows Usage | Linux Usage |
|---------|--------------|-------------|
| Rudder | RUDDER | ABS_RUDDER |
| Throttle | THROTTLE | ABS_THROTTLE |
| Accelerator | GAS | ABS_GAS |
| Brake | BRAKE | ABS_BRAKE |
| Steering | WHEEL | ABS_WHEEL |

### VID/PID

| Option | Default | Description |
|--------|---------|-------------|
| `setVid()` | 0xE502 | USB Vendor ID |
| `setPid()` | 0xBBAB | USB Product ID |

Custom VID/PID values affect how the OS identifies the device. Some games use VID/PID matching for controller-specific features.

### BLE Characteristics

| Option | Default | Description |
|--------|---------|-------------|
| `setControllerType()` | 0x03 (Gamepad) | HID controller type |
| `setModelNumber()` | "1.0.0" | Device Information model |
| `setSoftwareRevision()` | "1.0.0" | Device Information software |
| `setSerialNumber()` | "0123456789" | Device Information serial |
| `setFirmwareRevision()` | "0.7.4" | Device Information firmware |
| `setHardwareRevision()` | "1.0.0" | Device Information hardware |

### Report Options

| Option | Default | Description |
|--------|---------|-------------|
| `setAutoReport()` | true | Send report automatically on state change |
| `setEnableOutputReport()` | false | Enable HID Output Report characteristic |
| `setEnableFeatureReport()` | false | Enable HID Feature Report characteristic |

### Other

| Option | Default | Description |
|--------|---------|-------------|
| `setEnableNordicUARTService()` | false | Enable NUS alongside HID |
| `setTXPowerLevel()` | 9 | BLE transmit power (-12 to 9 dBm) |

## API Reference

### Construction

```cpp
BleGamepad bleGamepad;  // default name "ESP32 BLE Gamepad", manufacturer "Espressif", battery 100
BleGamepad bleGamepad("My Gamepad", "My Mfg", 80);
```

### Lifecycle

| Method | Description |
|--------|-------------|
| `begin(BleGamepadConfiguration *config)` | Start BLE with given config (or defaults) |
| `end()` | Stop BLE, release resources |
| `isConnected()` | Returns true when a host is connected |

### Buttons

| Method | Description |
|--------|-------------|
| `press(button)` | Press a button (BUTTON_1 to BUTTON_128) |
| `release(button)` | Release a button |
| `isPressed(button)` | Check if button is currently pressed |
| `pressStart()` / `releaseStart()` | Convenience for Start button |
| `pressSelect()` / `releaseSelect()` | Convenience for Select button |
| `pressHome()` / `releaseHome()` | Convenience for Home button |
| `pressBack()` / `releaseBack()` | Convenience for Back button |
| `pressMenu()` / `releaseMenu()` | Convenience for Menu button |
| `pressVolumeInc()` / `releaseVolumeInc()` | Convenience for Volume Up |
| `pressVolumeDec()` / `releaseVolumeDec()` | Convenience for Volume Down |
| `pressVolumeMute()` / `releaseVolumeMute()` | Convenience for Mute |
| `resetButtons()` | Release all buttons |

### Axes

| Method | Description |
|--------|-------------|
| `setX(val)` | Set X axis |
| `setY(val)` | Set Y axis |
| `setZ(val)` | Set Z axis |
| `setRX(val)` | Set Rx axis |
| `setRY(val)` | Set Ry axis |
| `setRZ(val)` | Set Rz axis |
| `setSlider(val)` | Set slider 1 |
| `setSlider1(val)` | Set slider 1 |
| `setSlider2(val)` | Set slider 2 |
| `setSliders(s1, s2)` | Set both sliders |
| `setLeftThumb(x, y)` | Set left stick (X, Y) |
| `setRightThumb(z, rz)` | Set right stick (Z, Rz) |
| `setLeftTrigger(rx)` | Set left trigger |
| `setRightTrigger(ry)` | Set right trigger |
| `setTriggers(rx, ry)` | Set both triggers |
| `setAxes(x,y,z,rx,ry,rz,s1,s2)` | Set all axes at once |
| `setHIDAxes(x,y,z,rz,rx,ry,s1,s2)` | Set all axes (HID order) |

### Hat Switches

| Method | Description |
|--------|-------------|
| `setHat1(val)` | Set hat 1 (HAT_CENTERED to HAT_UP_LEFT) |
| `setHat2(val)` | Set hat 2 |
| `setHat3(val)` | Set hat 3 |
| `setHat4(val)` | Set hat 4 |
| `setHats(h1,h2,h3,h4)` | Set all hats |

### Simulation Controls

| Method | Description |
|--------|-------------|
| `setRudder(val)` | Set rudder |
| `setThrottle(val)` | Set throttle |
| `setAccelerator(val)` | Set accelerator |
| `setBrake(val)` | Set brake |
| `setSteering(val)` | Set steering |
| `setSimulationControls(r,t,a,b,s)` | Set all simulation controls |

### Motion

| Method | Description |
|--------|-------------|
| `setGyroscope(gX, gY, gZ)` | Set gyroscope (int16, -32768..32767) |
| `setAccelerometer(aX, aY, aZ)` | Set accelerometer (int16, -32768..32767) |
| `setMotionControls(gX,gY,gZ,aX,aY,aZ)` | Set both |

### Battery

| Method | Description |
|--------|-------------|
| `setBatteryLevel(level)` | Set battery percentage (0-100) |
| `setBatteryPowerInformation(state)` | Set power state (PRESENT/NOT_PRESENT/NOT_SUPPORTED) |
| `setDischargingState(state)` | Set discharging state |
| `setChargingState(state)` | Set charging state |
| `setPowerLevel(level)` | Set power level |

### Reports

| Method | Description |
|--------|-------------|
| `sendReport()` | Manually send current state to host |

### Output/Feature Reports

| Method | Description |
|--------|-------------|
| `isOutputReceived()` | Check if Output Report was received |
| `getOutputBuffer()` | Get Output Report data |
| `isFeatureReceived()` | Check if Feature Report was received |
| `getFeatureBuffer()` | Get Feature Report data |
| `setFeatureBuffer(data, len)` | Set Feature Report response |

### Pairing

| Method | Description |
|--------|-------------|
| `deleteBond()` | Delete current bond |
| `deleteAllBonds()` | Delete all bonds |
| `enterPairingMode()` | Force pairing mode |
| `getAddress()` | Get BLE address |
| `getPeerInfo()` | Get connected peer info |

## Examples

See the [examples/Generic/](../examples/Generic/) directory. Key examples:

- **Gamepad.ino** -- Minimal button/axis example
- **IndividualAxes.ino** -- Set each axis independently with `sendReport()`
- **TestAll.ino** -- Exercise all configurable features
- **FlightControllerTest.ino** -- Simulation controls (rudder, throttle, brake)
- **DrivingControllerTest.ino** -- Steering, accelerator, brake
- **MotionController.ino** -- Gyroscope and accelerometer
- **TestFeatureReports.ino** -- Bidirectional Feature Report exchange
- **TestReceivingOutputReport.ino** -- Receive host-to-device Output Reports

## Features & Limitations

### What works
- Up to 128 buttons with press/release
- 6 axes with configurable range
- Up to 4 hat switches (D-pads)
- 2 sliders
- 5 simulation controls
- Gyroscope and accelerometer
- Battery level and power state
- HID Output and Feature Reports
- Nordic UART Service (NUS) alongside HID
- Force pairing / bond management
- Configurable VID/PID and BLE characteristics
- Works in Steam (may need manual button mapping via Steam Input)

### Limitations
- No built-in rumble support (requires custom Output Report handling)
- No player LED or RGB LED support
- No SDL3 native recognition (will work as generic gamepad)
- No XInput compatibility
- Android maps triggers to GAS/BRAKE instead of standard trigger axes
- iOS is not supported (not an MFi device)

## Linux Compatibility

### How It Works on Linux

When the ESP32 pairs over BLE, BlueZ's input plugin recognizes it as a HID device and bridges it into the kernel via `uhid`. This creates:

- `/dev/hidraw*` -- raw HID access (for `hidapi`/`hidraw` apps)
- `/dev/input/js*` -- joystick device (for `jstest`, SDL, games)
- `/dev/input/event*` -- evdev device (for `evtest`, evdev-aware apps)

The device appears in `/proc/bus/input/devices` with `Icon: input-gaming`.

### Kernel Driver

Generic mode uses the `hid-generic` kernel driver, which handles any standard HID device. No custom driver is needed. The device is recognized automatically as a gamepad/joystick.

### Pairing

```bash
bluetoothctl
agent NoInputNoOutput
default-agent
scan on
# Wait for "ESP32 BLE Gamepad" to appear
pair XX:XX:XX:XX:XX:XX
trust XX:XX:XX:XX:XX:XX
connect XX:XX:XX:XX:XX:XX
```

### Permissions (udev Rule)

`/dev/hidraw*` nodes are root-only by default. Add a udev rule for user access:

```bash
# /etc/udev/rules.d/99-esp32-gamepad.rules
SUBSYSTEM=="hidraw", KERNELS=="0005:E502:BBAB.*", MODE="0660", GROUP="plugdev"
```

Replace `E502:BBAB` if you changed the VID/PID. Then:
```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo gpasswd -a "$USER" plugdev
# Log out and back in for group changes to take effect
```

### Finding Your hidraw Node

```bash
ls /sys/bus/hid/devices/ | grep -i e502
# 0005:E502:BBAB.0003
ls -la /dev/hidraw*
```

### Testing

```bash
# Quick sanity check -- shows buttons/axes in real time
jstest --normal /dev/input/js0

# Lower-level evdev view
evtest /dev/input/event0

# Monitor Bluetooth traffic
sudo btmon -i hci0
```

### Battery Monitoring

Battery level appears in UPower automatically:
```bash
upower -e | grep gaming_input
upower -i /org/freedesktop/UPower/devices/gaming_input_dev_XX_XX_XX_XX_XX_XX
```

### Python hidapi Access

```python
import hid
for d in hid.enumerate():
    if d["product_string"] == "ESP32 BLE Gamepad":
        print(f"Path: {d['path']}")
```

### Troubleshooting

- **Device not found by `hidenumerate()`**: Confirm paired with `bluetoothctl info`, check `/dev/hidraw*` exists
- **`PermissionError` on hidraw**: Use `sudo` or set up the udev rule above
- **Pairing hangs**: Set `agent NoInputNoOutput` before pairing
- **GATT client can't see HID characteristics**: Expected -- BlueZ claims the HID service; use `hidraw`/`hidapi` instead

See [LinuxHIDTesting.md](../LinuxHIDTesting.md) for the full testing walkthrough.

## macOS Compatibility

### How It Works on macOS

macOS recognizes Generic mode as a standard Bluetooth HID gamepad via HID-over-GATT (HOGP). The device appears in System Settings > Bluetooth and is accessible via Apple's `GCController` (GameController framework) and `IOHIDManager`.

### Pairing

1. Open System Settings > Bluetooth
2. Put the ESP32 into pairing mode (call `begin()` in your sketch)
3. Click "Connect" next to the device name
4. The device appears as a gamepad in any app that supports controllers

Or via command line:
```bash
blueutil --pair XX:XX:XX:XX:XX:XX
```

### HID API Access (hidapi)

On macOS, the `hidapi` library uses `IOHIDManager` as its backend. The device can be opened directly:

```python
import hid
for d in hid.enumerate():
    if d["product_string"] == "ESP32 BLE Gamepad":
        dev = hid.Device(path=d["path"])
        # read/write reports...
        dev.close()
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
        // Map buttons, axes, etc.
    }
}
```

### Battery Level

Battery level is not automatically surfaced to macOS system tools (unlike Linux's `upower`). You must read it via the Battery Service characteristic using `hidapi` or equivalent.

### Troubleshooting

- **Device doesn't appear in Bluetooth settings**: Confirm sketch is running `begin()`, wait for BLE scan
- **`hid.enumerate()` doesn't find it**: Use `blueutil --info XX:XX:XX:XX:XX:XX` to confirm pairing
- **Controller detected but buttons/axes wrong**: Check your `BleGamepadConfiguration` matches what the game expects
- **Multiple HID devices conflict**: macOS may grab the wrong `IOHIDManager` device; set a unique `setDeviceName()`
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

# Monitor
sudo btmon -i hci0

# Battery
upower -e | grep gaming_input
```

## Android Axis Mapping

Android maps gamepad axes differently than Windows/Linux:

| Function | Windows/Linux | Android |
|----------|--------------|---------|
| Left Trigger | Rx axis | GAS (simulation) |
| Right Trigger | Ry axis | BRAKE (simulation) |
| Right Stick X | Z axis | Z axis |
| Right Stick Y | Rz axis | Rx axis |

To work around this on Android, enable the Accelerator and Brake simulation controls:
```cpp
config.setWhichSimulationControls(false, false, true, true, false);
```

Then use `setAccelerator()` for right trigger and `setBrake()` for left trigger on Android.

For right thumbstick, use `setZ()` and `setRX()` instead of `setRightThumb()`, or use `setRightThumbAndroid(z, rx)`.

## References

- [USB HID Usage Tables](https://usb.org/document-library/hid-usage-tables-14)
- [USB HID Specification 1.11](https://usb.org/document-library/hid-111)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
- [ESP32 BLE Arduino](https://github.com/espressif/arduino-esp32)
