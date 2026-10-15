#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=$(realpath "${1:-$root/build}")
output=${2:-$root/release}
mkdir -p "$output" "$build/packaging-tools"
output=$(realpath "$output")
tools="$build/packaging-tools"

download() {
    local file=$1 url=$2 digest=$3
    if [[ ! -f "$tools/$file" ]]; then curl -fL --retry 3 "$url" -o "$tools/$file"; fi
    printf '%s  %s\n' "$digest" "$tools/$file" | sha256sum --check --status
    chmod +x "$tools/$file"
}

download linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
download linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

stage=$(mktemp -d "$build/AppDir.XXXXXX")
trap 'rm -rf "$stage"' EXIT
cmake --install "$build" --prefix "$stage/usr"
mkdir -p "$stage/usr/share/doc/azkar-reminder/licenses"
cp "$root/LICENSE" "$root/THIRD_PARTY_NOTICES.md" "$stage/usr/share/doc/azkar-reminder/"
cp "$root/assets/licenses/"* "$stage/usr/share/doc/azkar-reminder/licenses/"
mkdir -p "$stage/usr/share/applications" "$stage/usr/share/icons/hicolor/256x256/apps"
cp "$root/assets/images/logo.png" "$stage/usr/share/icons/hicolor/256x256/apps/azkar-reminder.png"
cat > "$stage/usr/share/applications/azkar-reminder.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Azkar Reminder
Name[ar]=مذكر الأذكار
Exec=azkar_reminder
Icon=azkar-reminder
Categories=Utility;
Terminal=false
EOF
export QMAKE=${QMAKE:-$(command -v qmake6)}
export APPIMAGE_EXTRACT_AND_RUN=1
export OUTPUT="$output/AzkarReminder-Linux-x86_64.AppImage"
export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so;libqoffscreen.so"
qt_plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
mkdir -p "$stage/usr/plugins"
for directory in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
    test -d "$qt_plugins/$directory"
    cp -a "$qt_plugins/$directory" "$stage/usr/plugins/"
done
# linuxdeploy excludes HarfBuzz as a presumed system library. Minimal desktop
# installations need it bundled for Qt text shaping, including Arabic.
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$stage" --plugin qt \
    --library /usr/lib/x86_64-linux-gnu/libharfbuzz.so.0
# Include Debian copyright notices for libraries copied from the build host.
for library in "$stage"/usr/lib/*.so*; do
    system_file=$(readlink -f "/usr/lib/x86_64-linux-gnu/${library##*/}")
    [[ -e "$system_file" ]] || continue
    package=$(dpkg-query -S "$system_file" 2>/dev/null | head -1 | cut -d: -f1) || continue
    notice="/usr/share/doc/$package/copyright"
    if [[ -f "$notice" ]]; then cp "$notice" "$stage/usr/share/doc/azkar-reminder/licenses/$package.copyright"; fi
done
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$stage" --output appimage
test -s "$OUTPUT"
(
    cd "$output"
    sha256sum "${OUTPUT##*/}"
) > "$OUTPUT.sha256"
printf 'Created %s\n' "$OUTPUT"
