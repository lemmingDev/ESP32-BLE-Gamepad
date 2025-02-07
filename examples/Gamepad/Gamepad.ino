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
 * (x axis, y axis, z axis, rZ axis, rX axis, rY axis, slider 1, slider 2)  <- order HID report is actually given in
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
