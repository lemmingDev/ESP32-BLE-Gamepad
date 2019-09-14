/*
 * A simple sketch that maps a single pin on the ESP32 to a single button on the controller
 */

#include <BleGamepad.h> 

BleGamepad bleGamepad;

int previousButton1State = HIGH;

void setup() {
  pinMode(2, INPUT_PULLUP);
  bleGamepad.begin();
}

void loop() {
  if(bleGamepad.isConnected()) {

    int currentButton1State = digitalRead(2);

    if (currentButton1State != previousButton1State)
    {
      if(currentButton1State == LOW)
      {
        bleGamepad.press(BUTTON_1);
      }
      else
      {
        bleGamepad.release(BUTTON_1);
      }
    }
    previousButton1State = currentButton1State;
  }
}