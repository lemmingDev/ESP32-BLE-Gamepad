#ifndef BLE_XINPUT_DESCRIPTORS_H
#define BLE_XINPUT_DESCRIPTORS_H

#include <stdint.h>

// Xbox Series X / One S Bluetooth HID Report Descriptor.
// Adapted from Mystfit/ESP32-BLE-CompositeHID (MIT license), itself reverse
// engineered from real Xbox controller BLE captures. The descriptor defines
// four report IDs:
//
//   0x01 - Input:  18-byte gamepad state (sticks, triggers, hat, buttons, share)
//   0x02 - Input:   1-byte Consumer Control (Home / AC Back)
//   0x03 - Output:  8-byte PID Set Effect Report (rumble command)
//   0x04 - Input:   1-byte Battery Strength
//
// Both Xbox One S (PID 0x02FD) and Xbox Series X (PID 0x0B13) share this
// same descriptor layout; only the PnP PID differs.

static const uint8_t XboxInputDescriptor[] = {
    // ===== Report ID 0x01: Gamepad Input (18 bytes on wire) =====

    0x05, 0x01,                 // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                 // USAGE (Gamepad)
    0xA1, 0x01,                 // COLLECTION (Application)
    0x85, 0x01,                 //   REPORT_ID (1)

    // Left stick X/Y
    0x09, 0x01,                 //   USAGE (Pointer)
    0xA1, 0x00,                 //   COLLECTION (Physical)
    0x09, 0x30,                 //     USAGE (X)
    0x09, 0x31,                 //     USAGE (Y)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, // LOGICAL_MAXIMUM (65535)
    0x95, 0x02,                 //     REPORT_COUNT (2)
    0x75, 0x10,                 //     REPORT_SIZE (16)
    0x81, 0x02,                 //     INPUT (Data,Var,Abs)
    0xC0,                       //   END_COLLECTION (Physical)

    // Right stick X/Y (Z/Rz)
    0x09, 0x01,                 //   USAGE (Pointer)
    0xA1, 0x00,                 //   COLLECTION (Physical)
    0x09, 0x32,                 //     USAGE (Z)
    0x09, 0x35,                 //     USAGE (Rz)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, // LOGICAL_MAXIMUM (65535)
    0x95, 0x02,                 //     REPORT_COUNT (2)
    0x75, 0x10,                 //     REPORT_SIZE (16)
    0x81, 0x02,                 //     INPUT (Data,Var,Abs)
    0xC0,                       //   END_COLLECTION (Physical)

    // Left trigger (Brake) — 10-bit + 6-bit pad
    0x05, 0x02,                 //   USAGE_PAGE (Simulation Controls)
    0x09, 0xC5,                 //   USAGE (Brake)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x03,           //   LOGICAL_MAXIMUM (1023)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x75, 0x0A,                 //   REPORT_SIZE (10)
    0x81, 0x02,                 //   INPUT (Data,Var,Abs)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x25, 0x00,                 //   LOGICAL_MAXIMUM (0)
    0x75, 0x06,                 //   REPORT_SIZE (6)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x81, 0x03,                 //   INPUT (Const,Var,Abs) — padding

    // Right trigger (Accelerator) — 10-bit + 6-bit pad
    0x05, 0x02,                 //   USAGE_PAGE (Simulation Controls)
    0x09, 0xC4,                 //   USAGE (Accelerator)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x03,           //   LOGICAL_MAXIMUM (1023)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x75, 0x0A,                 //   REPORT_SIZE (10)
    0x81, 0x02,                 //   INPUT (Data,Var,Abs)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x25, 0x00,                 //   LOGICAL_MAXIMUM (0)
    0x75, 0x06,                 //   REPORT_SIZE (6)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x81, 0x03,                 //   INPUT (Const,Var,Abs) — padding

    // Hat switch (D-pad) — 4-bit + 4-bit pad
    0x05, 0x01,                 //   USAGE_PAGE (Generic Desktop)
    0x09, 0x39,                 //   USAGE (Hat switch)
    0x15, 0x01,                 //   LOGICAL_MINIMUM (1)
    0x25, 0x08,                 //   LOGICAL_MAXIMUM (8)
    0x35, 0x00,                 //   PHYSICAL_MINIMUM (0)
    0x46, 0x3B, 0x01,           //   PHYSICAL_MAXIMUM (315)
    0x66, 0x14, 0x00,           //   UNIT (English Rotation: Degrees)
    0x75, 0x04,                 //   REPORT_SIZE (4)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x81, 0x42,                 //   INPUT (Data,Var,Abs,Null State)
    0x75, 0x04,                 //   REPORT_SIZE (4)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x25, 0x00,                 //   LOGICAL_MAXIMUM (0)
    0x35, 0x00,                 //   PHYSICAL_MINIMUM (0)
    0x45, 0x00,                 //   PHYSICAL_MAXIMUM (0)
    0x65, 0x00,                 //   UNIT (None)
    0x81, 0x03,                 //   INPUT (Const,Var,Abs) — padding

    // 15 digital buttons + 1-bit pad
    0x05, 0x09,                 //   USAGE_PAGE (Button)
    0x19, 0x01,                 //   USAGE_MINIMUM (Button 1)
    0x29, 0x0F,                 //   USAGE_MAXIMUM (Button 15)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                 //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                 //   REPORT_SIZE (1)
    0x95, 0x0F,                 //   REPORT_COUNT (15)
    0x81, 0x02,                 //   INPUT (Data,Var,Abs)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x25, 0x00,                 //   LOGICAL_MAXIMUM (0)
    0x75, 0x01,                 //   REPORT_SIZE (1)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x81, 0x03,                 //   INPUT (Const,Var,Abs) — padding

    // Share button (Consumer Record) + 7-bit pad
    0x05, 0x0C,                 //   USAGE_PAGE (Consumer Device)
    0x0A, 0xB2, 0x00,           //   USAGE (Record)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                 //   LOGICAL_MAXIMUM (1)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x75, 0x01,                 //   REPORT_SIZE (1)
    0x81, 0x02,                 //   INPUT (Data,Var,Abs)
    0x15, 0x00,                 //   LOGICAL_MINIMUM (0)
    0x25, 0x00,                 //   LOGICAL_MAXIMUM (0)
    0x75, 0x07,                 //   REPORT_SIZE (7)
    0x95, 0x01,                 //   REPORT_COUNT (1)
    0x81, 0x03,                 //   INPUT (Const,Var,Abs) — padding

    // ===== Report ID 0x03: PID Set Effect Report (Rumble Output) =====
    0x05, 0x0F,                 //   USAGE_PAGE (Physical Interface Device)
    0x09, 0x21,                 //   USAGE (Set Effect Report)
    0x85, 0x03,                 //   REPORT_ID (3)
    0xA1, 0x02,                 //   COLLECTION (Logical)

    0x09, 0x97,                 //     USAGE (DC Enable Actuators)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                 //     LOGICAL_MAXIMUM (1)
    0x75, 0x04,                 //     REPORT_SIZE (4)
    0x95, 0x01,                 //     REPORT_COUNT (1)
    0x91, 0x02,                 //     OUTPUT (Data,Var,Abs)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x25, 0x00,                 //     LOGICAL_MAXIMUM (0)
    0x75, 0x04,                 //     REPORT_SIZE (4)
    0x95, 0x01,                 //     REPORT_COUNT (1)
    0x91, 0x03,                 //     OUTPUT (Const,Var,Abs) — padding

    0x09, 0x70,                 //     USAGE (Magnitude)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x25, 0x64,                 //     LOGICAL_MAXIMUM (100)
    0x75, 0x08,                 //     REPORT_SIZE (8)
    0x95, 0x04,                 //     REPORT_COUNT (4)
    0x91, 0x02,                 //     OUTPUT (Data,Var,Abs) — L/R trigger + weak/strong motors

    0x09, 0x50,                 //     USAGE (Duration)
    0x66, 0x01, 0x10,           //     UNIT (SI Linear: Time in seconds)
    0x55, 0x0E,                 //     UNIT_EXPONENT (-2)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00,           //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                 //     REPORT_SIZE (8)
    0x95, 0x01,                 //     REPORT_COUNT (1)
    0x91, 0x02,                 //     OUTPUT (Data,Var,Abs)

    0x09, 0xA7,                 //     USAGE (Start Delay)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00,           //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                 //     REPORT_SIZE (8)
    0x95, 0x01,                 //     REPORT_COUNT (1)
    0x91, 0x02,                 //     OUTPUT (Data,Var,Abs)

    0x65, 0x00,                 //     UNIT (None)
    0x55, 0x00,                 //     UNIT_EXPONENT (0)
    0x09, 0x7C,                 //     USAGE (Loop Count)
    0x15, 0x00,                 //     LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00,           //     LOGICAL_MAXIMUM (255)
    0x75, 0x08,                 //     REPORT_SIZE (8)
    0x95, 0x01,                 //     REPORT_COUNT (1)
    0x91, 0x02,                 //     OUTPUT (Data,Var,Abs)

    0xC0,                       //   END_COLLECTION (Logical)
    0xC0                        // END_COLLECTION (Application)
};

static const size_t XboxInputDescriptorSize = sizeof(XboxInputDescriptor);

#endif // BLE_XINPUT_DESCRIPTORS_H
