#ifndef ESP32_BLE_GAMEPAD_H
#define ESP32_BLE_GAMEPAD_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include "BleConnectionStatus.h"
#include "NimBLEHIDDevice.h"
#include "NimBLECharacteristic.h"

#define BUTTON_1    (1<<0)		// 1
#define BUTTON_2    (1<<1)		// 2
#define BUTTON_3    (1<<2)		// 4
#define BUTTON_4    (1<<3)		// 8
#define BUTTON_5    (1<<4)		// 16
#define BUTTON_6    (1<<5)		// 32
#define BUTTON_7    (1<<6)		// 64
#define BUTTON_8    (1<<7)		// 128
#define BUTTON_9    (1<<8)		// 256
#define BUTTON_10 	(1<<9)		// 512
#define BUTTON_11 	(1<<10)		// 1024
#define BUTTON_12 	(1<<11)		// 2048
#define BUTTON_13 	(1<<12)		// 4096
#define BUTTON_14 	(1<<13)		// 8192
#define BUTTON_15 	(1<<14)		// 16384
#define BUTTON_16 	(1<<15)		// 32768

#define BUTTON_17   (1<<16)		// 65536
#define BUTTON_18   (1<<17)		// 131072
#define BUTTON_19   (1<<18)		// 262144
#define BUTTON_20   (1<<19)		// 524288
#define BUTTON_21   (1<<20)		// 1048576
#define BUTTON_22   (1<<21)		// 2097152
#define BUTTON_23   (1<<22)		// 4194304
#define BUTTON_24   (1<<23)		// 8388608
#define BUTTON_25   (1<<24)		// 16777216
#define BUTTON_26   (1<<25)		// 33554432
#define BUTTON_27   (1<<26)		// 67108864
#define BUTTON_28   (1<<27)		// 134217728
#define BUTTON_29   (1<<28)		// 268435456
#define BUTTON_30   (1<<29)		// 536870912
#define BUTTON_31   (1<<30)		// 1073741824
#define BUTTON_32   (1LL<<31)		// 2147483648

#define BUTTON_33   (1LL<<32)
#define BUTTON_34   (1LL<<33)
#define BUTTON_35   (1LL<<34)
#define BUTTON_36   (1LL<<35)
#define BUTTON_37   (1LL<<36)
#define BUTTON_38   (1LL<<37)
#define BUTTON_39   (1LL<<38)
#define BUTTON_40   (1LL<<39)
#define BUTTON_41   (1LL<<40)
#define BUTTON_42   (1LL<<41)
#define BUTTON_43   (1LL<<42)
#define BUTTON_44   (1LL<<43)
#define BUTTON_45   (1LL<<44)
#define BUTTON_46   (1LL<<45)
#define BUTTON_47   (1LL<<46)
#define BUTTON_48   (1LL<<47)

#define BUTTON_49   (1LL<<48)
#define BUTTON_50   (1LL<<49)
#define BUTTON_51   (1LL<<50)
#define BUTTON_52   (1LL<<51)
#define BUTTON_53   (1LL<<52)
#define BUTTON_54   (1LL<<53)
#define BUTTON_55   (1LL<<54)
#define BUTTON_56   (1LL<<55)
#define BUTTON_57   (1LL<<56)
#define BUTTON_58   (1LL<<57)
#define BUTTON_59   (1LL<<58)
#define BUTTON_60   (1LL<<59)
#define BUTTON_61   (1LL<<60)
#define BUTTON_62   (1LL<<61)
#define BUTTON_63   (1LL<<62)
#define BUTTON_64   (1LL<<63)


#define DPAD_CENTERED 	0
#define DPAD_UP 		1
#define DPAD_UP_RIGHT 	2
#define DPAD_RIGHT 		3
#define DPAD_DOWN_RIGHT 4
#define DPAD_DOWN 		5
#define DPAD_DOWN_LEFT 	6
#define DPAD_LEFT 		7
#define DPAD_UP_LEFT 	8

#define HAT_CENTERED 	0
#define HAT_UP 			1
#define HAT_UP_RIGHT 	2
#define HAT_RIGHT 		3
#define HAT_DOWN_RIGHT 	4
#define HAT_DOWN 		5
#define HAT_DOWN_LEFT 	6
#define HAT_LEFT 		7
#define HAT_UP_LEFT 	8

class BleGamepad {
private:
  uint64_t _buttons;
  int16_t _x;
  int16_t _y;
  int16_t _z;
  int16_t _rZ;
  uint16_t _rX;
  uint16_t _rY;
  uint16_t _slider1;
  uint16_t _slider2;
  int16_t _hat1;
  int16_t _hat2;
  int16_t _hat3;
  int16_t _hat4;
  bool _autoReport;
  
  BleConnectionStatus* connectionStatus;

  NimBLEHIDDevice* hid;
  NimBLECharacteristic* inputGamepad;
  
  void buttons(uint64_t b);

  void rawAction(uint8_t msg[], char msgSize);
  static void taskServer(void* pvParameter);
public:
  BleGamepad(std::string deviceName = "ESP32 BLE Gamepad", std::string deviceManufacturer = "Espressif", uint8_t batteryLevel = 100);
  void begin(bool autoReport = true);
  void end(void);
  void setAxes(int16_t x = 0, int16_t y = 0, int16_t z = 0, int16_t rZ = 0, uint16_t rX = 0, uint16_t rY = 0, uint16_t slider1 = 0, uint16_t slider2 = 0, signed char hat1 = 0, signed char hat2 = 0, signed char hat3 = 0, signed char hat4 = 0);
  void press(uint64_t b = BUTTON_1);   // press BUTTON_1 by default
  void release(uint64_t b = BUTTON_1); // release BUTTON_1 by default
  void setLeftThumb(int16_t x = 0, int16_t y = 0);
  void setRightThumb(int16_t z = 0, int16_t rZ = 0);
  void setLeftTrigger(uint16_t rX = 0);
  void setRightTrigger(uint16_t rY = 0);
  void setHats(signed char hat1 = 0, signed char hat2 = 0, signed char hat3 = 0, signed char hat4 = 0);
  void setHat(signed char hat = 0);
  void setHat1(signed char hat1 = 0);
  void setHat2(signed char hat2 = 0);
  void setHat3(signed char hat3 = 0);
  void setHat4(signed char hat4 = 0);
  void setX(int16_t x = 0);
  void setY(int16_t y = 0);
  void setZ(int16_t z = 0);
  void setRZ(int16_t rZ = 0);
  void setRX(uint16_t rX = 0);
  void setRY(uint16_t rY = 0);
  void setSliders(uint16_t slider1 = 0, uint16_t slider2 = 0);
  void setSlider(uint16_t slider = 0);
  void setSlider1(uint16_t slider1 = 0);
  void setSlider2(uint16_t slider2 = 0);
  void setAutoReport(bool autoReport = true);
  void sendReport();
  bool isPressed(uint64_t b = BUTTON_1); // check BUTTON_1 by default
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
