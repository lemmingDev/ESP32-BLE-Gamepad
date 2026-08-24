# Testing Feature/Output/Input Reports on Linux via HIDAPI

This walks through testing this library's HID Reports (Input, Output, Feature)
from a base Ubuntu/Debian install, using the OS's own HID subsystem rather
than a raw BLE GATT client.

## Why not just use a BLE GATT library (bleak, gatttool, etc.)?

Once this device is paired, BlueZ's **input plugin** recognizes it as a
Bluetooth HID device (its `Icon` is reported as `input-gaming`) and bridges
its HID service straight into the kernel via `uhid`, creating `/dev/hidraw*`
and `/dev/input/js*` nodes for it. That plugin claims the HID service
(`0x1812`) internally — it does **not** expose it as a generic
`org.bluez.GattService1` D-Bus object, so a general-purpose GATT client
(Python's `bleak`, `gatttool`, etc.) will see the device's other services
(Device Information, Battery, GAP) just fine, but the HID service's
characteristics (Input/Output/Feature Report) simply won't appear during
service discovery. This is standard BLE HID-over-GATT host behavior, not a
bug in this library — macOS and Windows behave the same way, for the same
reason.

It's also a deliberate security boundary, not just a routing quirk: if any
paired GATT client could freely read/write a device's Report characteristics,
any app could inject arbitrary keystrokes, mouse movement, or gamepad input,
or read back potentially sensitive Feature Report data, without going
through the OS's normal input-permission model. Funneling HID reports
through a single kernel-mediated path (`uhid` → `hidraw`/`evdev`) is what
lets the OS treat a BLE HID device the same way it treats a USB one, with
the same access controls.

The correct way to talk to Input/Output/Feature Reports once a device is
paired is through the OS's HID API, which is exactly what a real
application/game would use. On Linux that's `hidraw` (via the `hidapi`
library); this doc uses its Python bindings.

## 1. Install system dependencies

```bash
sudo apt update
sudo apt install -y bluez libhidapi-hidraw0 python3-venv joystick evtest
```

- `bluez` — provides `bluetoothd`/`bluetoothctl` (usually already installed on desktop Ubuntu)
- `libhidapi-hidraw0` — native library the Python `hid` package needs
- `python3-venv` — for an isolated environment
- `joystick` — provides `jstest`, a zero-code sanity check for button/axis input
- `evtest` — optional, lower-level input event inspection

## 2. Set up a Python environment

```bash
mkdir -p ~/src/pyBLEgameTests && cd ~/src/pyBLEgameTests
python3 -m venv .venv
.venv/bin/pip install hid
```

## 3. Discover and pair the device

`bluetoothctl`'s default agent will otherwise stop and wait for a
yes/no prompt mid-script, so register a `NoInputNoOutput` agent first —
that auto-accepts "Just Works" pairing, which is what this device uses.

```bash
bluetoothctl <<'EOF'
agent NoInputNoOutput
default-agent
scan on
EOF
```

Let that run for ~10 seconds, then Ctrl-C and find the device's address:

```bash
bluetoothctl devices | grep "ESP32 BLE Gamepad"
# Device 14:2B:2F:EB:98:8A ESP32 BLE Gamepad
```

Then pair, trust, and connect (replace the address with yours):

```bash
bluetoothctl <<'EOF'
scan off
pair 14:2B:2F:EB:98:8A
trust 14:2B:2F:EB:98:8A
connect 14:2B:2F:EB:98:8A
EOF
```

Confirm it bonded successfully:

```bash
bluetoothctl info 14:2B:2F:EB:98:8A
```

Look for `Paired: yes`, `Bonded: yes`, `Connected: yes`, and
`Icon: input-gaming`.

## 4. Verify the kernel picked it up as a HID device

```bash
ls -la /dev/hidraw* /dev/input/js*
grep -A5 "ESP32 BLE Gamepad" /proc/bus/input/devices
```

You should see a new `/dev/hidraw*` and `/dev/input/js*` node appear
(compare timestamps before/after connecting if you have other HID devices
attached), and the `/proc/bus/input/devices` entry should list both an
`event*` and a `js*` handler — confirming the kernel's joystick subsystem
recognizes it as a real gamepad.

`/dev/hidraw*` nodes are root-only by default. Either run the steps below
with `sudo`, or add a udev rule to grant your user access, e.g.:

```bash
# /etc/udev/rules.d/99-esp32-gamepad.rules
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="e502", ATTRS{idProduct}=="bbab", MODE="0660", GROUP="plugdev"
```

(then `sudo udevadm control --reload-rules && sudo udevadm trigger`, and make
sure your user is in the `plugdev` group)

## 5. Quick sanity check: Input Reports (no code needed)

```bash
jstest --normal /dev/input/js1   # adjust js number to match what appeared above
```

Trigger a button press from your sketch (or just watch the axes/buttons if
your sketch reports live input) — you should see the corresponding
`Buttons:`/`Axes:` fields update in real time. This alone confirms the
device's Input Report is being parsed correctly by the kernel.

## 6. Feature / Output / Input Reports via Python (hidapi)

```python
import time
import hid

PRODUCT = "ESP32 BLE Gamepad"
REPORT_ID = 3  # BleGamepad's default HID Report ID; change if you called
                # bleGamepadConfig.setHidReportId(...)

def find_path():
    # manufacturer_string comes through populated on macOS ("Espressif") but
    # empty via Linux's BlueZ-to-uhid bridge, so match on product_string only.
    for d in hid.enumerate():
        if d["product_string"] == PRODUCT:
            return d["path"]
    return None

path = find_path()
dev = hid.Device(path=path)

# --- Feature Report: read, write, read back ---
print("Feature:", list(dev.get_feature_report(REPORT_ID, 4)))
dev.send_feature_report(bytes([REPORT_ID, 0xAA, 0xBB, 0xCC]))
time.sleep(0.2)
print("Feature after write:", list(dev.get_feature_report(REPORT_ID, 4)))

# --- Output Report: write ---
dev.write(bytes([REPORT_ID, 0x10, 0x20, 0x30]))

# --- Input Report: read a few, e.g. while pressing a button ---
dev.nonblocking = True
end = time.time() + 5
while time.time() < end:
    report = dev.read(64, timeout=200)
    if report:
        print("Input:", list(report))

dev.close()
```

Run it with `sudo` unless you've set up the udev rule above:

```bash
sudo .venv/bin/python your_script.py
```

### Notes on `get_feature_report`'s return value

Whether the leading Report ID byte is included in what `get_feature_report()`
returns is **platform-dependent** in `hidapi` itself: Linux's `hidraw`
backend includes it (`[3, 0xAA, 0xBB, 0xCC]`); macOS's `IOHIDManager` backend
strips it (`[0xAA, 0xBB, 0xCC]`). Handle both if you want a script that's
portable across the two:

```python
data = list(after)[1:] if list(after)[:1] == [REPORT_ID] else list(after)
```

## Monitoring Bluetooth traffic

To watch what's actually happening over the air — pairing/bonding, GATT
reads/writes, connection parameter updates — run:

```bash
sudo btmon -i hci0
```

in its own terminal before/while running any of the steps above (`btmon`
ships with `bluez`, so no extra install needed). It decodes HCI/ATT packets
live, which is useful for confirming things like whether a write actually
reached the device, whether pairing completed, or diagnosing a connection
that drops unexpectedly — much more direct than inferring state from
`bluetoothctl info` alone.

## Troubleshooting

**`hid.enumerate()` doesn't find the device**
- Confirm it's actually paired and connected: `bluetoothctl info <address>`
- Confirm a `/dev/hidraw*` node exists for it (step 4)
- If it was previously paired under a different firmware build/config,
  `bluetoothctl remove <address>` and re-pair — see the "Configuration
  Changes Not Taking Effect" entry in [TroubleshootingGuide.md](TroubleshootingGuide.md)

**Pairing hangs waiting for a yes/no prompt**
- You skipped (or the script raced past) the `agent NoInputNoOutput` /
  `default-agent` step — that's what auto-accepts the "Just Works" pairing
  this device uses. Re-run from step 3.

**`Paired: no` / `Bonded: no` after `pair`**
- The default agent likely asked for authorization and nothing answered it
  in time. Set the `NoInputNoOutput` agent as default *before* pairing, or
  answer the prompt interactively if running `bluetoothctl` by hand.
- Run `sudo btmon -i hci0` (see "Monitoring Bluetooth traffic" below) in a
  second terminal while pairing to see exactly where in the SMP/ATT exchange
  it's failing.

**`PermissionError` opening the hidraw device**
- `/dev/hidraw*` is root-only by default; use `sudo` or the udev rule in
  step 4.

**A raw BLE GATT client (bleak, gatttool, `bluetoothctl`'s own `gatt` menu)
can't see the Feature/Output Report characteristics**
- Expected — see the "Why not just use a BLE GATT library" section above.
  Use the OS's HID API instead.
