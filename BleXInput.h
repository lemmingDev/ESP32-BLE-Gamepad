#ifndef BLE_XINPUT_H
#define BLE_XINPUT_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"
#include "NimBLEConnInfo.h"

// XInput Report IDs
#define XINPUT_REPORT_ID_INPUT  0x01
#define XINPUT_REPORT_ID_OUTPUT 0x03

// XInput input report size (18 bytes on wire, excluding Report ID)
#define XINPUT_REPORT_LEN_INPUT 18

// XInput output report size (8 bytes on wire, excluding Report ID)
#define XINPUT_REPORT_LEN_OUTPUT 8

// Xbox button bitmasks (15-bit button field in input report)
#define XBOX_BUTTON_A      0x0001
#define XBOX_BUTTON_B      0x0002
#define XBOX_BUTTON_X      0x0008
#define XBOX_BUTTON_Y      0x0010
#define XBOX_BUTTON_LB     0x0040
#define XBOX_BUTTON_RB     0x0080
#define XBOX_BUTTON_SELECT 0x0400
#define XBOX_BUTTON_START  0x0800
#define XBOX_BUTTON_HOME   0x1000
#define XBOX_BUTTON_LS     0x2000
#define XBOX_BUTTON_RS     0x4000

// Share button (separate byte)
#define XBOX_BUTTON_SHARE  0x01

// D-pad hat values (4-bit, 1-8 range, 0 = released)
#define XBOX_DPAD_NONE      0
#define XBOX_DPAD_NORTH     1
#define XBOX_DPAD_NORTHEAST 2
#define XBOX_DPAD_EAST      3
#define XBOX_DPAD_SOUTHEAST 4
#define XBOX_DPAD_SOUTH     5
#define XBOX_DPAD_SOUTHWEST 6
#define XBOX_DPAD_WEST      7
#define XBOX_DPAD_NORTHWEST 8

// Thumbstick range
#define XBOX_STICK_MIN -32767
#define XBOX_STICK_MAX  32767
#define XBOX_AXIS_CENTER_OFFSET 0x8000

// Trigger range
#define XBOX_TRIGGER_MIN 0
#define XBOX_TRIGGER_MAX 1023

// XInput Input Report (Report ID 0x01, 18 bytes)
#pragma pack(push, 1)
struct XInputInputReport
{
    uint16_t x;            // Left stick X  (0x8000 = centered)
    uint16_t y;            // Left stick Y  (0x8000 = centered)
    uint16_t z;            // Right stick X (0x8000 = centered)
    uint16_t rz;           // Right stick Y (0x8000 = centered)
    uint16_t brake;        // Left trigger  (10-bit: 0-1023, upper 6 bits pad)
    uint16_t accelerator;  // Right trigger (10-bit: 0-1023, upper 6 bits pad)
    uint8_t hat;           // D-pad hat    (4-bit: 0-8, lower nibble; upper nibble pad)
    uint16_t buttons;      // 15 buttons   (1 bit each, bit 15 pad)
    uint8_t share;         // Share button (bit 0; bits 1-7 pad)
    uint8_t _pad[2];       // Alignment padding to reach 18 bytes on wire
};
#pragma pack(pop)

// XInput Output Report (Report ID 0x03, 8 bytes) — PID Set Effect Report
#pragma pack(push, 1)
struct XInputOutputReport
{
    uint8_t dcEnableActuators;     // 4-bit enable flags + 4-bit pad
    uint8_t leftTriggerMagnitude;  // Left trigger rumble (0-100)
    uint8_t rightTriggerMagnitude; // Right trigger rumble (0-100)
    uint8_t weakMotorMagnitude;    // Weak (high-freq) motor (0-100)
    uint8_t strongMotorMagnitude;  // Strong (low-freq) motor (0-100)
    uint8_t duration;              // Duration in 10ms units (0 = indefinite)
    uint8_t startDelay;            // Start delay in 10ms units
    uint8_t loopCount;             // Loop count (0 = infinite)
};
#pragma pack(pop)

// Handles Output Report (0x03) — receives rumble commands from the host
class BleXInputReceiver : public NimBLECharacteristicCallbacks
{
public:
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;

    bool rumbleFlag = false;
    uint8_t strongMotor = 0;
    uint8_t weakMotor = 0;
    uint8_t leftTriggerMagnitude = 0;
    uint8_t rightTriggerMagnitude = 0;
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // BLE_XINPUT_H
