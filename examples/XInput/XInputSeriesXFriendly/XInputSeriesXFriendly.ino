/*
 * XInput Series X Friendly Names Example
 *
 * Same Xbox Series X controller as XInputSeriesX, but every input is driven
 * through a friendly name instead of a raw number — use this as the cheat
 * sheet for the XInput API.
 *
 * BUTTONS (press/release/isPressed take a button NUMBER 1..11):
 *   Name          Number  Xbox button   Report bit
 *   XBTN_A        1       A             0x0001
 *   XBTN_B        2       B             0x0002
 *   XBTN_X        3       X             0x0008
 *   XBTN_Y        4       Y             0x0010
 *   XBTN_LB       5       Left bumper   0x0040
 *   XBTN_RB       6       Right bumper  0x0080
 *   XBTN_LS       7       L-stick click 0x2000
 *   XBTN_RS       8       R-stick click 0x4000
 *   XBTN_SELECT   9       View/Select   0x0400
 *   XBTN_START    10      Menu/Start    0x0800
 *   XBTN_GUIDE    11      Xbox/Guide    0x1000
 *   Share is separate: pressSpecialButton(BACK_BUTTON) sets the Share byte.
 *   NOTE: for XInput the START/SELECT/HOME *specials* do nothing — Start,
 *   Select and Guide live in the buttons field above, so always use the
 *   XBTN_* names for them. Only the BACK special (Share) is read.
 *
 *   GUIDE WARNING: pressing Guide (XBTN_GUIDE) opens the Steam overlay on
 *   PCs running Steam, which steals focus and interrupts testing. The loop
 *   below therefore SKIPS Guide by default. To test it, add XBTN_GUIDE to
 *   the buttons[] array (or call bleGamepad.press(XBTN_GUIDE) anywhere) —
 *   the mapping itself works fine.
 *
 * STICKS:   setLeftThumb(x, y) / setRightThumb(x, y), each -32767..32767.
 * TRIGGERS: setLeftTrigger(v) / setRightTrigger(v) / setTriggers(l, r),
 *           each 0..32767 (scaled to the 0..1023 wire range for you).
 * D-PAD:    setHat1(HAT_UP / HAT_UP_RIGHT / HAT_RIGHT / ... / HAT_CENTERED).
 * RUMBLE:   isXInputRumbleReceived() + getXInputStrongMotor() /
 *           getXInputWeakMotor() / getXInputLeftTriggerMagnitude() /
 *           getXInputRightTriggerMagnitude().
 *
 * For One S, swap the setGamepadMode line to GamepadMode::XInputOneS (same
 * names work; Share drives the 1708 AC Back slot instead of Record).
 */

#include <BleGamepad.h>

// Friendly names for the Series X buttons (button numbers for
// press()/release()/isPressed()).
#define XBTN_A      BUTTON_1   // A
#define XBTN_B      BUTTON_2   // B
#define XBTN_X      BUTTON_3   // X
#define XBTN_Y      BUTTON_4   // Y
#define XBTN_LB     BUTTON_5   // Left bumper
#define XBTN_RB     BUTTON_6   // Right bumper
#define XBTN_LS     BUTTON_7   // Left stick click
#define XBTN_RS     BUTTON_8   // Right stick click
#define XBTN_SELECT BUTTON_9   // View (Select)
#define XBTN_START  BUTTON_10  // Menu (Start)
#define XBTN_GUIDE  BUTTON_11  // Xbox (Guide)
// Share has no button number: pressSpecialButton(BACK_BUTTON).

BleGamepad bleGamepad;
BleGamepadConfiguration config;

void setup()
{
  Serial.begin(115200);

  // Xbox Series X preset: 11 buttons, D-pad, sticks, triggers, Share.
  config.setGamepadMode(GamepadMode::XInputSeriesX);

  bleGamepad.begin(&config);
}

void loop()
{
  if (!bleGamepad.isConnected())
  {
    delay(500);
    return;
  }

  // Buttons, one tap each. Guide (XBTN_GUIDE) is deliberately left out:
  // on a PC running Steam it opens the overlay and steals focus. To test
  // Guide, just append XBTN_GUIDE to this array.
  const uint8_t buttons[] = {XBTN_A, XBTN_B, XBTN_X, XBTN_Y, XBTN_LB, XBTN_RB,
                             XBTN_LS, XBTN_RS, XBTN_SELECT, XBTN_START};
  for (uint8_t i = 0; i < 10; i++)
  {
    bleGamepad.press(buttons[i]);
    bleGamepad.sendReport();
    delay(250);
    bleGamepad.release(buttons[i]);
    bleGamepad.sendReport();
    delay(150);
  }

  // Share (BACK special).
  bleGamepad.pressSpecialButton(BACK_BUTTON);
  bleGamepad.sendReport();
  delay(250);
  bleGamepad.releaseSpecialButton(BACK_BUTTON);
  bleGamepad.sendReport();
  delay(150);

  // Sticks: full deflection each way, then center.
  bleGamepad.setLeftThumb(0, 32767);
  delay(300);
  bleGamepad.setLeftThumb(32767, 0);
  delay(300);
  bleGamepad.setLeftThumb(0, -32767);
  delay(300);
  bleGamepad.setLeftThumb(-32767, 0);
  delay(300);
  bleGamepad.setLeftThumb(0, 0);
  bleGamepad.setRightThumb(0, 32767);
  delay(300);
  bleGamepad.setRightThumb(32767, 0);
  delay(300);
  bleGamepad.setRightThumb(0, -32767);
  delay(300);
  bleGamepad.setRightThumb(-32767, 0);
  delay(300);
  bleGamepad.setRightThumb(0, 0);

  // Triggers: left full, right full, both half, released.
  bleGamepad.setTriggers(32767, 0);
  delay(300);
  bleGamepad.setTriggers(0, 32767);
  delay(300);
  bleGamepad.setTriggers(16383, 16383);
  delay(300);
  bleGamepad.setTriggers(0, 0);

  // D-pad: all 8 directions, then centered.
  const uint8_t hats[] = {HAT_UP, HAT_UP_RIGHT, HAT_RIGHT, HAT_DOWN_RIGHT,
                          HAT_DOWN, HAT_DOWN_LEFT, HAT_LEFT, HAT_UP_LEFT};
  for (uint8_t i = 0; i < 8; i++)
  {
    bleGamepad.setHat1(hats[i]);
    delay(300);
  }
  bleGamepad.setHat1(HAT_CENTERED);

  // Rumble from the host, if any arrived during the script.
  if (bleGamepad.isXInputRumbleReceived())
  {
    Serial.printf("Rumble: strong=%d weak=%d L_trig=%d R_trig=%d\n",
                  bleGamepad.getXInputStrongMotor(),
                  bleGamepad.getXInputWeakMotor(),
                  bleGamepad.getXInputLeftTriggerMagnitude(),
                  bleGamepad.getXInputRightTriggerMagnitude());
  }

  delay(2000);
}
