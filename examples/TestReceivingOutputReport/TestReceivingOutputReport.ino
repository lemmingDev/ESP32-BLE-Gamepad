/*
 * Test receiving Output Report
 * Here is a C# testing program for sending OutputReport from the host to the device
 * https://github.com/Sab1e-GitHub/HIDOutputDemo_CSharp
 */

#include <BleGamepad.h>

#define numOfButtons 16

BleGamepad bleGamepad;
BleGamepadConfiguration bleGamepadConfig;

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS

  bleGamepadConfig.setEnableOutputReport(true);   // (Necessary) Enable Output Report. Default is false. 
  bleGamepadConfig.setOutputReportLength(128);  // (Optional) Set Report Length 128(Bytes). The default value is 64 bytes.
  // Do not set the OutputReportLength too large, otherwise it will be truncated. For example, if the hexadecimal value of 10000 in decimal is 0x2710, it will be truncated to 0x710.

  bleGamepadConfig.setHidReportId(0x05);  // (Optional) Set ReportID to 0x05. 
  // When you send data from the upper computer to ESP32, you must send the ReportID in the first byte! The default ReportID is 3.

  bleGamepadConfig.setButtonCount(numOfButtons);

  bleGamepadConfig.setAxesMin(0x0000); // 0 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepadConfig.setAxesMax(0x7FFF); // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal 

  // Try NOT to modify VID, otherwise it may cause the host to be unable to send output reports to the device.
  bleGamepadConfig.setVid(0x1234);
  
  // You can freely set the PID
  bleGamepadConfig.setPid(0x0001);
  bleGamepad.begin(&bleGamepadConfig);

  // Changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop() 
{
  if (bleGamepad.isConnected()) 
  {
    if (bleGamepad.isOutputReceived()) 
    {
      uint8_t* buffer = bleGamepad.getOutputBuffer();
      Serial.print("Receive: ");
      
      for (int i = 0; i < 64; i++) 
      {
        Serial.printf("0x%X ",buffer[i]); // Print data from buffer
      }
      
      Serial.println("");
    }
  }
}
