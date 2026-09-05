/*
 * Generic-mode gamepad driven from a BLE terminal over NUS.
 *
 * Requires the external NuS-NimBLE-Serial library
 * (https://github.com/afpineda/NuS-NimBLE-Serial, CC BY 4.0), installable
 * from the Arduino Library Manager. Connect with any Nordic UART capable
 * terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) and type
 * "help" for the command list.
 *
 * This is the Generic-mode bridge: press/release buttons, move axes, set the
 * hat switch and battery level from the terminal, with a state summary pushed
 * back every few seconds. For the SInput and XInput equivalents (which also
 * surface host-to-device output reports), see NuSSInputBridge and
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
BleGamepad bleGamepad("ESP32 Gamepad NuS Generic", "Espressif", 100, true);

unsigned long lastStateTime = 0;
size_t lastNusSubscribers = 0;
String nusLine; // Accumulates one incoming NUS line

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

    // Advertise once for both services together.
    NimBLEDevice::getServer()->getAdvertising()->start();

    Serial.println("[NuSGenericBridge] Ready. Connect a BLE terminal and send 'help'.");
}

void printHelp()
{
    NuSerial.println("Commands:");
    NuSerial.println("  help                 - show this message");
    NuSerial.println("  press <1..16>        - press a button");
    NuSerial.println("  release <1..16>      - release a button");
    NuSerial.println("  axis <name> <value>  - x y z rx ry rz s1 s2, value -32767..32767");
    NuSerial.println("  hat <0..8>           - 0=centered 1=up 2=up-right ... 8=up-left");
    NuSerial.println("  battery <0..100>     - set reported battery level");
    NuSerial.println("  status               - reply with current state immediately");
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

    if (cmd == "help")
    {
        printHelp();
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

void loop()
{
    // Greet new subscribers (NuSerial is a singleton - no subscribe callback
    // to override, so poll the count). Writes with no subscriber go nowhere,
    // so the pushes below are additionally gated on isConnected().
    size_t subs = NuSerial.subscriberCount();
    if (subs > 0 && lastNusSubscribers == 0)
    {
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
