# Hardware-in-the-loop testing

The compile-only CI in this repo proves the library *builds* for every board.
It can't prove that `bleGamepad.press(5)` produces one distinct key event on a
real host, that all 128 buttons and both axis sign conventions map correctly,
that the Device Information / PnP / battery values actually reach a GATT client,
or how fast reports get through the air. A separate **hardware-in-the-loop (HIL)
rig** does that, driving the real firmware on a real ESP32 and asserting the
result on a Linux host's `/dev/input/event*` and GATT stack.

- Rig repo: `ssh://git@gitea.h.leenx.nz:2222/leenx-foss/esp32-ble-gamepad-hil.git`
- Rig hardware: a dedicated Raspberry Pi 3B+ at `192.168.101.16`
  (ssh `bot-gitea-esp32-hil`) with two MCUs attached — `esp32dev` and `esp32c3`.

## How it fits together

```
  this repo (working tree)                    Pi tester (192.168.101.16)
  ┌───────────────────────┐   bundle  +       ┌──────────────────────────────┐
  │ scripts/hil.sh:       │   harness code    │ tester/test.sh:              │
  │  build hil_runner fw   │ ────rsync/ssh───► │  esptool flash (no PlatformIO)│
  │  against THIS checkout │                   │  pytest  (evdev + BlueZ)      │
  │                        │ ◄───results/──────│  + --bench latency sweep      │
  └───────────────────────┘                   │   ESP32 ─BLE─► /dev/input/... │
        hil-results/  ◄── charts/table         └──────────────────────────────┘
```

The Pi never compiles anything — it's a 3B+. Firmware is built here (or on a CI
runner) and pushed as a flashable bundle. Command injection into `hil_runner`
is over **USB serial**, a channel independent of the BLE path under test.

## Running it

Prerequisites on your machine: PlatformIO (`pio`), a local clone of the rig
repo, and ssh access to the Pi.

```bash
git clone ssh://git@gitea.h.leenx.nz:2222/leenx-foss/esp32-ble-gamepad-hil.git ~/src/esp32-ble-gamepad-hil

# from this repo, on whatever branch/working tree you want tested:
scripts/hil.sh                                   # every board x profile, functional suite
scripts/hil.sh --bench                           # + latency / polling-rate benchmark
scripts/hil.sh --boards esp32dev --profiles "default maxbtn"
scripts/hil.sh --profiles default -- -k buttons  # args after -- go to pytest
```

`scripts/hil.sh`:

1. writes a temporary `hil_config.local.toml` in the rig checkout pointing the
   builder at **this working tree** (uncommitted changes included) and at the Pi;
2. builds the `hil_runner` firmware bundles here;
3. rsyncs the harness code **and** the bundles to the Pi (its real serial-port
   config is preserved);
4. ssh-runs `tester/test.sh` for each bundle — esptool flash, re-pair on any HID
   descriptor change, pytest, and (with `--bench`) the benchmark sweep;
5. pulls `results/` back into `./hil-results/` (gitignored) and regenerates the
   distilled table + SVG charts.

Env overrides: `HIL_REPO`, `HIL_SSH_HOST`, `HIL_SSH_USER`, `HIL_SSH_KEY`,
`HIL_PIO`. To run it from GitHub Actions later, expose the Pi over Tailscale SSH
and set `HIL_SSH_HOST` to its MagicDNS name — nothing else changes. (The rig
repo also has a Gitea `hil.yml` workflow that does the same build→ssh→test flow
from CI, triggerable by `repository_dispatch`.)

## What it covers

| Area | Tests |
|---|---|
| Buttons | every configured button → one distinct evdev key, one-to-one, in the gamepad key range. `maxbtn` (128) pins the finding that Linux only surfaces ~79 of them — `BTN_GAMEPAD + n` runs out of the named key block at `0x17e` (a game using evdev/SDL sees the same) |
| Axes | each axis → exactly one ABS code, monotonic, exact min/centre/max endpoints, well-known code mapping; negative rail via `signed-axes` |
| Hats | 8 directions + centre; the Linux "only `ABS_HAT0` surfaces" and reversed-emission quirks pinned as strict xfails |
| Special buttons | start/select/menu/home/back/vol± → one event each, across every input node the device exposes |
| Connection | BEGIN → connected → evdev node with a sane capability set; HID report descriptor stays inside the library's 150-byte buffer |
| Device Information | model / serial / firmware / hardware / software revision + manufacturer, read over GATT, match what the firmware configured |
| PnP ID | `0x2A50` vendor/product/version match `setVid`/`setPid`/`setGuidVersion` |
| Battery | `setBatteryLevel()` read back via GATT **and** `upower`; `setPowerStateAll()` charging/discharging bitfield read from `0x2A1A` |
| Latency / throughput | see below |

Compile profiles (rig `firmware/include/hil_profile.h`): `default`,
`signed-axes`, `specials`, `minimal` (smallest possible report), `maxbtn`
(128 buttons — the library's ceiling).

## Performance characteristics

`scripts/hil.sh --bench` measures, per profile:

- **Per-event latency** — button / axis / hat, host-side, split into the
  BLE-air-plus-host-stack portion and the full end-to-end figure (p50/p90/p99).
- **Sustained report rate** — how many rapid toggles per second actually reach
  the host before the BLE link or host input stack starts dropping them, at a
  range of inter-report spacings.
- **Connection interval / MTU** — the negotiated BLE parameters that set the
  ceiling on report rate.
- **HID report + descriptor size** — so latency and throughput can be plotted
  against payload size. The five profiles span 3-byte (`minimal`) to 28-byte
  (`default` / `signed-axes`) input reports.

Raw data lands in the rig's `results/bench-*.json`; a distilled
`results/bench-table.md` and three SVG charts are regenerated by
`python -m hil.charts` and committed here under [docs/hil/](docs/hil/).

### Baseline — esp32dev, ESP32-BLE-Gamepad `v0.7.5-rc0`, Raspberry Pi 3B+ tester

Tester: Raspberry Pi 3B+ (built-in Cypress BT 5.0 adapter), Debian, kernel
6.18.34, BlueZ 5.82, Python 3.13.

| Profile | HID report | Descriptor | Conn interval | MTU | Button latency (end-to-end) p50 / p99 | Dropped |
|---|---|---|---|---|---|---|
| minimal (1 btn, 1 axis) | 3 B | 50 B | 48.75 ms | 255 | 18.6 / 67.6 ms | 0 / 200 |
| maxbtn (128 btn) | 16 B | 25 B | 48.75 ms | 255 | 18.6 / 67.6 ms | 0 / 200 |
| specials (16 btn, 8 special) | 20 B | 132 B | 48.75 ms | 255 | 18.6 / 67.5 ms | 0 / 200 |
| default (64 btn, 4 hat, 8 axis) | 28 B | 102 B | 48.75 ms | 255 | 18.6 / 67.5 ms | 0 / 200 |
| signed-axes | 28 B | 102 B | 48.75 ms | 255 | 18.6 / 95.6 ms | 0 / 200 |

![latency vs report size](docs/hil/latency-vs-reportsize.svg)

**What this shows:**

- **A single button press reaches the host in ~18.6 ms** (median), of which
  ~11 ms is BLE air time + the host input stack and ~7.6 ms is the rig's own
  USB-serial command + firmware parse. p99 sits around 67 ms — one connection
  interval of jitter.
- **Zero dropped events.** Every one of 200 deliberately-paced presses per
  profile produced exactly one host event.
- **Latency is flat against HID report size** (3 → 28 bytes). At a 255-byte MTU
  the report fits in one link-layer packet regardless, so payload size costs
  nothing.
- **The connection interval — 48.75 ms — is the single dominant factor**, and
  it's the same for every profile. This library does not request a fast
  connection interval; typical BLE HID negotiates 7.5–15 ms. That 48.75 ms is
  the ceiling on report *rate*: roughly one report per interval, ~20 Hz. Firing
  `sendReport()` faster than that overflows NimBLE's transmit queue and the
  ESP32 drops the surplus silently before it goes on air (the `burst` columns
  in `bench-table.md` show that curve). A future library change to request a
  shorter interval would move all of these numbers.

Absolute figures carry host-scheduling jitter — treat them as comparative
across profiles and as a regression baseline, not a hard guarantee.

## See also

- [LinuxHIDTesting.md](LinuxHIDTesting.md) — doing the same checks by hand on a
  Linux box: pairing, `hidraw`/`hidapi`, `jstest`/`evtest`, `upower`, `btmon`.
- [GattVsHid.md](GattVsHid.md) — why the HID service is invisible to a generic
  GATT client once paired, while Device Information / Battery stay readable
  (which is what makes the GATT tests above possible).
