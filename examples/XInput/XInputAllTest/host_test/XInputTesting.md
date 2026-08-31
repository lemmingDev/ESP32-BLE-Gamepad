# Testing XInputAllTest.ino as real XInput (Windows + Linux)

This verifies `examples/XInput/XInputAllTest/XInputAllTest.ino` is routed as **XInput** (`VID 045E:02FD` / `0B13`, Report `0x03` PID at `BleXInputDescriptors.h:156`) and not just generic DirectInput/HID (`joy.cpl` `6 axis 16 button...` at `BleGamepad.cpp:973`).

## Firmware stimulus (same for both hosts)

Flash and pair `XInputAllTest.ino` (`BleXInputDescriptors.h:16` 1708 descriptor now 5/5 balanced, `BleGamepad.cpp:973` `Microsoft` + `XBOX_1708_SERIAL`). It cycles like Mystfit `XboxXInputController.ino:68`:

* 11 buttons `A/B/X/Y/LB/RB/LS/RS/Start/Select/Home` (`BleXInput.h:24` / `BleGamepad.cpp:1372`) + `Back` Share (`BleXInputDescriptors.h:111`) + 8-way hat
* Triggers slow sweep (`0..32767` → `0..1023` at `BleGamepad.cpp:1402`, then `bLeftTrigger`/`bRightTrigger` `0..255` in `XInputGetState`)
* Both thumbs circling 8 s with triggers in opposite phase (`host_test/XInputAllTest.ino` `testThumbsticksWithTriggers`)

If this sketch is running, both host tests below should show the same packet counter / axis motion.

## Windows — XInputGetState (`host_test/xinput_test.c`)

`joy.cpl` will show `OK` even when `XInputGetState` returns `ERROR_DEVICE_NOT_CONNECTED` if the Report Map is generic. This test is the authoritative XInput check.

```powershell
# Build (MinGW)
gcc host_test/xinput_test.c -lxinput -o host_test/xinput_test.exe
# or MSVC: cl host_test/xinput_test.c xinput.lib

# Run while paired as "Xbox Wireless Controller"
.\host_test\xinput_test.exe
# Expected: [0] pkt=N btn=... LX=... LY=... LT=.. RT=.. incrementing every ~10 ms
# Also: uncomment the XInputSetState line in the .c to drive rumble and watch
# the ESP32 Serial "Rumble: strong=.. weak=.. L=.. R=.." from BleXInput.cpp:8
```

***Troubleshooting Windows***
* `No XInput device` but `joy.cpl` shows `Controller` `OK`: Report Map or `VID:PID` allowlist wrong — check `BleXInputDescriptors.h:16` PID `0x03` `DC Enable`/`Magnitude` and `BleGamepadConfiguration.cpp:292` `XINPUT_PID_*`.
* Cached pairing from before the descriptor fix (orphan `0xC0`): **Settings → Bluetooth & devices → Remove** the device, toggle Bluetooth, re-pair (same as `SDL3Testing.md` flash-erase note).
* `hardwaretester` / `gamepad-tester.com` showing `Vendor:045e Product:02fd` `Unknown` / `Standard Gamepad: Unknown` is the *browser* `Gamepad API` (which is DInput-like), not `XInput` — `xinput_test.exe` `pkt` increment is the XInput signal.

## Linux — xpad / evdev (`host_test/xpad_evdev_test.py`)

`xpad` allowlists `045E:02FD` since ~4.x; `0B13` (Series X) only since `6.5`. Check your kernel (`uname -r`) if Series X never appears as `Microsoft X-Box One S`.

```bash
sudo apt install -y bluez libhidapi-hidraw0 python3-venv joystick evtest
python3 -m venv .venv && .venv/bin/pip install hid evdev

# Pair once (NoInputNoOutput auto-accepts Just Works, see LinuxHIDTesting.md:3)
bluetoothctl <<'EOF'
agent NoInputNoOutput
default-agent
scan on
EOF
# ... find "Xbox Wireless Controller" address, then:
bluetoothctl <<'EOF'
scan off
pair <addr>
trust <addr>
connect <addr>
EOF

# Run the parity check while XInputAllTest.ino circles
sudo .venv/bin/python host_test/xpad_evdev_test.py
# Expected hid: path ... prod "Xbox Wireless Controller" manuf "Microsoft"
# Expected /proc/bus/input/devices: N: Name="Microsoft X-Box One S pad"  H: Handlers=js0 eventN  -> xpad driver (XInput path)
# Expected FF: capabilities/ff lists FF_RUMBLE (from PID 0x03) — verify with:
#   sudo evtest --ff /dev/input/eventN   # should list FF_RUMBLE

# Generic HID (not XInput) would show N: Name="HID  ... 045e:02fd" with hid-generic and no FF_RUMBLE
# Also matches LinuxHIDTesting.md:5 seat vs plugdev/input groups and :4 udev KERNELS=="0005:E502:BBAB.*" rule
```

***Troubleshooting Linux***
* `hid.enumerate: no 045e:02fd` — not paired/connected (`bluetoothctl info <addr>` should show `Paired: yes Bonded: yes Icon: input-gaming`).
* No `FF_RUMBLE`: Report `0x03` PID missing or malformed (`BleXInputDescriptors.h:156`).
* `evtest` permission denied over SSH but works locally: `getent group input` / `sudo gpasswd -a $USER input` + re-login, or `TAG+="uaccess"` seat ACL (see `LinuxHIDTesting.md:5`).
* `dmesg` `Event data for report N was too short` — Report Map vs `hidReportDescriptorSize` mismatch, or forgot `bluetoothctl remove <addr>` + ESP32 flash erase after descriptor change.

## Cross-OS wedge to remember

* `gamepad-tester.com` / `hardwaretester` = browser `Gamepad API` (DInput-like, shows `Unknown` for BLE XInput even when `XInputGetState` and `xpad` are correct).
* `xinput_test.exe` packet counter (Windows) and `xpad_evdev_test.py` `Microsoft X-Box` + `FF_RUMBLE` (Linux) are the XInput signals.

## See also

* `BleGamepad.cpp:130` / `docs/SInputMode.md` for SInput verbatim 139-byte descriptor testing (SDL3 path).
* `LinuxHIDTesting.md:1` and `GattVsHid.md:103` for the HID-over-GATT vs NUS split.
