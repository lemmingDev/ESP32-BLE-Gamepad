/*
 * Flight controller test
*/

#include <BleGamepad.h> 

#define numOfButtons        32
#define numOfHatSwitches    0
#define enableX             true
#define enableY             true
#define enableZ             false
#define enableRZ            false
#define enableRX            false
#define enableRY            false
#define enableSlider1       false
#define enableSlider2       false
#define enableRudder        true
#define enableThrottle      true
#define enableAccelerator   false
#define enableBrake         false
#define enableSteering      false

BleGamepad bleGamepad("BLE Flight Controller", "lemmingDev", 100);

void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  
  //Setup controller with 10 buttons, accelerator, brake and steering
  bleGamepad.setAutoReport(false);
  bleGamepad.setControllerType(CONTROLLER_TYPE_MULTI_AXIS);  //CONTROLLER_TYPE_JOYSTICK, CONTROLLER_TYPE_GAMEPAD (DEFAULT), CONTROLLER_TYPE_MULTI_AXIS
  bleGamepad.begin(numOfButtons,numOfHatSwitches,enableX,enableY,enableZ,enableRZ,enableRX,enableRY,enableSlider1,enableSlider2,enableRudder,enableThrottle,enableAccelerator,enableBrake,enableSteering);
  
  //Set throttle to min
  bleGamepad.setThrottle(-32767);
  
  //Set x and y axes and rudder to center
  bleGamepad.setX(0);
  bleGamepad.setY(0);
  bleGamepad.setRudder(0);
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

    Serial.println("Move x axis from center to max");
    for(int i = 0 ; i > -32767 ; i -= 256)
    {
      bleGamepad.setX(i);
      bleGamepad.sendReport();
      delay(10);
    }
  
    Serial.println("Move x axis from min to max");
    for(int i = -32767 ; i < 32767 ; i += 256)
    {
      bleGamepad.setX(i);
      bleGamepad.sendReport();
      delay(10);
    }
  
    Serial.println("Move x axis from max to center");
    for(int i = 32767 ; i > 0 ; i -= 256)
    {
      bleGamepad.setX(i);
      bleGamepad.sendReport();
      delay(10);
    }
    bleGamepad.setX(0);
    bleGamepad.sendReport();
	
	
	Serial.println("Move y axis from center to max");
    for(int i = 0 ; i > -32767 ; i -= 256)
    {
      bleGamepad.setY(i);
      bleGamepad.sendReport();
      delay(10);
    }
  
    Serial.println("Move y axis from min to max");
    for(int i = -32767 ; i < 32767 ; i += 256)
    {
      bleGamepad.setY(i);
      bleGamepad.sendReport();
      delay(10);
    }
  
    Serial.println("Move y axis from max to center");
    for(int i = 32767 ; i > 0 ; i -= 256)
    {
      bleGamepad.setY(i);
      bleGamepad.sendReport();
      delay(10);
    }
    bleGamepad.setY(0);
    bleGamepad.sendReport();
	

    Serial.println("Move rudder from min to max");
    //for(int i = 32767 ; i > -32767 ; i -= 256)    //Use this for loop setup instead if rudder is reversed
    for(int i = -32767 ; i < 32767 ; i += 256)
    {
      bleGamepad.setRudder(i);
      bleGamepad.sendReport();
      delay(10);
    }
    bleGamepad.setRudder(0);
    bleGamepad.sendReport();


    Serial.println("Move throttle from min to max");
    for(int i = -32767 ; i < 32767 ; i += 256)
    {
      bleGamepad.setThrottle(i);
      bleGamepad.sendReport();
      delay(10);
    }
    bleGamepad.setThrottle(-32767);
    bleGamepad.sendReport();
  }
}