/*
 * XInput All-Inputs Test — cycles every Xbox input so you can compare
 * 1:1 with Mystfit's XboxXInputController.ino (BleXInputDescriptors.h:16)
 */
#include <BleGamepad.h>

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting XInput All-Inputs Test...");
  config.setGamepadMode(GamepadMode::XInput);
  config.setButtonCount(11);
  config.setHatSwitchCount(1);
  config.setWhichAxes(true, true, true, true, true, true, false, false);
  config.setWhichSpecialButtons(true, true, false, true, false, false, false, false);
  bleGamepad.begin(&config);
  Serial.println("Advertising as Xbox Wireless Controller — pair in Windows/Bluetooth");
}

void loop() {
  if (!bleGamepad.isConnected()) { delay(100); return; }
  testButtons();
  testShare();
  testDPad();
  testTriggers();
  testThumbsticksWithTriggers();
}

void testButtons() {
  Serial.println("=== Buttons (11) ===");
  struct { uint8_t id; const char* name; } btns[] = {
    {1, "A (B0)"}, {2, "B (B1)"}, {3, "X (B2)"}, {4, "Y (B3)"},
    {5, "LB (B4)"}, {6, "RB (B5)"}, {7, "LS (B6)"}, {8, "RS (B7)"},
    {9, "View/Select (B8)"}, {10, "Menu/Start (B9)"}, {11, "Guide/Home (B10)"}
  };
  for (auto &b : btns) {
    Serial.printf("Press BUTTON_%d = %s\n", b.id, b.name);
    bleGamepad.press(b.id);
    bleGamepad.sendReport();
    delay(600);
    bleGamepad.release(b.id);
    bleGamepad.sendReport();
    delay(200);
  }
  // Explicit check: button 3 via its Xbox name path as well
  Serial.println("Re-check BUTTON_3 (X) explicitly");
  bleGamepad.press(3); bleGamepad.sendReport(); delay(600);
  bleGamepad.release(3); bleGamepad.sendReport(); delay(200);

  // Special-button API (same bits, alternate path at BleGamepad.cpp:1440)
  Serial.println("Press Start via pressStart()"); bleGamepad.pressStart(); bleGamepad.sendReport(); delay(600); bleGamepad.releaseStart(); bleGamepad.sendReport(); delay(200);
  Serial.println("Press Select via pressSelect()"); bleGamepad.pressSelect(); bleGamepad.sendReport(); delay(600); bleGamepad.releaseSelect(); bleGamepad.sendReport(); delay(200);
  Serial.println("Press Home via pressHome()"); bleGamepad.pressHome(); bleGamepad.sendReport(); delay(600); bleGamepad.releaseHome(); bleGamepad.sendReport(); delay(200);
}

void testShare() {
  Serial.println("=== Share (Back) ===");
  bleGamepad.pressBack(); bleGamepad.sendReport(); delay(600);
  bleGamepad.releaseBack(); bleGamepad.sendReport(); delay(200);
}

void testDPad() {
  Serial.println("=== DPad ===");
  signed char dirs[] = { HAT_UP, HAT_UP_RIGHT, HAT_RIGHT, HAT_DOWN_RIGHT, HAT_DOWN, HAT_DOWN_LEFT, HAT_LEFT, HAT_UP_LEFT };
  const char* names[] = {"N","NE","E","SE","S","SW","W","NW"};
  for (int i = 0; i < 8; i++) {
    Serial.printf("DPad %s\n", names[i]);
    bleGamepad.setHat(dirs[i]); bleGamepad.sendReport(); delay(600);
    bleGamepad.setHat(HAT_CENTERED); bleGamepad.sendReport(); delay(200);
  }
}

void testTriggers() {
  Serial.println("=== Triggers (slow sweep) ===");
  for (int v = 0; v <= 32767; v += 1024) { bleGamepad.setTriggers(v, 32767 - v); bleGamepad.sendReport(); delay(30); }
  for (int v = 32767; v >= 0; v -= 1024) { bleGamepad.setTriggers(v, 32767 - v); bleGamepad.sendReport(); delay(30); }
  bleGamepad.setTriggers(0,0); bleGamepad.sendReport();
}

void testThumbsticksWithTriggers() {
  Serial.println("=== Thumbsticks + triggers moving together (8s) ===");
  int start = millis();
  while (millis() - start < 8000) {
    float t = millis() / 1000.0f;
    int16_t x = cos(t) * 32000;
    int16_t y = sin(t) * 32000;
    bleGamepad.setLeftThumb(x, y);
    bleGamepad.setRightThumb(-x, -y);
    // Move triggers slowly in opposite phase while sticks circle
    int16_t trig = (int16_t)((sin(t * 0.5f) + 1.0f) * 16383.5f); // 0..32767 slow
    bleGamepad.setTriggers(trig, 32767 - trig);
    bleGamepad.sendReport();
    delay(10);
  }
  bleGamepad.setLeftThumb(0,0); bleGamepad.setRightThumb(0,0); bleGamepad.setTriggers(0,0); bleGamepad.sendReport();
  if (bleGamepad.isXInputRumbleReceived()) {
    Serial.printf("Rumble: strong=%d weak=%d L=%d R=%d\n",
      bleGamepad.getXInputStrongMotor(), bleGamepad.getXInputWeakMotor(),
      bleGamepad.getXInputLeftTriggerMagnitude(), bleGamepad.getXInputRightTriggerMagnitude());
  }
}
