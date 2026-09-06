/*
 * An auto-pressing gamepad with the Nordic UART Service (NUS) also enabled.
 * Intended as a quick diagnostics tool rather than a real controller: connect any
 * BLE UART terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) to the
 * device. As soon as it subscribes to NUS notifications it gets a greeting, then
 * a status line proactively every few seconds (not just replies to what it sends)
 * - this confirms the NUS TX path works without needing any input. Anything sent
 * that isn't a recognised command is echoed straight back; send "help" for the
 * list of commands.
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
 * Requires the external NuS-NimBLE-Serial library
 * (https://github.com/afpineda/NuS-NimBLE-Serial, CC BY 4.0), installable
 * from the Arduino Library Manager. See docs/NuSCompatibility.md for details.
 */

#include <Arduino.h>
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad
#if !__has_include("NuSerial.hpp")
#error "Install NuS-NimBLE-Serial from the Arduino Library Manager (see docs/NuSCompatibility.md)"
#endif
#include <NuSerial.hpp> // https://github.com/afpineda/NuS-NimBLE-Serial
#include <NimBLEDevice.h>

#define STATUS_INTERVAL_MS 3000        // How often to proactively push a status line over NUS
#define BUTTON_PRESS_INTERVAL_MS 10000 // How often to auto-press BUTTON_3
#define BUTTON_HOLD_MS 500             // How long BUTTON_3 stays pressed
#define BUTTON4_HOLD_MS 5000           // How long BUTTON_4 stays pressed when triggered via NUS
#define BATTERY_STEP_INTERVAL_MS 1000  // How often to step the battery level
#define BATTERY_MIN 25
#define BATTERY_MAX 95
#define LED_BLINK_INTERVAL_DISCONNECTED_MS 1000 // Slow blink while waiting to connect
#define LED_BLINK_INTERVAL_CONNECTED_MS 150     // Fast blink once connected

// Nordic UART Service UUID, advertised in the scan response (see setup()).
#define NUS_ADV_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"

// delayAdvertising=true: begin() builds the HID service and configures
// advertising but does not start it - NuSerial registers first (see setup()).
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
String nusLine;               // Accumulates one incoming NUS line

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

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    // The library's default config reports 16 buttons, a hat switch and all 8 axes.
    // This sketch only ever presses BUTTON_3/BUTTON_4, so trim the HID descriptor down
    // to just those - the oversized default was causing button presses to be
    // misread by Android's gamepad parser (report offsets shifted, wrong button lit).
    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setButtonCount(8); // covers BUTTON_1..BUTTON_8
    // Need one HAT or else gamepad is not registered on AndroidTV. The hat is never used, so just leave it at 1.
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

    // Advertise the NuS service UUID in the scan response.
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getServer()->getAdvertising();
    pAdvertising->enableScanResponse(true);
    if (!pAdvertising->addServiceUUID(NUS_ADV_UUID))
    {
        Serial.println("[Diagnostics] WARNING: NuS UUID did not fit advertising data.");
    }
    if (!pAdvertising->setName("ESP32-Diag"))
    {
        Serial.println("[Diagnostics] WARNING: NuS alias did not fit scan response.");
    }
    pAdvertising->start();

    bleGamepad.setBatteryLevel(batteryLevel);

    Serial.println("[Diagnostics] Ready - waiting for a BLE connection. Send 'help' over NUS once connected for a list of commands.");
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
        nusGreetAt = millis() + 500;
    }
    else if (nusGreetAt != 0 && (long)(millis() - nusGreetAt) >= 0)
    {
        nusGreetAt = 0;
        NuSerial.println("[NUS] Subscribed. Send 'help' for a list of commands.");
    }
    lastNusSubscribers = subs;

    // A NUS-only client (e.g. a terminal app that connects and subscribes without ever
    // bonding, since NUS doesn't require the encryption the HID profile does) can be fully
    // reachable over NUS while bleGamepad.isConnected() (gamepad HID authentication) is still
    // false. So NUS input/output below must not be gated on `connected` - only the gamepad-
    // specific behaviours (button auto-press, battery reporting) are.

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
        command.toLowerCase();

        if (command == "help")
        {
            NuSerial.println("Available commands:");
            NuSerial.println("  help     - show this help message");
            NuSerial.println("  button4  - press BUTTON_4 for 5s (tests NUS input/output + gamepad HID)");
            NuSerial.println("Anything else is echoed straight back.");
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
            NuSerial.println("Echo: " + command);
        }
    }

    // --- Periodic status line over NUS, to confirm the TX path works ---
    if (NuSerial.isConnected() && millis() - lastStatusTime >= STATUS_INTERVAL_MS)
    {
        lastStatusTime = millis();

        String status = "uptime_ms=" + String(millis()) +
                         " gamepad_connected=" + (bleGamepad.isConnected() ? "yes" : "no") +
                         " host_mac=" + String(bleGamepad.getAddress().toString().c_str()) +
                         " button3=" + (buttonHeld ? "PRESSED" : "released") +
                         " button4=" + (button4Held ? "PRESSED" : "released") +
                         " battery=" + String(batteryLevel) +
                         " free_heap=" + String(ESP.getFreeHeap());
        NuSerial.println(status);
        Serial.println(status);
    }
}
