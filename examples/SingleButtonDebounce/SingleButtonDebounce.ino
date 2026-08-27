#include <Arduino.h>
#include <Bounce2.h>    // https://github.com/thomasfredericks/Bounce2
#include <BleGamepad.h> // https://github.com/lemmingDev/ESP32-BLE-Gamepad

#define BOUNCE_WITH_PROMPT_DETECTION // Make button state changes available immediately
#define BUTTON_PIN 35
#define LED_PIN 13

Bounce debouncer = Bounce(); // Instantiate a Bounce object
BleGamepad bleGamepad;       // Instantiate a BleGamepad object

void setup()
{
    bleGamepad.begin(); // Begin the gamepad

    pinMode(BUTTON_PIN, INPUT_PULLUP); // Setup the button with an internal pull-up

    debouncer.attach(BUTTON_PIN); // After setting up the button, setup the Bounce instance :
    debouncer.interval(5);        // interval in ms

    pinMode(LED_PIN, OUTPUT); // Setup the LED :
}

void loop()
{
    if (bleGamepad.isConnected())
    {
        debouncer.update(); // Update the Bounce instance

        int value = debouncer.read(); // Get the updated value

        // Press/release gamepad button and turn on or off the LED as determined by the state
        if (value == LOW)
        {
            digitalWrite(LED_PIN, HIGH);
            bleGamepad.press(BUTTON_1);
        }
        else
        {
            digitalWrite(LED_PIN, LOW);
            bleGamepad.release(BUTTON_1);
        }
    }
}
