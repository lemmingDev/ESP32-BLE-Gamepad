#include "BleSInput.h"
#include <string.h>

BleSInputReceiver::BleSInputReceiver(BleGamepadConfiguration *configuration, NimBLECharacteristic *cmdInputReport)
{
    this->configuration = configuration;
    this->cmdInputReport = cmdInputReport;
}

void BleSInputReceiver::sendFeaturesResponse()
{
    uint8_t report[SINPUT_REPORT_LEN_INPUT];
    memset(report, 0, sizeof(report));

    report[0] = SINPUT_COMMAND_FEATURES; // command echo, so the host knows which reply this is

    // SDL doesn't currently gate any behavior on this, but populate it anyway
    // in case a future SDL version does -- see BleSInput.h's note above
    // SINPUT_FEAT_IDX_PROTOCOL_VERSION for why this field exists at all.
    report[SINPUT_FEAT_IDX_PROTOCOL_VERSION] = 1;
    report[SINPUT_FEAT_IDX_PROTOCOL_VERSION + 1] = 0;

    uint8_t caps0 = SINPUT_FEAT_CAP_PLAYERLED; // the only command this library actually acts on today
    if (configuration->getIncludeXAxis() && configuration->getIncludeYAxis())
        caps0 |= SINPUT_FEAT_CAP_LEFT_STICK;
    if (configuration->getIncludeZAxis() && configuration->getIncludeRzAxis())
        caps0 |= SINPUT_FEAT_CAP_RIGHT_STICK;
    if (configuration->getIncludeRxAxis())
        caps0 |= SINPUT_FEAT_CAP_LEFT_TRIGGER;
    if (configuration->getIncludeRyAxis())
        caps0 |= SINPUT_FEAT_CAP_RIGHT_TRIGGER;
    report[SINPUT_FEAT_IDX_CAPS0] = caps0;

    // report[SINPUT_FEAT_IDX_CAPS1] (touchpad/RGB/handheld), TYPE, STYLE, POLL_US,
    // ACCEL_RANGE, GYRO_RANGE all stay 0 -- unsupported/unclassified, and SDL only
    // reads POLL_US/*_RANGE when the corresponding capability bit above is set.

    uint8_t usageMask0 = 0;
    if (configuration->getButtonCount() >= 1) usageMask0 |= SINPUT_BTN0_SOUTH;
    if (configuration->getButtonCount() >= 2) usageMask0 |= SINPUT_BTN0_EAST;
    if (configuration->getButtonCount() >= 3) usageMask0 |= SINPUT_BTN0_WEST;
    if (configuration->getButtonCount() >= 4) usageMask0 |= SINPUT_BTN0_NORTH;
    if (configuration->getHatSwitchCount() >= 1)
        usageMask0 |= SINPUT_BTN0_DUP | SINPUT_BTN0_DDOWN | SINPUT_BTN0_DLEFT | SINPUT_BTN0_DRIGHT;
    report[SINPUT_FEAT_IDX_USAGE_MASK_0] = usageMask0;

    uint8_t usageMask1 = 0;
    if (configuration->getButtonCount() >= 5) usageMask1 |= SINPUT_BTN1_LSHOULDER;
    if (configuration->getButtonCount() >= 6) usageMask1 |= SINPUT_BTN1_RSHOULDER;
    report[SINPUT_FEAT_IDX_USAGE_MASK_1] = usageMask1;

    // report[SINPUT_FEAT_IDX_USAGE_MASK_2] (Start/Back/Guide/...) stays 0 -- this
    // library's special buttons don't have a fixed bit position to map from (their
    // position depends on which ones are enabled, see specialButtonBitPosition() in
    // BleGamepad.cpp), so they aren't represented in SInput's usage mask yet.
    // USAGE_MASK_3 (Power/Misc) and touchpad count/finger-count also stay 0.

    cmdInputReport->setValue(report, sizeof(report));
    cmdInputReport->notify();
}

void BleSInputReceiver::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo)
{
    // Retrieve data sent from the host
    std::string value = pCharacteristic->getValue();

    // Some hosts (e.g. macOS's BLE HID bridge) prepend the Report ID byte to
    // Output Report writes even though the GATT characteristic already
    // identifies the report; strip it if present (same pattern as
    // BleOutputReceiver::onWrite / BleFeatureReceiver::onWrite).
    const uint8_t* data = (const uint8_t*)value.c_str();
    size_t length = value.length();
    if (length == (size_t)SINPUT_REPORT_LEN_OUTPUT + 1)
    {
        data++;
        length--;
    }

    if (length < 1)
    {
        return;
    }

    switch (data[0])
    {
        case SINPUT_COMMAND_FEATURES:
            sendFeaturesResponse();
            break;

        case SINPUT_COMMAND_PLAYERLED:
            if (length >= 2)
            {
                playerLedIndex = data[1];
                playerLedFlag = true;
            }
            break;

        // SINPUT_COMMAND_HAPTIC and SINPUT_COMMAND_JOYSTICKRGB: the write still
        // succeeds (no error returned to the host), but this library doesn't have
        // a rumble motor or RGB LED to drive, so the payload is just dropped.
        default:
            break;
    }
}
