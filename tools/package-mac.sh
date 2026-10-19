#!/bin/bash
# Builds dist/PatSuite-<version>-Mac.dmg on a Mac, after every plugin is built in build/mac (see
# .github/workflows/mac.yml). Inside the .dmg: one installer (.pkg) where each plugin can be
# ticked on or off, plus a "read me" for the first open. The installer puts:
#   VST3 -> /Library/Audio/Plug-Ins/VST3      AU -> /Library/Audio/Plug-Ins/Components
#   Standalone apps -> /Applications/PatSuite
# Running a newer .dmg updates the plugins in place.
#   bash tools/package-mac.sh 3.1.4
set -euo pipefail
VERSION="$1"
SUITE="$(cd "$(dirname "$0")/.." && pwd)"
PROJECTS="$(dirname "$SUITE")"
WORK="$SUITE/build/macpkg"
OUT="$SUITE/dist"
rm -rf "$WORK"
mkdir -p "$WORK/pkgs" "$WORK/res" "$WORK/dmg" "$OUT"

# name|description (same list as installer/PatSuite.iss)
PLUGINS="PatWave|Wavetable synth
PatVerb|Visual reverb
PatDeEsser|De-esser
PatChain|Sidechain / ducking
PatDoubler|Vocal doubler
PatTune|Real-time pitch correction
Patan|Auto panner with drawn curves
PatPatPat|Stereo / ping-pong delay
PatQpro|Pro equalizer
PatSquash|Multiband upward / downward compressor
PatMaster|Master limiter + loudness meter
PatBass|Bass synth"

artefacts() {  # PatWave and PatVerb are their own projects; the rest live in PatSuite
    if [ -d "$PROJECTS/$1/build/mac/$1_artefacts" ]; then echo "$PROJECTS/$1/build/mac/$1_artefacts/Release"
    else echo "$SUITE/build/mac/plugins/$1/$1_artefacts/Release"; fi
}

outline=""
choices=""
while IFS='|' read -r name desc; do
    a="$(artefacts "$name")"
    if [ ! -d "$a/VST3/$name.vst3" ]; then echo "[--] $name (not built)"; continue; fi
    id="com.patsuite.$(echo "$name" | tr '[:upper:]' '[:lower:]')"
    root="$WORK/root-$name"
    mkdir -p "$root/Library/Audio/Plug-Ins/VST3" "$root/Library/Audio/Plug-Ins/Components" "$root/Applications/PatSuite"
    cp -R "$a/VST3/$name.vst3" "$root/Library/Audio/Plug-Ins/VST3/"
    [ -d "$a/AU/$name.component" ] && cp -R "$a/AU/$name.component" "$root/Library/Audio/Plug-Ins/Components/"
    [ -d "$a/Standalone/$name.app" ] && cp -R "$a/Standalone/$name.app" "$root/Applications/PatSuite/"
    # Ad-hoc signature: without any signature Apple Silicon Macs refuse to load the code.
    find "$root" -maxdepth 4 \( -name "*.vst3" -o -name "*.component" -o -name "*.app" \) -prune \
        -exec codesign --force --deep --sign - {} \;
    # Install exactly there (by default macOS would "relocate" into an older copy found elsewhere).
    pkgbuild --analyze --root "$root" "$WORK/$name.plist" >/dev/null
    i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "$WORK/$name.plist" >/dev/null 2>&1; do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$name.plist"
        i=$((i + 1))
    done
    pkgbuild --quiet --root "$root" --component-plist "$WORK/$name.plist" --identifier "$id" \
        --version "$VERSION" --install-location / "$WORK/pkgs/$name.pkg"
    outline="$outline        <line choice=\"$id\"/>
"
    choices="$choices    <choice id=\"$id\" title=\"$name\" description=\"$desc\">
        <pkg-ref id=\"$id\"/>
    </choice>
    <pkg-ref id=\"$id\" version=\"$VERSION\">$name.pkg</pkg-ref>
"
    echo "[ok] $name"
done <<< "$PLUGINS"

# Look: the neon background (dark mode) and the license page with the Pat Osk banner. The
# Windows colours are made darker here, because the Mac license page has a white background in
# light mode.
cp "$SUITE/installer/art/back.png" "$WORK/res/back.png"
sed -e 's/\\red57\\green255\\blue20/\\red20\\green140\\blue40/' \
    -e 's/\\red168\\green255\\blue176/\\red30\\green110\\blue60/' \
    "$SUITE/installer/art/license.rtf" > "$WORK/res/license.rtf"
cat > "$WORK/res/welcome.html" <<EOF
<html><body style="font-family: -apple-system, Helvetica; font-size: 13px;">
<h2 style="color:#1fa83a;">PatSuite $VERSION</h2>
<p>Free, open-source plugins by Pat Osk: VST3, AU and standalone apps.</p>
<p>Plugins go to <b>/Library/Audio/Plug-Ins</b>, where every DAW finds them. The standalone apps go to <b>Applications/PatSuite</b>.</p>
<p>If you already have PatSuite, this updates it.</p>
<p>Plugins gratis y de código abierto. Si ya tenés PatSuite, esto lo actualiza.</p>
</body></html>
EOF
cat > "$WORK/res/conclusion.html" <<EOF
<html><body style="font-family: -apple-system, Helvetica; font-size: 13px;">
<h2 style="color:#1fa83a;">PatSuite is installed / PatSuite quedó instalado</h2>
<p>Open your DAW and rescan plugins. In FL Studio: Options &gt; Manage plugins &gt; Find installed plugins.</p>
<p>Abrí tu DAW y buscá plugins nuevos. En FL Studio: Options &gt; Manage plugins &gt; Find installed plugins.</p>
</body></html>
EOF
cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>PatSuite $VERSION</title>
    <background-darkAqua file="back.png" alignment="bottomleft" scaling="proportional"/>
    <welcome file="welcome.html" mime-type="text/html"/>
    <license file="license.rtf"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
$outline    </choices-outline>
$choices</installer-gui-script>
EOF
productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" --package-path "$WORK/pkgs" \
    "$WORK/dmg/PatSuite-$VERSION.pkg"

cat > "$WORK/dmg/LEEME - READ ME.txt" <<EOF
PatSuite $VERSION - by Pat Osk

ESPAÑOL
1. Doble clic en PatSuite-$VERSION.pkg.
2. Si la Mac dice que no puede abrirlo porque es de un desarrollador no identificado:
   - Clic derecho (o Control + clic) sobre el .pkg > Abrir > Abrir.
   - En macOS 15 o más nuevo: Ajustes del Sistema > Privacidad y seguridad > "Abrir igualmente".
3. Seguí los pasos. Después, en tu DAW, buscá plugins nuevos.

ENGLISH
1. Double-click PatSuite-$VERSION.pkg.
2. If the Mac says it can't be opened because it is from an unidentified developer:
   - Right-click (or Control-click) the .pkg > Open > Open.
   - On macOS 15 or newer: System Settings > Privacy & Security > "Open Anyway".
3. Follow the steps, then rescan plugins in your DAW.

PatSuite is free and open source (GNU AGPL v3).
EOF

rm -f "$OUT/PatSuite-$VERSION-Mac.dmg"
hdiutil create -quiet -volname "PatSuite $VERSION" -srcfolder "$WORK/dmg" -ov -format UDZO "$OUT/PatSuite-$VERSION-Mac.dmg"
ls -lh "$OUT"/*.dmg
