# Based on common issues reported by users:

## Configuration Changes Not Taking Effect:
Symptom: Updated configurations (e.g., button count, button mappings, device name, HID settings) are not reflected when reconnecting to the host device.  

Cause: Many host operating systems cache Bluetooth device characteristics (e.g., device name, button configurations, and HID descriptors) for faster reconnections. When the ESP32 configuration changes, the cached data may no longer match the updated settings, leading to inconsistencies.  

Solution:
- If you modify the BLE Gamepad configuration, such as device name, device features, button count, axes count or button assignments, you must remove the previously paired connection from the host device (PC, mobile, or console).
- Go to the host's Bluetooth settings, locate the ESP32 device, and select "Remove," "Delete," "Unpair," or "Forget."
- Restart both the ESP32 and the host device, then re-pair them to ensure the updated configuration is applied.
- Ensure you're actually using a custom configuration with ``bleGamepad.begin(&bleGamepadConfig);`` as shown in the CharacteristicsConfiguration.ino example

## Button Mapping Issues:
Symptom: Buttons not responding as expected or incorrect button assignments.  

Solution:
- Review the button mapping configuration in your code.
- Ensure that each button is correctly assigned according to the library's documentation. 

## Connection Problems:
Symptom: The ESP32 device does not appear on the PC and/or is visible on mobile devices.  

Solution:
- Verify that your PC's Bluetooth drivers are up to date.
- Additionally, ensure that the ESP32 is not already paired with another device, which might prevent it from appearing on the PC.
  - See ForcePairingMode.ino example for the 3 different methods of dealing with this
- ESP32-S2 Series has no Bluetooth
- ESP32, ESP32-C3 and ESP32-S3 series have been tested to work

## Custom Configuration Challenges:
Symptom: Difficulties in setting up a custom configuration for the gamepad.  

Solution:
- Refer to the library's examples and documentation to understand the correct procedures for custom configurations.
- Ensure that all parameters are set correctly and that there are no conflicts in the configuration.
- Ensure you're actually using a custom configuration with ``bleGamepad.begin(&bleGamepadConfig);`` as shown in the CharacteristicsConfiguration.ino example

## Multiple Device Pairing Issues:
Symptom: Inability to pair multiple devices simultaneously.

Solution:
- Update your ESP32-BLE-Gamepad and NimBLE-Arduino libraries to the latest versions
- Try setting different PID values, though this shouldn't be needed

## Slow or Inconsistent Connections:
Symptom: The ESP32 takes a long time to connect or fails to connect intermittently.  

Solution:
- Ensure that the ESP32 is in close proximity to the host device to avoid signal interference.
- Check for any power supply issues; unstable power can affect Bluetooth performance.
- Update the ESP32 firmware and the BLE-Gamepad library to the latest versions, as updates often include connectivity improvements.
- Try setting a stronger TX power level (9 is the highest) as shown in example CharacteristicsConfiguration.ino

## High Latency or Unresponsive Controls:
Symptom: Noticeable delay between input on the gamepad and action on the host device.  

Solution:
- Optimize the code to reduce any delays in the loop handling the gamepad inputs.
- Minimize the use of delay functions in the code, as they can introduce latency.
- Ensure that the BLE connection interval is set appropriately for low-latency communication.

## Pin Functionality Issues:
Symptom: Certain GPIO pins not working as intended for button inputs or analog readings.  

Solution:
- Consult the ESP32 pinout to ensure that the selected pins support the desired functions.
- Avoid using pins that are reserved or have dual functions that might interfere with input readings.
- Test the pins independently to confirm their functionality before integrating them into the gamepad setup.
- Pinouts:
  - [ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
  - [ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/gpio.html)
  - [ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html)

## Device Name:
Symptom: Name in testing software is generic such as '8 axis 16 button device with hat switch'.  

Solution: **Linux**: 
- Please contribute method if you know how

Solution: **Windows**: You need to change a registry entry if you want that name changed

**Custom display name by means of the Windows registry editor (Windows only)**

- Open the "registry editor" (regedit.exe) with user privileges (not administrator).
- Navigate to the following folder (registry key):
      HKEY_CURRENT_USER\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM
- Inside that folder, locate the subfolder (registry key) for your device.
- Those keys are named using this pattern: "VID -hex number- &PID -hex number- ", where "VID" means "vendor identifier" and "PID" means "product identifier". Together, they are a (plug-and-play) hardware identifier (ID). - The default hardware ID set in this project is "VID_E502&PID_BBAB".
- Double-click (edit) the value "OEMName".
- Set a custom display name and hit "enter".
- NOTE: Info taken from [here](https://github.com/afpineda/OpenSourceSimWheelESP32/blob/main/doc/RenameDeviceWin_en.md#why-all-this-mess)
