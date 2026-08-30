/*
 * Driving controller test
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
#define enableRudder false
#define enableThrottle false
#define enableAccelerator true
#define enableBrake true
#define enableSteering true

//int16_t simMin = 0x8000;      // -32767 --> Some non-Windows operating systems and web based gamepad testers don't like min axis set below 0, so 0 is set by default
//int16_t axesCenter = 0x00;      
int16_t simMin = 0x00;        // Set simulation minimum axes to zero.
int16_t axesCenter = 0x3FFF;
int16_t simMax = 0x7FFF;        // 32767 --> int16_t - 16 bit signed integer - Can be in decimal or hexadecimal
int16_t stepAmount = 0xFF;
uint16_t delayAmount = 25;

BleGamepad bleGamepad("BLE Driving Controller", "lemmingDev", 100);

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    // Setup controller with 10 buttons, accelerator, brake and steering
    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setAutoReport(false);
    bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD); // CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
    bleGamepadConfig.setButtonCount(numOfButtons);
    bleGamepadConfig.setWhichAxes(enableX, enableY, enableZ, enableRX, enableRY, enableRZ, enableSlider1, enableSlider2);      // Can also be done per-axis individually. All are true by default
    bleGamepadConfig.setWhichSimulationControls(enableRudder, enableThrottle, enableAccelerator, enableBrake, enableSteering); // Can also be done per-control individually. All are false by default
    bleGamepadConfig.setHatSwitchCount(numOfHatSwitches);                                                                      // 1 by default
    bleGamepadConfig.setSimulationMin(simMin);
    bleGamepadConfig.setSimulationMax(simMax);
    
    bleGamepad.begin(&bleGamepadConfig);
    // changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again

    // Set steering to center
    bleGamepad.setSteering(axesCenter);

    // Set brake and accelerator to min
    bleGamepad.setBrake(simMin);
    bleGamepad.setAccelerator(simMax);

    bleGamepad.sendReport();
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

        Serial.println("Move steering from center to min");
        for (int i = axesCenter; i > simMin; i -= stepAmount)
        {
            bleGamepad.setSteering(i);
            bleGamepad.sendReport();
            delay(delayAmount);
        }

        Serial.println("Move steering from min to max");
        for (int i = simMin; i < simMax; i += stepAmount)
        {
            bleGamepad.setSteering(i);
            bleGamepad.sendReport();
            delay(delayAmount);
        }

        Serial.println("Move steering from max to center");
        for (int i = simMax; i > axesCenter; i -= stepAmount)
        {
            bleGamepad.setSteering(i);
            bleGamepad.sendReport();
            delay(delayAmount);
        }
        bleGamepad.setSteering(axesCenter);
        bleGamepad.sendReport();

        Serial.println("Move accelerator from min to max");
        // Axis is reversed, so swap min <--> max
        for (int i = simMax; i > simMin; i -= stepAmount)
        {
            bleGamepad.setAccelerator(i);
            bleGamepad.sendReport();
            delay(delayAmount);
        }

        Serial.println("Move accelerator from max to min");
        // Axis is reversed, so swap min <--> max
        for (int i = simMin; i < simMax; i += stepAmount)
        {
            bleGamepad.setAccelerator(i);
            bleGamepad.sendReport();
            delay(delayAmount);
        }
        bleGamepad.setAccelerator(simMax);
        bleGamepad.sendReport();

        Serial.println("Move brake from min to max");
        for (int i = simMin; i < simMax; i += stepAmount)
        {
            bleGamepad.setBrake(i);
            bleGamepad.sendReport();
            delay(delayAmount);
        }
        
        Serial.println("Move brake from max to min");
        for (int i = simMax; i > simMin; i -= stepAmount)
        {
            bleGamepad.setBrake(i);
            bleGamepad.sendReport();
            delay(delayAmount);
        }
        bleGamepad.setBrake(simMin);
        bleGamepad.sendReport();
    }
}