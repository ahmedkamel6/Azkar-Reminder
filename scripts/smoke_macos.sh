#!/usr/bin/env bash
set -euo pipefail
# Run only on a disposable macOS runner; never replace an existing library.
[[ "${CI:-}" == true ]] || { echo 'Requires a disposable CI profile.' >&2; exit 1; }
# The deployed app must use its own plugins rather than install-qt-action's SDK.
unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QML2_IMPORT_PATH QML_IMPORT_PATH DYLD_LIBRARY_PATH DYLD_FRAMEWORK_PATH
image=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
task_directory=$(mktemp -d)
config="$HOME/Library/Application Support/Azkar Reminder"
if [[ -e "$config" && -n "$(find "$config" -type f -print -quit)" ]]; then
    echo 'Existing application data found.' >&2
    exit 1
fi
process=
mounted=false
cleanup() {
    if [[ -n "$process" ]]; then kill -KILL "$process" 2>/dev/null || true; wait "$process" 2>/dev/null || true; fi
    if [[ "$mounted" == true ]]; then hdiutil detach "$task_directory/mount" >/dev/null || hdiutil detach -force "$task_directory/mount" >/dev/null || true; fi
    rm -rf "$task_directory"
}
trap cleanup EXIT
cp "$image" "$image.sha256" "$task_directory/"
(
    cd "$task_directory"
    shasum -a 256 --check "$(basename "$image").sha256"
)
mkdir "$task_directory/mount"
hdiutil attach "$image" -readonly -nobrowse -mountpoint "$task_directory/mount"
mounted=true
ditto "$task_directory/mount/Azkar Reminder.app" "$task_directory/Azkar Reminder.app"
app="$task_directory/Azkar Reminder.app"
binary="$app/Contents/MacOS/Azkar Reminder"
launch=("$binary")
codesign --verify --deep --strict "$app"
lipo "$binary" -verify_arch arm64 x86_64
test -s "$app/Contents/Resources/audio/diode_matrix.mp3"
test -s "$app/Contents/Resources/AzkarReminder.icns"
test "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$app/Contents/Info.plist")" == 'Azkar Reminder'
start() {
    "${launch[@]}" --background > "$task_directory/application.log" 2>&1 &
    process=$!
    for attempt in {1..30}; do
        if ! kill -0 "$process" 2>/dev/null; then cat "$task_directory/application.log"; return 1; fi
        if [[ -f "$config/instance.lock" && "$(head -1 "$config/instance.lock")" == "$process" ]] &&
            /usr/bin/perl -e 'alarm 10; exec @ARGV or die "exec failed: $!";' "$binary" --background; then return; fi
        sleep 1
    done
    cat "$task_directory/application.log"
    return 1
}
stop() {
    "$binary" --quit
    for attempt in {1..15}; do
        if ! kill -0 "$process" 2>/dev/null; then wait "$process"; process=; return; fi
        sleep 1
    done
    echo 'IPC quit did not stop the application.' >&2
    cat "$task_directory/application.log" >&2
    sample "$process" 1 1 -file "$task_directory/hang.txt" >/dev/null 2>&1 || true
    if [[ -f "$task_directory/hang.txt" ]]; then head -100 "$task_directory/hang.txt" >&2; fi
    return 1
}
echo 'Testing native Cocoa startup and Launch Services.'
start
# Exercise Launch Services with the deployed bundle, then IPC show and quit.
open -n -a "$app" --args --background
"$binary"
stop
echo 'Testing an intentionally empty library.'
printf '[]' > "$config/content.json"
start
stop
[[ "$(cat "$config/content.json")" == '[]' ]]
printf '{invalid' > "$config/content.json"
echo 'Testing a malformed library.'
set +e
"$binary" --background
status=$?
set -e
[[ "$status" == 1 && "$(cat "$config/content.json")" == '{invalid' ]]
printf '[]' > "$config/content.json"
printf '{invalid-settings' > "$config/settings.json"
echo 'Testing settings recovery.'
start
stop
backup=$(find "$config" -name 'settings-invalid-*.json' -print -quit)
[[ -n "$backup" && "$(cat "$backup")" == '{invalid-settings' ]]
if [[ "$(uname -m)" == arm64 ]]; then
    echo 'Testing the deployed x86_64 slice through Rosetta.'
    arch -x86_64 /usr/bin/true
    launch=(arch -x86_64 "$binary")
    start
    stop
    echo 'PASS: deployed x86_64 slice executes under Rosetta on the ARM host.'
fi
echo 'PASS: DMG mount, deployed Cocoa app, Launch Services, single instance, IPC quit and library recovery.'
