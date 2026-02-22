"""Pre-build script: patches PS4Controller library to remove delay(250)
in the connection callback that blocks the Bluedroid task and causes
HCI receive buffer overflow (assert host_recv_pkt_cb) when WiFi is active."""

Import("env")

import os

ps4_file = os.path.join(
    env.subst("$PROJECT_LIBDEPS_DIR"),
    env.subst("$PIOENV"),
    "PS4Controller", "src", "PS4Controller.cpp"
)

if os.path.isfile(ps4_file):
    with open(ps4_file, "r") as f:
        content = f.read()

    old = "delay(250);"
    if old in content:
        content = content.replace(old, "// delay(250); // PATCHED: removed, blocks BT task")
        with open(ps4_file, "w") as f:
            f.write(content)
        print("*** PS4Controller patched: removed delay(250) in connection callback ***")
