#!/usr/bin/env python3
"""
Direct SInput protocol test for SInputPlayerLED.ino -- talks the raw
Output/Input Report bytes straight to the device over hidraw, bypassing
SDL's SInput driver entirely. Useful for isolating a firmware bug from an
SDL/driver-layer one.

Checks:
  - Features response: requests it and decodes it the same way SDL's driver
    does, printing the capability bits (PLAYERLED is always set).
  - Player LED: writes SINPUT_COMMAND_PLAYERLED directly and cycles the
    index, the same command SDL_SetGamepadPlayerIndex() sends.

Setup: needs the Python `hid` package (hidapi bindings) -- see
LinuxHIDTesting.md step 2. Needs to see /dev/hidraw* for the SInput-mode
device (VID 0x2E8A / PID 0x10C6, only present once
bleGamepadConfig.setEnableSInput(true) is active and the board is paired
under it): run with sudo, or set up the udev rule from LinuxHIDTesting.md
step 4 (adjusted to KERNELS=="0005:2E8A:10C6.*").

Usage:
    sudo .venv/bin/python sinput_hid_test.py                 # Features once, then cycle Player LED until Ctrl-C
    sudo .venv/bin/python sinput_hid_test.py --features-only # just request+print Features, no LED writes
    sudo .venv/bin/python sinput_hid_test.py --led-only       # skip the Features request
    sudo .venv/bin/python sinput_hid_test.py --device /dev/hidraw2  # skip VID/PID enumeration
"""

import argparse
import sys
import time

import hid

VID = 0x2E8A  # SINPUT_USB_VID, only advertised once setEnableSInput(true) is active
PID = 0x10C6  # SINPUT_USB_PID_GENERIC

# Report IDs (BleSInput.h). hidapi always frames the Report ID as buf[0].
REPORT_ID_INPUT_CMDDAT = 0x02  # Features response arrives on this
REPORT_ID_OUTPUT = 0x03  # commands are sent on this

REPORT_LEN_OUTPUT = 47  # payload only, excludes the Report ID byte
REPORT_LEN_INPUT = 63

# Output Report (0x03) sub-commands, payload byte 0
COMMAND_FEATURES = 0x02
COMMAND_PLAYERLED = 0x03

# Features response (Input Report 0x02) payload offsets: BleSInput.h's
# SINPUT_FEAT_IDX_* constants +1, since hidraw reads prepend the Report ID
# as byte 0 on Linux.
FEAT_CMD_ECHO_OFFSET = 1
FEAT_PROTOCOL_VERSION_OFFSET = 2  # uint16 LE
FEAT_CAPS0_OFFSET = 4
FEAT_USAGE_MASK_0_OFFSET = 14
FEAT_USAGE_MASK_1_OFFSET = 15

CAP_RUMBLE = 1 << 0
CAP_PLAYERLED = 1 << 1
CAP_LEFT_STICK = 1 << 4
CAP_RIGHT_STICK = 1 << 5
CAP_LEFT_TRIGGER = 1 << 6
CAP_RIGHT_TRIGGER = 1 << 7


def find_path():
    for d in hid.enumerate():
        if d["vendor_id"] == VID and d["product_id"] == PID:
            return d["path"]
    return None


def send_command(dev, command, *payload_bytes):
    buf = bytearray(1 + REPORT_LEN_OUTPUT)
    buf[0] = REPORT_ID_OUTPUT
    buf[1] = command
    for i, b in enumerate(payload_bytes):
        buf[2 + i] = b
    dev.write(bytes(buf))


def request_features(dev):
    send_command(dev, COMMAND_FEATURES)

    # Input Report 0x01 (regular gamepad state) is interleaved on the same
    # read stream, so keep reading until the FEATURES echo shows up.
    dev.nonblocking = True
    end = time.time() + 2
    while time.time() < end:
        report = dev.read(1 + REPORT_LEN_INPUT, timeout=200)
        if report and report[0] == REPORT_ID_INPUT_CMDDAT and report[FEAT_CMD_ECHO_OFFSET] == COMMAND_FEATURES:
            return report
    return None


def print_features(report):
    if report is None:
        print("Features: no response within 2s -- device didn't reply to a FEATURES request at all")
        return

    protocol_version = report[FEAT_PROTOCOL_VERSION_OFFSET] | (report[FEAT_PROTOCOL_VERSION_OFFSET + 1] << 8)
    caps0 = report[FEAT_CAPS0_OFFSET]
    print(f"Features response received, protocol_version={protocol_version}, caps0=0x{caps0:02X}")
    print(f"  PLAYERLED capability bit: {'set' if caps0 & CAP_PLAYERLED else 'NOT set'}")
    for name, bit in (
        ("RUMBLE", CAP_RUMBLE),
        ("LEFT_STICK", CAP_LEFT_STICK),
        ("RIGHT_STICK", CAP_RIGHT_STICK),
        ("LEFT_TRIGGER", CAP_LEFT_TRIGGER),
        ("RIGHT_TRIGGER", CAP_RIGHT_TRIGGER),
    ):
        print(f"  {name}: {'set' if caps0 & bit else 'not set'}")
    print(f"  usage_mask_0=0x{report[FEAT_USAGE_MASK_0_OFFSET]:02X} "
          f"usage_mask_1=0x{report[FEAT_USAGE_MASK_1_OFFSET]:02X}")


def cycle_player_led(dev, interval_s):
    print(f"Cycling Player LED index 0->1->2->3->0... every {interval_s:.0f}s "
          "(watch the ESP32's Serial Monitor for 'Player LED index: N' and the onboard LED). Ctrl-C to stop.")
    index = 0
    try:
        while True:
            index = (index + 1) % 4
            send_command(dev, COMMAND_PLAYERLED, index)
            print(f"-> wrote SINPUT_COMMAND_PLAYERLED index={index}")
            time.sleep(interval_s)
    except KeyboardInterrupt:
        print("Stopping, turning Player LED off (index 0)...")
        send_command(dev, COMMAND_PLAYERLED, 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--device", help="hidraw path to use directly, skipping VID/PID enumeration")
    parser.add_argument("--features-only", action="store_true", help="only request+print the Features response")
    parser.add_argument("--led-only", action="store_true", help="skip the Features request, only cycle the Player LED")
    parser.add_argument("--interval", type=float, default=3.0, help="seconds between Player LED changes (default: 3)")
    args = parser.parse_args()

    if args.features_only and args.led_only:
        print("--features-only and --led-only are mutually exclusive", file=sys.stderr)
        return 1

    path = args.device or find_path()
    if path is None:
        print(f"No HID device found with VID=0x{VID:04X} PID=0x{PID:04X}. Is the board paired/connected with "
              "setEnableSInput(true) active? See SDL3Testing.md step 3.", file=sys.stderr)
        return 1

    print(f"Opening {path}")
    dev = hid.Device(path=path)
    try:
        if not args.led_only:
            print_features(request_features(dev))
        if not args.features_only:
            cycle_player_led(dev, args.interval)
    finally:
        dev.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
