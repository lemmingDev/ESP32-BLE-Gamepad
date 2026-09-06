# PlatformIO pre-build step. Exposes two macros:
#   BLE_GAMEPAD_LIB_VERSION - "ESP32-BLE-Gamepad <version>+g<short-sha>" from
#     ../../library.properties and git; main.cpp reports it as the Software
#     Revision DIS characteristic so a GATT browser shows which build is flashed.
#   BLE_GAMEPAD_BUILD_TIME - UTC timestamp of this build. It changes every run,
#     which also forces main.cpp to recompile, so the value printed on boot is a
#     reliable "did my upload actually land?" check.

import subprocess
import time
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

build_time = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

print("BLE_GAMEPAD_LIB_VERSION = %s" % identifier)
print("BLE_GAMEPAD_BUILD_TIME  = %s" % build_time)
env.Append(CPPDEFINES=[
    ("BLE_GAMEPAD_LIB_VERSION", env.StringifyMacro(identifier)),
    ("BLE_GAMEPAD_BUILD_TIME", env.StringifyMacro(build_time)),
])
