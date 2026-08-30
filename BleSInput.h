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
// from SDL's reference driver (src/joystick/hidapi/SDL_hidapi_sinput.c), since
// that's what actually decides whether a real SDL application recognizes this
// device. See GattVsHid.md.
//
// IMPORTANT: the driver has moved on since the PR that originally added it
// (https://github.com/libsdl-org/SDL/pull/13343) -- SDL 3.4.14's actual source
// already includes several later protocol revisions (PRs #13565, #13624,
// #13667 "Version as a capabilities vehicle", #13524), all merged before that
// tag. The Features response offsets below are reverse engineered against
// that current 3.4.14 source, NOT the original PR, after a real mismatch there
// was root-caused as the reason SDL always saw player_leds_supported=false --
// see the "Known issue" section (now resolved) in
// examples/SInputPlayerLED/host_test/SDL3Testing.md for the full story. If
// SDL's driver moves again, re-derive
// against its actual current source, not this comment or the original PR.
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

// Features response (Input Report 0x02) payload offsets. SDL reads
// protocol_version as a plain uint16 it stores but doesn't currently gate any
// behavior on -- it's populated anyway so a future SDL version that does
// check it doesn't regress. Every offset from CAPS0 onward is 2 higher than
// you'd expect from SINPUT_COMMAND_FEATURES's own reverse-engineering history,
// because of this field.
#define SINPUT_FEAT_IDX_PROTOCOL_VERSION 1 // uint16 LE
#define SINPUT_FEAT_IDX_CAPS0        3 // capability bitmask byte 0 (rumble/led/imu/sticks/triggers)
#define SINPUT_FEAT_IDX_CAPS1        4 // capability bitmask byte 1 (touchpad/RGB/handheld)
#define SINPUT_FEAT_IDX_TYPE         5 // SDL_GamepadType
#define SINPUT_FEAT_IDX_STYLE        6 // high nibble: face button style, low nibble: subtype
#define SINPUT_FEAT_IDX_POLL_US      7 // uint16 LE, polling rate in MICROSECONDS (only read by SDL if accel/gyro supported)
#define SINPUT_FEAT_IDX_ACCEL_RANGE  9 // uint16 LE (only read by SDL if accelerometer supported)
#define SINPUT_FEAT_IDX_GYRO_RANGE   11 // uint16 LE (only read by SDL if gyroscope supported)
#define SINPUT_FEAT_IDX_USAGE_MASK_0 13 // South, East, West, North, DUp, DDown, DLeft, DRight
#define SINPUT_FEAT_IDX_USAGE_MASK_1 14 // StickL, StickR, LShoulder, RShoulder, LTrigger, RTrigger, LPaddle1, RPaddle1
#define SINPUT_FEAT_IDX_USAGE_MASK_2 15 // Start, Back, Guide, Capture, LPaddle2, RPaddle2, TouchpadL, TouchpadR
#define SINPUT_FEAT_IDX_USAGE_MASK_3 16 // Power, Misc4-10
#define SINPUT_FEAT_IDX_TOUCHPAD_COUNT        17
#define SINPUT_FEAT_IDX_TOUCHPAD_FINGER_COUNT 18
// report[19..24]: 6-byte MAC-style serial SDL reads for its device serial string --
// not populated by this library (stays 0 from the response buffer's memset);
// cosmetic only, SDL doesn't gate any functionality on it.

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

// Digital button usage-mask bits, buttons_0 (payload byte SINPUT_IN_IDX_BUTTONS_0).
//
// NOTE: SDL_hidapi_sinput.c also defines SINPUT_BUTTONMASK_EAST/SOUTH/NORTH/WEST
// (0x01/0x02/0x04/0x08) and SINPUT_BUTTON_IDX_EAST/SOUTH/NORTH/WEST (0/1/2/3) --
// don't match bit values against those, they're unused dead constants in that
// file. The actual live decode (its per-frame button handler, not the Features
// parser) assigns raw joystick button indices purely by scanning usage_mask
// bits 0..7 in position order and incrementing a counter for each enabled bit
// -- it never looks at those named masks. Its mapping-string generator then
// assigns raw index 0/1/2/3 to South/East/West/North unconditionally, in that
// fixed order. So what actually matters is that these four bits are the
// lowest 4 enabled usage_mask bits, in this literal order -- which they are.
#define SINPUT_BTN0_SOUTH  (1 << 0)
#define SINPUT_BTN0_EAST   (1 << 1)
#define SINPUT_BTN0_WEST   (1 << 2)
#define SINPUT_BTN0_NORTH  (1 << 3)
#define SINPUT_BTN0_DUP    (1 << 4)
#define SINPUT_BTN0_DDOWN  (1 << 5)
#define SINPUT_BTN0_DLEFT  (1 << 6)
#define SINPUT_BTN0_DRIGHT (1 << 7)

// buttons_1 (payload byte SINPUT_IN_IDX_BUTTONS_1) -- only shoulders are
// mapped today. Same scan-order rule as buttons_0 above applies; these two
// being the lowest 2 enabled bits in buttons_1 is what matters, not their
// absolute bit position.
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
