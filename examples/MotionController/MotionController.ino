/*
   Motion controller test

   USB HID specification can only support 8 axes, plus hat (shows as 9th axis in online testers),
   so Please don't add more than that

   So, if you have left thumbstick (X, Y -> 2 axes), then add gyroscope (Rx, Ry, Rz -> 3 axes) and
   accelerometer (rX, rY, rZ -> 3 axes), then you're already at 8

   Windows joy.cpl will show the gyroscope data as X Rotation, Y Rotation and Z Rotation
   Windows joy.cpl will not show the accelerometer data, but you can see it using online
   tools such as https://hardwaretester.com/gamepad

   This HID descriptor uses the correct HID usage IDs for gyroscope (33, 34, 35 -> Rotation -> Rx, Ry, Rz) and
   accelerometer (40, 41, 42 -> Vector -> Vx, Vy, Vz)
   https://www.usb.org/sites/default/files/hut1_6.pdf

   Unfortunately, Windows and other OSs don't usually know exactly what their intent is and may not
   map them exactly how you think

   Controllers such as PS3/4 Dual Shock use custom HID reports and drivers to get around this

*/

#include <Arduino.h>
#include <BleGamepad.h>

#define numOfButtons 10
#define numOfHatSwitches 0
#define enableX false
#define enableY false
#define enableZ false
#define enableRX false
#define enableRY false
#define enableRZ false
#define enableSlider1 false
#define enableSlider2 false

int16_t motionMin = 0x8000;       // -32767 --> Some non-Windows operating systems and web based gamepad testers don't like min axis set below 0, so 0 is set by default
int16_t motionCenter = 0x00;
//int16_t motionMin = 0x00;         // Set motionulation minimum axes to zero.
//int16_t motionCenter = 0x3FFF;
int16_t motionMax = 0x7FFF;       // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
int16_t stepAmount = 0xFF;        // 255
uint16_t delayAmount = 25;

BleGamepad bleGamepad("BLE Motion Controller", "lemmingDev", 100);

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  // Setup controller with 10 buttons, gyroscope and accelerometer
  BleGamepadConfiguration bleGamepadConfig;
  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  bleGamepadConfig.setButtonCount(numOfButtons);
  bleGamepadConfig.setWhichAxes(enableX, enableY, enableZ, enableRX, enableRY, enableRZ, enableSlider1, enableSlider2);      // Can also be done per-axis individually. All are true by default
  bleGamepadConfig.setHatSwitchCount(numOfHatSwitches);                                                                      // 1 by default
  bleGamepadConfig.setIncludeGyroscope(true);
  bleGamepadConfig.setIncludeAccelerometer(true);
  bleGamepadConfig.setMotionMin(motionMin);
  bleGamepadConfig.setMotionMax(motionMax);

  bleGamepad.begin(&bleGamepadConfig);
  // Changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again

  // Set gyroscope and accelerometer to center (first 3 are gyroscope, last 3 are accelerometer)
  bleGamepad.setMotionControls(motionCenter, motionCenter, motionCenter, motionCenter, motionCenter, motionCenter);

  bleGamepad.sendReport();
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // BUTTONS
    Serial.println("Press all buttons one by one");
    for (int i = 1; i <= numOfButtons; i += 1)
    {
      bleGamepad.press(i);
      bleGamepad.sendReport();
      delay(100);
      bleGamepad.release(i);
      bleGamepad.sendReport();
      delay(25);
    }

    // GYROSCOPE
    Serial.println("Move all 3 gyroscope axes from center to min");
    for (int i = motionCenter; i > motionMin; i -= stepAmount)
    {
      bleGamepad.setGyroscope(i, i, i);
      bleGamepad.sendReport();
      delay(delayAmount);
    }

    Serial.println("Move all 3 gyroscope axes from min to max");
    for (int i = motionMin; i < motionMax; i += stepAmount)
    {
      bleGamepad.setGyroscope(i, i, i);
      bleGamepad.sendReport();
      delay(delayAmount);
    }

    Serial.println("Move all 3 gyroscope axes from max to center");
    for (int i = motionMax; i > motionCenter; i -= stepAmount)
    {
      bleGamepad.setGyroscope(i, i, i);
      bleGamepad.sendReport();
      delay(delayAmount);
    }
    bleGamepad.setGyroscope(motionCenter);
    bleGamepad.sendReport();

    // ACCELEROMETER
    Serial.println("Move all 3 accelerometer axes from center to min");
    for (int i = motionCenter; i > motionMin; i -= stepAmount)
    {
      bleGamepad.setAccelerometer(i, i, i);
      bleGamepad.sendReport();
      delay(delayAmount);
    }

    Serial.println("Move all 3 accelerometer axes from min to max");
    for (int i = motionMin; i < motionMax; i += stepAmount)
    {
      bleGamepad.setAccelerometer(i, i, i);
      bleGamepad.sendReport();
      delay(delayAmount);
    }

    Serial.println("Move all 3 accelerometer axes from max to center");
    for (int i = motionMax; i > motionCenter; i -= stepAmount)
    {
      bleGamepad.setAccelerometer(i, i, i);
      bleGamepad.sendReport();
      delay(delayAmount);
    }
    bleGamepad.setAccelerometer(motionCenter);
    bleGamepad.sendReport();
  }
}