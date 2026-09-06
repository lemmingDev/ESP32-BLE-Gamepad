/*
 * Generic-mode gamepad driven from a BLE terminal over NUS.
 *
 * Requires the external NuS-NimBLE-Serial library
 * (https://github.com/afpineda/NuS-NimBLE-Serial, CC BY 4.0), installable
 * from the Arduino Library Manager. Connect with any Nordic UART capable
 * terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) and type
 * "help" for the command list.
 *
 * This is the strict Generic-mode bridge: it runs on the pure library-default
 * configuration (16 buttons, 8 axes, 1 hat, no special buttons, no
 * output/feature reports), so every command below is live on the wire with
 * zero config. Press/release buttons, move axes, set the hat switch, battery
 * level and power state, and manage bonds/TX power from the terminal, with a
 * state summary pushed back every few seconds. For the same bridge plus
 * special buttons and HID output/feature reports, see NuSGenericAdvanced.
 * For the SInput and XInput equivalents, see NuSSInputBridge and
 * NuSXInputBridge in this folder.
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
#define NUS_PROFILE_ID "nus-bridge/generic-strict"
#define NUS_PROTO_VER 1

// Nordic UART Service UUID, advertised in the scan response (see setup()).
#define NUS_ADV_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

#define STATE_INTERVAL_MS 3000 // How often to push a state summary to subscribers

// delayAdvertising=true: begin() builds the HID service and configures
// advertising but does not start it - NuS registers first (see setup()).
BleGamepad bleGamepad("ESP32 Gamepad NuS Generic", "Espressif", 100, true);

unsigned long lastStateTime = 0;
size_t lastNusSubscribers = 0;
unsigned long nusGreetAt = 0; // millis() timestamp for the delayed greeting, 0 = none pending
String nusLine; // Accumulates one incoming NUS line

// Local mirrors of device state for the `status` readout. The library offers
// no getters for axes/hats/battery (only isPressed()), so the sketch tracks
// what it last commanded. Initial values match the library defaults.
int16_t stX = 0, stY = 0, stZ = 0, stRX = 0, stRY = 0, stRZ = 0, stS1 = 0, stS2 = 0;
int stHat = 0;
int stBattery = 100;
int stPower[4] = {0, 0, 0, 0};

void setup()
{
    Serial.begin(115200);

    // Default config: 16 buttons, all 8 axes, 1 hat - everything below is valid.
    bleGamepad.begin();

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
        Serial.println("[NuSGenericBridge] WARNING: NuS UUID did not fit advertising data.");
    }
    // Short alias in the scan response. You'll usually see THIS name, not the
    // full device name, in filtered scanner views - and it MUST stay <= 11
    // characters so it fits next to the 128-bit NUS UUID (18 + len + 2 <= 31).
    // The GAP/GATT display name is unaffected. With scan responses enabled,
    // setName() targets the scan data only.
    if (!pAdvertising->setName("Generic-NuS"))
    {
        Serial.println("[NuSGenericBridge] WARNING: NuS alias did not fit scan response.");
    }

    // Advertise once for both services together.
    pAdvertising->start();

    Serial.println("[NuSGenericBridge] Ready. Connect a BLE terminal and send 'help'.");
}

void printHelp()
{
    NuSerial.println("Commands (strict defaults: 16 buttons, 8 axes, 1 hat):");
    NuSerial.println("  help                 - show this message");
    NuSerial.println("  proto?               - show sketch profile id and protocol version");
    NuSerial.println("  press <1..16>        - press a button");
    NuSerial.println("  release <1..16>      - release a button");
    NuSerial.println("  axis <name> <value>  - x y z rx ry rz s1 s2, value -32768..32767");
    NuSerial.println("  hat <0..8>           - 0=centered 1=up 2=up-right ... 8=up-left");
    NuSerial.println("  battery <0..100>     - set reported battery level");
    NuSerial.println("  power <b> <d> <c> <l> - battery power state, each 0..3");
    NuSerial.println("  pair                 - enter pairing mode (BLOCKS till a new host pairs)");
    NuSerial.println("  unpair               - delete current bond");
    NuSerial.println("  unpairall confirm    - delete ALL bonds (needs the word 'confirm')");
    NuSerial.println("  txpower <-12..9>     - set BLE TX power (-12 -9 -6 -3 0 3 6 9)");
    NuSerial.println("  txpower?             - show BLE TX power");
    NuSerial.println("  addr?                - show this device's BLE address");
    NuSerial.println("  status               - reply with current state immediately");
}

bool validTxPower(int v)
{
    return v == -12 || v == -9 || v == -6 || v == -3 || v == 0 || v == 3 || v == 6 || v == 9;
}

bool setAxisByName(const String &name, int16_t value)
{
    if (name == "x") { bleGamepad.setX(value); stX = value; return true; }
    if (name == "y") { bleGamepad.setY(value); stY = value; return true; }
    if (name == "z") { bleGamepad.setZ(value); stZ = value; return true; }
    if (name == "rx") { bleGamepad.setRX(value); stRX = value; return true; }
    if (name == "ry") { bleGamepad.setRY(value); stRY = value; return true; }
    if (name == "rz") { bleGamepad.setRZ(value); stRZ = value; return true; }
    if (name == "s1") { bleGamepad.setSlider1(value); stS1 = value; return true; }
    if (name == "s2") { bleGamepad.setSlider2(value); stS2 = value; return true; }
    return false;
}

void pushState()
{
    String s = "state buttons=";
    for (int b = 1; b <= 16; b++)
    {
        s += bleGamepad.isPressed(b) ? "1" : "0";
    }
    s += " axes=" + String(stX) + "," + String(stY) + "," + String(stZ) + "," +
         String(stRX) + "," + String(stRY) + "," + String(stRZ) + "," +
         String(stS1) + "," + String(stS2);
    s += " hat=" + String(stHat);
    s += " battery=" + String(stBattery);
    s += " power=" + String(stPower[0]) + "," + String(stPower[1]) + "," +
         String(stPower[2]) + "," + String(stPower[3]);
    NuSerial.println(s);
}

void handleCommand(String cmd)
{
    cmd.toLowerCase();

    if (cmd == "help")
    {
        printHelp();
        return;
    }
    if (cmd == "proto?")
    {
        NuSerial.println("proto " NUS_PROFILE_ID " " + String(NUS_PROTO_VER));
        return;
    }
    if (cmd == "status")
    {
        pushState();
        return;
    }
    if (cmd.startsWith("press "))
    {
        int b = cmd.substring(6).toInt();
        if (b >= 1 && b <= 16)
        {
            bleGamepad.press(b);
            NuSerial.println("ok pressed " + String(b));
        }
        else
        {
            NuSerial.println("err button must be 1..16");
        }
        return;
    }
    if (cmd.startsWith("release "))
    {
        int b = cmd.substring(8).toInt();
        if (b >= 1 && b <= 16)
        {
            bleGamepad.release(b);
            NuSerial.println("ok released " + String(b));
        }
        else
        {
            NuSerial.println("err button must be 1..16");
        }
        return;
    }
    if (cmd.startsWith("axis "))
    {
        int sp = cmd.indexOf(' ', 5);
        if (sp > 0 && setAxisByName(cmd.substring(5, sp), (int16_t)cmd.substring(sp + 1).toInt()))
        {
            NuSerial.println("ok " + cmd);
        }
        else
        {
            NuSerial.println("err usage: axis <x|y|z|rx|ry|rz|s1|s2> <value>");
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
    // to override, so poll the count). Writes with no subscriber go nowhere,
    // so the pushes below are additionally gated on isConnected().
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
        NuSerial.println("[NuS] Generic bridge ready. Send 'help'.");
    }
    lastNusSubscribers = subs;

    String cmd = readNuSLine();
    if (cmd.length() > 0)
    {
        Serial.println("[NUS] got: " + cmd);
        handleCommand(cmd);
    }

    if (NuSerial.isConnected() && millis() - lastStateTime >= STATE_INTERVAL_MS)
    {
        lastStateTime = millis();
        pushState();
    }
}
