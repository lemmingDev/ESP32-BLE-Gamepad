/*
 * Shows how to use a 4 x 4 keypad, commonly seen in Arduino starter kits, with the library
 * https://www.aliexpress.com/item/32879638645.html
 * It maps the 16 buttons to the first 16 buttons of the controller
 * Only certain combinations work for multiple presses over 2 buttons
 */

#include <Keypad.h>       // https://github.com/Chris--A/Keypad
#include <BleGamepad.h>   // https://github.com/lemmingDev/ESP32-BLE-Gamepad

BleGamepad bleGamepad("ESP32 Keypad", "lemmingDev", 100);	//Shows how you can customise the device name, manufacturer name and initial battery level

#define ROWS 4
#define COLS 4
uint8_t rowPins[ROWS] = {2, 13, 14, 25};    //ESP32 pins used for rows      --> adjust to suit --> Pinout on board: R1, R2, R3, R4
uint8_t colPins[COLS] = {4, 23, 21, 22};    //ESP32 pins used for columns   --> adjust to suit --> Pinout on board: Q1, Q2, Q3, Q4
uint8_t keymap[ROWS][COLS] = 
{
  {1, 2, 3, 4},      //Buttons  1,  2,  3,  4      --> Used for calulating the bitmask for sending to the library 
  {5, 6, 7, 8},      //Buttons  5,  6,  7,  8      --> Adjust to suit which buttons you want the library to send
  {9, 10,11,12},     //Buttons  9, 10, 11, 12      --> 
  {13,14,15,16}      //Buttons 13, 14, 15, 16      --> Eg. The value 12 in the array refers to button 12
};

Keypad customKeypad = Keypad( makeKeymap(keymap), rowPins, colPins, ROWS, COLS);

void KeypadUpdate()
{
  customKeypad.getKeys();
  
  for (int i=0; i<LIST_MAX; i++)   // Scan the whole key list.      //LIST_MAX is provided by the Keypad library and gives the number of buttons of the Keypad instance
  {
      if ( customKeypad.key[i].stateChanged )   //Only find keys that have changed state.
      {
        uint8_t keystate = customKeypad.key[i].kstate;

        if(bleGamepad.isConnected()) 
        {
          if (keystate==PRESSED) {  bleGamepad.press(customKeypad.key[i].kchar); }    //Press or release button based on the current state
          if (keystate==RELEASED) { bleGamepad.release(customKeypad.key[i].kchar); }
        
          bleGamepad.sendReport();    //Send the HID report after values for all button states are updated, and at least one button state had changed
        }
      }
  }
}

void setup() 
{
  bleGamepad.begin();				//Begin library with default buttons/hats/axes
  bleGamepad.setAutoReport(false);	//Disable auto reports --> You then need to force HID updates with bleGamepad.sendReport()
}

void loop() 
{
  KeypadUpdate();
  delay(10);
}