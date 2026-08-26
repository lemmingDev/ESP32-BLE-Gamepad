#!/usr/bin/env python3
"""
Direct SInput protocol test for SInputPlayerLED.ino -- talks the raw
Output/Input Report bytes straight to the device over hidraw, bypassing
SDL's SInput driver entirely.

This exists to answer one question in isolation: does the firmware
(BleSInputReceiver::onWrite()/sendFeaturesResponse() in BleSInput.cpp) behave
correctly, independent of whatever SDL's own driver is or isn't doing? See
"Resolved: Player LED wasn't reaching SDL's driver" in SDL3Testing.md in this
directory for the full story -- what actually turned out to be wrong was a
stale Features response byte layout in BleSInput.h (missing a protocol_version
field SDL's driver now expects), not an SDL bug. This script's offsets below
are kept in sync with BleSInput.h's SINPUT_FEAT_IDX_* constants -- if that
header's offsets change again, update these to match or this script will
silently read the wrong bytes.

Two independent checks:
  - Features response: requests the SInput Features response and decodes it
    the same way SDL's own driver does. Prints whether the PLAYERLED
    capability bit is set (it always is, unconditionally -- BleSInput.cpp) --
    compare this against sdl3_gamepad_test.c's "Player LED capable (SDL's
    view)" line for the exact same connection; they should now agree.
  - Player LED: writes SINPUT_COMMAND_PLAYERLED directly and cycles the
    index, the same command SDL_SetGamepadPlayerIndex() sends. The ESP32's
    Serial Monitor should print "Player LED index: N" for this exactly like
    it now does when driven via sdl3_gamepad_test.c.

Setup: needs the Python `hid` package (hidapi bindings) -- see
LinuxHIDTesting.md step 2. Needs to see /dev/hidraw* for the SInput-mode
device (VID 0x2E8A / PID 0x10C6 -- only present once
bleGamepadConfig.setEnableSInput(true) is active and the board has been
re-paired under it, see SDL3Testing.md step 3): run with sudo, or set up the
udev rule from LinuxHIDTesting.md step 4 (adjusted to KERNELS=="0005:2E8A:10C6.*").

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

VID = 0x2E8A  # SINPUT_USB_VID (BleGamepadConfiguration.h) -- only advertised once setEnableSInput(true) is active
PID = 0x10C6  # SINPUT_USB_PID_GENERIC

# Report IDs (BleSInput.h). hidapi's hid_write()/hid_read() always frame the
# Report ID as buf[0], same as this directory's inline SDL3Testing.md example
# and LinuxHIDTesting.md's get_feature_report() note -- regardless of what the
# underlying BLE GATT payload looks like.
REPORT_ID_INPUT_CMDDAT = 0x02  # SINPUT_REPORT_ID_INPUT_CMDDAT -- Features response arrives on this
REPORT_ID_OUTPUT = 0x03  # SINPUT_REPORT_ID_OUTPUT_CMDDAT -- commands are sent on this

REPORT_LEN_OUTPUT = 47  # SINPUT_REPORT_LEN_OUTPUT -- payload only, excludes the Report ID byte
REPORT_LEN_INPUT = 63  # SINPUT_REPORT_LEN_INPUT -- payload only

# Output Report (0x03) sub-commands, payload byte 0 (BleSInput.h)
COMMAND_FEATURES = 0x02  # SINPUT_COMMAND_FEATURES
COMMAND_PLAYERLED = 0x03  # SINPUT_COMMAND_PLAYERLED

# Features response (Input Report 0x02) payload offsets, BleSInput.h's
# SINPUT_FEAT_IDX_* constants +1: those are indices into the GATT
# payload-only buffer BleSInput.cpp builds, but hidraw's reads (like its
# get_feature_report(), per LinuxHIDTesting.md) prepend the Report ID as
# byte 0 on Linux, shifting everything else up by one.
FEAT_CMD_ECHO_OFFSET = 1  # SINPUT_FEAT_IDX 0 + 1 -- echoes COMMAND_FEATURES back
FEAT_PROTOCOL_VERSION_OFFSET = 2  # SINPUT_FEAT_IDX_PROTOCOL_VERSION (1) + 1, uint16 LE
FEAT_CAPS0_OFFSET = 4  # SINPUT_FEAT_IDX_CAPS0 (3) + 1
FEAT_USAGE_MASK_0_OFFSET = 14  # SINPUT_FEAT_IDX_USAGE_MASK_0 (13) + 1
FEAT_USAGE_MASK_1_OFFSET = 15  # SINPUT_FEAT_IDX_USAGE_MASK_1 (14) + 1

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

    # The device also continuously notifies Input Report 0x01 (regular
    # gamepad state) at the same time, interleaved on the same read stream --
    # keep reading until the FEATURES echo shows up or we time out.
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
    print(f"  PLAYERLED capability bit: {'set' if caps0 & CAP_PLAYERLED else 'NOT set'} "
          "(this library always sets it -- BleSInput.cpp's sendFeaturesResponse())")
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
