#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."

app=$(find ios/build -name 'Spryte.app' -type d -print -quit)
test -n "$app"
mkdir -p shots
maestro --version

find_device() {
    xcrun simctl list devices available --json | python3 -c '
import json, re, subprocess, sys
pattern = sys.argv[1]
devices = json.load(sys.stdin)["devices"]
for runtime in sorted(devices, reverse=True):
    for device in devices[runtime]:
        if re.search(pattern, device["name"]):
            print(device["udid"])
            sys.exit(0)
types = json.loads(subprocess.check_output(["xcrun", "simctl", "list", "devicetypes", "--json"]))["devicetypes"]
device_type = next(t for t in types if re.search(pattern, t["name"]))
runtimes = json.loads(subprocess.check_output(["xcrun", "simctl", "list", "runtimes", "--json"]))["runtimes"]
runtime = max((r for r in runtimes if r.get("isAvailable") and ".iOS-" in r["identifier"]),
              key=lambda r: tuple(int(x) for x in r["version"].split(".")))
print(subprocess.check_output(["xcrun", "simctl", "create", "Spryte-" + pattern,
                               device_type["identifier"], runtime["identifier"]], text=True).strip())
' "$1"
}

test_device() {
    local udid; udid=$(find_device "$1")
    local prefix="$2"
    echo "Testing $prefix on $udid"
    xcrun simctl boot "$udid" >/dev/null 2>&1 || true
    xcrun simctl bootstatus "$udid" -b
    xcrun simctl status_bar "$udid" override --time '9:41' --batteryState charged --batteryLevel 100
    xcrun simctl install "$udid" "$app"

    maestro --udid "$udid" test --test-output-dir "shots/$prefix-layout" \
        --format junit --output "shots/$prefix-layout.xml" ios/tests/setup-and-layout.yaml

    # Verify the drag reached persistent storage before resetting the layout.
    local container; container=$(xcrun simctl get_app_container "$udid" com.crownparkcomputing.spryte data)
    python3 - "$container" <<'PY'
import json, pathlib, plistlib, sys
preferences = pathlib.Path(sys.argv[1]) / 'Library/Preferences/com.crownparkcomputing.spryte.plist'
with preferences.open('rb') as stream:
    values = plistlib.load(stream)
positions = json.loads(values['onscreenPadLayout.v1'])
assert positions['landscape.actions']['x'] < 0.5, positions
print('Dragged layout persisted across app relaunch:', positions)
PY

    maestro --udid "$udid" test --test-output-dir "shots/$prefix-demo" \
        --format junit --output "shots/$prefix-demo.xml" ios/tests/demo-and-return.yaml

    if [ "$prefix" = ipad-13in ]; then
        maestro --udid "$udid" test --test-output-dir "shots/$prefix-portrait" \
            --format junit --output "shots/$prefix-portrait.xml" ios/tests/ipad-portrait.yaml
    fi
    xcrun simctl shutdown "$udid"
}

devices=("${@:-iphone-6.9in ipad-13in}")
if [ "$#" = 0 ]; then devices=(iphone-6.9in ipad-13in); fi
for device in "${devices[@]}"; do
    case "$device" in
        iphone-6.9in) test_device 'iPhone 16 Pro Max' "$device" ;;
        ipad-13in) test_device 'iPad Pro 13-inch' "$device" ;;
        *) echo "Unknown device: $device" >&2; exit 1 ;;
    esac
done

# Export PNG pixels in the orientation XCTest records in their metadata.
"${SHOT_PYTHON:-python3}" ios/tests/normalise_shots.py shots "${devices[@]}"
