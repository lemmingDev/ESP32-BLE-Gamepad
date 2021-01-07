/*
 * Test all gamepad buttons, axes and dpad 
*/

#include <BleGamepad.h> 

BleGamepad bleGamepad;

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleGamepad.begin();
}

void loop() 
{
  if(bleGamepad.isConnected()) 
  {    
    Serial.println("Press all buttons one by one");
    for(int i = 0 ; i < 32 ; i += 1)
    {
      bleGamepad.press(pow(2, i));
      delay(200);
      bleGamepad.release(pow(2, i));
      delay(50);
    }

    Serial.println("Move all axis simultaneously from min to max");
    for(int i = -127 ; i < 128 ; i += 1)
    {
      bleGamepad.setAxes(i*256, i*256, i*256, i*256, i+127, i+127, 0);
      delay(50);
    }

    Serial.println("Send all dpad one by one in an anticlockwise rotation");
    for(int i = 8 ; i>= 0 ; i--)
    {
      bleGamepad.setAxes(0,0,0,0,0,0, i);
      delay(500);
    }
  }
}