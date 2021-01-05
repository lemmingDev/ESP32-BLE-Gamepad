#ifndef ESP32_BLE_GAMEPAD_H
#define ESP32_BLE_GAMEPAD_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include "BleConnectionStatus.h"
#include "NimBLEHIDDevice.h"
#include "NimBLECharacteristic.h"

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
#define BUTTON_15 	16384
#define BUTTON_16 	32768
#define BUTTON_17 	65536
#define BUTTON_18 	131072
#define BUTTON_19 	262144
#define BUTTON_20 	524288
#define BUTTON_21 	1048576
#define BUTTON_22 	2097152
#define BUTTON_23 	4194304
#define BUTTON_24 	8388608
#define BUTTON_25 	16777216
#define BUTTON_26 	33554432
#define BUTTON_27 	67108864
#define BUTTON_28 	134217728
#define BUTTON_29 	268435456
#define BUTTON_30 	536870912
#define BUTTON_31 	1073741824
#define BUTTON_32 	2147483648

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
  uint32_t _buttons;
  BleConnectionStatus* connectionStatus;
  NimBLEHIDDevice* hid;
  NimBLECharacteristic* inputGamepad;
  void buttons(uint32_t b);
  void rawAction(uint8_t msg[], char msgSize);
  static void taskServer(void* pvParameter);
public:
  BleGamepad(std::string deviceName = "ESP32-Gamepad", std::string deviceManufacturer = "Espressif", uint8_t batteryLevel = 100);
  void begin(void);
  void end(void);
  void setAxes(int16_t x, int16_t y, int16_t z = 0, int16_t rZ = 0, char rX = 0, char rY = 0, signed char hat = 0);
  void press(uint32_t b = BUTTON_1);   // press BUTTON_1 by default
  void release(uint32_t b = BUTTON_1); // release BUTTON_1 by default
  bool isPressed(uint32_t b = BUTTON_1); // check BUTTON_1 by default
  bool isConnected(void);
  void setBatteryLevel(uint8_t level);
  uint8_t batteryLevel;
  std::string deviceManufacturer;
  std::string deviceName;
protected:
  virtual void onStarted(NimBLEServer *pServer) { };
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_GAMEPAD_H
