#ifndef BLE_XINPUT_DESCRIPTORS_H
#define BLE_XINPUT_DESCRIPTORS_H

#include <stdint.h>

// Xbox HID Report Descriptors — taken verbatim from Mystfit/ESP32-BLE-CompositeHID
// (MIT license) at src/gamepad/xbox/XboxDescriptors.h pr-74 (570d6da).
// Bytes below are byte-for-byte identical to Mystfit; only this header comment
// and the per-line annotations were added for readability.
//
// Which descriptor is used when:
//   XboxOneS_1708 (PID 0x02FD) — GamepadMode::XInputOneS. One S hardware has no
//       Share button: the main report carries AC Back (0x0C0224) where Series X
//       carries Record/Share, plus extra reports 0x02 (AC Home) and 0x04 (battery).
//   XboxOneS_1914 (PID 0x0B13) — GamepadMode::XInputSeriesX. Main report carries
//       Record (0x0C00B2 = Share); only reports 0x01 (input) + 0x03 (output).
//
// Main input report (Report ID 0x01) wire layout — 16 bytes, matches
// XInputInputReport in BleXInput.h field for field:
//   Byte  0-1 : left stick X   (uint16, 0..65535, center 0x8000) -> report.x
//   Byte  2-3 : left stick Y   (uint16, 0..65535, center 0x8000) -> report.y
//   Byte  4-5 : right stick X  (uint16, 0..65535, center 0x8000) -> report.z
//   Byte  6-7 : right stick Y  (uint16, 0..65535, center 0x8000) -> report.rz
//   Byte  8-9 : left trigger   (10 bits, 0..1023) + 6 bits pad  -> report.brake
//   Byte 10-11: right trigger  (10 bits, 0..1023) + 6 bits pad  -> report.accelerator
//   Byte 12   : D-pad hat      (4 bits, 0=centered, 1..8) + 4 bits pad -> report.hat
//   Byte 13-14: buttons        (15 x 1 bit) + 1 bit pad          -> report.buttons
//   Byte 15   : share          (1 bit) + 7 bits pad              -> report.share
//
// Buttons field bit map (uint16, matches XBOX_BUTTON_* in BleXInput.h,
// which matches Mystfit's XboxGamepadDevice.h values exactly):
//   bit  0 (0x0001) : A        bit  7 (0x0080) : RB
//   bit  1 (0x0002) : B        bit 10 (0x0400) : Select
//   bit  3 (0x0008) : X        bit 11 (0x0800) : Start
//   bit  4 (0x0010) : Y        bit 12 (0x1000) : Guide/Home
//   bit  6 (0x0040) : LB       bit 13 (0x2000) : Left stick click
//   (bits 2,5,8,9 are unused gaps inherited from the real controller;
//    bit 14 (0x4000) : Right stick click; bit 15 is padding)
// The D-pad is NOT in the buttons field — hat byte only.
//
// HID item shorthand used in the per-line comments:
//   USAGE_PAGE xxx      = which usage table (Generic Desktop 0x01,
//                           Simulation 0x02, Button 0x09, Consumer 0x0C,
//                           PID 0x0F, Device Controls 0x06)
//   USAGE / MIN / MAX   = which control(s) the following INPUT/OUTPUT covers
//   LOGICAL_MIN/MAX     = value range on the wire
//   REPORT_COUNT x SIZE = field count x bits per field
//   INPUT Data/Const    = Data carries host-visible values, Const is padding
//   APPLICATION / PHYSICAL / LOGICAL collections group related controls;
//   every C0 closes the most recent A1.

static const uint8_t XboxOneS_1708_HIDDescriptor[] = {
    // ===== Report 0x01 (main input, 16 bytes) =====
    0x05, 0x01,                     // USAGE_PAGE Generic Desktop
    0x09, 0x05,                     // USAGE Game Pad
    0xA1, 0x01,                     // COLLECTION Application (whole gamepad)
    0x85, 0x01,                     // REPORT_ID 0x01
    // -- Left stick: X + Y, 2 x 16 bits = 4 bytes (report.x, report.y) --
    0x09, 0x01,                     // USAGE Pointer
    0xA1, 0x00,                     // COLLECTION Physical
    0x09, 0x30,                     // USAGE X (left stick X)
    0x09, 0x31,                     // USAGE Y (left stick Y)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x27, 0xFF, 0xFF, 0x00, 0x00,   // LOGICAL_MAXIMUM 65535
    0x95, 0x02,                     // REPORT_COUNT 2
    0x75, 0x10,                     // REPORT_SIZE 16
    0x81, 0x02,                     // INPUT Data (2 fields x 16 bits)
    0xC0,                           // END_COLLECTION Physical
    // -- Right stick: Z + Rz, 2 x 16 bits = 4 bytes (report.z, report.rz) --
    0x09, 0x01,                     // USAGE Pointer
    0xA1, 0x00,                     // COLLECTION Physical
    0x09, 0x32,                     // USAGE Z (right stick X)
    0x09, 0x35,                     // USAGE Rz (right stick Y)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x27, 0xFF, 0xFF, 0x00, 0x00,   // LOGICAL_MAXIMUM 65535
    0x95, 0x02,                     // REPORT_COUNT 2
    0x75, 0x10,                     // REPORT_SIZE 16
    0x81, 0x02,                     // INPUT Data (2 fields x 16 bits)
    0xC0,                           // END_COLLECTION Physical
    // -- Left trigger (Brake): 10 bits value + 6 bits pad = 2 bytes (report.brake) --
    0x05, 0x02,                     // USAGE_PAGE Simulation Controls
    0x09, 0xC5,                     // USAGE Brake (left trigger)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x03,               // LOGICAL_MAXIMUM 1023
    0x95, 0x01,                     // REPORT_COUNT 1
    0x75, 0x0A,                     // REPORT_SIZE 10
    0x81, 0x02,                     // INPUT Data (1 field x 10 bits)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x06,                     // REPORT_SIZE 6
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 6 bits)
    // -- Right trigger (Accelerator): 10 bits value + 6 bits pad = 2 bytes (report.accelerator) --
    0x05, 0x02,                     // USAGE_PAGE Simulation Controls
    0x09, 0xC4,                     // USAGE Accelerator (right trigger)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x03,               // LOGICAL_MAXIMUM 1023
    0x95, 0x01,                     // REPORT_COUNT 1
    0x75, 0x0A,                     // REPORT_SIZE 10
    0x81, 0x02,                     // INPUT Data (1 field x 10 bits)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x06,                     // REPORT_SIZE 6
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 6 bits)
    // -- D-pad hat: 4 bits value + 4 bits pad = 1 byte (report.hat) --
    // Values 0=centered, 1..8 clockwise from north; Null state allowed (0x42).
    0x05, 0x01,                     // USAGE_PAGE Generic Desktop
    0x09, 0x39,                     // USAGE Hat Switch
    0x15, 0x01,                     // LOGICAL_MINIMUM 1
    0x25, 0x08,                     // LOGICAL_MAXIMUM 8
    0x35, 0x00,                     // PHYSICAL_MINIMUM 0
    0x46, 0x3B, 0x01,               // PHYSICAL_MAXIMUM 315 (degrees)
    0x66, 0x14, 0x00,               // UNIT rotation in degrees
    0x75, 0x04,                     // REPORT_SIZE 4
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x42,                     // INPUT Data,Null (1 field x 4 bits)
    0x75, 0x04,                     // REPORT_SIZE 4
    0x95, 0x01,                     // REPORT_COUNT 1
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x35, 0x00,                     // PHYSICAL_MINIMUM 0
    0x45, 0x00,                     // PHYSICAL_MAXIMUM 0
    0x65, 0x00,                     // UNIT none
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 4 bits)
    // -- Buttons 1..15: 15 x 1 bit + 1 bit pad = 2 bytes (report.buttons) --
    // See the bit map in the file header for which bit is which Xbox button.
    0x05, 0x09,                     // USAGE_PAGE Button
    0x19, 0x01,                     // USAGE_MINIMUM Button 1
    0x29, 0x0F,                     // USAGE_MAXIMUM Button 15
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x01,                     // LOGICAL_MAXIMUM 1
    0x75, 0x01,                     // REPORT_SIZE 1
    0x95, 0x0F,                     // REPORT_COUNT 15
    0x81, 0x02,                     // INPUT Data (15 fields x 1 bit)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x01,                     // REPORT_SIZE 1
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 1 bit)
    // -- Share slot: 1 bit + 7 bits pad = 1 byte (report.share) --
    // 1708 hardware has no Share button, so this usage is AC Back (0x0C0224).
    // Firmware sets it from the BACK special (see sendXInputReport).
    0x05, 0x0C,                     // USAGE_PAGE Consumer
    0x0A, 0x24, 0x02,               // USAGE AC Back (Share replacement on 1708)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x01,                     // LOGICAL_MAXIMUM 1
    0x95, 0x01,                     // REPORT_COUNT 1
    0x75, 0x01,                     // REPORT_SIZE 1
    0x81, 0x02,                     // INPUT Data (1 field x 1 bit)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x07,                     // REPORT_SIZE 7
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 7 bits)

    // ===== Report 0x02 (extra input, 1 byte: AC Home) — 1708 only =====
    // Real 1708 hardware sends Guide here; our firmware (like Mystfit's) puts
    // Guide in the buttons field instead, so this characteristic is created
    // but never notified. It must still EXIST or Windows rejects the service.
    0x05, 0x0C,                     // USAGE_PAGE Consumer
    0x09, 0x01,                     // USAGE Consumer Control
    0x85, 0x02,                     // REPORT_ID 0x02
    0xA1, 0x01,                     // COLLECTION Application
    0x05, 0x0C,                     // USAGE_PAGE Consumer
    0x0A, 0x23, 0x02,               // USAGE AC Home
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x01,                     // LOGICAL_MAXIMUM 1
    0x95, 0x01,                     // REPORT_COUNT 1
    0x75, 0x01,                     // REPORT_SIZE 1
    0x81, 0x02,                     // INPUT Data (1 field x 1 bit)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x07,                     // REPORT_SIZE 7
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 7 bits)
    0xC0,                           // END_COLLECTION Application

    // ===== Report 0x03 (output, 8 bytes: rumble) =====
    // Host->device: actuator enables + 4 magnitudes (0..100) + duration /
    // start-delay / loop-count. Firmware reads it in BleXInputReceiver.
    0x05, 0x0F,                     // USAGE_PAGE PID (Physical Interface Device)
    0x09, 0x21,                     // USAGE Set Effect Report
    0x85, 0x03,                     // REPORT_ID 0x03
    0xA1, 0x02,                     // COLLECTION Logical
    0x09, 0x97,                     // USAGE DC Enable Actuators
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x01,                     // LOGICAL_MAXIMUM 1
    0x75, 0x04,                     // REPORT_SIZE 4
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x02,                     // OUTPUT Data (1 field x 4 bits)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x04,                     // REPORT_SIZE 4
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x03,                     // OUTPUT Const (padding, 1 field x 4 bits)
    0x09, 0x70,                     // USAGE Magnitude
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x64,                     // LOGICAL_MAXIMUM 100
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x04,                     // REPORT_COUNT 4 (L-trig, R-trig, weak, strong)
    0x91, 0x02,                     // OUTPUT Data (4 fields x 8 bits)
    0x09, 0x50,                     // USAGE Duration
    0x66, 0x01, 0x10,               // UNIT seconds
    0x55, 0x0E,                     // UNIT_EXPONENT -2 (x10ms units)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x00,               // LOGICAL_MAXIMUM 255
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x02,                     // OUTPUT Data (1 field x 8 bits)
    0x09, 0xA7,                     // USAGE Start Delay
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x00,               // LOGICAL_MAXIMUM 255
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x02,                     // OUTPUT Data (1 field x 8 bits)
    0x65, 0x00,                     // UNIT none
    0x55, 0x00,                     // UNIT_EXPONENT 0
    0x09, 0x7C,                     // USAGE Loop Count
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x00,               // LOGICAL_MAXIMUM 255
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x02,                     // OUTPUT Data (1 field x 8 bits)
    0xC0,                           // END_COLLECTION Logical

    // ===== Report 0x04 (extra input, 1 byte: battery) — 1708 only =====
    // Created (see BleGamepad.cpp HID setup) and initialised to 0, never notified.
    0x05, 0x06,                     // USAGE_PAGE Generic Device Controls
    0x09, 0x20,                     // USAGE Battery Strength
    0x85, 0x04,                     // REPORT_ID 0x04
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x00,               // LOGICAL_MAXIMUM 255
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x02,                     // INPUT Data (1 field x 8 bits)
    0xC0,                           // END_COLLECTION Application
};

static const uint8_t XboxOneS_1914_HIDDescriptor[] = {
    // ===== Report 0x01 (main input, 16 bytes) — identical fields to 1708
    // except the share slot below, which is Record (real Share button). =====
    0x05, 0x01,                     // USAGE_PAGE Generic Desktop
    0x09, 0x05,                     // USAGE Game Pad
    0xA1, 0x01,                     // COLLECTION Application (whole gamepad)
    0x85, 0x01,                     // REPORT_ID 0x01
    // -- Left stick: X + Y, 2 x 16 bits = 4 bytes (report.x, report.y) --
    0x09, 0x01,                     // USAGE Pointer
    0xA1, 0x00,                     // COLLECTION Physical
    0x09, 0x30,                     // USAGE X (left stick X)
    0x09, 0x31,                     // USAGE Y (left stick Y)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x27, 0xFF, 0xFF, 0x00, 0x00,   // LOGICAL_MAXIMUM 65535
    0x95, 0x02,                     // REPORT_COUNT 2
    0x75, 0x10,                     // REPORT_SIZE 16
    0x81, 0x02,                     // INPUT Data (2 fields x 16 bits)
    0xC0,                           // END_COLLECTION Physical
    // -- Right stick: Z + Rz, 2 x 16 bits = 4 bytes (report.z, report.rz) --
    0x09, 0x01,                     // USAGE Pointer
    0xA1, 0x00,                     // COLLECTION Physical
    0x09, 0x32,                     // USAGE Z (right stick X)
    0x09, 0x35,                     // USAGE Rz (right stick Y)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x27, 0xFF, 0xFF, 0x00, 0x00,   // LOGICAL_MAXIMUM 65535
    0x95, 0x02,                     // REPORT_COUNT 2
    0x75, 0x10,                     // REPORT_SIZE 16
    0x81, 0x02,                     // INPUT Data (2 fields x 16 bits)
    0xC0,                           // END_COLLECTION Physical
    // -- Left trigger (Brake): 10 bits value + 6 bits pad = 2 bytes (report.brake) --
    0x05, 0x02,                     // USAGE_PAGE Simulation Controls
    0x09, 0xC5,                     // USAGE Brake (left trigger)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x03,               // LOGICAL_MAXIMUM 1023
    0x95, 0x01,                     // REPORT_COUNT 1
    0x75, 0x0A,                     // REPORT_SIZE 10
    0x81, 0x02,                     // INPUT Data (1 field x 10 bits)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x06,                     // REPORT_SIZE 6
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 6 bits)
    // -- Right trigger (Accelerator): 10 bits value + 6 bits pad = 2 bytes (report.accelerator) --
    0x05, 0x02,                     // USAGE_PAGE Simulation Controls
    0x09, 0xC4,                     // USAGE Accelerator (right trigger)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x03,               // LOGICAL_MAXIMUM 1023
    0x95, 0x01,                     // REPORT_COUNT 1
    0x75, 0x0A,                     // REPORT_SIZE 10
    0x81, 0x02,                     // INPUT Data (1 field x 10 bits)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x06,                     // REPORT_SIZE 6
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 6 bits)
    // -- D-pad hat: 4 bits value + 4 bits pad = 1 byte (report.hat) --
    0x05, 0x01,                     // USAGE_PAGE Generic Desktop
    0x09, 0x39,                     // USAGE Hat Switch
    0x15, 0x01,                     // LOGICAL_MINIMUM 1
    0x25, 0x08,                     // LOGICAL_MAXIMUM 8
    0x35, 0x00,                     // PHYSICAL_MINIMUM 0
    0x46, 0x3B, 0x01,               // PHYSICAL_MAXIMUM 315 (degrees)
    0x66, 0x14, 0x00,               // UNIT rotation in degrees
    0x75, 0x04,                     // REPORT_SIZE 4
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x42,                     // INPUT Data,Null (1 field x 4 bits)
    0x75, 0x04,                     // REPORT_SIZE 4
    0x95, 0x01,                     // REPORT_COUNT 1
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x35, 0x00,                     // PHYSICAL_MINIMUM 0
    0x45, 0x00,                     // PHYSICAL_MAXIMUM 0
    0x65, 0x00,                     // UNIT none
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 4 bits)
    // -- Buttons 1..15: 15 x 1 bit + 1 bit pad = 2 bytes (report.buttons) --
    // See the bit map in the file header for which bit is which Xbox button.
    0x05, 0x09,                     // USAGE_PAGE Button
    0x19, 0x01,                     // USAGE_MINIMUM Button 1
    0x29, 0x0F,                     // USAGE_MAXIMUM Button 15
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x01,                     // LOGICAL_MAXIMUM 1
    0x75, 0x01,                     // REPORT_SIZE 1
    0x95, 0x0F,                     // REPORT_COUNT 15
    0x81, 0x02,                     // INPUT Data (15 fields x 1 bit)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x01,                     // REPORT_SIZE 1
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 1 bit)
    // -- Share slot: 1 bit + 7 bits pad = 1 byte (report.share) --
    // 1914 hardware HAS a Share button: usage is Record (0x0C00B2).
    // Firmware sets it from the BACK special (see sendXInputReport).
    0x05, 0x0C,                     // USAGE_PAGE Consumer
    0x0A, 0xB2, 0x00,               // USAGE Record (Share button on 1914)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x01,                     // LOGICAL_MAXIMUM 1
    0x95, 0x01,                     // REPORT_COUNT 1
    0x75, 0x01,                     // REPORT_SIZE 1
    0x81, 0x02,                     // INPUT Data (1 field x 1 bit)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x07,                     // REPORT_SIZE 7
    0x95, 0x01,                     // REPORT_COUNT 1
    0x81, 0x03,                     // INPUT Const (padding, 1 field x 7 bits)

    // ===== Report 0x03 (output, 8 bytes: rumble) — same as 1708 =====
    0x05, 0x0F,                     // USAGE_PAGE PID (Physical Interface Device)
    0x09, 0x21,                     // USAGE Set Effect Report
    0x85, 0x03,                     // REPORT_ID 0x03
    0xA1, 0x02,                     // COLLECTION Logical
    0x09, 0x97,                     // USAGE DC Enable Actuators
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x01,                     // LOGICAL_MAXIMUM 1
    0x75, 0x04,                     // REPORT_SIZE 4
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x02,                     // OUTPUT Data (1 field x 4 bits)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x00,                     // LOGICAL_MAXIMUM 0
    0x75, 0x04,                     // REPORT_SIZE 4
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x03,                     // OUTPUT Const (padding, 1 field x 4 bits)
    0x09, 0x70,                     // USAGE Magnitude
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x25, 0x64,                     // LOGICAL_MAXIMUM 100
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x04,                     // REPORT_COUNT 4 (L-trig, R-trig, weak, strong)
    0x91, 0x02,                     // OUTPUT Data (4 fields x 8 bits)
    0x09, 0x50,                     // USAGE Duration
    0x66, 0x01, 0x10,               // UNIT seconds
    0x55, 0x0E,                     // UNIT_EXPONENT -2 (x10ms units)
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x00,               // LOGICAL_MAXIMUM 255
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x02,                     // OUTPUT Data (1 field x 8 bits)
    0x09, 0xA7,                     // USAGE Start Delay
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x00,               // LOGICAL_MAXIMUM 255
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x02,                     // OUTPUT Data (1 field x 8 bits)
    0x65, 0x00,                     // UNIT none
    0x55, 0x00,                     // UNIT_EXPONENT 0
    0x09, 0x7C,                     // USAGE Loop Count
    0x15, 0x00,                     // LOGICAL_MINIMUM 0
    0x26, 0xFF, 0x00,               // LOGICAL_MAXIMUM 255
    0x75, 0x08,                     // REPORT_SIZE 8
    0x95, 0x01,                     // REPORT_COUNT 1
    0x91, 0x02,                     // OUTPUT Data (1 field x 8 bits)
    0xC0,                           // END_COLLECTION Logical
    0xC0,                           // END_COLLECTION Application
};

// Descriptor sizes (used by BleGamepad.cpp HID setup to copy the active variant
// into the report map). No backward-compat alias: callers use the two arrays above.
static const size_t XboxOneS_1708_DescriptorSize = sizeof(XboxOneS_1708_HIDDescriptor);
static const size_t XboxOneS_1914_DescriptorSize = sizeof(XboxOneS_1914_HIDDescriptor);

#endif // BLE_XINPUT_DESCRIPTORS_H