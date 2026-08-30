#include <Arduino.h>
#include <BleGamepad.h>

BleGamepad bleGamepad;

void setup()
{
    Serial.begin(115200);
    BleGamepadConfiguration bleGamepadConfig;
    bleGamepadConfig.setWhichSpecialButtons(true, true, true, true, true, true, true, true);
    // Can also enable special buttons individually with the following <-- They are all disabled by default
    // bleGamepadConfig.setIncludeStart(true);
    // bleGamepadConfig.setIncludeSelect(true);
    // bleGamepadConfig.setIncludeMenu(true);
    // bleGamepadConfig.setIncludeHome(true);
    // bleGamepadConfig.setIncludeBack(true);
    // bleGamepadConfig.setIncludeVolumeInc(true);
    // bleGamepadConfig.setIncludeVolumeDec(true);
    // bleGamepadConfig.setIncludeVolumeMute(true);
    bleGamepad.begin(&bleGamepadConfig);

    // Changing bleGamepadConfig after the begin function has no effect, unless you call the begin function again
}

void loop()
{
    if (bleGamepad.isConnected())
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
        delay(100);
        bleGamepad.releaseVolumeInc();
        delay(100);
        bleGamepad.pressVolumeInc();
        delay(100);
        bleGamepad.releaseVolumeInc();
        delay(100);
        
        Serial.println("Muting volume");
        bleGamepad.pressVolumeMute();
        delay(100);
        bleGamepad.releaseVolumeMute();
        delay(1000);
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
        delay(2000);
    }
}
