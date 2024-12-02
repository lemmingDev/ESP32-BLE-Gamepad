#include "BleConnectionStatus.h"

BleConnectionStatus::BleConnectionStatus(void)
{
}

void BleConnectionStatus::onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo)
{
    pServer->updateConnParams(connInfo.getConnHandle(), 6, 7, 0, 600);
    this->connected = true;
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason)
{
    this->connected = false;
}
