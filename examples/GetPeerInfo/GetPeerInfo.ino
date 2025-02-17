/*
   A simple sketch that, upon a button press, shows peer info

   bleGamepad.getPeerInfo(); returns type NimBLEConnInfo
   From there, you have access to all info here https://h2zero.github.io/NimBLE-Arduino/class_nim_b_l_e_conn_info.html

   If you just need the MAC address, you can instead call bleGamepad.getAddress() which returns type NimBLEAddress
   or bleGamepad.getStringAddress() to get it directly as a string

   This sketch also shows how to access the information used to configure the BLE device such as vid, pid, model number and software revision etc
   See CharacteristicsConfiguration.ino example to see how to set them
*/

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

#define PEER_INFO_PIN 0 // Pin button is attached to

BleGamepad bleGamepad;

void setup()
{
  Serial.begin(115200);
  pinMode(PEER_INFO_PIN, INPUT_PULLUP);
  bleGamepad.begin();
}

void loop()
{
  if (bleGamepad.isConnected())
  {

    if (digitalRead(PEER_INFO_PIN) == LOW) // On my board, pin 0 is LOW for pressed
    {
      Serial.println("\n----- OUPTPUT PEER INFORMATION -----\n");

      // Get the HEX address as a string;
      Serial.println(bleGamepad.getStringAddress());

      // Get the HEX address as an NimBLEAddress instance
      NimBLEAddress bleAddress = bleGamepad.getAddress();
      Serial.println(bleAddress.toString().c_str());

      // Get values directly from an NimBLEConnInfo instance
      NimBLEConnInfo peerInfo = bleGamepad.getPeerInfo();

      Serial.println(peerInfo.getAddress().toString().c_str());      // NimBLEAddress
      Serial.println(peerInfo.getIdAddress().toString().c_str());    // NimBLEAddress
      Serial.println(peerInfo.getConnHandle());                      // uint16_t
      Serial.println(peerInfo.getConnInterval());                    // uint16_t
      Serial.println(peerInfo.getConnTimeout());                     // uint16_t
      Serial.println(peerInfo.getConnLatency());                     // uint16_t
      Serial.println(peerInfo.getMTU());                             // uint16_t
      Serial.println(peerInfo.isMaster());                           // bool
      Serial.println(peerInfo.isSlave());                            // bool
      Serial.println(peerInfo.isBonded());                           // bool
      Serial.println(peerInfo.isEncrypted());                        // bool
      Serial.println(peerInfo.isAuthenticated());                    // bool
      Serial.println(peerInfo.getSecKeySize());                      // uint8_t

      Serial.println("\n----- OUPTPUT CONFIGURATION INFORMATION -----\n");
      Serial.println(bleGamepad.getDeviceName());
      Serial.println(bleGamepad.getDeviceManufacturer());
      Serial.println(bleGamepad.configuration.getModelNumber());
      Serial.println(bleGamepad.configuration.getSoftwareRevision());
      Serial.println(bleGamepad.configuration.getSerialNumber());
      Serial.println(bleGamepad.configuration.getFirmwareRevision());
      Serial.println(bleGamepad.configuration.getHardwareRevision());
      Serial.println(bleGamepad.configuration.getVid(), HEX);
      Serial.println(bleGamepad.configuration.getPid(), HEX);
      Serial.println(bleGamepad.configuration.getGuidVersion());
      Serial.println(bleGamepad.configuration.getTXPowerLevel());
      Serial.println();
      delay(1000);
    }
  }
  else
  {
    Serial.println("No device connected");
    delay(1000);
  }
}