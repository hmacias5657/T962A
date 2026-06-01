import shutil, os
Import("env")

def rename_firmware(source, target, env):
    version = "1.9.0"
    build_dir = env.subst("$BUILD_DIR")
    old = os.path.join(build_dir, "firmware.bin")
    new = os.path.join(build_dir, "firmware_v{}.bin".format(version))
    if os.path.exists(old):
        shutil.copy(old, new)
        print("  Copied firmware -> {}".format(new))

env.AddPostAction("$BUILD_DIR/firmware.bin", rename_firmware)
