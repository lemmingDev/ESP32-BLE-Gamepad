/*
 * Test HID Feature Reports (bidirectional: host can read from and write to the device)
 * See FeatureReport-example.py for a host-side test script using bleak.
 */

#include <BleGamepad.h>

#define numOfButtons 16

BleGamepad bleGamepad;
BleGamepadConfiguration bleGamepadConfig;

uint8_t featureReport[3] = { 0x10, 0x20, 0x30 };

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS

  bleGamepadConfig.setEnableFeatureReport(true);  // (Necessary) Enable Feature Report. Default is false.
  bleGamepadConfig.setFeatureReportLength(sizeof(featureReport));  // (Optional) Set Report Length. The default value is 64 bytes.

  bleGamepadConfig.setButtonCount(numOfButtons);

  bleGamepadConfig.setAxesMin(0x0000); // 0 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
  bleGamepadConfig.setAxesMax(0x7FFF); // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal

  bleGamepad.begin(&bleGamepadConfig);

  // Changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // begin() sets up the feature report characteristic on a background task, so
    // wait until we're connected (which implies that task has finished) before
    // seeding it with our initial data.
    static bool seeded = false;
    if (!seeded)
    {
      bleGamepad.setFeatureBuffer(featureReport, sizeof(featureReport));
      seeded = true;
    }

    // Poll for data the host has written into the feature report
    if (bleGamepad.isFeatureReceived())
    {
      uint8_t* buffer = bleGamepad.getFeatureBuffer();

      for (size_t i = 0; i < sizeof(featureReport); i++)
      {
        featureReport[i] = buffer[i];
      }

      Serial.print("Feature Report Updated: ");
      for (size_t i = 0; i < sizeof(featureReport); i++)
      {
        Serial.printf("0x%02X ", featureReport[i]);
      }
      Serial.println();
    }
  }
}
