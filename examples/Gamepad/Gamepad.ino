/*
 * This example turns the ESP32 into a Bluetooth LE gamepad that presses buttons and moves axis
 * 
 * Possible buttons are:
 * BUTTON_1 through to BUTTON_14* 
 * 
 * Possible DPAD/HAT switch position values are: 
 * DPAD_CENTERED, DPAD_UP, DPAD_UP_RIGHT, DPAD_RIGHT, DPAD_DOWN_RIGHT, 
 * DPAD_DOWN, DPAD_DOWN_LEFT, DPAD_LEFT, DPAD_UP_LEFT
 * 
 * bleGamepad.setAxes takes the following signed char parameters: 
 * (Left Thumb X, Left Thumb Y, Right Thumb X, Right Thumb Y, Left Trigger, Right Trigger, Hat switch position);
 */
 
#include <BleGamepad.h> 

BleGamepad bleGamepad;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleGamepad.begin();
}

void loop() {
  if(bleGamepad.isConnected()) {
    Serial.println("Press buttons 1 and 14. Move all axes to max. Set DPAD to down right.");
    bleGamepad.press(BUTTON_14);
    bleGamepad.press(BUTTON_1);
    bleGamepad.setAxes(127, 127, 127, 127, 127, 127, DPAD_DOWN_RIGHT);
    delay(500);

    Serial.println("Release button 14. Move all axes to min. Set DPAD to centred.");
    bleGamepad.release(BUTTON_14);
    bleGamepad.setAxes(-127, -127, -127, -127, -127, -127, DPAD_CENTERED);
    delay(500);
  }
}