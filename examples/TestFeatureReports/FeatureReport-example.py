"""
Host-side companion for TestFeatureReports.ino.

Reads the onboard LED's current state via the Feature Report -- a
human-readable string like "LED=ON t=48213ms", where the timestamp is the
device's millis() when the LED last changed -- reports it, then flips it --
three times, 15 seconds apart. Three flips is an odd number, so each run
leaves the LED in the opposite state it found it in; the *next* run will
then report (and flip from) that opposite state, and the timestamp makes it
obvious a write actually reached the device rather than being a stale read.
The device also toggles BUTTON_1 on a timer whose speed is tied to the LED
(faster while on), so an Input Report client (jstest/evtest, a gamepad
tester, etc.) run alongside this script is a second, independent way to see
each flip take effect. Requires: pip install bleak

Note: on macOS, if this device has been paired as a system Bluetooth HID
device (System Settings > Bluetooth), macOS's own HID stack will hold the
connection and this script won't be able to reach it directly. Forget the
device there first, then run this script.
"""

import asyncio
from bleak import BleakClient, BleakScanner

DEVICE_NAME = "ESP32 BLE Gamepad"  # BleGamepad's default device name
REPORT_CHAR_UUID = "00002a4d-0000-1000-8000-00805f9b34fb"  # HID "Report" characteristic
REPORT_REF_DESC_UUID = "00002908-0000-1000-8000-00805f9b34fb"  # Report Reference descriptor
REPORT_TYPE_FEATURE = 3  # per the Report Reference descriptor spec (1=Input, 2=Output, 3=Feature)

FLIP_COUNT = 3
FLIP_DELAY_SECONDS = 15


async def find_feature_characteristic(client):
    """A HID device can expose several 'Report' characteristics sharing the
    same UUID (Input/Output/Feature); the Report Reference descriptor on each
    one says which is which, so look it up rather than hardcoding a handle."""
    for service in client.services:
        for char in service.characteristics:
            if char.uuid != REPORT_CHAR_UUID:
                continue
            for desc in char.descriptors:
                if desc.uuid != REPORT_REF_DESC_UUID:
                    continue
                report_id, report_type = await client.read_gatt_descriptor(desc.handle)
                if report_type == REPORT_TYPE_FEATURE:
                    return char
    return None


async def read_feature_report(client, feature_char):
    """Returns (state, text): state is True/False for ON/OFF, text is the
    full human-readable report, e.g. "LED=ON t=48213ms"."""
    data = await client.read_gatt_char(feature_char)
    text = data.split(b"\x00", 1)[0].decode("ascii", errors="replace")
    state = "LED=ON" in text
    return state, text


async def main():
    print(f"Scanning for '{DEVICE_NAME}'...")
    device = await BleakScanner.find_device_by_filter(
        lambda d, adv: adv.local_name == DEVICE_NAME, timeout=15.0
    )
    if not device:
        print("Device not found.")
        return

    print(f"Connecting to {device.address}...")
    async with BleakClient(device) as client:
        feature_char = await find_feature_characteristic(client)
        if not feature_char:
            print("Feature Report characteristic not found.")
            return

        for i in range(1, FLIP_COUNT + 1):
            state, text = await read_feature_report(client, feature_char)
            print(f"[{i}/{FLIP_COUNT}] {text}")

            new_text = "OFF" if state else "ON"
            await client.write_gatt_char(feature_char, new_text.encode("ascii"), response=True)
            print(f"[{i}/{FLIP_COUNT}] Wrote {new_text}")

            if i < FLIP_COUNT:
                print(f"Waiting {FLIP_DELAY_SECONDS}s...")
                await asyncio.sleep(FLIP_DELAY_SECONDS)

        _, final_text = await read_feature_report(client, feature_char)
        print(f"Final Feature Report: {final_text}")


asyncio.run(main())
