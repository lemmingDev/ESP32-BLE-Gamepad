/*
 * XInput-mode gamepad driven from a BLE terminal over NUS, with the Xbox
 * host rumble report surfaced back to the terminal.
 *
 * Requires the external NuS-NimBLE-Serial library
 * (https://github.com/afpineda/NuS-NimBLE-Serial, CC BY 4.0), installable
 * from the Arduino Library Manager. Connect with any Nordic UART capable
 * terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) and type
 * "help" for the command list.
 *
 * This is the XInput-mode bridge (Xbox Series X, VID 0x045E / PID 0x0B13):
 * drive A/B/X/Y/LB/RB/sticks/triggers/D-pad, special buttons (incl. Back for
 * Share), battery and bond/TX-power management from the terminal, and query
 * (or get pushed) the last rumble report the Xbox host sent via Output
 * Report 0x03 (strong/weak motors + trigger magnitudes). Series X is used
 * for native XInput over BLE on Win11 22H2+; swap one line below to
 * GamepadMode::XInputOneS for the One S variant (PID 0x02FD, broader
 * xpad<6.5 compat). For the Generic and SInput equivalents, see
 * NuSGenericBridge and NuSSInputBridge in this folder.
 *
 * Init order matters (see docs/NuSCompatibility.md): delayAdvertising=true,
 * wait for the NimBLE server, NuSerial.start(false), then start advertising
 * manually. Do NOT use NuSerial.begin() - it enables automatic advertising
 * and would stomp the HID advertising setup.
 */

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad
#if !__has_include("NuSerial.hpp")
#error "Install NuS-NimBLE-Serial from the Arduino Library Manager (see docs/NuSCompatibility.md)"
#endif
#include <NuSerial.hpp> // https://github.com/afpineda/NuS-NimBLE-Serial
#include <NimBLEDevice.h>

// Machine-readable sketch identity for companion apps (see examples/NuS/README.md).
// Greeted on subscribe, queryable via the 'proto?' command.
#define NUS_PROFILE_ID "nus-bridge/xinput"
#define NUS_PROTO_VER 1

// Nordic UART Service UUID, advertised in the scan response (see setup()).
#define NUS_ADV_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

#define STATE_INTERVAL_MS 3000 // How often to push a state summary to subscribers

// delayAdvertising=true: begin() builds the HID service and configures
// advertising but does not start it - NuS registers first (see setup()).
// Note: begin() overrides the device name to "Xbox Wireless Controller" in
// XInput modes (the Xbox driver matches on it), so the name below only
// matters before begin() runs.
BleGamepad bleGamepad("Gamepad NuS XInput", "lemmingDev", 100, true);
BleGamepadConfiguration config;

unsigned long lastStateTime = 0;
size_t lastNusSubscribers = 0;
unsigned long nusGreetAt = 0; // millis() timestamp for the delayed greeting, 0 = none pending
String nusLine; // Accumulates one incoming NUS line

// Local mirrors of device state for the `status` readout.
int16_t stLX = 0, stLY = 0, stRX = 0, stRY = 0;
int16_t stLT = 0, stRT = 0;
int stHat = 0;
int stBattery = 100;
int stPower[4] = {0, 0, 0, 0};

void setup()
{
    Serial.begin(115200);

    // XInput Series X mode: 11 buttons (A/B/X/Y/LB/RB/LS/RS/Select/Start/Home),
    // Xbox VID/PID/serial. Do not override setVid()/setPid() - the host Xbox
    // driver recognises the device by that exact pair. Start/select/home/back
    // specials enabled so `special` drives the Xbox buttons (back = Share).
    // Note BUTTON_9/10/11 set the same Xbox bits as select/start/home.
    config.setGamepadMode(GamepadMode::XInputSeriesX);
    config.setWhichSpecialButtons(true, true, false, true, true, false, false, false);
    bleGamepad.begin(&config);

    // begin() initialises NimBLE asynchronously on its own task. NuSerial
    // needs the stack up first, so wait for the server to exist.
    while (NimBLEDevice::getServer() == nullptr)
    {
        delay(10);
    }

    // false = register the NuS service but leave advertising alone.
    NuSerial.start(false);

    // Advertise the NuS service UUID in the scan response. The 31-byte adv
    // packet is already full (flags + appearance + HID UUID + truncated
    // name), so addServiceUUID overflows the 128-bit NUS UUID into the scan
    // response payload automatically once scan responses are enabled. Active
    // scanners then see NUS for service-UUID filtering; passive-scan bytes
    // are unchanged from before.
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getServer()->getAdvertising();
    pAdvertising->enableScanResponse(true);
    if (!pAdvertising->addServiceUUID(NUS_ADV_UUID))
    {
        Serial.println("[NuSXInputBridge] WARNING: NuS UUID did not fit advertising data.");
    }
    // Short alias in the scan response. You'll usually see THIS name, not the
    // full device name, in filtered scanner views - and it MUST stay <= 11
    // characters so it fits next to the 128-bit NUS UUID (18 + len + 2 <= 31).
    // (The 25-char device name never fit the adv packet anyway, so this alias
    // is the only on-air name besides the GAP record - pairing display and
    // the Xbox driver path are unaffected.) With scan responses enabled,
    // setName() targets the scan data only.
    if (!pAdvertising->setName("XInput-NuS"))
    {
        Serial.println("[NuSXInputBridge] WARNING: NuS alias did not fit scan response.");
    }

    // Advertise once for both services together.
    pAdvertising->start();

    Serial.println("[NuSXInputBridge] Ready. Connect a BLE terminal and send 'help'.");
}

void printHelp()
{
    NuSerial.println("Commands (buttons: 1=A 2=B 3=X 4=Y 5=LB 6=RB 7=LS 8=RS 9=Select 10=Start 11=Home):");
    NuSerial.println("  help                  - show this message");
    NuSerial.println("  proto?                - show sketch profile id and protocol version");
    NuSerial.println("  press <1..11>         - press a button");
    NuSerial.println("  release <1..11>       - release a button");
    NuSerial.println("  stick left <x> <y>    - left stick, each -32767..32767");
    NuSerial.println("  stick right <x> <y>   - right stick, each -32767..32767");
    NuSerial.println("  trigger <l> <r>       - triggers, each 0..32767");
    NuSerial.println("  hat <0..8>            - D-pad: 0=centered 1=up 2=up-right ... 8=up-left");
    NuSerial.println("  special <name> <on|off> - start select home back(back=Share)");
    NuSerial.println("  battery <0..100>      - set reported battery level (std Battery Service)");
    NuSerial.println("  power <b> <d> <c> <l> - battery power state, each 0..3");
    NuSerial.println("  rumble?               - last rumble report from Xbox host");
    NuSerial.println("  pair                  - enter pairing mode (BLOCKS till a new host pairs)");
    NuSerial.println("  unpair                - delete current bond");
    NuSerial.println("  unpairall confirm     - delete ALL bonds (needs the word 'confirm')");
    NuSerial.println("  txpower <-12..9>      - set BLE TX power (-12 -9 -6 -3 0 3 6 9)");
    NuSerial.println("  txpower?              - show BLE TX power");
    NuSerial.println("  addr?                 - show this device's BLE address");
    NuSerial.println("  status                - reply with current state immediately");
}

bool validTxPower(int v)
{
    return v == -12 || v == -9 || v == -6 || v == -3 || v == 0 || v == 3 || v == 6 || v == 9;
}

void pushRumble()
{
    NuSerial.println("event rumble strong=" + String(bleGamepad.getXInputStrongMotor()) +
                     " weak=" + String(bleGamepad.getXInputWeakMotor()) +
                     " ltrig=" + String(bleGamepad.getXInputLeftTriggerMagnitude()) +
                     " rtrig=" + String(bleGamepad.getXInputRightTriggerMagnitude()));
}

void pushState()
{
    String s = "state buttons=";
    for (int b = 1; b <= 11; b++)
    {
        s += bleGamepad.isPressed(b) ? "1" : "0";
    }
    s += " sticks=" + String(stLX) + "," + String(stLY) + "," + String(stRX) + "," + String(stRY);
    s += " triggers=" + String(stLT) + "," + String(stRT);
    s += " hat=" + String(stHat);
    s += " battery=" + String(stBattery);
    s += " power=" + String(stPower[0]) + "," + String(stPower[1]) + "," +
         String(stPower[2]) + "," + String(stPower[3]);
    NuSerial.println(s);
}

void handleCommand(String cmd)
{
    cmd.toLowerCase();

    if (cmd == "help") { printHelp(); return; }
    if (cmd == "proto?") { NuSerial.println("proto " NUS_PROFILE_ID " " + String(NUS_PROTO_VER)); return; }
    if (cmd == "status") { pushState(); return; }
    if (cmd == "rumble?") { pushRumble(); return; }
    if (cmd.startsWith("special "))
    {
        // special <start|select|home|back> <on|off> (back = Share button)
        int sp = cmd.indexOf(' ', 8);
        if (sp > 0)
        {
            String which = cmd.substring(8, sp);
            String onoff = cmd.substring(sp + 1);
            uint8_t btn = 255;
            if (which == "start") btn = START_BUTTON;
            else if (which == "select") btn = SELECT_BUTTON;
            else if (which == "home") btn = HOME_BUTTON;
            else if (which == "back") btn = BACK_BUTTON;
            if (btn != 255 && (onoff == "on" || onoff == "off"))
            {
                if (onoff == "on") { bleGamepad.pressSpecialButton(btn); }
                else { bleGamepad.releaseSpecialButton(btn); }
                NuSerial.println("ok " + cmd);
            }
            else
            {
                NuSerial.println("err usage: special <start|select|home|back> <on|off>");
            }
        }
        else
        {
            NuSerial.println("err usage: special <start|select|home|back> <on|off>");
        }
        return;
    }
    if (cmd.startsWith("battery "))
    {
        int lvl = cmd.substring(8).toInt();
        if (lvl >= 0 && lvl <= 100)
        {
            bleGamepad.setBatteryLevel((uint8_t)lvl);
            stBattery = lvl;
            NuSerial.println("ok battery " + String(lvl));
        }
        else
        {
            NuSerial.println("err battery must be 0..100");
        }
        return;
    }
    if (cmd.startsWith("power "))
    {
        // power <batteryPowerInfo> <discharging> <charging> <level>, each 0..3
        int p[4];
        int idx = 6, ok = 1;
        for (int i = 0; i < 4; i++)
        {
            int sp = cmd.indexOf(' ', idx);
            String tok = (sp > 0 || i == 3) ? cmd.substring(idx, sp > 0 ? sp : cmd.length()) : "";
            if (tok.length() == 0) { ok = 0; break; }
            p[i] = tok.toInt();
            if (p[i] < 0 || p[i] > 3) { ok = 0; break; }
            idx = sp + 1;
        }
        if (ok)
        {
            bleGamepad.setPowerStateAll((uint8_t)p[0], (uint8_t)p[1], (uint8_t)p[2], (uint8_t)p[3]);
            stPower[0] = p[0]; stPower[1] = p[1]; stPower[2] = p[2]; stPower[3] = p[3];
            NuSerial.println("ok " + cmd);
        }
        else
        {
            NuSerial.println("err usage: power <b> <d> <c> <l>, each 0..3");
        }
        return;
    }
    if (cmd == "pair")
    {
        NuSerial.println("pairing mode - waiting for a NEW host to pair (terminal unresponsive till then)...");
        if (bleGamepad.enterPairingMode())
        {
            NuSerial.println("ok paired with new host");
        }
        else
        {
            NuSerial.println("err pairing failed");
        }
        return;
    }
    if (cmd == "unpair")
    {
        if (bleGamepad.deleteBond(false))
        {
            NuSerial.println("ok current bond deleted");
        }
        else
        {
            NuSerial.println("err no bond to delete");
        }
        return;
    }
    if (cmd.startsWith("unpairall"))
    {
        if (cmd == "unpairall confirm")
        {
            if (bleGamepad.deleteAllBonds(false))
            {
                NuSerial.println("ok all bonds deleted");
            }
            else
            {
                NuSerial.println("err delete failed");
            }
        }
        else
        {
            NuSerial.println("err usage: unpairall confirm");
        }
        return;
    }
    if (cmd.startsWith("txpower "))
    {
        int v = cmd.substring(8).toInt();
        if (validTxPower(v))
        {
            bleGamepad.setTXPowerLevel((int8_t)v);
            NuSerial.println("ok txpower " + String(v));
        }
        else
        {
            NuSerial.println("err txpower must be one of -12 -9 -6 -3 0 3 6 9");
        }
        return;
    }
    if (cmd == "txpower?")
    {
        NuSerial.println("txpower " + String(bleGamepad.getTXPowerLevel()));
        return;
    }
    if (cmd == "addr?")
    {
        NuSerial.println("addr " + bleGamepad.getStringAddress());
        return;
    }

    if (cmd.startsWith("press ") || cmd.startsWith("release "))
    {
        bool press = cmd.startsWith("press ");
        int b = cmd.substring(press ? 6 : 8).toInt();
        if (b >= 1 && b <= 11)
        {
            if (press) { bleGamepad.press(b); } else { bleGamepad.release(b); }
            NuSerial.println(String(press ? "ok pressed " : "ok released ") + String(b));
        }
        else
        {
            NuSerial.println("err button must be 1..11");
        }
        return;
    }
    if (cmd.startsWith("stick "))
    {
        // stick <left|right> <x> <y>
        int s1 = cmd.indexOf(' ', 6);
        int s2 = s1 > 0 ? cmd.indexOf(' ', s1 + 1) : -1;
        if (s1 > 0 && s2 > 0)
        {
            String which = cmd.substring(6, s1);
            int16_t x = (int16_t)cmd.substring(s1 + 1, s2).toInt();
            int16_t y = (int16_t)cmd.substring(s2 + 1).toInt();
            if (which == "left") { bleGamepad.setLeftThumb(x, y); stLX = x; stLY = y; NuSerial.println("ok " + cmd); }
            else if (which == "right") { bleGamepad.setRightThumb(x, y); stRX = x; stRY = y; NuSerial.println("ok " + cmd); }
            else { NuSerial.println("err usage: stick <left|right> <x> <y>"); }
        }
        else
        {
            NuSerial.println("err usage: stick <left|right> <x> <y>");
        }
        return;
    }
    if (cmd.startsWith("trigger "))
    {
        // trigger <l> <r>, each 0..32767
        int s1 = cmd.indexOf(' ', 8);
        if (s1 > 0)
        {
            int16_t l = (int16_t)cmd.substring(8, s1).toInt();
            int16_t r = (int16_t)cmd.substring(s1 + 1).toInt();
            bleGamepad.setTriggers(l, r);
            stLT = l; stRT = r;
            NuSerial.println("ok " + cmd);
        }
        else
        {
            NuSerial.println("err usage: trigger <l> <r>");
        }
        return;
    }
    if (cmd.startsWith("hat "))
    {
        int h = cmd.substring(4).toInt();
        if (h >= 0 && h <= 8)
        {
            bleGamepad.setHat1((signed char)h);
            stHat = h;
            NuSerial.println("ok hat " + String(h));
        }
        else
        {
            NuSerial.println("err hat must be 0..8");
        }
        return;
    }
    NuSerial.println("err unknown command - send 'help'");
}

// Returns one complete trimmed line, or an empty String if none is ready yet.
String readNuSLine()
{
    while (NuSerial.available())
    {
        char c = (char)NuSerial.read();
        if (c == '\n')
        {
            String out = nusLine;
            nusLine = "";
            out.trim();
            return out;
        }
        if (c != '\r')
        {
            nusLine += c;
            if (nusLine.length() > 200)
            {
                nusLine = ""; // Overlong line - drop it rather than grow forever
            }
        }
    }
    return String();
}

void loop()
{
    // Greet new subscribers (NuSerial is a singleton - no subscribe callback
    // to override, so poll the count).
    size_t subs = NuSerial.subscriberCount();
    if (subs == 0)
    {
        nusGreetAt = 0;
    }
    else if (subs > lastNusSubscribers)
    {
        // New arrival(s): hold the greeting 500ms so the notify handler is
        // attached before we push (avoids losing hello to the CCCD race).
        // Triggers on ANY increase so later subscribers are greeted too.
        nusGreetAt = millis() + 500;
    }
    else if (nusGreetAt != 0 && (long)(millis() - nusGreetAt) >= 0)
    {
        nusGreetAt = 0;
        NuSerial.println("hello " NUS_PROFILE_ID " " + String(NUS_PROTO_VER));
        NuSerial.println("[NuS] XInput bridge ready. Send 'help'.");
    }
    lastNusSubscribers = subs;

    String cmd = readNuSLine();
    if (cmd.length() > 0)
    {
        Serial.println("[NUS] got: " + cmd);
        handleCommand(cmd);
    }

    // Surface Xbox host rumble reports as they arrive.
    if (bleGamepad.isXInputRumbleReceived())
    {
        pushRumble();
        Serial.println("[XInput] rumble strong=" + String(bleGamepad.getXInputStrongMotor()) +
                       " weak=" + String(bleGamepad.getXInputWeakMotor()));
    }

    if (NuSerial.isConnected() && millis() - lastStateTime >= STATE_INTERVAL_MS)
    {
        lastStateTime = millis();
        pushState();
    }
}
