#include "BleFeatureReport.h"

BleFeatureReceiver::BleFeatureReceiver(uint16_t featureReportLength, BleGamepadConfiguration *configuration)
{
    this->featureReportLength = featureReportLength;
    this->configuration = configuration;
    featureBuffer = new uint8_t[featureReportLength];
    memset(featureBuffer, 0, featureReportLength);
}

BleFeatureReceiver::~BleFeatureReceiver()
{
    // Release memory
    if (featureBuffer)
    {
        delete[] featureBuffer;
    }
}

// Builds a capability report modeled on the SInput "Features 0x02" report:
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
    buildFeatureReport();

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