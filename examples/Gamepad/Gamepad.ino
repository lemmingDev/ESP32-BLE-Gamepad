/*
 * This example turns the ESP32 into a Bluetooth LE gamepad that presses buttons and moves axis
 * 
 * Possible buttons are:
 * BUTTON_1 through to BUTTON_16 
 * (16 buttons supported by default. Library can be configured to support up to 128)
 * 
 * Possible DPAD/HAT switch position values are: 
 * DPAD_CENTERED, DPAD_UP, DPAD_UP_RIGHT, DPAD_RIGHT, DPAD_DOWN_RIGHT, DPAD_DOWN, DPAD_DOWN_LEFT, DPAD_LEFT, DPAD_UP_LEFT
 * (or HAT_CENTERED, HAT_UP etc)
 *
 * bleGamepad.setAxes takes the following int16_t parameters for the Left/Right Thumb X/Y, Left/Right Triggers plus slider1 and slider2, and hat switch position as above: 
 * (Left Thumb X, Left Thumb Y, Right Thumb X, Right Thumb Y, Left Trigger, Right Trigger, Hat switch position 
 ^ (1 hat switch (dpad) supported by default. Library can be configured to support up to 4)
 *
 * Library can also be configured to support up to 5 simulation controls (can be set with setSimulationControls)
 * (rudder, throttle, accelerator, brake, steering), but they are not enabled by default.
 */
 
#include <BleGamepad.h> 

BleGamepad bleGamepad;

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleGamepad.begin();
  // The default bleGamepad.begin() above is the same as bleGamepad.begin(16, 1, true, true, true, true, true, true, true, true, false, false, false, false, false);
  // which enables a gamepad with 16 buttons, 1 hat switch, enabled x, y, z, rZ, rX, rY, slider 1, slider 2 and disabled rudder, throttle, accelerator, brake, steering
  // Auto reporting is enabled by default. 
  // Use bleGamepad.setAutoReport(false); to disable auto reporting, and then use bleGamepad.sendReport(); as needed
}

void loop() 
{
  if(bleGamepad.isConnected()) 
  {
    Serial.println("Press buttons 5 and 16. Move all enabled axes to max. Set DPAD (hat 1) to down right.");
    bleGamepad.press(BUTTON_5);
    bleGamepad.press(BUTTON_16);
    bleGamepad.setAxes(32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, DPAD_DOWN_RIGHT);
    // All axes, sliders, hats etc can also be set independently. See the IndividualAxes.ino example
    delay(500);

    Serial.println("Release button 5. Move all axes to min. Set DPAD (hat 1) to centred.");
    bleGamepad.release(BUTTON_5);
    bleGamepad.setAxes(-32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, DPAD_CENTERED);
    delay(500);
  }
}