#ifndef ESP32_BLE_CONNECTION_STATUS_H
#define ESP32_BLE_CONNECTION_STATUS_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#if defined(USE_NIMBLE)
#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"

class BleConnectionStatus : public NimBLEServerCallbacks
{
public:
    BleConnectionStatus(void);
    bool connected = false;
    void onConnect(NimBLEServer *pServer, ble_gap_conn_desc* desc);
    void onDisconnect(NimBLEServer *pServer);
    NimBLECharacteristic *inputGamepad;
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL

#else  // USE_NIMBLE
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLECharacteristic.h"

class BleConnectionStatus : public BLEServerCallbacks
{
public:
  BleConnectionStatus(void);
  bool connected = false;
  void onConnect(BLEServer* pServer);
  void onDisconnect(BLEServer* pServer);
  BLECharacteristic *inputGamepad;
};

#endif // USE_NIMBLE
#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_CONNECTION_STATUS_H
