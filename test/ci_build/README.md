# CI test firmware

A self-contained firmware that builds this library and exercises it at runtime,
with no wiring and no extra libraries. It exists so you can flash a known-good
device and confirm the library works before chasing a build- or wiring-specific
problem (issue #342).

## What it does

`src/main.cpp` advertises as **`ESP32 BLE Gamepad Test`** and brings up the full
BLE profile the library offers:

- the **gamepad HID** service (8 buttons + one unused hat switch; axes disabled
  to keep the HID descriptor small);
- every **Device Information Service** characteristic, set to an identifiable
  value (model / serial number / firmware / hardware / software revision);
- the **Nordic UART Service (NUS)** — on connect it sends a greeting, then a
  status line every ~3 s, and answers `help` / `status` (anything else is echoed
  back). Use nRF Connect or any BLE UART terminal.

Once a host connects it, on its own:

- presses one button every ~250 ms, cycling **`BUTTON_3..BUTTON_8`**.
  `BUTTON_1` / `BUTTON_2` are skipped on purpose — they map to A / B
  (accept / back) on Android, so auto-pressing them would trigger UI navigation
  on the host;
- steps the battery level down `100% -> 0%` (then wraps back to `100%`) every
  ~5 s, reporting **discharging / on battery** above 30% and **charging /
  plugged in** at or below it, and a **critical** power level at or below 10%.

Progress is printed to serial at `115200` baud. The Software Revision
characteristic reports the exact build, e.g.
`ESP32-BLE-Gamepad 0.7.5-rc0+g05599be` (set by `inject_version.py` from
`library.properties` + git).

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
