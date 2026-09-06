# CI test firmware

A self-contained firmware that builds this library and exercises it at runtime,
with no wiring and no extra libraries. It exists so you can flash a known-good
device and confirm the library works before chasing a build- or wiring-specific
problem.

## What it does

`src/main.cpp` advertises as **`ESP32 BLE Gamepad Test`** and brings up the full
BLE profile the library offers:

- the **gamepad HID** service (24 buttons, the right thumbstick, one unused hat
  switch; everything else left out to keep the HID descriptor small);
- every **Device Information Service** characteristic, set to an identifiable
  value (model / serial number / firmware / hardware / software revision — the
  Software Revision carries the build id);
- the **Nordic UART Service (NUS)** — on connect it sends a greeting, then a
  status line every ~3 s, and answers `help` / `status` (anything else is echoed
  back). Use nRF Connect or any BLE UART terminal.

Once a host connects it, on its own:

- presses one button at a time, cycling a range **well above `BUTTON_16`**
  (`FIRST_TEST_BUTTON`..`LAST_TEST_BUTTON` in `src/main.cpp`). Buttons `1..16`
  are skipped on purpose — Android maps them to named controls (A/B/X/Y,
  shoulders/triggers, start/select, stick clicks), so auto-pressing them drives
  UI navigation on the host. Buttons `17+` land on Android's generic
  `KEYCODE_BUTTON_*` range that nothing acts on;
- sweeps the **right thumbstick** (Z / RZ) around a circle, kept under ~50%
  deflection (right stick not left, small throw, so it doesn't move Android's
  on-screen focus);
- ramps the battery level `100% -> 10% -> 100%` on a ~5 s tick, reporting
  **discharging** while it falls and **charging** while it rises, with a
  **critical** power level at or below 20%.

Progress is printed to serial at `115200` baud. The Software Revision
characteristic reports the exact build, e.g.
`ESP32-BLE-Gamepad 0.7.5-rc0+g05599be` (set by `inject_version.py` from
`library.properties` + git).

## Watching the serial log — no local tools

<https://www.serialmonitor.org> is a browser serial monitor. Click **Connect**,
pick the board's port, and set **baud `115200`**.

It uses the **Web Serial API**, so it needs a Chromium-based desktop browser —
Chrome, Edge or Opera — the same requirement as
<https://espressif.github.io/esptool-js>. Firefox and Safari don't implement Web
Serial.

| OS | Port looks like |
| --- | --- |
| macOS | `/dev/cu.usbserial-XXXX` (CH340/CP210x boards) or `/dev/cu.usbmodemXXXX` (native-USB S3/C3) |
| Linux | `/dev/ttyUSB0` (CH340/CP210x) or `/dev/ttyACM0` (native USB) |
| Windows | `COM3`, `COM4`, … (see Device Manager) |

With a toolchain, `pio device monitor -e esp32dev` or the Arduino IDE Serial
Monitor at `115200` do the same job. Close any other monitor before flashing —
only one program can hold the port.

## Building locally

```
cd test/ci_build
pio run -e esp32dev        # or esp32s3, esp32c3
```

Each build produces, in `.pio/build/<env>/`:

- `firmware.bin` — application image (flash at the board's app offset), and
- `firmware-factory.bin` — bootloader + partitions + app merged into one image,
  flashable at offset `0x0` (see `merge_firmware.py`).

## Getting a prebuilt binary (no toolchain)

`.github/workflows/platformio.yml` builds all three boards on every push/PR and
uploads a `test-firmware-<board>` artifact (both `.bin` variants). Tagged
releases also get the binaries attached (and release assets need no login).

**Build any branch/PR/tag on demand:**

1. Fork this repo (or use it directly if you have write access) and enable
   Actions on the fork.
2. Actions tab → **ESP32-BLE-Gamepad platformio CI** → **Run workflow**.
3. Optionally set **ref** to the branch, tag or SHA to build (a PR is
   `refs/pull/<number>/head`); leave blank to build the branch you launch from.
4. When the run finishes, download the `test-firmware-<board>` artifact from it.

Downloading Actions artifacts requires a (free) GitHub login; release assets do
not.

## Flashing it

Grab `test-firmware-<board>-factory.bin` and either:

```
esptool.py --chip <esp32|esp32s3|esp32c3> write_flash 0x0 test-firmware-<board>-factory.bin
```

or drag it onto <https://espressif.github.io/esptool-js/> with flash address
`0x0`.
