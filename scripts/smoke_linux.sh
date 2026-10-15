#!/usr/bin/env bash
set -euo pipefail
application=$(realpath "$1")
shift
arguments=("$@")
task_directory=$(mktemp -d)
process=
cleanup() {
    if [[ -n "$process" ]]; then kill "$process" 2>/dev/null || true; wait "$process" 2>/dev/null || true; fi
    rm -rf "$task_directory"
}
trap cleanup EXIT
export XDG_CONFIG_HOME="$task_directory/config"
export XDG_DATA_HOME="$task_directory/data"
export XDG_STATE_HOME="$task_directory/state"
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}
owns_lock() {
    local owner
    owner=$(head -1 "$XDG_CONFIG_HOME/azkar-reminder/instance.lock") || return 1
    # The extract-and-run AppImage runtime can remain as the parent of Qt.
    # Accept only the launched process or one of its descendants.
    for depth in {1..20}; do
        [[ "$owner" =~ ^[0-9]+$ ]] || return 1
        [[ "$owner" == "$process" ]] && return 0
        [[ -r "/proc/$owner/status" ]] || return 1
        owner=$(sed -n 's/^PPid:[[:space:]]*//p' "/proc/$owner/status")
    done
    return 1
}
start() {
    "$application" "${arguments[@]}" --background > "$task_directory/application.log" 2>&1 &
    process=$!
    for attempt in {1..30}; do
        if ! kill -0 "$process" 2>/dev/null; then cat "$task_directory/application.log"; return 1; fi
        if [[ -f "$XDG_CONFIG_HOME/azkar-reminder/instance.lock" ]] && owns_lock &&
            timeout 10 "$application" "${arguments[@]}" --background; then return; fi
        sleep 1
    done
    cat "$task_directory/application.log"
    return 1
}
stop() {
    timeout 15 "$application" "${arguments[@]}" --quit
    for attempt in {1..15}; do
        if ! kill -0 "$process" 2>/dev/null; then wait "$process"; process=; return; fi
        sleep 1
    done
    echo 'IPC did not stop the first instance.' >&2
    return 1
}
start
timeout 15 "$application" "${arguments[@]}" --background
stop
library="$XDG_DATA_HOME/azkar-reminder/content.json"
printf '[]' > "$library"
start
stop
[[ "$(cat "$library")" == '[]' ]]
printf '{invalid' > "$library"
if timeout 15 "$application" "${arguments[@]}" --background > "$task_directory/corrupt.log" 2>&1; then
    echo 'Malformed library should report failure.' >&2
    exit 1
else
    status=$?
    [[ "$status" == 1 ]]
fi
[[ "$(cat "$library")" == '{invalid' ]]
printf '[]' > "$library"
printf '{invalid-settings' > "$XDG_CONFIG_HOME/azkar-reminder/settings.json"
start
stop
backup=$(find "$XDG_CONFIG_HOME/azkar-reminder" -name 'settings-invalid-*.json' -print -quit)
[[ -n "$backup" && "$(cat "$backup")" == '{invalid-settings' ]]
grep -q '"language"' "$XDG_CONFIG_HOME/azkar-reminder/settings.json"
echo 'PASS: startup, single instance, IPC quit, empty library, malformed library retained, settings backed up.'
