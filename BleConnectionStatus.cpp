#include "BleConnectionStatus.h"
#include "NimBLELog.h"
#include "NimBLEAdvertising.h"

static const char* LOG_TAG = "BleConnectionStatus";

BleConnectionStatus::BleConnectionStatus(void)
{
}

void BleConnectionStatus::onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo)
{
    NIMBLE_LOGD(LOG_TAG, "onConnect - Connected Address: %s", std::string(connInfo.getAddress()).c_str());
    pServer->updateConnParams(connInfo.getConnHandle(), 6, 7, 0, 600);

    // Keep advertising so additional centrals (e.g. a diagnostics client on the
    // NUS service) can connect alongside whichever peer is already connected.
    if (pServer->getConnectedCount() < CONFIG_BT_NIMBLE_MAX_CONNECTIONS)
    {
        NIMBLE_LOGD(LOG_TAG, "onConnect - Restarting advertising to allow additional connections");
        pServer->getAdvertising()->start();
    }
}

void BleConnectionStatus::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason)
{
    NIMBLE_LOGD(LOG_TAG, "onDisconnectConnect - Disconnected Address: %s", std::string(connInfo.getAddress()).c_str());
    this->authenticatedConnHandles.erase(connInfo.getConnHandle());
    this->connected = !this->authenticatedConnHandles.empty();
}

void BleConnectionStatus::onAuthenticationComplete(NimBLEConnInfo& connInfo)
{
    NIMBLE_LOGD(LOG_TAG, "onAuthenticationComplete - Authenticated Address: %s", std::string(connInfo.getAddress()).c_str());
    this->authenticatedConnHandles.insert(connInfo.getConnHandle());
    this->connected = true;
}

void BleConnectionStatus::onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue)
{
    NIMBLE_LOGD(LOG_TAG, "onSubscribe: subValue=%d", subValue);
}
