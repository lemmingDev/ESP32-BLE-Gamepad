#include "BleFeatureReport.h"

BleFeatureReceiver::BleFeatureReceiver(uint16_t featureReportLength, BleGamepadConfiguration *configuration)
{
    this->featureReportLength = featureReportLength;
    this->configuration = configuration;
    featureBuffer = new uint8_t[featureReportLength];
    memset(featureBuffer, 0, featureReportLength);
    buildFeatureReport(); // seed the buffer with the capability defaults, once
}

BleFeatureReceiver::~BleFeatureReceiver()
{
    // Release memory
    if (featureBuffer)
    {
        delete[] featureBuffer;
    }
}

// Seeds featureBuffer[0..3] with a capability report modeled on the SInput
// "Features 0x02" layout. Called once at construction so a host that reads the
// Feature Report without anything having written it still sees sane defaults.
// It is NOT re-run on every read -- that would clobber whatever the sketch put
// there with setFeatureBuffer() (the bidirectional buffer in GattVsHid.md B).
// https://docs.handheldlegend.com/s/sinput/doc/features-response-bytes-1lMp7WL7bq
void BleFeatureReceiver::buildFeatureReport()
{
    uint8_t caps = 0;

    if (configuration->getEnableRumble())     caps |= FEAT_CAP_RUMBLE;
    if (configuration->getEnablePlayerLED())  caps |= FEAT_CAP_PLAYERLED;
    if (configuration->getIncludeAccelerometer()) caps |= FEAT_CAP_ACCELEROMETER;
    if (configuration->getIncludeGyroscope())     caps |= FEAT_CAP_GYROSCOPE;
    if (configuration->getIncludeXAxis() || configuration->getIncludeYAxis())   caps |= FEAT_CAP_LEFT_STICK;
    if (configuration->getIncludeZAxis() || configuration->getIncludeRzAxis()) caps |= FEAT_CAP_RIGHT_STICK;
    if (configuration->getIncludeRxAxis()) caps |= FEAT_CAP_LEFT_TRIGGER;
    if (configuration->getIncludeRyAxis()) caps |= FEAT_CAP_RIGHT_TRIGGER;

    if (featureReportLength > 0) featureBuffer[0] = caps;
    if (featureReportLength > 1) featureBuffer[1] = (uint8_t)configuration->getButtonCount();
    if (featureReportLength > 2) featureBuffer[2] = configuration->getAxisCount();
    if (featureReportLength > 3) featureBuffer[3] = configuration->getHatSwitchCount();
}

void BleFeatureReceiver::onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo)
{
    // Serve whatever is in the buffer -- the capability defaults from the
    // constructor, or whatever setFeatureBuffer() / a host onWrite last stored.
    pCharacteristic->setValue(featureBuffer, featureReportLength);
}

void BleFeatureReceiver::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo)
{
    // Retrieve data sent from the host
    std::string value = pCharacteristic->getValue();

    // Some hosts (e.g. macOS's BLE HID bridge) prepend the Report ID byte to
    // Feature Report writes even though the GATT characteristic (and its Report
    // Reference descriptor) already identifies the report; strip it if present.
    const uint8_t* data = (const uint8_t*)value.c_str();
    size_t length = value.length();
    if (length == (size_t)featureReportLength + 1)
    {
        data++;
        length--;
    }

    if (length <= featureReportLength && memcmp(data, featureBuffer, length) != 0)
    {
        memcpy(featureBuffer, data, length);
    }

    featureFlag = true;
}