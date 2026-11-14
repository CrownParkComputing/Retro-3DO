#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."

app=$(find ios/build -name 'Retro-3DO.app' -type d -print -quit)
test -n "$app"
mkdir -p shots
maestro --version

find_device() {
    xcrun simctl list devices available --json | python3 -c '
import json, re, sys
pattern = sys.argv[1]
devices = json.load(sys.stdin)["devices"]
for runtime in sorted(devices, reverse=True):
    for device in devices[runtime]:
        if re.search(pattern, device["name"]):
            print(device["udid"])
            sys.exit(0)
sys.exit("No matching simulator: " + pattern)
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
    local container; container=$(xcrun simctl get_app_container "$udid" com.crownparkcomputing.retro3DO data)
    python3 - "$container" <<'PY'
import json, pathlib, plistlib, sys
preferences = pathlib.Path(sys.argv[1]) / 'Library/Preferences/com.crownparkcomputing.retro3DO.plist'
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

test_device 'iPhone 16 Pro Max' iphone-6.9in
test_device 'iPad Pro 13-inch' ipad-13in

# Maestro captures the screen in its actual orientation. Never rotate an iPad
# portrait screen into a sideways landscape store image merely to match a size.
python3 - <<'PY'
import pathlib, shutil, struct
root = pathlib.Path('shots')
for prefix, landscape in [('iphone-6.9in', (2868, 1320)), ('ipad-13in', (2752, 2064))]:
    destination = root / 'store' / prefix
    destination.mkdir(parents=True, exist_ok=True)
    names = ['01-library', '02-guide', '03-controls', '04-demo']
    if prefix.startswith('ipad'):
        names += ['05-portrait-controls', '06-portrait-demo']
    for name in names:
        candidates = [p for p in root.rglob(name + '.png') if p.parts[1].startswith(prefix)]
        assert len(candidates) == 1, (prefix, name, candidates)
        source = candidates[0]
        with source.open('rb') as stream:
            header = stream.read(24)
        size = struct.unpack('>II', header[16:24])
        expected = landscape[::-1] if 'portrait' in name else landscape
        assert size == expected, (source, size, expected)
        shutil.copy2(source, destination / (name + '.png'))
        print(prefix, name, size)
PY
