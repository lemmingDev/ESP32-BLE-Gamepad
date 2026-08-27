# PlatformIO post-build step: merge the bootloader, partition table, boot_app0
# and application into one image flashable at offset 0x0. This is what web
# flashers (https://espressif.github.io/esptool-js/) expect and it lets people
# flash the CI test firmware with a single command and no toolchain.
#
# Produces <PROGNAME>-factory.bin next to firmware.bin in $BUILD_DIR.

Import("env")  # noqa: F821 (injected by PlatformIO)

board_config = env.BoardConfig()

chip = board_config.get("build.mcu", "esp32")
flash_size = board_config.get("upload.flash_size", "4MB")
flash_mode = board_config.get("build.flash_mode", env.subst("$BOARD_FLASH_MODE"))
flash_freq = str(board_config.get("build.f_flash", "40000000L"))
flash_freq = flash_freq.replace("000000L", "m").replace("L", "")
# The espressif32 builder exposes the app offset as $ESP32_APP_OFFSET; fall
# back to the board manifest and then the common default.
app_offset = (
    env.subst("$ESP32_APP_OFFSET")
    or board_config.get("upload.offset_address", "")
    or "0x10000"
)


def merge_bin(source, target, env):
    firmware_path = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    merged_path = env.subst("$BUILD_DIR/${PROGNAME}-factory.bin")

    # FLASH_EXTRA_IMAGES holds (offset, path) pairs for the bootloader,
    # partition table and boot_app0; the app itself goes at app_offset.
    segments = []
    for offset, image in env.get("FLASH_EXTRA_IMAGES", []):
        segments += [offset, image]
    segments += [app_offset, firmware_path]

    cmd = [
        '"$PYTHONEXE"',
        '"$OBJCOPY"',
        "--chip",
        chip,
        "merge_bin",
        "-o",
        '"%s"' % merged_path,
        "--flash_mode",
        flash_mode,
        "--flash_freq",
        flash_freq,
        "--flash_size",
        flash_size,
    ]
    for item in segments:
        cmd.append(item if item.startswith("0x") else '"%s"' % item)

    env.Execute(env.VerboseAction(" ".join(cmd), "Merging %s" % merged_path))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin)
