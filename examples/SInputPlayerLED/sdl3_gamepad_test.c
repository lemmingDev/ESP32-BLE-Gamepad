/*
 * Minimal SDL3 test for SInputPlayerLED.ino: opens the first recognized SInput
 * gamepad, prints its identity, cycles the Player LED index every few seconds
 * (watch the ESP32's Serial output / onboard LED react), and continuously
 * prints button/axis/battery state as it changes.
 *
 * No display needed -- this only uses SDL_INIT_GAMEPAD, so it runs fine over
 * SSH on a headless machine. See SDL3Testing.md in this directory for how to
 * build SDL3, the required hint, and what to expect.
 *
 * Build:   gcc sdl3_gamepad_test.c -o sdl3_gamepad_test $(pkg-config --cflags --libs sdl3)
 * Run:     ./sdl3_gamepad_test
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *power_state_name(SDL_PowerState state)
{
    switch (state)
    {
        case SDL_POWERSTATE_ON_BATTERY: return "on battery";
        case SDL_POWERSTATE_NO_BATTERY: return "no battery";
        case SDL_POWERSTATE_CHARGING:   return "charging";
        case SDL_POWERSTATE_CHARGED:    return "charged";
        case SDL_POWERSTATE_ERROR:      return "error";
        default:                        return "unknown";
    }
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_GAMEPAD))
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Gamepad *gamepad = NULL;
    printf("Waiting for a gamepad (pair/connect the ESP32 now if it isn't already)...\n");
    while (!gamepad)
    {
        SDL_PumpEvents();

        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        if (ids)
        {
            if (count > 0)
            {
                gamepad = SDL_OpenGamepad(ids[0]);
            }
            SDL_free(ids);
        }

        if (!gamepad)
        {
            SDL_Delay(200);
        }
    }

    Uint16 vendor = SDL_GetGamepadVendor(gamepad);
    Uint16 product = SDL_GetGamepadProduct(gamepad);
    printf("Opened: %s (VID=0x%04X PID=0x%04X) -- expect VID=0x2E8A PID=0x10C6 for SInput mode\n",
           SDL_GetGamepadName(gamepad), vendor, product);

    SDL_PropertiesID props = SDL_GetGamepadProperties(gamepad);
    printf("Rumble capable: %s (expected false -- not implemented by this library yet, see GattVsHid.md)\n",
           SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false) ? "true" : "false");

    Uint64 lastLedChange = 0;
    int playerIndex = 0;
    bool lastButtons[SDL_GAMEPAD_BUTTON_COUNT];
    memset(lastButtons, 0, sizeof(lastButtons));
    int lastPercent = -1;
    SDL_PowerState lastPowerState = SDL_POWERSTATE_UNKNOWN;

    for (;;)
    {
        SDL_PumpEvents();

        Uint64 now = SDL_GetTicks();

        // Cycle the Player LED index every 3s so you can watch the ESP32
        // react (Serial log + onboard LED) without needing another app.
        if (now - lastLedChange >= 3000)
        {
            lastLedChange = now;
            playerIndex = (playerIndex + 1) % 4;
            SDL_SetGamepadPlayerIndex(gamepad, playerIndex);
            printf("-> SDL_SetGamepadPlayerIndex(%d)\n", playerIndex);
        }

        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
        {
            bool down = SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)b);
            if (down != lastButtons[b])
            {
                lastButtons[b] = down;
                printf("Button %d: %s\n", b, down ? "down" : "up");
            }
        }

        int percent = -1;
        SDL_PowerState powerState = SDL_GetGamepadPowerInfo(gamepad, &percent);
        if (percent != lastPercent || powerState != lastPowerState)
        {
            lastPercent = percent;
            lastPowerState = powerState;
            printf("Battery: %d%% (%s)\n", percent, power_state_name(powerState));
        }

        SDL_Delay(50);
    }

    SDL_CloseGamepad(gamepad);
    SDL_Quit();
    return 0;
}
