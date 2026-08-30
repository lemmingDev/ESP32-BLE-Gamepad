#include "BleXInput.h"
#include "BleReportUtils.h"

#if defined(CONFIG_BT_ENABLED)

#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

void BleXInputReceiver::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    const NimBLEAttValue &value = pCharacteristic->getValue();
    size_t len = value.size();
    const uint8_t *raw = value.data();
    size_t actualLen = 0;
    const uint8_t *data = stripReportIdIfPresent(raw, len, sizeof(XInputOutputReport), actualLen);

    if (actualLen < sizeof(XInputOutputReport))
    {
        return;
    }

    const XInputOutputReport *report = (const XInputOutputReport *)data;

    leftTriggerMagnitude = report->leftTriggerMagnitude;
    rightTriggerMagnitude = report->rightTriggerMagnitude;
    weakMotor = report->weakMotorMagnitude;
    strongMotor = report->strongMotorMagnitude;

    if (report->dcEnableActuators & 0x0F)
    {
        rumbleFlag = true;
    }
    else if (report->weakMotorMagnitude == 0 && report->strongMotorMagnitude == 0)
    {
        rumbleFlag = false;
    }
}

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
