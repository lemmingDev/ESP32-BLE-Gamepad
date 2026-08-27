/*
 * Self-contained BLE gamepad test firmware.
 *
 * Needs no wiring and no extra libraries. It brings up the full BLE profile the
 * library offers so a host can be checked against a known-good build:
 *   - the gamepad HID service, plus one (never-touched) hat switch so Android
 *     still registers it as a gamepad;
 *   - every Device Information Service characteristic, filled with an
 *     identifiable value;
 *   - the Nordic UART Service (NUS): on connect it greets, then pushes a status
 *     line every few seconds, and answers "help" / "status" (anything else is
 *     echoed back).
 *
 * Once a host connects it, on its own:
 *   - presses one button every ~250 ms, cycling BUTTON_3..BUTTON_8. BUTTON_1 and
 *     BUTTON_2 are deliberately skipped: they map to A / B (accept / back) on
 *     Android, so auto-pressing them would fire UI navigation on the host.
 *   - walks the battery level down 100 -> 0 (wrapping back to 100) every ~5 s,
 *     switching the reported power state between "unplugged / discharging" and
 *     "plugged in / charging" as it goes.
 *
 * The point is a known-good device you can watch in any gamepad tester, in
 * nRF Connect, or over a BLE UART terminal without a local toolchain: flash it,
 * connect, and confirm the library itself works before chasing a build- or
 * wiring-specific problem.
 *
 * .github/workflows/platformio.yml builds this for esp32dev / esp32s3 /
 * esp32c3 and uploads the flashable binaries as run artifacts (and attaches
 * them to GitHub releases). See test/ci_build/README.md. Issue #342.
 */

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

// Set by inject_version.py at build time, e.g. "ESP32-BLE-Gamepad 0.7.5-rc0+g05599be".
#ifndef BLE_GAMEPAD_LIB_VERSION
#define BLE_GAMEPAD_LIB_VERSION "ESP32-BLE-Gamepad (unknown build)"
#endif

#define BUTTON_COUNT 8
#define FIRST_TEST_BUTTON 3 // skip BUTTON_1 / BUTTON_2 (A / B = accept / back on Android)
#define LAST_TEST_BUTTON 8
#define BUTTON_INTERVAL_MS 250
#define BATTERY_INTERVAL_MS 5000
#define NUS_STATUS_INTERVAL_MS 3000

BleGamepad bleGamepad("ESP32 BLE Gamepad Test", "lemmingDev ESP32-BLE-Gamepad", 100);
BleGamepadConfiguration bleGamepadConfig;

static uint8_t currentButton = FIRST_TEST_BUTTON;
static int batteryLevel = 100;
static bool wasConnected = false;
static unsigned long lastButtonStep = 0;
static unsigned long lastBatteryStep = 0;
static unsigned long lastNusStatus = 0;

String statusLine()
{
    return String("uptime_ms=") + millis() +
           " connected=" + (bleGamepad.isConnected() ? "yes" : "no") +
           " button=" + currentButton +
           " battery=" + batteryLevel +
           " free_heap=" + ESP.getFreeHeap();
}

// A central can subscribe to NUS without bonding, so this is a separate signal
// from the gamepad HID link (bleGamepad.isConnected()).
void onNusSubscribeChanged(bool subscribed, const std::string &address)
{
    Serial.printf("NUS %s %s\n", address.c_str(), subscribed ? "subscribed" : "unsubscribed");

    if (subscribed)
    {
        BleNUS *nus = bleGamepad.getNUS();
        if (nus)
        {
            nus->println("ESP32-BLE-Gamepad test firmware. Commands: help, status.");
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("ESP32-BLE-Gamepad test firmware starting");

    bleGamepadConfig.setAutoReport(false); // report is sent explicitly below
    bleGamepadConfig.setButtonCount(BUTTON_COUNT);
    // Android won't register the device as a gamepad with zero hats; keep one
    // (never touched), and drop the axes so the HID descriptor stays small
    // enough that Android's parser reads the button offsets correctly.
    bleGamepadConfig.setHatSwitchCount(1);
    bleGamepadConfig.setWhichAxes(false, false, false, false, false, false, false, false);
    bleGamepadConfig.setVid(0xe502);
    bleGamepadConfig.setPid(0xabcd);

    // Fill every Device Information Service characteristic with an identifiable
    // value so a GATT browser can confirm each one is present and readable.
    bleGamepadConfig.setModelNumber("1.0");
    bleGamepadConfig.setSoftwareRevision(BLE_GAMEPAD_LIB_VERSION); // library version + git sha
    bleGamepadConfig.setSerialNumber("9876543210");
    bleGamepadConfig.setFirmwareRevision("2.0");
    bleGamepadConfig.setHardwareRevision("1.7");

    bleGamepad.begin(&bleGamepadConfig);
    bleGamepad.beginNUS(); // Nordic UART Service alongside the gamepad HID service
    bleGamepad.getNUS()->setSubscribeCallback(onNusSubscribeChanged);
}

// Press then release the next button, cycling FIRST_TEST_BUTTON..LAST_TEST_BUTTON.
void stepButtons()
{
    bleGamepad.press(currentButton);
    bleGamepad.sendReport();
    delay(20);
    bleGamepad.release(currentButton);
    bleGamepad.sendReport();

    Serial.printf("Pressed button %u (cycling %u..%u)\n",
                  currentButton, FIRST_TEST_BUTTON, LAST_TEST_BUTTON);
    currentButton = (currentButton >= LAST_TEST_BUTTON) ? FIRST_TEST_BUTTON : currentButton + 1;
}

// Drop the battery level by 10% (wrapping 0 -> 100) and report a power state
// that matches: discharging above 30%, charging at/below it, critical at/below
// 10%.
void stepBattery()
{
    batteryLevel -= 10;
    if (batteryLevel < 0)
    {
        batteryLevel = 100;
    }

    if (batteryLevel <= 30)
    {
        bleGamepad.setPowerStateAll(POWER_STATE_PRESENT,
                                    POWER_STATE_NOT_DISCHARGING,
                                    POWER_STATE_CHARGING,
                                    batteryLevel <= 10 ? POWER_STATE_CRITICAL : POWER_STATE_GOOD);
        Serial.printf("Battery %d%% - plugged in, charging\n", batteryLevel);
    }
    else
    {
        bleGamepad.setPowerStateAll(POWER_STATE_PRESENT,
                                    POWER_STATE_DISCHARGING,
                                    POWER_STATE_NOT_CHARGING,
                                    POWER_STATE_GOOD);
        Serial.printf("Battery %d%% - on battery, discharging\n", batteryLevel);
    }

    bleGamepad.setBatteryLevel(batteryLevel);
}

// NUS input/output is not gated on the gamepad HID link - a terminal app can be
// subscribed while bleGamepad.isConnected() is still false.
void serviceNus()
{
    BleNUS *nus = bleGamepad.getNUS();
    if (!nus)
    {
        return;
    }

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
            nus->println("Commands: help, status. Anything else is echoed back.");
        }
        else if (command == "status")
        {
            nus->println(statusLine());
        }
        else
        {
            nus->println("Echo: " + received);
        }
    }

    if (millis() - lastNusStatus >= NUS_STATUS_INTERVAL_MS)
    {
        lastNusStatus = millis();
        String line = statusLine();
        nus->println(line);
        Serial.println(line);
    }
}

void loop()
{
    bool connected = bleGamepad.isConnected();

    if (connected != wasConnected)
    {
        Serial.println(connected ? "Host connected" : "Host disconnected");
        wasConnected = connected;
    }

    if (connected)
    {
        unsigned long now = millis();

        if (now - lastButtonStep >= BUTTON_INTERVAL_MS)
        {
            lastButtonStep = now;
            stepButtons();
        }

        if (now - lastBatteryStep >= BATTERY_INTERVAL_MS)
        {
            lastBatteryStep = now;
            stepBattery();
        }
    }

    serviceNus();
    delay(10);
}
