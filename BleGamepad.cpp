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

  // ------------------------------------------------- Buttons (1 to 64)
  USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
  USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
  USAGE_MAXIMUM(1),    0x40, //     USAGE_MAXIMUM (Button 64)
  LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
  REPORT_COUNT(1),     0x40, //     REPORT_COUNT (64)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ;64 button bits
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
  0x15, 0x00,                // 	Logical Minimum (0)
  0x27, 0xFF, 0xFF, 0, 0,    // 	Logical Maximum (65535)
  REPORT_SIZE(1),      0x10, //     REPORT_SIZE (16)
  REPORT_COUNT(1),     0x02, //     REPORT_COUNT (2)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ;4 bytes (X,Y,Z,rZ)
  // ------------------------------------------------- Sliders
  USAGE(1),            0x36, //     USAGE (Slider) Slider 1
  USAGE(1),            0x36, //     USAGE (Slider) Slider 2
  0x15, 0x00,                // 	Logical Minimum (0)
  0x27, 0xFF, 0xFF, 0, 0,    // 	Logical Maximum (65535)
  REPORT_SIZE(1),      0x10, //     REPORT_SIZE (16)
  REPORT_COUNT(1),     0x02, //     REPORT_COUNT (2)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ;20 bytes (slider 1 and slider 2)
  END_COLLECTION(0),		 //     END_COLLECTION
  // ------------------------------------------------- Hats  
  USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
  USAGE(1), 0x39,			 //     Usage (Hat Switch) Hat 4
  USAGE(1), 0x39,			 //     Usage (Hat Switch) Hat 3
  USAGE(1), 0x39,			 //     Usage (Hat Switch) Hat 2
  USAGE(1), 0x39,			 //     Usage (Hat Switch) Hat 1
  0x15, 0x01, 				 //	    Logical Min (1)
  0x25, 0x08,				 //	    Logical Max (8)
  0x35, 0x00,				 //     Physical Min (0)
  0x46, 0x3B, 0x01,			 //     Physical Max (315)
  0x65, 0x12,				 //     Unit (SI Rot : Ang Pos)
  0x75, 0x08, 				 //	    Report Size (8)
  0x95, 0x04, 				 //	    Report Count (4)
  0x81, 0x42, 				 //	    Input (Data, Variable, Absolute)

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
  _slider1(0),
  _slider2(0),
  _hat1(0),
  _hat2(0),
  _hat3(0),
  _hat4(0),
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

void BleGamepad::setAxes(int16_t x, int16_t y, int16_t z, int16_t rZ, uint16_t rX, uint16_t rY, uint16_t slider1, uint16_t slider2, signed char hat1, signed char hat2, signed char hat3, signed char hat4)
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
	_slider1 = slider1;
	_slider2 = slider2;
	_hat1 = hat1;
	_hat2 = hat2;
	_hat3 = hat3;
	_hat4 = hat4;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setHats(signed char hat1, signed char hat2, signed char hat3, signed char hat4)
{
	_hat1 = hat1;
	_hat2 = hat2;
	_hat3 = hat3;
	_hat4 = hat4;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setSliders(uint16_t slider1, uint16_t slider2)
{	
	_slider1 = slider1;
	_slider2 = slider2;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::sendReport(void)
{
	if (this->isConnected())
	{
		uint8_t m[28];
		m[0] = _buttons;
		m[1] = (_buttons >> 8);
		m[2] = (_buttons >> 16);
		m[3] = (_buttons >> 24);
		m[4] = (_buttons >> 32);
		m[5] = (_buttons >> 40);
		m[6] = (_buttons >> 48);
		m[7] = (_buttons >> 56);
		m[8] = _x;
		m[9] = (_x >> 8);
		m[10] = _y;
		m[11] = (_y >> 8);
		m[12] = _z;
		m[13] = (_z >> 8);
		m[14] = _rZ;
		m[15] = (_rZ >> 8);
		m[16] = _rX;
		m[17] = (_rX >> 8);
		m[18] = _rY;
		m[19] = (_rY >> 8);
		m[20] = _slider1;
		m[21] = (_slider1 >> 8);
		m[22] = _slider2;
		m[23] = (_slider2 >> 8);
		m[24] = _hat4;
		m[25] = _hat3;
		m[26] = _hat2;
		m[27] = _hat1;
		this->inputGamepad->setValue(m, sizeof(m));
		this->inputGamepad->notify();
	}
}
void BleGamepad::buttons(uint64_t b)
{
  if (b != _buttons)
  {
    _buttons = b;
	
	if(_autoReport){ sendReport(); }
  }
}

void BleGamepad::press(uint64_t b)
{
  buttons(_buttons | b);
}

void BleGamepad::release(uint64_t b)
{
  buttons(_buttons & ~b);
}

void BleGamepad::setLeftThumb(int16_t x, int16_t y)
{
	if(x == -32768) { x = -32767; }
	if(y == -32768) { y = -32767; }
	
	_x = x;
	_y = y;
	
	if(_autoReport){ sendReport(); }
}
void BleGamepad::setRightThumb(int16_t z, int16_t rZ)
{
	if(z == -32768) { z = -32767; }
	if(rZ == -32768) { rZ = -32767; }
	
	_z = z;
	_rZ = rZ;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setLeftTrigger(uint16_t rX)
{
	_rX = rX;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setRightTrigger(uint16_t rY)
{
	_rY = rY;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setHat(signed char hat)
{
	_hat1 = hat;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setHat1(signed char hat1)
{
	_hat1 = hat1;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setHat2(signed char hat2)
{
	_hat2 = hat2;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setHat3(signed char hat3)
{
	_hat3 = hat3;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setHat4(signed char hat4)
{
	_hat4 = hat4;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setX(int16_t x)
{
	if(x == -32768) { x = -32767; }

	_x = x;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setY(int16_t y)
{
	if(y == -32768) { y = -32767; }
	
	_y = y;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setZ(int16_t z)
{
	if(z == -32768) { z = -32767; }
	
	_z = z;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setRZ(int16_t rZ)
{
	if(rZ == -32768) { rZ = -32767; }
	
	_rZ = rZ;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setRX(uint16_t rX)
{
	_rX = rX;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setRY(uint16_t rY)
{
	_rY = rY;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setSlider(uint16_t slider)
{
	_slider1 = slider;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setSlider1(uint16_t slider1)
{
	_slider1 = slider1;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setSlider2(uint16_t slider2)
{
	_slider2 = slider2;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setAutoReport(bool autoReport)
{
	_autoReport = autoReport;
}

bool BleGamepad::isPressed(uint64_t b)
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

  BleGamepadInstance->hid->pnp(0x01,0x02e5,0xabbb,0x0110);
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
