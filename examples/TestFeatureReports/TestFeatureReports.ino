/*
 * Test HID Feature Reports (bidirectional: host can read from and write to the device)
 * using the onboard LED most ESP32 dev boards expose on LED_BUILTIN.
 *
 * The Feature Report is a human-readable ASCII string, e.g. "LED=ON t=48213ms",
 * so a change is easy to spot -- in the Serial Monitor, in FeatureReport-example.py,
 * or in a generic BLE inspector -- without decoding raw bytes. The timestamp is
 * this device's millis() at the moment the LED last changed, so a host can tell
 * whether its write actually took effect (or that nothing has changed since its
 * last read). A host reads the report to check the LED's state, or writes "ON"/
 * "OFF" to flip it remotely.
 *
 * BUTTON_1 is also pressed/released on a repeating timer, so the example still
 * demonstrates an Input Report -- and its repeat rate is tied to the LED, so
 * flipping the LED (locally or from a host) is visible on a second, independent
 * channel: watch the button toggle speed up/slow down in jstest/evtest.
 *
 * See FeatureReport-example.py for a host-side test script using bleak.
 */

#include <BleGamepad.h>

#define numOfButtons 16

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Fall back to GPIO2 (common onboard LED pin) if the board package doesn't define one
#endif

// Long enough for "LED=OFF t=4294967295ms" (millis() at its max) plus a null terminator
#define FEATURE_REPORT_LEN 24

#define BUTTON_TOGGLE_INTERVAL_LED_ON_MS 500   // BUTTON_1 toggles twice a second while the LED is on...
#define BUTTON_TOGGLE_INTERVAL_LED_OFF_MS 2000 // ...and once every 2s while it's off

BleGamepad bleGamepad;
BleGamepadConfiguration bleGamepadConfig;

char featureReport[FEATURE_REPORT_LEN];
bool ledOn = false;

void updateFeatureReport()
{
  digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
  memset(featureReport, 0, sizeof(featureReport)); // Clear any leftover bytes from a previous, longer string
  snprintf(featureReport, sizeof(featureReport), "LED=%s t=%lums", ledOn ? "ON" : "OFF", millis());
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  pinMode(LED_BUILTIN, OUTPUT);
  updateFeatureReport();

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
      bleGamepad.setFeatureBuffer((uint8_t*)featureReport, sizeof(featureReport));
      seeded = true;
    }

    // Poll for data the host has written into the feature report
    if (bleGamepad.isFeatureReceived())
    {
      char* buffer = (char*)bleGamepad.getFeatureBuffer();
      ledOn = (buffer[0] == 'O' && buffer[1] == 'N'); // "ON" vs "OFF"

      updateFeatureReport();
      bleGamepad.setFeatureBuffer((uint8_t*)featureReport, sizeof(featureReport)); // Publish the new state (and its timestamp) for the next read

      Serial.print("Feature Report Updated: ");
      Serial.println(featureReport);
    }

    // Toggle BUTTON_1 on a repeating timer, faster while the LED is on -- a
    // second, Input-Report-based way to see the LED's state take effect.
    static bool button1Pressed = false;
    static unsigned long lastButtonToggleTime = 0;
    unsigned long buttonToggleInterval = ledOn ? BUTTON_TOGGLE_INTERVAL_LED_ON_MS : BUTTON_TOGGLE_INTERVAL_LED_OFF_MS;
    if (millis() - lastButtonToggleTime >= buttonToggleInterval)
    {
      lastButtonToggleTime = millis();
      button1Pressed = !button1Pressed;
      button1Pressed ? bleGamepad.press(BUTTON_1) : bleGamepad.release(BUTTON_1);
      bleGamepad.sendReport(); // setAutoReport(false), so presses/releases need an explicit send
    }
  }
}
