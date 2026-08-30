# XInput Mode

XInput mode emulates an Xbox One S or Xbox Series X controller over BLE. Windows recognizes it natively as an XInput device, giving broad compatibility with games that use XInput or DirectInput.

## Overview

XInput is Microsoft's API for Xbox controller input on Windows. By emulating the Xbox HID protocol, the ESP32 appears as a genuine Xbox controller to the OS. This library supports two variants:

- **Xbox One S** (PID 0x02FD) -- Default. Broad Linux compatibility via the `xpad` kernel driver.
- **Xbox Series X** (PID 0x0B13) -- Adds Share button support. Requires Linux 6.5+ for full compatibility.

Use XInput mode when:
- You're targeting Windows games that use XInput/DirectInput
- You want maximum compatibility with Xbox-oriented games
- You need strong/weak rumble motors
- You don't need IMU, touchpad, or player LED

### References

- [Mystfit/ESP32-BLE-CompositeHID](https://github.com/Mystfit/ESP32-BLE-CompositeHID) -- reference for Xbox HID descriptors and XInput implementation
- [Xbox BLE HID Descriptor](BleXInputDescriptors.h) -- the descriptor this library uses
- [Linux xpad driver](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) -- kernel driver for Xbox controllers
- [XUSB22 HID Descriptor](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/xusb22-hid-sample-descriptor)

## Protocol

XInput mode uses two Report IDs:

| Report ID | Direction | Size | Purpose |
|-----------|-----------|------|---------|
| 0x01 | Device -> Host | 18 bytes | Gamepad input state |
| 0x03 | Host -> Device | 8 bytes | Rumble/vibration output |

### Input Report 0x01 -- Gamepad State

18-byte payload. All multi-byte fields are little-endian.

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | uint16 | Buttons | Bitmask (see below) |
| 2 | uint16 | Left Stick X | 0-65535, center = 32768 |
| 4 | uint16 | Left Stick Y | 0-65535, center = 32768 |
| 6 | uint16 | Right Stick X | 0-65535, center = 32768 |
| 8 | uint16 | Right Stick Y | 0-65535, center = 32768 |
| 10 | uint16 | Left Trigger | 0-1023 |
| 12 | uint16 | Right Trigger | 0-1023 |
| 14 | uint8 | Share button | Bit 4 (Series X only) |
| 15-17 | | (padding) | |

### Button Bitmask (bytes 0-1)

| Bit | Button |
|-----|--------|
| 0 | A |
| 1 | B |
| 2 | X |
| 3 | Y |
| 4 | Left Bumper |
| 5 | Right Bumper |
| 6 | Back/Select |
| 7 | Start |
| 8 | Left Stick Click |
| 9 | Right Stick Click |
| 10 | Guide/Home |

### Axis Encoding

**Sticks (16-bit unsigned, 0-65535):**
- Center: 32768
- Full left/down: 0
- Full right/up: 65535

The library converts from signed int16 (-32767..32767) to unsigned by adding the center offset:
```cpp
uint16_t raw = (uint16_t)((int32_t)signedValue + 32768);
```

**Triggers (10-bit unsigned, 0-1023):**
- Released: 0
- Fully pressed: 1023

### D-pad

The D-pad is encoded as part of the hat switch, mapped to the hat1 value in the library.

## Output Report 0x03 -- Rumble

8-byte payload from host to device.

| Offset | Size | Field |
|--------|------|-------|
| 0 | uint8 | Report ID (0x03) |
| 1 | uint8 | Padding (0x00) |
| 2 | uint8 | Left motor (weak) amplitude |
| 3 | uint8 | Right motor (strong) amplitude |
| 4 | uint8 | Left trigger magnitude |
| 5 | uint8 | Right trigger magnitude |
| 6-7 | | (padding) |

## Configuration

### Enabling XInput Mode

```cpp
BleGamepadConfiguration config;
config.setGamepadMode(GamepadMode::XInput);           // Xbox One S (PID 0x02FD)
// OR
config.setGamepadMode(GamepadMode::XInputSeriesX);    // Xbox Series X (PID 0x0B13)
```

This automatically:
- Sets VID to 0x045E (Microsoft)
- Sets PID to 0x02FD (One S) or 0x0B13 (Series X)
- Sets button count to 11
- Sets hat switch count to 1
- Disables sliders, simulation controls, gyro, accelerometer
- Disables output/feature reports (XInput owns those Report IDs)

### PID Differences

| Mode | PID | Share Button | Linux Support |
|------|-----|:------------:|---------------|
| `GamepadMode::XInput` | 0x02FD | No | `xpad` driver, broad compatibility |
| `GamepadMode::XInputSeriesX` | 0x0B13 | Yes (Button 11) | Linux 6.5+ for full support |

### Configuration Options

| Option | Default (XInput) | Description |
|--------|-------------------|-------------|
| `setButtonCount()` | 11 | Number of buttons (1-11) |
| `setHatSwitchCount()` | 1 | D-pad |
| `setWhichAxes(x,y,z,rx,ry,rz,s1,s2)` | sticks + triggers | Which axes to report |

## API Reference

### Input Methods

| Method | XInput Mapping |
|--------|---------------|
| `press(BUTTON_1)` | A |
| `press(BUTTON_2)` | B |
| `press(BUTTON_3)` | X |
| `press(BUTTON_4)` | Y |
| `press(BUTTON_5)` | Left Bumper |
| `press(BUTTON_6)` | Right Bumper |
| `press(BUTTON_7)` | Back/Select |
| `press(BUTTON_8)` | Start |
| `press(BUTTON_9)` | Left Stick Click |
| `press(BUTTON_10)` | Right Stick Click |
| `press(BUTTON_11)` | Share (Series X only) |
| `setLeftThumb(x, y)` | Left stick |
| `setRightThumb(z, rz)` | Right stick |
| `setLeftTrigger(rx)` | Left trigger |
| `setRightTrigger(ry)` | Right trigger |
| `setHat1(val)` | D-pad |

### Rumble

| Method | Description |
|--------|-------------|
| `isXInputRumbleReceived()` | Check if rumble command arrived |
| `getXInputStrongMotor()` | Right/strong motor amplitude (0-255) |
| `getXInputWeakMotor()` | Left/weak motor amplitude (0-255) |
| `getXInputLeftTriggerMagnitude()` | Left trigger vibration (0-255) |
| `getXInputRightTriggerMagnitude()` | Right trigger vibration (0-255) |

### Rumble Example

```cpp
if (bleGamepad.isXInputRumbleReceived()) {
    uint8_t strong = bleGamepad.getXInputStrongMotor();
    uint8_t weak = bleGamepad.getXInputWeakMotor();
    uint8_t leftTrig = bleGamepad.getXInputLeftTriggerMagnitude();
    uint8_t rightTrig = bleGamepad.getXInputRightTriggerMagnitude();

    Serial.printf("Rumble: strong=%d weak=%d L_trig=%d R_trig=%d\n",
                  strong, weak, leftTrig, rightTrig);

    // Drive motors:
    // analogWrite(STRONG_MOTOR_PIN, strong);
    // analogWrite(WEAK_MOTOR_PIN, weak);
}
```

## HID Descriptor

The XInput HID descriptor is defined in [BleXInputDescriptors.h](../BleXInputDescriptors.h). It follows the Xbox BLE HID profile with four Report IDs:

| Report ID | Type | Purpose |
|-----------|------|---------|
| 0x01 | Input | Gamepad state (buttons, axes, triggers) |
| 0x02 | Input |>battery/vendor (not used by this library) |
| 0x03 | Output | Rumble/vibration |
| 0x04 | Feature | Authentication (not used by this library) |

Only Report IDs 0x01 (input) and 0x03 (output/rumble) are actively used.

## Linux Compatibility

### How It Works on Linux

The `xpad` kernel driver handles Xbox controllers connected via Bluetooth. When the ESP32 pairs, BlueZ bridges the HID service into the kernel via `uhid`, and `xpad` recognizes the Xbox VID/PID. The controller appears as `/dev/input/js*` (joystick) and `/dev/input/event*` (evdev).

### Xbox One S (PID 0x02FD)

Works with the `xpad` kernel driver, which is included in most Linux distributions. Broad compatibility across kernel versions. Steam on Linux recognizes it as an Xbox controller via SDL and handles mapping automatically.

### Xbox Series X (PID 0x0B13)

Requires Linux 6.5+ for the Share button to be recognized. Basic gamepad functionality works on older kernels via `xpad`, but the Share button (button 11) may not be reported.

### Pairing

```bash
bluetoothctl
agent NoInputNoOutput
default-agent
scan on
# Wait for "Xbox Wireless Controller" to appear
pair XX:XX:XX:XX:XX:XX
trust XX:XX:XX:XX:XX:XX
connect XX:XX:XX:XX:XX:XX
```

### Checking Driver Loading

```bash
lsmod | grep xpad
# If empty, load it:
sudo modprobe xpad
```

### Permissions (udev Rule)

`/dev/input/event*` nodes are root-only by default. Add a udev rule for user access:

```bash
# /etc/udev/rules.d/99-esp32-xinput.rules
SUBSYSTEM=="hidraw", KERNELS=="0005:045E:02FD.*", MODE="0660", GROUP="plugdev"
# For Series X PID:
# SUBSYSTEM=="hidraw", KERNELS=="0005:045E:0B13.*", MODE="0660", GROUP="plugdev"
```

Then:
```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo gpasswd -a "$USER" plugdev
# Log out and back in for group changes to take effect
```

### Testing

```bash
# Quick sanity check -- shows buttons/axes in real time
jstest --normal /dev/input/js0

# Lower-level evdev view
evtest /dev/input/event0

# Check what the kernel sees
cat /proc/bus/input/devices | grep -A5 "Xbox"

# Monitor Bluetooth traffic
sudo btmon -i hci0
```

### Rumble on Linux

Rumble is sent via Output Report 3 (6 bytes: Report ID + 5 data). Write to the `hidraw` device:

```python
import hid

dev = hid.Device(path="/dev/hidraw0")
# Output Report 3: rumble (6 bytes total)
rumble = bytes([3, 0x40, 0x00, 0x00, 0x00, 0x00])
dev.write(rumble)
dev.close()
```

### Troubleshooting on Linux

If the controller isn't recognized:
1. Check `dmesg | tail` for xpad messages
2. Verify the device appears in `lsinput` or `evtest`
3. Test with `jstest /dev/input/js0`
4. Ensure `xpad` is loaded: `lsmod | grep xpad`
5. If pairing fails, remove the old bond: `bluetoothctl remove XX:XX:XX:XX:XX:XX`

## Windows Compatibility

Windows recognizes the Xbox HID descriptor natively. No additional drivers are needed. The controller appears in:
- **Settings > Bluetooth & devices > Controllers** as "Xbox Wireless Controller"
- **DirectX Input** as an XInput device
- **SDL** as `SDL_GAMEPAD_TYPE_XBOXONE`
- **Steam**: Recognized as Xbox controller. Steam Input handles mapping automatically.

## macOS Compatibility

### How It Works on macOS

macOS natively supports Xbox Wireless Controllers with Bluetooth. This library's XInput mode presents as an Xbox One S (PID `0x02FD`) or Xbox Series X (PID `0x0B13`), so macOS recognizes it as a standard Xbox controller via HID-over-GATT (HOGP). The device appears in System Settings > Bluetooth and works with any game that supports Xbox controllers.

### Supported macOS Versions

| macOS Version | Support Level |
|---|---|
| macOS Big Sur (11.0)+ | Xbox One S via Bluetooth -- full support |
| macOS Monterey (12.0)+ | Xbox One S + GCController framework |
| macOS Ventura (13.0)+ | Improved controller mapping |
| macOS Sonoma (14.0)+ | Rumble/haptics via GCController |
| macOS Sequoia (15.0)+ | Wired Xbox support added (USB-C), BLE unchanged |
| macOS Tahoe (26.0)+ | Current -- full Xbox controller support |

### Pairing

1. Open System Settings > Bluetooth
2. Put the ESP32 into pairing mode (call `begin()` with `GamepadMode::XInput` or `GamepadMode::XInputSeriesX`)
3. The device appears as "Xbox Wireless Controller"
4. Click "Connect"

### Game Compatibility

macOS games that use Apple's `GCController` framework automatically detect the controller:

- **Steam**: Recognizes via SDL's HIDAPI driver. Works out of the box.
- **Apple Arcade**: Full support via GCController.
- **Native macOS games**: Most modern titles support GCController.
- **Emulators** (OpenEmu, etc.): Full support via GCController or SDL.

### Controller Mapping

macOS maps Xbox controller inputs as follows:

| Xbox Input | macOS GCController | Notes |
|---|---|---|
| A/B/X/Y | A/B/X/Y | Standard face buttons |
| LB/RB | Left/Right Shoulder | Standard |
| LT/RT | Left/Right Trigger | Analog (0.0-1.0) |
| Left Stick | Left Thumbstick | X/Y axes |
| Right Stick | Right Thumbstick | X/Y axes |
| D-pad | Up/Down/Left/Right | Hat switch |
| Start | Menu | Standard |
| Back/Select | Options | Standard |
| Home (Xbox button) | N/A | macOS doesn't map this |
| Share (Series X only) | N/A | macOS doesn't map this |

### HID API Access (hidapi)

For direct HID access (e.g., rumble, feature reports):

```python
import hid

dev = hid.Device(path="/dev/hidraw0")
# Output Report 1: rumble (8 bytes: Report ID + 7 data)
rumble = bytes([1, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00])
dev.write(rumble)
dev.close()
```

### Rumble on macOS

macOS supports Xbox controller rumble via the GCController haptics API. SDL also routes rumble through GCController when available. Direct `hidapi` writes to Output Report 1 (rumble) work for applications that use the HID path.

### SDL Integration on macOS

SDL recognizes the device as `SDL_GAMEPAD_TYPE_XBOXONE` and routes it through the HIDAPI Xbox driver or GCController, depending on availability. Build SDL from source for best results:

```bash
git clone --branch release-3.2.14 --depth 1 https://github.com/libsdl-org/SDL.git ~/src/SDL
cmake -S ~/src/SDL -B ~/src/SDL/build -DCMAKE_BUILD_TYPE=Release
cmake --build ~/src/SDL/build -j$(sysctl -n hw.ncpu)
sudo cmake --install ~/src/SDL/build
```

### Troubleshooting

- **Device doesn't appear in Bluetooth settings**: Confirm sketch is running `begin()` with XInput mode, wait for BLE scan
- **Controller detected but buttons/axes wrong**: Check if the game expects GCController or HID; try both paths
- **Rumble doesn't work**: Ensure the game uses GCController haptics; direct `hidapi` rumble requires `sudo` on macOS
- **Connection drops intermittently**: Ensure the ESP32 has adequate power; BLE on macOS can be sensitive to signal strength
- **Share button not recognized**: macOS doesn't map the Share button; use a keyboard shortcut or game-specific mapping

### Steam on macOS

Steam on macOS uses SDL and GCController. XInput mode works:

- Steam recognizes the device as an Xbox controller
- Button mapping is automatic
- Rumble is forwarded via GCController haptics
- No additional configuration needed

## Examples

See the [examples/XInput/](../examples/XInput/) directory:

- **XInputGamepad.ino** -- Xbox One S mode, left stick demo, rumble reception
- **XInputSeriesX.ino** -- Xbox Series X mode, Share button, all inputs + rumble

## Features & Limitations

### What works
- Native Xbox controller recognition on Windows
- Native Xbox controller recognition on macOS (GCController)
- Native Xbox controller recognition on Linux (xpad driver)
- 11 buttons (A/B/X/Y/LB/RB/LS/RS/Select/Start/Home)
- 2 thumbsticks (unsigned 16-bit)
- 2 analog triggers (unsigned 10-bit)
- D-pad (hat switch)
- Strong/weak rumble motors
- Trigger vibration
- Share button (Series X mode)

### Limitations
- No IMU/gyroscope/accelerometer
- No touchpad
- No player LED
- No RGB LED
- No battery reporting
- No iOS support
- Share button requires Linux 6.5+ (Series X PID)
- **Cannot customize VID/PID** — XInput mode uses Microsoft's VID (`0x045E`) and Xbox PIDs (`0x02FD` / `0x0B13`). The host OS matches on VID/PID to load the correct driver: `xpad` on Linux, the native Xbox driver on Windows and macOS. **Do not call `setVid()`/`setPid()` after selecting XInput mode** — changing the VID/PID means the OS won't load the Xbox driver and the device won't work as an XInput controller.
- No social buttons (Share, View, Menu are partially mapped)

## Comparison with Mystfit/ESP32-BLE-CompositeHID

The [Mystfit/ESP32-BLE-CompositeHID](https://github.com/Mystfit/ESP32-BLE-CompositeHID) library also implements XInput alongside composite HID (gamepad + mouse + keyboard). Key differences:

| Aspect | This Library | Mystfit Library |
|--------|-------------|-----------------|
| Focus | Dedicated gamepad library | Composite HID (gamepad+mouse+keyboard) |
| XInput | Dedicated mode | Part of composite device |
| SInput | Yes | No |
| Generic | Yes (configurable) | Yes |
| NimBLE | Yes | Yes |
| DualSense | No | Yes |
| Form factor | Library for gamepad projects | Library for multi-device projects |

Both produce compatible XInput HID reports. Choose this library for dedicated gamepad projects; choose Mystfit for composite multi-device projects.
