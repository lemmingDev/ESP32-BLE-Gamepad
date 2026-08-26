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
[LinuxHIDTesting.md](../../LinuxHIDTesting.md) steps 1-4 (pairing via
`bluetoothctl`, confirming the `/dev/hidraw*`/`/dev/input/js*` nodes appear,
and the `hidraw` udev rule so you don't need `sudo` for step 5). Everything
there applies unchanged — SInput mode is still HID-over-GATT underneath, see
[GattVsHid.md](../../GattVsHid.md).

One thing specific to SInput mode: `bleGamepadConfig.setEnableSInput(true)`
switches the advertised VID/PID to `0x2E8A`/`0x10C6` (SDL's hardcoded SInput
allowlist requires that exact pair — see `BleGamepadConfiguration.h`). If
you'd previously paired this same board while it was running different
firmware (a different VID/PID, or a different example), remove the old
bonding first — `bluetoothctl remove <address>` — then re-pair, or BlueZ/SDL
may still be looking at stale cached info for the old identity.

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

## What to expect

```
Waiting for a gamepad (pair/connect the ESP32 now if it isn't already)...
Opened: ESP32 BLE Gamepad (VID=0x2E8A PID=0x10C6) -- expect VID=0x2E8A PID=0x10C6 for SInput mode
Rumble capable: false (expected false -- not implemented by this library yet, see GattVsHid.md)
-> SDL_SetGamepadPlayerIndex(0)
Button 0: down
Battery: 25% (on battery)
Button 0: up
-> SDL_SetGamepadPlayerIndex(1)
Battery: 26% (on battery)
...
```

- **VID/PID** should read `0x2E8A`/`0x10C6` — if it shows this library's
  usual default (`0xE502`/`0xBBAB`) instead, `setEnableSInput(true)` isn't
  taking effect (check it's called before `begin()`, and that you're not
  also calling `setVid()`/`setPid()` afterwards and overriding it).
- **Button 0** toggles roughly once a second, tracking BUTTON_1 in the
  sketch.
- **Battery** climbs from 25 to 90 and back down over about 40 seconds, `on
  battery` throughout (the sketch never reports charging).
- Every 3 seconds the test program itself calls
  `SDL_SetGamepadPlayerIndex()`, cycling 0→1→2→3→0... — watch the ESP32's
  own Serial Monitor at the same time: you should see `Player LED index: N`
  print each time, staying in sync with the values this program prints.
- **Rumble capable** should read `false` — this library's SInput support
  doesn't drive a rumble motor yet, and the Features response says so
  honestly (see [GattVsHid.md](../../GattVsHid.md)'s note on this).

## Troubleshooting

**VID/PID looks right but the test program never finds a gamepad at all**
- `SDL_GetGamepads()` only returns devices SDL's gamepad layer recognizes,
  which is a stricter check than "the OS sees a joystick" (LinuxHIDTesting.md's
  `jstest`/`evtest` steps can pass while this still doesn't). Re-check step
  1 — `pkg-config --modversion sdl3` printing `3.2.x` here is the most
  common cause, since 3.2.x has no SInput driver and this device's VID/PID
  isn't on its list of pre-3.4 hidapi drivers either.
- Confirm the hint from step 2 actually reached the process: SDL reads
  `SDL_JOYSTICK_HIDAPI_SINPUT` from the environment at `SDL_Init()` time, so
  `export` it in the *same* shell before running (or `sudo -E` if running
  under `sudo`, which drops the environment by default).

**Found it, but VID/PID reads as `0xE502`/`0xBBAB` (or whatever you'd
customized) instead of `0x2E8A`/`0x10C6`**
- `setEnableSInput(true)` wasn't the last VID/PID-affecting call before
  `begin()` — see the note in step 3 above.

**Buttons/axes work, but Player LED never reaches the device (no Serial
log line on the ESP32)**
- Confirm `sInputOutputGamepad`/`BleSInputReceiver::onWrite()` are actually
  being hit — enable `BLE_GAMEPAD_DEBUG` (see the main README) for extra
  Serial diagnostics on the firmware side, and run
  `sudo btmon -i hci0` (see [LinuxHIDTesting.md](../../LinuxHIDTesting.md#monitoring-bluetooth-traffic))
  in a second terminal to confirm the write is actually reaching the ATT
  layer at all, rather than being dropped somewhere in SDL/BlueZ.
