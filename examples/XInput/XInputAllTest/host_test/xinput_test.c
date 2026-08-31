/*
 * xinput_test.c — Minimal Windows XInput verifier for
 * examples/XInput/XInputAllTest/XInputAllTest.ino
 *
 * Verifies the ESP32 is routed via the Microsoft XInput driver
 * (VID 0x045E PID 0x02FD/0x0B13, HID Report PID 0x03) and not just
 * generic HID / DirectInput ("6 axis 16 button" in joy.cpl).
 *
 * Build (MinGW):
 *   gcc host_test/xinput_test.c -lxinput -o host_test/xinput_test.exe
 * Build (MSVC):
 *   cl host_test/xinput_test.c xinput.lib
 * Run while XInputAllTest.ino is paired and circling:
 *   ./host_test/xinput_test.exe
 * Expected: dwPacketNumber increments, wButtons/sThumb*/bTriggers follow
 * the sketch's cycle (A/B/X/Y/LB/RB/LS/RS/Start/Select/Home, hat via
 * thumb, triggers 0..1023 mapped to 0..255). If XInputGetState returns
 * ERROR_DEVICE_NOT_CONNECTED for all XUSER_MAX_COUNT even though
 * joy.cpl shows OK, the Report Map / VID:PID allowlist is wrong
 * (see BleXInputDescriptors.h:16, BleGamepad.cpp:973).
 */
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")
#else
#error "xinput_test.c is Windows-only. For Linux use host_test/xpad_evdev_test.py"
#endif

static const char *btnName(WORD w) {
    // Cheap decoder for the bits BleXInput.h:24 / BleGamepad.cpp:1372 maps
    // to XINPUT_GAMEPAD_* - only for printf, not exhaustive.
    static char buf[128];
    buf[0] = 0;
    if (w & XINPUT_GAMEPAD_A)              strcat(buf, " A");
    if (w & XINPUT_GAMEPAD_B)              strcat(buf, " B");
    if (w & XINPUT_GAMEPAD_X)              strcat(buf, " X");
    if (w & XINPUT_GAMEPAD_Y)              strcat(buf, " Y");
    if (w & XINPUT_GAMEPAD_LEFT_SHOULDER)  strcat(buf, " LB");
    if (w & XINPUT_GAMEPAD_RIGHT_SHOULDER) strcat(buf, " RB");
    if (w & XINPUT_GAMEPAD_LEFT_THUMB)     strcat(buf, " LS");
    if (w & XINPUT_GAMEPAD_RIGHT_THUMB)    strcat(buf, " RS");
    if (w & XINPUT_GAMEPAD_START)          strcat(buf, " Start");
    if (w & XINPUT_GAMEPAD_BACK)           strcat(buf, " Back");
    return buf[0] ? buf + 1 : "(none)";
}

int main(void) {
    printf("XInput polling XUSER_MAX_COUNT=%d, Ctrl-C to exit.\n", XUSER_MAX_COUNT);
    printf("Pair XInputAllTest.ino as \"Xbox Wireless Controller\" (VID 045E:02FD) first.\n");
    DWORD lastPkt[XUSER_MAX_COUNT] = {0};
    while (1) {
        int found = 0;
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
            XINPUT_STATE st; ZeroMemory(&st, sizeof(st));
            DWORD r = XInputGetState(i, &st);
            if (r == ERROR_SUCCESS) {
                found = 1;
                if (st.dwPacketNumber != lastPkt[i]) {
                    printf("[%lu] pkt=%lu btn=%04x (%s) LX=%6d LY=%6d RX=%6d RY=%6d LT=%3u RT=%3u\n",
                           i, st.dwPacketNumber, st.Gamepad.wButtons, btnName(st.Gamepad.wButtons),
                           st.Gamepad.sThumbLX, st.Gamepad.sThumbLY,
                           st.Gamepad.sThumbRX, st.Gamepad.sThumbRY,
                           st.Gamepad.bLeftTrigger, st.Gamepad.bRightTrigger);
                    lastPkt[i] = st.dwPacketNumber;
                }
            }
        }
        if (!found) {
            static int t = 0;
            if ((t++ % 100) == 0)
                printf("No XInput device — check joy.cpl shows OK but XInputGetState still ERROR_DEVICE_NOT_CONNECTED (descriptor/PID mismatch, see BleXInputDescriptors.h:16)\n");
        }
        Sleep(10);
        // Optional rumble check: uncomment to drive the sketch's BleXInput.cpp:8 path
        // XINPUT_VIBRATION vib = { 16000, 16000 }; XInputSetState(0, &vib);
    }
    return 0;
}
