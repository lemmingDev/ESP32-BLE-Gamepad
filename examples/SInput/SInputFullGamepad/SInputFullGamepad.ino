/*
 * SInput Full Gamepad Example
 *
 * Comprehensive demo of ALL SInput features:
 *
 * Inputs:
 *   - 25 buttons:
 *       BUTTON_1-4:   Face (South/East/West/North)
 *       BUTTON_5-6:   Left/Right Shoulder
 *       BUTTON_7-8:   Left/Right Stick Click
 *       BUTTON_9-10:  Left/Right Trigger (digital)
 *       BUTTON_11-12: Left/Right Paddle 1
 *       BUTTON_13:    Capture/Share
 *       BUTTON_14-15: Left/Right Paddle 2
 *       BUTTON_16-17: Touchpad 1/2 Click
 *       BUTTON_18:    Power
 *       BUTTON_19-25: Misc 1-7
 *   - Plus Start/Back/Guide via pressStart()/pressBack()/pressHome()
 *   - Plus D-pad via setHat1()
 *   - 2 thumbsticks (left and right, circular + figure-eight motion)
 *   - 2 analog triggers (L2/R2, ramping)
 *
 * Outputs:
 *   - Rumble (haptic/vibration)
 *   - Player LED (onboard LED indicates Player 1)
 *   - RGB LED (set by host via SDL_GameControllerSetLED)
 *
 * Other:
 *   - Battery level reporting (ramp 25-90%)
 *   - Gyroscope and accelerometer (simulated)
 *   - Dual touchpad (simulated circular motion)
 *
 * Each input is demonstrated on its own timer so you can see them
 * independently in jstest, evtest, or SDL test tools.
 */

#include <BleGamepad.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Discrete RGB LED pins (optional -- remove if not wired)
#define RGB_RED_PIN   12
#define RGB_GREEN_PIN 13
#define RGB_BLUE_PIN  14

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting SInput Full Gamepad...");

  // Configure as SInput device with all output features enabled
  config.setAutoReport(false);
  config.setGamepadMode(GamepadMode::SInput);
  config.setEnableRumble(true);
  config.setEnableSInputRGB(true);
  config.setEnableSInputIMU(true);

  bleGamepad.begin(&config);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Uncomment if you have discrete RGB LED pins wired up
  // pinMode(RGB_RED_PIN, OUTPUT);
  // pinMode(RGB_GREEN_PIN, OUTPUT);
  // pinMode(RGB_BLUE_PIN, OUTPUT);
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    unsigned long now = millis();

    // Seed power state once after connection
    static bool powerStateSeeded = false;
    if (!powerStateSeeded)
    {
      bleGamepad.setBatteryPowerInformation(POWER_STATE_PRESENT);
      bleGamepad.setDischargingState(POWER_STATE_DISCHARGING);
      powerStateSeeded = true;
    }

    // --- Player LED ---
    if (bleGamepad.isPlayerLedReceived())
    {
      uint8_t idx = bleGamepad.getPlayerLedIndex();
      digitalWrite(LED_BUILTIN, idx == 1 ? HIGH : LOW);
      Serial.printf("Player LED: %d\n", idx);
    }

    // --- Rumble ---
    if (bleGamepad.isRumbleReceived())
    {
      uint8_t weak = bleGamepad.getRumbleLeftAmplitude();
      uint8_t strong = bleGamepad.getRumbleRightAmplitude();
      Serial.printf("Rumble: weak=%d strong=%d\n", weak, strong);

      // Drive rumble motors here, e.g.:
      // analogWrite(WEAK_MOTOR_PIN, weak);
      // analogWrite(STRONG_MOTOR_PIN, strong);
    }

    // --- RGB LED (set by host via SDL_GameControllerSetLED) ---
    if (bleGamepad.isRgbReceived())
    {
      uint8_t r = bleGamepad.getRgbRed();
      uint8_t g = bleGamepad.getRgbGreen();
      uint8_t b = bleGamepad.getRgbBlue();

      // Uncomment if you have discrete RGB LED pins wired up
      // analogWrite(RGB_RED_PIN, r);
      // analogWrite(RGB_GREEN_PIN, g);
      // analogWrite(RGB_BLUE_PIN, b);

      Serial.printf("RGB: %d, %d, %d\n", r, g, b);
    }

    // --- Left stick: circular motion at ~50Hz ---
    static unsigned long lastStickTime = 0;
    if (now - lastStickTime >= 20)
    {
      lastStickTime = now;
      int16_t lx = (int16_t)(sin(now / 500.0) * 32000);
      int16_t ly = (int16_t)(cos(now / 500.0) * 32000);
      bleGamepad.setLeftThumb(lx, ly);
    }

    // --- Right stick: figure-eight at ~50Hz ---
    static unsigned long lastRStickTime = 0;
    if (now - lastRStickTime >= 20)
    {
      lastRStickTime = now;
      int16_t rx = (int16_t)(sin(now / 400.0) * 32000);
      int16_t ry = (int16_t)(sin(now / 200.0) * 16000);
      bleGamepad.setRightThumb(rx, ry);
    }

    // --- Triggers: counter-rotating ramp at ~100Hz ---
    static unsigned long lastTriggerTime = 0;
    if (now - lastTriggerTime >= 10)
    {
      lastTriggerTime = now;
      int16_t t = (int16_t)((sin(now / 1000.0) * 0.5 + 0.5) * 32767);
      bleGamepad.setLeftTrigger(t);
      bleGamepad.setRightTrigger(32767 - t);
    }

    // --- D-pad: cycle through 8 directions every 500ms ---
    static unsigned long lastHatTime = 0;
    static uint8_t hatDir = 0;
    if (now - lastHatTime >= 500)
    {
      lastHatTime = now;
      bleGamepad.setHat1(hatDir);
      hatDir = (hatDir + 1) % 8;
    }

    // --- Buttons: cycle through all 25 buttons every 300ms ---
    static unsigned long lastBtnTime = 0;
    static uint8_t btnIdx = 0;
    if (now - lastBtnTime >= 300)
    {
      lastBtnTime = now;
      bleGamepad.press(btnIdx + 1);
      bleGamepad.sendReport();
      delay(50);
      bleGamepad.release(btnIdx + 1);
      btnIdx = (btnIdx + 1) % 25;
    }

    // --- Special buttons: cycle Start, Back, Home every 800ms ---
    static unsigned long lastSpecialTime = 0;
    static uint8_t specialIdx = 0;
    if (now - lastSpecialTime >= 800)
    {
      lastSpecialTime = now;
      bleGamepad.pressSpecialButton(specialIdx);
      bleGamepad.sendReport();
      delay(50);
      bleGamepad.releaseSpecialButton(specialIdx);
      specialIdx = (specialIdx + 1) % 3;
    }

    // --- Battery: ramp 25-90% every 300ms ---
    static uint8_t batteryLevel = 25;
    static int8_t batteryDir = 1;
    static unsigned long lastBatteryTime = 0;
    if (now - lastBatteryTime >= 300)
    {
      lastBatteryTime = now;
      batteryLevel += batteryDir;
      if (batteryLevel >= 90) batteryDir = -1;
      if (batteryLevel <= 25) batteryDir = 1;
      bleGamepad.setBatteryLevel(batteryLevel);
    }

    // --- IMU: simulated gyro + accelerometer at ~200Hz ---
    float t = now / 1000.0;
    int16_t gyroX = (int16_t)(sin(t * 2.0) * 2000);
    int16_t gyroY = (int16_t)(cos(t * 3.0) * 2000);
    int16_t gyroZ = (int16_t)(sin(t * 1.5) * 2000);
    int16_t accelX = (int16_t)(sin(t * 0.5) * 8000);
    int16_t accelY = (int16_t)(cos(t * 0.7) * 8000);
    int16_t accelZ = (int16_t)(sin(t * 0.3 + 1.57) * 8000);
    bleGamepad.setGyroscope(gyroX, gyroY, gyroZ);
    bleGamepad.setAccelerometer(accelX, accelY, accelZ);

    // --- Touchpad: dual pad at ~50Hz ---
    static unsigned long lastTouchTime = 0;
    if (now - lastTouchTime >= 20)
    {
      lastTouchTime = now;
      int16_t t0x = (int16_t)(sin(now / 600.0) * 16000) - 16000;
      int16_t t0y = (int16_t)(cos(now / 600.0) * 32000);
      bleGamepad.setTouchpad(0, t0x, t0y, 32767);

      int16_t t1x = (int16_t)(sin(now / 400.0) * 16000) + 16000;
      int16_t t1y = (int16_t)(cos(now / 400.0) * 32000);
      uint16_t pressure = (uint16_t)(fabs(sin(now / 300.0)) * 32767);
      bleGamepad.setTouchpad(1, t1x, t1y, pressure);
    }

    // Send all batched input data
    bleGamepad.sendReport();

    delay(5); // ~200Hz main loop
  }
}
