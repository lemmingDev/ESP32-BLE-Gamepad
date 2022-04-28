/*
 * Test all gamepad buttons, axes and dpad
 */

#include <Arduino.h>
#include <BleGamepad.h>

#define numOfButtons 128
#define numOfHatSwitches 4

BleGamepad bleGamepad;

void setup()
{
    BleGamepadConfiguration bleGamepadConfig;
    Serial.begin(115200);
    Serial.println("Starting BLE work!");
    bleGamepadConfig.setAutoReport(false);
    bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
    bleGamepadConfig.setButtonCount(numOfButtons);
    bleGamepadConfig.setHatSwitchCount(numOfHatSwitches);
    bleGamepad.begin(bleGamepadConfig); // Simulation controls are disabled by default

    // changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
    if (bleGamepad.isConnected())
    {
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

        Serial.println("Press start and select");
        bleGamepad.pressStart();
        delay(100);
        bleGamepad.pressSelect();
        delay(100);
        bleGamepad.releaseStart();
        delay(100);
        bleGamepad.releaseSelect();

        Serial.println("Move all axis simultaneously from min to max");
        for (int i = -127; i < 128; i += 1)
        {
            bleGamepad.setAxes(i * 256, i * 256, i * 256, i * 256, i * 256, i * 256);
            bleGamepad.sendReport();
            delay(10);
        }
        bleGamepad.setAxes(); // Reset all axes to zero
        bleGamepad.sendReport();

        Serial.println("Move all sliders simultaneously from min to max");
        for (int i = -127; i < 128; i += 1)
        {
            bleGamepad.setSliders(i * 256, i * 256);
            bleGamepad.sendReport();
            delay(10);
        }
        bleGamepad.setSliders(); // Reset all sliders to zero
        bleGamepad.sendReport();

        Serial.println("Send hat switch 1 one by one in an anticlockwise rotation");
        for (int i = 8; i >= 0; i--)
        {
            bleGamepad.setHat1(i);
            bleGamepad.sendReport();
            delay(200);
        }

        Serial.println("Send hat switch 2 one by one in an anticlockwise rotation");
        for (int i = 8; i >= 0; i--)
        {
            bleGamepad.setHat2(i);
            bleGamepad.sendReport();
            delay(200);
        }

        Serial.println("Send hat switch 3 one by one in an anticlockwise rotation");
        for (int i = 8; i >= 0; i--)
        {
            bleGamepad.setHat3(i);
            bleGamepad.sendReport();
            delay(200);
        }

        Serial.println("Send hat switch 4 one by one in an anticlockwise rotation");
        for (int i = 8; i >= 0; i--)
        {
            bleGamepad.setHat4(i);
            bleGamepad.sendReport();
            delay(200);
        }

        //    Simulation controls are disabled by default
        //    Serial.println("Move all simulation controls simultaneously from min to max");
        //    for(int i = -127 ; i < 128 ; i += 1)
        //    {
        //      bleGamepad.setRudder(i*256);
        //      bleGamepad.setThrottle(i*256);
        //      bleGamepad.setAccelerator(i*256);
        //      bleGamepad.setBrake(i*256);
        //      bleGamepad.setSteering(i*256);
        //      bleGamepad.sendReport();
        //      delay(10);
        //    }
        //    bleGamepad.setSimulationControls(); //Reset all simulation controls to zero
        bleGamepad.sendReport();
    }
}