#ifndef BLE_FEATURE_REPORT_H
#define BLE_FEATURE_REPORT_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"
#include "NimBLEConnInfo.h"
#include "BleGamepadConfiguration.h"

#define FEATURE_REPORT_ID 0x03
#define FEATURE_REPORT_LEN 4

// Capability bitmask (byte 0), bit positions match the SInput "Features 0x02"
// capability byte: https://docs.handheldlegend.com/s/sinput/doc/features-response-bytes-1lMp7WL7bq
#define FEAT_CAP_RUMBLE        (1 << 0)
#define FEAT_CAP_PLAYERLED     (1 << 1)
#define FEAT_CAP_ACCELEROMETER (1 << 2)
#define FEAT_CAP_GYROSCOPE     (1 << 3)
#define FEAT_CAP_LEFT_STICK    (1 << 4)
#define FEAT_CAP_RIGHT_STICK   (1 << 5)
#define FEAT_CAP_LEFT_TRIGGER  (1 << 6)
#define FEAT_CAP_RIGHT_TRIGGER (1 << 7)

class BleFeatureReceiver : public NimBLECharacteristicCallbacks
{
public:
    BleFeatureReceiver(uint16_t featureReportLength, BleGamepadConfiguration *configuration);
    ~BleFeatureReceiver();
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override;
    bool featureFlag = false;
    uint16_t featureReportLength;
    uint8_t *featureBuffer;

private:
    BleGamepadConfiguration *configuration;
    void buildFeatureReport();
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // BLE_FEATURE_REPORT_H
