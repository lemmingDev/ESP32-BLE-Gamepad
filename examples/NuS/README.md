# NuS Examples — gamepad + BLE serial side channel

These examples pair the HID gamepad with a Nordic UART Service (NUS) serial
channel from the external
[NuS-NimBLE-Serial](https://github.com/afpineda/NuS-NimBLE-Serial) library
(CC BY 4.0, © Ángel Fernández Pineda — install it from the Arduino Library
Manager; it is **not** bundled here). Full background, init sequence, and
client apps: [docs/NuSCompatibility.md](../../docs/NuSCompatibility.md).

One bridge example per gamepad mode — they are separate files so there is no
confusion about which commands and queries belong to which HID report layout.
The serial channel itself works identically in all modes.

| Example | Mode | Terminal → device | Device → terminal |
|---------|------|-------------------|-------------------|
| [NuSSerialDiag](NuSSerialDiag/NuSSerialDiag.ino) | Generic | `help`, `button4`, echo | Greeting on subscribe, proactive status lines (uptime, buttons, battery, heap) |
| [NuSGenericBridge](NuSGenericBridge/NuSGenericBridge.ino) | Generic (strict defaults) | `press`/`release` 1–16, `axis` ×8, `hat`, `battery`, `power`, `status`, `pair`, `unpair`, `txpower`, `addr?` | `ok`/`err` replies, periodic `state` |
| [NuSGenericAdvanced](NuSGenericAdvanced/NuSGenericAdvanced.ino) | Generic (+ start/select, output/feature reports) | Everything in `NuSGenericBridge`, plus `special`, `output?`, `feature get`/`set` | `ok`/`err` replies, `event output`/`event feature` pushes, periodic `state` |
| [NuSSInputBridge](NuSSInputBridge/NuSSInputBridge.ino) | SInput (IMU + RGB caps) | `press`/`release` 1–25, `stick`, `trigger`, `hat`, `motion`, `touch`, `special` (start/select/home), `battery`, `power`, `status`, `pair`, `unpair`, `txpower`, `addr?` | `ok`/`err` replies, `event led/rumble/rgb` pushed as SInput Output Reports arrive, plus `led?`/`rumble?`/`rgb?` queries |
| [NuSXInputBridge](NuSXInputBridge/NuSXInputBridge.ino) | XInput One S | `press`/`release` 1–11 (A…Home), `stick`, `trigger`, `hat`, `special` (start/select/home/back=Share), `battery`, `power`, `status`, `pair`, `unpair`, `txpower`, `addr?` | `ok`/`err` replies, `event rumble` (strong/weak + trigger magnitudes) pushed as Xbox Output Reports arrive, plus `rumble?` query |

## Strict vs advanced Generic bridge

`NuSGenericBridge` runs on the pure library-default configuration (16 buttons,
8 axes, 1 hat — `begin()` with no config), so every command is live on the
wire with nothing extra enabled. `NuSGenericAdvanced` adds exactly three
config lines (start/select special buttons, HID output report, HID feature
report) to unlock `special`, `output?`, and `feature get`/`set`. Pick strict
if you want the smallest descriptor and a guaranteed-default device; pick
advanced if you need specials or host-to-device HID reports.

## Wire contract for companion apps

Every sketch here identifies itself the same machine-readable way, so a
companion app (e.g. `NuS-Gamepad-Companion`) can auto-select the right
command profile instead of asking the user:

- ~500ms after ANY subscriber-count increase, each sketch pushes
  `hello <profile-id> <proto-ver>` (currently `1`) before the human-readable
  greeting — so second and later subscribers are greeted too, and the delay
  lets the new subscriber's notify handler attach first (CCCD race).
- The `proto?` command returns `proto <profile-id> <proto-ver>` on demand.
- Line vocabulary is shared: `ok` / `err` replies, `event <name> …` pushes,
  periodic `state …` lines, everything else informational.

Profile IDs (v1):

| Sketch | `hello` / `proto?` identity |
|--------|------------------------------|
| NuSSerialDiag | `nus-diag` |
| NuSGenericBridge | `nus-bridge/generic-strict` |
| NuSGenericAdvanced | `nus-bridge/generic-advanced` |
| NuSSInputBridge | `nus-bridge/sinput` |
| NuSXInputBridge | `nus-bridge/xinput` |

Bump `NUS_PROTO_VER` (and document the delta here) if the vocabulary ever
changes incompatibly. Sketches for other firmware (e.g. CompositeHID bridges)
should mint their own `nus-bridge/<name>` IDs under the same scheme.

## Scan-response aliases

The 128-bit NUS UUID lives in the scan response, and a scanner filtering by
service UUID shows whatever name (if any) shares that packet — the full
device name cannot fit there next to the UUID. Each sketch therefore also
advertises a short `NuS-…` alias in the scan response (same string a filtered
view displays). The GAP/GATT display name and pairing UX are unaffected.

| Sketch | Full name (GAP display + pairing) | Scan-response alias |
|--------|-----------------------------------|---------------------|
| NuSSerialDiag | ESP32 BLE Gamepad Diag | `NuS-Diag` |
| NuSGenericBridge | ESP32 Gamepad NuS Generic | `NuS-Gen` |
| NuSGenericAdvanced | ESP32 Gamepad NuS Adv | `NuS-GenAdv` |
| NuSSInputBridge | ESP32 Gamepad NuS SInput | `NuS-SInput` |
| NuSXInputBridge | Xbox Wireless Controller | `NuS-XInput` |

Whether the full name also appears in the adv packet itself varies with
length (the 25-char Xbox name demonstrably doesn't fit next to
flags + appearance + HID UUID, and `setName()` fails silently) — the alias
is the reliable scanner identity in every filtered view.

## Shared pattern (all sketches)

```cpp
BleGamepad bleGamepad("...", "Espressif", 100, true); // delayAdvertising = true
bleGamepad.begin();                                    // builds HID, no advertising yet
while (NimBLEDevice::getServer() == nullptr) { delay(10); } // wait for NimBLE init
NuSerial.start(false);                                 // register NuS, leave advertising alone
NimBLEDevice::getServer()->getAdvertising()->start();  // advertise once, both services
```

- `delayAdvertising=true` is required: without it, advertising starts before
  the NuS service exists.
- Use `NuSerial.start(false)` — never `NuSerial.begin()`, which enables
  automatic advertising and stomps the HID advertising setup.
- `NuSerial` is a singleton with no subscribe callback: poll
  `NuSerial.subscriberCount()` / `NuSerial.isConnected()`.
- The 128-bit NUS UUID is advertised in the scan response (the 31-byte adv
  packet itself is full), so active scanners can filter by service UUID.
- The `hello` greeting is held 500ms after a new subscriber appears, so the
  subscriber's notify handler is attached before the push (a notify sent
  during CCCD enable can be lost). `proto?` re-queries the identity anytime.

## Which one do I want?

- **"Is my BLE link alive, and is HID flowing?"** → `NuSSerialDiag`
  (reimplementation of the removed `examples/Generic/Diagnostics` sketch).
- **"Drive a Generic / SInput / Xbox pad from my phone or laptop terminal"**
  → the matching `NuS*Bridge` (`NuSGenericBridge` for pure defaults,
  `NuSGenericAdvanced` if you also need start/select or HID reports).
- **"Show SInput player-LED / rumble / RGB, or Xbox rumble, in a terminal"**
  → `NuSSInputBridge` / `NuSXInputBridge` (note: those host reports only
  arrive from an SInput-aware host, e.g. SDL3 with the SInput hint, or an
  Xbox host — not from the plain OS joystick path).
