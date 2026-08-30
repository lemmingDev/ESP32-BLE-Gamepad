/*
 * SInput Rumble Example
 *
 * Demonstrates receiving rumble (haptic/vibration) commands from the host
 * over the SInput protocol. When an SDL3 app calls SDL_GameControllerRumble(),
 * the motor data arrives on SInput Output Report 0x03 and is exposed via
 * isRumbleReceived() / getRumbleLeftAmplitude() / getRumbleRightAmplitude().
 *
 * This example prints the motor values to Serial. Wire the amplitudes to
 * actual motors via analogWrite() or a driver IC (DRV2605L, L9110S, etc.)
 * in real hardware.
 *
 * Hardware wiring example (L9110S dual H-bridge):
 *   ESP32 GPIO 12 --> L9110S A-IA (weak/left motor)
 *   ESP32 GPIO 13 --> L9110S B-IA (strong/right motor)
 *   L9110S outputs --> DC motors
 *   L9110S VCC --> 5V, GND --> GND
 *
 * Test with: SDL_GameControllerRumble(gamepad, 0xFFFF, 0xFFFF, 500)
 * or use the SDL3 test program in examples/SInput/host_test/
 */

#include <BleGamepad.h>

// Uncomment and set these if you want to drive actual motors
// #define WEAK_MOTOR_PIN   12
// #define STRONG_MOTOR_PIN 13

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting SInput Rumble...");

  // Configure as SInput device with rumble enabled
  config.setAutoReport(false);
  config.setGamepadMode(GamepadMode::SInput);
  config.setEnableRumble(true);

  bleGamepad.begin(&config);

  // Uncomment to set up motor pins
  // pinMode(WEAK_MOTOR_PIN, OUTPUT);
  // pinMode(STRONG_MOTOR_PIN, OUTPUT);
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // Poll for rumble (haptic) commands from the host
    if (bleGamepad.isRumbleReceived())
    {
      // Left amplitude = weak motor (0-255)
      uint8_t weakMotor = bleGamepad.getRumbleLeftAmplitude();

      // Right amplitude = strong motor (0-255)
      uint8_t strongMotor = bleGamepad.getRumbleRightAmplitude();

      Serial.print("Rumble: weak=");
      Serial.print(weakMotor);
      Serial.print(" strong=");
      Serial.println(strongMotor);

      // Drive rumble motors here, e.g.:
      // analogWrite(WEAK_MOTOR_PIN, weakMotor);
      // analogWrite(STRONG_MOTOR_PIN, strongMotor);
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
