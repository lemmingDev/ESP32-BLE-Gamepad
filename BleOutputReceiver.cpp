#include "BleOutputReceiver.h"

BleOutputReceiver::BleOutputReceiver(uint16_t outputReportLength)
{
    this->outputReportLength = outputReportLength;
    outputBuffer = new uint8_t[outputReportLength];
}

BleOutputReceiver::~BleOutputReceiver()
{
    // Release memory
    if (outputBuffer)
    {
        delete[] outputBuffer;
    }
}

void BleOutputReceiver::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo)
{
    // Retrieve data sent from the host
    std::string value = pCharacteristic->getValue();

    // Store the received data in the buffer
    for (int i = 0; i < std::min(value.length(), (size_t)outputReportLength); i++)
    {
        outputBuffer[i] = (uint8_t)value[i];
    }

    // Testing
    // Serial.println("Received data from host:");
    // for (size_t i = 0; i < value.length(); i++) {
    //     Serial.print((uint8_t)value[i], HEX);
    //     Serial.print(" ");
    // }
    // Serial.println();

    outputFlag = true;
}