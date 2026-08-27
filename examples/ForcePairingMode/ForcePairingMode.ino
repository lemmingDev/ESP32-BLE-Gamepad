/*
 * A simple sketch that, upon a button press, disconnects from an aggressive client 
 * until a new client connects to the gamepad
 * 
 * If it finds another client that's already paired, it will connect to that
 * 
 * If it doesn't find another paired device, it will allow you to pair a new one
 * 
 * If you want a more permanent solution, instead use bleGamepad.deleteBond() 
 * which will delete the bond for the currently connected client and allow other
 * clients to connect to it
 * 
 * Use bleGamepad.deletAllBonds() to delete all bonds from the gamepad
 * 
 * After deleting bonds, it is best to unpair them from the client device such
 * as your phone or PC otherwise the gamepad may briefly connect while searching
 *
 * The deleteBond and deletAllBonds functions can optionally reset the gamepad with
 * deletAllBonds(true) or deleteBond(true), although it shouldn't be needed
 * as the advertising should now start again after a client is disconnected
 * 
 * They all return a boolean for success or failure if wanted
 */

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

#define DISCONNECTPIN 0 // Pin disconnect button is attached to

BleGamepad bleGamepad;

void setup()
{
    Serial.begin(115200);
    pinMode(DISCONNECTPIN, INPUT_PULLUP);
    bleGamepad.begin();
}

void loop()
{
    if (bleGamepad.isConnected())
    {
        // Enter forced pairing mode 
        // It repeatedly disconnects from currently connected device until a new device is paired
        // Returns true if a new device is connected
        // For now, enters an endless loop if no new device found
        // Simply reset device to have it revert to previous behaviour
        if (digitalRead(DISCONNECTPIN) == LOW) // On my board, pin 0 is LOW for pressed
        {   
            bool pairingResult = bleGamepad.enterPairingMode();

            if(pairingResult)
            {
              Serial.println("New device paired successfully"); 
            }
            else
            {
              Serial.println("No new device paired");
            }
        }
    }
    else
    {
        Serial.println("No device connected");
        delay(1000);
    }
}
