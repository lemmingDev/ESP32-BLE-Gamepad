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
| Buttons | every configured button → one distinct evdev key, one-to-one, in the gamepad key range; 128 via the `maxbtn` profile |
| Axes | each axis → exactly one ABS code, monotonic, exact min/centre/max endpoints, well-known code mapping; negative rail via `signed-axes` |
| Hats | 8 directions + centre; the Linux "only `ABS_HAT0` surfaces" and reversed-emission quirks pinned as strict xfails |
| Special buttons | start/select/menu/home/back/vol± → one event each, across every input node the device exposes |
| Connection | BEGIN → connected → evdev node with a sane capability set; HID report descriptor stays inside the library's 150-byte buffer |
| Device Information | model / serial / firmware / hardware / software revision + manufacturer, read over GATT, match what the firmware configured |
| PnP ID | `0x2A50` vendor/product/version match `setVid`/`setPid`/`setGuidVersion` |
| Battery | `setBatteryLevel()` read back via GATT **and** `upower`; `setPowerStateAll()` charging/discharging bitfield read from `0x2A1A` |
| Latency / throughput | see below |

Compile profiles (rig `firmware/include/hil_profile.h`): `default`,
`signed-axes`, `specials`, `minimal` (smallest possible report), `maxbtn` (the
128-button ceiling).

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
  against payload size. The five profiles span ~1 byte (`minimal`) to 16 bytes
  of button data (`maxbtn`).

Raw data lands in the rig's `results/bench-*.json`; a distilled
`results/bench-table.md` and three SVG charts (latency vs report size, sustained
rate per profile, latency distribution) are regenerated by
`python -m hil.charts`. Once a full run is recorded, the distilled table and
charts are committed here under `docs/hil/` and summarised in this section.

> Baselines pending the first full `--bench` run on the Pi. Expect the
> connection interval (7.5–15 ms typical for BLE HID) to dominate single-event
> latency, and the larger-report profiles to show lower sustained rates as each
> notification spans more link-layer packets.

## See also

- [LinuxHIDTesting.md](LinuxHIDTesting.md) — doing the same checks by hand on a
  Linux box: pairing, `hidraw`/`hidapi`, `jstest`/`evtest`, `upower`, `btmon`.
- [GattVsHid.md](GattVsHid.md) — why the HID service is invisible to a generic
  GATT client once paired, while Device Information / Battery stay readable
  (which is what makes the GATT tests above possible).
