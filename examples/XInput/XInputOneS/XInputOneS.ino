/*
 * XInput One S Robust Example — Xbox One S (1708) 02FD:0408
 *
 * Robust + feature-rich: cycles every input, handles rumble, battery,
 * hat, triggers, both sticks, and recovers from disconnect. Windows users
 * should prefer XInputSeriesX (0B13, Share) for native XInput over BLE on
 * Win11 22H2+ WGI — One S 02FD is Generic HID on Win11 (DInput joy.cpl OK)
 * but XInput on linux<6.5 xpad broad (like Mystfit). See docs/XInputMode.md.
 *
 * Features:
 *  - 11 buttons A/B/X/Y/LB/RB/LS/RS/Select/Start/Home + hat 1 + 6 axes
 *  - Left/right thumb + left/right trigger (Brake/Accelerator 0..1023)
 *  - D-pad 8-way + centered, Share via Select/Back
 *  - Rumble PID 0x03 (strong/weak + trigger motors), Battery 0x04 Input
 *  - Robust: non-blocking 100 Hz sendReport, disconnect recovery, serial cmds
 *  - Serial: 'b' buttons, 'h' hat, 't' triggers, 's' sticks, 'a' all, 'r' rumble test
 */

#include <BleGamepad.h>

BleGamepad bleGamepad;
BleGamepadConfiguration config;

// Robust timing
static uint32_t lastReportMs = 0;
static const uint32_t REPORT_INTERVAL_MS = 10; // 100 Hz
static uint32_t lastBatteryMs = 0;
static uint8_t batteryLevel = 100;
static bool demoAll = true;
static uint32_t demoPhaseStart = 0;
static uint8_t demoPhase = 0;

void handleRumble() {
  if (bleGamepad.isXInputRumbleReceived()) {
    uint8_t strong = bleGamepad.getXInputStrongMotor();
    uint8_t weak = bleGamepad.getXInputWeakMotor();
    uint8_t lTrig = bleGamepad.getXInputLeftTriggerMagnitude();
    uint8_t rTrig = bleGamepad.getXInputRightTriggerMagnitude();
    Serial.printf("Rumble: strong=%d weak=%d L=%d R=%d\n", strong, weak, lTrig, rTrig);
    // Drive motors here: analogWrite(PIN_STRONG, strong) etc.
  }
}

void handleSerial() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c == 'a' || c == 'A') { demoAll = true; demoPhase = 0; demoPhaseStart = millis(); Serial.println("Demo: all"); }
  else if (c == 'b') { demoAll = false; Serial.println("Press A..Home sequence"); for (int i=1;i<=11;i++){bleGamepad.press(i); bleGamepad.sendReport(); delay(120); bleGamepad.release(i); bleGamepad.sendReport(); delay(80);} }
  else if (c == 'h') { demoAll = false; signed char dirs[]={HAT_UP,HAT_UP_RIGHT,HAT_RIGHT,HAT_DOWN_RIGHT,HAT_DOWN,HAT_DOWN_LEFT,HAT_LEFT,HAT_UP_LEFT}; for(auto d:dirs){bleGamepad.setHat(d); bleGamepad.sendReport(); delay(300); } bleGamepad.setHat(HAT_CENTERED); bleGamepad.sendReport();}
  else if (c == 't') { demoAll=false; for(int v=0;v<=32767;v+=2048){bleGamepad.setTriggers(v,32767-v); bleGamepad.sendReport(); delay(15);} bleGamepad.setTriggers(0,0); bleGamepad.sendReport();}
  else if (c == 's') { demoAll=false; for(int i=0;i<200;i++){int16_t x=cos(i*0.05)*32000, y=sin(i*0.05)*32000; bleGamepad.setLeftThumb(x,y); bleGamepad.setRightThumb(-x,-y); bleGamepad.sendReport(); delay(10);} bleGamepad.setLeftThumb(0,0); bleGamepad.setRightThumb(0,0); bleGamepad.sendReport();}
  else if (c == 'r') { Serial.println("Rumble test: host should send PID 0x03"); }
  else if (c == '?') { Serial.println("Commands: a=all b=buttons h=hat t=triggers s=sticks r=rumble ? =help"); }
}

void demoLoop() {
  if (!demoAll) return;
  uint32_t now = millis();
  // Phase 0: buttons 6 s, 1: hat 4 s, 2: triggers 4 s, 3: sticks+triggers 8 s
  uint32_t elapsed = now - demoPhaseStart;
  if (demoPhase==0) {
    static uint32_t lastBtn=0; static uint8_t btn=1;
    if (now-lastBtn>700){ bleGamepad.release(btn); btn = (btn%11)+1; bleGamepad.press(btn); Serial.printf("Demo btn %d\n",btn); lastBtn=now; if(btn==11) {demoPhase=1; demoPhaseStart=now; bleGamepad.release(btn);} }
  } else if (demoPhase==1) {
    static uint32_t lastHat=0; static uint8_t hi=0; signed char dirs[]={HAT_UP,HAT_UP_RIGHT,HAT_RIGHT,HAT_DOWN_RIGHT,HAT_DOWN,HAT_DOWN_LEFT,HAT_LEFT,HAT_UP_LEFT, HAT_CENTERED};
    if (now-lastHat>400){ bleGamepad.setHat(dirs[hi]); hi=(hi+1)%9; lastHat=now; if(elapsed>4000){demoPhase=2; demoPhaseStart=now;}}
  } else if (demoPhase==2) {
    int16_t v = (int16_t)((sin(now/600.0)+1)*16383.5); bleGamepad.setTriggers(v, 32767-v);
    if (elapsed>4000){demoPhase=3; demoPhaseStart=now; bleGamepad.setTriggers(0,0);}
  } else {
    float t=now/900.0; int16_t x=cos(t)*32000, y=sin(t)*32000; bleGamepad.setLeftThumb(x,y); bleGamepad.setRightThumb(-x,-y);
    int16_t trig=(sin(t*0.4)+1)*16383.5; bleGamepad.setTriggers(trig,32767-trig);
    if (elapsed>8000){demoPhase=0; demoPhaseStart=now; bleGamepad.setLeftThumb(0,0); bleGamepad.setRightThumb(0,0); bleGamepad.setTriggers(0,0);}
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Starting XInput One S Robust (02FD:0408 1708 AC Back)...");
  Serial.println("  Win11: Generic HID DInput joy.cpl OK, not XInputGetState — use SeriesX 0B13 for Win11 XInput. See docs/XInputMode.md");
  Serial.println("  linux<6.5 xpad 02FD XInput, 6.5+ 0B13 Share. Commands: a b h t s r ?");

  config.setGamepadMode(GamepadMode::XInputOneS);
  config.setButtonCount(11);
  config.setHatSwitchCount(1);
  config.setWhichAxes(true, true, true, true, true, true, false, false);
  config.setWhichSpecialButtons(true, true, false, true, false, false, false, false);
  config.setAutoReport(false); // robust manual sendReport at 100 Hz
  bleGamepad.begin(&config);
  Serial.println("XInput One S ready. Advertising as Xbox Wireless Controller — pair in Bluetooth. Waiting for connection...");
  batteryLevel = 100;
  bleGamepad.setBatteryLevel(batteryLevel);
}

void loop() {
  handleSerial();
  if (!bleGamepad.isConnected()) {
    // Robust disconnect recovery: reset state, throttle battery updates
    static uint32_t lastDiscLog=0;
    if (millis()-lastDiscLog>2000){ Serial.println("Waiting for connection..."); lastDiscLog=millis(); }
    bleGamepad.setLeftThumb(0,0); bleGamepad.setRightThumb(0,0); bleGamepad.setTriggers(0,0); bleGamepad.setHat(HAT_CENTERED);
    delay(100);
    return;
  }

  uint32_t now = millis();
  if (now - lastReportMs < REPORT_INTERVAL_MS) { handleRumble(); return; }
  lastReportMs = now;

  demoLoop();
  // Battery 0x04 Input (1 byte) via BLE Battery Service + HID 0x04 — update every 30 s for robustness
  if (now - lastBatteryMs > 30000) {
    batteryLevel = (batteryLevel > 5) ? batteryLevel - 1 : 100;
    bleGamepad.setBatteryLevel(batteryLevel);
    // HID Battery 0x04 Input characteristic is created for OneS 1708 at BleGamepad.cpp:2563 and set to 0 on connect; Battery Service covers OS UI
    lastBatteryMs = now;
    Serial.printf("Battery %d%%\n", batteryLevel);
  }

  bleGamepad.sendReport();
  handleRumble();
}
