/*
 * This example turns the ESP32 into a Bluetooth LE gamepad that presses buttons and moves axis
 * 
 * Possible buttons are:
 * BUTTON_1 through to BUTTON_64 (Windows gamepad tester only visualised the first 32)
 * 
 * Possible DPAD/HAT switch position values are: 
 * DPAD_CENTERED, DPAD_UP, DPAD_UP_RIGHT, DPAD_RIGHT, DPAD_DOWN_RIGHT, DPAD_DOWN, DPAD_DOWN_LEFT, DPAD_LEFT, DPAD_UP_LEFT
 * 
 * bleGamepad.setAxes takes the following int16_t parameters for the Left/Right Thumb X/Y, uint16_t for the Left/Right Triggers plus slider1 and slider2, and hat1/hat2/hat3/hat4 switch positions as above: 
 * (Left Thumb X, Left Thumb Y, Right Thumb X, Right Thumb Y, Left Trigger, Right Trigger, Slider 1, Slider 2, Hat1 switch position, Hat2 switch position, Hat3 switch position, Hat4 switch position);
 *
 * bleGamepad.setLeftThumb (or setRightThumb) takes 2 int16_t parameters for x and y axes (or z and rZ axes)
 * 
 * bleGamepad.setLeftTrigger (or setRightTrigger) takes 1 uint16_t parameter for rX axis (or rY axis)
 *
 * bleGamepad.setSlider1 (or setSlider2) takes 1 uint16_t parameter for slider 1 (or slider 2)
 * 
 * bleGamepad.setHat1 takes a hat position as above (or 0 = centered and 1~8 are the 8 possible directions)
 * 
 * setHats amd setSliders functions are also available for setting all hats/sliders at once
 *
 * The example shows that you can set axes/hats independantly, or together.
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
    Serial.println("Press buttons 1, 32 and 64. Set DPAD to down right.");

    //Press buttons 5, 32 and 64
    bleGamepad.press(BUTTON_5);
    bleGamepad.press(BUTTON_32);
    bleGamepad.press(BUTTON_64);

    //Move all axes to max. 
    bleGamepad.setLeftThumb(32767, 32767);
    bleGamepad.setRightThumb(32767, 32767);
    bleGamepad.setLeftTrigger(65535);
    bleGamepad.setRightTrigger(65535);
    bleGamepad.setSlider1(65535);
    bleGamepad.setSlider2(65535);

    //Set hat 1 to down right (hats are otherwise centered by default)
    bleGamepad.setHat1(DPAD_DOWN_RIGHT);
    
    //Send the gamepad report
    bleGamepad.sendReport();
    delay(500);

    Serial.println("Release button 5 and 64. Move all axes to min. Set DPAD to centred.");
    bleGamepad.release(BUTTON_5);
    bleGamepad.release(BUTTON_64);
    bleGamepad.setAxes(-32767, -32767, -32767, -32767, 0, 0, 0, 0, DPAD_CENTERED);
    bleGamepad.sendReport();
    delay(500);
  }
}