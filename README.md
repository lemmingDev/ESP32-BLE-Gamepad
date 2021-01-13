# ESP32-BLE-Gamepad

Changed the original [ESP32-BLE-Gamepad](https://github.com/lemmingDev/ESP32-BLE-Gamepad) to support NimBLE.

## Features

 - [x] Button press (64 buttons)
 - [x] Button release (64 buttons)
 - [x] Axes movement (6 axes (16 bit) (x, y, z, rZ, rX, rY) --> (Left Thumb X, Left Thumb Y, Right Thumb X, Right Thumb Y, Left Trigger, Right Trigger))
 - [x] 2 Sliders (16 bit) (Slider 1 and Slider 2)
 - [x] 4 point of view hats (ie. d-pad plus 3 other hat switches)
 - [x] Report optional battery level to host (basically works, but it doesn't show up in Android's status bar)
 - [x] Customize Bluetooth device name/manufacturer
 - [x] Uses efficient NimBLE bluetooth library
 - [x] Compatible with Windows
 - [x] Compatible with Android (Android OS maps default buttons / axes / hats slightly differently than Windows)
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
```
By default, reports are sent on every button press/release or axis/slider/hat movement, however this can be disabled, and then you manually call sendReport on the gamepad instance as shown in the IndividualAxes.ino example.

There is also Bluetooth specific information that you can use (optional):

Instead of `BleGamepad bleGamepad;` you can do `BleGamepad bleGamepad("Bluetooth Device Name", "Bluetooth Device Manufacturer", 100);`.
The third parameter is the initial battery level of your device. To adjust the battery level later on you can simply call e.g.  `bleGamepad.setBatteryLevel(50)` (set battery level to 50%).
By default the battery level will be set to 100%, the device name will be `ESP32 BLE Gamepad` and the manufacturer will be `Espressif`.


## Credits
Credits to [T-vK](https://github.com/T-vK) as this library is based on his ESP32-BLE-Mouse library (https://github.com/T-vK/ESP32-BLE-Mouse) that he provided.

Credits to [chegewara](https://github.com/chegewara) as the ESP32-BLE-Mouse library is based on [this piece of code](https://github.com/nkolban/esp32-snippets/issues/230#issuecomment-473135679) that he provided.

Credits to [wakwak-koba](https://github.com/wakwak-koba) for the NimBLE [code](https://github.com/wakwak-koba/ESP32-NimBLE-Gamepad) that he provided.

## Notes
This library allows you to make the ESP32 act as a Bluetooth Mouse and control what it does.  
Relies on [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)

You might also be interested in:
- [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse)
- [ESP32-BLE-Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard)

or the NimBLE versions at

- [ESP32-NimBLE-Mouse](https://github.com/wakwak-koba/ESP32-NimBLE-Mouse)
- [ESP32-NimBLE-Keyboard](https://github.com/wakwak-koba/ESP32-NimBLE-Keyboard)
