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

    // A bonded central that reconnects may never fire onAuthenticationComplete
    // (no new pairing ceremony: the link just re-encrypts with stored keys).
    // Without this latch, `connected` stays false forever after any reboot and
    // sendReport() silently drops every HID report while the serial side keeps
    // replying `ok` - exactly the failure that looks like "joy.cpl is dead".
    if (connInfo.isEncrypted() || connInfo.isBonded())
    {
        NIMBLE_LOGD(LOG_TAG, "onConnect - bonded/encrypted link, marking connected");
        this->authenticatedConnHandles.insert(connInfo.getConnHandle());
        this->connected = true;
    }

    // Keep advertising so additional centrals can connect alongside whichever
    // peer is already connected.
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
