#ifndef BleNUS_h
#define BleNUS_h

#include <NimBLEDevice.h>

#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_CHARACTERISTIC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

class BleNUS : public NimBLECharacteristicCallbacks {
public:
    BleNUS(NimBLEServer* existingServer);
    ~BleNUS();

    void begin();
    void end();
    
    void sendData(const uint8_t* data, size_t length);
    
    void setDataReceivedCallback(void (*callback)(const uint8_t* data, size_t length));
    
    size_t available();
    int read();
    void flush();
    int peek();  // Add peek method

    void print(const char* str);
    void print(const String& str);
    void print(int i);
    void print(long l);
    void print(unsigned long ul);
    void print(float f, int digits = 2);
    void print(double d, int digits = 2);
    void print(char c);

    void println(const char* str);
    void println(const String& str);
    void println(int i);
    void println(long l);
    void println(unsigned long ul);
    void println(float f, int digits = 2);
    void println(double d, int digits = 2);
    void println(char c);
    
    void write(uint8_t byte);
    void write(const uint8_t *buffer, size_t size);
    
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;

private:
    NimBLEServer* pServer;
    NimBLEService* pService;
    NimBLECharacteristic* pTxCharacteristic;
    NimBLECharacteristic* pRxCharacteristic;
    
    void (*dataReceivedCallback)(const uint8_t* data, size_t length);
    std::string buffer; // For storing received data
};

#endif
