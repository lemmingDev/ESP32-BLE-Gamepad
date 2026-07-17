/*
 * A minimal single-button gamepad with the Nordic UART Service (NUS) also enabled.
 * Intended as a quick diagnostics tool rather than a real controller: connect any
 * BLE UART terminal app (e.g. "Serial Bluetooth Terminal", nRF Connect) to the same
 * device to see a live status line and to test that the NUS RX/TX path works -
 * anything you send is echoed straight back.
 */

#include <Arduino.h>
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

#define BUTTONPIN 35            // Pin button is attached to
#define STATUS_INTERVAL_MS 1000 // How often to send a status line over NUS

BleGamepad bleGamepad;

int previousButtonState = HIGH;
unsigned long lastStatusTime = 0;

void setup()
{
    Serial.begin(115200);
    pinMode(BUTTONPIN, INPUT_PULLUP);

    bleGamepad.begin();
    bleGamepad.beginNUS(); // Adds the Nordic UART Service alongside the gamepad HID service

    Serial.println("[Diagnostics] Ready - waiting for a BLE connection.");
}

void loop()
{
    if (!bleGamepad.isConnected())
    {
        previousButtonState = HIGH;
        return;
    }

    // --- Button handling (same pattern as the SingleButton example) ---
    int currentButtonState = digitalRead(BUTTONPIN);
    if (currentButtonState != previousButtonState)
    {
        if (currentButtonState == LOW)
        {
            bleGamepad.press(BUTTON_1);
        }
        else
        {
            bleGamepad.release(BUTTON_1);
        }
    }
    previousButtonState = currentButtonState;

    BleNUS *nus = bleGamepad.getNUS();
    if (!nus)
    {
        return;
    }

    // --- Echo anything received back over NUS, to confirm the RX path works ---
    if (nus->available())
    {
        String received;
        while (nus->available())
        {
            received += (char)nus->read();
        }
        nus->println("Echo: " + received);
    }

    // --- Periodic status line over NUS, to confirm the TX path works ---
    if (millis() - lastStatusTime >= STATUS_INTERVAL_MS)
    {
        lastStatusTime = millis();

        String status = "uptime_ms=" + String(millis()) +
                         " button1=" + (currentButtonState == LOW ? "PRESSED" : "released") +
                         " free_heap=" + String(ESP.getFreeHeap());
        nus->println(status);
        Serial.println(status);
    }
}
