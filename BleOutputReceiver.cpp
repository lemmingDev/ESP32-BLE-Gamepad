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

    // Some hosts (e.g. macOS's BLE HID bridge) prepend the Report ID byte to
    // Output Report writes even though the GATT characteristic (and its Report
    // Reference descriptor) already identifies the report; strip it if present.
    const uint8_t* data = (const uint8_t*)value.c_str();
    size_t length = value.length();
    if (length == (size_t)outputReportLength + 1)
    {
        data++;
        length--;
    }

    // Store the received data in the buffer
    for (size_t i = 0; i < std::min(length, (size_t)outputReportLength); i++)
    {
        outputBuffer[i] = data[i];
    }

    outputFlag = true;
}