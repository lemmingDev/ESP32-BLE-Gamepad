/*
 * XInput Gamepad Example
 *
 * Emulates an Xbox One S controller over BLE. The device advertises with
 * Microsoft's VID (0x045E) and the Xbox One S PID (0x02FD), which gives
 * broad compatibility across Windows, Linux (xpad driver), and macOS
 * (GCController framework).
 *
 * The Xbox One S PID is used by default because it works everywhere:
 *   - Windows: recognized natively as an XInput device
 *   - Linux: xpad driver, works on all kernel versions
 *   - macOS: supported since macOS Big Sur (11.0)
 *
 * If you need the Share button, use GamepadMode::XInputSeriesX instead
 * (PID 0x0B13, requires Linux 6.5+).
 *
 * Supported inputs:
 *   Buttons: A/B/X/Y/LB/RB/LS/RS/Select/Start/Home (11 total)
 *   Sticks:  Left thumb (X/Y), Right thumb (Z/Rz)
 *   Triggers: Left (Rx), Right (Ry)
 *   D-pad:   Hat switch 1
 *
 * Rumble is received via the PID Set Effect Report output characteristic.
 * The strong and weak motor values (0-255) and trigger vibration values
 * are available via isXInputRumbleReceived().
 *
 * Platform notes:
 *   Windows: Shows as "Xbox Wireless Controller" in Settings
 *   Linux:   Requires xpad kernel module (usually pre-installed)
 *   macOS:   Appears as GCController, rumble via haptics API
 */

#include <BleGamepad.h>

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting XInput Gamepad...");

  // Configure as Xbox One S (PID 0x02FD) -- broad Linux/macOS compatibility
  config.setGamepadMode(GamepadMode::XInput);

  // 11 buttons: A, B, X, Y, LB, RB, LS (thumbstick click), RS, Select, Start, Home
  config.setButtonCount(11);

  // 1 hat switch for the D-pad (8 directions + centered)
  config.setHatSwitchCount(1);

  // Axis assignment for Xbox controller mapping:
  //   X, Y   = Left thumbstick horizontal/vertical
  //   Z, Rz  = Right thumbstick horizontal/vertical
  //   Rx     = Left trigger (0-32767)
  //   Ry     = Right trigger (0-32767)
  //   Sliders = not used
  config.setWhichAxes(true, true, true, true, true, true, false, false);

  // Special buttons: Select (Back), Start, Home (Guide)
  config.setWhichSpecialButtons(true, true, false, true, false, false, false, false);

  bleGamepad.begin(&config);

  Serial.println("XInput Gamepad ready. Waiting for connection...");
}

void loop()
{
  if (bleGamepad.isConnected())
  {
    // Demo: move left stick in a circle using sin/cos
    // This produces smooth circular motion at ~2Hz (full rotation every 500ms)
    int16_t x = (int16_t)(sin(millis() / 500.0) * 32000);
    int16_t y = (int16_t)(cos(millis() / 500.0) * 32000);
    bleGamepad.setLeftThumb(x, y);
    bleGamepad.sendReport();

    // Check for rumble (vibration) from the host
    // On Windows: triggered by XInput API calls
    // On Linux: triggered by SDL or direct xpad writes
    // On macOS: triggered by GCController haptics
    if (bleGamepad.isXInputRumbleReceived())
    {
      // Strong motor: large rumble weight (0-255)
      uint8_t strong = bleGamepad.getXInputStrongMotor();

      // Weak motor: small rumble weight (0-255)
      uint8_t weak = bleGamepad.getXInputWeakMotor();

      // Trigger vibration motors (0-255)
      uint8_t leftTrigger = bleGamepad.getXInputLeftTriggerMagnitude();
      uint8_t rightTrigger = bleGamepad.getXInputRightTriggerMagnitude();

      Serial.printf("Rumble: strong=%d weak=%d L_trigger=%d R_trigger=%d\n",
                    strong, weak, leftTrigger, rightTrigger);

      // Drive your rumble motors here, e.g.:
      // analogWrite(STRONG_MOTOR_PIN, strong);
      // analogWrite(WEAK_MOTOR_PIN, weak);
      // analogWrite(L_TRIG_MOTOR_PIN, leftTrigger);
      // analogWrite(R_TRIG_MOTOR_PIN, rightTrigger);
    }

    // Print button state changes for debugging
    // This polls all 11 buttons and prints when any changes state
    static bool lastButtons[11] = {};
    const char *buttonNames[] = {
      "A", "B", "X", "Y", "LB", "RB", "LS", "RS", "Select", "Start", "Home"
    };

    for (int i = 0; i < 11; i++)
    {
      bool pressed = bleGamepad.isPressed(i + 1);
      if (pressed != lastButtons[i])
      {
        Serial.printf("Button %s: %s\n", buttonNames[i],
                      pressed ? "PRESSED" : "RELEASED");
        lastButtons[i] = pressed;
      }
    }

    delay(10); // ~100Hz update rate
  }
}
