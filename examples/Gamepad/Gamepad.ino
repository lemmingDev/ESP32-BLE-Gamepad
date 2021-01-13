/*
 * This example turns the ESP32 into a Bluetooth LE gamepad that presses buttons and moves axis
 * 
 * Possible buttons are:
 * BUTTON_1 through to BUTTON_64 
 * 
 * Possible DPAD/HAT switch position values are: 
 * DPAD_CENTERED, DPAD_UP, DPAD_UP_RIGHT, DPAD_RIGHT, DPAD_DOWN_RIGHT, DPAD_DOWN, DPAD_DOWN_LEFT, DPAD_LEFT, DPAD_UP_LEFT
 * (or HAT_CENTERED, HAT_UP etc)
 *
 * bleGamepad.setAxes takes the following int16_t parameters for the Left/Right Thumb X/Y, uint16_t for the Left/Right Triggers plus slider1 and slider2, and hat switch position as above: 
 * (Left Thumb X, Left Thumb Y, Right Thumb X, Right Thumb Y, Left Trigger, Right Trigger, Hat switch positions (hat1, hat2, hat3, hat4));
 */
 
#include <BleGamepad.h> 

BleGamepad bleGamepad;

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleGamepad.begin();
}

void loop() 
{
  if(bleGamepad.isConnected()) 
  {
    Serial.println("Press buttons 5 and 32. Move all axes to max. Set DPAD (hat 1) to down right.");
    bleGamepad.press(BUTTON_5);
    bleGamepad.press(BUTTON_32);
    bleGamepad.setAxes(32767, 32767, 32767, 32767, 65535, 65535, 65535, 65535, DPAD_DOWN_RIGHT); //(can also optionally set hat2/3/4 after DPAD/hat1 as seen below)
    // All axes, sliders, hats can also be set independently. See the IndividualAxes.ino example
    delay(500);

    Serial.println("Release button 5. Move all axes to min. Set DPAD (hat 1) to centred.");
    bleGamepad.release(BUTTON_5);
    bleGamepad.setAxes(-32767, -32767, -32767, -32767, 0, 0, 0, 0, DPAD_CENTERED, HAT_CENTERED, HAT_CENTERED, HAT_CENTERED);
    delay(500);
  }
}