/*
 * This example turns the ESP32 into a Bluetooth LE gamepad that presses buttons and moves axis
 *
 * Possible buttons are:
 * BUTTON_1 through to BUTTON_128 (Windows gamepad tester only visualises the first 32)
 ^ Use http://www.planetpointy.co.uk/joystick-test-application/ to visualise all of them
 * Whenever you adjust the amount of buttons/axes etc, make sure you unpair and repair the BLE device
 *
 * Possible DPAD/HAT switch position values are:
 * DPAD_CENTERED, DPAD_UP, DPAD_UP_RIGHT, DPAD_RIGHT, DPAD_DOWN_RIGHT, DPAD_DOWN, DPAD_DOWN_LEFT, DPAD_LEFT, DPAD_UP_LEFT
 *
 * bleGamepad.setAxes takes the following int16_t parameters for the Left/Right Thumb X/Y, Left/Right Triggers plus slider1 and slider2:
 * (Left Thumb X, Left Thumb Y, Right Thumb X, Right Thumb Y, Left Trigger, Right Trigger, Slider 1, Slider 2) (x, y, z, rx, ry, rz)
 *
 * bleGamepad.setHIDAxes instead takes them in a slightly different order (x, y, z, rz, rx, ry)
 *
 * bleGamepad.setLeftThumb (or setRightThumb) takes 2 int16_t parameters for x and y axes (or z and rZ axes)
 * bleGamepad.setRightThumbAndroid takes 2 int16_t parameters for z and rx axes
 *
 * bleGamepad.setLeftTrigger (or setRightTrigger) takes 1 int16_t parameter for rX axis (or rY axis)
 *
 * bleGamepad.setSlider1 (or setSlider2) takes 1 int16_t parameter for slider 1 (or slider 2)
 *
 * bleGamepad.setHat1 takes a hat position as above (or 0 = centered and 1~8 are the 8 possible directions)
 *
 * setHats, setTriggers and setSliders functions are also available for setting all hats/triggers/sliders at once
 *
 * The example shows that you can set axes/hats independantly, or together.
 *
 * It also shows that you can disable the autoReport feature (enabled by default), and manually call the sendReport() function when wanted
 *
 */

#include <Arduino.h>
#include <BleGamepad.h>

BleGamepad bleGamepad;

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting BLE work!");
    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setAutoReport(false); // This is true by default
    bleGamepadConfig.setButtonCount(128);
    bleGamepadConfig.setHatSwitchCount(2);
    bleGamepad.begin(&bleGamepadConfig); // Creates a gamepad with 128 buttons, 2 hat switches and x, y, z, rZ, rX, rY and 2 sliders (no simulation controls enabled by default)

    // Changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
    if (bleGamepad.isConnected())
    {
        Serial.println("Press buttons 1, 32, 64 and 128. Set hat 1 to down right and hat 2 to up left");

        // Press buttons 5, 32, 64 and 128
        bleGamepad.press(BUTTON_5);
        bleGamepad.press(BUTTON_32);
        bleGamepad.press(BUTTON_64);
        bleGamepad.press(BUTTON_128);

        // Move all axes to max.
        bleGamepad.setLeftThumb(32767, 32767);  // or bleGamepad.setX(32767); and bleGamepad.setY(32767);
        bleGamepad.setRightThumb(32767, 32767); // or bleGamepad.setZ(32767); and bleGamepad.setRZ(32767);
        bleGamepad.setLeftTrigger(32767);       // or bleGamepad.setRX(32767);
        bleGamepad.setRightTrigger(32767);      // or bleGamepad.setRY(32767);
        bleGamepad.setSlider1(32767);
        bleGamepad.setSlider2(32767);

        // Set hat 1 to down right and hat 2 to up left (hats are otherwise centered by default)
        bleGamepad.setHat1(DPAD_DOWN_RIGHT); // or bleGamepad.setHat1(HAT_DOWN_RIGHT);
        bleGamepad.setHat2(DPAD_UP_LEFT);    // or bleGamepad.setHat2(HAT_UP_LEFT);
        // Or bleGamepad.setHats(DPAD_DOWN_RIGHT, DPAD_UP_LEFT);

        // Send the gamepad report
        bleGamepad.sendReport();
        delay(500);

        Serial.println("Release button 5 and 64. Move all axes to min. Set hat 1 and 2 to centred.");
        bleGamepad.release(BUTTON_5);
        bleGamepad.release(BUTTON_64);
        bleGamepad.setAxes(0, 0, 0, 0, 0, 0, 0, 0);
        bleGamepad.setHats(DPAD_CENTERED, HAT_CENTERED);
        bleGamepad.sendReport();
        delay(500);
    }
}
