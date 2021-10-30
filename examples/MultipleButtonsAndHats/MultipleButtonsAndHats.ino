/*
 * This code programs a number of pins on an ESP32 as buttons and hats on a BLE gamepad
 * 
 * It uses arrays to cut down on code
 *
 * Before using, adjust the numOfButtons, buttonPins, physicalButtons, numOfHats and hatPins to suit your scenario
 *
 */
 
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

BleGamepad bleGamepad;

#define numOfButtons 4
#define numOfHats 1  // Maximum 4 hat switches supported

byte previousButtonStates[numOfButtons];
byte currentButtonStates[numOfButtons];
byte buttonPins[numOfButtons] = { 18, 19, 25, 26 };
byte physicalButtons[numOfButtons] = { 1, 2, 3, 4 };

byte previousHatStates[numOfHats*4];
byte currentHatStates[numOfHats*4];
byte hatPins[numOfHats*4] = { 0, 23, 18, 35 }; // in order UP, LEFT, DOWN, RIGHT. 4 pins per hat switch (Eg. List 12 pins if there are 3 hat switches)

void setup()
{
  // Setup Buttons
  for (byte currentPinIndex = 0 ; currentPinIndex < numOfButtons ; currentPinIndex++)
  {
    pinMode(buttonPins[currentPinIndex], INPUT_PULLUP);
    previousButtonStates[currentPinIndex] = HIGH;
    currentButtonStates[currentPinIndex] =  HIGH;
  }

  // Setup Hat Switches
  for (byte currentPinIndex = 0 ; currentPinIndex < numOfHats*4 ; currentPinIndex++)
  {
    pinMode(hatPins[currentPinIndex], INPUT_PULLUP);
    previousHatStates[currentPinIndex] = HIGH;
    currentHatStates[currentPinIndex] =  HIGH;
  }
  
  bleGamepad.begin(numOfButtons, numOfHats);
  bleGamepad.setAutoReport(false);
}

void loop()
{
  if(bleGamepad.isConnected())
  {
    // Deal with buttons
    for (byte currentIndex = 0 ; currentIndex < numOfButtons ; currentIndex++)
    {
      currentButtonStates[currentIndex]  = digitalRead(buttonPins[currentIndex]);

      if (currentButtonStates[currentIndex] != previousButtonStates[currentIndex])
      {
        if(currentButtonStates[currentIndex] == LOW)
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
    for (byte currentHatPinsIndex = 0 ; currentHatPinsIndex < numOfHats*4 ; currentHatPinsIndex++)
    {
      currentHatStates[currentHatPinsIndex]  = digitalRead(hatPins[currentHatPinsIndex]);
    }

    // Update hats
    signed char hatValues[4] = { 0, 0, 0, 0 };
    
    for (byte currentHatIndex = 0 ; currentHatIndex < numOfHats ; currentHatIndex++)
    {
      signed char hatValueToSend = 0;

      for (byte currentHatPin = 0 ; currentHatPin < 4 ; currentHatPin++)
      {
        // Check for direction
        if(currentHatStates[currentHatPin + currentHatIndex*4] == LOW)
        {
          hatValueToSend = currentHatPin * 2 + 1;

          // Account for last diagonal
          if(currentHatPin == 0)
          {
            if(currentHatStates[currentHatIndex*4 + 3] == LOW)
            {
              hatValueToSend = 8 ;
              break;
            }
          }
          
          // Account for first 3 diagonals
          if(currentHatPin < 3)
          {
            if(currentHatStates[currentHatPin + currentHatIndex*4 + 1] == LOW)
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
    if (currentButtonStates != previousButtonStates || currentHatStates != previousHatStates)
    {
      for (byte currentIndex = 0 ; currentIndex < numOfButtons ; currentIndex++)
      {
        previousButtonStates[currentIndex] = currentButtonStates[currentIndex]; 
      }

      for (byte currentIndex = 0 ; currentIndex < numOfHats*4 ; currentIndex++)
      {
        previousHatStates[currentIndex] = currentHatStates[currentIndex]; 
      }
      
      bleGamepad.sendReport();
    }
    
    delay(20);
  }
}