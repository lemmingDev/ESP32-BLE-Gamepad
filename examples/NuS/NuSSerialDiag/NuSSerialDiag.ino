/*
 * An auto-pressing gamepad with a Nordic UART Service (NUS) serial channel
 * alongside it, via the external NuS-NimBLE-Serial library
 * (https://github.com/afpineda/NuS-NimBLE-Serial, CC BY 4.0).
 *
 * Intended as a quick diagnostics tool rather than a real controller: install
 * NuS-NimBLE-Serial from the Arduino Library Manager, then connect any BLE
 * UART terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) to the
 * same device. As soon as it subscribes to NUS notifications it gets a
 * greeting, then a status line proactively every few seconds (not just replies
 * to what it sends) - this confirms the NUS TX path works without needing any
 * input. Anything sent that isn't a recognised command is echoed straight
 * back; send "help" for the list of commands.
 *
 * BUTTON_3 is pressed automatically every 10s and released 0.5s later, to test
 * the gamepad HID path continuously. Sending "button4" over NUS presses and
 * holds BUTTON_4 for 5s before releasing it, as an on-demand test that NUS
 * input, NUS output and the gamepad HID path are all working together.
 *
 * The reported battery level ramps up and down between 25% and 95%, and the
 * onboard LED blinks slowly while waiting for a BLE connection and quickly
 * once connected.
 *
 * Init order matters (see docs/NuSCompatibility.md): the gamepad is started
 * with delayAdvertising=true so advertising stays off while the NuS service
 * registers via NuSerial.start(false). Advertising is then started manually,
 * once, for both services together. Do NOT use NuSerial.begin() here - it
 * enables automatic advertising and would stomp the HID advertising setup.
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
#define NUS_PROFILE_ID "nus-diag"
#define NUS_PROTO_VER 1

// Nordic UART Service UUID, advertised in the scan response (see setup()).
#define NUS_ADV_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Fallback if the board package doesn't define one
#endif

#define STATUS_INTERVAL_MS 3000        // How often to proactively push a status line over NUS
#define BUTTON_PRESS_INTERVAL_MS 10000 // How often to auto-press BUTTON_3
#define BUTTON_HOLD_MS 500             // How long BUTTON_3 stays pressed
#define BUTTON4_HOLD_MS 5000           // How long BUTTON_4 stays pressed when triggered via NUS
#define BATTERY_STEP_INTERVAL_MS 1000  // How often to step the battery level
#define BATTERY_MIN 25
#define BATTERY_MAX 95
#define LED_BLINK_INTERVAL_DISCONNECTED_MS 1000 // Slow blink while waiting to connect
#define LED_BLINK_INTERVAL_CONNECTED_MS 150     // Fast blink once connected

// delayAdvertising=true: begin() builds the HID service and configures
// advertising but does not start it - NuS registers first (see setup()).
// Distinct name so this board is identifiable in a scanner next to other
// gamepad sketches using the library default.
BleGamepad bleGamepad("ESP32 BLE Gamepad Diag", "Espressif", 100, true);

unsigned long lastStatusTime = 0;
unsigned long lastButtonPressTime = 0;
bool buttonHeld = false;
unsigned long buttonPressStartTime = 0;

bool button4Held = false;
unsigned long button4PressStartTime = 0;

uint8_t batteryLevel = BATTERY_MIN;
int8_t batteryStep = 1;
unsigned long lastBatteryStepTime = 0;

bool ledState = false;
unsigned long lastLedToggleTime = 0;

size_t lastNusSubscribers = 0;
unsigned long nusGreetAt = 0; // millis() timestamp for the delayed greeting, 0 = none pending
String nusLine; // Accumulates one incoming NUS line

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    // This sketch only ever presses BUTTON_3/BUTTON_4, so trim the HID
    // descriptor down to just those - the oversized default was causing button
    // presses to be misread by Android's gamepad parser (report offsets
    // shifted, wrong button lit). Keep one HAT or the pad is not registered
    // on Android; it is never used, so just leave it at 1.
    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setButtonCount(8); // covers BUTTON_1..BUTTON_8
    bleGamepadConfig.setHatSwitchCount(1);
    bleGamepadConfig.setWhichAxes(false, false, false, false, false, false, false, false);

    bleGamepad.begin(&bleGamepadConfig);

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
        Serial.println("[Diagnostics] WARNING: NuS UUID did not fit advertising data.");
    }
    // Short alias in the scan response so filtered scanner views (e.g. nRF
    // Connect filtered by service UUID) show a name instead of N/A. The full
    // device name cannot fit here alongside the 128-bit NUS UUID, so this
    // stays a stub - the GAP/GATT display name is unaffected. With scan
    // responses enabled, setName() targets the scan data only.
    if (!pAdvertising->setName("ESP32-Diag"))
    {
        Serial.println("[Diagnostics] WARNING: NuS alias did not fit scan response.");
    }

    // Advertise once for both services together.
    pAdvertising->start();

    bleGamepad.setBatteryLevel(batteryLevel);

    Serial.println("[Diagnostics] Ready - waiting for a BLE connection. Send 'help' over NUS once connected for a list of commands.");
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

void handleCommand(const String &received)
{
    String command = received;
    command.toLowerCase();

    if (command == "help")
    {
        NuSerial.println("Available commands:");
        NuSerial.println("  help     - show this help message");
        NuSerial.println("  proto?   - show sketch profile id and protocol version");
        NuSerial.println("  button4  - press BUTTON_4 for 5s (tests NUS input/output + gamepad HID)");
        NuSerial.println("Anything else is echoed straight back.");
    }
    else if (command == "proto?")
    {
        NuSerial.println("proto " NUS_PROFILE_ID " " + String(NUS_PROTO_VER));
    }
    else if (command == "button4")
    {
        if (!button4Held)
        {
            button4Held = true;
            button4PressStartTime = millis();
            bleGamepad.press(BUTTON_4);
            NuSerial.println("BUTTON_4 pressed - will release in 5s.");
        }
        else
        {
            NuSerial.println("BUTTON_4 is already held.");
        }
    }
    else
    {
        NuSerial.println("Echo: " + received);
    }
}

void loop()
{
    bool connected = bleGamepad.isConnected();

    // --- LED: slow blink while disconnected, fast blink while connected ---
    unsigned long ledInterval = connected ? LED_BLINK_INTERVAL_CONNECTED_MS : LED_BLINK_INTERVAL_DISCONNECTED_MS;
    if (millis() - lastLedToggleTime >= ledInterval)
    {
        lastLedToggleTime = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }

    // Greet new NUS subscribers. NuSerial is a singleton and cannot be
    // subclassed, so there is no subscribe callback - poll the subscriber
    // count instead. This is the real "NUS is connected" signal, separate
    // from the general BLE link (a central can be connected without ever
    // subscribing, in which case NUS pushes would silently go nowhere).
    size_t subs = NuSerial.subscriberCount();
    if (subs == 0)
    {
        if (lastNusSubscribers > 0)
        {
            Serial.println("[NUS] all subscribers gone");
        }
        nusGreetAt = 0;
    }
    else if (subs > lastNusSubscribers)
    {
        // New arrival(s): hold the greeting 500ms so the notify handler is
        // attached before we push (a notify sent during CCCD enable can be
        // lost in the race - the client can always ask again via 'proto?').
        // Triggers on ANY increase, not just 0->1, so later subscribers are
        // greeted too (one greeting covers simultaneous arrivals).
        nusGreetAt = millis() + 500;
        Serial.printf("[NUS] subscriber appeared (free_heap=%u)\n", ESP.getFreeHeap());
    }
    else if (nusGreetAt != 0 && (long)(millis() - nusGreetAt) >= 0)
    {
        nusGreetAt = 0;
        NuSerial.println("hello " NUS_PROFILE_ID " " + String(NUS_PROTO_VER));
        NuSerial.println("[NUS] Subscribed. Send 'help' for a list of commands.");
    }
    lastNusSubscribers = subs;

    // A NUS-only client (e.g. a terminal app that connects and subscribes
    // without ever bonding, since NUS doesn't require the encryption the HID
    // profile does) can be fully reachable over NUS while
    // bleGamepad.isConnected() (gamepad HID authentication) is still false.
    // So NUS input/output below must not be gated on `connected` - only the
    // gamepad-specific behaviours (button auto-press, battery reporting) are.
    if (!connected)
    {
        buttonHeld = false;
        button4Held = false;
    }
    else
    {
        // --- Auto-press BUTTON_3 every 10s, releasing it 0.5s later ---
        if (!buttonHeld && millis() - lastButtonPressTime >= BUTTON_PRESS_INTERVAL_MS)
        {
            lastButtonPressTime = millis();
            buttonPressStartTime = lastButtonPressTime;
            buttonHeld = true;
            bleGamepad.press(BUTTON_3);
        }
        else if (buttonHeld && millis() - buttonPressStartTime >= BUTTON_HOLD_MS)
        {
            buttonHeld = false;
            bleGamepad.release(BUTTON_3);
        }

        // --- Release BUTTON_4 once its 5s NUS-triggered hold has elapsed ---
        if (button4Held && millis() - button4PressStartTime >= BUTTON4_HOLD_MS)
        {
            button4Held = false;
            bleGamepad.release(BUTTON_4);
            NuSerial.println("BUTTON_4 released after 5s hold.");
        }

        // --- Ramp the reported battery level up and down between 25% and 95% ---
        if (millis() - lastBatteryStepTime >= BATTERY_STEP_INTERVAL_MS)
        {
            lastBatteryStepTime = millis();

            batteryLevel += batteryStep;
            if (batteryLevel >= BATTERY_MAX)
            {
                batteryLevel = BATTERY_MAX;
                batteryStep = -1;
            }
            else if (batteryLevel <= BATTERY_MIN)
            {
                batteryLevel = BATTERY_MIN;
                batteryStep = 1;
            }
            bleGamepad.setBatteryLevel(batteryLevel);
        }
    }

    // --- Handle commands received over NUS ---
    String command = readNuSLine();
    if (command.length() > 0)
    {
        Serial.println("[NUS] got: " + command);
        handleCommand(command);
    }

    // --- Periodic status line over NUS, to confirm the TX path works ---
    // Only push when someone is subscribed - otherwise the bytes go nowhere.
    if (NuSerial.isConnected() && millis() - lastStatusTime >= STATUS_INTERVAL_MS)
    {
        lastStatusTime = millis();

        String status = "uptime_ms=" + String(millis()) +
                        " gamepad_connected=" + (bleGamepad.isConnected() ? "yes" : "no") +
                        " button3=" + (buttonHeld ? "PRESSED" : "released") +
                        " button4=" + (button4Held ? "PRESSED" : "released") +
                        " battery=" + String(batteryLevel) +
                        " free_heap=" + String(ESP.getFreeHeap());
        if (bleGamepad.isConnected())
        {
            status += " host_mac=" + bleGamepad.getStringAddress();
        }
        NuSerial.println(status);
        Serial.println(status);
    }
}
