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
 * Run:     ./sdl3_gamepad_test [-v]
 *
 * -v/--verbose enables SDL_LOG_CATEGORY_INPUT at SDL_LOG_PRIORITY_VERBOSE,
 * SDL3's dedicated log category for the joystick/gamepad/hidapi subsystem --
 * useful for chasing the "Known issue" in SDL3Testing.md (SDL not delivering
 * Player LED commands): it surfaces whatever the SInput hidapi driver logs
 * during its Features-response parse. To go further still (the driver's own
 * DEBUG_SINPUT_INIT/DEBUG_SINPUT_PROTOCOL macros in
 * src/joystick/hidapi/SDL_hidapi_sinput.c), you need to flip those on and
 * rebuild SDL itself -- this flag alone can't reach code gated out at
 * compile time. See sinput_hid_test.py in this directory for a way to test
 * the device's actual protocol behavior independent of SDL entirely.
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

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0); // line-buffer stdout even when piped/redirected (e.g. over SSH)

    bool verbose = false;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            verbose = true;
    }
    if (verbose)
    {
        // Set before SDL_Init() so subsystem-init-time logging (device
        // enumeration, the Features-response parse) isn't missed.
        SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_VERBOSE);
        printf("Verbose SDL_LOG_CATEGORY_INPUT logging enabled.\n");
    }

    if (!SDL_Init(SDL_INIT_GAMEPAD))
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int sdlVersion = SDL_GetVersion();
    printf("SDL runtime version: %d.%d.%d -- must be 3.4.x+ for the SInput driver, see SDL3Testing.md step 1\n",
           SDL_VERSIONNUM_MAJOR(sdlVersion), SDL_VERSIONNUM_MINOR(sdlVersion), SDL_VERSIONNUM_MICRO(sdlVersion));

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

    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
    const char *devicePath = joystick ? SDL_GetJoystickPath(joystick) : NULL;
    printf("Underlying device path: %s (cross-reference with sinput_hid_test.py --device <path>, or a "
           "concurrent `sudo btmon -i hci0`)\n", devicePath ? devicePath : "(none reported)");

    SDL_PropertiesID props = SDL_GetGamepadProperties(gamepad);
    printf("Rumble capable: %s (expected false -- not implemented by this library yet, see GattVsHid.md)\n",
           SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false) ? "true" : "false");

    // This is SDL's own view of the SInput driver's internal
    // player_leds_supported flag (see SDL3Testing.md's "Known issue" section)
    // surfaced through the public API -- if this prints false while
    // sinput_hid_test.py's Features-response dump shows the PLAYERLED bit
    // set, that mismatch *is* the bug, and it's on SDL's parsing, not this
    // library's firmware.
    printf("Player LED capable (SDL's view): %s -- expected true; false means SDL will silently drop every "
           "SDL_SetGamepadPlayerIndex() call below\n",
           SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN, false) ? "true" : "false");

    Uint64 lastLedChange = 0;
    int playerIndex = 0;
    bool lastButtons[SDL_GAMEPAD_BUTTON_COUNT];
    memset(lastButtons, 0, sizeof(lastButtons));
    int lastPercent = -1;
    SDL_PowerState lastPowerState = SDL_POWERSTATE_UNKNOWN;

    bool quit = false;
    while (!quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // SDL installs its own SIGINT/SIGTERM handler that turns the signal
            // into this event rather than killing the process outright -- without
            // checking for it, Ctrl-C (or `timeout`, which sends SIGTERM by
            // default) would never actually stop this loop.
            if (event.type == SDL_EVENT_QUIT)
            {
                quit = true;
            }
        }
        if (quit)
        {
            break;
        }

        Uint64 now = SDL_GetTicks();

        // Cycle the Player LED index every 3s so you can watch the ESP32
        // react (Serial log + onboard LED) without needing another app.
        if (now - lastLedChange >= 3000)
        {
            lastLedChange = now;
            playerIndex = (playerIndex + 1) % 4;
            bool ok = SDL_SetGamepadPlayerIndex(gamepad, playerIndex);
            if (ok)
                printf("-> SDL_SetGamepadPlayerIndex(%d) succeeded\n", playerIndex);
            else
                printf("-> SDL_SetGamepadPlayerIndex(%d) FAILED: %s\n", playerIndex, SDL_GetError());
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
