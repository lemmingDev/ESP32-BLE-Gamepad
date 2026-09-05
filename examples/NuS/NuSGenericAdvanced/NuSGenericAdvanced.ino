/*
 * Generic-mode gamepad driven from a BLE terminal over NUS, extended with
 * special buttons and HID output/feature reports.
 *
 * Requires the external NuS-NimBLE-Serial library
 * (https://github.com/afpineda/NuS-NimBLE-Serial, CC BY 4.0), installable
 * from the Arduino Library Manager. Connect with any Nordic UART capable
 * terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) and type
 * "help" for the command list.
 *
 * This is the advanced Generic-mode bridge: everything NuSGenericBridge does
 * (buttons, axes, hat, battery, power state, bond/TX-power management), plus
 * start/select special buttons and bidirectional HID output/feature reports:
 * host Output Reports are pushed to the terminal as they arrive, and the
 * Feature Report can be read and written. Enabling those costs three config
 * lines (see setup()) - for the pure library-default version with nothing
 * enabled beyond defaults, use NuSGenericBridge instead.
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
#define NUS_PROFILE_ID "nus-bridge/generic-advanced"
#define NUS_PROTO_VER 1

#define STATE_INTERVAL_MS 3000 // How often to push a state summary to subscribers
#define REPORT_LEN 64          // Must match the output/feature report lengths in setup()

// delayAdvertising=true: begin() builds the HID service and configures
// advertising but does not start it - NuS registers first (see setup()).
BleGamepad bleGamepad("ESP32 Gamepad NuS Adv", "Espressif", 100, true);
BleGamepadConfiguration advConfig;

unsigned long lastStateTime = 0;
size_t lastNusSubscribers = 0;
String nusLine; // Accumulates one incoming NUS line

uint8_t lastOutput[REPORT_LEN];
bool haveOutput = false;
uint8_t lastFeature[REPORT_LEN];
bool haveFeature = false;

void setup()
{
    Serial.begin(115200);

    // Library defaults (16 buttons, 8 axes, 1 hat), plus the three extras
    // this sketch needs: start/select special buttons (otherwise the
    // `special` command would set bits no host can see), and the HID output
    // and feature reports (otherwise `output?`/`feature` have nothing to
    // talk to). Everything else stays at defaults.
    advConfig.setWhichSpecialButtons(true, true, false, false, false, false, false, false);
    advConfig.setEnableOutputReport(true);
    advConfig.setOutputReportLength(REPORT_LEN);
    advConfig.setEnableFeatureReport(true);
    advConfig.setFeatureReportLength(REPORT_LEN);
    bleGamepad.begin(&advConfig);

    // begin() initialises NimBLE asynchronously on its own task. NuSerial
    // needs the stack up first, so wait for the server to exist.
    while (NimBLEDevice::getServer() == nullptr)
    {
        delay(10);
    }

    // false = register the NuS service but leave advertising alone.
    NuSerial.start(false);

    // Advertise once for both services together.
    NimBLEDevice::getServer()->getAdvertising()->start();

    Serial.println("[NuSGenericAdvanced] Ready. Connect a BLE terminal and send 'help'.");
}

void printHelp()
{
    NuSerial.println("Commands (16 buttons, 8 axes, 1 hat + start/select + reports):");
    NuSerial.println("  help                 - show this message");
    NuSerial.println("  proto?               - show sketch profile id and protocol version");
    NuSerial.println("  press <1..16>        - press a button");
    NuSerial.println("  release <1..16>      - release a button");
    NuSerial.println("  special <start|select> <on|off>");
    NuSerial.println("  axis <name> <value>  - x y z rx ry rz s1 s2, value -32768..32767");
    NuSerial.println("  hat <0..8>           - 0=centered 1=up 2=up-right ... 8=up-left");
    NuSerial.println("  battery <0..100>     - set reported battery level");
    NuSerial.println("  power <b> <d> <c> <l> - battery power state, each 0..3");
    NuSerial.println("  output?              - last host Output Report as hex (none yet = no report)");
    NuSerial.println("  feature get          - last host Feature Report as hex");
    NuSerial.println("  feature set <hex>    - set Feature Report, e.g. 'feature set 0102ff'");
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

String toHex(const uint8_t *data, uint16_t len)
{
    String s;
    s.reserve(len * 2);
    for (uint16_t i = 0; i < len; i++)
    {
        if (data[i] < 0x10) s += "0";
        s += String(data[i], HEX);
    }
    return s;
}

// Parses even-length hex (spaces ignored) into buf. Returns byte count or -1.
int fromHex(const String &hex, uint8_t *buf, uint16_t maxLen)
{
    String h;
    for (unsigned int i = 0; i < hex.length(); i++)
    {
        if (hex[i] != ' ') h += hex[i];
    }
    if (h.length() == 0 || (h.length() % 2) != 0 || h.length() / 2 > maxLen) return -1;
    for (unsigned int i = 0; i < h.length(); i++)
    {
        char c = h[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return -1;
    }
    for (unsigned int i = 0; i < h.length() / 2; i++)
    {
        buf[i] = (uint8_t)strtoul(h.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    }
    return h.length() / 2;
}

bool setAxisByName(const String &name, int16_t value)
{
    if (name == "x") { bleGamepad.setX(value); return true; }
    if (name == "y") { bleGamepad.setY(value); return true; }
    if (name == "z") { bleGamepad.setZ(value); return true; }
    if (name == "rx") { bleGamepad.setRX(value); return true; }
    if (name == "ry") { bleGamepad.setRY(value); return true; }
    if (name == "rz") { bleGamepad.setRZ(value); return true; }
    if (name == "s1") { bleGamepad.setSlider1(value); return true; }
    if (name == "s2") { bleGamepad.setSlider2(value); return true; }
    return false;
}

void setSpecial(const String &which, bool on, bool &ok)
{
    ok = true;
    if (which == "start") { if (on) { bleGamepad.pressStart(); } else { bleGamepad.releaseStart(); } }
    else if (which == "select") { if (on) { bleGamepad.pressSelect(); } else { bleGamepad.releaseSelect(); } }
    else { ok = false; }
}

void pushState()
{
    String s = "state buttons=";
    for (int b = 1; b <= 16; b++)
    {
        s += bleGamepad.isPressed(b) ? "1" : "0";
    }
    NuSerial.println(s);
}

void handleCommand(String cmd)
{
    cmd.toLowerCase();

    if (cmd == "help") { printHelp(); return; }
    if (cmd == "proto?") { NuSerial.println("proto " NUS_PROFILE_ID " " + String(NUS_PROTO_VER)); return; }
    if (cmd == "status") { pushState(); return; }
    if (cmd == "output?")
    {
        NuSerial.println(haveOutput ? "output " + toHex(lastOutput, REPORT_LEN) : "output none yet");
        return;
    }
    if (cmd == "feature get")
    {
        NuSerial.println(haveFeature ? "feature " + toHex(lastFeature, REPORT_LEN) : "feature none yet");
        return;
    }
    if (cmd.startsWith("feature set "))
    {
        uint8_t buf[REPORT_LEN];
        int n = fromHex(cmd.substring(12), buf, REPORT_LEN);
        if (n > 0)
        {
            bleGamepad.setFeatureBuffer(buf, (uint16_t)n);
            NuSerial.println("ok feature set " + String(n) + " bytes");
        }
        else
        {
            NuSerial.println("err usage: feature set <hex>, even digits, max 128 chars");
        }
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
    if (cmd.startsWith("special "))
    {
        // special <start|select> <on|off>
        int sp = cmd.indexOf(' ', 8);
        if (sp > 0)
        {
            bool ok = false;
            String onoff = cmd.substring(sp + 1);
            if (onoff == "on" || onoff == "off")
            {
                setSpecial(cmd.substring(8, sp), onoff == "on", ok);
            }
            NuSerial.println(ok ? "ok " + cmd : "err usage: special <start|select> <on|off>");
        }
        else
        {
            NuSerial.println("err usage: special <start|select> <on|off>");
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

// Surface host-to-device HID reports as they arrive. The flags are
// consume-on-read, so cache the bytes locally for later `output?` queries.
void pollHostReports()
{
    if (bleGamepad.isOutputReceived())
    {
        uint8_t *buf = bleGamepad.getOutputBuffer();
        if (buf)
        {
            memcpy(lastOutput, buf, REPORT_LEN);
            haveOutput = true;
            NuSerial.println("event output " + toHex(lastOutput, REPORT_LEN));
            Serial.println("[HID] output report received");
        }
    }
    if (bleGamepad.isFeatureReceived())
    {
        uint8_t *buf = bleGamepad.getFeatureBuffer();
        if (buf)
        {
            memcpy(lastFeature, buf, REPORT_LEN);
            haveFeature = true;
            NuSerial.println("event feature " + toHex(lastFeature, REPORT_LEN));
            Serial.println("[HID] feature report received");
        }
    }
}

void loop()
{
    // Greet new subscribers (NuSerial is a singleton - no subscribe callback
    // to override, so poll the count).
    size_t subs = NuSerial.subscriberCount();
    if (subs > 0 && lastNusSubscribers == 0)
    {
        NuSerial.println("hello " NUS_PROFILE_ID " " + String(NUS_PROTO_VER));
        NuSerial.println("[NuS] Advanced Generic bridge ready. Send 'help'.");
    }
    lastNusSubscribers = subs;

    String cmd = readNuSLine();
    if (cmd.length() > 0)
    {
        Serial.println("[NUS] got: " + cmd);
        handleCommand(cmd);
    }

    pollHostReports();

    if (NuSerial.isConnected() && millis() - lastStateTime >= STATE_INTERVAL_MS)
    {
        lastStateTime = millis();
        pushState();
    }
}
