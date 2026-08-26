# Testing SInputPlayerLED.ino with SDL3 on Linux

This walks through verifying SInputPlayerLED.ino end-to-end against a real
SDL3 app on Linux: buttons/axes, Player LED, and the battery ramp added in
this example — using [sdl3_gamepad_test.c](sdl3_gamepad_test.c), a small
standalone test program in this directory.

This exact flow — real ESP32 hardware, SDL3 3.4.14 built from source, tested
over SSH — is what shook out the two real bugs the "Troubleshooting" and
"Two things specific to SInput mode" sections below describe; they aren't
hypothetical.

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

## What to expect

```
Waiting for a gamepad (pair/connect the ESP32 now if it isn't already)...
Opened: ESP32 BLE Gamepad (VID=0x2E8A PID=0x10C6) -- expect VID=0x2E8A PID=0x10C6 for SInput mode
Rumble capable: false (expected false -- not implemented by this library yet, see GattVsHid.md)
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
- **Button 0** toggles roughly once a second, tracking BUTTON_1 in the
  sketch.
- **Battery** climbs from 25 to 90 and back down over about 40 seconds, `on
  battery` throughout (the sketch never reports charging).
- Every 3 seconds the test program itself calls
  `SDL_SetGamepadPlayerIndex()`, cycling 0→1→2→3→0..., and prints
  `succeeded`/`FAILED` for each call — watch the ESP32's own Serial Monitor
  at the same time for a matching `Player LED index: N` line. In practice
  this often doesn't show up even though SDL reports `succeeded` — see
  "Known issue: SDL doesn't reliably deliver Player LED commands" below
  before assuming something's wrong with your setup.
- **Rumble capable** should read `false` — this library's SInput support
  doesn't drive a rumble motor yet, and the Features response says so
  honestly (see [GattVsHid.md](../../GattVsHid.md)'s note on this).

## Known issue: SDL doesn't reliably deliver Player LED commands

**Status: reproducible, root-caused as far as SDL's own internals, not yet
fixed. This is an SDL3 3.4.14 issue, not a bug in this library's firmware —
see the evidence below.**

### Symptom

`sdl3_gamepad_test` calls `SDL_SetGamepadPlayerIndex()` every 3 seconds and
it always reports `succeeded`:

```
-> SDL_SetGamepadPlayerIndex(1) succeeded
-> SDL_SetGamepadPlayerIndex(2) succeeded
-> SDL_SetGamepadPlayerIndex(3) succeeded
```

but no `Player LED index: N` line ever appears on the ESP32's Serial
Monitor, and the LED never reacts. Confirmed across multiple independent
runs (different sessions, different people), not a one-off — this is the
normal/expected result right now, not something you're doing wrong.

`succeeded` is misleading here: `SDL_SetGamepadPlayerIndex()` →
`SDL_SetJoystickIDForPlayerIndex()` returns `true` based on its own internal
player-slot bookkeeping succeeding, then separately calls
`driver->SetDevicePlayerIndex()` — a `void` function with no return value —
without checking whether that call actually wrote anything. So the public
API has no way to signal this failure at the call site; `succeeded` only
means "SDL found a driver for this instance."

### The firmware is proven correct

Writing the exact same command SDL would send, directly to the device's
`hidraw` node (bypassing SDL/hidapi entirely), reliably works every time:

```bash
python3 -c "
import os
buf = bytearray(48)
buf[0] = 0x03  # Output Report ID
buf[1] = 0x03  # SINPUT_COMMAND_PLAYERLED
buf[2] = 0x01  # player index 1 (0 = off)
fd = os.open('/dev/hidraw2', os.O_RDWR)  # adjust the node
os.write(fd, bytes(buf))
os.close(fd)
"
```

This reliably produces a `Player LED index: 1` Serial line within
milliseconds and the LED itself visibly turns on (confirmed by direct
observation, several times, alternating index 1/0). So
`BleSInputReceiver::onWrite()` and `SInputPlayerLED.ino`'s
`playerLedIndex == 1` logic are both correct — this is entirely an SDL-side
delivery problem.

### What's been ruled out on this library's side

- **This library's recent descriptor changes** — reverted to the commit
  before the field-level HID usage / top-level usage page fixes (see git
  log) and rebuilt: still failed identically. Not caused by those changes.
- **Stale BlueZ/bonding state** — `sudo systemctl restart bluetooth`, full
  `bluetoothctl remove` + fresh re-pair: still failed identically.

### What's been found on SDL's side

`HIDAPI_DriverSInput_SetDevicePlayerIndex()` in
`src/joystick/hidapi/SDL_hidapi_sinput.c` only sends the write if
`ctx->player_leds_supported` is true, which gets set once by
`ProcessSDLFeaturesResponse()` when parsing the device's Features response
(`RetrieveSDLFeatures()`, called from `InitDevice()`). Rebuilding SDL3 with
an `SDL_Log()` added to `SetDevicePlayerIndex()` (or enabling the existing
but disabled `DEBUG_SINPUT_INIT`/`DEBUG_SINPUT_PROTOCOL` macros near the top
of that file) shows `player_leds_supported=false` at call time.

That's the puzzling part: the wire-level Features response, captured with
`sudo btmon -i hci0` at the same time, is byte-for-byte correct — capability
byte `0xf2` (bit 1, `PLAYERLED`, set). Replicating SDL's own parsing logic
in Python against those exact captured bytes computes the *correct* values
(`buttons_count=10`, `gyro_range=0`, `player_leds_supported=true`) — but
SDL's own debug log printed different, wrong numbers from what should be
the same payload (`buttons_count=5`, `gyro_range=3327` in one capture),
meaning SDL is decoding a different or misaligned byte sequence than what's
actually on the air. A kernel `dmesg` line appeared during this testing:

```
hid-generic 0005:2E8A:10C6.NNNN: Event data for report 1 was too short (63 vs 20)
```

suggesting BLE GATT notification truncation or reassembly issue somewhere
in SDL's Linux hidapi read path (or the kernel's uhid bridge feeding it) —
plausible given a 63-byte payload needs a negotiated ATT MTU well above the
BLE default (23 bytes / 20 usable), though this hasn't been definitively
tied to the exact failure yet.

### Next steps (for the WIP PR / further investigation)

- Instrument `RetrieveSDLFeatures()`'s read loop directly (not just the
  parsed result) to see the raw bytes `SDL_hid_read_timeout()` actually
  returns on each iteration, and whether `read == USB_PACKET_LENGTH`
  passes on a genuinely correct 64-byte buffer or a coincidentally-matching
  garbled one.
- Check whether ATT MTU is fully negotiated before `InitDevice()` runs its
  Features exchange — if the timing is racy, forcing a short delay before
  the first write/read might confirm or rule this out.
- If reproducible outside this specific setup, this looks worth filing
  against [libsdl-org/SDL](https://github.com/libsdl-org/SDL) directly —
  the evidence above (byte-perfect wire capture vs. wrong parsed values,
  the "too short" kernel warning) doesn't point at anything specific to
  this library's implementation.

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
- SDL's hidapi layer discards, at the enumeration level, any HID device
  whose *top-level* Report Descriptor usage isn't Generic Desktop
  Joystick/Gamepad/MultiAxisController — `SDL_HIDAPI_ShouldIgnoreDevice()` in
  `src/hidapi/SDL_hidapi.c`, gated by the (default-on)
  `SDL_HINT_HIDAPI_ENUMERATE_ONLY_CONTROLLERS` hint — regardless of VID/PID. This
  bit this library's own SInput implementation during development: the
  earlier draft used a Vendor Defined top-level usage (reasonable, since
  SDL's SInput driver reads every report by fixed byte offset and doesn't
  care about descriptor semantics for anything else) and got silently
  filtered out before VID/PID was ever checked. Fixed as of the version
  you're reading this against — `BleGamepad.cpp`'s SInput branch now uses
  Generic Desktop/Gamepad for the outer collection, same as the library's
  classic descriptor. If you've modified the descriptor yourself, this is
  the first thing to check.
- Otherwise, this is likely a permissions issue on `/dev/hidraw*` itself, not
  an SDL/hidapi-specific one — see the `hidraw` udev rule in
  [LinuxHIDTesting.md](../../LinuxHIDTesting.md).

**Found it, but VID/PID reads as `0xE502`/`0xBBAB` (or whatever you'd
customized) instead of `0x2E8A`/`0x10C6`**
- `setEnableSInput(true)` wasn't the last VID/PID-affecting call before
  `begin()` — see the note in step 3 above.

**Buttons/axes work, but Player LED never reaches the device (no Serial
log line on the ESP32), even though `sdl3_gamepad_test` prints
`SDL_SetGamepadPlayerIndex(...) succeeded`**
- This is the known SDL-side issue documented above in "Known issue: SDL
  doesn't reliably deliver Player LED commands" — read that section first,
  it covers the firmware-side verification (raw `hidraw` write) and what's
  been traced so far on SDL's side.
