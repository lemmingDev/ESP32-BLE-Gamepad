# Using ESP32-BLE-Gamepad alongside NuS-NimBLE-Serial

This library is a HID-only gamepad: it exposes buttons/axes over the standard
HID Service (`0x1812`). If you also want a free-form serial channel to a
companion app, terminal, or debug console — configuration, telemetry, logging —
add the external [NuS-NimBLE-Serial](https://github.com/afpineda/NuS-NimBLE-Serial)
library next to it. The two coexist on the same NimBLE stack and the same GATT
server with no changes to this library. See [GattVsHid.md](../GattVsHid.md)
(path C) for why this serial path is separate from the HID path.

> **Attribution:** NuS-NimBLE-Serial is © Ángel Fernández Pineda, licensed
> [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). It is a separate
> dependency — it is not bundled with this library, and this library's MIT
> license does not cover it.

## Install

Arduino IDE: **Tools → Manage Libraries →** filter for `NuS-NimBLE-Serial`
(by afpineda) and install. It pulls in `NimBLE-Arduino` automatically.

PlatformIO: add both to `lib_deps`:

```ini
lib_deps =
  h2zero/NimBLE-Arduino@^2.5.1
  https://github.com/afpineda/NuS-NimBLE-Serial.git
```

> **Version constraint:** NuS-NimBLE-Serial requires
> `NimBLE-Arduino >= 2.1.0 && < 3.0.0`. This library pins `^2.5.1`
> (see `library.json`). Re-check compatibility when NimBLE-Arduino 3.x
> releases and NuS-NimBLE-Serial adds 3.x support.

## Why this works without modifying either library

- `NimBLEDevice::init()` is ref-counted — calling it from both libraries is safe.
- `NimBLEDevice::createServer()` always returns the same global server, so the
  HID service (this library) and the Nordic UART Service (NuS) end up as two
  services on one server.
- `BleGamepad`'s 4th constructor argument, `delayAdvertising`, exists for
  exactly this: when `true`, `begin()` builds the HID service and configures
  advertising but does **not** start it, so you can register NuS first and
  then start advertising once for both services together.
- `NuSerial.start(false)` registers the NuS service **without** touching
  advertising, leaving this library's advertising setup (appearance, name, HID
  UUID) intact.

## Init sequence (all gamepad modes)

The order matters. `NuSerial.start()` requires the NimBLE stack to be
initialized, but `bleGamepad.begin()` initializes it asynchronously on its own
task — so wait for the server to exist before starting NuS:

```cpp
#include <Arduino.h>
#include <BleGamepad.h>
#include <NuSerial.hpp>   // from NuS-NimBLE-Serial
#include <NimBLEDevice.h>

// delayAdvertising = true: build HID + configure advertising, but don't start it yet
BleGamepad bleGamepad("ESP32 BLE Gamepad", "Espressif", 100, true);

void setup()
{
    Serial.begin(115200);
    bleGamepad.begin();

    // taskServer() runs NimBLEDevice::init() + createServer() asynchronously.
    // NuSerial.start() needs the stack up first, so wait for the server.
    while (NimBLEDevice::getServer() == nullptr)
    {
        delay(10);
    }

    // false = register the NuS service but leave advertising alone
    NuSerial.start(false);

    // Now advertise once: the packet carries the HID service; NuS is
    // discoverable via GATT service discovery after connecting (see below).
    NimBLEDevice::getServer()->getAdvertising()->start();
}
```

> **Do NOT call `NuSerial.begin()` here.** `begin()` defaults to automatic
> advertising, which reconfigures advertising behind this library's back and
> breaks the HID advertising setup. Always use `NuSerial.start(false)` and
> start advertising yourself as shown above.

## Reading and writing serial data

`NuSerial` behaves like Arduino's `Serial` (it inherits from `Stream`):

```cpp
void loop()
{
    // Incoming: drain everything the central sent
    while (NuSerial.available())
    {
        char c = (char)NuSerial.read();
        // ... accumulate a line, parse a command ...
    }

    // Outgoing: notify subscribed centrals
    NuSerial.println("uptime_ms=" + String(millis()));
}
```

Connection state is polled — `NuSerial` is a singleton and cannot be
subclassed, so there is no subscribe callback to override:

| Question | Call |
|----------|------|
| Is a central subscribed to NuS TX? | `NuSerial.isConnected()` |
| How many subscribers? | `NuSerial.subscriberCount()` |
| Is the service running? | `NuSerial.isStarted()` |

Note the two "connected" concepts are independent: a game/terminal can be
connected to the HID gamepad (`bleGamepad.isConnected()`) without subscribing
to NuS, and a BLE terminal app can subscribe to NuS without ever bonding the
HID profile. Gate gamepad behavior on `bleGamepad.isConnected()` and serial
pushes on `NuSerial.isConnected()` separately. (Writes with no subscriber are
silently dropped, so the `isConnected()` guard just saves you work.)

## Advertising: NuS UUID rides in the scan response

The 31-byte BLE advertising packet is already full (flags + appearance +
16-bit HID UUID + truncated name), and the 128-bit Nordic UART UUID
(`6E400001-...`) does not fit there. The sketches therefore enable scan
responses and add the NuS UUID via `addServiceUUID()`, which NimBLE
automatically overflows into the scan-response payload:

```cpp
NimBLEAdvertising *pAdvertising = NimBLEDevice::getServer()->getAdvertising();
pAdvertising->enableScanResponse(true);
pAdvertising->addServiceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
```

Verified live: the adv packet carries flags + HID UUID and the scan response
carries the NUS UUID, so scanners doing active scans (nRF Connect, Serial
Bluetooth Terminal, `bleak`) can filter by service UUID. Passive-scan bytes
are unchanged. If `addServiceUUID()` ever reports false (payload full), the
sketches print a warning and carry on — serial still works via GATT service
discovery after connecting.

After a disconnect, `advertiseOnDisconnect(true)` (set by this library)
restarts advertising automatically with the same data, so both paths keep
working across reconnections without further code.

## Troubleshooting

**Terminal can't find the NUS service, but the sketch is running it.**
Windows caches the GATT database per bonded device and only refreshes it on
a Service Changed indication or a fresh pairing. Reflashing the firmware does
not invalidate that cache, so a scanner can show a stale service list (missing
NUS, or missing characteristics inside it) while the service works fine for
already-connected clients. Fix: Windows Settings → Bluetooth & devices →
remove the gamepad, then pair again. This only bites during firmware
development; end users flash once and bond once.

**Board sits in `waiting for download` after flashing.**
Some boards' auto-reset circuits leave the chip strapped into the ROM
bootloader instead of rebooting into flash (esptool reports success and
"Hard resetting" anyway). Fix: press EN/RST (or power-cycle). Firmware-side
there is nothing to change.

**No `hello` line after subscribing.**
The greeting is best-effort: a notify sent while the central is still
enabling notifications (CCCD race) can be lost, so the sketches hold it
500ms — and always answer `proto?` on demand. If neither arrives, the link
itself is suspect, not the greeting. Related: NuS only decrements its
subscriber count on *explicit unsubscribe*, so clients that disconnect
dirty leave a phantom subscriber until reboot (status pushes keep flowing
to nobody). Tear down cleanly — unsubscribe before disconnecting.

**`addServiceUUID()` reports false in the serial monitor.**
The scan-response payload is full too (it holds the 128-bit NUS UUID plus
whatever else overflowed there). Serial still works via GATT service
discovery after connecting; only service-UUID scan filtering is lost.

## Client apps

Any BLE terminal supporting the Nordic UART Service works. Known-good options
(per the NuS-NimBLE-Serial README):

- Android: [nRF Connect for Mobile](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp),
  [Serial Bluetooth Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal)
- iOS: [nRF Connect for Mobile](https://apps.apple.com/es/app/nrf-connect-for-mobile/id1054362403)
- Desktop: [NeutralNUS](https://github.com/KevinJohnMulligan/neutral-nus-terminal/releases)

On Android, enable Bluetooth **and** location services or the device will not
be discovered.

## Migrating from the old built-in NUS (`BleNUS`, pre-strip)

This library used to ship its own hand-rolled NUS (`beginNUS()`,
`sendDataOverNUS()`, `getNUS()`). That was removed in favor of
NuS-NimBLE-Serial. Mapping for old sketches:

| Old (removed) | New (NuS-NimBLE-Serial) |
|---------------|--------------------------|
| `bleGamepad.beginNUS()` | `NuSerial.start(false)` after the server-exists wait (see above) |
| `bleGamepad.getNUS()->println(...)` / `print` / `write` | `NuSerial.println(...)` / `print` / `write` directly |
| `bleGamepad.sendDataOverNUS(data, len)` | `NuSerial.write(data, len)` |
| `nus->available()` / `nus->read()` | `NuSerial.available()` / `NuSerial.read()` (same `Stream` API) |
| `nus->setSubscribeCallback(fn)` | Poll `NuSerial.subscriberCount()` for changes; greet on 0→1 transitions |
| `nus->setDataReceivedCallback(fn)` | Poll `NuSerial.available()` in `loop()` and dispatch yourself |

## Per-mode notes

NuS works identically in all three gamepad modes — the serial channel is
orthogonal to the HID report layout. The `examples/NuS/` folder has one
bidirectional bridge example per mode because the *interesting commands*
differ:

- **Generic, strict** (`NuSGenericBridge`): pure library defaults —
  `press`/`release`, `axis` ×8, `hat`, `battery`, `power`, plus bond/TX-power
  management (`pair`, `unpair`, `txpower`, `addr?`).
- **Generic, advanced** (`NuSGenericAdvanced`): everything in the strict
  bridge, plus start/select special buttons and bidirectional HID
  output/feature reports (`special`, `output?`, `feature get`/`set`).
- **SInput** (`NuSSInputBridge`): same gamepad control, plus `motion`,
  `touch`, start/select/home specials, and `led?` / `rumble?` / `rgb?`
  queries that surface the last SInput Output Report (`0x03`) state the host
  sent. IMU + RGB capability flags are enabled so SDL advertises full caps.
  Verified live: a WebHID host (e.g. joypad.ai) drives player-LED, rumble,
  and RGB end to end — including lighting the onboard LED for Player 1.
  Note `rumble?` reports the *last* frame, so query mid-pulse; stop-frames
  read back as zero, and non-`0x02`/short haptic frames are ignored.
- **XInput** (`NuSXInputBridge`): same gamepad control, plus start/select/
  home/back specials (back = Share), `battery`, and `rumble?` surfacing
  strong/weak motors and trigger magnitudes from the Xbox Output Report
  (`0x03`).
- **All modes** (`NuSSerialDiag`): diagnostics reimplementation — auto-press,
  echo/`help`, proactive status lines, battery ramp, LED blink (see
  `examples/NuS/README.md` for the full tour).
