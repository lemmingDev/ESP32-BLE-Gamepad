/*
 * SInput-mode gamepad driven from a BLE terminal over NUS, with host
 * output-report state (player LED, rumble, RGB) surfaced back to the terminal.
 *
 * Requires the external NuS-NimBLE-Serial library
 * (https://github.com/afpineda/NuS-NimBLE-Serial, CC BY 4.0), installable
 * from the Arduino Library Manager. Connect with any Nordic UART capable
 * terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) and type
 * "help" for the command list.
 *
 * This is the SInput-mode bridge: drive buttons/sticks/triggers/hat from the
 * terminal, and query (or get pushed) the last player-LED index, rumble
 * amplitudes and RGB colour the SInput host sent via Output Report 0x03.
 * SInput needs an SDL3 host with the SInput hint enabled for those reports;
 * plain button/axis input also works over the normal OS joystick path. For
 * the Generic and XInput equivalents, see NuSGenericBridge and
 * NuSXInputBridge in this folder.
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
BleGamepad bleGamepad("ESP32 Gamepad NuS SInput", "Espressif", 100, true);
BleGamepadConfiguration config;

unsigned long lastStateTime = 0;
size_t lastNusSubscribers = 0;
String nusLine; // Accumulates one incoming NUS line

void setup()
{
    Serial.begin(115200);

    // SInput mode: fixed 25 buttons, VID 0x2E8A / PID 0x10C6, SInput report
    // layout. Do not override setVid()/setPid() - SDL recognises the device
    // by that exact pair.
    config.setGamepadMode(GamepadMode::SInput);
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

    Serial.println("[NuSSInputBridge] Ready. Connect a BLE terminal and send 'help'.");
}

void printHelp()
{
    NuSerial.println("Commands:");
    NuSerial.println("  help                    - show this message");
    NuSerial.println("  press <1..25>           - press a button");
    NuSerial.println("  release <1..25>         - release a button");
    NuSerial.println("  stick left <x> <y>      - left stick, each -32767..32767");
    NuSerial.println("  stick right <x> <y>     - right stick, each -32767..32767");
    NuSerial.println("  trigger left <v>        - left trigger 0..32767");
    NuSerial.println("  trigger right <v>       - right trigger 0..32767");
    NuSerial.println("  hat <0..8>              - 0=centered 1=up 2=up-right ... 8=up-left");
    NuSerial.println("  battery <0..100>        - set reported battery level");
    NuSerial.println("  led?                    - last player-LED index from host (0=none)");
    NuSerial.println("  rumble?                 - last rumble amplitudes from host");
    NuSerial.println("  rgb?                    - last RGB colour from host");
    NuSerial.println("  status                  - reply with current state immediately");
}

void pushLed()
{
    NuSerial.println("event led " + String(bleGamepad.getPlayerLedIndex()));
}

void pushRumble()
{
    NuSerial.println("event rumble left=" + String(bleGamepad.getRumbleLeftAmplitude()) +
                     " right=" + String(bleGamepad.getRumbleRightAmplitude()));
}

void pushRgb()
{
    NuSerial.println("event rgb r=" + String(bleGamepad.getRgbRed()) +
                     " g=" + String(bleGamepad.getRgbGreen()) +
                     " b=" + String(bleGamepad.getRgbBlue()));
}

void pushState()
{
    String s = "state buttons=";
    for (int b = 1; b <= 25; b++)
    {
        s += bleGamepad.isPressed(b) ? "1" : "0";
    }
    s += " led=" + String(bleGamepad.getPlayerLedIndex());
    NuSerial.println(s);
}

void handleCommand(String cmd)
{
    cmd.toLowerCase();

    if (cmd == "help") { printHelp(); return; }
    if (cmd == "status") { pushState(); return; }
    if (cmd == "led?") { pushLed(); return; }
    if (cmd == "rumble?") { pushRumble(); return; }
    if (cmd == "rgb?") { pushRgb(); return; }

    if (cmd.startsWith("press ") || cmd.startsWith("release "))
    {
        bool press = cmd.startsWith("press ");
        int b = cmd.substring(press ? 6 : 8).toInt();
        if (b >= 1 && b <= 25)
        {
            if (press) { bleGamepad.press(b); } else { bleGamepad.release(b); }
            NuSerial.println(String(press ? "ok pressed " : "ok released ") + String(b));
        }
        else
        {
            NuSerial.println("err button must be 1..25");
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
        // trigger <left|right> <v>
        int s1 = cmd.indexOf(' ', 8);
        if (s1 > 0)
        {
            String which = cmd.substring(8, s1);
            int16_t v = (int16_t)cmd.substring(s1 + 1).toInt();
            if (which == "left") { bleGamepad.setLeftTrigger(v); NuSerial.println("ok " + cmd); }
            else if (which == "right") { bleGamepad.setRightTrigger(v); NuSerial.println("ok " + cmd); }
            else { NuSerial.println("err usage: trigger <left|right> <v>"); }
        }
        else
        {
            NuSerial.println("err usage: trigger <left|right> <v>");
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

// Surface SInput host-to-device reports as they arrive. These only come from
// an SInput-aware host (SDL3 with the SInput hint); the flags stay clear on
// the plain OS joystick path.
void pollSInputHostReports()
{
    if (bleGamepad.isPlayerLedReceived())
    {
        pushLed();
        Serial.println("[SInput] player LED " + String(bleGamepad.getPlayerLedIndex()));
    }
    if (bleGamepad.isRumbleReceived())
    {
        pushRumble();
        Serial.println("[SInput] rumble left=" + String(bleGamepad.getRumbleLeftAmplitude()) +
                       " right=" + String(bleGamepad.getRumbleRightAmplitude()));
    }
    if (bleGamepad.isRgbReceived())
    {
        pushRgb();
        Serial.println("[SInput] rgb r=" + String(bleGamepad.getRgbRed()) +
                       " g=" + String(bleGamepad.getRgbGreen()) +
                       " b=" + String(bleGamepad.getRgbBlue()));
    }
}

void loop()
{
    // Greet new subscribers (NuSerial is a singleton - no subscribe callback
    // to override, so poll the count).
    size_t subs = NuSerial.subscriberCount();
    if (subs > 0 && lastNusSubscribers == 0)
    {
        NuSerial.println("[NuS] SInput bridge ready. Send 'help'.");
    }
    lastNusSubscribers = subs;

    String cmd = readNuSLine();
    if (cmd.length() > 0)
    {
        Serial.println("[NUS] got: " + cmd);
        handleCommand(cmd);
    }

    pollSInputHostReports();

    if (NuSerial.isConnected() && millis() - lastStateTime >= STATE_INTERVAL_MS)
    {
        lastStateTime = millis();
        pushState();
    }
}
