Import("env")

import subprocess


def _erase_otadata_before_upload(source, target, env):
    try:
        env.AutodetectUploadPort()
    except Exception:
        pass

    port = env.subst("$UPLOAD_PORT")
    uploader = env.subst("$UPLOADER")
    speed = env.subst("$UPLOAD_SPEED")

    if not port or "$UPLOAD_PORT" in port:
        print("pre_upload_ota_reset: UPLOAD_PORT unresolved, skipping otadata erase")
        return

    cmd = [
        uploader,
        "--chip",
        "esp32s3",
        "--port",
        port,
        "--baud",
        str(speed),
        "erase_region",
        "0xe000",
        "0x2000",
    ]

    print(f"pre_upload_ota_reset: erasing otadata on {port}")
    try:
        rc = subprocess.call(cmd)
        if rc != 0:
            print(f"pre_upload_ota_reset: warning: erase_region failed with exit code {rc}; continuing upload")
    except Exception as exc:
        print(f"pre_upload_ota_reset: warning: {exc}; continuing upload")


env.AddPreAction("upload", _erase_otadata_before_upload)
