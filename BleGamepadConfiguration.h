#ifndef ESP32_BLE_GAMEPAD_CONFIG_H
#define ESP32_BLE_GAMEPAD_CONFIG_H

class BleGamepadConfiguration
{
private:
    bool _autoReport;
	uint16_t _buttonCount;
	uint8_t _specialButtonCount;
	uint8_t _hatSwitchCount;
	bool _includeSpecialButton[8];
    bool _includeAxis[8];
    bool _includeSimulation[5];

public:
    bool autoReport();
    uint16_t _buttonCount();
    uint16_t 
}

#endif