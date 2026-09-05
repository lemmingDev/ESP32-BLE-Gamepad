# NuS Examples — gamepad + BLE serial side channel

These examples pair the HID gamepad with a Nordic UART Service (NUS) serial
channel from the external
[NuS-NimBLE-Serial](https://github.com/afpineda/NuS-NimBLE-Serial) library
(CC BY 4.0, © Ángel Fernández Pineda — install it from the Arduino Library
Manager; it is **not** bundled here). Full background, init sequence, and
client apps: [docs/NuSCompatibility.md](../../docs/NuSCompatibility.md).

One bridge example per gamepad mode — they are separate files so there is no
confusion about which commands and queries belong to which HID report layout.
The serial channel itself works identically in all modes.

| Example | Mode | Terminal → device | Device → terminal |
|---------|------|-------------------|-------------------|
| [NuSSerialDiag](NuSSerialDiag/NuSSerialDiag.ino) | Generic | `help`, `button4`, echo | Greeting on subscribe, proactive status lines (uptime, buttons, battery, heap) |
| [NuSGenericBridge](NuSGenericBridge/NuSGenericBridge.ino) | Generic | `press`/`release` 1–16, `axis`, `hat`, `battery`, `status` | `ok`/`err` replies, periodic `state` |
| [NuSSInputBridge](NuSSInputBridge/NuSSInputBridge.ino) | SInput | `press`/`release` 1–25, `stick`, `trigger`, `hat`, `battery`, `status` | `ok`/`err` replies, `event led/rumble/rgb` pushed as SInput Output Reports arrive, plus `led?`/`rumble?`/`rgb?` queries |
| [NuSXInputBridge](NuSXInputBridge/NuSXInputBridge.ino) | XInput One S | `press`/`release` 1–11 (A…Home), `stick`, `trigger`, `hat`, `status` | `ok`/`err` replies, `event rumble` (strong/weak + trigger magnitudes) pushed as Xbox Output Reports arrive, plus `rumble?` query |

## Shared pattern (all four sketches)

```cpp
BleGamepad bleGamepad("...", "Espressif", 100, true); // delayAdvertising = true
bleGamepad.begin();                                    // builds HID, no advertising yet
while (NimBLEDevice::getServer() == nullptr) { delay(10); } // wait for NimBLE init
NuSerial.start(false);                                 // register NuS, leave advertising alone
NimBLEDevice::getServer()->getAdvertising()->start();  // advertise once, both services
```

- `delayAdvertising=true` is required: without it, advertising starts before
  the NuS service exists.
- Use `NuSerial.start(false)` — never `NuSerial.begin()`, which enables
  automatic advertising and stomps the HID advertising setup.
- `NuSerial` is a singleton with no subscribe callback: poll
  `NuSerial.subscriberCount()` / `NuSerial.isConnected()`.
- The NUS UUID is not in the 31-byte advertising packet (it is full);
  terminal apps find NuS via GATT service discovery after connecting.

## Which one do I want?

- **"Is my BLE link alive, and is HID flowing?"** → `NuSSerialDiag`
  (reimplementation of the removed `examples/Generic/Diagnostics` sketch).
- **"Drive a Generic / SInput / Xbox pad from my phone or laptop terminal"**
  → the matching `NuS*Bridge`.
- **"Show SInput player-LED / rumble / RGB, or Xbox rumble, in a terminal"**
  → `NuSSInputBridge` / `NuSXInputBridge` (note: those host reports only
  arrive from an SInput-aware host, e.g. SDL3 with the SInput hint, or an
  Xbox host — not from the plain OS joystick path).
