# Testing SInputPlayerLED.ino with SDL3 on Linux

This walks through verifying SInputPlayerLED.ino end-to-end against a real
SDL3 app on Linux: buttons/axes, Player LED, and the battery ramp added in
this example — using [sdl3_gamepad_test.c](sdl3_gamepad_test.c), a small
standalone test program in this directory.

This exact flow — real ESP32 hardware, SDL3 3.4.14 built from source, tested
over SSH — is what shook out the real bugs the "Troubleshooting", "Two
things specific to SInput mode", and "Resolved: Player LED wasn't reaching
SDL's driver" sections below describe; they aren't hypothetical.

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
`src/joystick/hidapi/SDL_hidapi_sinput.c` and rebuild SDL — see "What's been
found on SDL's side" below) — but it does surface whatever SDL already logs
at debug/verbose priority elsewhere in the hidapi/joystick stack, at no cost
beyond the flag.

There's also [sinput_hid_test.py](sinput_hid_test.py) in this directory,
which talks the same protocol directly over `hidraw`, bypassing SDL
entirely — useful for isolating a firmware bug from an SDL/driver-layer one
(see "Resolved: Player LED wasn't reaching SDL's driver" below, where doing
exactly that was how a real bug here got root-caused). If you followed step 3 above via
[LinuxHIDTesting.md](../../LinuxHIDTesting.md)'s steps 1-5, its step 2
already set you up a Python venv with the `hid` package installed, so you
can run it right away:

```bash
sudo ~/src/pyBLEgameTests/.venv/bin/python sinput_hid_test.py
```

Otherwise, set one up the same way that step does:

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
  flag discussed in "Resolved: Player LED wasn't reaching SDL's driver"
  below. It should read `true`; if it prints `false`, see that section —
  either you're on an old version of this library from before the fix, or
  something has drifted again.
- **Button 0** toggles roughly once a second, tracking BUTTON_1 in the
  sketch.
- **Battery** climbs from 25 to 90 and back down over about 40 seconds, `on
  battery` throughout (the sketch never reports charging).
- Every 3 seconds the test program itself calls
  `SDL_SetGamepadPlayerIndex()`, cycling 0→1→2→3→0..., and prints
  `succeeded`/`FAILED` for each call — watch the ESP32's own Serial Monitor
  at the same time for a matching `Player LED index: N` line. This should
  now show up reliably; if it doesn't, see "Resolved: Player LED wasn't
  reaching SDL's driver" below before assuming something's wrong with your
  setup.
- **Rumble capable** should read `false` — this library's SInput support
  doesn't drive a rumble motor yet, and the Features response says so
  honestly (see [GattVsHid.md](../../GattVsHid.md)'s note on this).

## Resolved: Player LED wasn't reaching SDL's driver

**Status: root-caused and fixed.** This turned out to be a bug in this
library's own SInput implementation, not in SDL — the opposite of what the
investigation below first concluded. Kept here (with the outdated
conclusions struck through rather than deleted) because the debugging trail
itself is the useful part if something like this happens again.

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
`hidraw` node (bypassing SDL/hidapi entirely), reliably works every time.
[sinput_hid_test.py](sinput_hid_test.py) in this directory does exactly
that — it also requests and decodes the Features response the same way
SDL's driver is supposed to, so you can compare its (correct) parse against
`sdl3_gamepad_test.c`'s "Player LED capable (SDL's view)" line for the same
device (see step 4 above for the one-time Python venv setup):

```
Opening /dev/hidraw2
Features response received, caps0=0xF2
  PLAYERLED capability bit: set (this library always sets it -- BleSInput.cpp's sendFeaturesResponse())
  RUMBLE: not set
  LEFT_STICK: set
  RIGHT_STICK: set
  LEFT_TRIGGER: set
  RIGHT_TRIGGER: set
  usage_mask_0=0x01 usage_mask_1=0x00
Cycling Player LED index 0->1->2->3->0... every 3s (watch the ESP32's Serial Monitor for 'Player LED index: N' and the onboard LED). Ctrl-C to stop.
-> wrote SINPUT_COMMAND_PLAYERLED index=1
```

This reliably produces a `Player LED index: 1` Serial line within
milliseconds and the LED itself visibly turns on (confirmed by direct
observation, several times, alternating index 1/0), and the Features
response's PLAYERLED bit always comes back set. So
`BleSInputReceiver::onWrite()`/`sendFeaturesResponse()` and
`SInputPlayerLED.ino`'s `playerLedIndex == 1` logic are all correct.

~~This is entirely an SDL-side delivery problem.~~ **This conclusion was
wrong** — see "Root cause" below. The firmware being correct and SDL's
capability check failing were both true at the same time, but not because
either side was buggy in isolation: the two sides were decoding the same
bytes according to two different, incompatible layouts.

### What's been ruled out on this library's side

- **This library's recent descriptor changes** — reverted to the commit
  before the field-level HID usage / top-level usage page fixes (see git
  log) and rebuilt: still failed identically. Not caused by those changes.
- **Stale BlueZ/bonding state** — `sudo systemctl restart bluetooth`, full
  `bluetoothctl remove` + fresh re-pair: still failed identically.

### ~~What's been found on SDL's side~~ Root cause

`HIDAPI_DriverSInput_SetDevicePlayerIndex()` in
`src/joystick/hidapi/SDL_hidapi_sinput.c` only sends the write if
`ctx->player_leds_supported` is true, which gets set once by
`ProcessSDLFeaturesResponse()` when parsing the device's Features response
(`RetrieveSDLFeatures()`, called from `InitDevice()`). That part of the
original investigation was right: `player_leds_supported` was indeed false.
Where it went wrong was concluding SDL was misparsing a correct payload.

Diffing SDL's actual `src/joystick/hidapi/SDL_hidapi_sinput.c` at the exact
3.4.14 tag against this library's `BleSInput.h` found the real story: the
SInput driver picked up several protocol revisions after the PR this
library's constants were originally reverse-engineered from
([libsdl-org/SDL#13343](https://github.com/libsdl-org/SDL/pull/13343)) —
notably [#13667, "Version as a capabilities vehicle"](https://github.com/libsdl-org/SDL/pull/13667)
— all already merged into the 3.4.14 tag itself (confirmed via GitHub's
compare API: zero SInput-related file changes between the 3.4.14 tag and
current `main`, so this isn't something a newer SDL build would have fixed
either). That PR inserted a 2-byte `protocol_version` field into the
Features response right after the command-echo byte, shifting every field
after it by 2:

| Byte offset (from command echo) | SDL 3.4.14 actually reads | This library used to send |
|---|---|---|
| 0 | command echo | command echo ✓ |
| 1-2 | `protocol_version` (u16) — new in #13667 | caps0, caps1 |
| **3** | **caps0, incl. PLAYERLED bit `0x02`** | `type` (always 0) |
| 4 | caps1 | `style` (always 0) |
| 5 | gamepad type | `poll_ms` |
| 7-8 | poll rate (**microseconds**, not ms) | accel_range |
| 9-10 | accel_range | gyro_range |
| 11-12 | gyro_range | usage_mask_0, usage_mask_1 |

So SDL was reading byte 3 — this library's always-zero `type` byte — as
`caps0`, computing `player_leds_supported = false` from a field that never
carried the real capability data in the first place. The `0xf2` capability
byte the original investigation captured via `btmon` was real and correct —
it just landed one field to the left of where SDL 3.4.14 actually looks for
it. Both sides were decoding their inputs correctly; only the layout
contract between them was stale.

Separately (and unrelated to the LED bug, since it doesn't affect
`player_leds_supported`): `SINPUT_BTN0_SOUTH`/`EAST`/`WEST`/`NORTH`'s bit
*values* were correct as originally written, despite superficially looking
inconsistent with SDL's own `SINPUT_BUTTONMASK_*` constants of the same
names in `SDL_hidapi_sinput.c` — those constants turned out to be unused
dead code in SDL's own driver. SDL's actual live-button decode assigns raw
joystick button indices purely by scanning `usage_mask` bits in position
order, with its mapping-string generator unconditionally assigning South/
East/West/North to raw indices 0/1/2/3 in that order — so what matters is
that those four bits are the lowest four enabled `usage_mask` bits, in that
literal order, which they already were. (Caught this the hard way: an
earlier pass at this fix "corrected" these bit values to match the dead
`SINPUT_BUTTONMASK_*` constants, which silently broke which physical button
reported as SDL's `SOUTH` — verified via a full board reflash and re-test.
It's reverted; see `BleSInput.h`'s comment on `SINPUT_BTN0_SOUTH` for the
full explanation, kept there so it isn't "corrected" again the same way.)

### The fix

`BleSInput.h`/`BleSInput.cpp` now match SDL 3.4.14's actual layout: a
`SINPUT_FEAT_IDX_PROTOCOL_VERSION` field was added, and every
`SINPUT_FEAT_IDX_*` offset from `CAPS0` onward shifted by 2 to match.
Verified end-to-end on real hardware, not just by re-derivation: after
reflashing, `sdl3_gamepad_test -v` prints `Player LED capable (SDL's view):
true`, and its `SDL_SetGamepadPlayerIndex()` calls now produce real `Player
LED index: N` lines on the ESP32's own Serial Monitor and visibly drive the
onboard LED — through the actual SDL path, not the `sinput_hid_test.py`
bypass. [sinput_hid_test.py](sinput_hid_test.py)'s own offsets were updated
in lockstep (it was reading the old, now-wrong layout too) and its Features
dump matches SDL's parse exactly post-fix (`protocol_version=1,
caps0=0xF2`, agreeing with `sdl3_gamepad_test.c`'s capability line for the
same connection).

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
`SDL_SetGamepadPlayerIndex(...) succeeded`, and "Player LED capable (SDL's
view)" prints `false`**
- If you're on a version of this library from before this was fixed (check
  `BleSInput.h` for `SINPUT_FEAT_IDX_PROTOCOL_VERSION` — present means
  fixed), see "Resolved: Player LED wasn't reaching SDL's driver" above for
  the full root cause and fix. Update to a version that has it.
- If you're already on a fixed version and still seeing this, something
  else is wrong — re-run [sinput_hid_test.py](sinput_hid_test.py) to confirm
  the Features response's `PLAYERLED` bit and `sdl3_gamepad_test.c`'s
  "Player LED capable (SDL's view)" line still agree (both `true`); if they
  now disagree again, the offsets have drifted from SDL's driver a second
  time and need re-deriving the same way "Resolved" describes.
