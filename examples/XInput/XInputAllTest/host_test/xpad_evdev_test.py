#!/usr/bin/env python3
"""
xpad_evdev_test.py — Linux XInput/xpad parity check for
examples/XInput/XInputAllTest/XInputAllTest.ino

Mirrors host_test/xinput_test.c (Windows) for Linux: proves the ESP32
is bound via xpad (VID 0x045E PID 0x02FD/0x0B13, HID Report PID 0x03 at
BleXInputDescriptors.h:156) and not just hid-generic / joy.cpl generic.

- Enumerates hidraw (hid.enumerate VID 045E:02FD) — should appear once
  XInputAllTest.ino is paired as "Xbox Wireless Controller"
  (BleGamepad.cpp:973, BleGamepadConfiguration.cpp:292).
- Checks /proc/bus/input/devices for "Microsoft X-Box" (xpad) vs "HID"
  (hid-generic) and kernel FF_RUMBLE exposure (PID 0x03 DC Enable/
  Magnitude at BleXInputDescriptors.h:156).
- Optionally streams /dev/input/event* via evdev (same bytes
  BleGamepad.cpp:1372 maps to X/Y/Z/Rz/Rx/Ry at 0x30/0x31/0x32/0x35/0x33/0x34).

Setup (matches LinuxHIDTesting.md:1):
  sudo apt install -y bluez libhidapi-hidraw0 python3-venv joystick evtest
  python3 -m venv .venv && .venv/bin/pip install hid evdev

Run while XInputAllTest.ino is circling:
  sudo .venv/bin/python host_test/xpad_evdev_test.py
  # or: sudo python3 host_test/xpad_evdev_test.py

Fallback: if python evdev isn't available, the script still runs the
hid / /proc / FF enumeration checks and skips the streaming loop.
"""
import sys, time, glob, re

VID, PID_ONES, PID_SERIESX = 0x045E, 0x02FD, 0x0B13

def check_hidapi():
    try:
        import hid
    except ImportError:
        print("hid not installed — run: .venv/bin/pip install hid", file=sys.stderr)
        return []
    devs = [d for d in hid.enumerate() if d["vendor_id"] == VID and d["product_id"] in (PID_ONES, PID_SERIESX)]
    if not devs:
        print("hid.enumerate: no 045e:02fd/0b13 found — is XInputAllTest.ino paired? See LinuxHIDTesting.md:3", file=sys.stderr)
    else:
        for d in devs:
            print(f"hidraw: {d['path'].decode() if isinstance(d['path'], bytes) else d['path']} prod={d['product_string']} manuf={d['manufacturer_string']}")
    return devs

def check_proc():
    try:
        txt = open("/proc/bus/input/devices").read()
    except Exception as e:
        print(f"/proc/bus/input/devices: {e}", file=sys.stderr)
        return
    # Find blocks mentioning Microsoft/Xbox vs HID
    for block in txt.strip().split("\n\n"):
        if "045E" in block or "045e" in block or "Microsoft" in block or "X-Box" in block or "Xbox" in block:
            name = re.search(r'N: Name="([^"]+)"', block)
            handlers = re.search(r'H: Handlers=(.+)', block)
            print(f"input: {name.group(1) if name else '?'}  {handlers.group(1) if handlers else ''}")
            # Bus 0005 = BLUETOOTH, handlers js* vs event*
            if "Microsoft" in block or "X-Box" in block:
                print("  -> xpad driver (XInput path)")
            elif "HID" in block:
                print("  -> hid-generic (generic DInput, not XInput)")

def check_ff():
    # Look for FF_RUMBLE capable event* node (from PID 0x03)
    for p in glob.glob("/sys/bus/hid/devices/*/capabilities/ff") + glob.glob("/sys/class/input/event*/device/capabilities/ff"):
        print(f"ff: {p}")
    # evdev FF via ioctl is more direct but needs evdev lib; hint:
    print("For FF_RUMBLE detail: sudo evtest --ff /dev/input/eventN (pick N from /proc/bus/input/devices) — should list FF_RUMBLE if PID 0x03 is correct")

def stream_evdev():
    try:
        import evdev
    except ImportError:
        print("evdev not installed — skipping stream (still did hid/proc/ff checks). pip install evdev for live axis/button print", file=sys.stderr)
        return
    # Find the js* that is xpad
    devs = [evdev.InputDevice(p) for p in evdev.list_devices()]
    target = None
    for d in devs:
        if "045e" in d.phys.lower() or "microsoft" in d.name.lower() or "x-box" in d.name.lower():
            target = d
            break
    if not target and devs:
        # Fallback: pick first with ABS_X
        for d in devs:
            if evdev.ecodes.ABS_X in d.capabilities().get(evdev.ecodes.EV_ABS, []):
                target = d
                break
    if not target:
        print("No evdev device found for 045e:02fd — check jstest /dev/input/js*", file=sys.stderr)
        return
    print(f"Streaming {target.path} ({target.name}) — move sticks/triggers, press A/B/X/Y (BUTTON_1..4 at BleGamepad.cpp:1372), Ctrl-C to stop")
    for ev in target.read_loop():
        if ev.type in (evdev.ecodes.EV_KEY, evdev.ecodes.EV_ABS):
            print(f"{evdev.categorize(ev)}")

if __name__ == "__main__":
    print("=== hidapi enumerate ===")
    check_hidapi()
    print("\n=== /proc/bus/input/devices ===")
    check_proc()
    print("\n=== FF_RUMBLE (PID 0x03) ===")
    check_ff()
    print("\n=== evdev stream (10s sample, Ctrl-C anytime) ===")
    try:
        import signal
        signal.alarm(10)
        stream_evdev()
    except Exception as e:
        print(f"stream: {e}", file=sys.stderr)
