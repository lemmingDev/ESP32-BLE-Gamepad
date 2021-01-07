# ESP32-BLE-Gamepad
Bluetooth LE Gamepad library for the ESP32

This library allows you to make the ESP32 act as a Bluetooth Gamepad and control what it does. E.g. move axes and press buttons

## Features

 - [x] Button press (32 buttons)
 - [x] Button release (32 buttons)
 - [x] Axes movement (6 axes (x, y, z, rZ, rX, rY) --> (Left Thumb X, Left Thumb Y, Right Thumb X, Right Thumb Y, Left Trigger, Right Trigger)))
 - [x] Point of view hat (d-pad)
 - [x] Report optional battery level to host (basically works, but it doesn't show up in Android's status bar)
 - [x] Customize Bluetooth device name/manufacturer
 - [x] Compatible with Windows
 - [x] Compatible with Android (Android OS maps default buttons / axes slightly differently than Windows)
 - [x] Compatible with Linux
 - [x] Compatible with MacOS X
 - [ ] Compatible with iOS (No - not even for accessibility switch - This is not a “Made for iPhone” (MFI) compatible device)

## Installation
- (Make sure you can use the ESP32 with the Arduino IDE. [Instructions can be found here.](https://github.com/espressif/arduino-esp32#installation-instructions))
- [Download the latest release of this library from the release page.](https://github.com/lemmingDev/ESP32-BLE-Gamepad/releases)
- In the Arduino IDE go to "Sketch" -> "Include Library" -> "Add .ZIP Library..." and select the file you just downloaded.
- You can now go to "File" -> "Examples" -> "ESP32 BLE Gamepad" and select the example to get started.

## Example

``` C++
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
 * It also shows that you can disable the aotoReport feature (enabled by default), and manually call the sendReport() function when wanted 
 * 
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
    bleGamepad.setAutoReport(false);
    
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
```
By default, reports are sent on every button press/release or axis/hat movement, however this can be disabled, and then you manually call sendReport on the gamepad instance as shown in the example above.

There is also Bluetooth specific information that you can use (optional):

Instead of `BleGamepad bleGamepad;` you can do `BleGamepad bleGamepad("Bluetooth Device Name", "Bluetooth Device Manufacturer", 100);`.
The third parameter is the initial battery level of your device. To adjust the battery level later on you can simply call e.g.  `bleGamepad.setBatteryLevel(50)` (set battery level to 50%).
By default the battery level will be set to 100%, the device name will be `ESP32 BLE Gamepad` and the manufacturer will be `Espressif`.


## Credits
Credits to [T-vK](https://github.com/T-vK) as this library is based on his ESP32-BLE-Mouse library (https://github.com/T-vK/ESP32-BLE-Mouse) that he provided.

Credits to [chegewara](https://github.com/chegewara) as the ESP32-BLE-Mouse library is based on [this piece of code](https://github.com/nkolban/esp32-snippets/issues/230#issuecomment-473135679) that he provided.
