#ifndef ESP32_BLE_GAMEPAD_H
#define ESP32_BLE_GAMEPAD_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "BleConnectionStatus.h"
#include "BLEHIDDevice.h"
#include "BLECharacteristic.h"

#define BUTTON_1 	1
#define BUTTON_2 	2
#define BUTTON_3 	4
#define BUTTON_4 	8
#define BUTTON_5 	16
#define BUTTON_6 	32
#define BUTTON_7 	64
#define BUTTON_8 	128
#define BUTTON_9 	256
#define BUTTON_10 	512
#define BUTTON_11 	1024
#define BUTTON_12 	2048
#define BUTTON_13 	4096
#define BUTTON_14 	8192

#define DPAD_CENTERED 	0
#define DPAD_UP 		1
#define DPAD_UP_RIGHT 	2
#define DPAD_RIGHT 		3
#define DPAD_DOWN_RIGHT 4
#define DPAD_DOWN 		5
#define DPAD_DOWN_LEFT 	6
#define DPAD_LEFT 		7
#define DPAD_UP_LEFT 	8

class BleGamepad {
private:
  uint16_t _buttons;
  BleConnectionStatus* connectionStatus;
  BLEHIDDevice* hid;
  BLECharacteristic* inputGamepad;
  void buttons(uint16_t b);
  void rawAction(uint8_t msg[], char msgSize);
  static void taskServer(void* pvParameter);
public:
  BleGamepad(std::string deviceName = "ESP32 BLE Gamepad", std::string deviceManufacturer = "Espressif", uint8_t batteryLevel = 100);
  void begin(void);
  void end(void);
  void setAxes(signed char x, signed char y, signed char z = 0, signed char rZ = 0, char rX = 0, char rY = 0, signed char hat = 0);
  void press(uint16_t b = BUTTON_1);   // press BUTTON_1 by default
  void release(uint16_t b = BUTTON_1); // release BUTTON_1 by default
  bool isPressed(uint16_t b = BUTTON_1); // check BUTTON_1 by default
  bool isConnected(void);
  void setBatteryLevel(uint8_t level);
  uint8_t batteryLevel;
  std::string deviceManufacturer;
  std::string deviceName;
protected:
  virtual void onStarted(BLEServer *pServer) { };
};

#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_GAMEPAD_H
