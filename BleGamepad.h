#ifndef ESP32_BLE_GAMEPAD_H
#define ESP32_BLE_GAMEPAD_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include "BleConnectionStatus.h"
#include "NimBLEHIDDevice.h"
#include "NimBLECharacteristic.h"

#define CONTROLLER_TYPE_JOYSTICK	0x04
#define CONTROLLER_TYPE_GAMEPAD		0x05
#define CONTROLLER_TYPE_MULTI_AXIS	0x08

#define BUTTON_1      0x1
#define BUTTON_2      0x2
#define BUTTON_3      0x3
#define BUTTON_4      0x4
#define BUTTON_5      0x5
#define BUTTON_6      0x6
#define BUTTON_7      0x7
#define BUTTON_8      0x8

#define BUTTON_9      0x9
#define BUTTON_10     0xa
#define BUTTON_11     0xb
#define BUTTON_12     0xc
#define BUTTON_13     0xd
#define BUTTON_14     0xe
#define BUTTON_15     0xf
#define BUTTON_16    0x10

#define BUTTON_17    0x11
#define BUTTON_18    0x12
#define BUTTON_19    0x13
#define BUTTON_20    0x14
#define BUTTON_21    0x15
#define BUTTON_22    0x16
#define BUTTON_23    0x17
#define BUTTON_24    0x18

#define BUTTON_25    0x19
#define BUTTON_26    0x1a
#define BUTTON_27    0x1b
#define BUTTON_28    0x1c
#define BUTTON_29    0x1d
#define BUTTON_30    0x1e
#define BUTTON_31    0x1f
#define BUTTON_32    0x20

#define BUTTON_33    0x21
#define BUTTON_34    0x22
#define BUTTON_35    0x23
#define BUTTON_36    0x24
#define BUTTON_37    0x25
#define BUTTON_38    0x26
#define BUTTON_39    0x27
#define BUTTON_40    0x28

#define BUTTON_41    0x29
#define BUTTON_42    0x2a
#define BUTTON_43    0x2b
#define BUTTON_44    0x2c
#define BUTTON_45    0x2d
#define BUTTON_46    0x2e
#define BUTTON_47    0x2f
#define BUTTON_48    0x30

#define BUTTON_49    0x31
#define BUTTON_50    0x32
#define BUTTON_51    0x33
#define BUTTON_52    0x34
#define BUTTON_53    0x35
#define BUTTON_54    0x36
#define BUTTON_55    0x37
#define BUTTON_56    0x38

#define BUTTON_57    0x39
#define BUTTON_58    0x3a
#define BUTTON_59    0x3b
#define BUTTON_60    0x3c
#define BUTTON_61    0x3d
#define BUTTON_62    0x3e
#define BUTTON_63    0x3f
#define BUTTON_64    0x40

#define BUTTON_65    0x41
#define BUTTON_66    0x42
#define BUTTON_67    0x43
#define BUTTON_68    0x44
#define BUTTON_69    0x45
#define BUTTON_70    0x46
#define BUTTON_71    0x47
#define BUTTON_72    0x48

#define BUTTON_73    0x49
#define BUTTON_74    0x4a
#define BUTTON_75    0x4b
#define BUTTON_76    0x4c
#define BUTTON_77    0x4d
#define BUTTON_78    0x4e
#define BUTTON_79    0x4f
#define BUTTON_80    0x50

#define BUTTON_81    0x51
#define BUTTON_82    0x52
#define BUTTON_83    0x53
#define BUTTON_84    0x54
#define BUTTON_85    0x55
#define BUTTON_86    0x56
#define BUTTON_87    0x57
#define BUTTON_88    0x58

#define BUTTON_89    0x59
#define BUTTON_90    0x5a
#define BUTTON_91    0x5b
#define BUTTON_92    0x5c
#define BUTTON_93    0x5d
#define BUTTON_94    0x5e
#define BUTTON_95    0x5f
#define BUTTON_96    0x60

#define BUTTON_97    0x61
#define BUTTON_98    0x62
#define BUTTON_99    0x63
#define BUTTON_100   0x64
#define BUTTON_101   0x65
#define BUTTON_102   0x66
#define BUTTON_103   0x67
#define BUTTON_104   0x68

#define BUTTON_105   0x69
#define BUTTON_106   0x6a
#define BUTTON_107   0x6b
#define BUTTON_108   0x6c
#define BUTTON_109   0x6d
#define BUTTON_110   0x6e
#define BUTTON_111   0x6f
#define BUTTON_112   0x70

#define BUTTON_113   0x71
#define BUTTON_114   0x72
#define BUTTON_115   0x73
#define BUTTON_116   0x74
#define BUTTON_117   0x75
#define BUTTON_118   0x76
#define BUTTON_119   0x77
#define BUTTON_120   0x78

#define BUTTON_121   0x79
#define BUTTON_122   0x7a
#define BUTTON_123   0x7b
#define BUTTON_124   0x7c
#define BUTTON_125   0x7d
#define BUTTON_126   0x7e
#define BUTTON_127   0x7f
#define BUTTON_128   0x80


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
  uint8_t  _controllerType;
  uint8_t _buttons[16];	//8 bytes x 16 --> 128 bytes 
  int16_t _x;
  int16_t _y;
  int16_t _z;
  int16_t _rZ;
  int16_t _rX;
  int16_t _rY;
  int16_t _slider1;
  int16_t _slider2;
  int16_t _rudder;
  int16_t _throttle;
  int16_t _accelerator;
  int16_t _brake;
  int16_t _steering;
  int16_t _hat1;
  int16_t _hat2;
  int16_t _hat3;
  int16_t _hat4;
  bool _autoReport;
  uint16_t _buttonCount;
  uint8_t _hatSwitchCount;
  bool _includeXAxis;
  bool _includeYAxis;
  bool _includeZAxis;
  bool _includeRxAxis;
  bool _includeRyAxis;
  bool _includeRzAxis;
  bool _includeSlider1;
  bool _includeSlider2;
  bool _includeRudder;
  bool _includeThrottle;
  bool _includeAccelerator;
  bool _includeBrake;
  bool _includeSteering;
  
  BleConnectionStatus* connectionStatus;

  NimBLEHIDDevice* hid;
  NimBLECharacteristic* inputGamepad;

  void rawAction(uint8_t msg[], char msgSize);
  static void taskServer(void* pvParameter);

public:
  BleGamepad(std::string deviceName = "ESP32 BLE Gamepad", std::string deviceManufacturer = "Espressif", uint8_t batteryLevel = 100);
  void begin(uint16_t buttonCount = 16, uint8_t hatSwitchCount = 1, bool includeXAxis = true, bool includeYAxis = true, bool includeZAxis = true, bool includeRzAxis = true, bool includeRxAxis = true, bool includeRyAxis = true, bool includeSlider1 = true, bool includeSlider2 = true, bool includeRudder = false, bool includeThrottle = false, bool includeAccelerator = false, bool includeBrake = false, bool includeSteering = false);
  void end(void);
  void setControllerType(uint8_t controllerType = CONTROLLER_TYPE_GAMEPAD);
  void setAxes(int16_t x = 0, int16_t y = 0, int16_t z = 0, int16_t rZ = 0, int16_t rX = 0, int16_t rY = 0, int16_t slider1 = 0, int16_t slider2 = 0, signed char hat1 = 0, signed char hat2 = 0, signed char hat3 = 0, signed char hat4 = 0);
  void press(uint8_t b = BUTTON_1);   // press BUTTON_1 by default
  void release(uint8_t b = BUTTON_1); // release BUTTON_1 by default
  void setLeftThumb(int16_t x = 0, int16_t y = 0);
  void setRightThumb(int16_t z = 0, int16_t rZ = 0);
  void setLeftTrigger(int16_t rX = 0);
  void setRightTrigger(int16_t rY = 0);
  void setTriggers(int16_t rX = 0, int16_t rY = 0);
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
  void setRX(int16_t rX = 0);
  void setRY(int16_t rY = 0);
  void setSliders(int16_t slider1 = 0, int16_t slider2 = 0);
  void setSlider(int16_t slider = 0);
  void setSlider1(int16_t slider1 = 0);
  void setSlider2(int16_t slider2 = 0);
  void setRudder(int16_t rudder = 0);
  void setThrottle(int16_t throttle = 0);
  void setAccelerator(int16_t accelerator = 0);
  void setBrake(int16_t brake = 0);
  void setSteering(int16_t steering = 0); 
  void setSimulationControls(int16_t rudder = 0, int16_t throttle = 0, int16_t accelerator = 0, int16_t brake = 0, int16_t steering = 0);
  void setAutoReport(bool autoReport = true);
  void sendReport();
  bool isPressed(uint8_t b = BUTTON_1); // check BUTTON_1 by default
  bool isConnected(void);
  void resetButtons();
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
