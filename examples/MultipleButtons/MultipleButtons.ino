/*
 * This code programs a number of pins on an ESP32 as buttons on a BLE gamepad
 * 
 * It uses arrays to cut down on code
 *
 * Before using, adjust the numOfButtons, buttonPins and physicalButtons to suit your senario
 *
 */
 
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

BleGamepad bleGamepad;

#define numOfButtons 10

byte previousButtonStates[numOfButtons];
byte currentButtonStates[numOfButtons];
byte buttonPins[numOfButtons] = { 4, 16, 17, 18, 19, 23, 25, 26, 27, 32 };
byte physicalButtons[numOfButtons] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

void setup()
{
  for (byte currentPinIndex = 0 ; currentPinIndex < numOfButtons ; currentPinIndex++)
  {
    pinMode(buttonPins[currentPinIndex], INPUT_PULLUP);
    previousButtonStates[currentPinIndex] = HIGH;
    currentButtonStates[currentPinIndex] =  HIGH;
  }

  bleGamepad.begin(false);
  Serial.begin(115200);
}

void loop()
{
  if(bleGamepad.isConnected())
  {
    for (byte currentIndex = 0 ; currentIndex < numOfButtons ; currentIndex++)
    {
      currentButtonStates[currentIndex]  = digitalRead(buttonPins[currentIndex]);

      if (currentButtonStates[currentIndex] != previousButtonStates[currentIndex])
      {
        if(currentButtonStates[currentIndex] == LOW)
        {
          bleGamepad.press(pow(2, physicalButtons[currentIndex] - 1));
          Serial.print("Button ");
          Serial.print(physicalButtons[currentIndex]);
          Serial.println(" pushed.");
        }
        else
        {
          bleGamepad.release(pow(2, physicalButtons[currentIndex] - 1));
          Serial.print("Button ");
          Serial.print(physicalButtons[currentIndex]);
          Serial.println(" released.");
        }
      } 
    }
    
    if (currentButtonStates != previousButtonStates)
    {
      for (byte currentIndex = 0 ; currentIndex < numOfButtons ; currentIndex++)
      {
        previousButtonStates[currentIndex] = currentButtonStates[currentIndex]; 
      }
      
      bleGamepad.sendReport();
    }
    
    delay(20);
  }
}