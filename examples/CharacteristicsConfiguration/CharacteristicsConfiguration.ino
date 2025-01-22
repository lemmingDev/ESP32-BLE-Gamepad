/*
 * Sets BLE characteristic options
 * Also shows how to set a custom MAC address
 * Use BLE Scanner etc on Android to see them
 */

#include <Arduino.h>
#include <BleGamepad.h>

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
}

void loop()
{
    if (bleGamepad.isConnected())
    {

    }
}
