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

    featureFlag = true;
}

void BleFeatureReceiver::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo)
{
    // Retrieve data sent from the host
    std::string value = pCharacteristic->getValue();

    if (value.length() <= featureReportLength) {
        // Check if data has changed
        if (memcmp(value.c_str(), featureBuffer, value.length()) != 0) {
            memcpy(featureBuffer, value.c_str(), value.length());
            // If there's a callback defined, call it
            //if (_onFeatureReportReceived) {
            //    _onFeatureReportReceived((const uint8_t*)value.c_str(), value.length());
            //}
        }
    }

    /*

    // Store the received data in the buffer
    for (int i = 0; i < std::min(value.length(), (size_t)featureReportLength); i++)
    {
        featureBuffer[i] = (uint8_t)value[i];
    }

    */

    // Testing
    // Serial.println("Received data from host:");
    // for (size_t i = 0; i < value.length(); i++) {
    //     Serial.print((uint8_t)value[i], HEX);
    //     Serial.print(" ");
    // }
    // Serial.println();

    featureFlag = true;
}