#include <Arduino.h>
#include <BleGamepad.h>

BleGamepad bleGamepad;

void setup()
{
    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setWhichSpecialButtons(true, true, true, true, true, true, true, true);
    bleGamepad.begin(bleGamepadConfig);

    // changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
    Serial.println("Pressing start and select");
    bleGamepad.pressStart();
    delay(100);
    bleGamepad.releaseStart();
    delay(100);
    bleGamepad.pressSelect();
    delay(100);
    bleGamepad.releaseSelect();
    delay(100);

    Serial.println("Increasing volume");
    bleGamepad.pressVolumeInc();
    delay(1500);
    bleGamepad.releaseVolumeInc();

    Serial.println("Muting volume");
    bleGamepad.pressVolumeMute();
    delay(100);
    bleGamepad.releaseVolumeMute();

    Serial.println("Pressing menu and back");
    bleGamepad.pressMenu();
    delay(100);
    bleGamepad.releaseMenu();
    delay(100);
    bleGamepad.pressBack();
    delay(100);
    bleGamepad.releaseBack();
    delay(100);

    Serial.println("Pressing home");
    bleGamepad.pressHome();
    delay(100);
    bleGamepad.releaseHome();
}
