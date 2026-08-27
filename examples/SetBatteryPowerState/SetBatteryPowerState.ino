/*
   This example builds on the SetBatteryLevel example by also showing how to set the battery power state information to be reported to the host OS
   Use nRF Connect on Android for example to see the set information

   You can set:

   - Whether there is a battery installed or not
   - Whether the device is discharging or not
   - Whether the device is charging or not
   - Whether the power level is good or critically low

   Each of the above also supports unknown or not supported states

   You can use the definitions as shown below, or direct values

   #define POWER_STATE_UNKNOWN         0 // 0b00
   #define POWER_STATE_NOT_SUPPORTED   1 // 0b01
   #define POWER_STATE_NOT_PRESENT     2 // 0b10
   #define POWER_STATE_NOT_DISCHARGING 2 // 0b10
   #define POWER_STATE_NOT_CHARGING    2 // 0b10
   #define POWER_STATE_GOOD            2 // 0b10
   #define POWER_STATE_PRESENT         3 // 0b11
   #define POWER_STATE_DISCHARGING     3 // 0b11
   #define POWER_STATE_CHARGING        3 // 0b11
   #define POWER_STATE_CRITICAL        3 // 0b11

*/

#include <Arduino.h>
#include <BleGamepad.h>

BleGamepad bleGamepad;

int batteryLevel = 100;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleGamepad.begin();
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // bleGamepad.setPowerStateAll(POWER_STATE_PRESENT, POWER_STATE_NOT_DISCHARGING, POWER_STATE_CHARGING, POWER_STATE_GOOD); // Can set all values together or separate as below
    bleGamepad.setBatteryPowerInformation(POWER_STATE_PRESENT);   // POWER_STATE_UNKNOWN or POWER_STATE_NOT_SUPPORTED or POWER_STATE_NOT_PRESENT or POWER_STATE_PRESENT
    bleGamepad.setDischargingState(POWER_STATE_NOT_DISCHARGING);  // POWER_STATE_UNKNOWN or POWER_STATE_NOT_SUPPORTED or POWER_STATE_NOT_DISCHARGING or POWER_STATE_DISCHARGING
    bleGamepad.setChargingState(POWER_STATE_CHARGING);            // POWER_STATE_UNKNOWN or POWER_STATE_NOT_SUPPORTED or POWER_STATE_NOT_CHARGING or POWER_STATE_CHARGING
    bleGamepad.setPowerLevel(POWER_STATE_GOOD);                   // POWER_STATE_UNKNOWN or POWER_STATE_NOT_SUPPORTED or POWER_STATE_GOOD or POWER_STATE_CRITICAL

    if (batteryLevel > 0)
    {
      batteryLevel -= 1;
    }

    Serial.print("Battery Level Set To: ");
    Serial.println(batteryLevel);
    bleGamepad.setBatteryLevel(batteryLevel);

    delay(10000);
  }
}
