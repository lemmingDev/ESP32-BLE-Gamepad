/*
 * This code programs a number of pins on an ESP32 as buttons on a BLE gamepad
 * 
 * It uses arrays to cut down on code
 *
 * Uses the Bounce2 library to debounce all buttons
 * 
 * Uses the rose/fell states of the Bounce instance to track button states
 *
 * Before using, adjust the numOfButtons, buttonPins and physicalButtons to suit your senario
 *
 */

#define BOUNCE_WITH_PROMPT_DETECTION    // Make button state changes available immediately

#include <Bounce2.h>      // https://github.com/thomasfredericks/Bounce2
#include <BleGamepad.h>   // https://github.com/lemmingDev/ESP32-BLE-Gamepad


#define numOfButtons 10

Bounce debouncers[numOfButtons];
BleGamepad bleGamepad;

byte buttonPins[numOfButtons] = { 0, 35, 17, 18, 19, 23, 25, 26, 27, 32 };
byte physicalButtons[numOfButtons] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

void setup()
{
  for (byte currentPinIndex = 0 ; currentPinIndex < numOfButtons ; currentPinIndex++)
  {
    pinMode(buttonPins[currentPinIndex], INPUT_PULLUP);
    
    debouncers[currentPinIndex] = Bounce();
    debouncers[currentPinIndex].attach(buttonPins[currentPinIndex]);      // After setting up the button, setup the Bounce instance :
    debouncers[currentPinIndex].interval(5);        
  }

  bleGamepad.begin(numOfButtons);
  bleGamepad.setAutoReport(false);
  Serial.begin(115200);
}

void loop()
{
  if(bleGamepad.isConnected())
  {
    bool sendReport = false;
    
    for (byte currentIndex = 0 ; currentIndex < numOfButtons ; currentIndex++)
    {
      debouncers[currentIndex].update();

      if (debouncers[currentIndex].fell())
      {
        bleGamepad.press(physicalButtons[currentIndex]);
        sendReport = true;
        Serial.print("Button ");
        Serial.print(physicalButtons[currentIndex]);
        Serial.println(" pushed.");
      }
      else if (debouncers[currentIndex].rose())
      {
        bleGamepad.release(physicalButtons[currentIndex]);
        sendReport = true;
        Serial.print("Button ");
        Serial.print(physicalButtons[currentIndex]);
        Serial.println(" released.");
      }
    } 
    
    if(sendReport)
    {
      bleGamepad.sendReport();
    }
    
    //delay(20);	// (Un)comment to remove/add delay between loops 
  }
}
