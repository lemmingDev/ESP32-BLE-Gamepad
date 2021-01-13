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
    for(int i = 0 ; i < 64 ; i += 1)
    {
      bleGamepad.press(pow(2, i));
      delay(100);
      bleGamepad.release(pow(2, i));
      delay(25);
    }

    Serial.println("Move all axis simultaneously from min to max");
    for(int i = -127 ; i < 128 ; i += 1)
    {
      bleGamepad.setAxes(i*256, i*256, i*256, i*256, (i+127) * 256 , (i+127) * 256);
      delay(10);
    }
    bleGamepad.setAxes(); //Reset all axes to zero

    Serial.println("Move all sliders simultaneously from min to max");
    for(int i = 0 ; i < 255 ; i += 1)
    {
      bleGamepad.setSliders(i*256, i*256);
      delay(10);
    }
    bleGamepad.setSliders(); //Reset all sliders to zero

    Serial.println("Send hat switch 1 one by one in an anticlockwise rotation");
    for(int i = 8 ; i>= 0 ; i--)
    {
      bleGamepad.setHat1(i);
      delay(200);
    }

    Serial.println("Send hat switch 2 one by one in an anticlockwise rotation");
    for(int i = 8 ; i>= 0 ; i--)
    {
      bleGamepad.setHat2(i);
      delay(200);
    }

    Serial.println("Send hat switch 3 one by one in an anticlockwise rotation");
    for(int i = 8 ; i>= 0 ; i--)
    {
      bleGamepad.setHat3(i);
      delay(200);
    }

    Serial.println("Send hat switch 4 one by one in an anticlockwise rotation");
    for(int i = 8 ; i>= 0 ; i--)
    {
      bleGamepad.setHat4(i);
      delay(200);
    }
  }
}