#include "BleNUS.h"
#include <NimBLEDevice.h>
#include "NimBLELog.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "BleNUS"
#else
#include "esp_log.h"
static const char *LOG_TAG = "BleNUS";
#endif

BleNUS::BleNUS(NimBLEServer* existingServer) 
    : pServer(existingServer), pService(nullptr), pTxCharacteristic(nullptr), pRxCharacteristic(nullptr), dataReceivedCallback(nullptr) {}

BleNUS::~BleNUS() {
    end();
}

void BleNUS::begin() {
    delay(1000);  // Give some time for other services to complete their business
    
    if (!pServer) {
        Serial.println("No pServer");
        NIMBLE_LOGD(LOG_TAG, "No existing pServer available");
        return;
    }

    NimBLEAdvertising* pAdvertising = pServer->getAdvertising();
    NIMBLE_LOGD(LOG_TAG, "Stopping main NimBLE server from advertising (shouldn't be at this stage if you set delayAdvertising to true)");
    pAdvertising->stop();
    
    NIMBLE_LOGD(LOG_TAG, "Creating Nordic UART Service");
    pService = pServer->createService(NUS_SERVICE_UUID); // This pService is local only to this class. Nothing to do with the ones from BleBamepad
    
    NIMBLE_LOGD(LOG_TAG, "Adding Nordic UART Service TX and RX characteristics");
    pTxCharacteristic = pService->createCharacteristic(NUS_TX_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::NOTIFY);
    pRxCharacteristic = pService->createCharacteristic(NUS_RX_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE);
    NIMBLE_LOGD(LOG_TAG, "Registering Nordic UART Service callbacks");
    pRxCharacteristic->setCallbacks(this);
    
    NIMBLE_LOGD(LOG_TAG, "Starting Nordic UART Service");
    pService->start();
    
    // Can't add Nordic UART Service UUID to the main advertisement as it makes it larger than 31 bytes
    // It's not strictly needed anyway
    //NIMBLE_LOGD(LOG_TAG, "Adding  Nordic UART Service UUID to main NimBLE server advertising");
    //pAdvertising->addServiceUUID(pService->getUUID());
    
    // Get around the above issue by adding Nordic UART Service UUID to NimBLEAdvertisementData scanResponseData;
    // Add it in a scan response instead so devices can still see it has that capability
    // https://github.com/h2zero/NimBLE-Arduino/issues/135
    NIMBLE_LOGD(LOG_TAG, "Adding  Nordic UART Service UUID to advertising scan response data");
    NimBLEAdvertisementData scanResponseData;             // Create NimBLEAdvertisementData object
    scanResponseData.addServiceUUID(pService->getUUID()); // Add UUID
    pAdvertising->setScanResponseData(scanResponseData);  // Assign the scan response data
    
    NIMBLE_LOGD(LOG_TAG, "Main NimBLE server advertising started!");
    pAdvertising->start();
}

void BleNUS::end() {
    if (pService) {
        // Nothing I can think of
    }
}

void BleNUS::sendData(const uint8_t* data, size_t length) {
    if (pTxCharacteristic && pServer->getConnectedCount() > 0) {
        pTxCharacteristic->setValue(data, length);
        pTxCharacteristic->notify();
    }
}

void BleNUS::setDataReceivedCallback(void (*callback)(const uint8_t* data, size_t length)) {
    dataReceivedCallback = callback;
}

void BleNUS::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
    if (dataReceivedCallback) {
        std::string value = pCharacteristic->getValue();
        buffer += value; // Append to internal buffer
        dataReceivedCallback((const uint8_t*)value.data(), value.length());
    }
}

size_t BleNUS::available() {
    return buffer.length();  // Check the length of data in the internal buffer
}

int BleNUS::read() {
    if (buffer.length() > 0) {
        char c = buffer[0];
        buffer.erase(0, 1);  // Remove the first byte after reading
        return c;
    }
    return -1;  // No data available
}

int BleNUS::peek() {
    if (buffer.length() > 0) {
        return buffer[0];  // Peek at the first byte without removing it
    }
    return -1;  // No data available to peek
}

void BleNUS::flush() {
    buffer.clear();  // Clear the internal buffer
}

void BleNUS::print(const char* str) {
    sendData((const uint8_t*)str, strlen(str));
}

void BleNUS::print(const String& str) {
    print(str.c_str());
}

void BleNUS::print(int i) {
    char buf[32];
    itoa(i, buf, 10);
    print(buf);
}

void BleNUS::print(long l) {
    char buf[32];
    ltoa(l, buf, 10);
    print(buf);
}

void BleNUS::print(unsigned long ul) {
    char buf[32];
    ultoa(ul, buf, 10);
    print(buf);
}

void BleNUS::print(float f, int digits) {
    char buf[32];
    dtostrf(f, 6, digits, buf);
    print(buf);
}

void BleNUS::print(double d, int digits) {
    print((float)d, digits);
}

void BleNUS::print(char c) {
    char buf[2] = {c, '\0'};
    print(buf);
}

void BleNUS::println(const char* str) {
    print(str);
    print("\n");
}

void BleNUS::println(const String& str) {
    println(str.c_str());
}

void BleNUS::println(int i) {
    print(i);
    print("\n");
}

void BleNUS::println(long l) {
    print(l);
    print("\n");
}

void BleNUS::println(unsigned long ul) {
    print(ul);
    print("\n");
}

void BleNUS::println(float f, int digits) {
    print(f, digits);
    print("\n");
}

void BleNUS::println(double d, int digits) {
    print(d, digits);
    print("\n");
}

void BleNUS::println(char c) {
    print(c);
    print("\n");
}

void BleNUS::write(uint8_t byte) {
    print(byte);  // Just use the print method to send 1 byte
}

void BleNUS::write(const uint8_t *buffer, size_t size) {
    sendData(buffer, size);
}
