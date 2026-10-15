#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-$root/build}
output=${2:-$root/release}
mkdir -p "$output"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/payload"
app="$stage/payload/Azkar Reminder.app"
ditto "$build/Azkar Reminder.app" "$app"
mkdir -p "$app/Contents/Resources/audio" "$stage/icon.iconset"
cp "$root/assets/audio/diode_matrix.mp3" "$app/Contents/Resources/audio/"
mkdir -p "$app/Contents/Resources/licenses"
cp "$root/assets/licenses/"* "$app/Contents/Resources/licenses/"
cp "$root/LICENSE" "$root/THIRD_PARTY_NOTICES.md" "$app/Contents/Resources/"
for size in 16 32 128 256 512; do
    sips -z "$size" "$size" "$root/assets/images/logo.png" --out "$stage/icon.iconset/icon_${size}x${size}.png" >/dev/null
    retina=$((size * 2))
    sips -z "$retina" "$retina" "$root/assets/images/logo.png" --out "$stage/icon.iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$stage/icon.iconset" -o "$app/Contents/Resources/AzkarReminder.icns"
/usr/libexec/PlistBuddy -c 'Set :CFBundleIconFile AzkarReminder.icns' "$app/Contents/Info.plist" 2>/dev/null || \
    /usr/libexec/PlistBuddy -c 'Add :CFBundleIconFile string AzkarReminder.icns' "$app/Contents/Info.plist"
if [[ -n "${MACOS_SIGNING_IDENTITY:-}" ]]; then
    macdeployqt "$app" -always-overwrite
else
    macdeployqt "$app" -always-overwrite -codesign=-
    codesign --verify --deep --strict "$app"
fi
binary="$app/Contents/MacOS/Azkar Reminder"
lipo "$binary" -verify_arch x86_64 arm64
# Every bundled Qt framework must support both architectures too.
while IFS= read -r framework; do
    if file -b "$framework" | grep -q 'Mach-O'; then lipo "$framework" -verify_arch x86_64 arm64; fi
done < <(find "$app/Contents/Frameworks" "$app/Contents/PlugIns" -type f)
suffix=Unsigned
if [[ -n "${MACOS_SIGNING_IDENTITY:-}" ]]; then
    macdeployqt "$app" -sign-for-notarization="$MACOS_SIGNING_IDENTITY"
    codesign --verify --deep --strict "$app"
    test -n "${MACOS_NOTARY_PROFILE:-}"
    notary_arguments=(--keychain-profile "$MACOS_NOTARY_PROFILE")
    if [[ -n "${MACOS_NOTARY_KEYCHAIN:-}" ]]; then notary_arguments+=(--keychain "$MACOS_NOTARY_KEYCHAIN"); fi
    ditto -c -k --keepParent "$app" "$stage/notarize.zip"
    xcrun notarytool submit "$stage/notarize.zip" "${notary_arguments[@]}" --wait --output-format json > "$stage/notary.json"
    [[ "$(plutil -extract status raw "$stage/notary.json")" == Accepted ]]
    xcrun stapler staple "$app"
    spctl --assess --type execute "$app"
    suffix=Universal
elif [[ "${GITHUB_REF:-}" == refs/tags/* ]]; then
    echo 'Release tags require signing and notarization credentials.' >&2
    exit 1
fi
artifact="$output/AzkarReminder-macOS-$suffix.dmg"
ln -s /Applications "$stage/payload/Applications"
hdiutil create -volname 'Azkar Reminder' -srcfolder "$stage/payload" -ov -format UDZO "$artifact"
if [[ "$suffix" == Universal ]]; then
    codesign --sign "$MACOS_SIGNING_IDENTITY" "$artifact"
    xcrun notarytool submit "$artifact" "${notary_arguments[@]}" --wait --output-format json > "$stage/notary-dmg.json"
    [[ "$(plutil -extract status raw "$stage/notary-dmg.json")" == Accepted ]]
    xcrun stapler staple "$artifact"
fi
(
    cd "$output"
    shasum -a 256 "${artifact##*/}"
) > "$artifact.sha256"
printf 'Created %s\n' "$artifact"
