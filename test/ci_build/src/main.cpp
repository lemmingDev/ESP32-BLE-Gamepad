/*
 * Self-contained BLE gamepad test firmware.
 *
 * Needs no wiring and no extra libraries. It brings up the full BLE profile the
 * library offers so a host can be checked against a known-good build:
 *   - the gamepad HID service (BUTTON_COUNT buttons, the right thumbstick, and
 *     one never-touched hat switch so Android still registers it as a gamepad);
 *   - every Device Information Service characteristic, filled with an
 *     identifiable value;
 *   - the Nordic UART Service (NUS): on connect it greets, then pushes a status
 *     line every few seconds, and answers "help" / "status" (anything else is
 *     echoed back).
 *
 * Once a host connects it, on its own:
 *   - presses one button every ~2.5 s, holding each for ~350 ms, cycling
 *     FIRST_TEST_BUTTON..LAST_TEST_BUTTON. See the note on that #define for how
 *     the range interacts with Android's button mapping.
 *   - sweeps the right thumbstick (Z / RZ axes) around a small circle. The right
 *     stick is used rather than the left, and the deflection kept under ~50%, so
 *     it doesn't drive Android's on-screen focus.
 *   - ramps the reported battery level down 100 -> 10 and back up again on a
 *     ~5 s tick, reporting "discharging" while it falls and "charging" while it
 *     rises (critical at/below 20%).
 *
 * The point is a known-good device you can watch in any gamepad tester, in
 * nRF Connect, or over a BLE UART terminal without a local toolchain: flash it,
 * connect, and confirm the library itself works before chasing a build- or
 * wiring-specific problem.
 *
 * .github/workflows/platformio.yml builds this for esp32dev / esp32s3 /
 * esp32c3 and uploads the flashable binaries as run artifacts (and attaches
 * them to GitHub releases). See test/ci_build/README.md.
 */

#include <Arduino.h>
#include <math.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

// Set by inject_version.py at build time, e.g. "ESP32-BLE-Gamepad 0.7.5-rc0+g05599be"
// and "2026-08-27T14:32:10Z". BLE_GAMEPAD_BUILD_TIME changes every build, so the
// line printed on boot confirms a fresh upload actually landed.
#ifndef BLE_GAMEPAD_LIB_VERSION
#define BLE_GAMEPAD_LIB_VERSION "ESP32-BLE-Gamepad (unknown build)"
#endif
#ifndef BLE_GAMEPAD_BUILD_TIME
#define BLE_GAMEPAD_BUILD_TIME __DATE__ " " __TIME__
#endif

// Which buttons to auto-press, and how many to declare. The range matters on a
// host: the Linux/Android HID layer maps gamepad HID buttons 1..16 onto the
// named BTN_GAMEPAD codes (A/B/X/Y, shoulders, triggers, start/select, stick
// clicks) that Android TV navigates its UI with; buttons 17+ fall through to
// BTN_TRIGGER_HAPPY -> KEYCODE_BUTTON_1.., which testers still show but the UI
// ignores. Move FIRST/LAST to suit the host you're testing against.
#define BUTTON_COUNT 12
#define FIRST_TEST_BUTTON 8
#define LAST_TEST_BUTTON 11
#define BUTTON_INTERVAL_MS 2500
#define BUTTON_HOLD_MS 350 // long enough for a gamepad tester to show it

#define AXIS_INTERVAL_MS 80
#define AXIS_CENTER 16384 // default axis range is 0..32767
// Stay well under ~50% deflection: at/above that Android synthesises D-pad
// presses from a stick, which is itself UI navigation.
#define AXIS_RADIUS 7000
#define AXIS_STEP_RAD 0.20f // ~0.8 s per revolution

#define BATTERY_INTERVAL_MS 5000
#define BATTERY_MIN 10
#define BATTERY_MAX 100
#define BATTERY_STEP 10
#define BATTERY_CRITICAL 20

#define NUS_STATUS_INTERVAL_MS 3000

BleGamepad bleGamepad("ESP32 BLE Gamepad Test", "lemmingDev", 100);
BleGamepadConfiguration bleGamepadConfig;

static uint8_t currentButton = FIRST_TEST_BUTTON;
static float axisAngle = 0.0f;
static int batteryLevel = BATTERY_MAX;
static int batteryStep = -BATTERY_STEP; // start by draining
static bool wasConnected = false;
static bool nusSubscribed = false;
static unsigned long lastButtonStep = 0;
static unsigned long lastAxisStep = 0;
static unsigned long lastBatteryStep = 0;
static unsigned long lastNusStatus = 0;

String statusLine()
{
    return String("uptime_ms=") + millis() +
           " connected=" + (bleGamepad.isConnected() ? "yes" : "no") +
           " button=" + currentButton +
           " battery=" + batteryLevel + (batteryStep < 0 ? " (draining)" : " (charging)") +
           " free_heap=" + ESP.getFreeHeap();
}

// A central can subscribe to NUS without bonding, so this is a separate signal
// from the gamepad HID link (bleGamepad.isConnected()).
void onNusSubscribeChanged(bool subscribed, const std::string &address)
{
    Serial.printf("NUS %s %s\n", address.c_str(), subscribed ? "subscribed" : "unsubscribed");
    nusSubscribed = subscribed;

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
    Serial.printf("\nESP32-BLE-Gamepad test firmware starting\n  build: %s\n  built: %s\n",
                  BLE_GAMEPAD_LIB_VERSION, BLE_GAMEPAD_BUILD_TIME);

    //bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
    //bleGamepadConfig.setControllerType(CONTROLLER_TYPE_JOYSTICK);

    bleGamepadConfig.setAutoReport(false); // reports are sent explicitly below
    bleGamepadConfig.setButtonCount(BUTTON_COUNT);

    // Android won't register the device as a gamepad with zero hats; keep one
    // (never touched). Only the right thumbstick (Z / RZ) is enabled - a small
    // HID descriptor keeps Android's report parser reading button offsets right.
    bleGamepadConfig.setHatSwitchCount(1);
    bleGamepadConfig.setWhichAxes(false, false, true, false, false, true, false, false);
    //bleGamepadConfig.setVid(0xe502);
    //bleGamepadConfig.setPid(0xabcd);

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
    Serial.printf("Pressing button %u (cycling %u..%u)\n",
                  currentButton, FIRST_TEST_BUTTON, LAST_TEST_BUTTON);

    bleGamepad.press(currentButton);
    bleGamepad.sendReport();
    delay(BUTTON_HOLD_MS);
    bleGamepad.release(currentButton);
    bleGamepad.sendReport();

    Serial.printf("Released button %u\n", currentButton);
    currentButton = (currentButton >= LAST_TEST_BUTTON) ? FIRST_TEST_BUTTON : currentButton + 1;
}

// Advance the right thumbstick one step around a circle.
void stepAxes()
{
    axisAngle += AXIS_STEP_RAD;
    if (axisAngle >= TWO_PI)
    {
        axisAngle -= TWO_PI;
    }

    int16_t z = AXIS_CENTER + (int16_t)(AXIS_RADIUS * cosf(axisAngle));
    int16_t rZ = AXIS_CENTER + (int16_t)(AXIS_RADIUS * sinf(axisAngle));
    bleGamepad.setRightThumb(z, rZ);
    bleGamepad.sendReport();
}

// Ramp the reported battery level between BATTERY_MIN and BATTERY_MAX, reversing
// direction at each end, and report a matching power state.
void stepBattery()
{
    batteryLevel += batteryStep;
    if (batteryLevel <= BATTERY_MIN)
    {
        batteryLevel = BATTERY_MIN;
        batteryStep = BATTERY_STEP; // start charging back up
    }
    else if (batteryLevel >= BATTERY_MAX)
    {
        batteryLevel = BATTERY_MAX;
        batteryStep = -BATTERY_STEP; // start draining again
    }

    uint8_t powerLevel = (batteryLevel <= BATTERY_CRITICAL) ? POWER_STATE_CRITICAL : POWER_STATE_GOOD;

    if (batteryStep > 0)
    {
        bleGamepad.setPowerStateAll(POWER_STATE_PRESENT,
                                    POWER_STATE_NOT_DISCHARGING,
                                    POWER_STATE_CHARGING,
                                    powerLevel);
        Serial.printf("Battery %d%% - plugged in, charging\n", batteryLevel);
    }
    else
    {
        bleGamepad.setPowerStateAll(POWER_STATE_PRESENT,
                                    POWER_STATE_DISCHARGING,
                                    POWER_STATE_NOT_CHARGING,
                                    powerLevel);
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
        Serial.println(line);
        if (nusSubscribed) // don't spray TX at nobody
        {
            nus->println(line);
        }
    }
}

void loop()
{
    static unsigned long lastHeartbeat = 0;
    bool connected = bleGamepad.isConnected();
    unsigned long now = millis();

    if (connected != wasConnected)
    {
        wasConnected = connected;
        Serial.printf("Host %s at %lu ms\n", connected ? "connected" : "disconnected", now);
        // Run the first step of each activity immediately on connect.
        lastButtonStep = now - BUTTON_INTERVAL_MS;
        lastAxisStep = now - AXIS_INTERVAL_MS;
        lastBatteryStep = now - BATTERY_INTERVAL_MS;
    }

    // Heartbeat: once a second, print connection state and how long since each
    // timed step last ran, so the sequence is visible on serial even with no
    // host connected.
    if (now - lastHeartbeat >= 1000)
    {
        lastHeartbeat = now;
        Serial.printf("[hb] now=%lu conn=%d dButton=%lu dAxis=%lu dBattery=%lu button=%u\n",
                      now, connected, now - lastButtonStep, now - lastAxisStep,
                      now - lastBatteryStep, currentButton);
    }

    if (connected)
    {
        if (now - lastButtonStep >= BUTTON_INTERVAL_MS)
        {
            lastButtonStep = now;
            stepButtons();
        }

        if (now - lastAxisStep >= AXIS_INTERVAL_MS)
        {
            lastAxisStep = now;
            stepAxes();
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
