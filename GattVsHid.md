# GATT vs HID-over-GATT, and How Extra Features (RGB/Player LEDs, Rumble) Get to an App

This library exposes the ESP32 over BLE in two distinct ways at once, and
mixing them up is the source of most confusion about "why doesn't my game
see the LED/rumble feature I added":

1. **HID-over-GATT (HOGP)** — the standard Bluetooth SIG profile that makes
   this device look like a real gamepad/joystick to the OS. This is what
   `BleGamepad`'s buttons/axes (Input Report), `setFeatureBuffer()`/
   `getFeatureBuffer()` (Feature Report), `getOutputBuffer()` (Output
   Report), and SInput mode (`setEnableSInput()`) all ride on.
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
precisely the gap community protocols like [SInput](https://github.com/HandHeldLegend/SInput-HID)
are trying to close — define a well-known report layout (capability bitmask,
LED/rumble command bytes) so a **userspace** layer (a custom SDL `hidapi`
driver, or an app talking `hidraw` directly, the same way
[LinuxHIDTesting.md](LinuxHIDTesting.md#6-feature--output--input-reports-via-python-hidapi)'s
Python example does) can drive it, instead of waiting on kernel driver
support per device. SDL's own `hidapi` driver for it landed as
[libsdl-org/SDL#13343](https://github.com/libsdl-org/SDL/pull/13343), which
is what turns a SInput-shaped device into a normal
`SDL_GameController` with rumble/LED support, no per-VID/PID driver of our
own required — see the [References](#references) section below.

One easy assumption to get wrong here (this library did, briefly, in an
earlier prototype on this branch): SInput does **not** use the BLE HID
Feature Report at all. Its reference driver (`SDL_hidapi_sinput.c`)
never calls `hid_get_feature_report()`/`hid_send_feature_report()` — it
multiplexes everything through Input and Output Reports only, split by
Report ID: Input Report `0x01` (regular gamepad state), Input Report `0x02`
(a command/feature-response report, reusing the *Input* report type), and
Output Report `0x03` (every host → device command — haptics, a "send me your
features" request, Player LED, RGB — dispatched by a sub-command byte). If
you're extending this yourself against the real spec, byte-match the
reference driver, not just the capability-bitmask doc page — see
[References](#references).

## Extending capabilities: three paths, different reachability

This library has three mechanisms for anything beyond plain buttons/axes —
they are not interchangeable, because they're reachable from different
software on the host, and (for the first two) because they claim the same
Report IDs and can't run at the same time.

### A. SInput mode (`setEnableSInput(true)`)

`configuration.setEnableSInput(true)` — see [BleSInput.h](BleSInput.h)/[BleSInput.cpp](BleSInput.cpp) —
replaces this library's usual Input Report entirely with SInput's fixed
Input `0x01`/`0x02` + Output `0x03` layout described above, and switches the
advertised VID/PID to the exact pair (`0x2E8A`/`0x10C6`) SDL's hardcoded
SInput allowlist requires. It's mutually exclusive with
`setEnableOutputReport()`/`setEnableFeatureReport()` (SInput owns those
Report IDs outright — `begin()` just doesn't build the classic descriptor
when SInput is on).

This library's SInput support today implements: real buttons/axes on Input
Report `0x01` (mapped from the buttons/axes you already configure), a
correct Features response on `0x02`, and Player LED handling on Output
`0x03` — poll it with `bleGamepad.isPlayerLedReceived()` /
`bleGamepad.getPlayerLedIndex()`, demonstrated in
[examples/SInputPlayerLED](examples/SInputPlayerLED/SInputPlayerLED.ino).
Haptics and RGB commands are accepted (the write succeeds) but not acted on
— there's no rumble motor or RGB LED driven yet; see
`BleSInputReceiver::onWrite()` for where to add one.

Reachability actually splits in two, because Input Report `0x01`'s
buttons/axes fields carry real HID usages (Button page, Generic Desktop
X/Y/Z/Rz/Rx/Ry) over the same bytes SDL reads by fixed offset — deliberately,
so both consumers work off one descriptor — while Reports `0x02`/`0x03`
(features/Player LED/haptics/RGB) don't:

- **Buttons/axes** — reachable from *both* a real SInput host (an SDL3 app
  with `SDL_HINT_JOYSTICK_HIDAPI_SINPUT` enabled) *and* the plain OS
  joystick path: `/dev/input/js*`, `jstest`, `evtest`, anything using evdev,
  no SInput awareness required at all.
- **Player LED / features negotiation / haptics / RGB** — reachable only
  from SInput-aware software: SDL's SInput driver, or an app talking
  `hidraw`/`hidapi` directly using the same report layout (see
  [LinuxHIDTesting.md](LinuxHIDTesting.md#6-feature--output--input-reports-via-python-hidapi)
  for the general pattern). Reports `0x02`/`0x03` have no field-level HID
  usages, so evdev/`jstest`/plain joystick APIs never see them — no second
  connection needed to reach them, but a generic joystick API has no
  concept of "send a Player LED command" to expose in the first place.

### B. Extending the classic Output/Feature Report yourself

If you're not targeting SInput specifically, the library's older, more
generic mechanism still exists independently of SInput mode: add bytes to
the configurable Output Report (host → device) and/or Feature Report
(bidirectional) via `setEnableOutputReport()`/`setEnableFeatureReport()`.
`BleFeatureReceiver::buildFeatureReport()` in
[BleFeatureReport.cpp](BleFeatureReport.cpp) already does this for a
capability bitmask happening to reuse SInput's bit layout — but to be clear,
**this is a different, non-SInput mechanism**: it rides on the GATT Feature
Report characteristic, which SDL's SInput driver never reads (see the note
above). It's still useful for a custom app that talks `hidraw`/`hidapi`
directly against its own protocol, just not for SDL's SInput recognition.

- Reachable from: any app already reading this device's Input Report via
  `hidraw`/`hidapi` — i.e. exactly the kind of code a game or a custom
  `hidapi` driver runs. No second connection, no extra pairing.
- Not reachable from: SDL's generic `evdev`/`IOHIDManager`-only path, or any
  plain OS joystick API that only surfaces axes/buttons — those never
  expose Feature Reports at all. The app has to drop to `hidapi`/`hidraw`
  directly to reach it.
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

### C. Outside HID entirely (NUS)

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

If you want an existing SDL3 game to recognize this device natively —
buttons/axes, and Player LED today — use SInput mode (path A). It's the only
path a stock SDL build (with the SInput hint enabled) understands without
you writing any host-side code at all.

If you're shipping your own host-side app anyway and don't need SDL's
native recognition, extending the classic Output/Feature Report yourself
(path B) works the same way, minus the VID/PID and Report ID constraints
SInput imposes.

If it's configuration/telemetry meant for a **separate companion app**
(calibration, firmware info, arbitrary logging) that doesn't need to be
synchronized with game input timing — NUS (path C) is simpler, since it has
no report-length/ID constraints and doesn't require touching the HID Report
Descriptor at all, and it can run alongside either A or B.

## Architecture summary

```
Game / App (SDL3, SInput hint on)
     |
     |  SDL_GameController*() / SDL_Joystick*()
     v
 OS input stack           (evdev+hidraw on Linux, IOHIDManager on macOS,
     |                     HID class driver + XInput/WGI on Windows)
     |
     |  claims this on connect, standard HOGP behavior
     v
 HID Service (0x1812)  <-----------------  BLE  <-----  ESP32 (BleGamepad)
 EITHER (mutually exclusive, chosen by setEnableSInput()):
   SInput: Input 0x01/0x02 + Output 0x03      (BleSInput.h/.cpp)
   OR the classic report:  Input/Output/Feature, one shared Report ID
                                                (BleFeatureReport.cpp etc.)


 Companion / config app
     |
     |  general-purpose GATT client (bleak, custom app)
     v
 NUS Service (custom UUID)  <-------------  BLE  <-----  ESP32 (BleNUS)
 free-form bytes, no HID framing -- can run alongside either option above
```

The top path is what an SInput-aware game already reaches without any
device-specific code, once SInput mode is enabled. The bottom path requires
a purpose-built companion app, but has none of the HID Report Descriptor's
constraints.

## References

- [HandHeldLegend/SInput-HID](https://github.com/HandHeldLegend/SInput-HID) —
  the protocol's reference repo: supported devices, and links to the spec.
- [SInput HID protocol spec](https://docs.handheldlegend.com/s/sinput/doc/sinput-hid-protocol-TkPYWlDMAg) —
  full report layout (buttons, gyro/accel, haptics, player LEDs, touchpads).
- [SInput "Features" (0x02) response bytes](https://docs.handheldlegend.com/s/sinput/doc/features-response-bytes-1lMp7WL7bq) —
  the capability bitmask `BleFeatureReceiver::buildFeatureReport()` in
  [BleFeatureReport.cpp](BleFeatureReport.cpp) is modeled on.
- [libsdl-org/SDL#13343 — Implement SInput Device Support](https://github.com/libsdl-org/SDL/pull/13343) —
  the SDL `hidapi` driver that recognizes SInput-shaped devices, turning
  them into a normal `SDL_GameController` with rumble/LED support without a
  per-VID/PID driver.
- [lemmingDev/ESP32-BLE-Gamepad#288](https://github.com/lemmingDev/ESP32-BLE-Gamepad/issues/288) —
  the feature request that started this library's SInput work.
- [BleSInput.h](BleSInput.h)/[BleSInput.cpp](BleSInput.cpp) — this library's
  SInput implementation, with every report offset documented against the
  reference driver's (ID-prefixed) indices.
- [examples/SInputPlayerLED](examples/SInputPlayerLED/SInputPlayerLED.ino) —
  a basic SInput sketch using the onboard LED to show the Player LED index,
  with a battery ramp to test against too.
- [examples/SInputPlayerLED/host_test/SDL3Testing.md](examples/SInputPlayerLED/host_test/SDL3Testing.md) —
  testing that sketch end-to-end against a real SDL3 app on Linux, including
  a ready-to-run test program.
