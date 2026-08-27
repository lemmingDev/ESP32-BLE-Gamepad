# PlatformIO pre-build step: derive a build-time identifier for the library
# ("ESP32-BLE-Gamepad <version>+g<short-sha>") from ../../library.properties and
# git, and expose it as the BLE_GAMEPAD_LIB_VERSION macro. main.cpp reports it
# as the Software Revision DIS characteristic, so a GATT browser shows exactly
# which build is flashed.

import subprocess
from pathlib import Path

Import("env")  # noqa: F821 (injected by PlatformIO)

lib_root = Path(env["PROJECT_DIR"]).resolve().parent.parent

version = "unknown"
props = lib_root / "library.properties"
if props.is_file():
    for line in props.read_text().splitlines():
        if line.startswith("version="):
            version = line.split("=", 1)[1].strip() or version
            break

sha = ""
try:
    sha = subprocess.check_output(
        ["git", "-C", str(lib_root), "rev-parse", "--short", "HEAD"],
        text=True,
        stderr=subprocess.DEVNULL,
    ).strip()
except Exception:
    pass

identifier = "ESP32-BLE-Gamepad " + version
if sha:
    identifier += "+g" + sha

print("BLE_GAMEPAD_LIB_VERSION = %s" % identifier)
env.Append(CPPDEFINES=[("BLE_GAMEPAD_LIB_VERSION", env.StringifyMacro(identifier))])
