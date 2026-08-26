#include "BleFeatureReport.h"

BleFeatureReceiver::BleFeatureReceiver(uint16_t featureReportLength)
{
    this->featureReportLength = featureReportLength;
    featureBuffer = new uint8_t[featureReportLength];
}

BleFeatureReceiver::~BleFeatureReceiver()
{
    // Release memory
    if (featureBuffer)
    {
        delete[] featureBuffer;
    }
}

void BleFeatureReceiver::onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo)
{
    // Set data for the host
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