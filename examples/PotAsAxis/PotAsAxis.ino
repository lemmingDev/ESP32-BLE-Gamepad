/*
 * Reads a potentiometer on pin 34 and maps the reading to the X axis
 * 
 * Potentiometers can be noisy, so the sketch can take multiple samples to average out the readings
 */

#include <BleGamepad.h> 

BleGamepad bleGamepad;

const int potPin = 34;                // Potentiometer is connected to GPIO 34 (Analog ADC1_CH6) 
const int numberOfPotSamples = 5;     // Number of pot samples to take (to smooth the values)
const int delayBetweenSamples = 4;    // Delay in milliseconds between pot samples
const int delayBetweenHIDReports = 5; // Additional delay in milliseconds between HID reports

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
    int potValues[numberOfPotSamples];  // Array to store pot readings
    int potValue = 0;   // Variable to store calculated pot reading average

    // Populate readings
    for(int i = 0 ; i < numberOfPotSamples ; i++)
    {
      potValues[i] = analogRead(potPin);
      potValue += potValues[i];
      delay(delayBetweenSamples);
    }

    // Calculate the average
    potValue = potValue / numberOfPotSamples;

    // Map analog reading from 0 ~ 4095 to 32737 ~ -32737 for use as an axis reading
    int adjustedValue = map(potValue, 0, 4095, 32737, -32737);

    // Update X axis and auto-send report
    bleGamepad.setX(adjustedValue);
    delay(delayBetweenHIDReports);
   
    // The code below (apart from the 2 closing braces) is for pot value degugging, and can be removed
    // Print readings to serial port
    Serial.print("Sent: ");
    Serial.print(adjustedValue);
    Serial.print("\tRaw Avg: ");
    Serial.print(potValue);
    Serial.print("\tRaw: {");

    // Iterate through raw pot values, printing them to the serial port
    for(int i = 0 ; i < numberOfPotSamples ; i++)
    {
      Serial.print(potValues[i]);

      // Format the values into a comma seperated list
      if(i == numberOfPotSamples-1)
      {
        Serial.println("}");
      }
      else
      {
        Serial.print(", ");  
      }
    }    
  }
}