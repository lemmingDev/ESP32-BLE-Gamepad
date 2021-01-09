#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include "NimBLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <driver/adc.h>
#include "sdkconfig.h"

#include "BleConnectionStatus.h"
#include "BleGamepad.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
  #include "esp32-hal-log.h"
  #define LOG_TAG "BLEGamepad"
#else
  #include "esp_log.h"
  static const char* LOG_TAG = "BLEGamepad";
#endif

static const uint8_t _hidReportDescriptor[] = {
  USAGE_PAGE(1),       0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x05, // USAGE (Gamepad)
  COLLECTION(1),       0x01, // COLLECTION (Application)
  USAGE(1),            0x01, //   USAGE (Pointer)
  COLLECTION(1),       0x00, //   COLLECTION (Physical)

  // ------------------------------------------------- Buttons (1 to 32)
  USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
  USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
  USAGE_MAXIMUM(1),    0x20, //     USAGE_MAXIMUM (Button 32)
  LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
  REPORT_COUNT(1),     0x20, //     REPORT_COUNT (32)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ;32 button bits
  // ------------------------------------------------- X/Y position, Z/rZ position
  USAGE_PAGE(1), 	   0x01, //		USAGE_PAGE (Generic Desktop)
  COLLECTION(1), 	   0x00, //		COLLECTION (Physical)
  USAGE(1), 		   0x30, //     USAGE (X)
  USAGE(1), 		   0x31, //     USAGE (Y)
  USAGE(1), 		   0x32, //     USAGE (Z)
  USAGE(1), 		   0x35, //     USAGE (rZ)
  0x16, 			   0x01, 0x80,//LOGICAL_MINIMUM (-32767)
  0x26, 			   0xFF, 0x7F,//LOGICAL_MAXIMUM (32767)
  REPORT_SIZE(1), 	   0x10, //		REPORT_SIZE (16)
  REPORT_COUNT(1), 	   0x04, //		REPORT_COUNT (4)
  HIDINPUT(1), 		   0x02, //     INPUT (Data,Var,Abs)
  // ------------------------------------------------- Triggers
  USAGE(1),            0x33, //     USAGE (rX) Left Trigger
  USAGE(1),            0x34, //     USAGE (rY) Right Trigger
  LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
  LOGICAL_MAXIMUM(1),  0x7f, //     LOGICAL_MAXIMUM (127)
  REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
  REPORT_COUNT(1),     0x02, //     REPORT_COUNT (2)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ;4 bytes (X,Y,Z,rZ)
  END_COLLECTION(0),		 //     END_COLLECTION
  
  USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x39, //     USAGE (Hat switch)
  USAGE(1),            0x39, //     USAGE (Hat switch)
  LOGICAL_MINIMUM(1),  0x01, //     LOGICAL_MINIMUM (1)
  LOGICAL_MAXIMUM(1),  0x08, //     LOGICAL_MAXIMUM (8)
  REPORT_SIZE(1),      0x04, //     REPORT_SIZE (4)
  REPORT_COUNT(1),     0x02, //     REPORT_COUNT (2)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ;1 byte Hat1, Hat2

  END_COLLECTION(0),         //     END_COLLECTION
  END_COLLECTION(0)          //     END_COLLECTION
};

BleGamepad::BleGamepad(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel) :
  _buttons(0),
  _x(0),
  _y(0),
  _z(0),
  _rZ(0),
  _rX(0),
  _rY(0),
  _hat(0),
  _autoReport(false),
  hid(0)
{
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
  this->connectionStatus = new BleConnectionStatus();
}

void BleGamepad::begin(bool autoReport)
{
  _autoReport = autoReport;
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

void BleGamepad::end(void)
{
}

void BleGamepad::setAxes(int16_t x, int16_t y, int16_t z, int16_t rZ, char rX, char rY, signed char hat)
{
	if(x == -32768) { x = -32767; }
	if(y == -32768) { y = -32767; }
	if(z == -32768) { z = -32767; }
	if(rZ == -32768) { rZ = -32767; }

	_x = x;
	_y = y;
	_z = z;
	_rZ = rZ;
	_rX = rX;
	_rY = rY;
	_hat = hat;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::sendReport(void)
{
	if (this->isConnected())
	{
		uint8_t m[15];
		m[0] = _buttons;
		m[1] = (_buttons >> 8);
		m[2] = (_buttons >> 16);
		m[3] = (_buttons >> 24);
		m[4] = _x;
		m[5] = (_x >> 8);
		m[6] = _y;
		m[7] = (_y >> 8);
		m[8] = _z;
		m[9] = (_z >> 8);
		m[10] = _rZ;
		m[11] = (_rZ >> 8);
		m[12] = (signed char)(_rX - 128);
		m[13] = (signed char)(_rY - 128);
		m[14] = _hat;
		if (m[12] == -128) { m[12] = -127; }
		if (m[13] == -128) { m[13] = -127; }
		this->inputGamepad->setValue(m, sizeof(m));
		this->inputGamepad->notify();
	}
}
void BleGamepad::buttons(uint32_t b)
{
  if (b != _buttons)
  {
    _buttons = b;
	
	if(_autoReport){ sendReport(); }
  }
}

void BleGamepad::press(uint32_t b)
{
  buttons(_buttons | b);
}

void BleGamepad::release(uint32_t b)
{
  buttons(_buttons & ~b);
}

void BleGamepad::setLeftThumb(int16_t x, int16_t y)
{
	_x = x;
	_y = y;
	
	if(_autoReport){ sendReport(); }
}
void BleGamepad::setRightThumb(int16_t z, int16_t rZ)
{
	_z = z;
	_rZ = rZ;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setLeftTrigger(char rX)
{
	_rX = rX;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setRightTrigger(char rY)
{
	_rY = rY;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setHat(signed char hat)
{
	_hat = hat;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setX(int16_t x)
{
	_x = x;
}

void BleGamepad::setY(int16_t y)
{
	_y = y;
}

void BleGamepad::setZ(int16_t z)
{
	_z = z;
}

void BleGamepad::setRZ(int16_t rZ)
{
	_rZ = rZ;
}

void BleGamepad::setRX(int16_t rX)
{
	_rX = rX;
}

void BleGamepad::setRY(int16_t rY)
{
	_rY = rY;
}

void BleGamepad::setAutoReport(bool autoReport)
{
	_autoReport = autoReport;
}

bool BleGamepad::isPressed(uint32_t b)
{
  if ((b & _buttons) > 0)
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
    this->hid->setBatteryLevel(this->batteryLevel);
}

void BleGamepad::taskServer(void* pvParameter) 
{
  BleGamepad* BleGamepadInstance = (BleGamepad *) pvParameter; //static_cast<BleGamepad *>(pvParameter);
  NimBLEDevice::init(BleGamepadInstance->deviceName);
  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(BleGamepadInstance->connectionStatus);

  BleGamepadInstance->hid = new NimBLEHIDDevice(pServer);
  BleGamepadInstance->inputGamepad = BleGamepadInstance->hid->inputReport(0); // <-- input REPORTID from report map
  BleGamepadInstance->connectionStatus->inputGamepad = BleGamepadInstance->inputGamepad;

  BleGamepadInstance->hid->manufacturer()->setValue(BleGamepadInstance->deviceManufacturer);

  BleGamepadInstance->hid->pnp(0x01,0x02e5,0xabcd,0x0110);
  BleGamepadInstance->hid->hidInfo(0x00,0x01);

  NimBLESecurity *pSecurity = new NimBLESecurity();

  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  BleGamepadInstance->hid->reportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
  BleGamepadInstance->hid->startServices();

  BleGamepadInstance->onStarted(pServer);

  NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_GAMEPAD);
  pAdvertising->addServiceUUID(BleGamepadInstance->hid->hidService()->getUUID());
  pAdvertising->start();
  BleGamepadInstance->hid->setBatteryLevel(BleGamepadInstance->batteryLevel);

  ESP_LOGD(LOG_TAG, "Advertising started!");
  vTaskDelay(portMAX_DELAY); //delay(portMAX_DELAY);
}
