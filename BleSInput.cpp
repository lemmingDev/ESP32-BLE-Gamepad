#include "BleSInput.h"
#include "BleReportUtils.h"
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

    uint8_t caps0 = SINPUT_FEAT_CAP_PLAYERLED;
    if (configuration->getEnableRumble())
        caps0 |= SINPUT_FEAT_CAP_RUMBLE;
    if (configuration->getEnableSInputIMU())
    {
        caps0 |= SINPUT_FEAT_CAP_ACCELEROMETER;
        caps0 |= SINPUT_FEAT_CAP_GYROSCOPE;
    }
    if (configuration->getIncludeXAxis() && configuration->getIncludeYAxis())
        caps0 |= SINPUT_FEAT_CAP_LEFT_STICK;
    if (configuration->getIncludeZAxis() && configuration->getIncludeRzAxis())
        caps0 |= SINPUT_FEAT_CAP_RIGHT_STICK;
    if (configuration->getIncludeRxAxis())
        caps0 |= SINPUT_FEAT_CAP_LEFT_TRIGGER;
    if (configuration->getIncludeRyAxis())
        caps0 |= SINPUT_FEAT_CAP_RIGHT_TRIGGER;
    report[SINPUT_FEAT_IDX_CAPS0] = caps0;

    uint8_t caps1 = 0;
    if (configuration->getEnableSInputRGB())
        caps1 |= SINPUT_FEAT_CAP_JOYSTICKRGB;
    if (configuration->getEnableTouchpad())
        caps1 |= SINPUT_FEAT_CAP_TOUCHPAD;
    report[SINPUT_FEAT_IDX_CAPS1] = caps1;

    report[SINPUT_FEAT_IDX_TYPE] = configuration->getSInputGamepadType();
    report[SINPUT_FEAT_IDX_STYLE] = configuration->getSInputFaceStyle();

    if (configuration->getEnableTouchpad())
    {
        report[SINPUT_FEAT_IDX_TOUCHPAD_COUNT] = configuration->getTouchpadCount();
        report[SINPUT_FEAT_IDX_TOUCHPAD_FINGER_COUNT] = configuration->getTouchpadFingerCount();
    }

    // IMU polling rate and ranges -- only read by SDL when the corresponding
    // capability bits above are set. 5000us = 200Hz, +/-8g accel, +/-2000dps gyro.
    if (configuration->getEnableSInputIMU())
    {
        report[SINPUT_FEAT_IDX_POLL_US] = 0x88;     // 5000us = 200Hz (uint16 LE)
        report[SINPUT_FEAT_IDX_POLL_US + 1] = 0x13;
        report[SINPUT_FEAT_IDX_ACCEL_RANGE] = 8;     // +/-8g (uint16 LE)
        report[SINPUT_FEAT_IDX_ACCEL_RANGE + 1] = 0;
        report[SINPUT_FEAT_IDX_GYRO_RANGE] = 0xD0;   // 2000dps (uint16 LE)
        report[SINPUT_FEAT_IDX_GYRO_RANGE + 1] = 0x07;
    }

    uint8_t usageMask0 = 0;
    if (configuration->getButtonCount() >= 1) usageMask0 |= SINPUT_BTN0_SOUTH;
    if (configuration->getButtonCount() >= 2) usageMask0 |= SINPUT_BTN0_EAST;
    if (configuration->getButtonCount() >= 3) usageMask0 |= SINPUT_BTN0_WEST;
    if (configuration->getButtonCount() >= 4) usageMask0 |= SINPUT_BTN0_NORTH;
    if (configuration->getHatSwitchCount() >= 1)
        usageMask0 |= SINPUT_BTN0_DUP | SINPUT_BTN0_DDOWN | SINPUT_BTN0_DLEFT | SINPUT_BTN0_DRIGHT;
    report[SINPUT_FEAT_IDX_USAGE_MASK_0] = usageMask0;

    uint8_t usageMask1 = 0;
    if (configuration->getButtonCount() >= 7)  usageMask1 |= SINPUT_BTN1_LSTICK;
    if (configuration->getButtonCount() >= 8)  usageMask1 |= SINPUT_BTN1_RSTICK;
    if (configuration->getButtonCount() >= 5)  usageMask1 |= SINPUT_BTN1_LSHOULDER;
    if (configuration->getButtonCount() >= 6)  usageMask1 |= SINPUT_BTN1_RSHOULDER;
    if (configuration->getButtonCount() >= 9)  usageMask1 |= SINPUT_BTN1_LTRIGGER;
    if (configuration->getButtonCount() >= 10) usageMask1 |= SINPUT_BTN1_RTRIGGER;
    if (configuration->getButtonCount() >= 11) usageMask1 |= SINPUT_BTN1_LPADDLE1;
    if (configuration->getButtonCount() >= 12) usageMask1 |= SINPUT_BTN1_RPADDLE1;
    report[SINPUT_FEAT_IDX_USAGE_MASK_1] = usageMask1;

    uint8_t usageMask2 = 0;
    if (configuration->getIncludeStart()) usageMask2 |= SINPUT_BTN2_START;
    if (configuration->getIncludeBack() || configuration->getIncludeSelect()) usageMask2 |= SINPUT_BTN2_BACK;
    if (configuration->getIncludeHome()) usageMask2 |= SINPUT_BTN2_GUIDE;
    if (configuration->getButtonCount() >= 13) usageMask2 |= SINPUT_BTN2_CAPTURE;
    if (configuration->getButtonCount() >= 14) usageMask2 |= SINPUT_BTN2_LPADDLE2;
    if (configuration->getButtonCount() >= 15) usageMask2 |= SINPUT_BTN2_RPADDLE2;
    if (configuration->getButtonCount() >= 16) usageMask2 |= SINPUT_BTN2_TOUCHPAD1;
    if (configuration->getButtonCount() >= 17) usageMask2 |= SINPUT_BTN2_TOUCHPAD2;
    report[SINPUT_FEAT_IDX_USAGE_MASK_2] = usageMask2;

    uint8_t usageMask3 = 0;
    if (configuration->getButtonCount() >= 18) usageMask3 |= SINPUT_BTN3_POWER;
    if (configuration->getButtonCount() >= 19) usageMask3 |= SINPUT_BTN3_MISC1;
    if (configuration->getButtonCount() >= 20) usageMask3 |= SINPUT_BTN3_MISC2;
    if (configuration->getButtonCount() >= 21) usageMask3 |= SINPUT_BTN3_MISC3;
    if (configuration->getButtonCount() >= 22) usageMask3 |= SINPUT_BTN3_MISC4;
    if (configuration->getButtonCount() >= 23) usageMask3 |= SINPUT_BTN3_MISC5;
    if (configuration->getButtonCount() >= 24) usageMask3 |= SINPUT_BTN3_MISC6;
    if (configuration->getButtonCount() >= 25) usageMask3 |= SINPUT_BTN3_MISC7;
    report[SINPUT_FEAT_IDX_USAGE_MASK_3] = usageMask3;

    cmdInputReport->setValue(report, sizeof(report));
    cmdInputReport->notify();
}

void BleSInputReceiver::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo)
{
    // Retrieve data sent from the host
    std::string value = pCharacteristic->getValue();

    const uint8_t* raw = (const uint8_t*)value.c_str();
    size_t length = 0;
    const uint8_t* data = stripReportIdIfPresent(raw, value.length(), SINPUT_REPORT_LEN_OUTPUT, length);

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

        case SINPUT_COMMAND_HAPTIC:
            if (length >= 6 && data[1] == 0x02) // type 2 = ERM simulation
            {
                rumbleLeftAmplitude = data[2];   // left motor (weak)
                rumbleRightAmplitude = data[4];  // right motor (strong)
                rumbleFlag = true;
            }
            break;

        case SINPUT_COMMAND_JOYSTICKRGB:
            if (length >= 5)
            {
                rgbRed = data[2];
                rgbGreen = data[3];
                rgbBlue = data[4];
                rgbFlag = true;
            }
            break;

        default:
            break;
    }
}
