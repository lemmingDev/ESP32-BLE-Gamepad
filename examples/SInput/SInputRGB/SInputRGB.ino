/*
 * SInput RGB LED Example (Discrete PWM)
 *
 * Demonstrates receiving an RGB color from the host over the SInput protocol.
 * When an SDL3 app calls SDL_GameControllerSetLED(), the color arrives on
 * SInput Output Report 0x03 and is exposed via isRgbReceived() /
 * getRgbRed() / getRgbGreen() / getRgbBlue().
 *
 * This example drives three discrete PWM-capable pins connected to an
 * RGB LED (common-anode or common-cathode). For WS2812/Neopixel strips,
 * see SInputRGB_NeoPixel instead.
 *
 * Hardware wiring (common-cathode RGB LED):
 *   ESP32 GPIO 12 --> 220 ohm --> LED Red anode
 *   ESP32 GPIO 13 --> 220 ohm --> LED Green anode
 *   ESP32 GPIO 14 --> 220 ohm --> LED Blue anode
 *   LED cathode --> GND
 *
 * For common-anode, invert the values: analogWrite(pin, 255 - color).
 */

#include <BleGamepad.h>

// Discrete RGB LED pins -- adjust to match your wiring
#define RGB_RED_PIN   12
#define RGB_GREEN_PIN 13
#define RGB_BLUE_PIN  14

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting SInput RGB LED (PWM)...");

  // Configure as SInput device with RGB enabled
  config.setAutoReport(false);
  config.setGamepadMode(GamepadMode::SInput);
  config.setEnableSInputRGB(true);

  bleGamepad.begin(&config);

  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // Poll for RGB color commands from the host
    if (bleGamepad.isRgbReceived())
    {
      uint8_t r = bleGamepad.getRgbRed();
      uint8_t g = bleGamepad.getRgbGreen();
      uint8_t b = bleGamepad.getRgbBlue();

      analogWrite(RGB_RED_PIN, r);
      analogWrite(RGB_GREEN_PIN, g);
      analogWrite(RGB_BLUE_PIN, b);

      Serial.print("RGB: ");
      Serial.print(r);
      Serial.print(", ");
      Serial.print(g);
      Serial.print(", ");
      Serial.println(b);
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
  }
}
