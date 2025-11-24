import asyncio
from bleak import BleakClient, BleakScanner

FEATURE_UUID = "INSERT-YOUR-FEATURE-REPORT-CHAR-UUID-HERE"

async def main():
    print("Scanning for ESP32 Gamepad...")
    devices = await BleakScanner.discover()

    esp = None
    for d in devices:
        if "ESP32 Gamepad" in d.name:
            esp = d
            break

    if not esp:
        print("ESP32 Gamepad not found.")
        return

    print(f"Connecting to {esp.address}...")
    async with BleakClient(esp.address) as client:
        if not client.is_connected:
            print("Failed to connect.")
            return

        print("Reading feature report...")
        data = await client.read_gatt_char(FEATURE_UUID)
        print("Initial Feature Report:", list(data))

        print("Writing new feature report...")
        new_values = bytes([0xAA, 0xBB, 0xCC])
        await client.write_gatt_char(FEATURE_UUID, new_values)

        print("Reading feature report again...")
        data2 = await client.read_gatt_char(FEATURE_UUID)
        print("Updated Feature Report:", list(data2))

asyncio.run(main())
