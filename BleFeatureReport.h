#ifndef BLE_FEATURE_REPORT_H
#define BLE_FEATURE_REPORT_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"
#include "NimBLEConnInfo.h"

class BleFeatureReceiver : public NimBLECharacteristicCallbacks
{
public:
    BleFeatureReceiver(uint16_t featureReportLength);
    ~BleFeatureReceiver();
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override;
    bool featureFlag = false;
    uint16_t featureReportLength;
    uint8_t *featureBuffer;
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // BLE_FEATURE_REPORT_H
