#ifndef BLE_SINPUT_H
#define BLE_SINPUT_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include <NimBLEServer.h>
#include "NimBLECharacteristic.h"
#include "NimBLEConnInfo.h"
#include "BleGamepadConfiguration.h"

// SInput (https://github.com/HandHeldLegend/SInput-HID) HID report layout. There
// is no machine-readable spec for this -- these constants were reverse engineered
// from SDL's reference driver (src/joystick/hidapi/SDL_hidapi_sinput.c, added in
// https://github.com/libsdl-org/SDL/pull/13343), since that's what actually decides
// whether a real SDL application recognizes this device. See GattVsHid.md.
//
// hidapi's hid_write()/hid_read() always frame the Report ID as byte[0] of the
// buffer (SDL's SINPUT_REPORT_IDX_* constants are indices into that buffer), but a
// BLE HID-over-GATT Report characteristic's own GATT value never includes it -- the
// ID is implied by which characteristic/Report Reference descriptor you're using,
// and the OS's HID bridge adds/strips it transparently when relaying to
// hidraw/hidapi (see LinuxHIDTesting.md and BleFeatureReport.cpp/BleOutputReceiver.cpp
// for the same ambiguity on writes). So every offset below is SDL's index minus 1,
// to be relative to the raw GATT payload this library actually builds/receives.

#define SINPUT_REPORT_ID_INPUT         0x01 // Input Report: regular gamepad state
#define SINPUT_REPORT_ID_INPUT_CMDDAT  0x02 // Input Report: command/feature response
#define SINPUT_REPORT_ID_OUTPUT_CMDDAT 0x03 // Output Report: host -> device commands

#define SINPUT_REPORT_LEN_INPUT  63 // SDL's 64-byte report, minus the Report ID byte
#define SINPUT_REPORT_LEN_OUTPUT 47 // SDL's 48-byte report, minus the Report ID byte

// Output Report (0x03) sub-commands -- byte 0 of the payload
#define SINPUT_COMMAND_HAPTIC      0x01
#define SINPUT_COMMAND_FEATURES    0x02
#define SINPUT_COMMAND_PLAYERLED   0x03
#define SINPUT_COMMAND_JOYSTICKRGB 0x04

// Features response (Input Report 0x02) payload offsets
#define SINPUT_FEAT_IDX_CAPS0        1 // capability bitmask byte 0 (rumble/led/imu/sticks/triggers)
#define SINPUT_FEAT_IDX_CAPS1        2 // capability bitmask byte 1 (touchpad/RGB)
#define SINPUT_FEAT_IDX_TYPE         3 // SDL_GamepadType
#define SINPUT_FEAT_IDX_STYLE        4 // high nibble: face button style, low nibble: subtype
#define SINPUT_FEAT_IDX_POLL_MS      5 // polling rate in ms (only read by SDL if accel/gyro supported)
#define SINPUT_FEAT_IDX_ACCEL_RANGE  7 // uint16 LE (only read by SDL if accelerometer supported)
#define SINPUT_FEAT_IDX_GYRO_RANGE   9 // uint16 LE (only read by SDL if gyroscope supported)
#define SINPUT_FEAT_IDX_USAGE_MASK_0 11 // South, East, West, North, DUp, DDown, DLeft, DRight
#define SINPUT_FEAT_IDX_USAGE_MASK_1 12 // StickL, StickR, LShoulder, RShoulder, LTrigger, RTrigger, LPaddle1, RPaddle1
#define SINPUT_FEAT_IDX_USAGE_MASK_2 13 // Start, Back, Guide, Capture, LPaddle2, RPaddle2, TouchpadL, TouchpadR
#define SINPUT_FEAT_IDX_USAGE_MASK_3 14 // Power, Misc4-10
#define SINPUT_FEAT_IDX_TOUCHPAD_COUNT        15
#define SINPUT_FEAT_IDX_TOUCHPAD_FINGER_COUNT 16

#define SINPUT_FEAT_CAP_RUMBLE        (1 << 0)
#define SINPUT_FEAT_CAP_PLAYERLED     (1 << 1)
#define SINPUT_FEAT_CAP_ACCELEROMETER (1 << 2)
#define SINPUT_FEAT_CAP_GYROSCOPE     (1 << 3)
#define SINPUT_FEAT_CAP_LEFT_STICK    (1 << 4)
#define SINPUT_FEAT_CAP_RIGHT_STICK   (1 << 5)
#define SINPUT_FEAT_CAP_LEFT_TRIGGER  (1 << 6)
#define SINPUT_FEAT_CAP_RIGHT_TRIGGER (1 << 7)

#define SINPUT_FEAT_CAP_TOUCHPAD    (1 << 0)
#define SINPUT_FEAT_CAP_JOYSTICKRGB (1 << 1)

// Joystick Input Report (0x01) payload offsets
#define SINPUT_IN_IDX_PLUG_STATUS   0
#define SINPUT_IN_IDX_CHARGE_LEVEL  1

// SINPUT_IN_IDX_PLUG_STATUS values (SDL_hidapi_sinput.c's HandleStatePacket switch)
#define SINPUT_PLUG_STATUS_UNKNOWN    0 // also anything SDL doesn't recognize
#define SINPUT_PLUG_STATUS_NO_BATTERY 1 // wired / no battery -- SDL forces charge level to 0
#define SINPUT_PLUG_STATUS_CHARGING   2
#define SINPUT_PLUG_STATUS_CHARGED    3 // SDL forces charge level to 100
#define SINPUT_PLUG_STATUS_ON_BATTERY 4

#define SINPUT_IN_IDX_BUTTONS_0     2
#define SINPUT_IN_IDX_BUTTONS_1     3
#define SINPUT_IN_IDX_BUTTONS_2     4
#define SINPUT_IN_IDX_BUTTONS_3     5
#define SINPUT_IN_IDX_LEFT_X        6
#define SINPUT_IN_IDX_LEFT_Y        8
#define SINPUT_IN_IDX_RIGHT_X       10
#define SINPUT_IN_IDX_RIGHT_Y       12
#define SINPUT_IN_IDX_LEFT_TRIGGER  14
#define SINPUT_IN_IDX_RIGHT_TRIGGER 16
// Bytes 18-62 (IMU timestamp/accel/gyro, up to 2 touchpads, reserved) are left at 0 --
// this library doesn't have motion or touch input to report through SInput yet.

// Digital button usage-mask bits, buttons_0 (payload byte SINPUT_IN_IDX_BUTTONS_0)
#define SINPUT_BTN0_SOUTH  (1 << 0)
#define SINPUT_BTN0_EAST   (1 << 1)
#define SINPUT_BTN0_WEST   (1 << 2)
#define SINPUT_BTN0_NORTH  (1 << 3)
#define SINPUT_BTN0_DUP    (1 << 4)
#define SINPUT_BTN0_DDOWN  (1 << 5)
#define SINPUT_BTN0_DLEFT  (1 << 6)
#define SINPUT_BTN0_DRIGHT (1 << 7)

// buttons_1 (payload byte SINPUT_IN_IDX_BUTTONS_1) -- only shoulders are mapped today
#define SINPUT_BTN1_LSHOULDER (1 << 2)
#define SINPUT_BTN1_RSHOULDER (1 << 3)

// Handles the Output Report (0x03) this library's SInput mode receives commands on,
// and owns the Input Report (0x02) it replies to a FEATURES request on. PlayerLED is
// the only command actually acted on right now -- HAPTIC and JOYSTICKRGB are accepted
// (the write succeeds) but not driven, since this library has no rumble motor or RGB
// LED output today. See GattVsHid.md for extending this.
class BleSInputReceiver : public NimBLECharacteristicCallbacks
{
public:
    BleSInputReceiver(BleGamepadConfiguration *configuration, NimBLECharacteristic *cmdInputReport);
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override;

    bool playerLedFlag = false;
    uint8_t playerLedIndex = 0;

private:
    BleGamepadConfiguration *configuration;
    NimBLECharacteristic *cmdInputReport; // Input Report 0x02, replied to on a FEATURES request
    void sendFeaturesResponse();
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // BLE_SINPUT_H
