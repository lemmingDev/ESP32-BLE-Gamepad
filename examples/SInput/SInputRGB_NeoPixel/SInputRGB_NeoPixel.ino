/*
 * SInput RGB LED Example (WS2812 / NeoPixel)
 *
 * Demonstrates receiving an RGB color from the host over the SInput protocol.
 * When an SDL3 app calls SDL_GameControllerSetLED(), the color arrives on
 * SInput Output Report 0x03 and is exposed via isRgbReceived() /
 * getRgbRed() / getRgbGreen() / getRgbBlue().
 *
 * This example drives a WS2812 (NeoPixel) addressable RGB LED strip.
 * Requires the Adafruit_NeoPixel library (install via Arduino Library Manager).
 *
 * For discrete R/G/B PWM pins, see SInputRGB instead.
 *
 * Hardware wiring:
 *   ESP32 GPIO 12 --> NeoPixel DIN
 *   NeoPixel VCC  --> 5V (use external supply for >1 pixel)
 *   NeoPixel GND  --> GND
 */

#include <BleGamepad.h>
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_PIN   12
#define NEOPIXEL_COUNT 1

BleGamepad bleGamepad;
BleGamepadConfiguration config;
Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting SInput RGB LED (NeoPixel)...");

  strip.begin();
  strip.setBrightness(50);
  strip.show(); // Initialize all pixels to off

  // Configure as SInput device with RGB enabled
  config.setAutoReport(false);
  config.setGamepadMode(GamepadMode::SInput);
  config.setEnableSInputRGB(true);

  bleGamepad.begin(&config);
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

      strip.setPixelColor(0, strip.Color(r, g, b));
      strip.show();

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
