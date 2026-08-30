/*
 * SInput Touchpad Example
 *
 * Demonstrates dual-touchpad input over SInput.
 * Uses simulated circular touch data so it works without real touch hardware.
 *
 * SDL3 reports touchpad data as normalized coordinates with pressure.
 * This example moves two simulated touch points in circles on separate pads.
 *
 * To use a real touchpad (e.g. FT6206, CST816S), replace the simulated
 * data section with I2C reads from your touch controller.
 *
 * Hardware for real touchpad (FT6206):
 *   ESP32 GPIO 21 (SDA) --> FT6206 SDA
 *   ESP32 GPIO 22 (SCL) --> FT6206 SCL
 *   FT6206 VCC --> 3.3V
 *   FT6206 GND --> GND
 *   FT6206 INT --> ESP32 GPIO 4 (optional, for interrupt-driven reads)
 */

#include <BleGamepad.h>

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting SInput Touchpad...");

  // Configure as SInput device
  config.setAutoReport(false);
  config.setGamepadMode(GamepadMode::SInput);

  bleGamepad.begin(&config);
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    unsigned long now = millis();

    // Update touchpad data at ~50Hz (every 20ms)
    static unsigned long lastTouchTime = 0;
    if (now - lastTouchTime >= 20)
    {
      lastTouchTime = now;

      // Pad 0: circular motion in left half, full pressure
      int16_t pad0X = (int16_t)(sin(now / 600.0) * 16000) - 16000;
      int16_t pad0Y = (int16_t)(cos(now / 600.0) * 32000);
      bleGamepad.setTouchpad(0, pad0X, pad0Y, 32767);

      // Pad 1: circular motion in right half, pulsing pressure
      int16_t pad1X = (int16_t)(sin(now / 400.0) * 16000) + 16000;
      int16_t pad1Y = (int16_t)(cos(now / 400.0) * 32000);
      uint16_t pressure = (uint16_t)(fabs(sin(now / 300.0)) * 32767);
      bleGamepad.setTouchpad(1, pad1X, pad1Y, pressure);
    }

    // Toggle BUTTON_1 every 1 second as a heartbeat,
    // so the host knows the device is alive and sending input reports
    static bool buttonPressed = false;
    static unsigned long lastToggleTime = 0;

    if (millis() - lastToggleTime >= 1000)
    {
      lastToggleTime = millis();

      if (buttonPressed)
      {
        bleGamepad.release(BUTTON_1);
        buttonPressed = false;
      }
      else
      {
        bleGamepad.press(BUTTON_1);
        buttonPressed = true;
      }

      bleGamepad.sendReport();
    }

    // Print touchpad values at ~5Hz to avoid flooding Serial
    static unsigned long lastPrintTime = 0;
    if (now - lastPrintTime >= 200)
    {
      lastPrintTime = now;
      int16_t px = (int16_t)(sin(now / 600.0) * 16000) - 16000;
      int16_t py = (int16_t)(cos(now / 600.0) * 32000);
      int16_t qx = (int16_t)(sin(now / 400.0) * 16000) + 16000;
      int16_t qy = (int16_t)(cos(now / 400.0) * 32000);
      uint16_t pr = (uint16_t)(fabs(sin(now / 300.0)) * 32767);
      Serial.printf("Pad0: x=%6d y=%6d  Pad1: x=%6d y=%6d p=%5u\n",
                    px, py, qx, qy, pr);
    }

    delay(5);
  }
}
