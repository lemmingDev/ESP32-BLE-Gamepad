#ifndef BLE_OUTPUT_RECEIVER_H
#define BLE_OUTPUT_RECEIVER_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"
#include "NimBLEConnInfo.h"

class BleOutputReceiver : public NimBLECharacteristicCallbacks
{
public:
    BleOutputReceiver(uint16_t outputReportLength);
    ~BleOutputReceiver();
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override;
    bool outputFlag = false;
    uint16_t outputReportLength;
    uint8_t *outputBuffer;
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // BLE_OUTPUT_RECEIVER_H
