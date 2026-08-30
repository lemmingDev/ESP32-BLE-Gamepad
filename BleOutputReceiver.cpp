#include "BleOutputReceiver.h"
#include "BleReportUtils.h"
#include <algorithm>

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

    const uint8_t* raw = (const uint8_t*)value.c_str();
    size_t length = 0;
    const uint8_t* data = stripReportIdIfPresent(raw, value.length(), outputReportLength, length);

    // Store the received data in the buffer
    for (size_t i = 0; i < std::min(length, (size_t)outputReportLength); i++)
    {
        outputBuffer[i] = data[i];
    }

    outputFlag = true;
}