# Testing SInputPlayerLED.ino with SDL3 on Linux

This walks through verifying SInputPlayerLED.ino end-to-end against a real
SDL3 app on Linux: buttons/axes, Player LED, and the battery ramp added in
this example — using [sdl3_gamepad_test.c](sdl3_gamepad_test.c), a small
standalone test program in this directory.

## 1. Get an SDL3 build that actually has the SInput driver

The SInput `hidapi` driver landed in SDL3 via
[libsdl-org/SDL#13343](https://github.com/libsdl-org/SDL/pull/13343). As of
this writing, Debian/Ubuntu's `libsdl3-dev` package (stable and testing/
unstable alike) is still on the 3.2.x series — no SInput driver. You need
3.4.x or newer. Check what you've got first:

```bash
pkg-config --modversion sdl3   # if this prints 3.2.x (or errors: not installed), build from source below
```

If your distro happens to already carry ≥3.4.x (check
`apt-cache policy libsdl3-dev`, or Debian *experimental* which had `3.4.14`
as of August 2026), a regular package install is fine and you can skip to
[step 2](#2-enable-the-sinput-hidapi-driver):

```bash
sudo apt install libsdl3-dev
```

Otherwise, build it from source:

```bash
sudo apt install -y build-essential cmake git \
  libasound2-dev libpulse-dev libudev-dev libdbus-1-dev \
  libgl1-mesa-dev libwayland-dev libxkbcommon-dev \
  libx11-dev libxext-dev libxrandr-dev libxi-dev libxss-dev libxcursor-dev

git clone --branch release-3.4.14 --depth 1 https://github.com/libsdl-org/SDL.git ~/src/SDL3
cmake -S ~/src/SDL3 -B ~/src/SDL3/build -DCMAKE_BUILD_TYPE=Release
cmake --build ~/src/SDL3/build -j"$(nproc)"
sudo cmake --install ~/src/SDL3/build
sudo ldconfig
```

(swap `release-3.4.14` for whatever the current 3.4.x tag is if that one's
gone stale — `git ls-remote --tags https://github.com/libsdl-org/SDL.git` to
check)

Confirm it worked:

```bash
pkg-config --modversion sdl3
```

## 2. Enable the SInput `hidapi` driver

It should be on by default (it follows `SDL_JOYSTICK_HIDAPI`, which
defaults on), but set it explicitly so a disabled default elsewhere on the
system doesn't cost you an hour of confusion:

```bash
export SDL_JOYSTICK_HIDAPI_SINPUT=1
```

## 3. Pair the ESP32, same as any other test in this library

If it isn't already paired/trusted/connected, follow
[LinuxHIDTesting.md](../../LinuxHIDTesting.md) steps 1-5 unchanged — SInput
mode is still HID-over-GATT underneath (see
[GattVsHid.md](../../GattVsHid.md)), and it registers as a completely normal
Linux joystick too: `/dev/input/js*` appears, `/proc/bus/input/devices`
shows a real entry, and `jstest --normal /dev/input/jsN` reports 6 axes and
32 buttons, live. That's not incidental — `BleGamepad.cpp`'s SInput branch
deliberately gives Input Report 0x01's buttons/axes fields real HID usages
(Button page, Generic Desktop X/Y/Z/Rz/Rx/Ry) over the exact same bytes SDL
reads by fixed offset, specifically so both paths work off one descriptor.
Only Reports 0x02/0x03 (SInput's own command/feature-response and
output-command reports) stay opaque — nothing outside SDL's SInput driver
(or an app speaking the same protocol) needs to read those.

Two things specific to SInput mode:

- `bleGamepadConfig.setEnableSInput(true)` switches the advertised VID/PID to
  `0x2E8A`/`0x10C6` (SDL's hardcoded SInput allowlist requires that exact
  pair — see `BleGamepadConfiguration.h`). If you'd previously paired this
  same board while it was running different firmware (a different VID/PID,
  or a different example), remove the old bonding first —
  `bluetoothctl remove <address>` — then re-pair.
- If that re-pair fails with `org.bluez.Error.AuthenticationFailed` (check
  with `sudo btmon -i hci0` in a second terminal — you'll see
  `SMP: Pairing Failed (0x05)  Reason: Passkey entry failed`), removing the
  bond from the *host* side isn't enough: the ESP32 still has its half of the
  old bond in flash, and the two sides' keys no longer agree. Fully erase the
  board's flash before reflashing (`esptool.py --port <port> erase_flash`,
  or `pio run -t erase` under PlatformIO) rather than just re-uploading —
  this is the same underlying issue as
  [TroubleshootingGuide.md](../../TroubleshootingGuide.md)'s "Configuration
  Changes Not Taking Effect" entry, just also needing the ESP32 side cleared,
  not only the host's.

## 4. Build and run the test program

No display needed — this only calls `SDL_Init(SDL_INIT_GAMEPAD)`, so it runs
fine over SSH on a headless box:

```bash
gcc sdl3_gamepad_test.c -o sdl3_gamepad_test $(pkg-config --cflags --libs sdl3)
./sdl3_gamepad_test
```

(if you built SDL3 from source into a non-standard prefix and `pkg-config`
can't find it: `PKG_CONFIG_PATH=/usr/local/lib/pkgconfig gcc ...`, adjusting
the path to wherever `sdl3.pc` landed)

Pass `-v`/`--verbose` to also enable `SDL_LOG_CATEGORY_INPUT` at
`SDL_LOG_PRIORITY_VERBOSE` (SDL3's dedicated log category for the
joystick/gamepad/hidapi subsystem):

```bash
./sdl3_gamepad_test -v
```

This is a runtime flag, not a rebuild, so it can't reach logging gated behind
the SInput driver's own `DEBUG_SINPUT_INIT`/`DEBUG_SINPUT_PROTOCOL` macros
(those are compiled out entirely unless you edit
`src/joystick/hidapi/SDL_hidapi_sinput.c` and rebuild SDL) — but it does
surface whatever SDL already logs at debug/verbose priority elsewhere in the
hidapi/joystick stack, at no cost beyond the flag.

There's also [sinput_hid_test.py](sinput_hid_test.py) in this directory,
which talks the same protocol directly over `hidraw`, bypassing SDL
entirely — useful for isolating a firmware bug from an SDL/driver-layer one.
If you followed step 3 above via
[LinuxHIDTesting.md](../../LinuxHIDTesting.md)'s steps 1-5, its step 2
already set you up a Python venv with the `hid` package installed, so you
can run it right away:

```bash
python3 -m venv .venv
.venv/bin/pip install hid
sudo .venv/bin/python sinput_hid_test.py
```

(`sudo` unless you've set up the `hidraw` udev rule from
[LinuxHIDTesting.md](../../LinuxHIDTesting.md) step 4)

## What to expect

```
SDL runtime version: 3.4.14 -- must be 3.4.x+ for the SInput driver, see SDL3Testing.md step 1
Waiting for a gamepad (pair/connect the ESP32 now if it isn't already)...
Opened: ESP32 BLE Gamepad (VID=0x2E8A PID=0x10C6) -- expect VID=0x2E8A PID=0x10C6 for SInput mode
Underlying device path: /dev/hidraw2 (cross-reference with sinput_hid_test.py --device <path>, or a concurrent `sudo btmon -i hci0`)
Rumble capable: false (expected false -- not implemented by this library yet, see GattVsHid.md)
Player LED capable (SDL's view): true -- expected true; false means SDL will silently drop every SDL_SetGamepadPlayerIndex() call below
Button 0: down
Battery: 25% (on battery)
Button 0: up
-> SDL_SetGamepadPlayerIndex(1) succeeded
Battery: 26% (on battery)
...
```

- **VID/PID** should read `0x2E8A`/`0x10C6` — if it shows this library's
  usual default (`0xE502`/`0xBBAB`) instead, `setEnableSInput(true)` isn't
  taking effect (check it's called before `begin()`, and that you're not
  also calling `setVid()`/`setPid()` afterwards and overriding it).
- **Underlying device path** is the `hidraw` node SDL opened for this
  gamepad — hand it straight to `sinput_hid_test.py --device <path>` (see
  below) to test the same connection at the raw protocol level, or watch for
  it in a concurrent `sudo btmon -i hci0` capture.
- **Player LED capable (SDL's view)** is `SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN`
  — this is the public-API window onto the internal `player_leds_supported`
  flag. It should read `true`; if it prints `false`, see Troubleshooting
  below.
- **Button 0** toggles roughly once a second, tracking BUTTON_1 in the
  sketch.
- **Battery** climbs from 25 to 90 and back down over about 40 seconds, `on
  battery` throughout (the sketch never reports charging).
- Every 3 seconds the test program itself calls
  `SDL_SetGamepadPlayerIndex()`, cycling 0→1→2→3→0..., and prints
  `succeeded`/`FAILED` for each call — watch the ESP32's own Serial Monitor
  at the same time for a matching `Player LED index: N` line. If it doesn't
  show up, see Troubleshooting below.
- **Rumble capable** should read `false` — this library's SInput support
  doesn't drive a rumble motor yet, and the Features response says so
  honestly (see [GattVsHid.md](../../GattVsHid.md)'s note on this).

## Troubleshooting

**The test program never finds a gamepad at all, and raw hidapi doesn't see
it either (see below)**
- Re-check step 1 — `pkg-config --modversion sdl3` printing `3.2.x` here is
  the most common cause, since 3.2.x has no SInput driver at all.
- Confirm the hint from step 2 actually reached the process: SDL reads
  `SDL_JOYSTICK_HIDAPI_SINPUT` from the environment at `SDL_Init()` time, so
  `export` it in the *same* shell before running (or `sudo -E` if running
  under `sudo`, which drops the environment by default).
- To check whether hidapi sees the device at all, independent of SDL's
  gamepad-layer VID/PID matching, a couple of lines of `SDL_hid_init()` +
  `SDL_hid_enumerate(0, 0)` (see `<SDL3/SDL_hidapi.h>`) will list every HID
  device hidapi can see, with vendor/product IDs — a much smaller surface to
  debug than the full gamepad stack. If that also comes back empty, see the
  next entry.

**Raw hidapi enumeration finds nothing at all (not just this device)**
- This is likely a permissions issue on `/dev/hidraw*` itself, not an
  SDL/hidapi-specific one — see the `hidraw` udev rule in
  [LinuxHIDTesting.md](../../LinuxHIDTesting.md).

**Found it, but VID/PID reads as `0xE502`/`0xBBAB` (or whatever you'd
customized) instead of `0x2E8A`/`0x10C6`**
- `setEnableSInput(true)` wasn't the last VID/PID-affecting call before
  `begin()` — see the note in step 3 above.

**Buttons/axes work, but Player LED never reaches the device (no Serial
log line on the ESP32), even though `sdl3_gamepad_test` prints
`SDL_SetGamepadPlayerIndex(...) succeeded`, and "Player LED capable (SDL's
view)" prints `false`**
- Run [sinput_hid_test.py](sinput_hid_test.py) against the same connection
  and compare its Features response dump (specifically the `PLAYERLED`
  capability bit) against `sdl3_gamepad_test.c`'s "Player LED capable (SDL's
  view)" line. If they disagree, `BleSInput.h`'s `SINPUT_FEAT_IDX_*` offsets
  no longer match SDL's actual `src/joystick/hidapi/SDL_hidapi_sinput.c` —
  diff that file at your SDL version's tag against `BleSInput.h`'s offsets
  and correct them to match.
