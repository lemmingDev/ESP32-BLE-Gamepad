#ifndef ESP32_BLE_CONNECTION_STATUS_H
#define ESP32_BLE_CONNECTION_STATUS_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include <Arduino.h>
#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"
#include "NimBLEConnInfo.h"
#include <set>

class BleConnectionStatus : public NimBLEServerCallbacks, public NimBLECharacteristicCallbacks
{
public:
    BleConnectionStatus(void);
    bool connected = false;
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo& connInfo, int reason) override;
    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override;

    // Registered as the characteristic callback for the HID input report, so we can
    // log when a peer subscribes to it - the real signal that it wants the gamepad
    // profile, as opposed to (say) only the NUS service.
    void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override;

    NimBLECharacteristic *inputGamepad;

private:
    std::set<uint16_t> authenticatedConnHandles;
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_CONNECTION_STATUS_H
