/*
 * Test all gamepad buttons, axes and dpad
 */

#include <Arduino.h>
#include <BleGamepad.h>

#define numOfButtons 64
#define numOfHatSwitches 4

BleGamepad bleGamepad;
BleGamepadConfiguration bleGamepadConfig;

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting BLE work!");
    bleGamepadConfig.setAutoReport(false);
    bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
    bleGamepadConfig.setButtonCount(numOfButtons);
    bleGamepadConfig.setHatSwitchCount(numOfHatSwitches);
    bleGamepadConfig.setVid(0xe502);
    bleGamepadConfig.setPid(0xabcd);
    // Some non-Windows operating systems and web based gamepad testers don't like min axis set below 0, so 0 is set by default
    //bleGamepadConfig.setAxesMin(0x8001); // -32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
    bleGamepadConfig.setAxesMin(0x0000); // 0 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
    bleGamepadConfig.setAxesMax(0x7FFF); // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal 
    bleGamepad.begin(&bleGamepadConfig); // Simulation controls, special buttons and hats 2/3/4 are disabled by default

    // Changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
    if (bleGamepad.isConnected())
    {
        Serial.println("\nn--- Axes Decimal ---");
        Serial.print("Axes Min: ");
        Serial.println(bleGamepadConfig.getAxesMin());
        Serial.print("Axes Max: ");
        Serial.println(bleGamepadConfig.getAxesMax());
        Serial.println("\nn--- Axes Hex ---");
        Serial.print("Axes Min: ");
        Serial.println(bleGamepadConfig.getAxesMin(), HEX);
        Serial.print("Axes Max: ");
        Serial.println(bleGamepadConfig.getAxesMax(), HEX);        
        
        Serial.println("\n\nPress all buttons one by one");
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
        bleGamepad.sendReport();
        delay(100);
        bleGamepad.pressSelect();
        bleGamepad.sendReport();
        delay(100);
        bleGamepad.releaseStart();
        bleGamepad.sendReport();
        delay(100);
        bleGamepad.releaseSelect();
        bleGamepad.sendReport();

        Serial.println("Move all axis simultaneously from min to max");
        for (int i = bleGamepadConfig.getAxesMin(); i < bleGamepadConfig.getAxesMax(); i += (bleGamepadConfig.getAxesMax() / 256) + 1)
        {
            bleGamepad.setAxes(i, i, i, i, i, i);       // (x, y, z, rx, ry, rz)
            //bleGamepad.setHIDAxes(i, i, i, i, i, i);  // (x, y, z, rz, rx, ry)
            bleGamepad.sendReport();
            delay(10);
        }
        bleGamepad.setAxes(); // Reset all axes to zero
        bleGamepad.sendReport();

        Serial.println("Move all sliders simultaneously from min to max");
        for (int i = bleGamepadConfig.getAxesMin(); i < bleGamepadConfig.getAxesMax(); i += (bleGamepadConfig.getAxesMax() / 256) + 1)
        {
            bleGamepad.setSliders(i, i);
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
        //    for (int i = bleGamepadConfig.getSimulationMin(); i < bleGamepadConfig.getSimulationMax(); i += (bleGamepadConfig.getAxesMax() / 256) + 1)
        //    {
        //      bleGamepad.setRudder(i);
        //      bleGamepad.setThrottle(i);
        //      bleGamepad.setAccelerator(i);
        //      bleGamepad.setBrake(i);
        //      bleGamepad.setSteering(i);
        //      bleGamepad.sendReport();
        //      delay(10);
        //    }
        //    bleGamepad.setSimulationControls(); //Reset all simulation controls to zero
        bleGamepad.sendReport();
    }
}
