#include "BleGamepadConfiguration.h"

BleGamepadConfiguration::BleGamepadConfiguration() : _controllerType(CONTROLLER_TYPE_GAMEPAD),
                                                     _autoReport(true),
                                                     _hidReportId(3),
                                                     _buttonCount(16),
                                                     _hatSwitchCount(1),
                                                     _whichSpecialButtons{false, false, false, false, false, false, false, false},
                                                     _whichAxes{true, true, true, true, true, true, true, true},
                                                     _whichSimulationControls{false, false, false, false, false},
                                                     _includeGyroscope(false),
                                                     _includeAccelerometer(false),
                                                     _vid(0xe502),
                                                     _pid(0xbbab),
                                                     _guidVersion(0x0110),
                                                     _axesMin(0x0000),
                                                     _axesMax(0x7FFF),
                                                     _simulationMin(0x0000),
                                                     _simulationMax(0x7FFF),
                                                     _motionMin(0x0000),
                                                     _motionMax(0x7FFF),
                                                     _modelNumber("1.0.0"),
                                                     _softwareRevision("1.0.0"),
                                                     _serialNumber("0123456789"),
                                                     _firmwareRevision("0.7.3"),
                                                     _hardwareRevision("1.0.0"),
                                                     _enableOutputReport(false),
                                                     _outputReportLength(64),
													                           _powerLevel(9)
{
}

uint8_t BleGamepadConfiguration::getTotalSpecialButtonCount()
{
    int count = 0;
    for (int i = 0; i < POSSIBLESPECIALBUTTONS; i++)
    {
        count += (int)_whichSpecialButtons[i];
    }

    return count;
}

uint8_t BleGamepadConfiguration::getDesktopSpecialButtonCount()
{
    int count = 0;
    for (int i = 0; i < 3; i++)
    {
        count += (int)_whichSpecialButtons[i];
    }

    return count;
}

uint8_t BleGamepadConfiguration::getConsumerSpecialButtonCount()
{
    int count = 0;
    for (int i = 3; i < 8; i++)
    {
        count += (int)_whichSpecialButtons[i];
    }

    return count;
}

uint8_t BleGamepadConfiguration::getAxisCount()
{
    int count = 0;
    for (int i = 0; i < POSSIBLEAXES; i++)
    {
        count += (int)_whichAxes[i];
    }

    return count;
}

uint8_t BleGamepadConfiguration::getSimulationCount()
{
    int count = 0;
    for (int i = 0; i < POSSIBLESIMULATIONCONTROLS; i++)
    {
        count += (int)_whichSimulationControls[i];
    }

    return count;
}

uint16_t BleGamepadConfiguration::getVid(){ return _vid; }
uint16_t BleGamepadConfiguration::getPid(){ return _pid; }
uint16_t BleGamepadConfiguration::getGuidVersion(){ return _guidVersion; }
int16_t BleGamepadConfiguration::getAxesMin(){ return _axesMin; }
int16_t BleGamepadConfiguration::getAxesMax(){ return _axesMax; }
int16_t BleGamepadConfiguration::getSimulationMin(){ return _simulationMin; }
int16_t BleGamepadConfiguration::getSimulationMax(){ return _simulationMax; }
int16_t BleGamepadConfiguration::getMotionMin(){ return _motionMin; }
int16_t BleGamepadConfiguration::getMotionMax(){ return _motionMax; }
uint8_t BleGamepadConfiguration::getControllerType() { return _controllerType; }
uint8_t BleGamepadConfiguration::getHidReportId() { return _hidReportId; }
uint16_t BleGamepadConfiguration::getButtonCount() { return _buttonCount; }
uint8_t BleGamepadConfiguration::getHatSwitchCount() { return _hatSwitchCount; }
bool BleGamepadConfiguration::getAutoReport() { return _autoReport; }
bool BleGamepadConfiguration::getIncludeStart() { return _whichSpecialButtons[START_BUTTON]; }
bool BleGamepadConfiguration::getIncludeSelect() { return _whichSpecialButtons[SELECT_BUTTON]; }
bool BleGamepadConfiguration::getIncludeMenu() { return _whichSpecialButtons[MENU_BUTTON]; }
bool BleGamepadConfiguration::getIncludeHome() { return _whichSpecialButtons[HOME_BUTTON]; }
bool BleGamepadConfiguration::getIncludeBack() { return _whichSpecialButtons[BACK_BUTTON]; }
bool BleGamepadConfiguration::getIncludeVolumeInc() { return _whichSpecialButtons[VOLUME_INC_BUTTON]; }
bool BleGamepadConfiguration::getIncludeVolumeDec() { return _whichSpecialButtons[VOLUME_DEC_BUTTON]; }
bool BleGamepadConfiguration::getIncludeVolumeMute() { return _whichSpecialButtons[VOLUME_MUTE_BUTTON]; }
const bool *BleGamepadConfiguration::getWhichSpecialButtons() const { return _whichSpecialButtons; }
bool BleGamepadConfiguration::getIncludeXAxis() { return _whichAxes[X_AXIS]; }
bool BleGamepadConfiguration::getIncludeYAxis() { return _whichAxes[Y_AXIS]; }
bool BleGamepadConfiguration::getIncludeZAxis() { return _whichAxes[Z_AXIS]; }
bool BleGamepadConfiguration::getIncludeRxAxis() { return _whichAxes[RX_AXIS]; }
bool BleGamepadConfiguration::getIncludeRyAxis() { return _whichAxes[RY_AXIS]; }
bool BleGamepadConfiguration::getIncludeRzAxis() { return _whichAxes[RZ_AXIS]; }
bool BleGamepadConfiguration::getIncludeSlider1() { return _whichAxes[SLIDER1]; }
bool BleGamepadConfiguration::getIncludeSlider2() { return _whichAxes[SLIDER2]; }
const bool *BleGamepadConfiguration::getWhichAxes() const { return _whichAxes; }
bool BleGamepadConfiguration::getIncludeRudder() { return _whichSimulationControls[RUDDER]; }
bool BleGamepadConfiguration::getIncludeThrottle() { return _whichSimulationControls[THROTTLE]; }
bool BleGamepadConfiguration::getIncludeAccelerator() { return _whichSimulationControls[ACCELERATOR]; }
bool BleGamepadConfiguration::getIncludeBrake() { return _whichSimulationControls[BRAKE]; }
bool BleGamepadConfiguration::getIncludeSteering() { return _whichSimulationControls[STEERING]; }
const bool *BleGamepadConfiguration::getWhichSimulationControls() const { return _whichSimulationControls; }
bool BleGamepadConfiguration::getIncludeGyroscope() { return _includeGyroscope; }
bool BleGamepadConfiguration::getIncludeAccelerometer() { return _includeAccelerometer; }
char *BleGamepadConfiguration::getModelNumber(){ return _modelNumber; }
char *BleGamepadConfiguration::getSoftwareRevision(){ return _softwareRevision; }
char *BleGamepadConfiguration::getSerialNumber(){ return _serialNumber; }
char *BleGamepadConfiguration::getFirmwareRevision(){ return _firmwareRevision; }
char *BleGamepadConfiguration::getHardwareRevision(){ return _hardwareRevision; }
bool BleGamepadConfiguration::getEnableOutputReport(){ return _enableOutputReport; }
uint16_t BleGamepadConfiguration::getOutputReportLength(){ return _outputReportLength; }
int8_t BleGamepadConfiguration::getTXPowerLevel(){ return _powerLevel; }	// Returns the power level that was set as the server started

void BleGamepadConfiguration::setWhichSpecialButtons(bool start, bool select, bool menu, bool home, bool back, bool volumeInc, bool volumeDec, bool volumeMute)
{
    _whichSpecialButtons[START_BUTTON] = start;
    _whichSpecialButtons[SELECT_BUTTON] = select;
    _whichSpecialButtons[MENU_BUTTON] = menu;
    _whichSpecialButtons[HOME_BUTTON] = home;
    _whichSpecialButtons[BACK_BUTTON] = back;
    _whichSpecialButtons[VOLUME_INC_BUTTON] = volumeInc;
    _whichSpecialButtons[VOLUME_DEC_BUTTON] = volumeDec;
    _whichSpecialButtons[VOLUME_MUTE_BUTTON] = volumeMute;
}

void BleGamepadConfiguration::setWhichAxes(bool xAxis, bool yAxis, bool zAxis, bool rxAxis, bool ryAxis, bool rzAxis, bool slider1, bool slider2)
{
    _whichAxes[X_AXIS] = xAxis;
    _whichAxes[Y_AXIS] = yAxis;
    _whichAxes[Z_AXIS] = zAxis;
    _whichAxes[RZ_AXIS] = rzAxis;
    _whichAxes[RX_AXIS] = rxAxis;
    _whichAxes[RY_AXIS] = ryAxis;
    _whichAxes[SLIDER1] = slider1;
    _whichAxes[SLIDER2] = slider2;
}

void BleGamepadConfiguration::setWhichSimulationControls(bool rudder, bool throttle, bool accelerator, bool brake, bool steering)
{
    _whichSimulationControls[RUDDER] = rudder;
    _whichSimulationControls[THROTTLE] = throttle;
    _whichSimulationControls[ACCELERATOR] = accelerator;
    _whichSimulationControls[BRAKE] = brake;
    _whichSimulationControls[STEERING] = steering;
}

void BleGamepadConfiguration::setControllerType(uint8_t value) { _controllerType = value; }
void BleGamepadConfiguration::setHidReportId(uint8_t value) { _hidReportId = value; }
void BleGamepadConfiguration::setButtonCount(uint16_t value) { _buttonCount = value; }
void BleGamepadConfiguration::setHatSwitchCount(uint8_t value) { _hatSwitchCount = value; }
void BleGamepadConfiguration::setAutoReport(bool value) { _autoReport = value; }
void BleGamepadConfiguration::setIncludeStart(bool value) { _whichSpecialButtons[START_BUTTON] = value; }
void BleGamepadConfiguration::setIncludeSelect(bool value) { _whichSpecialButtons[SELECT_BUTTON] = value; }
void BleGamepadConfiguration::setIncludeMenu(bool value) { _whichSpecialButtons[MENU_BUTTON] = value; }
void BleGamepadConfiguration::setIncludeHome(bool value) { _whichSpecialButtons[HOME_BUTTON] = value; }
void BleGamepadConfiguration::setIncludeBack(bool value) { _whichSpecialButtons[BACK_BUTTON] = value; }
void BleGamepadConfiguration::setIncludeVolumeInc(bool value) { _whichSpecialButtons[VOLUME_INC_BUTTON] = value; }
void BleGamepadConfiguration::setIncludeVolumeDec(bool value) { _whichSpecialButtons[VOLUME_DEC_BUTTON] = value; }
void BleGamepadConfiguration::setIncludeVolumeMute(bool value) { _whichSpecialButtons[VOLUME_MUTE_BUTTON] = value; }
void BleGamepadConfiguration::setIncludeXAxis(bool value) { _whichAxes[X_AXIS] = value; }
void BleGamepadConfiguration::setIncludeYAxis(bool value) { _whichAxes[Y_AXIS] = value; }
void BleGamepadConfiguration::setIncludeZAxis(bool value) { _whichAxes[Z_AXIS] = value; }
void BleGamepadConfiguration::setIncludeRzAxis(bool value) { _whichAxes[RZ_AXIS] = value; }
void BleGamepadConfiguration::setIncludeRxAxis(bool value) { _whichAxes[RX_AXIS] = value; }
void BleGamepadConfiguration::setIncludeRyAxis(bool value) { _whichAxes[RY_AXIS] = value; }
void BleGamepadConfiguration::setIncludeSlider1(bool value) { _whichAxes[SLIDER1] = value; }
void BleGamepadConfiguration::setIncludeSlider2(bool value) { _whichAxes[SLIDER2] = value; }
void BleGamepadConfiguration::setIncludeRudder(bool value) { _whichSimulationControls[RUDDER] = value; }
void BleGamepadConfiguration::setIncludeThrottle(bool value) { _whichSimulationControls[THROTTLE] = value; }
void BleGamepadConfiguration::setIncludeAccelerator(bool value) { _whichSimulationControls[ACCELERATOR] = value; }
void BleGamepadConfiguration::setIncludeBrake(bool value) { _whichSimulationControls[BRAKE] = value; }
void BleGamepadConfiguration::setIncludeSteering(bool value) { _whichSimulationControls[STEERING] = value; }
void BleGamepadConfiguration::setIncludeGyroscope(bool value) { _includeGyroscope = value; }
void BleGamepadConfiguration::setIncludeAccelerometer(bool value) { _includeAccelerometer = value; }
void BleGamepadConfiguration::setVid(uint16_t value) { _vid = value; }
void BleGamepadConfiguration::setPid(uint16_t value) { _pid = value; }
void BleGamepadConfiguration::setGuidVersion(uint16_t value) { _guidVersion = value; }
void BleGamepadConfiguration::setAxesMin(int16_t value) { _axesMin = value; }
void BleGamepadConfiguration::setAxesMax(int16_t value) { _axesMax = value; }
void BleGamepadConfiguration::setSimulationMin(int16_t value) { _simulationMin = value; }
void BleGamepadConfiguration::setSimulationMax(int16_t value) { _simulationMax = value; }
void BleGamepadConfiguration::setMotionMin(int16_t value) { _motionMin = value; }
void BleGamepadConfiguration::setMotionMax(int16_t value) { _motionMax = value; }
void BleGamepadConfiguration::setModelNumber(char *value) { _modelNumber = value; }
void BleGamepadConfiguration::setSoftwareRevision(char *value) { _softwareRevision = value; }
void BleGamepadConfiguration::setSerialNumber(char *value) { _serialNumber = value; }
void BleGamepadConfiguration::setFirmwareRevision(char *value) { _firmwareRevision = value; }
void BleGamepadConfiguration::setHardwareRevision(char *value) { _hardwareRevision = value; }
void BleGamepadConfiguration::setEnableOutputReport(bool value) { _enableOutputReport = value; }
void BleGamepadConfiguration::setOutputReportLength(uint16_t value) { _outputReportLength = value; }
void BleGamepadConfiguration::setTXPowerLevel(int8_t value) { _powerLevel = value; }
