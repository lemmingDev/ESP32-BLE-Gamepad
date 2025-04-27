/*
 * This code programs an ESP32 as a BLE fightstick
 *
 * Before using, adjust the numOfButtons, buttonPins, physicalButtons, hatPins etc to suit your scenario
 *
 * If you're looking for a more compatible fighstick experience in games, try the xinput compatible fork at:
 * https://github.com/Mystfit/ESP32-BLE-CompositeHID
 *
 */

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

BleGamepad bleGamepad("BLE Fightstick", "lemmingDev", 100);

#define numOfButtons 11
#define numOfHats 1 // Maximum 4 hat switches supported

byte previousButtonStates[numOfButtons];
byte currentButtonStates[numOfButtons];
byte buttonPins[numOfButtons] = {13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25};
byte physicalButtons[numOfButtons] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

byte previousHatStates[numOfHats * 4];
byte currentHatStates[numOfHats * 4];
byte hatPins[numOfHats * 4] = {26, 27, 32, 33}; // In order UP, LEFT, DOWN, RIGHT. 4 pins per hat switch (Eg. List 12 pins if there are 3 hat switches)

void setup()
{
    // Setup Buttons
    for (byte currentPinIndex = 0; currentPinIndex < numOfButtons; currentPinIndex++)
    {
        pinMode(buttonPins[currentPinIndex], INPUT_PULLUP);
        previousButtonStates[currentPinIndex] = HIGH;
        currentButtonStates[currentPinIndex] = HIGH;
    }

    // Setup Hat Switches
    for (byte currentPinIndex = 0; currentPinIndex < numOfHats * 4; currentPinIndex++)
    {
        pinMode(hatPins[currentPinIndex], INPUT_PULLUP);
        previousHatStates[currentPinIndex] = HIGH;
        currentHatStates[currentPinIndex] = HIGH;
    }

    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setAutoReport(false);
    bleGamepadConfig.setWhichAxes(0, 0, 0, 0, 0, 0, 0, 0);	// Disable all axes
    bleGamepadConfig.setButtonCount(numOfButtons);
    bleGamepadConfig.setHatSwitchCount(numOfHats);
    bleGamepad.begin(&bleGamepadConfig);

    // changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
    if (bleGamepad.isConnected())
    {
        // Deal with buttons
        for (byte currentIndex = 0; currentIndex < numOfButtons; currentIndex++)
        {
            currentButtonStates[currentIndex] = digitalRead(buttonPins[currentIndex]);

            if (currentButtonStates[currentIndex] != previousButtonStates[currentIndex])
            {
                if (currentButtonStates[currentIndex] == LOW)
                {
                    bleGamepad.press(physicalButtons[currentIndex]);
                }
                else
                {
                    bleGamepad.release(physicalButtons[currentIndex]);
                }
            }
        }

        // Update hat switch pin states
        for (byte currentHatPinsIndex = 0; currentHatPinsIndex < numOfHats * 4; currentHatPinsIndex++)
        {
            currentHatStates[currentHatPinsIndex] = digitalRead(hatPins[currentHatPinsIndex]);
        }

        // Update hats
        signed char hatValues[4] = {0, 0, 0, 0};

        for (byte currentHatIndex = 0; currentHatIndex < numOfHats; currentHatIndex++)
        {
            signed char hatValueToSend = 0;

            for (byte currentHatPin = 0; currentHatPin < 4; currentHatPin++)
            {
                // Check for direction
                if (currentHatStates[currentHatPin + currentHatIndex * 4] == LOW)
                {
                    hatValueToSend = currentHatPin * 2 + 1;

                    // Account for last diagonal
                    if (currentHatPin == 0)
                    {
                        if (currentHatStates[currentHatIndex * 4 + 3] == LOW)
                        {
                            hatValueToSend = 8;
                            break;
                        }
                    }

                    // Account for first 3 diagonals
                    if (currentHatPin < 3)
                    {
                        if (currentHatStates[currentHatPin + currentHatIndex * 4 + 1] == LOW)
                        {
                            hatValueToSend += 1;
                            break;
                        }
                    }
                }
            }

            hatValues[currentHatIndex] = hatValueToSend;
        }

        // Set hat values
        bleGamepad.setHats(hatValues[0], hatValues[1], hatValues[2], hatValues[3]);

        // Update previous states to current states and send report
        // readable, but with compiler warning:
        //  if (currentButtonStates != previousButtonStates || currentHatStates != previousHatStates)
        if ((memcmp((const void *)currentButtonStates, (const void *)previousButtonStates, sizeof(currentButtonStates)) != 0) && (memcmp((const void *)currentHatStates, (const void *)previousHatStates, sizeof(currentHatStates)) != 0))
        {
            for (byte currentIndex = 0; currentIndex < numOfButtons; currentIndex++)
            {
                previousButtonStates[currentIndex] = currentButtonStates[currentIndex];
            }

            for (byte currentIndex = 0; currentIndex < numOfHats * 4; currentIndex++)
            {
                previousHatStates[currentIndex] = currentHatStates[currentIndex];
            }

            bleGamepad.sendReport();	// Send a report if any of the button states or hat directions have changed
        }

        delay(10);	// Reduce for less latency
    }
}
