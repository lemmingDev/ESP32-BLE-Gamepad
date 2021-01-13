#include "BleConnectionStatus.h"

BleConnectionStatus::BleConnectionStatus(void) {
}

void BleConnectionStatus::onConnect(NimBLEServer* pServer)
{
  this->connected = true;
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer)
{
  this->connected = false;
}
