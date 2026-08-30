/*
 * XInput Series X Example
 *
 * Emulates an Xbox Series X controller over BLE. Differs from the
 * XInputGamepad example by using PID 0x0B13 instead of 0x02FD, which
 * enables the Share button (Button 11).
 *
 * PID differences:
 *   0x02FD (Xbox One S) -- broad compatibility, no Share button
 *   0x0B13 (Xbox Series X) -- Share button, requires Linux 6.5+
 *
 * Platform notes:
 *   Windows: Recognized natively, Share button may need XInput support
 *   Linux:   Requires kernel 6.5+ for Share button; older kernels
 *            still work for basic gamepad via xpad driver
 *   macOS:   Supported since Big Sur, Share button not mapped by GCController
 *
 * Supported inputs:
 *   Buttons: A/B/X/Y/LB/RB/LS/RS/Select/Start/Share (11 total)
 *   Sticks:  Left thumb (X/Y), Right thumb (Z/Rz)
 *   Triggers: Left (Rx), Right (Ry)
 *   D-pad:   Hat switch 1
 *
 * Rumble: Strong/weak motors + trigger vibration (0-255)
 */

#include <BleGamepad.h>

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting XInput Series X Gamepad...");

  // Configure as Xbox Series X (PID 0x0B13)
  config.setGamepadMode(GamepadMode::XInputSeriesX);

  // 11 buttons: A, B, X, Y, LB, RB, LS, RS, Select, Start, Share
  config.setButtonCount(11);

  // 1 hat switch for the D-pad
  config.setHatSwitchCount(1);

  // Same axis mapping as Xbox One S:
  //   X/Y = left stick, Z/Rz = right stick, Rx = left trigger, Ry = right trigger
  config.setWhichAxes(true, true, true, true, true, true, false, false);

  // Special buttons: Select (Back), Start, Home (Guide)
  config.setWhichSpecialButtons(true, true, false, true, false, false, false, false);

  bleGamepad.begin(&config);

  Serial.println("XInput Series X ready. Waiting for connection...");
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    unsigned long now = millis();

    // Left stick: smooth circular motion at ~2Hz
    static unsigned long lastLStickTime = 0;
    if (now - lastLStickTime >= 20)
    {
      lastLStickTime = now;
      int16_t x = (int16_t)(sin(now / 500.0) * 32000);
      int16_t y = (int16_t)(cos(now / 500.0) * 32000);
      bleGamepad.setLeftThumb(x, y);
    }

    // Right stick: figure-eight pattern (Lissajous curve)
    // Frequency ratio 2:1 produces the figure-eight shape
    static unsigned long lastRStickTime = 0;
    if (now - lastRStickTime >= 20)
    {
      lastRStickTime = now;
      int16_t x = (int16_t)(sin(now / 400.0) * 32000);
      int16_t y = (int16_t)(sin(now / 200.0) * 16000);
      bleGamepad.setRightThumb(x, y);
    }

    // Triggers: counter-rotating ramp
    // Left goes up while right goes down, and vice versa
    static unsigned long lastTriggerTime = 0;
    if (now - lastTriggerTime >= 10)
    {
      lastTriggerTime = now;
      int16_t t = (int16_t)((sin(now / 1000.0) * 0.5 + 0.5) * 32767);
      bleGamepad.setLeftTrigger(t);
      bleGamepad.setRightTrigger(32767 - t);
    }

    // D-pad: cycle through all 8 directions every 500ms
    // 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW
    static unsigned long lastHatTime = 0;
    static uint8_t hatDir = 0;
    if (now - lastHatTime >= 500)
    {
      lastHatTime = now;
      bleGamepad.setHat1(hatDir);
      hatDir = (hatDir + 1) % 8;
    }

    // Buttons: cycle through all 11 buttons every 300ms
    // Each button is pressed for 50ms then released, showing the next one
    // Button 11 is the Share button (unique to Series X PID)
    static unsigned long lastBtnTime = 0;
    static uint8_t btnIdx = 0;
    if (now - lastBtnTime >= 300)
    {
      lastBtnTime = now;
      bleGamepad.press(btnIdx + 1);
      bleGamepad.sendReport();
      delay(50);
      bleGamepad.release(btnIdx + 1);
      btnIdx = (btnIdx + 1) % 11;
    }

    // Check for rumble (vibration) from the host
    if (bleGamepad.isXInputRumbleReceived())
    {
      uint8_t strong = bleGamepad.getXInputStrongMotor();
      uint8_t weak = bleGamepad.getXInputWeakMotor();
      uint8_t leftTrigger = bleGamepad.getXInputLeftTriggerMagnitude();
      uint8_t rightTrigger = bleGamepad.getXInputRightTriggerMagnitude();

      Serial.printf("Rumble: strong=%d weak=%d L_trig=%d R_trig=%d\n",
                    strong, weak, leftTrigger, rightTrigger);

      // Drive your rumble motors here, e.g.:
      // analogWrite(STRONG_MOTOR_PIN, strong);
      // analogWrite(WEAK_MOTOR_PIN, weak);
      // analogWrite(L_TRIG_MOTOR_PIN, leftTrigger);
      // analogWrite(R_TRIG_MOTOR_PIN, rightTrigger);
    }

    delay(5); // ~200Hz main loop
  }
}
