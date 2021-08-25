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

uint8_t tempHidReportDescriptor[150];
int hidReportDescriptorSize = 0;
uint8_t _hidReportId = 3;
uint8_t reportSize = 0;
uint8_t numOfButtonBytes = 0;

BleGamepad::BleGamepad(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel) :
  _controllerType(CONTROLLER_TYPE_GAMEPAD),
  _buttons(),
  _x(0),
  _y(0),
  _z(0),
  _rZ(0),
  _rX(0),
  _rY(0),
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
  _autoReport(true),
  _buttonCount(0),
  _hatSwitchCount(0),
  _includeXAxis(true),
  _includeYAxis(true),
  _includeZAxis(true),
  _includeRxAxis(true),
  _includeRyAxis(true),
  _includeRzAxis(true),
  _includeSlider1(true),
  _includeSlider2(true),
  _includeRudder(false),
  _includeThrottle(false),
  _includeAccelerator(false),
  _includeBrake(false),
  _includeSteering(false),
  hid(0)
{
  this->resetButtons();
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
  this->connectionStatus = new BleConnectionStatus();
}

void BleGamepad::resetButtons() {
  memset(&_buttons,0,sizeof(_buttons));
}

void BleGamepad::setControllerType(uint8_t controllerType){
	_controllerType = controllerType;
}
  
  
void BleGamepad::begin(uint16_t buttonCount, uint8_t hatSwitchCount, bool includeXAxis, bool includeYAxis, bool includeZAxis, bool includeRzAxis, bool includeRxAxis, bool includeRyAxis, bool includeSlider1, bool includeSlider2, bool includeRudder, bool includeThrottle, bool includeAccelerator, bool includeBrake, bool includeSteering)
{
	_buttonCount = buttonCount;
	_hatSwitchCount = hatSwitchCount;
	
	_includeXAxis = includeXAxis;
	_includeYAxis = includeYAxis;
	_includeZAxis = includeZAxis;
	_includeRzAxis = includeRzAxis;
	_includeRxAxis = includeRxAxis;
	_includeRyAxis = includeRyAxis;
	_includeSlider1 = includeSlider1;
	_includeSlider2 = includeSlider2;
	
	_includeRudder = includeRudder;
	_includeThrottle = includeThrottle;
	_includeAccelerator = includeAccelerator;
	_includeBrake = includeBrake;
	_includeSteering = includeSteering;
	
	uint8_t axisCount = 0;
	if (_includeXAxis){ axisCount++; }
	if (_includeYAxis){ axisCount++; }
	if (_includeZAxis){ axisCount++; }
	if (_includeRzAxis){ axisCount++; }
	if (_includeRxAxis){ axisCount++; }
	if (_includeRyAxis){ axisCount++; }
	if (_includeSlider1){ axisCount++; }
	if (_includeSlider2){ axisCount++; }
	
	uint8_t simulationCount = 0;
	if (_includeRudder){ simulationCount++; }
	if (_includeThrottle){ simulationCount++; }
	if (_includeAccelerator){ simulationCount++; }
	if (_includeBrake){ simulationCount++; }
	if (_includeSteering){ simulationCount++; }
	
	
	uint8_t buttonPaddingBits = 8 - (_buttonCount % 8);
	if(buttonPaddingBits == 8){buttonPaddingBits = 0;}
	uint8_t numOfAxisBytes = axisCount * 2;
	uint8_t numOfSimulationBytes = simulationCount * 2;
	
	numOfButtonBytes = _buttonCount / 8;
	if( buttonPaddingBits > 0){numOfButtonBytes++;}
	
	reportSize = numOfButtonBytes + numOfAxisBytes + numOfSimulationBytes + _hatSwitchCount;
	
	
    // USAGE_PAGE (Generic Desktop)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // USAGE (Joystick - 0x04; Gamepad - 0x05; Multi-axis Controller - 0x08)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
    tempHidReportDescriptor[hidReportDescriptorSize++] = _controllerType;

    // COLLECTION (Application)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xa1;
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

    // REPORT_ID (Default: 3)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0x85;
    tempHidReportDescriptor[hidReportDescriptorSize++] = _hidReportId;
	
	if (_buttonCount > 0) {

		// USAGE_PAGE (Button)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;

		// USAGE_MINIMUM (Button 1)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x19;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

		// USAGE_MAXIMUM (Up to 128 buttons possible)            
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x29;
		tempHidReportDescriptor[hidReportDescriptorSize++] = _buttonCount;

		// LOGICAL_MINIMUM (0)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x15;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

		// LOGICAL_MAXIMUM (1)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x25;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

		// REPORT_SIZE (1)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

		// REPORT_COUNT (# of buttons)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
		tempHidReportDescriptor[hidReportDescriptorSize++] = _buttonCount;

		// UNIT_EXPONENT (0)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x55;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

		// UNIT (None)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x65;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

		// INPUT (Data,Var,Abs)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;

		if (buttonPaddingBits > 0) {
			
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
	
	if (axisCount > 0) {
		// USAGE_PAGE (Generic Desktop)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
	
		// USAGE (Pointer)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;

		// LOGICAL_MINIMUM (-32767)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x80;

		// LOGICAL_MAXIMUM (+32767)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x7F;

		// REPORT_SIZE (16)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

		// REPORT_COUNT (axisCount)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
		tempHidReportDescriptor[hidReportDescriptorSize++] = axisCount;
						
		// COLLECTION (Physical)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

		if (_includeXAxis == true) {
			// USAGE (X)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x30;
		}

		if (_includeYAxis == true) {
			// USAGE (Y)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x31;
		}
		
		if (_includeZAxis == true) {
			// USAGE (Z)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x32;
		}
		
		if (_includeRzAxis == true) {
			// USAGE (Rz)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x35;
		}
		
		if (_includeRxAxis == true) {
			// USAGE (Rx)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x33;
		}
		
		if (_includeRyAxis == true) {
			// USAGE (Ry)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x34;
		}
		
		
		if (_includeSlider1 == true) {
			// USAGE (Slider)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x36;
		}
		
		if (_includeSlider2 == true) {
			// USAGE (Slider)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x36;
		}
		
		// INPUT (Data,Var,Abs)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
		
		// END_COLLECTION (Physical)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
		
	} // X, Y, Z, Rx, Ry, and Rz Axis	
	
	if (simulationCount > 0) {
	
		// USAGE_PAGE (Simulation Controls)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x05;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x02;
		
		// LOGICAL_MINIMUM (-32767)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x16;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x01;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x80;

		// LOGICAL_MAXIMUM (+32767)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x26;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0xFF;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x7F;

		// REPORT_SIZE (16)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x75;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x10;

		// REPORT_COUNT (simulationCount)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x95;
		tempHidReportDescriptor[hidReportDescriptorSize++] = simulationCount;

		// COLLECTION (Physical)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;

		if (includeRudder == true) {
			// USAGE (Rudder)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0xBA;
		}

		if (includeThrottle == true) {
			// USAGE (Throttle)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0xBB;
		}

		if (includeAccelerator == true) {
			// USAGE (Accelerator)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC4;
		}

		if (includeBrake == true) {
			// USAGE (Brake)
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0x09;
			tempHidReportDescriptor[hidReportDescriptorSize++] = 0xC5;
		}

		if (includeSteering == true) {
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
	
	if(_hatSwitchCount > 0){
		
		// COLLECTION (Physical)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0xA1;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x00;
		
		// USAGE_PAGE (Generic Desktop)
		tempHidReportDescriptor[hidReportDescriptorSize++] =   USAGE_PAGE(1);
		tempHidReportDescriptor[hidReportDescriptorSize++] =  0x01;
		
		// USAGE (Hat Switch)
		for(int currentHatIndex = 0 ; currentHatIndex < _hatSwitchCount ; currentHatIndex++){
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
		tempHidReportDescriptor[hidReportDescriptorSize++] = _hatSwitchCount;
		
		// Input (Data, Variable, Absolute)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x81;
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0x42;
		
		// END_COLLECTION (Physical)
		tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
	}
	
	// END_COLLECTION (Application)
    tempHidReportDescriptor[hidReportDescriptorSize++] = 0xc0;
	
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

void BleGamepad::end(void)
{
}

void BleGamepad::setAxes(int16_t x, int16_t y, int16_t z, int16_t rZ, int16_t rX, int16_t rY, int16_t slider1, int16_t slider2, signed char hat1, signed char hat2, signed char hat3, signed char hat4)
{
	if(x == -32768) { x = -32767; }
	if(y == -32768) { y = -32767; }
	if(z == -32768) { z = -32767; }
	if(rZ == -32768) { rZ = -32767; }
	if(rX == -32768) { rX = -32767; }
	if(rY == -32768) { rY = -32767; }
	if(slider1 == -32768) { slider1 = -32767; }
	if(slider2 == -32768) { slider2 = -32767; }

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

void BleGamepad::setSimulationControls(int16_t rudder, int16_t throttle, int16_t accelerator, int16_t brake, int16_t steering)
{
  if(rudder == -32768) { rudder = -32767; }
  if(throttle == -32768) { throttle = -32767; }
  if(accelerator == -32768) { accelerator = -32767; }
  if(brake == -32768) { brake = -32767; }
  if(steering == -32768) { steering = -32767; }
  
  _rudder = rudder;
  _throttle = throttle;
  _accelerator = accelerator;
  _brake = brake;
  _steering = steering;
	
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

void BleGamepad::setSliders(int16_t slider1, int16_t slider2)
{	
  if(slider1 == -32768) { slider1 = -32767; }
  if(slider2 == -32768) { slider2 = -32767; }
  
  _slider1 = slider1;
  _slider2 = slider2;

  if(_autoReport){ sendReport(); }
}

void BleGamepad::sendReport(void)
{
	if (this->isConnected())
	{
		uint8_t currentReportIndex = 0;
		
		uint8_t m[reportSize];
		
		memset(&m,0,sizeof(m));
		memcpy(&m, &_buttons, sizeof(_buttons));
		
		currentReportIndex+=numOfButtonBytes;
		
 		if(_includeXAxis){ m[currentReportIndex++] = _x;	m[currentReportIndex++] = (_x >> 8); }
		if(_includeYAxis){ m[currentReportIndex++] = _y;	m[currentReportIndex++] = (_y >> 8); }
		if(_includeZAxis){ m[currentReportIndex++] = _z;	m[currentReportIndex++] = (_z >> 8); }
		if(_includeRzAxis){ m[currentReportIndex++] = _rZ; 	m[currentReportIndex++] = (_rZ >> 8); }
		if(_includeRxAxis){ m[currentReportIndex++] = _rX; 	m[currentReportIndex++] = (_rX >> 8); }
		if(_includeRyAxis){ m[currentReportIndex++] = _rY; 	m[currentReportIndex++] = (_rY >> 8); }
		
		if(_includeSlider1){ m[currentReportIndex++] = _slider1;	m[currentReportIndex++] = (_slider1 >> 8); }
		if(_includeSlider2){ m[currentReportIndex++] = _slider2;	m[currentReportIndex++] = (_slider2 >> 8); }
  
		if(_includeRudder){ m[currentReportIndex++] = _rudder;	m[currentReportIndex++] = (_rudder >> 8); }
		if(_includeThrottle){ m[currentReportIndex++] = _throttle;	m[currentReportIndex++] = (_throttle >> 8); }
		if(_includeAccelerator){ m[currentReportIndex++] = _accelerator;	m[currentReportIndex++] = (_accelerator >> 8); }
		if(_includeBrake){ m[currentReportIndex++] = _brake; 	m[currentReportIndex++] = (_brake >> 8); }
		if(_includeSteering){ m[currentReportIndex++] = _steering; 	m[currentReportIndex++] = (_steering >> 8); }
		
		if(_hatSwitchCount > 0)
		{
			signed char hats[4];
			
			hats[0] = _hat1;
			hats[1] = _hat2;
			hats[2] = _hat3;
			hats[3] = _hat4;
	        	
			for(int currentHatIndex = _hatSwitchCount -1 ; currentHatIndex >= 0 ; currentHatIndex--)
			{
				m[currentReportIndex++] = hats[currentHatIndex];
			}
		}
		
		this->inputGamepad->setValue(m, sizeof(m));
		this->inputGamepad->notify();
	}
}

void BleGamepad::press(uint8_t b)
{

  uint8_t index = (b-1) / 8;
  uint8_t bit = (b-1) % 8;
  uint8_t bitmask = (1 << bit);

  uint8_t result = _buttons[index] | bitmask;
  
  if (result != _buttons[index]) 
  {
    _buttons[index] = result;
  }
  
  if(_autoReport){ sendReport(); }
}

void BleGamepad::release(uint8_t b)
{
  uint8_t index = (b-1) / 8;
  uint8_t bit = (b-1) % 8;
  uint8_t bitmask = (1 << bit);

  uint64_t result = _buttons[index] & ~bitmask;
  
  if (result != _buttons[index]) 
  {
    _buttons[index] = result;
  }
  
  if(_autoReport){ sendReport(); }

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

void BleGamepad::setLeftTrigger(int16_t rX)
{
	if(rX == -32768) {rX = -32767; }
	
	_rX = rX;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setRightTrigger(int16_t rY)
{
	if(rY == -32768) {rY = -32767; }
	
	_rY = rY;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setTriggers(int16_t rX, int16_t rY)
{
	if(rX == -32768) {rX = -32767; }
	if(rY == -32768) {rY = -32767; }
	
	_rX = rX;
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

void BleGamepad::setRX(int16_t rX)
{
	if(rX == -32768) { rX = -32767; }
	
	_rX = rX;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setRY(int16_t rY)
{
	if(rY == -32768) { rY = -32767; }
	
	_rY = rY;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setSlider(int16_t slider)
{
	if(slider == -32768) { slider = -32767; }
	
	_slider1 = slider;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setSlider1(int16_t slider1)
{
	if(slider1 == -32768) { slider1 = -32767; }
	
	_slider1 = slider1;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setSlider2(int16_t slider2)
{
	if(slider2 == -32768) { slider2 = -32767; }
	
	_slider2 = slider2;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setRudder(int16_t rudder)
{
	if(rudder == -32768) { rudder = -32767; }
	
	_rudder = rudder;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setThrottle(int16_t throttle)
{
	if(throttle == -32768) { throttle = -32767; }
	
	_throttle = throttle;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setAccelerator(int16_t accelerator)
{
	if(accelerator == -32768) { accelerator = -32767; }
	
	_accelerator = accelerator;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setBrake(int16_t brake)
{
	if(brake == -32768) { brake = -32767; }
	
	_brake = brake;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setSteering(int16_t steering)
{
	if(steering == -32768) { steering = -32767; }
	
	_steering = steering;
	
	if(_autoReport){ sendReport(); }
}

void BleGamepad::setAutoReport(bool autoReport)
{
	_autoReport = autoReport;
}

bool BleGamepad::isPressed(uint8_t b)
{
  uint8_t index = (b-1) / 8;
  uint8_t bit = (b-1) % 8;
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
    this->hid->setBatteryLevel(this->batteryLevel);
}

void BleGamepad::taskServer(void* pvParameter) 
{
  BleGamepad* BleGamepadInstance = (BleGamepad *) pvParameter; //static_cast<BleGamepad *>(pvParameter);
  NimBLEDevice::init(BleGamepadInstance->deviceName);
  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(BleGamepadInstance->connectionStatus);

  BleGamepadInstance->hid = new NimBLEHIDDevice(pServer);
  BleGamepadInstance->inputGamepad = BleGamepadInstance->hid->inputReport(_hidReportId); // <-- input REPORTID from report map
  BleGamepadInstance->connectionStatus->inputGamepad = BleGamepadInstance->inputGamepad;

  BleGamepadInstance->hid->manufacturer()->setValue(BleGamepadInstance->deviceManufacturer);

  BleGamepadInstance->hid->pnp(0x01,0x02e5,0xabbb,0x0110);
  BleGamepadInstance->hid->hidInfo(0x00,0x01);

  NimBLESecurity *pSecurity = new NimBLESecurity();

  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);


  uint8_t *customHidReportDescriptor = new uint8_t[hidReportDescriptorSize];
  memcpy(customHidReportDescriptor, tempHidReportDescriptor, hidReportDescriptorSize);

  BleGamepadInstance->hid->reportMap((uint8_t*)customHidReportDescriptor, hidReportDescriptorSize);
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
