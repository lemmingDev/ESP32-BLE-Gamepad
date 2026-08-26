#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include "NimBLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include "sdkconfig.h"
#include "BleConnectionStatus.h"
#include "BleGamepad.h"
#include "NimBLELog.h"
#include "BleGamepadConfiguration.h"

#include <stdexcept>

#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "BLEGamepad"
#else
#include "esp_log.h"
static const char *LOG_TAG = "BLEGamepad";
#endif

#define SERVICE_UUID_DEVICE_INFORMATION         "180A"      // Service - Device information

#define CHARACTERISTIC_UUID_MODEL_NUMBER        "2A24"      // Characteristic - Model Number String - 0x2A24
#define CHARACTERISTIC_UUID_SOFTWARE_REVISION   "2A28"      // Characteristic - Software Revision String - 0x2A28
#define CHARACTERISTIC_UUID_SERIAL_NUMBER       "2A25"      // Characteristic - Serial Number String - 0x2A25
#define CHARACTERISTIC_UUID_FIRMWARE_REVISION   "2A26"      // Characteristic - Firmware Revision String - 0x2A26
#define CHARACTERISTIC_UUID_HARDWARE_REVISION   "2A27"      // Characteristic - Hardware Revision String - 0x2A27
#define CHARACTERISTIC_UUID_BATTERY_POWER_STATE "2A1A"      // Characteristic - Battery Power State - 0x2A1A

#define POWER_STATE_UNKNOWN         0 // 0b00
#define POWER_STATE_NOT_SUPPORTED   1 // 0b01
#define POWER_STATE_NOT_PRESENT     2 // 0b10
#define POWER_STATE_NOT_DISCHARGING 2 // 0b10
#define POWER_STATE_NOT_CHARGING    2 // 0b10
#define POWER_STATE_GOOD            2 // 0b10
#define POWER_STATE_PRESENT         3 // 0b11
#define POWER_STATE_DISCHARGING     3 // 0b11
#define POWER_STATE_CHARGING        3 // 0b11
#define POWER_STATE_CRITICAL        3 // 0b11

#if BLE_GAMEPAD_DEBUG == 1
static void dumpHIDReport(const uint8_t* report, size_t len);
#endif

BleGamepad::BleGamepad(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel, bool delayAdvertising) : _buttons(),
  _specialButtons(0),
  _x(0),
  _y(0),
  _z(0),
  _rX(0),
  _rY(0),
  _rZ(0),
  _slider1(0),
  _slider2(0),
  _rudder(0),
  _throttle(0),
  _accelerator(0),
  _brake(0),
  _steering(0),
  _hat1(0),
  _hat2(0),
  _hat3(0),
  _hat4(0),
  _gX(0),
  _gY(0),
  _gZ(0),
  _aX(0),
  _aY(0),
  _aZ(0),
  _batteryPowerInformation(0),
  _dischargingState(0),
  _chargingState(0),
  _powerLevel(0),
  hid(0),
  pCharacteristic_Power_State(0),
  configuration(),
  pServer(nullptr), 
  nus(nullptr)
{
  this->resetButtons();
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
  this->delayAdvertising = delayAdvertising;
  this->connectionStatus = new BleConnectionStatus();
  
  hidReportDescriptorSize = 0;
  hidReportSize = 0;
  numOfButtonBytes = 0;

  enableOutputReport = false;
  outputReportLength = 64;
  enableFeatureReport = false;
  featureReportLength = 64;

  nusInitialized = false;
}

void BleGamepad::resetButtons()
{
  memset(&_buttons, 0, sizeof(_buttons));
}

void BleGamepad::begin(BleGamepadConfiguration *config)
{
  configuration = *config; // we make a copy, so the user can't change actual values midway through operation, without calling the begin function again

  enableOutputReport = configuration.getEnableOutputReport();
  outputReportLength = configuration.getOutputReportLength();
  enableFeatureReport = configuration.getEnableFeatureReport();
  featureReportLength = configuration.getFeatureReportLength();
  enableSInput = configuration.getEnableSInput();

  uint8_t buttonPaddingBits = 8 - (configuration.getButtonCount() % 8);
  if (buttonPaddingBits == 8)
  {
    buttonPaddingBits = 0;
  }
  uint8_t specialButtonPaddingBits = 8 - (configuration.getTotalSpecialButtonCount() % 8);
  if (specialButtonPaddingBits == 8)
  {
    specialButtonPaddingBits = 0;
  }
  uint8_t numOfAxisBytes = configuration.getAxisCount() * 2;
  uint8_t numOfSimulationBytes = configuration.getSimulationCount() * 2;

  numOfButtonBytes = configuration.getButtonCount() / 8;
  if (buttonPaddingBits > 0)
  {
    numOfButtonBytes++;
  }

  uint8_t numOfSpecialButtonBytes = configuration.getTotalSpecialButtonCount() / 8;
  if (specialButtonPaddingBits > 0)
  {
    numOfSpecialButtonBytes++;
  }
  
  uint8_t numOfMotionBytes = 0;
  if (configuration.getIncludeAccelerometer())
  {
    numOfMotionBytes += 6;
  }
  
  if (configuration.getIncludeGyroscope())
  {
    numOfMotionBytes += 6;
  }

  hidReportSize = numOfButtonBytes + numOfSpecialButtonBytes + numOfAxisBytes + numOfSimulationBytes + numOfMotionBytes + configuration.getHatSwitchCount();

  if (enableSInput)
  {
    // SInput owns Report IDs 0x01-0x03 outright (see BleSInput.h) -- it doesn't
    // layer on top of the configurable report below, it replaces it. One
    // collection with three fixed-size reports: Input 0x01 (regular gamepad
    // state), Input 0x02 (command/feature response), Output 0x03 (host ->
    // device commands). SDL's SInput driver reads/writes these by fixed byte
    // offset and doesn't parse this descriptor's field-level contents at all --
    // see GattVsHid.md.
    //
    // The top-level Usage Page/Usage below is Generic Desktop/Gamepad, NOT
    // Vendor Defined, and that part *does* matter, confirmed against a live
    // SDL3 build: SDL_HIDAPI_ShouldIgnoreDevice() (src/hidapi/SDL_hidapi.c)
    // discards any device from hidapi enumeration entirely -- before its VID/PID
    // is even checked against SDL_IsJoystickSInputController() -- unless its top
    // -level usage is Generic Desktop Joystick/Gamepad/MultiAxisController (this
    // is gated by the SDL_HINT_JOYSTICK_HIDAPI_CONTROLLERS hint, default on). A
    // Vendor Defined top-level usage here means the device never reaches SDL's
    // gamepad layer at all, regardless of everything else being correct.

    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE (Gamepad)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;

    // COLLECTION (Application)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xa1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Input Report 0x01 (regular gamepad state). Unlike Reports 0x02/0x03
    // below, this one's buttons/axes fields carry real HID usages (Button,
    // Generic Desktop X/Y/Z/Rz/Rx/Ry) over the exact same bytes SDL's SInput
    // driver already reads by fixed offset -- SDL ignores usage annotations
    // entirely, but the kernel's hid-generic/hid-input driver needs them to
    // map fields to evdev BTN_*/ABS_* codes. Without this, SDL still works
    // (confirmed), but nothing else does -- no /dev/input/js*, no plain
    // jstest/evtest, no non-SInput-aware app. With it, both paths work off
    // the same bytes.
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x85; // REPORT_ID
    tempHidReportDescriptor[hidReportDescriptorSize++] = SINPUT_REPORT_ID_INPUT;

    // Bytes 0-1 (plug status, charge level): no generic meaning, pad them out
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15; // LOGICAL_MINIMUM (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26; // LOGICAL_MAXIMUM (255)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75; // REPORT_SIZE (8)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95; // REPORT_COUNT (2)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81; // INPUT (Const,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;

    // Bytes 2-5 (buttons_0..3): 32 generic buttons, matching this library's
    // classic descriptor's own button block above
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05; // USAGE_PAGE (Button)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15; // LOGICAL_MINIMUM (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x25; // LOGICAL_MAXIMUM (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75; // REPORT_SIZE (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x19; // USAGE_MINIMUM (Button 1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x29; // USAGE_MAXIMUM (Button 32)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x20;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95; // REPORT_COUNT (32)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x20;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81; // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // Bytes 6-17 (left X/Y, right X/Y, left/right trigger): 6 signed 16-bit
    // axes, same X/Y/Z/Rz/Rx/Ry usage codes and ordering this library's
    // classic descriptor/setHIDAxes() already use
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05; // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16; // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x80;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26; // LOGICAL_MAXIMUM (32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x7F;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75; // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09; // USAGE (X) -- left stick X
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x30;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09; // USAGE (Y) -- left stick Y
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x31;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09; // USAGE (Z) -- right stick X
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x32;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09; // USAGE (Rz) -- right stick Y
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x35;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09; // USAGE (Rx) -- left trigger
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x33;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09; // USAGE (Ry) -- right trigger
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x34;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95; // REPORT_COUNT (6)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x06;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81; // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // Bytes 18-62 (IMU/touchpad/reserved): unused, pad them out
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15; // LOGICAL_MINIMUM (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26; // LOGICAL_MAXIMUM (255)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75; // REPORT_SIZE (8)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95; // REPORT_COUNT (45)
    tempHidReportDescriptor[hidReportDescriptorSize++] = SINPUT_REPORT_LEN_INPUT - 2 - 4 - 12;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81; // INPUT (Const,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;

    // Input Report 0x02 (command/feature response) and Output Report 0x03
    // (host -> device commands): plain opaque blobs, no field-level usages.
    // Only SDL's SInput driver (or an app speaking the same protocol) ever
    // touches these -- see BleSInput.h -- so there's nothing for the kernel's
    // generic HID input mapping to usefully do with them either way.
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15; // LOGICAL_MINIMUM (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26; // LOGICAL_MAXIMUM (255)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75; // REPORT_SIZE (8)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x85; // REPORT_ID
    tempHidReportDescriptor[hidReportDescriptorSize++] = SINPUT_REPORT_ID_INPUT_CMDDAT;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95; // REPORT_COUNT
    tempHidReportDescriptor[hidReportDescriptorSize++] = SINPUT_REPORT_LEN_INPUT;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81; // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x85; // REPORT_ID
    tempHidReportDescriptor[hidReportDescriptorSize++] = SINPUT_REPORT_ID_OUTPUT_CMDDAT;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95; // REPORT_COUNT
    tempHidReportDescriptor[hidReportDescriptorSize++] = SINPUT_REPORT_LEN_OUTPUT;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x91; // OUTPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // END_COLLECTION (Application)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
  }
  else
  {

  // USAGE_PAGE (Generic Desktop)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

  // USAGE (Joystick - 0x04; Gamepad - 0x05; Multi-axis Controller - 0x08)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
  tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getControllerType();

  // COLLECTION (Application)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0xa1;
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

  // REPORT_ID (Default: 3)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0x85;
  tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getHidReportId();

  if (configuration.getButtonCount() > 0)
  {
    // USAGE_PAGE (Button)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;

    // LOGICAL_MINIMUM (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // LOGICAL_MAXIMUM (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x25;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // REPORT_SIZE (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE_MINIMUM (Button 1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x19;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE_MAXIMUM (Up to 128 buttons possible)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x29;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getButtonCount();

    // REPORT_COUNT (# of buttons)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getButtonCount();

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    if (buttonPaddingBits > 0)
    {

      // REPORT_SIZE (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // REPORT_COUNT (# of padding bits)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = buttonPaddingBits;

      // INPUT (Const,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;

    } // Padding Bits Needed

  } // Buttons

  if (configuration.getTotalSpecialButtonCount() > 0)
  {
    // LOGICAL_MINIMUM (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // LOGICAL_MAXIMUM (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x25;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // REPORT_SIZE (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    if (configuration.getDesktopSpecialButtonCount() > 0)
    {

      // USAGE_PAGE (Generic Desktop)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // REPORT_COUNT
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getDesktopSpecialButtonCount();

      if (configuration.getIncludeStart())
      {
        // USAGE (Start)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x3D;
      }

      if (configuration.getIncludeSelect())
      {
        // USAGE (Select)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x3E;
      }

      if (configuration.getIncludeMenu())
      {
        // USAGE (App Menu)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x86;
      }

      // INPUT (Data,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
    }

    if (configuration.getConsumerSpecialButtonCount() > 0)
    {

      // USAGE_PAGE (Consumer Page)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x0C;

      // REPORT_COUNT
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getConsumerSpecialButtonCount();

      if (configuration.getIncludeHome())
      {
        // USAGE (Home)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x0A;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x23;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
      }

      if (configuration.getIncludeBack())
      {
        // USAGE (Back)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x0A;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x24;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
      }

      if (configuration.getIncludeVolumeInc())
      {
        // USAGE (Volume Increment)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0xE9;
      }

      if (configuration.getIncludeVolumeDec())
      {
        // USAGE (Volume Decrement)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0xEA;
      }

      if (configuration.getIncludeVolumeMute())
      {
        // USAGE (Mute)
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
        tempHidReportDescriptor[hidReportDescriptorSize++] = 0xE2;
      }

      // INPUT (Data,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
    }

    if (specialButtonPaddingBits > 0)
    {

      // REPORT_SIZE (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // REPORT_COUNT (# of padding bits)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = specialButtonPaddingBits;

      // INPUT (Const,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;

    } // Padding Bits Needed

  } // Special Buttons

  if (configuration.getAxisCount() > 0)
  {
    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE (Pointer)
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getAxesMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getAxesMin());
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;    // Use these two lines for 0 min
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;    // Use these two lines for -32767 min
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x80;

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getAxesMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getAxesMax());
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;	// Use these two lines for 255 max
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;	// Use these two lines for +32767 max
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x7F;

    // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

    // REPORT_COUNT (configuration.getAxisCount() - sliders)
    uint8_t sliders = (configuration.getIncludeSlider1() ? 1 : 0) + (configuration.getIncludeSlider2() ? 1 : 0);
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getAxisCount() - sliders;

    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    if (configuration.getIncludeXAxis())
    {
      // USAGE (X)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x30;
    }

    if (configuration.getIncludeYAxis())
    {
      // USAGE (Y)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x31;
    }

    if (configuration.getIncludeZAxis())
    {
      // USAGE (Z)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x32;
    }

    if (configuration.getIncludeRzAxis())
    {
      // USAGE (Rz)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x35;
    }

    if (configuration.getIncludeRxAxis())
    {
      // USAGE (Rx)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x33;
    }

    if (configuration.getIncludeRyAxis())
    {
      // USAGE (Ry)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x34;
    }

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    if (configuration.getIncludeSlider1())
    {
      // REPORT_COUNT (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // COLLECTION (Logical)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

      // USAGE (Slider)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x36;

      // INPUT (Data,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

      // END_COLLECTION (Logical)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC0;
    }

    if (configuration.getIncludeSlider2())
    {
      // REPORT_COUNT (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // COLLECTION (Logical)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

      // USAGE (Slider)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x36;

      // INPUT (Data,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

      // END_COLLECTION (Logical)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC0;
    }

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;

  } // X, Y, Z, Rx, Ry, and Rz Axis

  if (configuration.getSimulationCount() > 0)
  {

    // USAGE_PAGE (Simulation Controls)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getSimulationMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getSimulationMin());
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;	    // Use these two lines for 0 min
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;	    // Use these two lines for -32767 min
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x80;

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getSimulationMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getSimulationMax());
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;	    // Use these two lines for 255 max
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;	    // Use these two lines for +32767 max
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x7F;

    // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

    // REPORT_COUNT (configuration.getSimulationCount())
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getSimulationCount();

    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    if (configuration.getIncludeRudder())
    {
      // USAGE (Rudder)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xBA;
    }

    if (configuration.getIncludeThrottle())
    {
      // USAGE (Throttle)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xBB;
    }

    if (configuration.getIncludeAccelerator())
    {
      // USAGE (Accelerator)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC4;
    }

    if (configuration.getIncludeBrake())
    {
      // USAGE (Brake)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC5;
    }

    if (configuration.getIncludeSteering())
    {
      // USAGE (Steering)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC8;
    }

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;

  } // Simulation Controls
  
  
  // Gyroscope
  if (configuration.getIncludeGyroscope())
  {
    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    
    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
    
    // USAGE (Gyroscope - Rotational X - Rx)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x33;
    
    // USAGE (Rotational - Rotational Y - Ry)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x34;
    
    // USAGE (Rotational - Rotational Z - Rz)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x35;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMin());
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;	    // Use these two lines for 0 min
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;	    // Use these two lines for -32767 min
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x80;

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMax());
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;	    // Use these two lines for 255 max
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;	    // Use these two lines for +32767 max
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x7F;

    // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

    // REPORT_COUNT (3)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
    
  } //Gyroscope
  
  // Accelerometer
  if (configuration.getIncludeAccelerometer())
  {
    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    
    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
    
    // USAGE (Accelerometer - Vector X - Vx)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x40;
    
    // USAGE (Accelerometer - Vector Y - Vy)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x41;
    
    // USAGE (Accelerometer - Vector Z - Vz)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x42;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMin());
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;	    // Use these two lines for 0 min
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;	    // Use these two lines for -32767 min
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x80;

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMax());
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;	    // Use these two lines for 255 max
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;	    // Use these two lines for +32767 max
    //tempHidReportDescriptor[hidReportDescriptorSize++] = 0x7F;

    // REPORT_SIZE (16)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

    // REPORT_COUNT (3)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;

    // INPUT (Data,Var,Abs)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
    
  } //Accelerometer

  if (configuration.getHatSwitchCount() > 0)
  {

    // COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = USAGE_PAGE(1);
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE (Hat Switch)
    for (int currentHatIndex = 0; currentHatIndex < configuration.getHatSwitchCount(); currentHatIndex++)
    {
      tempHidReportDescriptor[hidReportDescriptorSize++] = USAGE(1);
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x39;
    }

    // Logical Min (1)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Logical Max (8)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x25;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    // Physical Min (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x35;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Physical Max (315)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x46;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x3B;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Unit (SI Rot : Ang Pos)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x65;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x12;

    // Report Size (8)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    // Report Count (4)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
    tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getHatSwitchCount();

    // Input (Data, Variable, Absolute)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x42;

    // END_COLLECTION (Physical)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
  } // Hat Switches


  if (configuration.getEnableOutputReport())
  {
    // Usage Page (Vendor Defined 0xFF00)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x06;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;

    // Usage (Vendor Usage 0x01)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Usage (0x01)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Logical Minimum (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Logical Maximum (255)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Report Size (8 bits)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    if (configuration.getOutputReportLength() <= 0xFF)
    {
      // Report Count (0~255 bytes)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getOutputReportLength();
    }
    else
    {
      // Report Count (0~65535 bytes)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x96;
      tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getOutputReportLength());
      tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getOutputReportLength());
    }

    // Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x91;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
  }

  if (configuration.getEnableFeatureReport())
  {
    // Vendor blob feature report
    // Usage Page (Vendor Defined 0xFF00)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x06;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;

    // Usage (Vendor Usage 0x01)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Usage (0x01)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // Logical Minimum (0)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Logical Maximum (255)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

    // Report Size (8 bits)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x08;

    if (configuration.getFeatureReportLength() <= 0xFF)
    {
      // Report Count (0~255 bytes)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = configuration.getFeatureReportLength();
    }
    else
    {
      // Report Count (0~65535 bytes)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x96;
      tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getFeatureReportLength());
      tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getFeatureReportLength());
    }

    // Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xB1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
  }

  // END_COLLECTION (Application)
  tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;

  } // enableSInput

  // Set task priority from 5 to 1 in order to get ESP32-C3 working
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 1, NULL);
}

void BleGamepad::end(void)
{
}

void BleGamepad::setAxes(int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ, int16_t slider1, int16_t slider2)
{
  if (x == -32768)
  {
    x = -32767;
  }
  if (y == -32768)
  {
    y = -32767;
  }
  if (z == -32768)
  {
    z = -32767;
  }
  if (rZ == -32768)
  {
    rZ = -32767;
  }
  if (rX == -32768)
  {
    rX = -32767;
  }
  if (rY == -32768)
  {
    rY = -32767;
  }
  if (slider1 == -32768)
  {
    slider1 = -32767;
  }
  if (slider2 == -32768)
  {
    slider2 = -32767;
  }

  _x = x;
  _y = y;
  _z = z;
  _rZ = rZ;
  _rX = rX;
  _rY = rY;
  _slider1 = slider1;
  _slider2 = slider2;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setHIDAxes(int16_t x, int16_t y, int16_t z, int16_t rZ, int16_t rX, int16_t rY, int16_t slider1, int16_t slider2)
{
  if (x == -32768)
  {
    x = -32767;
  }
  if (y == -32768)
  {
    y = -32767;
  }
  if (z == -32768)
  {
    z = -32767;
  }
  if (rZ == -32768)
  {
    rZ = -32767;
  }
  if (rX == -32768)
  {
    rX = -32767;
  }
  if (rY == -32768)
  {
    rY = -32767;
  }
  if (slider1 == -32768)
  {
    slider1 = -32767;
  }
  if (slider2 == -32768)
  {
    slider2 = -32767;
  }

  _x = x;
  _y = y;
  _z = z;
  _rZ = rZ;
  _rX = rX;
  _rY = rY;
  _slider1 = slider1;
  _slider2 = slider2;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSimulationControls(int16_t rudder, int16_t throttle, int16_t accelerator, int16_t brake, int16_t steering)
{
  if (rudder == -32768)
  {
    rudder = -32767;
  }
  if (throttle == -32768)
  {
    throttle = -32767;
  }
  if (accelerator == -32768)
  {
    accelerator = -32767;
  }
  if (brake == -32768)
  {
    brake = -32767;
  }
  if (steering == -32768)
  {
    steering = -32767;
  }

  _rudder = rudder;
  _throttle = throttle;
  _accelerator = accelerator;
  _brake = brake;
  _steering = steering;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setHats(signed char hat1, signed char hat2, signed char hat3, signed char hat4)
{
  _hat1 = hat1;
  _hat2 = hat2;
  _hat3 = hat3;
  _hat4 = hat4;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSliders(int16_t slider1, int16_t slider2)
{
  if (slider1 == -32768)
  {
    slider1 = -32767;
  }
  if (slider2 == -32768)
  {
    slider2 = -32767;
  }

  _slider1 = slider1;
  _slider2 = slider2;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::sendReport(void)
{
  if (this->isConnected() && enableSInput)
  {
    uint8_t m[SINPUT_REPORT_LEN_INPUT];
    memset(m, 0, sizeof(m));

    // Derived from the same state setBatteryLevel()/setBatteryPowerInformation()/
    // setChargingState()/setDischargingState() already drive for the standard BLE
    // Battery Service (0x180F) -- SInput doesn't read that GATT service (see
    // GattVsHid.md), so it needs its own copy of the same information here.
    // There's no dedicated signal in this library for "finished charging", so
    // SINPUT_PLUG_STATUS_CHARGED is never produced; a still-connected charger
    // that's stopped topping up reports as SINPUT_PLUG_STATUS_CHARGING.
    uint8_t plugStatus = SINPUT_PLUG_STATUS_UNKNOWN;
    if (_chargingState == POWER_STATE_CHARGING)
    {
      plugStatus = SINPUT_PLUG_STATUS_CHARGING;
    }
    else if (_batteryPowerInformation == POWER_STATE_NOT_PRESENT || _batteryPowerInformation == POWER_STATE_NOT_SUPPORTED)
    {
      plugStatus = SINPUT_PLUG_STATUS_NO_BATTERY;
    }
    else if (_dischargingState == POWER_STATE_DISCHARGING)
    {
      plugStatus = SINPUT_PLUG_STATUS_ON_BATTERY;
    }
    m[SINPUT_IN_IDX_PLUG_STATUS] = plugStatus;
    m[SINPUT_IN_IDX_CHARGE_LEVEL] = batteryLevel;

    uint8_t buttons0 = 0;
    if (configuration.getButtonCount() >= 1 && isPressed(BUTTON_1)) buttons0 |= SINPUT_BTN0_SOUTH;
    if (configuration.getButtonCount() >= 2 && isPressed(BUTTON_2)) buttons0 |= SINPUT_BTN0_EAST;
    if (configuration.getButtonCount() >= 3 && isPressed(BUTTON_3)) buttons0 |= SINPUT_BTN0_WEST;
    if (configuration.getButtonCount() >= 4 && isPressed(BUTTON_4)) buttons0 |= SINPUT_BTN0_NORTH;
    if (configuration.getHatSwitchCount() >= 1)
    {
      if (_hat1 == HAT_UP || _hat1 == HAT_UP_RIGHT || _hat1 == HAT_UP_LEFT) buttons0 |= SINPUT_BTN0_DUP;
      if (_hat1 == HAT_DOWN_RIGHT || _hat1 == HAT_DOWN || _hat1 == HAT_DOWN_LEFT) buttons0 |= SINPUT_BTN0_DDOWN;
      if (_hat1 == HAT_DOWN_LEFT || _hat1 == HAT_LEFT || _hat1 == HAT_UP_LEFT) buttons0 |= SINPUT_BTN0_DLEFT;
      if (_hat1 == HAT_UP_RIGHT || _hat1 == HAT_RIGHT || _hat1 == HAT_DOWN_RIGHT) buttons0 |= SINPUT_BTN0_DRIGHT;
    }
    m[SINPUT_IN_IDX_BUTTONS_0] = buttons0;

    uint8_t buttons1 = 0;
    if (configuration.getButtonCount() >= 5 && isPressed(BUTTON_5)) buttons1 |= SINPUT_BTN1_LSHOULDER;
    if (configuration.getButtonCount() >= 6 && isPressed(BUTTON_6)) buttons1 |= SINPUT_BTN1_RSHOULDER;
    m[SINPUT_IN_IDX_BUTTONS_1] = buttons1;
    // m[SINPUT_IN_IDX_BUTTONS_2]/_3 (Start/Back/Guide/..., Power/Misc) stay 0 --
    // see BleSInputReceiver::sendFeaturesResponse() for why.

    if (configuration.getIncludeXAxis() && configuration.getIncludeYAxis())
    {
      m[SINPUT_IN_IDX_LEFT_X] = (uint8_t)_x;
      m[SINPUT_IN_IDX_LEFT_X + 1] = (uint8_t)(_x >> 8);
      m[SINPUT_IN_IDX_LEFT_Y] = (uint8_t)_y;
      m[SINPUT_IN_IDX_LEFT_Y + 1] = (uint8_t)(_y >> 8);
    }
    if (configuration.getIncludeZAxis() && configuration.getIncludeRzAxis())
    {
      m[SINPUT_IN_IDX_RIGHT_X] = (uint8_t)_z;
      m[SINPUT_IN_IDX_RIGHT_X + 1] = (uint8_t)(_z >> 8);
      m[SINPUT_IN_IDX_RIGHT_Y] = (uint8_t)_rZ;
      m[SINPUT_IN_IDX_RIGHT_Y + 1] = (uint8_t)(_rZ >> 8);
    }
    if (configuration.getIncludeRxAxis())
    {
      m[SINPUT_IN_IDX_LEFT_TRIGGER] = (uint8_t)_rX;
      m[SINPUT_IN_IDX_LEFT_TRIGGER + 1] = (uint8_t)(_rX >> 8);
    }
    if (configuration.getIncludeRyAxis())
    {
      m[SINPUT_IN_IDX_RIGHT_TRIGGER] = (uint8_t)_rY;
      m[SINPUT_IN_IDX_RIGHT_TRIGGER + 1] = (uint8_t)(_rY >> 8);
    }
    // Bytes past here (IMU, touchpads, reserved) stay 0 -- see BleSInput.h.

    this->sInputGamepad->setValue(m, sizeof(m));
    this->sInputGamepad->notify();
    return;
  }

  if (this->isConnected())
  {
    uint8_t currentReportIndex = 0;

    uint8_t m[hidReportSize];

    memset(&m, 0, sizeof(m));
    memcpy(&m, &_buttons, sizeof(_buttons));

    currentReportIndex += numOfButtonBytes;

    if (configuration.getTotalSpecialButtonCount() > 0)
    {
      m[currentReportIndex++] = _specialButtons;
    }

    if (configuration.getIncludeXAxis())
    {
      m[currentReportIndex++] = _x;
      m[currentReportIndex++] = (_x >> 8);
    }
    if (configuration.getIncludeYAxis())
    {
      m[currentReportIndex++] = _y;
      m[currentReportIndex++] = (_y >> 8);
    }
    if (configuration.getIncludeZAxis())
    {
      m[currentReportIndex++] = _z;
      m[currentReportIndex++] = (_z >> 8);
    }
    if (configuration.getIncludeRzAxis())
    {
      m[currentReportIndex++] = _rZ;
      m[currentReportIndex++] = (_rZ >> 8);
    }
    if (configuration.getIncludeRxAxis())
    {
      m[currentReportIndex++] = _rX;
      m[currentReportIndex++] = (_rX >> 8);
    }
    if (configuration.getIncludeRyAxis())
    {
      m[currentReportIndex++] = _rY;
      m[currentReportIndex++] = (_rY >> 8);
    }

    if (configuration.getIncludeSlider1())
    {
      m[currentReportIndex++] = _slider1;
      m[currentReportIndex++] = (_slider1 >> 8);
    }
    if (configuration.getIncludeSlider2())
    {
      m[currentReportIndex++] = _slider2;
      m[currentReportIndex++] = (_slider2 >> 8);
    }

    if (configuration.getIncludeRudder())
    {
      m[currentReportIndex++] = _rudder;
      m[currentReportIndex++] = (_rudder >> 8);
    }
    if (configuration.getIncludeThrottle())
    {
      m[currentReportIndex++] = _throttle;
      m[currentReportIndex++] = (_throttle >> 8);
    }
    if (configuration.getIncludeAccelerator())
    {
      m[currentReportIndex++] = _accelerator;
      m[currentReportIndex++] = (_accelerator >> 8);
    }
    if (configuration.getIncludeBrake())
    {
      m[currentReportIndex++] = _brake;
      m[currentReportIndex++] = (_brake >> 8);
    }
    if (configuration.getIncludeSteering())
    {
      m[currentReportIndex++] = _steering;
      m[currentReportIndex++] = (_steering >> 8);
    }

    if (configuration.getIncludeGyroscope())
    {
      m[currentReportIndex++] = _gX;
      m[currentReportIndex++] = (_gX >> 8);
      m[currentReportIndex++] = _gY;
      m[currentReportIndex++] = (_gY >> 8);
      m[currentReportIndex++] = _gZ;
      m[currentReportIndex++] = (_gZ >> 8);
    }
    
    if (configuration.getIncludeAccelerometer())
    {
      m[currentReportIndex++] = _aX;
      m[currentReportIndex++] = (_aX >> 8);
      m[currentReportIndex++] = _aY;
      m[currentReportIndex++] = (_aY >> 8);
      m[currentReportIndex++] = _aZ;
      m[currentReportIndex++] = (_aZ >> 8);
    }
    
    if (configuration.getHatSwitchCount() > 0)
    {
      signed char hats[4];

      hats[0] = _hat1;
      hats[1] = _hat2;
      hats[2] = _hat3;
      hats[3] = _hat4;

      for (int currentHatIndex = configuration.getHatSwitchCount() - 1; currentHatIndex >= 0; currentHatIndex--)
      {
        m[currentReportIndex++] = hats[currentHatIndex];
      }
    }
  
    #if BLE_GAMEPAD_DEBUG == 1
      dumpHIDReport(m, sizeof(m));
    #endif

    this->inputGamepad->setValue(m, sizeof(m));
    this->inputGamepad->notify();
  }
}

void BleGamepad::press(uint8_t b)
{
  uint8_t index = (b - 1) / 8;
  uint8_t bit = (b - 1) % 8;
  uint8_t bitmask = (1 << bit);

  uint8_t result = _buttons[index] | bitmask;

  if (result != _buttons[index])
  {
    _buttons[index] = result;
  }

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::release(uint8_t b)
{
  uint8_t index = (b - 1) / 8;
  uint8_t bit = (b - 1) % 8;
  uint8_t bitmask = (1 << bit);

  uint64_t result = _buttons[index] & ~bitmask;

  if (result != _buttons[index])
  {
    _buttons[index] = result;
  }

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

uint8_t BleGamepad::specialButtonBitPosition(uint8_t b)
{
  uint8_t bit = 0;
  
  if (b >= POSSIBLESPECIALBUTTONS)
  {
    // Just return 0 if the button bit is out of range
    return bit;
  }
  else
  {
    for (int i = 0; i < b; i++)
    {
      if (configuration.getWhichSpecialButtons()[i])
        bit++;
    }
    return bit;
  }  
}

void BleGamepad::pressSpecialButton(uint8_t b)
{
  uint8_t button = specialButtonBitPosition(b);
  uint8_t bit = button % 8;
  uint8_t bitmask = (1 << bit);

  uint64_t result = _specialButtons | bitmask;

  if (result != _specialButtons)
  {
    _specialButtons = result;
  }

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::releaseSpecialButton(uint8_t b)
{
  uint8_t button = specialButtonBitPosition(b);
  uint8_t bit = button % 8;
  uint8_t bitmask = (1 << bit);

  uint64_t result = _specialButtons & ~bitmask;

  if (result != _specialButtons)
  {
    _specialButtons = result;
  }

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::pressStart()
{
  pressSpecialButton(START_BUTTON);
}

void BleGamepad::releaseStart()
{
  releaseSpecialButton(START_BUTTON);
}

void BleGamepad::pressSelect()
{
  pressSpecialButton(SELECT_BUTTON);
}

void BleGamepad::releaseSelect()
{
  releaseSpecialButton(SELECT_BUTTON);
}

void BleGamepad::pressMenu()
{
  pressSpecialButton(MENU_BUTTON);
}

void BleGamepad::releaseMenu()
{
  releaseSpecialButton(MENU_BUTTON);
}

void BleGamepad::pressHome()
{
  pressSpecialButton(HOME_BUTTON);
}

void BleGamepad::releaseHome()
{
  releaseSpecialButton(HOME_BUTTON);
}

void BleGamepad::pressBack()
{
  pressSpecialButton(BACK_BUTTON);
}

void BleGamepad::releaseBack()
{
  releaseSpecialButton(BACK_BUTTON);
}

void BleGamepad::pressVolumeInc()
{
  pressSpecialButton(VOLUME_INC_BUTTON);
}

void BleGamepad::releaseVolumeInc()
{
  releaseSpecialButton(VOLUME_INC_BUTTON);
}

void BleGamepad::pressVolumeDec()
{
  pressSpecialButton(VOLUME_DEC_BUTTON);
}

void BleGamepad::releaseVolumeDec()
{
  releaseSpecialButton(VOLUME_DEC_BUTTON);
}

void BleGamepad::pressVolumeMute()
{
  pressSpecialButton(VOLUME_MUTE_BUTTON);
}

void BleGamepad::releaseVolumeMute()
{
  releaseSpecialButton(VOLUME_MUTE_BUTTON);
}

void BleGamepad::setLeftThumb(int16_t x, int16_t y)
{
  if (x == -32768)
  {
    x = -32767;
  }
  if (y == -32768)
  {
    y = -32767;
  }

  _x = x;
  _y = y;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRightThumb(int16_t z, int16_t rZ)
{
  if (z == -32768)
  {
    z = -32767;
  }
  if (rZ == -32768)
  {
    rZ = -32767;
  }

  _z = z;
  _rZ = rZ;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRightThumbAndroid(int16_t z, int16_t rX)
{
  if (z == -32768)
  {
    z = -32767;
  }
  if (rX == -32768)
  {
    rX = -32767;
  }

  _z = z;
  _rX = rX;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setLeftTrigger(int16_t rX)
{
  if (rX == -32768)
  {
    rX = -32767;
  }

  _rX = rX;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRightTrigger(int16_t rY)
{
  if (rY == -32768)
  {
    rY = -32767;
  }

  _rY = rY;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setTriggers(int16_t rX, int16_t rY)
{
  if (rX == -32768)
  {
    rX = -32767;
  }
  if (rY == -32768)
  {
    rY = -32767;
  }

  _rX = rX;
  _rY = rY;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setHat(signed char hat)
{
  _hat1 = hat;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setHat1(signed char hat1)
{
  _hat1 = hat1;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setHat2(signed char hat2)
{
  _hat2 = hat2;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setHat3(signed char hat3)
{
  _hat3 = hat3;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setHat4(signed char hat4)
{
  _hat4 = hat4;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setX(int16_t x)
{
  if (x == -32768)
  {
    x = -32767;
  }

  _x = x;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setY(int16_t y)
{
  if (y == -32768)
  {
    y = -32767;
  }

  _y = y;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setZ(int16_t z)
{
  if (z == -32768)
  {
    z = -32767;
  }

  _z = z;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRZ(int16_t rZ)
{
  if (rZ == -32768)
  {
    rZ = -32767;
  }

  _rZ = rZ;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRX(int16_t rX)
{
  if (rX == -32768)
  {
    rX = -32767;
  }

  _rX = rX;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRY(int16_t rY)
{
  if (rY == -32768)
  {
    rY = -32767;
  }

  _rY = rY;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSlider(int16_t slider)
{
  if (slider == -32768)
  {
    slider = -32767;
  }

  _slider1 = slider;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSlider1(int16_t slider1)
{
  if (slider1 == -32768)
  {
    slider1 = -32767;
  }

  _slider1 = slider1;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSlider2(int16_t slider2)
{
  if (slider2 == -32768)
  {
    slider2 = -32767;
  }

  _slider2 = slider2;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRudder(int16_t rudder)
{
  if (rudder == -32768)
  {
    rudder = -32767;
  }

  _rudder = rudder;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setThrottle(int16_t throttle)
{
  if (throttle == -32768)
  {
    throttle = -32767;
  }

  _throttle = throttle;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setAccelerator(int16_t accelerator)
{
  if (accelerator == -32768)
  {
    accelerator = -32767;
  }

  _accelerator = accelerator;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setBrake(int16_t brake)
{
  if (brake == -32768)
  {
    brake = -32767;
  }

  _brake = brake;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSteering(int16_t steering)
{
  if (steering == -32768)
  {
    steering = -32767;
  }

  _steering = steering;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

bool BleGamepad::isPressed(uint8_t b)
{
  uint8_t index = (b - 1) / 8;
  uint8_t bit = (b - 1) % 8;
  uint8_t bitmask = (1 << bit);

  if ((bitmask & _buttons[index]) > 0)
    return true;
  return false;
}

bool BleGamepad::isConnected(void)
{
  return this->connectionStatus->connected;
}

void BleGamepad::setBatteryLevel(uint8_t level)
{
  this->batteryLevel = level;
  if (hid != 0)
  {

    this->hid->setBatteryLevel(this->batteryLevel, this->isConnected() ? true : false);

    if (configuration.getAutoReport())
    {
      sendReport();
    }
  }
}

bool BleGamepad::isOutputReceived()
{
  if (enableOutputReport && outputReceiver)
  {
    if (this->outputReceiver->outputFlag)
    {
      this->outputReceiver->outputFlag = false; // Clear Flag
      return true;
    }
  }
  return false;
}

uint8_t* BleGamepad::getOutputBuffer()
{
  if (enableOutputReport && outputReceiver)
  {
    memcpy(outputBackupBuffer, outputReceiver->outputBuffer, outputReportLength); // Creating a backup to avoid buffer being overwritten while processing data
    return outputBackupBuffer;
  }
  return nullptr;
}

bool BleGamepad::isFeatureReceived()
{
  if (enableFeatureReport && featureReceiver)
  {
    if (this->featureReceiver->featureFlag)
    {
      this->featureReceiver->featureFlag = false; // Clear Flag
      return true;
    }
  }
  return false;
}

uint8_t* BleGamepad::getFeatureBuffer()
{
  if (enableFeatureReport && featureReceiver)
  {
    memcpy(featureBackupBuffer, featureReceiver->featureBuffer, featureReportLength); // Creating a backup to avoid buffer being overwritten while processing data
    return featureBackupBuffer;
  }
  return nullptr;
}

void BleGamepad::setFeatureBuffer(const uint8_t* data, uint16_t length)
{
  if (enableFeatureReport && featureReceiver)
  {
    uint16_t copyLength = length < featureReportLength ? length : featureReportLength;
    memcpy(featureReceiver->featureBuffer, data, copyLength);
  }
}

bool BleGamepad::isPlayerLedReceived()
{
  if (enableSInput && sInputReceiver)
  {
    if (this->sInputReceiver->playerLedFlag)
    {
      this->sInputReceiver->playerLedFlag = false; // Clear Flag
      return true;
    }
  }
  return false;
}

uint8_t BleGamepad::getPlayerLedIndex()
{
  if (enableSInput && sInputReceiver)
  {
    return sInputReceiver->playerLedIndex;
  }
  return 0;
}

bool BleGamepad::deleteAllBonds(bool resetBoard)
{
  bool success = false;

  NimBLEDevice::deleteAllBonds();
  NIMBLE_LOGD(LOG_TAG, "deleteAllBonds - All bonds deleted");
  success = true;
  delay(500);

  if (resetBoard)
  {
    NIMBLE_LOGD(LOG_TAG, "deleteAllBonds - Reboot ESP32");
    ESP.restart();
  }

  return success;	// Returns false if all bonds are not deleted
}

bool BleGamepad::deleteBond(bool resetBoard)
{
  bool success = false;

  NimBLEServer* server = NimBLEDevice::getServer();

  if (server)
  {
    NimBLEConnInfo info = server->getPeerInfo(0);
    NimBLEAddress address = info.getAddress();

    success = NimBLEDevice::deleteBond(address);
    NIMBLE_LOGD(LOG_TAG, "deleteBond - Bond for %s deleted", std::string(address).c_str());

    delay(500);

    if (resetBoard)
    {
      NIMBLE_LOGD(LOG_TAG, "deleteBond - Reboot ESP32");
      ESP.restart();
    }
  }
  return success;	// Returns false if current bond is not deleted
}

bool BleGamepad::enterPairingMode()
{
  NimBLEServer* server = NimBLEDevice::getServer();

  if (server)
  {
    NIMBLE_LOGD(LOG_TAG, "enterPairingMode - Pairing mode entered");

    // Get current connection information and address
    NimBLEConnInfo currentConnInfo = server->getPeerInfo(0);
    NimBLEAddress currentAddress = currentConnInfo.getAddress();
    NIMBLE_LOGD(LOG_TAG, "enterPairingMode - Connected Address: %s", std::string(currentAddress).c_str());

    // Disconnect from current connection
    for (uint16_t connHandle : server->getPeerDevices())
    {
      server->disconnect(connHandle); // Disconnect the client
      NIMBLE_LOGD(LOG_TAG, "enterPairingMode - Disconnected from client");
      delay(500);
    }

    bool connectedToOldDevice = true;

    // While connected to old device, keep allowing to connect new new devices
    NIMBLE_LOGD(LOG_TAG, "enterPairingMode - Advertising for clients...");
    while (connectedToOldDevice)
    {
      delay(10);	// Needs a delay to work - do not remove!

      if (this->isConnected())
      {
        NimBLEConnInfo newConnInfo = server->getPeerInfo(0);
        NimBLEAddress newAddress = newConnInfo.getAddress();

        // Block specific MAC address
        if (newAddress == currentAddress)
        {
          NIMBLE_LOGD(LOG_TAG, "enterPairingMode - Connected to previous client, so disconnect and continue advertising for new client");
          server->disconnect(newConnInfo.getConnHandle());
          delay(500);
        }
        else
        {
          NIMBLE_LOGD(LOG_TAG, "enterPairingMode - Connected to new client");
          NIMBLE_LOGD(LOG_TAG, "enterPairingMode - Exit pairing mode");
          connectedToOldDevice = false;
          return true;
        }
      }
    }
    return false; // Might want to adjust this function to stay in pairing mode for a while, and then return false after a while if no other device pairs with it
  }
  return false;
}

NimBLEAddress BleGamepad::getAddress()
{
  NimBLEServer* server = NimBLEDevice::getServer();

  if (server)
  {
    // Get current connection information and address
    NimBLEConnInfo currentConnInfo = server->getPeerInfo(0);
    NimBLEAddress currentAddress = currentConnInfo.getAddress();
    return currentAddress;
  }
  NimBLEAddress blankAddress("00:00:00:00:00:00", 0);
  return blankAddress;
}

String BleGamepad::getStringAddress()
{
  NimBLEServer* server = NimBLEDevice::getServer();

  if (server)
  {
    // Get current connection information and address
    NimBLEConnInfo currentConnInfo = server->getPeerInfo(0);
    NimBLEAddress currentAddress = currentConnInfo.getAddress();
    return currentAddress.toString().c_str();
  }
  NimBLEAddress blankAddress("00:00:00:00:00:00", 0);
  return blankAddress.toString().c_str();
}

NimBLEConnInfo BleGamepad::getPeerInfo()
{
  NimBLEServer* server = NimBLEDevice::getServer();
  NimBLEConnInfo currentConnInfo = server->getPeerInfo(0);
  return currentConnInfo;
}

String BleGamepad::getDeviceName()
{
  return this->deviceName.c_str();
}

String BleGamepad::getDeviceManufacturer()
{
  return this->deviceManufacturer.c_str();
}

int8_t BleGamepad::getTXPowerLevel()
{
  return NimBLEDevice::getPower();
}

void BleGamepad::setTXPowerLevel(int8_t level)
{
  NimBLEDevice::setPower(level);  // The only valid values are: -12, -9, -6, -3, 0, 3, 6 and 9
  configuration.setTXPowerLevel(level);
}

void BleGamepad::setGyroscope(int16_t gX, int16_t gY, int16_t gZ)
{
  if (gX == -32768)
  {
    gX = -32767;
  }
  
  if (gY == -32768)
  {
    gY = -32767;
  }
  
  if (gY == -32768)
  {
    gY = -32767;
  }
  
  _gX = gX;
  _gY = gY;
  _gZ = gZ; 
  
  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setAccelerometer(int16_t aX, int16_t aY, int16_t aZ)
{
  _aX = aX;
  _aY = aY;
  _aZ = aZ;
  
  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setMotionControls(int16_t gX, int16_t gY, int16_t gZ, int16_t aX, int16_t aY, int16_t aZ)
{
  if (gX == -32768)
  {
    gX = -32767;
  }
  
  if (gY == -32768)
  {
    gY = -32767;
  }
  
  if (gZ == -32768)
  {
    gZ = -32767;
  }
  
  if (aX == -32768)
  {
    aX = -32767;
  }
  
  if (aY == -32768)
  {
    aY = -32767;
  }
  
  if (aZ == -32768)
  {
    aZ = -32767;
  }
  
  _gX = gX;
  _gY = gY;
  _gZ = gZ;
  _aX = aX;
  _aY = aY;
  _aZ = aZ; 
  
  if (configuration.getAutoReport())
  {
    sendReport();
  }
}
    
void BleGamepad::setPowerStateAll(uint8_t batteryPowerInformation, uint8_t dischargingState, uint8_t chargingState, uint8_t powerLevel)
{
    uint8_t powerStateBits = 0b00000000;
    
    _batteryPowerInformation = batteryPowerInformation;
    _dischargingState = dischargingState;
    _chargingState = chargingState;
    _powerLevel = powerLevel;

    // HID Battery Power State Bits:
    // Bits 0 and 1: Battery Power Information : 0(0b00) = Unknown, 1(0b01) = Not Supported,  2(0b10) = Not Present,               3(0b11) = Present
    // Bits 2 and 3: Discharging State         : 0(0b00) = Unknown, 1(0b01) = Not Supported,  2(0b10) = Not Discharging,           3(0b11) = Discharging
    // Bits 4 and 5: Charging State            : 0(0b00) = Unknown, 1(0b01) = Not Chargeable, 2(0b10) = Not Charging (Chargeable), 3(0b11) = Charging (Chargeable)
    // Bits 6 and 7: Power Level               : 0(0b00) = Unknown, 1(0b01) = Not Supported,  2(0b10) = Good Level,                3(0b11) = Critically Low Level

    powerStateBits |= (_batteryPowerInformation << 0);  // Populate first 2 bits with data
    powerStateBits |= (_dischargingState        << 2);  // Populate second 2 bits with data
    powerStateBits |= (_chargingState           << 4);  // Populate third 2 bits with data
    powerStateBits |= (_powerLevel              << 6);  // Populate last 2 bits with data

    if (this->pCharacteristic_Power_State) 
    {
      this->pCharacteristic_Power_State->setValue(&powerStateBits, 1);
      this->pCharacteristic_Power_State->notify();
    }
}


void BleGamepad::setBatteryPowerInformation(uint8_t batteryPowerInformation)
{
  _batteryPowerInformation = batteryPowerInformation;
  setPowerStateAll(_batteryPowerInformation, _dischargingState, _chargingState, _powerLevel);
}

void BleGamepad::setDischargingState(uint8_t dischargingState)
{
  _dischargingState = dischargingState;
  setPowerStateAll(_batteryPowerInformation, _dischargingState, _chargingState, _powerLevel);
}

void BleGamepad::setChargingState(uint8_t chargingState)
{
  _chargingState = chargingState;
  setPowerStateAll(_batteryPowerInformation, _dischargingState, _chargingState, _powerLevel);
}

void BleGamepad::setPowerLevel(uint8_t powerLevel)
{
  _powerLevel = powerLevel;
  setPowerStateAll(_batteryPowerInformation, _dischargingState, _chargingState, _powerLevel);
}

#if BLE_GAMEPAD_DEBUG == 1
static void dumpHidReportDescriptor(const uint8_t* desc, size_t size) {
    if (!Serial) {
        // Serial not initialized yet, avoid printing
        return;
    }

    if (desc == nullptr || size == 0) {
        Serial.println("[BLEGamepad][ERROR] HID Report Descriptor is null or empty!");
        return;
    }

    Serial.printf("[BLEGamepad][INFO] HID Report Descriptor size: %u bytes\n", (unsigned)size);
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) {
            Serial.printf("\n%03u: ", (unsigned)i);
        }
        Serial.printf("%02X ", desc[i]);
    }
    Serial.println("\n[BLEGamepad][INFO] End of HID Report Descriptor");

    Serial.printf("\n\nCopy start under here\n");
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) {
            Serial.printf("\n");
        }
        Serial.printf("%02X ", desc[i]);
    }
    Serial.println("\nCopy end above here ");
    Serial.println("\n\nCopy and paste the output above and use a parser such as at https://eleccelerator.com/usbdescreqparser to create a readable HID Report Descriptor\n\n");
}

static void dumpHIDReport(const uint8_t* report, size_t size)
{
    if (!Serial) {
        // Serial not initialized yet, avoid printing
        return;
    }

    Serial.printf("[BLEGamepad][INFO] HID Report Dump size: %u bytes\n", (unsigned)size);
    for (size_t i = 0; i < size; i++)
    {
        Serial.printf("%02X ", report[i]);
        // Optional: break line every 16 bytes
        if ((i + 1) % 16 == 0) Serial.println();
    }
    Serial.println();
    Serial.println("\n[BLEGamepad][INFO] End of HID Report Dump");
}
#endif

void BleGamepad::beginNUS() 
{
    if (!this->nusInitialized) 
    {
        // Extrememly important to make sure that the pointer to server is actually valid
        while(!NimBLEDevice::isInitialized ()){}        // Wait until the server is initialized
        while(NimBLEDevice::getServer() == nullptr){}   // Ensure pointer to server is actually valid
        
        // Now server is nkown to be valid, initialise nus to new BleNUS instance
        nus = new BleNUS(NimBLEDevice::getServer()); // Pass the existing BLE server
        nus->begin();
        nusInitialized = true;
    }
}

BleNUS* BleGamepad::getNUS() 
{
    return nus;  // Return a pointer instead of a reference
}

void BleGamepad::sendDataOverNUS(const uint8_t* data, size_t length) 
{
  if (nus) 
  {
    nus->sendData(data, length);
  }
}

void BleGamepad::setNUSDataReceivedCallback(void (*callback)(const uint8_t* data, size_t length)) 
{
  if (nus) 
  {
    nus->setDataReceivedCallback(callback);
  }
}

void BleGamepad::taskServer(void *pvParameter)
{
  BleGamepad *BleGamepadInstance = (BleGamepad *)pvParameter; // static_cast<BleGamepad *>(pvParameter);

  NimBLEDevice::init(BleGamepadInstance->deviceName);
  NimBLEDevice::setPower(BleGamepadInstance->configuration.getTXPowerLevel()); // Set transmit power for advertising (Range: -12 to +9 dBm)
  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(BleGamepadInstance->connectionStatus);
  pServer->advertiseOnDisconnect(true);

  BleGamepadInstance->hid = new NimBLEHIDDevice(pServer);

  if (BleGamepadInstance->enableSInput)
  {
    // SInput owns Report IDs 0x01-0x03 outright -- see the matching branch in
    // begin() and BleSInput.h -- so the classic Input/Output/Feature Report
    // characteristics below aren't created at all in this mode.
    BleGamepadInstance->sInputGamepad = BleGamepadInstance->hid->getInputReport(SINPUT_REPORT_ID_INPUT);
    BleGamepadInstance->connectionStatus->inputGamepad = BleGamepadInstance->sInputGamepad;
    BleGamepadInstance->sInputGamepad->setCallbacks(BleGamepadInstance->connectionStatus); // Logs which peer subscribes to the gamepad profile

    BleGamepadInstance->sInputCmdGamepad = BleGamepadInstance->hid->getInputReport(SINPUT_REPORT_ID_INPUT_CMDDAT);

    BleGamepadInstance->sInputReceiver = new BleSInputReceiver(&BleGamepadInstance->configuration, BleGamepadInstance->sInputCmdGamepad);
    BleGamepadInstance->sInputOutputGamepad = BleGamepadInstance->hid->getOutputReport(SINPUT_REPORT_ID_OUTPUT_CMDDAT);
    BleGamepadInstance->sInputOutputGamepad->setCallbacks(BleGamepadInstance->sInputReceiver);
  }
  else
  {
    BleGamepadInstance->inputGamepad = BleGamepadInstance->hid->getInputReport(BleGamepadInstance->configuration.getHidReportId()); // <-- input REPORTID from report map
    BleGamepadInstance->connectionStatus->inputGamepad = BleGamepadInstance->inputGamepad;
    BleGamepadInstance->inputGamepad->setCallbacks(BleGamepadInstance->connectionStatus); // Logs which peer subscribes to the gamepad profile

    if (BleGamepadInstance->enableOutputReport)
    {
      BleGamepadInstance->outputGamepad = BleGamepadInstance->hid->getOutputReport(BleGamepadInstance->configuration.getHidReportId());
      BleGamepadInstance->outputReceiver = new BleOutputReceiver(BleGamepadInstance->outputReportLength);
      BleGamepadInstance->outputBackupBuffer = new uint8_t[BleGamepadInstance->outputReportLength];
      BleGamepadInstance->outputGamepad->setCallbacks(BleGamepadInstance->outputReceiver);
    }

    if (BleGamepadInstance->enableFeatureReport)
    {
      BleGamepadInstance->featureGamepad = BleGamepadInstance->hid->getFeatureReport(BleGamepadInstance->configuration.getHidReportId());
      BleGamepadInstance->featureReceiver = new BleFeatureReceiver(BleGamepadInstance->featureReportLength, &BleGamepadInstance->configuration);
      BleGamepadInstance->featureBackupBuffer = new uint8_t[BleGamepadInstance->featureReportLength];
      BleGamepadInstance->featureGamepad->setCallbacks(BleGamepadInstance->featureReceiver);
    }
  }

  BleGamepadInstance->hid->setManufacturer(BleGamepadInstance->deviceManufacturer);

  NimBLEService *pService = pServer->getServiceByUUID(SERVICE_UUID_DEVICE_INFORMATION);

  BLECharacteristic* pCharacteristic_Model_Number = pService->createCharacteristic(
        CHARACTERISTIC_UUID_MODEL_NUMBER,
        NIMBLE_PROPERTY::READ
      );
  pCharacteristic_Model_Number->setValue(std::string(BleGamepadInstance->configuration.getModelNumber()));

  BLECharacteristic* pCharacteristic_Software_Revision = pService->createCharacteristic(
        CHARACTERISTIC_UUID_SOFTWARE_REVISION,
        NIMBLE_PROPERTY::READ
      );
  pCharacteristic_Software_Revision->setValue(std::string(BleGamepadInstance->configuration.getSoftwareRevision()));

  BLECharacteristic* pCharacteristic_Serial_Number = pService->createCharacteristic(
        CHARACTERISTIC_UUID_SERIAL_NUMBER,
        NIMBLE_PROPERTY::READ
      );
  pCharacteristic_Serial_Number->setValue(std::string(BleGamepadInstance->configuration.getSerialNumber()));

  BLECharacteristic* pCharacteristic_Firmware_Revision = pService->createCharacteristic(
        CHARACTERISTIC_UUID_FIRMWARE_REVISION,
        NIMBLE_PROPERTY::READ
      );
  pCharacteristic_Firmware_Revision->setValue(std::string(BleGamepadInstance->configuration.getFirmwareRevision()));

  BLECharacteristic* pCharacteristic_Hardware_Revision = pService->createCharacteristic(
        CHARACTERISTIC_UUID_HARDWARE_REVISION,
        NIMBLE_PROPERTY::READ
      );
  pCharacteristic_Hardware_Revision->setValue(std::string(BleGamepadInstance->configuration.getHardwareRevision()));
  
  NimBLECharacteristic* pCharacteristic_Power_State = BleGamepadInstance->hid->getBatteryService()->createCharacteristic(
        CHARACTERISTIC_UUID_BATTERY_POWER_STATE,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
      );
  BleGamepadInstance->pCharacteristic_Power_State = pCharacteristic_Power_State; // Assign the created characteristic
  BleGamepadInstance->pCharacteristic_Power_State->setValue(0b00000000); // Now it's safe to call setValue <- Set all to unknown by default

  BleGamepadInstance->hid->setPnp(0x01, BleGamepadInstance->configuration.getVid(), BleGamepadInstance->configuration.getPid(), BleGamepadInstance->configuration.getGuidVersion());
  BleGamepadInstance->hid->setHidInfo(0x00, 0x01);

  // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND);
  NimBLEDevice::setSecurityAuth(true, false, false); // enable bonding, no MITM, no SC


  uint8_t *customHidReportDescriptor = new uint8_t[BleGamepadInstance->hidReportDescriptorSize];
  memcpy(customHidReportDescriptor, BleGamepadInstance->tempHidReportDescriptor, BleGamepadInstance->hidReportDescriptorSize);

  #if BLE_GAMEPAD_DEBUG == 1
    // Print HidReportDescriptor to Serial
    dumpHidReportDescriptor( BleGamepadInstance->tempHidReportDescriptor, BleGamepadInstance->hidReportDescriptorSize);
  #endif
  
  BleGamepadInstance->hid->setReportMap((uint8_t *)customHidReportDescriptor, BleGamepadInstance->hidReportDescriptorSize);
  BleGamepadInstance->hid->startServices();

  BleGamepadInstance->onStarted(pServer);

  NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_GAMEPAD);
  pAdvertising->setName(BleGamepadInstance->deviceName);
  pAdvertising->addServiceUUID(BleGamepadInstance->hid->getHidService()->getUUID());
  
  if(BleGamepadInstance->delayAdvertising)
  {
    NIMBLE_LOGD(LOG_TAG, "Main NimBLE server advertising delayed (until Nordic UART Service added)");
  }
  else
  {
    NIMBLE_LOGD(LOG_TAG, "Main NimBLE server advertising started!");
    pAdvertising->start();
  }
  
  BleGamepadInstance->hid->setBatteryLevel(BleGamepadInstance->batteryLevel);

  vTaskDelay(portMAX_DELAY); // delay(portMAX_DELAY);
}
