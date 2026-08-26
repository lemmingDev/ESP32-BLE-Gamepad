/*
 * A basic SInput (https://github.com/HandHeldLegend/SInput-HID) device, using the
 * onboard LED most ESP32 dev boards expose on LED_BUILTIN as a Player 1 indicator
 * -- lit only while a real SInput host (an SDL3 app with
 * SDL_HINT_JOYSTICK_HIDAPI_SINPUT enabled) has assigned this controller Player LED
 * index 1 via SDL_SetGamepadPlayerIndex(), off for every other player or none, the
 * same way a real controller's single-LED player indicator behaves.
 *
 * setEnableSInput(true) replaces this library's usual configurable HID report with
 * SInput's fixed one -- see GattVsHid.md for why SInput needs its own report
 * layout, and why it can't be combined with setEnableOutputReport()/
 * setEnableFeatureReport(). It also switches the advertised VID/PID to
 * SINPUT_USB_VID/SINPUT_USB_PID_GENERIC, since SDL only recognizes SInput devices
 * with that exact pair (see BleGamepadConfiguration.h).
 *
 * Buttons 1-6 and hat 1 (the D-pad) are wired into SInput's fixed button layout
 * automatically -- press/release/setHat1 etc. all work as usual. This example
 * presses BUTTON_1 on a timer just so there's some visible Input Report traffic
 * (watch it in jstest/evtest, same as TestFeatureReports.ino).
 *
 * This library only implements the Player LED command today -- see
 * BleSInputReceiver::onWrite() in BleSInput.cpp -- so rumble (haptics) and RGB
 * commands are accepted but otherwise ignored.
 *
 * Battery is also wired up: the charge level slides back and forth between 25%
 * and 90% (a fake "ramp", since there's no real fuel gauge here) so a host's
 * battery indicator has something visibly changing to test against -- watch it
 * with SDL_GetGamepadPowerInfo() (see host_test/SDL3Testing.md for a
 * ready-to-run test program covering this, Player LED, and buttons/axes
 * together).
 */

#include <BleGamepad.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Fall back to GPIO2 (common onboard LED pin) if the board package doesn't define one
#endif

#define BUTTON_TOGGLE_INTERVAL_MS 1000

#define BATTERY_RAMP_STEP_INTERVAL_MS 300 // 1%/step -- a full 25 -> 90 -> 25 sweep takes ~40s
#define BATTERY_RAMP_MIN 25
#define BATTERY_RAMP_MAX 90

BleGamepad bleGamepad;
BleGamepadConfiguration bleGamepadConfig;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  bleGamepadConfig.setAutoReport(false); // We'll call sendReport() ourselves on the button timer below
  bleGamepadConfig.setEnableSInput(true); // (Necessary) Also sets VID/PID to SInput's required values. Default is false.

  bleGamepad.begin(&bleGamepadConfig);

  // Changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // begin() sets up power state on a background task, so wait until we're
    // connected (which implies that task has finished) before seeding it --
    // same reasoning as TestFeatureReports.ino's "seeded" flag.
    static bool powerStateSeeded = false;
    if (!powerStateSeeded)
    {
      bleGamepad.setBatteryPowerInformation(POWER_STATE_PRESENT);
      bleGamepad.setDischargingState(POWER_STATE_DISCHARGING); // "on battery", not wired/charging
      powerStateSeeded = true;
    }

    // Poll for a Player LED command from the host
    if (bleGamepad.isPlayerLedReceived())
    {
      uint8_t playerLedIndex = bleGamepad.getPlayerLedIndex();

      // SInput's Player LED index is 1-based (0 means "no player assigned"),
      // so 1 is Player 1. A real device with 4 LEDs would light LED N-1 for
      // whichever index arrives here; with a single onboard LED, this wires
      // it specifically to Player 1 -- on only when this controller has been
      // assigned that slot, off for every other player (or none).
      digitalWrite(LED_BUILTIN, playerLedIndex == 1 ? HIGH : LOW);

      Serial.print("Player LED index: ");
      Serial.println(playerLedIndex);
    }

    // Slide the battery charge level back and forth between 25% and 90% --
    // sendReport() (on the button timer below) picks up whatever this is set
    // to each time it runs, so this doesn't need its own sendReport() call.
    static uint8_t batteryLevel = BATTERY_RAMP_MIN;
    static int8_t batteryRampDirection = 1;
    static unsigned long lastBatteryRampTime = 0;
    if (millis() - lastBatteryRampTime >= BATTERY_RAMP_STEP_INTERVAL_MS)
    {
      lastBatteryRampTime = millis();
      batteryLevel += batteryRampDirection;
      if (batteryLevel >= BATTERY_RAMP_MAX)
      {
        batteryLevel = BATTERY_RAMP_MAX;
        batteryRampDirection = -1;
      }
      else if (batteryLevel <= BATTERY_RAMP_MIN)
      {
        batteryLevel = BATTERY_RAMP_MIN;
        batteryRampDirection = 1;
      }
      bleGamepad.setBatteryLevel(batteryLevel); // Also updates the standard Battery Service (0x180F), independent of SInput
    }

    // Toggle BUTTON_1 on a repeating timer, just to demonstrate the Input Report
    static bool button1Pressed = false;
    static unsigned long lastButtonToggleTime = 0;
    if (millis() - lastButtonToggleTime >= BUTTON_TOGGLE_INTERVAL_MS)
    {
      lastButtonToggleTime = millis();
      button1Pressed = !button1Pressed;
      button1Pressed ? bleGamepad.press(BUTTON_1) : bleGamepad.release(BUTTON_1);
      bleGamepad.sendReport();

      Serial.print("Battery: ");
      Serial.print(batteryLevel);
      Serial.println("%");
    }
  }
}
