"""Pre-build script: patches PS4Controller library to remove delay(250)
in the connection callback that blocks the Bluedroid task and causes
HCI receive buffer overflow (assert host_recv_pkt_cb) when WiFi is active.

The patch is idempotent — safe to run on every build. It checks for the
PATCHED marker to avoid double-patching."""

Import("env")

import os, re

ps4_file = os.path.join(
    env.subst("$PROJECT_LIBDEPS_DIR"),
    env.subst("$PIOENV"),
    "PS4Controller", "src", "PS4Controller.cpp"
)

MARKER = "// PATCHED: delay removed"

if os.path.isfile(ps4_file):
    with open(ps4_file, "r") as f:
        content = f.read()

    # Skip if already cleanly patched
    if MARKER in content:
        pass
    else:
        # Match any line containing delay(250) — even previously mangled patches
        new_content = re.sub(
            r'^.*delay\(250\).*$',
            '    ' + MARKER,
            content,
            flags=re.MULTILINE
        )
        if new_content != content:
            with open(ps4_file, "w") as f:
                f.write(new_content)
            print("*** PS4Controller patched: removed delay(250) ***")
