/*
   Sets BLE characteristic options
   Use BLE Scanner etc on Android to see them

   Also shows how to set transmit power during initial configuration,
   or at any stage whilst running by using bleGamepad.setTXPowerLevel(int8_t)

   The only valid values are: -12, -9, -6, -3, 0, 3, 6 and 9
   Values correlate to dbm

   You can get the currently set TX power level by calling bleGamepad.setTXPowerLevel()

*/

#include <Arduino.h>
#include <BleGamepad.h>

int8_t txPowerLevel = 3;

BleGamepad bleGamepad("Custom Contoller Name", "lemmingDev", 100); // Set custom device name, manufacturer and initial battery level
BleGamepadConfiguration bleGamepadConfig;                          // Create a BleGamepadConfiguration object to store all of the options

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  bleGamepadConfig.setVid(0xe502);
  bleGamepadConfig.setPid(0xabcd);
  bleGamepadConfig.setTXPowerLevel(txPowerLevel);  // Defaults to 9 if not set. (Range: -12 to 9 dBm)

  bleGamepadConfig.setModelNumber("1.0");
  bleGamepadConfig.setSoftwareRevision("Software Rev 1");
  bleGamepadConfig.setSerialNumber("9876543210");
  bleGamepadConfig.setFirmwareRevision("2.0");
  bleGamepadConfig.setHardwareRevision("1.7");

  // Some non-Windows operating systems and web based gamepad testers don't like min axis set below 0, so 0 is set by default
  //bleGamepadConfig.setAxesMin(0x8001); // -32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepadConfig.setAxesMin(0x0000); // 0 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepadConfig.setAxesMax(0x7FFF); // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal

  bleGamepad.begin(&bleGamepadConfig); // Begin gamepad with configuration options
  
  // Change power level to 6
  bleGamepad.setTXPowerLevel(6);    // The default of 9 (strongest transmit power level) will be used if not set

}

void loop()
{
  if (bleGamepad.isConnected())
  {

  }
}
