#include "BleConnectionStatus.h"
#include <NimBLEDevice.h>

BleConnectionStatus::BleConnectionStatus(void)
{
}

void BleConnectionStatus::onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo)
{
    pServer->updateConnParams(connInfo.getConnHandle(), 6, 7, 0, 600);
	
    //NimBLEAddress addr = connInfo.getAddress();
    //Serial.print("onConnect - Connected from: ");
    //Serial.println(addr.toString().c_str());
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason)
{
    //NimBLEAddress addr = connInfo.getAddress();
    //Serial.print("onDisconnect - Disconnected from: ");
    //Serial.println(addr.toString().c_str());
	
    this->connected = false;
    delay(500);
    NimBLEDevice::startAdvertising();  // Restart advertising
}

void BleConnectionStatus::onAuthenticationComplete(NimBLEConnInfo& connInfo)
{
    //NimBLEAddress addr = connInfo.getAddress();
    //Serial.print("onAuthenticationComplete - Connected from: ");
    //Serial.println(addr.toString().c_str());
	
    this->connected = true;
}
