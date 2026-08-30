/*
 * SInput Player LED Example
 *
 * Demonstrates receiving a player LED index from the host over the SInput
 * protocol. When an SDL3 app assigns a player slot via
 * SDL_SetGamepadPlayerIndex(), the index arrives via isPlayerLedReceived().
 *
 * SInput player LED indices are 1-based:
 *   0 = no player assigned
 *   1 = Player 1
 *   2 = Player 2
 *   etc.
 *
 * This example uses the onboard LED (LED_BUILTIN) to indicate Player 1.
 * On a real device with 4 LEDs, you'd light LED (index - 1) for the
 * assigned player slot.
 *
 * Hardware:
 *   Most ESP32 dev boards have an onboard LED on LED_BUILTIN (usually GPIO 2).
 */

#include <BleGamepad.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // Fallback if the board package doesn't define one
#endif

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting SInput Player LED...");

  // Configure as SInput device
  config.setAutoReport(false);
  config.setGamepadMode(GamepadMode::SInput);

  bleGamepad.begin(&config);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // Poll for player LED commands from the host
    if (bleGamepad.isPlayerLedReceived())
    {
      uint8_t playerLedIndex = bleGamepad.getPlayerLedIndex();

      // Light the onboard LED only for Player 1
      if (playerLedIndex == 1)
      {
        digitalWrite(LED_BUILTIN, HIGH);
      }
      else
      {
        digitalWrite(LED_BUILTIN, LOW);
      }

      Serial.print("Player LED index: ");
      Serial.println(playerLedIndex);
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
