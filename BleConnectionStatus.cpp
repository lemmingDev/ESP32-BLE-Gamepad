#include "BleConnectionStatus.h"

BleConnectionStatus::BleConnectionStatus(void)
{
}

#if defined(USE_NIMBLE)
void BleConnectionStatus::onConnect(NimBLEServer *pServer, ble_gap_conn_desc* desc)
{
    pServer->updateConnParams(desc->conn_handle, 6, 7, 0, 600);
    this->connected = true;
}

void BleConnectionStatus::onDisconnect(NimBLEServer *pServer)
{
    this->connected = false;
}
#else  // USE_NIMBLE
void BleConnectionStatus::onConnect(BLEServer* pServer)
{
  this->connected = true;
  BLE2902* desc = (BLE2902*)this->inputGamepad->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
  desc->setNotifications(true);
}

void BleConnectionStatus::onDisconnect(BLEServer* pServer)
{
  this->connected = false;
  BLE2902* desc = (BLE2902*)this->inputGamepad->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
  desc->setNotifications(false);
}
#endif // USE_NIMBLE
