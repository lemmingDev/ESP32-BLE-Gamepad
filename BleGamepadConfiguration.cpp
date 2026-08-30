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
                                                     _axesMin(0x8000),
                                                     _axesMax(0x7FFF),
                                                     _simulationMin(0x0000),
                                                     _simulationMax(0x7FFF),
                                                     _motionMin(0x0000),
                                                     _motionMax(0x7FFF),
                                                     _modelNumber("1.0.0"),
                                                     _softwareRevision("1.0.0"),
                                                     _serialNumber("0123456789"),
                                                     _firmwareRevision("0.7.4"),
                                                     _hardwareRevision("1.0.0"),
                                                     _enableOutputReport(false),
                                                     _enableFeatureReport(false),
                                                     _enableNordicUARTService(false),
                                                     _enableRumble(false),
                                                     _enablePlayerLED(false),
                                                     _enableSInput(false),
                                                     _enableSInputIMU(false),
                                                     _enableSInputRGB(false),
                                                     _gamepadMode(GamepadMode::Generic),
                                                     _outputReportLength(64),
                                                     _featureReportLength(64),
                                                     _transmitPowerLevel(9),
                                                     _sinputGamepadType(1),
                                                     _sinputFaceStyle(1),
                                                     _enableTouchpad(false),
                                                     _touchpadCount(0),
                                                     _touchpadFingerCount(1)
{
}

uint8_t BleGamepadConfiguration::getTotalSpecialButtonCount() const
{
    int count = 0;
    for (int i = 0; i < POSSIBLESPECIALBUTTONS; i++)
    {
        count += (int)_whichSpecialButtons[i];
    }

    return count;
}

uint8_t BleGamepadConfiguration::getDesktopSpecialButtonCount() const
{
    int count = 0;
    for (int i = 0; i < 3; i++)
    {
        count += (int)_whichSpecialButtons[i];
    }

    return count;
}

uint8_t BleGamepadConfiguration::getConsumerSpecialButtonCount() const
{
    int count = 0;
    for (int i = 3; i < 8; i++)
    {
        count += (int)_whichSpecialButtons[i];
    }

    return count;
}

uint8_t BleGamepadConfiguration::getAxisCount() const
{
    int count = 0;
    for (int i = 0; i < POSSIBLEAXES; i++)
    {
        count += (int)_whichAxes[i];
    }

    return count;
}

uint8_t BleGamepadConfiguration::getSimulationCount() const
{
    int count = 0;
    for (int i = 0; i < POSSIBLESIMULATIONCONTROLS; i++)
    {
        count += (int)_whichSimulationControls[i];
    }

    return count;
}

uint16_t BleGamepadConfiguration::getVid() const { return _vid; }
uint16_t BleGamepadConfiguration::getPid() const { return _pid; }
uint16_t BleGamepadConfiguration::getGuidVersion() const { return _guidVersion; }
int16_t BleGamepadConfiguration::getAxesMin() const { return _axesMin; }
int16_t BleGamepadConfiguration::getAxesMax() const { return _axesMax; }
int16_t BleGamepadConfiguration::getSimulationMin() const { return _simulationMin; }
int16_t BleGamepadConfiguration::getSimulationMax() const { return _simulationMax; }
int16_t BleGamepadConfiguration::getMotionMin() const { return _motionMin; }
int16_t BleGamepadConfiguration::getMotionMax() const { return _motionMax; }
uint8_t BleGamepadConfiguration::getControllerType() const { return _controllerType; }
uint8_t BleGamepadConfiguration::getHidReportId() const { return _hidReportId; }
uint16_t BleGamepadConfiguration::getButtonCount() const { return _buttonCount; }
uint8_t BleGamepadConfiguration::getHatSwitchCount() const { return _hatSwitchCount; }
bool BleGamepadConfiguration::getAutoReport() const { return _autoReport; }
bool BleGamepadConfiguration::getIncludeStart() const { return _whichSpecialButtons[START_BUTTON]; }
bool BleGamepadConfiguration::getIncludeSelect() const { return _whichSpecialButtons[SELECT_BUTTON]; }
bool BleGamepadConfiguration::getIncludeMenu() const { return _whichSpecialButtons[MENU_BUTTON]; }
bool BleGamepadConfiguration::getIncludeHome() const { return _whichSpecialButtons[HOME_BUTTON]; }
bool BleGamepadConfiguration::getIncludeBack() const { return _whichSpecialButtons[BACK_BUTTON]; }
bool BleGamepadConfiguration::getIncludeVolumeInc() const { return _whichSpecialButtons[VOLUME_INC_BUTTON]; }
bool BleGamepadConfiguration::getIncludeVolumeDec() const { return _whichSpecialButtons[VOLUME_DEC_BUTTON]; }
bool BleGamepadConfiguration::getIncludeVolumeMute() const { return _whichSpecialButtons[VOLUME_MUTE_BUTTON]; }
const bool *BleGamepadConfiguration::getWhichSpecialButtons() const { return _whichSpecialButtons; }
bool BleGamepadConfiguration::getIncludeXAxis() const { return _whichAxes[X_AXIS]; }
bool BleGamepadConfiguration::getIncludeYAxis() const { return _whichAxes[Y_AXIS]; }
bool BleGamepadConfiguration::getIncludeZAxis() const { return _whichAxes[Z_AXIS]; }
bool BleGamepadConfiguration::getIncludeRxAxis() const { return _whichAxes[RX_AXIS]; }
bool BleGamepadConfiguration::getIncludeRyAxis() const { return _whichAxes[RY_AXIS]; }
bool BleGamepadConfiguration::getIncludeRzAxis() const { return _whichAxes[RZ_AXIS]; }
bool BleGamepadConfiguration::getIncludeSlider1() const { return _whichAxes[SLIDER1]; }
bool BleGamepadConfiguration::getIncludeSlider2() const { return _whichAxes[SLIDER2]; }
const bool *BleGamepadConfiguration::getWhichAxes() const { return _whichAxes; }
bool BleGamepadConfiguration::getIncludeRudder() const { return _whichSimulationControls[RUDDER]; }
bool BleGamepadConfiguration::getIncludeThrottle() const { return _whichSimulationControls[THROTTLE]; }
bool BleGamepadConfiguration::getIncludeAccelerator() const { return _whichSimulationControls[ACCELERATOR]; }
bool BleGamepadConfiguration::getIncludeBrake() const { return _whichSimulationControls[BRAKE]; }
bool BleGamepadConfiguration::getIncludeSteering() const { return _whichSimulationControls[STEERING]; }
const bool *BleGamepadConfiguration::getWhichSimulationControls() const { return _whichSimulationControls; }
bool BleGamepadConfiguration::getIncludeGyroscope() const { return _includeGyroscope; }
bool BleGamepadConfiguration::getIncludeAccelerometer() const { return _includeAccelerometer; }
const char *BleGamepadConfiguration::getModelNumber() const { return _modelNumber; }
const char *BleGamepadConfiguration::getSoftwareRevision() const { return _softwareRevision; }
const char *BleGamepadConfiguration::getSerialNumber() const { return _serialNumber; }
const char *BleGamepadConfiguration::getFirmwareRevision() const { return _firmwareRevision; }
const char *BleGamepadConfiguration::getHardwareRevision() const { return _hardwareRevision; }
bool BleGamepadConfiguration::getEnableOutputReport() const { return _enableOutputReport; }
bool BleGamepadConfiguration::getEnableFeatureReport() const { return _enableFeatureReport; }
bool BleGamepadConfiguration::getEnableNordicUARTService() const { return _enableNordicUARTService; }
bool BleGamepadConfiguration::getEnableRumble() const { return _enableRumble; }
bool BleGamepadConfiguration::getEnablePlayerLED() const { return _enablePlayerLED; }
bool BleGamepadConfiguration::getEnableSInput() const { return _enableSInput; }
bool BleGamepadConfiguration::getEnableSInputIMU() const { return _enableSInputIMU; }
bool BleGamepadConfiguration::getEnableSInputRGB() const { return _enableSInputRGB; }
GamepadMode BleGamepadConfiguration::getGamepadMode() const { return _gamepadMode; }
uint16_t BleGamepadConfiguration::getOutputReportLength() const { return _outputReportLength; }
uint16_t BleGamepadConfiguration::getFeatureReportLength() const { return _featureReportLength; }
int8_t BleGamepadConfiguration::getTXPowerLevel() const { return _transmitPowerLevel; }	// Returns the power level that was set as the server started

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
void BleGamepadConfiguration::setModelNumber(const char *value) { _modelNumber = value; }
void BleGamepadConfiguration::setSoftwareRevision(const char *value) { _softwareRevision = value; }
void BleGamepadConfiguration::setSerialNumber(const char *value) { _serialNumber = value; }
void BleGamepadConfiguration::setFirmwareRevision(const char *value) { _firmwareRevision = value; }
void BleGamepadConfiguration::setHardwareRevision(const char *value) { _hardwareRevision = value; }
void BleGamepadConfiguration::setEnableOutputReport(bool value) { _enableOutputReport = value; }
void BleGamepadConfiguration::setEnableFeatureReport(bool value) { _enableFeatureReport = value; }
void BleGamepadConfiguration::setEnableNordicUARTService(bool value) { _enableNordicUARTService = value; }
void BleGamepadConfiguration::setEnableRumble(bool value) { _enableRumble = value; }
void BleGamepadConfiguration::setEnablePlayerLED(bool value) { _enablePlayerLED = value; }

void BleGamepadConfiguration::setEnableSInput(bool value)
{
    _enableSInput = value;

    if (value)
    {
        _gamepadMode = GamepadMode::SInput;
        // SDL's SInput hidapi driver only matches this exact VID/PID pair (see the
        // SINPUT_USB_VID/SINPUT_USB_PID_GENERIC comment above) -- default to it so
        // SInput mode works out of the box. Call setVid()/setPid() after this to override.
        _vid = SINPUT_USB_VID;
        _pid = SINPUT_USB_PID_GENERIC;
        _buttonCount = 25;
    }
    else if (_gamepadMode == GamepadMode::SInput)
    {
        _gamepadMode = GamepadMode::Generic;
    }
}

void BleGamepadConfiguration::setGamepadMode(GamepadMode mode)
{
    _gamepadMode = mode;

    if (mode == GamepadMode::SInput)
    {
        _enableSInput = true;
        _vid = SINPUT_USB_VID;
        _pid = SINPUT_USB_PID_GENERIC;
        _buttonCount = 25;
        _hatSwitchCount = 1;
        _whichSimulationControls[RUDDER] = false;
        _whichSimulationControls[THROTTLE] = false;
        _whichSimulationControls[ACCELERATOR] = false;
        _whichSimulationControls[BRAKE] = false;
        _whichSimulationControls[STEERING] = false;
        _whichAxes[SLIDER1] = false;
        _whichAxes[SLIDER2] = false;
        _includeGyroscope = false;
        _includeAccelerometer = false;
        _enableOutputReport = false;
        _enableFeatureReport = false;
        _enableRumble = true;
        _enableTouchpad = true;
        _touchpadCount = 1;
        _touchpadFingerCount = 2;
    }
    else if (mode == GamepadMode::XInput || mode == GamepadMode::XInputSeriesX)
    {
        _enableSInput = false;
        _vid = XINPUT_USB_VID;
        _pid = (mode == GamepadMode::XInputSeriesX) ? XINPUT_PID_XBOX_SERIES_X : XINPUT_PID_XBOX_ONE_S;
        _buttonCount = 11;
        _hatSwitchCount = 1;
        _whichSimulationControls[RUDDER] = false;
        _whichSimulationControls[THROTTLE] = false;
        _whichSimulationControls[ACCELERATOR] = false;
        _whichSimulationControls[BRAKE] = false;
        _whichSimulationControls[STEERING] = false;
        _whichAxes[SLIDER1] = false;
        _whichAxes[SLIDER2] = false;
        _includeGyroscope = false;
        _includeAccelerometer = false;
        _enableOutputReport = false;
        _enableFeatureReport = false;
    }
    else if (mode == GamepadMode::Generic)
    {
        _enableSInput = false;
    }
}

void BleGamepadConfiguration::setEnableSInputIMU(bool value) { _enableSInputIMU = value; }
void BleGamepadConfiguration::setEnableSInputRGB(bool value) { _enableSInputRGB = value; }
void BleGamepadConfiguration::setOutputReportLength(uint16_t value) { _outputReportLength = value; }
void BleGamepadConfiguration::setFeatureReportLength(uint16_t value) { _featureReportLength = value; }
void BleGamepadConfiguration::setTXPowerLevel(int8_t value) { _transmitPowerLevel = value; }
uint8_t BleGamepadConfiguration::getSInputGamepadType() const { return _sinputGamepadType; }
uint8_t BleGamepadConfiguration::getSInputFaceStyle() const { return _sinputFaceStyle; }
void BleGamepadConfiguration::setSInputGamepadType(uint8_t value) { _sinputGamepadType = value; }
void BleGamepadConfiguration::setSInputFaceStyle(uint8_t value) { _sinputFaceStyle = value; }
bool BleGamepadConfiguration::getEnableTouchpad() const { return _enableTouchpad; }
uint8_t BleGamepadConfiguration::getTouchpadCount() const { return _touchpadCount; }
uint8_t BleGamepadConfiguration::getTouchpadFingerCount() const { return _touchpadFingerCount; }
void BleGamepadConfiguration::setEnableTouchpad(bool value) { _enableTouchpad = value; }
void BleGamepadConfiguration::setTouchpadCount(uint8_t value) { _touchpadCount = value; }
void BleGamepadConfiguration::setTouchpadFingerCount(uint8_t value) { _touchpadFingerCount = value; }
