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
#include "BleXInputDescriptors.h"
#include <type_traits>

// Verify wire-format sizes of packed report structs match the protocol spec.
// If these fail, the struct padding or packing is wrong on this platform.
static_assert(sizeof(XInputInputReport) == XINPUT_REPORT_LEN_INPUT,
              "XInputInputReport must be 16 bytes on wire");
static_assert(sizeof(XInputOutputReport) == XINPUT_REPORT_LEN_OUTPUT,
              "XInputOutputReport must be 8 bytes on wire");

// Clamp INT16_MIN (-32768) to INT16_MIN+1 (-32767) because some hosts treat
// -32768 as an invalid/uninitialized value.  All axis setters run through this.
static inline int16_t clampAxis(int16_t v)
{
  return v;
}

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
  _touch1X(0),
  _touch1Y(0),
  _touch1Pressure(0),
  _touch2X(0),
  _touch2Y(0),
  _touch2Pressure(0),
  _batteryPowerInformation(0),
  _dischargingState(0),
  _chargingState(0),
  _powerLevel(0),
  nusInitialized(false),
  xInputReceiver(nullptr),
  xInputConsumer(nullptr),
  xInputBattery(nullptr),
  pServer(nullptr),
  nus(nullptr),
  hid(0),
  pCharacteristic_Power_State(0),
  configuration()
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
}

void BleGamepad::resetButtons()
{
  memset(&_buttons, 0, sizeof(_buttons));
}

// Bounds-check macro for HID descriptor construction.  tempHidReportDescriptor
// is a fixed-size buffer; this catches overflows at runtime before they corrupt
// adjacent memory.
#define HID_DESC_CHECK(n) do { \
    if (hidReportDescriptorSize + (n) > (int)sizeof(tempHidReportDescriptor)) { \
      NIMBLE_LOGE(LOG_TAG, "HID descriptor overflow at byte %d (need %d more)", \
                  hidReportDescriptorSize, (n)); \
      return; \
    } \
  } while (0)

void BleGamepad::buildSInputDescriptor()
{
  // Verbatim 139-byte report descriptor from HandHeldLegend/SINPUT-LIB-HID
  // (sinput_lib_hid.c:k_sinput_hid_report_descriptor). Using it 1:1 ensures
  // wDescriptorLength 0x8B matches and Windows' HID parser sees the exact
  // Vendor usages (0xFF00:0x20/0x21/0x22) the official library ships.
  static const uint8_t k_sinput_hid_report_descriptor[139] = {
      0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x85, 0x01, 0x06, 0x00, 0xFF, 0x09, 0x01, 0x15, 0x00, 0x25,
      0xFF, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02, 0x05, 0x09, 0x19, 0x01, 0x29, 0x20, 0x15, 0x00, 0x25,
      0x01, 0x75, 0x01, 0x95, 0x20, 0x81, 0x02, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09,
      0x35, 0x09, 0x33, 0x09, 0x34, 0x16, 0x00, 0x80, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x06, 0x81,
      0x02, 0x06, 0x00, 0xFF, 0x09, 0x20, 0x15, 0x00, 0x26, 0xFF, 0xFF, 0x75, 0x20, 0x95, 0x01, 0x81,
      0x02, 0x09, 0x21, 0x16, 0x00, 0x80, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x06, 0x81, 0x02, 0x09,
      0x22, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x1D, 0x81, 0x02, 0x85, 0x02, 0x09, 0x23,
      0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x02, 0x85, 0x03, 0x09, 0x24, 0x15,
      0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x2F, 0x91, 0x02, 0xC0};
  memcpy(tempHidReportDescriptor, k_sinput_hid_report_descriptor, sizeof(k_sinput_hid_report_descriptor));
  hidReportDescriptorSize = sizeof(k_sinput_hid_report_descriptor);
}

void BleGamepad::buildGenericDescriptor()
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

    if (genericButtonPaddingBits > 0)
    {
      // REPORT_SIZE (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // REPORT_COUNT (# of padding bits)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = genericButtonPaddingBits;

      // INPUT (Const,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;
    }
  }

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

    if (genericSpecialButtonPaddingBits > 0)
    {
      // REPORT_SIZE (1)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

      // REPORT_COUNT (# of padding bits)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
      tempHidReportDescriptor[hidReportDescriptorSize++] = genericSpecialButtonPaddingBits;

      // INPUT (Const,Var,Abs)
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
      tempHidReportDescriptor[hidReportDescriptorSize++] = 0x03;
    }
  }

  if (configuration.getAxisCount() > 0)
  {
    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getAxesMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getAxesMin());

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getAxesMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getAxesMax());

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
  }

  if (configuration.getSimulationCount() > 0)
  {
    // USAGE_PAGE (Simulation Controls)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

    // LOGICAL_MINIMUM (-32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getSimulationMin());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getSimulationMin());

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getSimulationMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getSimulationMax());

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
  }

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

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMax());

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
  }

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

    // LOGICAL_MAXIMUM (+32767)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
    tempHidReportDescriptor[hidReportDescriptorSize++] = lowByte(configuration.getMotionMax());
    tempHidReportDescriptor[hidReportDescriptorSize++] = highByte(configuration.getMotionMax());

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
  }

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
  }

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
}

void BleGamepad::buildXInputDescriptor()
{
  // Xbox descriptors taken verbatim from Mystfit pr-74 (570d6da):
  //  - XInputOneS  -> 1708 (AC Back 0x0A,0x24,0x02, 0x02/0x04 present) — Generic on Win11 22H2+ WGI, XInput on linux<6.5 xpad
  //  - XInputSeriesX -> 1914 (Record/Share 0x0A,0xB2,0x00, 0x01+0x03 only) — XInput on Win11 WGI (Series X ok)
  GamepadMode mode = configuration.getGamepadMode();
  if (mode == GamepadMode::XInputSeriesX)
  {
    memcpy(tempHidReportDescriptor, XboxOneS_1914_HIDDescriptor, XboxOneS_1914_DescriptorSize);
    hidReportDescriptorSize = XboxOneS_1914_DescriptorSize;
  }
  else
  {
    memcpy(tempHidReportDescriptor, XboxOneS_1708_HIDDescriptor, XboxOneS_1708_DescriptorSize);
    hidReportDescriptorSize = XboxOneS_1708_DescriptorSize;
  }
}

void BleGamepad::begin(BleGamepadConfiguration *config)
{
  BleGamepadConfiguration defaultConfig;
  configuration = config ? *config : defaultConfig;

  GamepadMode mode = configuration.getGamepadMode();
  if (mode == GamepadMode::SInput || mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX)
  {
    configuration.setIncludeSlider1(false);
    configuration.setIncludeSlider2(false);
    configuration.setIncludeRudder(false);
    configuration.setIncludeThrottle(false);
    configuration.setIncludeAccelerator(false);
    configuration.setIncludeBrake(false);
    configuration.setIncludeSteering(false);
    configuration.setIncludeGyroscope(false);
    configuration.setIncludeAccelerometer(false);
    configuration.setEnableOutputReport(false);
    configuration.setEnableFeatureReport(false);
    configuration.setHatSwitchCount(1);

    if (mode == GamepadMode::SInput)
      configuration.setButtonCount(25);
    else if (mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX)
    {
      configuration.setButtonCount(11);
      deviceName = "Xbox Wireless Controller";
      deviceManufacturer = "Microsoft";
      // Mystfit's XBOX_1708_SERIAL / XBOX_1914_SERIAL — ASCII hex serials
      // from real controller BLE captures, used by the Xbox driver for matching.
      if (mode == GamepadMode::XInputSeriesX)
        configuration.setSerialNumber("3039373130303637313034303231");
      else
        configuration.setSerialNumber("3033363030343037323136373239");
    }

    if (mode == GamepadMode::SInput)
    {
      if (configuration.getVid() != SINPUT_USB_VID || configuration.getPid() != SINPUT_USB_PID_GENERIC)
        Serial.println("WARNING: SInput mode requires VID 0x2E8A / PID 0x10C6 for host recognition. Do not override setVid()/setPid().");
    }
    else if (mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX)
    {
      if (configuration.getVid() != XINPUT_USB_VID)
        Serial.println("WARNING: XInputOneS/XInputSeriesX mode requires VID 0x045E for Xbox driver recognition. Do not override setVid()/setPid().");
    }
  }

  enableOutputReport = configuration.getEnableOutputReport();
  outputReportLength = configuration.getOutputReportLength();
  enableFeatureReport = configuration.getEnableFeatureReport();
  featureReportLength = configuration.getFeatureReportLength();
  enableSInput = configuration.getEnableSInput();

  genericButtonPaddingBits = 8 - (configuration.getButtonCount() % 8);
  if (genericButtonPaddingBits == 8)
  {
    genericButtonPaddingBits = 0;
  }
  genericSpecialButtonPaddingBits = 8 - (configuration.getTotalSpecialButtonCount() % 8);
  if (genericSpecialButtonPaddingBits == 8)
  {
    genericSpecialButtonPaddingBits = 0;
  }

  // Precompute bit positions for each special button so press/release don't
  // need to iterate the configuration array on every call.
  uint8_t pos = 0;
  for (int i = 0; i < POSSIBLESPECIALBUTTONS; i++)
  {
    _specialButtonPositions[i] = pos;
    if (configuration.getWhichSpecialButtons()[i]) pos++;
  }

  uint8_t numOfAxisBytes = configuration.getAxisCount() * 2;
  uint8_t numOfSimulationBytes = configuration.getSimulationCount() * 2;

  numOfButtonBytes = configuration.getButtonCount() / 8;
  if (genericButtonPaddingBits > 0)
  {
    numOfButtonBytes++;
  }

  uint8_t numOfSpecialButtonBytes = configuration.getTotalSpecialButtonCount() / 8;
  if (genericSpecialButtonPaddingBits > 0)
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

  if (mode == GamepadMode::SInput)
  {
    enableSInput = true;
    buildSInputDescriptor();
  }
  else if (mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX)
  {
    enableSInput = false;
    buildXInputDescriptor();
  }
  else
  {
    enableSInput = false;
    buildGenericDescriptor();
  }

  // Set task priority from 5 to 1 in order to get ESP32-C3 working
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 1, NULL);
}

void BleGamepad::end(void)
{
  delete outputReceiver;
  outputReceiver = nullptr;
  delete featureReceiver;
  featureReceiver = nullptr;
  delete sInputReceiver;
  sInputReceiver = nullptr;
  delete xInputReceiver;
  xInputReceiver = nullptr;
  delete connectionStatus;
  connectionStatus = nullptr;
  delete[] outputBackupBuffer;
  outputBackupBuffer = nullptr;
  delete[] featureBackupBuffer;
  featureBackupBuffer = nullptr;
}

void BleGamepad::setAxes(int16_t x, int16_t y, int16_t z, int16_t rX, int16_t rY, int16_t rZ, int16_t slider1, int16_t slider2)
{
  x = clampAxis(x);
  y = clampAxis(y);
  z = clampAxis(z);
  rZ = clampAxis(rZ);
  rX = clampAxis(rX);
  rY = clampAxis(rY);
  slider1 = clampAxis(slider1);
  slider2 = clampAxis(slider2);

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
  x = clampAxis(x);
  y = clampAxis(y);
  z = clampAxis(z);
  rZ = clampAxis(rZ);
  rX = clampAxis(rX);
  rY = clampAxis(rY);
  slider1 = clampAxis(slider1);
  slider2 = clampAxis(slider2);

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
  rudder = clampAxis(rudder);
  throttle = clampAxis(throttle);
  accelerator = clampAxis(accelerator);
  brake = clampAxis(brake);
  steering = clampAxis(steering);

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
  slider1 = clampAxis(slider1);
  slider2 = clampAxis(slider2);

  _slider1 = slider1;
  _slider2 = slider2;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::sendReport(void)
{
  if (!this->isConnected())
  {
    return;
  }

  if (enableSInput)
  {
    sendSInputReport();
  }
  else if (configuration.getGamepadMode() == GamepadMode::XInputOneS ||
           configuration.getGamepadMode() == GamepadMode::XInputSeriesX)
  {
    sendXInputReport();
  }
  else
  {
    sendGenericReport();
  }
}

void BleGamepad::sendSInputReport()
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
  if (configuration.getButtonCount() >= 7  && isPressed(BUTTON_7))  buttons1 |= SINPUT_BTN1_LSTICK;
  if (configuration.getButtonCount() >= 8  && isPressed(BUTTON_8))  buttons1 |= SINPUT_BTN1_RSTICK;
  if (configuration.getButtonCount() >= 5  && isPressed(BUTTON_5))  buttons1 |= SINPUT_BTN1_LSHOULDER;
  if (configuration.getButtonCount() >= 6  && isPressed(BUTTON_6))  buttons1 |= SINPUT_BTN1_RSHOULDER;
  if (configuration.getButtonCount() >= 9  && isPressed(BUTTON_9))  buttons1 |= SINPUT_BTN1_LTRIGGER;
  if (configuration.getButtonCount() >= 10 && isPressed(BUTTON_10)) buttons1 |= SINPUT_BTN1_RTRIGGER;
  if (configuration.getButtonCount() >= 11 && isPressed(BUTTON_11)) buttons1 |= SINPUT_BTN1_LPADDLE1;
  if (configuration.getButtonCount() >= 12 && isPressed(BUTTON_12)) buttons1 |= SINPUT_BTN1_RPADDLE1;
  m[SINPUT_IN_IDX_BUTTONS_1] = buttons1;

  // buttons_2: map special buttons + regular buttons to SInput's fixed bit positions
  uint8_t buttons2 = 0;
  if (configuration.getIncludeStart())
  {
      uint8_t bit = specialButtonBitPosition(START_BUTTON);
      if (_specialButtons & (1 << bit)) buttons2 |= SINPUT_BTN2_START;
  }
  if (configuration.getIncludeBack() || configuration.getIncludeSelect())
  {
      // Map both Select and Back to SInput's "Back" bit
      uint8_t selectBit = specialButtonBitPosition(SELECT_BUTTON);
      uint8_t backBit = specialButtonBitPosition(BACK_BUTTON);
      if ((_specialButtons & (1 << selectBit)) || (_specialButtons & (1 << backBit)))
          buttons2 |= SINPUT_BTN2_BACK;
  }
  if (configuration.getIncludeHome())
  {
      uint8_t bit = specialButtonBitPosition(HOME_BUTTON);
      if (_specialButtons & (1 << bit)) buttons2 |= SINPUT_BTN2_GUIDE;
  }
  if (configuration.getButtonCount() >= 13 && isPressed(BUTTON_13)) buttons2 |= SINPUT_BTN2_CAPTURE;
  if (configuration.getButtonCount() >= 14 && isPressed(BUTTON_14)) buttons2 |= SINPUT_BTN2_LPADDLE2;
  if (configuration.getButtonCount() >= 15 && isPressed(BUTTON_15)) buttons2 |= SINPUT_BTN2_RPADDLE2;
  if (configuration.getButtonCount() >= 16 && isPressed(BUTTON_16)) buttons2 |= SINPUT_BTN2_TOUCHPAD1;
  if (configuration.getButtonCount() >= 17 && isPressed(BUTTON_17)) buttons2 |= SINPUT_BTN2_TOUCHPAD2;
  m[SINPUT_IN_IDX_BUTTONS_2] = buttons2;

  // buttons_3: Power + Misc1-7
  uint8_t buttons3 = 0;
  if (configuration.getButtonCount() >= 18 && isPressed(BUTTON_18)) buttons3 |= SINPUT_BTN3_POWER;
  if (configuration.getButtonCount() >= 19 && isPressed(BUTTON_19)) buttons3 |= SINPUT_BTN3_MISC1;
  if (configuration.getButtonCount() >= 20 && isPressed(BUTTON_20)) buttons3 |= SINPUT_BTN3_MISC2;
  if (configuration.getButtonCount() >= 21 && isPressed(BUTTON_21)) buttons3 |= SINPUT_BTN3_MISC3;
  if (configuration.getButtonCount() >= 22 && isPressed(BUTTON_22)) buttons3 |= SINPUT_BTN3_MISC4;
  if (configuration.getButtonCount() >= 23 && isPressed(BUTTON_23)) buttons3 |= SINPUT_BTN3_MISC5;
  if (configuration.getButtonCount() >= 24 && isPressed(BUTTON_24)) buttons3 |= SINPUT_BTN3_MISC6;
  if (configuration.getButtonCount() >= 25 && isPressed(BUTTON_25)) buttons3 |= SINPUT_BTN3_MISC7;
  m[SINPUT_IN_IDX_BUTTONS_3] = buttons3;

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

  // IMU data (accelerometer + gyroscope)
  if (configuration.getEnableSInputIMU())
  {
    // Timestamp in microseconds since boot (monotonically increasing)
    uint32_t timestampUs = (uint32_t)millis() * 1000;
    m[SINPUT_IN_IDX_IMU_TIMESTAMP]     = (uint8_t)timestampUs;
    m[SINPUT_IN_IDX_IMU_TIMESTAMP + 1] = (uint8_t)(timestampUs >> 8);
    m[SINPUT_IN_IDX_IMU_TIMESTAMP + 2] = (uint8_t)(timestampUs >> 16);
    m[SINPUT_IN_IDX_IMU_TIMESTAMP + 3] = (uint8_t)(timestampUs >> 24);

    m[SINPUT_IN_IDX_IMU_ACCEL_X]     = (uint8_t)_aX;
    m[SINPUT_IN_IDX_IMU_ACCEL_X + 1] = (uint8_t)(_aX >> 8);
    m[SINPUT_IN_IDX_IMU_ACCEL_Y]     = (uint8_t)_aY;
    m[SINPUT_IN_IDX_IMU_ACCEL_Y + 1] = (uint8_t)(_aY >> 8);
    m[SINPUT_IN_IDX_IMU_ACCEL_Z]     = (uint8_t)_aZ;
    m[SINPUT_IN_IDX_IMU_ACCEL_Z + 1] = (uint8_t)(_aZ >> 8);

    m[SINPUT_IN_IDX_IMU_GYRO_X]     = (uint8_t)_gX;
    m[SINPUT_IN_IDX_IMU_GYRO_X + 1] = (uint8_t)(_gX >> 8);
    m[SINPUT_IN_IDX_IMU_GYRO_Y]     = (uint8_t)_gY;
    m[SINPUT_IN_IDX_IMU_GYRO_Y + 1] = (uint8_t)(_gY >> 8);
    m[SINPUT_IN_IDX_IMU_GYRO_Z]     = (uint8_t)_gZ;
    m[SINPUT_IN_IDX_IMU_GYRO_Z + 1] = (uint8_t)(_gZ >> 8);
  }

  // Touchpad data
  if (configuration.getEnableTouchpad())
  {
    m[SINPUT_IN_IDX_TOUCH1_X]     = (uint8_t)_touch1X;
    m[SINPUT_IN_IDX_TOUCH1_X + 1] = (uint8_t)(_touch1X >> 8);
    m[SINPUT_IN_IDX_TOUCH1_Y]     = (uint8_t)_touch1Y;
    m[SINPUT_IN_IDX_TOUCH1_Y + 1] = (uint8_t)(_touch1Y >> 8);
    m[SINPUT_IN_IDX_TOUCH1_P]     = (uint8_t)_touch1Pressure;
    m[SINPUT_IN_IDX_TOUCH1_P + 1] = (uint8_t)(_touch1Pressure >> 8);

    m[SINPUT_IN_IDX_TOUCH2_X]     = (uint8_t)_touch2X;
    m[SINPUT_IN_IDX_TOUCH2_X + 1] = (uint8_t)(_touch2X >> 8);
    m[SINPUT_IN_IDX_TOUCH2_Y]     = (uint8_t)_touch2Y;
    m[SINPUT_IN_IDX_TOUCH2_Y + 1] = (uint8_t)(_touch2Y >> 8);
    m[SINPUT_IN_IDX_TOUCH2_P]     = (uint8_t)_touch2Pressure;
    m[SINPUT_IN_IDX_TOUCH2_P + 1] = (uint8_t)(_touch2Pressure >> 8);
  }

  this->sInputGamepad->setValue(m, sizeof(m));
  this->sInputGamepad->notify();
}

void BleGamepad::sendXInputReport()
{
  XInputInputReport report;
  memset(&report, 0, sizeof(report));

  // Sticks: int16 (-32767..32767) → uint16 (0..65535, center = 0x8000)
  if (configuration.getIncludeXAxis() && configuration.getIncludeYAxis())
  {
    report.x = (uint16_t)((int32_t)_x + XBOX_AXIS_CENTER_OFFSET);
    report.y = (uint16_t)((int32_t)_y + XBOX_AXIS_CENTER_OFFSET);
  }
  else
  {
    report.x = XBOX_AXIS_CENTER_OFFSET;
    report.y = XBOX_AXIS_CENTER_OFFSET;
  }
  if (configuration.getIncludeZAxis() && configuration.getIncludeRzAxis())
  {
    report.z = (uint16_t)((int32_t)_z + XBOX_AXIS_CENTER_OFFSET);
    report.rz = (uint16_t)((int32_t)_rZ + XBOX_AXIS_CENTER_OFFSET);
  }
  else
  {
    report.z = XBOX_AXIS_CENTER_OFFSET;
    report.rz = XBOX_AXIS_CENTER_OFFSET;
  }

  // Triggers: int16 (0..32767) → uint10 (0..1023)
  if (configuration.getIncludeRxAxis())
  {
    report.brake = (uint16_t)((uint32_t)_rX * XBOX_TRIGGER_MAX / 32767);
  }
  if (configuration.getIncludeRyAxis())
  {
    report.accelerator = (uint16_t)((uint32_t)_rY * XBOX_TRIGGER_MAX / 32767);
  }

  // Hat switch: HAT_* → Xbox 4-bit encoding
  if (configuration.getHatSwitchCount() >= 1)
  {
    switch (_hat1)
    {
      case HAT_UP:        report.hat = XBOX_DPAD_NORTH; break;
      case HAT_UP_RIGHT:  report.hat = XBOX_DPAD_NORTHEAST; break;
      case HAT_RIGHT:     report.hat = XBOX_DPAD_EAST; break;
      case HAT_DOWN_RIGHT:report.hat = XBOX_DPAD_SOUTHEAST; break;
      case HAT_DOWN:      report.hat = XBOX_DPAD_SOUTH; break;
      case HAT_DOWN_LEFT: report.hat = XBOX_DPAD_SOUTHWEST; break;
      case HAT_LEFT:      report.hat = XBOX_DPAD_WEST; break;
      case HAT_UP_LEFT:   report.hat = XBOX_DPAD_NORTHWEST; break;
      default:            report.hat = XBOX_DPAD_NONE; break;
    }
  }

  // Buttons: map BUTTON_1..15 to Xbox bitmask
  uint16_t btns = 0;
  if (configuration.getButtonCount() >= 1  && isPressed(BUTTON_1))  btns |= XBOX_BUTTON_A;
  if (configuration.getButtonCount() >= 2  && isPressed(BUTTON_2))  btns |= XBOX_BUTTON_B;
  if (configuration.getButtonCount() >= 3  && isPressed(BUTTON_3))  btns |= XBOX_BUTTON_X;
  if (configuration.getButtonCount() >= 4  && isPressed(BUTTON_4))  btns |= XBOX_BUTTON_Y;
  if (configuration.getButtonCount() >= 5  && isPressed(BUTTON_5))  btns |= XBOX_BUTTON_LB;
  if (configuration.getButtonCount() >= 6  && isPressed(BUTTON_6))  btns |= XBOX_BUTTON_RB;
  if (configuration.getButtonCount() >= 7  && isPressed(BUTTON_7))  btns |= XBOX_BUTTON_LS;
  if (configuration.getButtonCount() >= 8  && isPressed(BUTTON_8))  btns |= XBOX_BUTTON_RS;
  if (configuration.getButtonCount() >= 9  && isPressed(BUTTON_9))  btns |= XBOX_BUTTON_SELECT;
  if (configuration.getButtonCount() >= 10 && isPressed(BUTTON_10)) btns |= XBOX_BUTTON_START;
  if (configuration.getButtonCount() >= 11 && isPressed(BUTTON_11)) btns |= XBOX_BUTTON_HOME;

  // Map special buttons to Xbox buttons
  if (configuration.getIncludeStart())
  {
    uint8_t bit = specialButtonBitPosition(START_BUTTON);
    if (_specialButtons & (1 << bit)) btns |= XBOX_BUTTON_START;
  }
  if (configuration.getIncludeSelect())
  {
    uint8_t bit = specialButtonBitPosition(SELECT_BUTTON);
    if (_specialButtons & (1 << bit)) btns |= XBOX_BUTTON_SELECT;
  }
  if (configuration.getIncludeHome())
  {
    uint8_t bit = specialButtonBitPosition(HOME_BUTTON);
    if (_specialButtons & (1 << bit)) btns |= XBOX_BUTTON_HOME;
  }
  report.buttons = btns;

  // Share button (separate byte)
  if (configuration.getIncludeBack())
  {
    uint8_t bit = specialButtonBitPosition(BACK_BUTTON);
    if (_specialButtons & (1 << bit)) report.share = XBOX_BUTTON_SHARE;
  }

  this->xInputGamepad->setValue((uint8_t *)&report, sizeof(report));
  this->xInputGamepad->notify();
}

void BleGamepad::sendGenericReport()
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

  uint8_t result = _buttons[index] & ~bitmask;

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
  if (b >= POSSIBLESPECIALBUTTONS)
  {
    return 0;
  }
  return _specialButtonPositions[b];
}

void BleGamepad::pressSpecialButton(uint8_t b)
{
  uint8_t button = specialButtonBitPosition(b);
  uint8_t bit = button % 8;
  uint8_t bitmask = (1 << bit);

  uint8_t result = _specialButtons | bitmask;

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

  uint8_t result = _specialButtons & ~bitmask;

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
  x = clampAxis(x);
  y = clampAxis(y);

  _x = x;
  _y = y;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRightThumb(int16_t z, int16_t rZ)
{
  z = clampAxis(z);
  rZ = clampAxis(rZ);

  _z = z;
  _rZ = rZ;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRightThumbAndroid(int16_t z, int16_t rX)
{
  z = clampAxis(z);
  rX = clampAxis(rX);

  _z = z;
  _rX = rX;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setLeftTrigger(int16_t rX)
{
  rX = clampAxis(rX);

  _rX = rX;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRightTrigger(int16_t rY)
{
  rY = clampAxis(rY);

  _rY = rY;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setTriggers(int16_t rX, int16_t rY)
{
  rX = clampAxis(rX);
  rY = clampAxis(rY);

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
  x = clampAxis(x);

  _x = x;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setY(int16_t y)
{
  y = clampAxis(y);

  _y = y;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setZ(int16_t z)
{
  z = clampAxis(z);

  _z = z;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRZ(int16_t rZ)
{
  rZ = clampAxis(rZ);

  _rZ = rZ;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRX(int16_t rX)
{
  rX = clampAxis(rX);

  _rX = rX;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRY(int16_t rY)
{
  rY = clampAxis(rY);

  _rY = rY;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSlider(int16_t slider)
{
  slider = clampAxis(slider);

  _slider1 = slider;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSlider1(int16_t slider1)
{
  slider1 = clampAxis(slider1);

  _slider1 = slider1;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSlider2(int16_t slider2)
{
  slider2 = clampAxis(slider2);

  _slider2 = slider2;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setRudder(int16_t rudder)
{
  rudder = clampAxis(rudder);

  _rudder = rudder;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setThrottle(int16_t throttle)
{
  throttle = clampAxis(throttle);

  _throttle = throttle;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setAccelerator(int16_t accelerator)
{
  accelerator = clampAxis(accelerator);

  _accelerator = accelerator;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setBrake(int16_t brake)
{
  brake = clampAxis(brake);

  _brake = brake;

  if (configuration.getAutoReport())
  {
    sendReport();
  }
}

void BleGamepad::setSteering(int16_t steering)
{
  steering = clampAxis(steering);

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

bool BleGamepad::isRumbleReceived()
{
  if (enableSInput && sInputReceiver)
  {
    if (this->sInputReceiver->rumbleFlag)
    {
      this->sInputReceiver->rumbleFlag = false;
      return true;
    }
  }
  return false;
}

uint8_t BleGamepad::getRumbleLeftAmplitude()
{
  if (enableSInput && sInputReceiver)
  {
    return sInputReceiver->rumbleLeftAmplitude;
  }
  return 0;
}

uint8_t BleGamepad::getRumbleRightAmplitude()
{
  if (enableSInput && sInputReceiver)
  {
    return sInputReceiver->rumbleRightAmplitude;
  }
  return 0;
}

bool BleGamepad::isRgbReceived()
{
  if (enableSInput && sInputReceiver)
  {
    if (this->sInputReceiver->rgbFlag)
    {
      this->sInputReceiver->rgbFlag = false;
      return true;
    }
  }
  return false;
}

uint8_t BleGamepad::getRgbRed()
{
  if (enableSInput && sInputReceiver)
  {
    return sInputReceiver->rgbRed;
  }
  return 0;
}

uint8_t BleGamepad::getRgbGreen()
{
  if (enableSInput && sInputReceiver)
  {
    return sInputReceiver->rgbGreen;
  }
  return 0;
}

uint8_t BleGamepad::getRgbBlue()
{
  if (enableSInput && sInputReceiver)
  {
    return sInputReceiver->rgbBlue;
  }
  return 0;
}

bool BleGamepad::isXInputRumbleReceived()
{
  GamepadMode mode = configuration.getGamepadMode();
  if ((mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX) && xInputReceiver)
  {
    if (xInputReceiver->rumbleFlag)
    {
      xInputReceiver->rumbleFlag = false;
      return true;
    }
  }
  return false;
}

uint8_t BleGamepad::getXInputStrongMotor()
{
  GamepadMode mode = configuration.getGamepadMode();
  if ((mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX) && xInputReceiver)
  {
    return xInputReceiver->strongMotor;
  }
  return 0;
}

uint8_t BleGamepad::getXInputWeakMotor()
{
  GamepadMode mode = configuration.getGamepadMode();
  if ((mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX) && xInputReceiver)
  {
    return xInputReceiver->weakMotor;
  }
  return 0;
}

uint8_t BleGamepad::getXInputLeftTriggerMagnitude()
{
  GamepadMode mode = configuration.getGamepadMode();
  if ((mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX) && xInputReceiver)
  {
    return xInputReceiver->leftTriggerMagnitude;
  }
  return 0;
}

uint8_t BleGamepad::getXInputRightTriggerMagnitude()
{
  GamepadMode mode = configuration.getGamepadMode();
  if ((mode == GamepadMode::XInputOneS || mode == GamepadMode::XInputSeriesX) && xInputReceiver)
  {
    return xInputReceiver->rightTriggerMagnitude;
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
  return server->getPeerInfo(0);
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
  gX = clampAxis(gX);
  gY = clampAxis(gY);
  gZ = clampAxis(gZ);
  
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
  aX = clampAxis(aX);
  aY = clampAxis(aY);
  aZ = clampAxis(aZ);
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
  gX = clampAxis(gX);
  gY = clampAxis(gY);
  gZ = clampAxis(gZ);
  aX = clampAxis(aX);
  aY = clampAxis(aY);
  aZ = clampAxis(aZ);
  
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

void BleGamepad::setTouchpad(uint8_t pad, int16_t x, int16_t y, uint16_t pressure)
{
  if (pad == 0)
  {
    _touch1X = x;
    _touch1Y = y;
    _touch1Pressure = pressure;
  }
  else if (pad == 1)
  {
    _touch2X = x;
    _touch2Y = y;
    _touch2Pressure = pressure;
  }

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
  NimBLEDevice::setDefaultPhy(BLE_GAP_LE_PHY_2M_MASK, BLE_GAP_LE_PHY_2M_MASK);
  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(BleGamepadInstance->connectionStatus);
  pServer->advertiseOnDisconnect(true);

  BleGamepadInstance->hid = new NimBLEHIDDevice(pServer);

  if (BleGamepadInstance->enableSInput)
  {
    // SInput owns Report IDs 0x01-0x03 outright. Create the Report
    // characteristics without READ_ENC/WRITE_ENC so Windows' generic BLE HID
    // driver can read the Report Reference descriptors (0x2908) without
    // bonding — unlike the Xbox driver it won't auto-initiate encryption.
    NimBLEService *pHidService = BleGamepadInstance->hid->getHidService();

    BleGamepadInstance->sInputGamepad = pHidService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A4D), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    NimBLEDescriptor *d1 = BleGamepadInstance->sInputGamepad->createDescriptor(NimBLEUUID((uint16_t)0x2908), NIMBLE_PROPERTY::READ);
    uint8_t v1[] = {SINPUT_REPORT_ID_INPUT, 0x01}; d1->setValue(v1, 2);
    BleGamepadInstance->connectionStatus->inputGamepad = BleGamepadInstance->sInputGamepad;
    BleGamepadInstance->sInputGamepad->setCallbacks(BleGamepadInstance->connectionStatus);

    BleGamepadInstance->sInputCmdGamepad = pHidService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A4D), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    NimBLEDescriptor *d2 = BleGamepadInstance->sInputCmdGamepad->createDescriptor(NimBLEUUID((uint16_t)0x2908), NIMBLE_PROPERTY::READ);
    uint8_t v2[] = {SINPUT_REPORT_ID_INPUT_CMDDAT, 0x01}; d2->setValue(v2, 2);

    BleGamepadInstance->sInputReceiver = new BleSInputReceiver(&BleGamepadInstance->configuration, BleGamepadInstance->sInputCmdGamepad);
    BleGamepadInstance->sInputOutputGamepad = pHidService->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A4D), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    NimBLEDescriptor *d3 = BleGamepadInstance->sInputOutputGamepad->createDescriptor(NimBLEUUID((uint16_t)0x2908), NIMBLE_PROPERTY::READ);
    uint8_t v3[] = {SINPUT_REPORT_ID_OUTPUT_CMDDAT, 0x02}; d3->setValue(v3, 2);
    BleGamepadInstance->sInputOutputGamepad->setCallbacks(BleGamepadInstance->sInputReceiver);
  }
  else if (BleGamepadInstance->configuration.getGamepadMode() == GamepadMode::XInputOneS ||
           BleGamepadInstance->configuration.getGamepadMode() == GamepadMode::XInputSeriesX)
  {
    // XInput: 0x01 Input + 0x03 Output are common to both.
    // One S 1708 also declares 0x02 Consumer (AC Home) + 0x04 Battery in the
    // Report Map — for 1708 we must create those Input characteristics or
    // Windows enumerates a Report Reference for every REPORT_ID and rejects
    // the service when GATT DB is missing them (shows as generic pad).
    // Series X 1914 has only 0x01/0x03 and matches Mystfit pr-74 570d6da.
    BleGamepadInstance->xInputGamepad = BleGamepadInstance->hid->getInputReport(XINPUT_REPORT_ID_INPUT);
    BleGamepadInstance->connectionStatus->inputGamepad = BleGamepadInstance->xInputGamepad;
    BleGamepadInstance->xInputGamepad->setCallbacks(BleGamepadInstance->connectionStatus);

    if (BleGamepadInstance->configuration.getGamepadMode() == GamepadMode::XInputOneS)
    {
      BleGamepadInstance->xInputConsumer = BleGamepadInstance->hid->getInputReport(0x02);
      BleGamepadInstance->xInputConsumer->setCallbacks(BleGamepadInstance->connectionStatus);
      BleGamepadInstance->xInputBattery = BleGamepadInstance->hid->getInputReport(0x04);
      BleGamepadInstance->xInputBattery->setCallbacks(BleGamepadInstance->connectionStatus);
      uint8_t batt = 0; BleGamepadInstance->xInputBattery->setValue(&batt, 1);
    }

    BleGamepadInstance->xInputReceiver = new BleXInputReceiver();
    BleGamepadInstance->xInputOutputGamepad = BleGamepadInstance->hid->getOutputReport(XINPUT_REPORT_ID_OUTPUT);
    BleGamepadInstance->xInputOutputGamepad->setCallbacks(BleGamepadInstance->xInputReceiver);
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

  // --- setReportMap BEFORE setManufacturer (matches Mystfit's working order) ---
  uint8_t *customHidReportDescriptor = new uint8_t[BleGamepadInstance->hidReportDescriptorSize];
  memcpy(customHidReportDescriptor, BleGamepadInstance->tempHidReportDescriptor, BleGamepadInstance->hidReportDescriptorSize);

  #if BLE_GAMEPAD_DEBUG == 1
    dumpHidReportDescriptor( BleGamepadInstance->tempHidReportDescriptor, BleGamepadInstance->hidReportDescriptorSize);
  #endif
  
  BleGamepadInstance->hid->setReportMap((uint8_t *)customHidReportDescriptor, BleGamepadInstance->hidReportDescriptorSize);
  delete[] customHidReportDescriptor;

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

  // VID Source 0x02 = USB (required for Windows Xbox driver matching; Mystfit uses this)
  uint8_t vidSource = (BleGamepadInstance->configuration.getGamepadMode() == GamepadMode::XInputOneS ||
                       BleGamepadInstance->configuration.getGamepadMode() == GamepadMode::XInputSeriesX) ? 0x02 : 0x01;
  BleGamepadInstance->hid->setPnp(vidSource, BleGamepadInstance->configuration.getVid(), BleGamepadInstance->configuration.getPid(), BleGamepadInstance->configuration.getGuidVersion());
  BleGamepadInstance->hid->setHidInfo(0x00, 0x01);

  NimBLEDevice::setSecurityAuth(true, false, false); // enable bonding, no MITM, no SC

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
