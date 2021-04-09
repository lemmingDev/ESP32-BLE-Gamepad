/*
 * Driving controller test
*/

#include <BleGamepad.h> 

#define numOfButtons        10
#define numOfHatSwitches    0
#define enableX             false
#define enableY             false
#define enableZ             false
#define enableRZ            false
#define enableRX            false
#define enableRY            false
#define enableSlider1       false
#define enableSlider2       false
#define enableRudder        false
#define enableThrottle      false
#define enableAccelerator   true
#define enableBrake         true
#define enableSteering      true

BleGamepad bleGamepad("BLE Driving Controller", "lemmingDev", 100);

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  
  //Setup controller with 10 buttons, accelerator, brake and steering
  bleGamepad.setAutoReport(false);
  bleGamepad.setControllerType(CONTROLLER_TYPE_GAMEPAD);  //CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  bleGamepad.begin(numOfButtons,numOfHatSwitches,enableX,enableY,enableZ,enableRZ,enableRX,enableRY,enableSlider1,enableSlider2,enableRudder,enableThrottle,enableAccelerator,enableBrake,enableSteering);
  
  //Set accelerator and brake to min
  bleGamepad.setAccelerator(-32767);
  bleGamepad.setBrake(-32767);
  
  //Set steering to center
  bleGamepad.setSteering(0);
}

void loop() 
{
  if(bleGamepad.isConnected()) 
  {    
    Serial.println("Press all buttons one by one");
    for(int i = 1 ; i <= numOfButtons ; i += 1)
    {
      bleGamepad.press(i);
      bleGamepad.sendReport();
      delay(100);
      bleGamepad.release(i);
      bleGamepad.sendReport();
      delay(25);
    }

    Serial.println("Move steering from center to max");
    for(int i = 0 ; i > -32767 ; i -= 256)
    {
      bleGamepad.setSteering(i);
      bleGamepad.sendReport();
      delay(10);
    }
  
    Serial.println("Move steering from min to max");
    for(int i = -32767 ; i < 32767 ; i += 256)
    {
      bleGamepad.setSteering(i);
      bleGamepad.sendReport();
      delay(10);
    }
  
    Serial.println("Move steering from max to center");
    for(int i = 32767 ; i > 0 ; i -= 256)
    {
      bleGamepad.setSteering(i);
      bleGamepad.sendReport();
      delay(10);
    }
    bleGamepad.setSteering(0);
    bleGamepad.sendReport();

    Serial.println("Move accelerator from min to max");
    //for(int i = 32767 ; i > -32767 ; i -= 256)    //Use this for loop setup instead if accelerator is reversed
    for(int i = -32767 ; i < 32767 ; i += 256)
    {
      bleGamepad.setAccelerator(i);
      bleGamepad.sendReport();
      delay(10);
    }
    bleGamepad.setAccelerator(-32767);
    bleGamepad.sendReport();


    Serial.println("Move brake from min to max");
    for(int i = -32767 ; i < 32767 ; i += 256)
    {
      bleGamepad.setBrake(i);
      bleGamepad.sendReport();
      delay(10);
    }
    bleGamepad.setBrake(-32767);
    bleGamepad.sendReport();
  }
}