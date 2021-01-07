/*
 * This example turns the ESP32 into a Bluetooth LE gamepad that presses buttons and moves axis
 * 
 * Possible buttons are:
 * BUTTON_1 through to BUTTON_32 
 * 
 * Possible DPAD/HAT switch position values are: 
 * DPAD_CENTERED, DPAD_UP, DPAD_UP_RIGHT, DPAD_RIGHT, DPAD_DOWN_RIGHT, 
 * DPAD_DOWN, DPAD_DOWN_LEFT, DPAD_LEFT, DPAD_UP_LEFT
 * 
 * bleGamepad.setAxes takes the following int16_t parameters for the Left/Right Thumb X/Y, char for the Left/Right Triggers, and hat switch position as above: 
 * (Left Thumb X, Left Thumb Y, Right Thumb X, Right Thumb Y, Left Trigger, Right Trigger, Hat switch position);
 *
 * bleGamepad.setLeftThumb takes 2 int16_t parameters for x and y axes
 * 
 * bleGamepad.setRightThumb takes 2 int16_t parameters for z and rZ axes
 * 
 * bleGamepad.setLeftTrigger takes 1 char parameter for rX axis
 * 
 * bleGamepad.setRightTrigger takes 1 char parameter for rY axis
 * 
 * bleGamepad.setHat takes a hat position as above (or 0 = centered and 1~8 are the 8 possible directions)
 * 
 * The example shows that you can set axes/hat independantly, or together.
 * 
 * It also shows that you can disable the autoReport feature (enabled by default), and manually call the sendReport() function when wanted 
 * 
 */
 
#include <BleGamepad.h> 

BleGamepad bleGamepad;

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleGamepad.begin();
  bleGamepad.setAutoReport(false);   //This is true by default. You can also set this in the line above --> bleGamepad.begin(false);
}

void loop() 
{
  if(bleGamepad.isConnected()) 
  {
    Serial.println("Press buttons 1 and 32. Set DPAD to down right.");

    //Press buttons 1 and 32
    bleGamepad.press(BUTTON_1);
    bleGamepad.press(BUTTON_32);

    //Move all axes to max. 
    bleGamepad.setLeftThumb(32767, 32767);
    bleGamepad.setRightThumb(32767, 32767);
    bleGamepad.setLeftTrigger(255);
    bleGamepad.setRightTrigger(255);

    //Set DPAD to down right
    bleGamepad.setHat(DPAD_DOWN_RIGHT);
    
    //Send the gamepad report
    bleGamepad.sendReport();
    delay(500);

    Serial.println("Release button 1. Move all axes to min. Set DPAD to centred.");
    bleGamepad.release(BUTTON_1);
    bleGamepad.setAxes(-32767, -32767, -32767, -32767, 0, 0, DPAD_CENTERED);
    bleGamepad.sendReport();
    delay(500);
  }
}
