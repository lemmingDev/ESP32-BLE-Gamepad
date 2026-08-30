/*
 * SInput Battery Example
 *
 * Demonstrates battery level and power state reporting over SInput.
 * The battery charge level slides back and forth between 25% and 90%
 * (a fake ramp, since there's no real fuel gauge) so a host's battery
 * indicator has something visibly changing to test against.
 *
 * Power state is also reported: POWER_STATE_PRESENT means the device
 * has a battery, and POWER_STATE_DISCHARGING means it's running on
 * battery power (not plugged in).
 *
 * Watch battery level with:
 *   Linux:   upower -i /org/freedesktop/UPower/devices/gaming_input_dev_XX_XX_XX_XX_XX_XX
 *   SDL3:    SDL_GetGamepadPowerInfo(gamepad)
 */

#include <BleGamepad.h>

#define BATTERY_RAMP_MIN       25
#define BATTERY_RAMP_MAX       90
#define BATTERY_RAMP_INTERVAL  300 // ms per 1% step -- full sweep takes ~40s

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting SInput Battery...");

  // Configure as SInput device
  config.setAutoReport(false);
  config.setGamepadMode(GamepadMode::SInput);

  bleGamepad.begin(&config);
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // Seed power state once after connection is established
    static bool powerStateSeeded = false;
    if (!powerStateSeeded)
    {
      bleGamepad.setBatteryPowerInformation(POWER_STATE_PRESENT);    // Device has a battery
      bleGamepad.setDischargingState(POWER_STATE_DISCHARGING);       // Running on battery
      powerStateSeeded = true;
    }

    // Ramp battery level back and forth between 25% and 90%
    static uint8_t batteryLevel = BATTERY_RAMP_MIN;
    static int8_t batteryDirection = 1;
    static unsigned long lastRampTime = 0;

    if (millis() - lastRampTime >= BATTERY_RAMP_INTERVAL)
    {
      lastRampTime = millis();

      batteryLevel += batteryDirection;

      if (batteryLevel >= BATTERY_RAMP_MAX)
      {
        batteryLevel = BATTERY_RAMP_MAX;
        batteryDirection = -1; // Start going down
      }
      else if (batteryLevel <= BATTERY_RAMP_MIN)
      {
        batteryLevel = BATTERY_RAMP_MIN;
        batteryDirection = 1; // Start going up
      }

      // setBatteryLevel() also updates the standard BLE Battery Service (0x180F),
      // which shows up in Linux's upower, macOS system report, etc.
      bleGamepad.setBatteryLevel(batteryLevel);

      Serial.print("Battery: ");
      Serial.print(batteryLevel);
      Serial.println("%");
    }

    // Toggle BUTTON_1 every 1 second as a heartbeat,
    // so the host knows the device is alive and sending input reports
    static bool buttonPressed = false;
    static unsigned long lastToggleTime = 0;

    if (millis() - lastToggleTime >= 1000)
    {
      lastToggleTime = millis();

      if (buttonPressed)
      {
        bleGamepad.release(BUTTON_1);
        buttonPressed = false;
      }
      else
      {
        bleGamepad.press(BUTTON_1);
        buttonPressed = true;
      }

      bleGamepad.sendReport();
    }
  }
}
