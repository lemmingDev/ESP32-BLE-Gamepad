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
 * This is the XInput-mode bridge (Xbox One S, VID 0x045E / PID 0x02FD):
 * drive A/B/X/Y/LB/RB/sticks/triggers/D-pad from the terminal, and query (or
 * get pushed) the last rumble report the Xbox host sent via Output Report
 * 0x03 (strong/weak motors + trigger magnitudes). Change one line to
 * GamepadMode::XInputSeriesX for the Series X variant (PID 0x0B13, Share
 * button) - recommended for native XInput over BLE on Win11 22H2+. For the
 * Generic and SInput equivalents, see NuSGenericBridge and NuSSInputBridge
 * in this folder.
 *
 * Init order matters (see docs/NuSCompatibility.md): delayAdvertising=true,
 * wait for the NimBLE server, NuSerial.start(false), then start advertising
 * manually. Do NOT use NuSerial.begin() - it enables automatic advertising
 * and would stomp the HID advertising setup.
 */

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad
#include <NuSerial.hpp> // https://github.com/afpineda/NuS-NimBLE-Serial
#include <NimBLEDevice.h>

#define STATE_INTERVAL_MS 3000 // How often to push a state summary to subscribers

// delayAdvertising=true: begin() builds the HID service and configures
// advertising but does not start it - NuS registers first (see setup()).
// Note: begin() overrides the device name to "Xbox Wireless Controller" in
// XInput modes (the Xbox driver matches on it), so the name below only
// matters before begin() runs.
BleGamepad bleGamepad("ESP32 Gamepad NuS XInput", "Espressif", 100, true);
BleGamepadConfiguration config;

unsigned long lastStateTime = 0;
size_t lastNusSubscribers = 0;
String nusLine; // Accumulates one incoming NUS line

void setup()
{
    Serial.begin(115200);

    // XInput One S mode: 11 buttons (A/B/X/Y/LB/RB/LS/RS/Select/Start/Home),
    // Xbox VID/PID/serial. Do not override setVid()/setPid() - the host Xbox
    // driver recognises the device by that exact pair.
    config.setGamepadMode(GamepadMode::XInputOneS);
    bleGamepad.begin(&config);

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

    Serial.println("[NuSXInputBridge] Ready. Connect a BLE terminal and send 'help'.");
}

void printHelp()
{
    NuSerial.println("Commands (buttons: 1=A 2=B 3=X 4=Y 5=LB 6=RB 7=LS 8=RS 9=Select 10=Start 11=Home):");
    NuSerial.println("  help                  - show this message");
    NuSerial.println("  press <1..11>         - press a button");
    NuSerial.println("  release <1..11>       - release a button");
    NuSerial.println("  stick left <x> <y>    - left stick, each -32767..32767");
    NuSerial.println("  stick right <x> <y>   - right stick, each -32767..32767");
    NuSerial.println("  trigger <l> <r>       - triggers, each 0..32767");
    NuSerial.println("  hat <0..8>            - D-pad: 0=centered 1=up 2=up-right ... 8=up-left");
    NuSerial.println("  rumble?               - last rumble report from Xbox host");
    NuSerial.println("  status                - reply with current state immediately");
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
    NuSerial.println(s);
}

void handleCommand(String cmd)
{
    cmd.toLowerCase();

    if (cmd == "help") { printHelp(); return; }
    if (cmd == "status") { pushState(); return; }
    if (cmd == "rumble?") { pushRumble(); return; }

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
            if (which == "left") { bleGamepad.setLeftThumb(x, y); NuSerial.println("ok " + cmd); }
            else if (which == "right") { bleGamepad.setRightThumb(x, y); NuSerial.println("ok " + cmd); }
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
    if (subs > 0 && lastNusSubscribers == 0)
    {
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
