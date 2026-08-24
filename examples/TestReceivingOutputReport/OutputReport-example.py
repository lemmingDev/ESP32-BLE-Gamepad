"""
Host-side companion for TestReceivingOutputReport.ino.

Writes bytes to the device's Output Report and reads it back to confirm the
device applied the write. Requires: pip install bleak

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
REPORT_TYPE_OUTPUT = 2  # per the Report Reference descriptor spec (1=Input, 2=Output, 3=Feature)


async def find_output_characteristic(client):
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
                if report_type == REPORT_TYPE_OUTPUT:
                    return char
    return None


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
        output_char = await find_output_characteristic(client)
        if not output_char:
            print("Output Report characteristic not found.")
            return

        print("Writing output report...")
        new_values = bytes([0x10, 0x20, 0x30])
        await client.write_gatt_char(output_char, new_values, response=True)

        print("Reading output report back...")
        data = await client.read_gatt_char(output_char)
        print("Output Report after write:", list(data))


asyncio.run(main())
