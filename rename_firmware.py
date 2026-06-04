import re, shutil, os
Import("env")

def get_version():
    config_path = os.path.join(env.subst("$PROJECT_DIR"), "src", "Config.h")
    with open(config_path) as f:
        for line in f:
            m = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', line)
            if m:
                return m.group(1)
    return "unknown"

def rename_firmware(source, target, env):
    version = get_version()
    build_dir = env.subst("$BUILD_DIR")
    old = os.path.join(build_dir, "firmware.bin")
    new = os.path.join(build_dir, "firmware_v{}.bin".format(version))
    if os.path.exists(old):
        shutil.copy(old, new)
        print("  Copied firmware -> {}".format(new))

env.AddPostAction("$BUILD_DIR/firmware.bin", rename_firmware)
