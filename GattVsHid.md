# GATT vs HID-over-GATT, and How Extra Features (RGB/Player LEDs, Rumble) Get to an App

This library exposes the ESP32 over BLE in two distinct ways at once, and
mixing them up is the source of most confusion about "why doesn't my game
see the LED/rumble feature I added":

1. **HID-over-GATT (HOGP)** — the standard Bluetooth SIG profile that makes
   this device look like a real gamepad/joystick to the OS. This is what
   `BleGamepad`'s buttons/axes (Input Report), `setFeatureBuffer()`/
   `getFeatureBuffer()` (Feature Report), and `getOutputBuffer()` (Output
   Report) ride on.
2. **A private GATT service (NUS)** — a plain custom BLE service with no HID
   semantics at all, for arbitrary bytes to/from a companion app. This is
   `beginNUS()`/`sendDataOverNUS()`.

These two paths are reachable from completely different kinds of software on
the host, which is the crux of this doc.

## GATT, briefly

Every BLE peripheral (this ESP32) exposes a tree of **services**, each
containing **characteristics** a central (the host) can read, write, and/or
subscribe to for notifications. This library's GATT server has several
services: Generic Access, Device Information, Battery Service, the HID
Service (`0x1812`), and — if `beginNUS()` is called — the Nordic UART
Service (`6e400001-...`, a vendor-defined 128-bit UUID, not a Bluetooth SIG
standard service).

Nothing about "GATT" implies "gamepad" or "input device" on its own — it's
just a generic key/value RPC mechanism. What turns a GATT server into
something the OS recognizes as a controller is the HID Service specifically.

## HID-over-GATT: reusing the USB HID model over BLE

The HID Service (`0x1812`) is a standardized GATT profile that wraps the
same model USB HID has used for decades: a **Report Map** (the HID Report
Descriptor — the same byte language a USB gamepad advertises) plus one GATT
characteristic per **Report** (Input/Output/Feature), each tagged with a
Report Reference descriptor stating its `(Report ID, Report Type)`. That's
exactly what [BleGamepad.cpp](BleGamepad.cpp)'s `begin()` builds into
`tempHidReportDescriptor` and hands to `hid->setReportMap()`.

Because HOGP is a *standard* profile, the host's own Bluetooth stack
recognizes it and claims it on the OS's behalf:

- **Linux**: BlueZ's input plugin bridges it into the kernel via `uhid`,
  producing `/dev/hidraw*` and `/dev/input/js*`/`event*` nodes.
- **macOS**: `IOBluetoothHIDDriver` bridges it into IOKit's HID Manager,
  same as a USB HID device.
- **Windows**: the HID class driver claims it, same as USB.

This is why a generic GATT client (`bleak`, `gatttool`,
`bluetoothctl`'s `gatt` menu) can see this device's Battery/Device
Information services but **not** its HID Report characteristics once
paired — the OS has already claimed that service for itself, on purpose (see
["Why not just use a BLE GATT library"](LinuxHIDTesting.md#why-not-just-use-a-ble-gatt-library-bleak-gatttool-etc)
in LinuxHIDTesting.md for the full security-boundary rationale). The only
supported way in is through the OS's own HID/joystick API — `hidraw`/
`hidapi` on Linux, `IOHIDManager` on macOS, `HidD_*`/`WGI` on Windows — which
is also exactly what a real game uses.

## Where SDL fits

SDL never speaks BLE or GATT on any platform — `SDL_Joystick`/
`SDL_GameController` sit *above* the OS's native controller stack, not
beside it:

- **Linux**: SDL's `hidapi` joystick backend opens the same `/dev/hidraw*`
  node this library's [LinuxHIDTesting.md](LinuxHIDTesting.md) walks through
  by hand, plus a separate `evdev` backend for `/dev/input/event*`.
- **macOS**: SDL goes through `IOHIDManager`, the same one macOS's Bluetooth
  stack bridged the HID Service into.
- **Windows**: SDL uses `hidapi`/`XInput`/`WGI` depending on how the device
  identifies itself.

Because this device's Report Descriptor uses standard gamepad/joystick usage
pages, the OS's stack recognizes it as a real controller and SDL enumerates
it exactly like a wired one — no BLE-specific code needed for buttons/axes.

Where it gets less automatic is **rumble and LEDs**. SDL's
`SDL_GameControllerRumble()`/`SDL_GameControllerSetLED()` only work for a
device if *something in the stack* — SDL's `hidapi` controller drivers (the
`SDL_hidapi_joystick.c` family: PS4, PS5, Switch Pro, Xbox, etc.), a kernel
force-feedback driver (`hid-sony`, `hid-microsoft`, ...), or SDL's own
`gamecontrollerdb.txt` mapping — recognizes this device's specific VID/PID
and knows the exact byte layout of its Output/Feature Reports. A
`hid-generic`-bound device with a bespoke report layout gets none of that
for free: the kernel will happily carry the bytes, but nothing translates
"set player LED 2" into a `SDL_GameControllerSetLED()` call unless a driver
was written that knows this device's report format specifically. This is
precisely the gap community protocols like SInput are trying to close —
define a well-known vendor Feature/Output Report layout (capability
bitmask, LED/rumble command bytes) so a **userspace** layer (a custom SDL
`hidapi` driver, or an app talking `hidraw` directly, the same way
[LinuxHIDTesting.md](LinuxHIDTesting.md#6-feature--output--input-reports-via-python-hidapi)'s
Python example does) can drive it, instead of waiting on kernel driver
support per device.

## Extending capabilities: two paths, different reachability

This library already has two mechanisms for anything beyond plain
buttons/axes — they are not interchangeable, because they're reachable from
different software on the host:

### A. Inside HID-over-GATT (Output/Feature Report bytes)

Add more bytes to the existing Output Report (host → device: rumble motor
strength, player LED index, RGB values) and/or Feature Report (bidirectional:
device advertises which of those it supports). `BleFeatureReceiver::
buildFeatureReport()` in [BleFeatureReport.cpp](BleFeatureReport.cpp) already
does this for a capability bitmask (`FEAT_CAP_RUMBLE`, `FEAT_CAP_PLAYERLED`,
etc., modeled on SInput's `Features 0x02` report) — the same idea extends
naturally to actual LED/rumble *command* bytes in the Output Report.

- Reachable from: any app already reading this device's Input Report via
  `hidraw`/`hidapi` — i.e. exactly the kind of code a game or an SDL
  `hidapi` driver runs. No second connection, no extra pairing.
- Not reachable from: SDL's generic `evdev`/`IOHIDManager`-only path, or any
  plain OS joystick API that only surfaces axes/buttons — those never
  expose Feature Reports at all. The app has to drop to `hidapi`/`hidraw`
  directly (or SDL needs a driver purpose-built for this VID/PID) to reach
  it.
- Linux specifics: there is no generic "set gamepad LED color" kernel API
  for an arbitrary `hid-generic` device — the `/sys/class/leds/...` entries
  you may have seen for a DualShock 4 exist because `hid-playstation` is a
  dedicated kernel driver for that specific device, not something
  `hid-generic` provides. Absent a matching kernel driver, this path only
  works via a userspace app writing the Output Report through `hidraw`
  directly (see [LinuxHIDTesting.md](LinuxHIDTesting.md) step 6). The same
  applies to rumble via the kernel's Force Feedback (`EV_FF`) API — it needs
  a driver that translates FF effects into this device's specific Output
  Report bytes; without one, `hid-generic` won't expose an `EV_FF` capable
  `/dev/input/eventN` at all.

### B. Outside HID entirely (NUS)

`beginNUS()` opens a second, ordinary GATT service with no HID semantics —
free-form bytes, no Report Descriptor, no Report ID framing.

- Reachable from: any general-purpose GATT client — a companion
  configuration app (mobile app, `bleak` script, etc.) connecting directly
  over BLE.
- Not reachable from: SDL, `hidapi`, or any OS input/joystick API — the
  same input-plugin claiming described above means BlueZ's regular
  `org.bluez.GattService1` D-Bus objects, which `bleak`/NUS clients rely on,
  do not include the HID service, but NUS itself is unaffected and shows up
  normally since it isn't part of that claimed service. A game reading
  input via `hidraw` has no path to NUS at all — it would need its own,
  separate GATT connection to this device alongside the HID one, which most
  game engines have no support for.

### Picking one

If RGB/player LEDs or rumble need to be driven **from the same app that
already reads input** (a game calling `SDL_GameControllerSetLED()`, or a
custom `hidapi` driver) — extend the Output/Feature Report (path A). That's
the only path visible from that code.

If it's configuration/telemetry meant for a **separate companion app**
(calibration, firmware info, arbitrary logging) that doesn't need to be
synchronized with game input timing — NUS (path B) is simpler, since it has
no report-length/ID constraints and doesn't require touching the HID Report
Descriptor at all.

Nothing stops using both for different features simultaneously — e.g.
rumble/player-LED-index as Output Report bytes (so games can drive them),
while a companion app uses NUS for one-time RGB color calibration or
firmware updates.

## Architecture summary

```
Game / App
     |
     |  SDL_GameController*() / SDL_Joystick*()
     v
 OS input stack           (evdev+hidraw on Linux, IOHIDManager on macOS,
     |                     HID class driver + XInput/WGI on Windows)
     |
     |  claims this on connect, standard HOGP behavior
     v
 HID Service (0x1812)  <-----------------  BLE  <-----  ESP32 (BleGamepad)
 Input / Output / Feature Report
 (buttons, axes, rumble bytes,
  LED bytes, capability bitmask)


 Companion / config app
     |
     |  general-purpose GATT client (bleak, custom app)
     v
 NUS Service (custom UUID)  <-------------  BLE  <-----  ESP32 (BleNUS)
 free-form bytes, no HID framing
```

The top path is what any existing game/SDL code already reaches without
modification once a report layout is defined for it. The bottom path
requires a purpose-built companion app, but has none of the HID
Report Descriptor's constraints.
