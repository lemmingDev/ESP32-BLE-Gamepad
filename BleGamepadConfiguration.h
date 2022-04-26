#ifndef ESP32_BLE_GAMEPAD_CONFIG_H
#define ESP32_BLE_GAMEPAD_CONFIG_H

#define POSSIBLESPECIALBUTTONS = 8;
#define POSSIBLEAXES = 8;
#define POSSIBLESIMULATIONCONTROLS = 5;

class BleGamepadConfiguration
{
private:
    bool _autoReport;
	uint16_t _buttonCount;
	uint8_t _specialButtonCount;
	uint8_t _hatSwitchCount;
	bool _whichSpecialButtons[8];
    bool _whichAxes[8];
    bool _whichSimulationControls[5];

public:
    BleGamepadConfiguration();

    bool getAutoReport();
    uint16_t getButtonCount();
    uint8_t getSpecialButtonCount();
    uint8_t getHatSwitchCount();
    bool getIncludeStart();
    bool getIncludeSelect();
    bool getIncludeMenu();
    bool getIncludeHome();
    bool getIncludeBack();
    bool getIncludeVolumeInc();
    bool getIncludeVolumeDec();
    bool getIncludeVolumeMute();
    const bool* getwhichSpecialButtons() const;
    bool getIncludeXAxis();
    bool getIncludeYAxis();
    bool getIncludeZAxis();
    bool getIncludeRxAxis();
    bool getIncludeRyAxis();
    bool getIncludeRzAxis();
    bool getIncludeSlider1();
    bool getIncludeSlider2();
    const bool* getWhichAxes() const;
    bool getIncludeRudder();
    bool getIncludeThrottle();
    bool getIncludeAccelerator();
    bool getIncludeBrake();
    bool getIncludeSteering();
    const bool* getWhichSimulationControls() const;

    void setAutoReport(bool value);
    void setButtonCount(uint16_t value);
    void setHatSwitchCount(uint8_t value);
    void setIncludeStart(bool value);
    void setIncludeSelect(bool value);
    void setIncludeMenu(bool value);
    void setIncludeHome(bool value);
    void setIncludeBack(bool value);
    void setIncludeVolumeInc(bool value);
    void setIncludeVolumeDec(bool value);
    void setIncludeVolumeMute(bool value);
    void setwhichSpecialButtons(bool start, bool select, bool menu, bool home, bool back, bool volumeInc, bool volumeDec, bool volumeMute);
    void setIncludeXAxis(bool value);
    void setIncludeYAxis(bool value);
    void setIncludeZAxis(bool value);
    void setIncludeRxAxis(bool value);
    void setIncludeRyAxis(bool value);
    void setIncludeRzAxis(bool value);
    void setIncludeSlider1(bool value);
    void setIncludeSlider2(bool value);
    void setWhichAxes(bool xAxis, bool yAxis, bool zAxis, bool rxAxis, bool ryAxis, bool rzAxis, bool slider1, bool slider2);
    void setIncludeRudder(bool value);
    void setIncludeThrottle(bool value);
    void setIncludeAccelerator(bool value);
    void setIncludeBrake(bool value);
    void setIncludeSteering(bool value);
    void setWhichSimulationControls(bool rudder, bool throttle, bool accelerator, bool brake, bool steering);
}

#endif