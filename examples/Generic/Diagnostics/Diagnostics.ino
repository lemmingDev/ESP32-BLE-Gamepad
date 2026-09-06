/*
 * An auto-pressing gamepad with the Nordic UART Service (NUS) also enabled.
 * Intended as a quick diagnostics tool rather than a real controller: connect any
 * BLE UART terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) to the same
 * device. As soon as it subscribes to NUS notifications it gets a greeting, then a
 * status line proactively every few seconds (not just replies to what it sends) -
 * this confirms the NUS TX path works without needing any input. Anything sent that
 * isn't a recognised command is echoed straight back; send "help" for the list of
 * commands.
 *
 * BUTTON_3 is pressed automatically every 10s and released 0.5s later, to test
 * the gamepad HID path continuously. Sending "button4" over NUS presses and
 * holds BUTTON_4 for 5s before releasing it, as an on-demand test that NUS
 * input, NUS output and the gamepad HID path are all working together.
 *
 * The reported battery level ramps up and down between 25% and 95%, and the
 * onboard LED blinks slowly while waiting for a BLE connection and quickly
 * once connected.
 */

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

#define STATUS_INTERVAL_MS 3000        // How often to proactively push a status line over NUS
#define BUTTON_PRESS_INTERVAL_MS 10000 // How often to auto-press BUTTON_3
#define BUTTON_HOLD_MS 500             // How long BUTTON_3 stays pressed
#define BUTTON4_HOLD_MS 5000           // How long BUTTON_4 stays pressed when triggered via NUS
#define BATTERY_STEP_INTERVAL_MS 1000  // How often to step the battery level
#define BATTERY_MIN 25
#define BATTERY_MAX 95
#define LED_BLINK_INTERVAL_DISCONNECTED_MS 1000 // Slow blink while waiting to connect
#define LED_BLINK_INTERVAL_CONNECTED_MS 150     // Fast blink once connected

// Default name is "ESP32 BLE Gamepad" - shared by every sketch in this workspace that uses
// the library's default BleGamepad constructor. With more than one board powered on, they're
// indistinguishable in a scanner and it's easy to connect to the wrong one (e.g. one with no
// NUS service at all, which looks exactly like "no serial profile found").
BleGamepad bleGamepad("ESP32 BLE Gamepad Diag");

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

// Fires when a central subscribes/unsubscribes to NUS TX notifications - this is the
// real "NUS is connected" signal, separate from the general BLE link (a central can be
// connected without ever subscribing, in which case NUS pushes would silently go nowhere).
void onNusSubscribeChanged(bool subscribed, const std::string &address)
{
    Serial.printf("[NUS] %s %s (free_heap=%u)\n", address.c_str(),
                  subscribed ? "subscribed - NUS output will now reach it" : "unsubscribed",
                  ESP.getFreeHeap());

    if (subscribed)
    {
        BleNUS *nus = bleGamepad.getNUS();
        if (nus)
        {
            nus->println("[NUS] Subscribed. Send 'help' for a list of commands.");
        }
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    // The library's default config reports 16 buttons, a hat switch and all 8 axes.
    // This sketch only ever presses BUTTON_3/BUTTON_4, so trim the HID descriptor down
    // to just those - the oversized default was causing button presses to be
    // misread by Android's gamepad parser (report offsets shifted, wrong button lit).
    // CONTROLLER_TYPE_JOYSTICK was tried here to chase a button-mapping mismatch, but it
    // changes the top-level HID usage from "Game Pad" (0x05) to "Joystick" (0x04), which is
    // why Android/the gamepad tester stopped recognising it as a gamepad (SOURCE_JOYSTICK
    // instead of SOURCE_GAMEPAD). Keep GAMEPAD - the button mismatch needs a different fix.
    BleGamepadConfiguration bleGamepadConfig;
    //bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
    //bleGamepadConfig.setControllerType(CONTROLLER_TYPE_JOYSTICK);

    bleGamepadConfig.setButtonCount(8); // covers BUTTON_1..BUTTON_8
    //bleGamepadConfig.setHatSwitchCount(0);
    // Need one HAT or else gamepad is not registered on AndroidTV. The hat is never used, so just leave it at 1.
    bleGamepadConfig.setHatSwitchCount(1);
    bleGamepadConfig.setWhichAxes(false, false, false, false, false, false, false, false);

    bleGamepad.begin(&bleGamepadConfig);
    bleGamepad.beginNUS(); // Adds the Nordic UART Service alongside the gamepad HID service
    bleGamepad.getNUS()->setSubscribeCallback(onNusSubscribeChanged);
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

    // A NUS-only client (e.g. a terminal app that connects and subscribes without ever
    // bonding, since NUS doesn't require the encryption the HID profile does) can be fully
    // reachable over NUS while bleGamepad.isConnected() (gamepad HID authentication) is still
    // false. So NUS input/output below must not be gated on `connected` - only the gamepad-
    // specific behaviours (button auto-press, battery reporting) are.
    BleNUS *nus = bleGamepad.getNUS();

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
            if (nus)
            {
                nus->println("BUTTON_4 released after 5s hold.");
            }
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

    if (!nus)
    {
        return;
    }

    // --- Handle commands received over NUS ---
    if (nus->available())
    {
        String received;
        while (nus->available())
        {
            received += (char)nus->read();
        }

        String command = received;
        command.trim();
        command.toLowerCase();

        if (command == "help")
        {
            nus->println("Available commands:");
            nus->println("  help     - show this help message");
            nus->println("  button4  - press BUTTON_4 for 5s (tests NUS input/output + gamepad HID)");
            nus->println("Anything else is echoed straight back.");
        }
        else if (command == "button4")
        {
            if (!button4Held)
            {
                button4Held = true;
                button4PressStartTime = millis();
                bleGamepad.press(BUTTON_4);
                nus->println("BUTTON_4 pressed - will release in 5s.");
            }
            else
            {
                nus->println("BUTTON_4 is already held.");
            }
        }
        else
        {
            nus->println("Echo: " + received);
        }
    }

    // --- Periodic status line over NUS, to confirm the TX path works ---
    if (millis() - lastStatusTime >= STATUS_INTERVAL_MS)
    {
        lastStatusTime = millis();

        String status = "uptime_ms=" + String(millis()) +
                         " gamepad_connected=" + (bleGamepad.isConnected() ? "yes" : "no") +
                         " host_mac=" + String(bleGamepad.getAddress().toString().c_str()) +
                         " button3=" + (buttonHeld ? "PRESSED" : "released") +
                         " button4=" + (button4Held ? "PRESSED" : "released") +
                         " battery=" + String(batteryLevel) +
                         " free_heap=" + String(ESP.getFreeHeap());
        nus->println(status);
        Serial.println(status);
    }
}
