# ESP32-BLE-Gamepad

## License
Published under the MIT license. Please see license.txt.

It would be great however if any improvements are fed back into this version.

## Features

 - [x] Button press (128 buttons)
 - [x] Button release (128 buttons)
 - [x] Axes movement (6 axes (configurable resolution up to 16 bit) (x, y, z, rX, rY, rZ) --> In Windows usually (Left Thumb X, Left Thumb Y, Right Thumb X, Left Trigger, Right Trigger, Right Thumb Y))
 - [x] Gyroscope and Accelerometer
 - [x] Set battery percentage
 - [x] Set battery power state information using UUID 0x2A1A. Use nRF Connect on Android for example to see this information
 - [x] 2 Sliders (configurable resolution up to 16 bit) (Slider 1 and Slider 2)
 - [x] 4 point of view hats (ie. d-pad plus 3 other hat switches)
 - [x] Simulation controls (rudder, throttle, accelerator, brake, steering)
 - [x] Special buttons (start, select, menu, home, back, volume up, volume down, volume mute) all disabled by default
 - [x] Configurable HID descriptor
 - [x] Configurable VID and PID values
 - [x] Configurable BLE characteristics (name, manufacturer, model number, software revision, serial number, firmware revision, hardware revision)	
 - [x] Report optional battery level to host
 - [x] Uses efficient NimBLE bluetooth library
 - [x] Output report function
 - [x] Functions available for force pairing/ignore current client and/or delete pairings
 - [x] Compatible with Windows
 - [x] Compatible with Android (Android OS maps default buttons / axes / hats slightly differently than Windows) (see notes)
 - [x] Compatible with Linux (limited testing)
 - [x] Compatible with MacOS X (limited testing)
 - [ ] Compatible with iOS (No - not even for accessibility switch - This is not a “Made for iPhone” (MFI) compatible device)
                           (Use the Xinput fork suggested below which has been tested to work)

## NimBLE
Since version 3 of this library, the more efficient NimBLE library is used instead of the default BLE implementation
Please use the library manager to install it, or get it from here: https://github.com/h2zero/NimBLE-Arduino
Since version 3, this library also supports a configurable HID desciptor, which allows users to customise how the device presents itself to the OS (number of buttons, hats, axes, sliders, simulation controls etc).
See the examples for guidance.

## POSSIBLE BREAKING CHANGES - PLEASE READ
A large code rebase (configuration class) along with some extra features (start, select, menu, home, back, volume up, volume down and volume mute buttons) has been committed thanks to @dexterdy

Since version 5 of this library, the axes and simulation controls have configurable min and max values
The decision was made to set defaults to 0 for minimum and 32767 for maximum (previously -32767 to 32767)
This was due to the fact that non-Windows operating systems and some online web-based game controller testers didn't play well with negative numbers. Existing sketches should take note, and see the DrivingControllerTest example for how to set back to -32767 if wanted

This version endeavors to be compatible with the latest released version of NimBLE-Arduino through the Arduino Library Manager; currently version 2.2.1 at the time of this writing; --> https://github.com/h2zero/NimBLE-Arduino/releases/tag/2.2.1

setAxes accepts axes in the order (x, y, z, rx, ry, rz)
setHIDAxes accepts them in the order (x, y, z, rz, rx, ry)

Please see updated examples

## Installation
- (Make sure you can use the ESP32 with the Arduino IDE. [Instructions can be found here.](https://github.com/espressif/arduino-esp32#installation-instructions))
- [Download the latest release of this library from the release page.](https://github.com/lemmingDev/ESP32-BLE-Gamepad/releases)
- In the Arduino IDE go to "Sketch" -> "Include Library" -> "Add .ZIP Library..." and select the file you just downloaded.
- In the Arduino IDE go to "Tools" -> "Manage Libraries..." -> Filter for "NimBLE-Arduino" by h2zero and install.
- You can now go to "File" -> "Examples" -> "ESP32 BLE Gamepad" and select an example to get started.

## Example

``` C++
/*
 * This example turns the ESP32 into a Bluetooth LE gamepad that presses buttons and moves axis
 *
 * At the moment we are using the default settings, but they can be canged using a BleGamepadConfig instance as parameter for the begin function.
 *
 * Possible buttons are:
 * BUTTON_1 through to BUTTON_16
 * (16 buttons by default. Library can be configured to use up to 128)
 *
 * Possible DPAD/HAT switch position values are:
 * DPAD_CENTERED, DPAD_UP, DPAD_UP_RIGHT, DPAD_RIGHT, DPAD_DOWN_RIGHT, DPAD_DOWN, DPAD_DOWN_LEFT, DPAD_LEFT, DPAD_UP_LEFT
 * (or HAT_CENTERED, HAT_UP etc)
 *
 * bleGamepad.setAxes sets all axes at once. There are a few:
 * (x axis, y axis, z axis, rx axis, ry axis, rz axis, slider 1, slider 2)
 *
 * Alternatively, bleGamepad.setHIDAxes sets all axes at once. in the order of:
 * (x axis, y axis, z axis, rz axis, ry axis, rz axis, slider 1, slider 2)  <- order HID report is actually given in
 *
 * Library can also be configured to support up to 5 simulation controls
 * (rudder, throttle, accelerator, brake, steering), but they are not enabled by default.
 *
 * Library can also be configured to support different function buttons
 * (start, select, menu, home, back, volume increase, volume decrease, volume mute)
 * start and select are enabled by default
 */

#include <Arduino.h>
#include <BleGamepad.h>

BleGamepad bleGamepad;

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting BLE work!");
    bleGamepad.begin();
    // The default bleGamepad.begin() above enables 16 buttons, all axes, one hat, and no simulation controls or special buttons
}

void loop()
{
    if (bleGamepad.isConnected())
    {
        Serial.println("Press buttons 5, 16 and start. Move all enabled axes to max. Set DPAD (hat 1) to down right.");
        bleGamepad.press(BUTTON_5);
        bleGamepad.press(BUTTON_16);
        bleGamepad.pressStart();
        bleGamepad.setAxes(32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);       //(X, Y, Z, RX, RY, RZ)
        //bleGamepad.setHIDAxes(32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767);  //(X, Y, Z, RZ, RX, RY)
        bleGamepad.setHat1(HAT_DOWN_RIGHT);
        // All axes, sliders, hats etc can also be set independently. See the IndividualAxes.ino example
        delay(500);

        Serial.println("Release button 5 and start. Move all axes to min. Set DPAD (hat 1) to centred.");
        bleGamepad.release(BUTTON_5);
        bleGamepad.releaseStart();
        bleGamepad.setHat1(HAT_CENTERED);
        bleGamepad.setAxes(0, 0, 0, 0, 0, 0, 0, 0);           //(X, Y, Z, RX, RY, RZ)
        //bleGamepad.setHIDAxes(0, 0, 0, 0, 0, 0, 0, 0);      //(X, Y, Z, RZ, RX, RY)
        delay(500);
    }
}

```
By default, reports are sent on every button press/release or axis/slider/hat/simulation movement, however this can be disabled, and then you manually call sendReport on the gamepad instance as shown in the IndividualAxes.ino example.

VID and PID values can be set. See TestAll.ino for example.

There is also Bluetooth specific information that you can use (optional):

Instead of `BleGamepad bleGamepad;` you can do `BleGamepad bleGamepad("Bluetooth Device Name", "Bluetooth Device Manufacturer", 100);`.
The third parameter is the initial battery level of your device.
By default the battery level will be set to 100%, the device name will be `ESP32 BLE Gamepad` and the manufacturer will be `Espressif`.

Battery level can be set during operation by calling, for example, bleGamepad.setBatteryLevel(80);
Update sent on next gamepad update if auto reporting is not enabled


## Troubleshooting Guide
Troubleshooting guide and suggestions can be found in [TroubleshootingGuide](TroubleshootingGuide.md)

## Credits
Credits to [T-vK](https://github.com/T-vK) as this library is based on his ESP32-BLE-Mouse library (https://github.com/T-vK/ESP32-BLE-Mouse) that he provided.

Credits to [chegewara](https://github.com/chegewara) as the ESP32-BLE-Mouse library is based on [this piece of code](https://github.com/nkolban/esp32-snippets/issues/230#issuecomment-473135679) that he provided.

Credits to [wakwak-koba](https://github.com/wakwak-koba) for the NimBLE [code](https://github.com/wakwak-koba/ESP32-NimBLE-Gamepad) that he provided.

## Notes
This library allows you to make the ESP32 act as a Bluetooth Gamepad and control what it does.  
Relies on [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)

Use [this](http://www.planetpointy.co.uk/joystick-test-application/) Windows test app to test/see all of the buttons
Ensure you have Direct X 9 installed

Gamepads desgined for Android use a different button mapping. This effects analog triggers, where the standard left and right trigger axes are not detected.
Android calls the HID report for right trigger `"GAS"` and left trigger `"BRAKE"`. Enabling the `"Accelerator"` and `"Brake"` simulation controls allows them to be used instead of right and left trigger.

Right thumbstick on Windows is usually z, rz, whereas on Android, this may be z, rx, so you may want to set them separately with setZ and setRX, instead of using setRightThumb(z, rz), or use setRightThumbAndroid(z, rx)

You might also be interested in:
- [ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse)
- [ESP32-BLE-Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard)
- [Composite Gamepad/Mouse/Keyboard and Xinput capable fork of this library](https://github.com/Mystfit/ESP32-BLE-CompositeHID)

or the NimBLE versions at

- [ESP32-NimBLE-Mouse](https://github.com/wakwak-koba/ESP32-NimBLE-Mouse)
- [ESP32-NimBLE-Keyboard](https://github.com/wakwak-koba/ESP32-NimBLE-Keyboard)
