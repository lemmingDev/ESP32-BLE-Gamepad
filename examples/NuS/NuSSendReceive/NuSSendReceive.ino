/*
 * NuS Send/Receive — generic data over Nordic UART (tutorial).
 *
 * Shows the two directions of a Nordic UART (NuS) serial link with data that
 * means nothing but what YOU decide it means: no buttons, no sticks, no
 * gamepad reports involved. Copy the receive loop and the send helpers into
 * your own project and define your own vocabulary on top.
 *
 * DIRECTION 1 — RECEIVE (phone/PC terminal -> ESP32 -> act on it):
 *   1. The terminal sends a text line ending in '\n', e.g. "echo hello".
 *   2. loop() collects incoming bytes into one line (readNuSLine) and passes
 *      the trimmed line to handleCommand().
 *   3. handleCommand() parses the line and ACTS: here that means echoing
 *      text back, reading ESP state (millis, heap), or toggling the onboard
 *      LED. In your project this is where you drive motors, relays,
 *      displays — anything.
 *   4. Always reply "ok ..." or "err ..." so the sender knows what happened.
 *
 * DIRECTION 2 — SEND (ESP32 -> terminal):
 *   - Replies: every command gets an immediate "ok ..."/"err ..." line.
 *   - Greeting: new subscribers get "hello <profile> <ver>" (see the
 *     subscriber poll in loop()) so the terminal can confirm what firmware
 *     it reached. Re-query anytime with "proto?".
 *   - Telemetry: loop() pushes a "data ..." line every few seconds without
 *     being asked. This is the pattern for sensor readings, counters, or
 *     any ESP-side event you want the terminal to see unprompted.
 *
 * TRY IT:
 *   1. Install NuS-NimBLE-Serial from the Arduino Library Manager, flash this.
 *   2. Open nRF Connect or Serial Bluetooth Terminal, scan for "SendRx-NuS",
 *      connect, subscribe to the TX characteristic (UUID ...0003).
 *   3. Send "help". Then try:
 *        echo hello world     -> back comes "echo hello world"
 *        millis?              -> back comes "millis 12345"
 *        heap?                -> back comes "heap 234567"
 *        led on               -> onboard LED lights, back comes "ok led on"
 *        led off
 *        status               -> back comes one "state ..." line
 *      Every ~3 s an unprompted "data ..." line arrives on its own.
 *
 * Init order matters (see docs/NuSCompatibility.md): this sketch runs the
 * BLE gamepad alongside NuS, so delayAdvertising=true, wait for the NimBLE
 * server, NuSerial.start(false), then start advertising manually. Do NOT use
 * NuSerial.begin() — it enables automatic advertising and would stomp the
 * HID advertising setup. The gamepad itself is idle here; the NuS data path
 * below is independent of it.
 */

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad
#if !__has_include("NuSerial.hpp")
#error "Install NuS-NimBLE-Serial from the Arduino Library Manager (see docs/NuSCompatibility.md)"
#endif
#include <NuSerial.hpp> // https://github.com/afpineda/NuS-NimBLE-Serial
#include <NimBLEDevice.h>

// Machine-readable sketch identity, greeted on subscribe.
// Queryable via the 'proto?' command.
#define NUS_PROFILE_ID "nus-send-receive"
#define NUS_PROTO_VER 1

// Nordic UART Service UUID, advertised in the scan response (see setup()).
#define NUS_ADV_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

// How often to push an unprompted telemetry line.
#define DATA_INTERVAL_MS 3000

// delayAdvertising=true: begin() builds the HID service and configures
// advertising but does not start it — NuS registers first (see setup()).
BleGamepad bleGamepad("Gamepad NuS SendReceive", "lemmingDev", 100, true);
BleGamepadConfiguration config;

unsigned long nusGreetAt = 0; // millis() timestamp for the delayed greeting, 0 = none pending
size_t lastNusSubscribers = 0;
String nusLine; // Accumulates one incoming NUS line
unsigned long lastDataTime = 0;
bool ledOn = false;

// Onboard LED (GPIO 2 on most dev boards). LED_BUILTIN is not defined for
// every ESP32 target, so fall back explicitly.
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

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
    // packet is already full, so addServiceUUID overflows the 128-bit NUS
    // UUID into the scan response payload automatically once scan responses
    // are enabled. Active scanners then see NUS for service-UUID filtering.
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getServer()->getAdvertising();
    pAdvertising->enableScanResponse(true);
    if (!pAdvertising->addServiceUUID(NUS_ADV_UUID))
    {
        Serial.println("[NuSSendReceive] WARNING: NuS UUID did not fit advertising data.");
    }
    // Short alias in the scan response (MUST stay <= 11 chars so it fits
    // next to the 128-bit NUS UUID: 18 + len + 2 <= 31).
    if (!pAdvertising->setName("SendRx-NuS"))
    {
        Serial.println("[NuSSendReceive] WARNING: NuS alias did not fit scan response.");
    }

    // Advertise once for both services together.
    pAdvertising->start();

    Serial.println("[NuSSendReceive] Ready. Connect a BLE terminal and send 'help'.");
}

void printHelp()
{
    NuSerial.println("Commands (generic data demo — nothing here touches the gamepad):");
    NuSerial.println("  help            - show this message");
    NuSerial.println("  proto?          - show sketch profile id and protocol version");
    NuSerial.println("  echo <anything> - receive freeform text, send it straight back");
    NuSerial.println("  millis?         - reply with the ESP32 millisecond counter");
    NuSerial.println("  heap?           - reply with free heap in bytes");
    NuSerial.println("  led <on|off>    - act on hardware: onboard LED on/off");
    NuSerial.println("  status          - reply with one state line immediately");
}

// SEND path: one telemetry line, pushed unprompted on a timer (see loop())
// and on demand via "status". Same line shape both ways.
void pushData()
{
    NuSerial.println("data millis=" + String(millis()) +
                     " heap=" + String(ESP.getFreeHeap()) +
                     " led=" + String(ledOn ? "on" : "off"));
}

// RECEIVE path: one trimmed line in, parse + ACT + reply out.
// Convention: "ok <what>" on success, "err usage: ..." on bad input so the
// terminal (or your own sender code) can react programmatically.
void handleCommand(String cmd)
{
    // NOTE: echo matching runs before lowercasing so echoed text keeps
    // its original case.
    if (cmd.startsWith("echo ") || cmd.startsWith("ECHO "))
    {
        String text = cmd.substring(5); // everything after "echo "
        NuSerial.println("echo " + text); // ACT: send the data straight back
        return;
    }

    cmd.toLowerCase();

    if (cmd == "help") { printHelp(); return; }
    if (cmd == "proto?") { NuSerial.println("proto " NUS_PROFILE_ID " " + String(NUS_PROTO_VER)); return; }
    if (cmd == "status") { pushData(); return; }
    if (cmd == "millis?") { NuSerial.println("millis " + String(millis())); return; }
    if (cmd == "heap?") { NuSerial.println("heap " + String(ESP.getFreeHeap())); return; }
    if (cmd.startsWith("led "))
    {
        String onoff = cmd.substring(4);
        if (onoff == "on" || onoff == "off")
        {
            ledOn = (onoff == "on");
            digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW); // ACT on hardware
            NuSerial.println("ok led " + onoff);
        }
        else
        {
            NuSerial.println("err usage: led <on|off>");
        }
        return;
    }
    NuSerial.println("err unknown command - send 'help'");
}

// Returns one complete trimmed line, or an empty String if none is ready yet.
// Lines are '\n'-terminated; a stray '\r' is ignored. Anything arriving
// without a newline simply waits here until the line completes.
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
                nusLine = ""; // Overlong line — drop it rather than grow forever
            }
        }
    }
    return String();
}

void loop()
{
    // Greet new subscribers (NuSerial is a singleton — no subscribe callback
    // to override, so poll the count). The 500 ms hold lets the notify
    // handler attach before we push (avoids losing hello to the CCCD race).
    // Triggers on ANY increase so later subscribers are greeted too.
    size_t subs = NuSerial.subscriberCount();
    if (subs == 0)
    {
        nusGreetAt = 0;
    }
    else if (subs > lastNusSubscribers)
    {
        nusGreetAt = millis() + 500;
    }
    else if (nusGreetAt != 0 && (long)(millis() - nusGreetAt) >= 0)
    {
        nusGreetAt = 0;
        NuSerial.println("hello " NUS_PROFILE_ID " " + String(NUS_PROTO_VER));
        NuSerial.println("[NuS] Send/receive demo ready. Send 'help'.");
    }
    lastNusSubscribers = subs;

    // RECEIVE: pump incoming lines into the command handler.
    String cmd = readNuSLine();
    if (cmd.length() > 0)
    {
        Serial.println("[NUS] got: " + cmd);
        handleCommand(cmd);
    }

    // SEND: unprompted telemetry on a timer. Gate on isConnected() so we
    // never queue notifies nobody will receive.
    if (NuSerial.isConnected() && millis() - lastDataTime >= DATA_INTERVAL_MS)
    {
        lastDataTime = millis();
        pushData();
    }
}
