#ifndef ESP32_BLE_GAMEPAD_H
#define ESP32_BLE_GAMEPAD_H
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "nimconfig.h"
#if defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)

#include "BleConnectionStatus.h"
#include "NimBLEHIDDevice.h"
#include "NimBLECharacteristic.h"
#include "BleGamepadConfiguration.h"
#include "BleOutputReceiver.h"
#include "BleNUS.h"

class BleGamepad
{
  private:
    uint8_t _buttons[16]; // 8 bits x 16 bytes = 128 bits --> 128 button max
    uint8_t _specialButtons;
    
    int16_t _x, _y, _z, _rX, _rY, _rZ, _slider1, _slider2;
    int16_t _rudder, _throttle, _accelerator, _brake, _steering;
    int16_t _hat1, _hat2, _hat3, _hat4;
    int16_t _gX, _gY, _gZ, _aX, _aY, _aZ;
    
    uint8_t _batteryPowerInformation, _dischargingState, _chargingState, _powerLevel;
    
    NimBLEHIDDevice *hid;
    NimBLECharacteristic *pCharacteristic_Power_State;
    
    NimBLEServer *pServer;
    BleNUS *nus;
    
    std::string deviceManufacturer;
    std::string deviceName;
    
    uint8_t tempHidReportDescriptor[150];
    int hidReportDescriptorSize;
    uint8_t hidReportSize;
    uint8_t numOfButtonBytes;
    bool enableOutputReport;
    uint16_t outputReportLength;
    bool nusInitialized;
    
    BleConnectionStatus *connectionStatus;
    BleOutputReceiver *outputReceiver;
    
    NimBLECharacteristic *inputGamepad;
    NimBLECharacteristic *outputGamepad;
    uint8_t *outputBackupBuffer;

    void rawAction(uint8_t msg[], char msgSize);
    static void taskServer(void *pvParameter);
    uint8_t specialButtonBitPosition(uint8_t specialButton);

  public:
    BleGamepadConfiguration configuration;

    BleGamepad(std::string deviceName = "ESP32 BLE Gamepad", std::string deviceManufacturer = "Espressif", uint8_t batteryLevel = 100, bool delayAdvertising = false);
    
    void begin(BleGamepadConfiguration *config = new BleGamepadConfiguration());
    void end(void);
    
    void setAxes(int16_t x = 0, int16_t y = 0, int16_t z = 0, int16_t rX = 0, int16_t rY = 0, int16_t rZ = 0, int16_t slider1 = 0, int16_t slider2 = 0);
    void setHIDAxes(int16_t x = 0, int16_t y = 0, int16_t z = 0, int16_t rZ = 0, int16_t rX = 0, int16_t rY = 0, int16_t slider1 = 0, int16_t slider2 = 0);
    
    void press(uint8_t b = BUTTON_1);
    void release(uint8_t b = BUTTON_1);
    
    void pressSpecialButton(uint8_t b);
    void releaseSpecialButton(uint8_t b);
    
    void pressStart();
    void releaseStart();
    void pressSelect();
    void releaseSelect();
    void pressMenu();
    void releaseMenu();
    void pressHome();
    void releaseHome();
    void pressBack();
    void releaseBack();
    void pressVolumeInc();
    void releaseVolumeInc();
    void pressVolumeDec();
    void releaseVolumeDec();
    void pressVolumeMute();
    void releaseVolumeMute();
    
    void setLeftThumb(int16_t x = 0, int16_t y = 0);
    void setRightThumb(int16_t z = 0, int16_t rZ = 0);
    void setRightThumbAndroid(int16_t z = 0, int16_t rX = 0);
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
    
    void sendReport();
    bool isPressed(uint8_t b = BUTTON_1);
    bool isConnected(void);
    void resetButtons();
    
    void setBatteryLevel(uint8_t level);
    void setPowerStateAll(uint8_t batteryPowerInformation, uint8_t dischargingState, uint8_t chargingState, uint8_t powerLevel);
    void setBatteryPowerInformation(uint8_t batteryPowerInformation);
    void setDischargingState(uint8_t dischargingState);
    void setChargingState(uint8_t chargingState);
    void setPowerLevel(uint8_t powerLevel);
    
    void setTXPowerLevel(int8_t level = 9);
    int8_t getTXPowerLevel();
    
    bool isOutputReceived();
    uint8_t* getOutputBuffer();
    
    bool deleteBond(bool resetBoard = false);
    bool deleteAllBonds(bool resetBoard = false);
    bool enterPairingMode();
    
    NimBLEAddress getAddress();
    String getStringAddress();
    NimBLEConnInfo getPeerInfo();
    String getDeviceName();
    String getDeviceManufacturer();
    
    void setGyroscope(int16_t gX = 0, int16_t gY = 0, int16_t gZ = 0);
    void setAccelerometer(int16_t aX = 0, int16_t aY = 0, int16_t aZ = 0);
    void setMotionControls(int16_t gX = 0, int16_t gY = 0, int16_t gZ = 0, int16_t aX = 0, int16_t aY = 0, int16_t aZ = 0);
    
    void beginNUS();
    void sendDataOverNUS(const uint8_t* data, size_t length);
    void setNUSDataReceivedCallback(void (*callback)(const uint8_t* data, size_t length));
    BleNUS* getNUS();

  protected:
    virtual void onStarted(NimBLEServer *pServer) {};
};

#endif // CONFIG_BT_NIMBLE_ROLE_PERIPHERAL
#endif // CONFIG_BT_ENABLED
#endif // ESP32_BLE_GAMEPAD_H
